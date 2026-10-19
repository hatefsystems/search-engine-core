#include "../../include/search_engine/pulse/PulseAnalyticsService.h"

#include "../../include/Logger.h"
#include "../../include/search_engine/pulse/PulseQueryNormalizer.h"
#include "../../include/search_engine/storage/PulseAnalyticsStorage.h"
#include "../../include/search_engine/storage/UserAgentParser.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace search_engine {
namespace pulse {

PulseAnalyticsService& PulseAnalyticsService::instance() {
    static PulseAnalyticsService service;
    return service;
}

PulseAnalyticsService::PulseAnalyticsService() {
    enabled_ = envFlagEnabled(std::getenv("PULSE_ANALYTICS_ENABLED"));
    flushIntervalSeconds_ = readPositiveIntEnv("PULSE_FLUSH_INTERVAL_SECONDS", 5, 1, 60);
    maxQueueSize_ = static_cast<size_t>(readPositiveIntEnv("PULSE_MAX_QUEUE_SIZE", 1000, 10, 100000));

    if (enabled_) {
        LOG_INFO("Pulse analytics enabled with queue size " + std::to_string(maxQueueSize_) +
                 " and flush interval " + std::to_string(flushIntervalSeconds_) + "s");
    } else {
        LOG_INFO("Pulse analytics disabled");
    }
}

PulseAnalyticsService::~PulseAnalyticsService() {
    stopRequested_ = true;
    queueCv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void PulseAnalyticsService::recordSearchEvent(const std::string& rawQuery,
                                              int64_t resultCount,
                                              double latencyMs,
                                              PulseSearchStatus status,
                                              const std::string& userAgent,
                                              const std::string& requestAddress) noexcept {
    if (!enabled_) {
        return;
    }

    try {
        startWorkerIfNeeded();
        PulseSearchAnalyticsEvent event = buildEvent(rawQuery, resultCount, latencyMs, status, userAgent, requestAddress);
        if (!enqueue(std::move(event))) {
            uint64_t dropped = ++droppedEvents_;
            if (dropped == 1 || dropped % 100 == 0) {
                LOG_WARNING("Pulse analytics queue dropped events: " + std::to_string(dropped));
            }
        }
    } catch (const std::exception& e) {
        uint64_t dropped = ++droppedEvents_;
        if (dropped == 1 || dropped % 100 == 0) {
            LOG_WARNING("Pulse analytics event skipped: " + std::string(e.what()));
        }
    } catch (...) {
        uint64_t dropped = ++droppedEvents_;
        if (dropped == 1 || dropped % 100 == 0) {
            LOG_WARNING("Pulse analytics event skipped due to unknown error");
        }
    }
}

bool PulseAnalyticsService::enqueue(PulseSearchAnalyticsEvent event) noexcept {
    std::unique_lock<std::mutex> lock(queueMutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        return false;
    }

    if (queue_.size() >= maxQueueSize_) {
        return false;
    }

    queue_.push_back(std::move(event));
    if (queue_.size() >= 100) {
        queueCv_.notify_one();
    }
    return true;
}

void PulseAnalyticsService::startWorkerIfNeeded() {
    bool expected = false;
    if (workerStarted_.compare_exchange_strong(expected, true)) {
        worker_ = std::thread([this]() { workerLoop(); });
    }
}

void PulseAnalyticsService::workerLoop() {
    while (!stopRequested_) {
        std::vector<PulseSearchAnalyticsEvent> batch;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCv_.wait_for(lock, std::chrono::seconds(flushIntervalSeconds_), [this]() {
                return stopRequested_.load() || queue_.size() >= 100;
            });

            while (!queue_.empty() && batch.size() < 500) {
                batch.push_back(std::move(queue_.front()));
                queue_.pop_front();
            }
        }

        if (!batch.empty()) {
            flushBatch(batch);
        }
    }

    std::vector<PulseSearchAnalyticsEvent> batch;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!queue_.empty()) {
            batch.push_back(std::move(queue_.front()));
            queue_.pop_front();
        }
    }

    if (!batch.empty()) {
        flushBatch(batch);
    }
}

