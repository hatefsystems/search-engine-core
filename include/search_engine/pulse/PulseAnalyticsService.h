#pragma once

#include "PulseSearchAnalyticsEvent.h"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace search_engine {
namespace storage {
class PulseAnalyticsStorage;
}

namespace pulse {

class PulseAnalyticsService {
public:
    static PulseAnalyticsService& instance();

    void recordSearchEvent(const std::string& rawQuery,
                           int64_t resultCount,
                           double latencyMs,
                           PulseSearchStatus status,
                           const std::string& userAgent,
                           const std::string& requestAddress) noexcept;

    bool isEnabled() const noexcept { return enabled_; }
    uint64_t droppedEvents() const noexcept { return droppedEvents_.load(); }
    uint64_t storedEvents() const noexcept { return storedEvents_.load(); }

private:
    PulseAnalyticsService();
    ~PulseAnalyticsService();

    PulseAnalyticsService(const PulseAnalyticsService&) = delete;
    PulseAnalyticsService& operator=(const PulseAnalyticsService&) = delete;

    bool enqueue(PulseSearchAnalyticsEvent event) noexcept;
    void startWorkerIfNeeded();
    void workerLoop();
    void flushBatch(std::vector<PulseSearchAnalyticsEvent>& batch);
    PulseSearchAnalyticsEvent buildEvent(const std::string& rawQuery,
                                         int64_t resultCount,
                                         double latencyMs,
                                         PulseSearchStatus status,
                                         const std::string& userAgent,
                                         const std::string& requestAddress) const;

    static bool envFlagEnabled(const char* value);
    static int readPositiveIntEnv(const char* name, int fallback, int minValue, int maxValue);
    static std::string readEnv(const char* name, const std::string& fallback = "");
    static std::string stableHash(const std::string& value);
    static std::string trimToMaxBytes(const std::string& value, size_t maxBytes);

    bool enabled_ = false;
    int flushIntervalSeconds_ = 5;
    size_t maxQueueSize_ = 1000;

    mutable std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<PulseSearchAnalyticsEvent> queue_;
    std::thread worker_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> workerStarted_{false};
    std::atomic<uint64_t> droppedEvents_{0};
    std::atomic<uint64_t> storedEvents_{0};

    mutable std::mutex storageMutex_;
    std::unique_ptr<search_engine::storage::PulseAnalyticsStorage> storage_;
};

} // namespace pulse
} // namespace search_engine