void PulseAnalyticsService::flushBatch(std::vector<PulseSearchAnalyticsEvent>& batch) {
    try {
        std::lock_guard<std::mutex> lock(storageMutex_);
        if (!storage_) {
            std::string uri = readEnv("PULSE_MONGODB_URI", readEnv("MONGODB_URI", "mongodb://localhost:27017"));
            std::string database = readEnv("PULSE_MONGODB_DATABASE", "hatef_pulse");
            int retentionDays = readPositiveIntEnv("PULSE_RAW_RETENTION_DAYS", 14, 1, 90);
            storage_ = std::make_unique<search_engine::storage::PulseAnalyticsStorage>(uri, database, retentionDays);
        }

        auto result = storage_->storeEventsBatch(batch);
        if (result.success) {
            storedEvents_ += result.value;
        } else {
            droppedEvents_ += batch.size();
            LOG_WARNING("Pulse analytics batch store failed: " + result.message);
        }
    } catch (const std::exception& e) {
        droppedEvents_ += batch.size();
        LOG_WARNING("Pulse analytics storage unavailable: " + std::string(e.what()));
    }
}

PulseSearchAnalyticsEvent PulseAnalyticsService::buildEvent(const std::string& rawQuery,
                                                            int64_t resultCount,
                                                            double latencyMs,
                                                            PulseSearchStatus status,
                                                            const std::string& userAgent,
                                                            const std::string& requestAddress) const {
    PulseSearchAnalyticsEvent event;
    event.timestamp = std::chrono::system_clock::now();
    event.rawQuery = trimToMaxBytes(rawQuery, 256);
    event.normalizedQuery = PulseQueryNormalizer::normalize(event.rawQuery);
    event.normalizedQueryHash = PulseQueryNormalizer::hashNormalizedQuery(event.normalizedQuery);
    event.resultCount = std::max<int64_t>(0, resultCount);
    event.latencyMs = std::max(0.0, latencyMs);
    event.status = status;
    event.languageEstimate = PulseQueryNormalizer::estimateLanguage(event.rawQuery);
    event.source = "api_search";
    event.publicSafe = PulseQueryNormalizer::isPublicSafeQuery(event.rawQuery, event.normalizedQuery);

    auto parsedUserAgent = search_engine::storage::UserAgentParser::parse(userAgent);
    event.device.browser = parsedUserAgent.browser;
    event.device.os = parsedUserAgent.os;
    event.device.type = parsedUserAgent.deviceType;

    std::string salt = readEnv("PULSE_HASH_SALT");
    if (!salt.empty() && !requestAddress.empty() && requestAddress != "unknown") {
        event.anonymousUserHash = stableHash(salt + "|" + requestAddress + "|" + userAgent);
        event.requestHash = stableHash(salt + "|" + requestAddress + "|" + userAgent + "|" + event.rawQuery);
    }

    return event;
}

bool PulseAnalyticsService::envFlagEnabled(const char* value) {
    if (!value) {
        return false;
    }
    std::string flag = value;
    for (char& c : flag) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

int PulseAnalyticsService::readPositiveIntEnv(const char* name, int fallback, int minValue, int maxValue) {
    const char* value = std::getenv(name);
    if (!value) {
        return fallback;
    }

    try {
        int parsed = std::stoi(value);
        if (parsed < minValue || parsed > maxValue) {
            return fallback;
        }
        return parsed;
    } catch (...) {
        return fallback;
    }
}

std::string PulseAnalyticsService::readEnv(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (!value || std::string(value).empty()) {
        return fallback;
    }
    return std::string(value);
}

std::string PulseAnalyticsService::stableHash(const std::string& value) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : value) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }

    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::string PulseAnalyticsService::trimToMaxBytes(const std::string& value, size_t maxBytes) {
    if (value.size() <= maxBytes) {
        return value;
    }
    return value.substr(0, maxBytes);
}

} // namespace pulse
} // namespace search_engine
