#include "../../include/search_engine/storage/PulseAnalyticsStorage.h"

#include "../../include/Logger.h"
#include "../../include/mongodb.h"
#include "../../include/search_engine/pulse/PulseQueryNormalizer.h"
#include "../../include/search_engine/pulse/PulseScoring.h"
#include <algorithm>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <ctime>
#include <iomanip>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/options/index.hpp>
#include <mongocxx/options/insert.hpp>
#include <mongocxx/options/update.hpp>
#include <sstream>

namespace search_engine {
namespace storage {

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

namespace {

std::string statusCountField(pulse::PulseSearchStatus status) {
    switch (status) {
        case pulse::PulseSearchStatus::Ok:
            return "ok_count";
        case pulse::PulseSearchStatus::Empty:
            return "empty_count";
        case pulse::PulseSearchStatus::Error:
            return "error_count";
    }
    return "error_count";
}

std::string withFastFailureMongoOptions(std::string uri) {
    auto appendOption = [](std::string value, const std::string& key, const std::string& option) {
        if (value.find(key + "=") != std::string::npos) {
            return value;
        }

        if (!value.empty() && value.back() != '?' && value.back() != '&') {
            value += value.find('?') == std::string::npos ? '?' : '&';
        }
        value += option;
        return value;
    };

    if (uri.empty()) {
        return uri;
    }

    uri = appendOption(uri, "serverSelectionTimeoutMS", "serverSelectionTimeoutMS=1000");
    uri = appendOption(uri, "connectTimeoutMS", "connectTimeoutMS=1000");
    return uri;
}

} // namespace

PulseAnalyticsStorage::PulseAnalyticsStorage(const std::string& connectionString,
                                             const std::string& databaseName,
                                             int rawRetentionDays)
    : rawRetentionDays_(rawRetentionDays) {
    MongoDBInstance::getInstance();
    client_ = std::make_unique<mongocxx::client>(mongocxx::uri{withFastFailureMongoOptions(connectionString)});
    database_ = (*client_)[databaseName];
    rawEventsCollection_ = database_["search_events_raw"];
    queryBucketsCollection_ = database_["search_query_buckets"];
    dailyStatsCollection_ = database_["search_daily_stats"];
    trendsCollection_ = database_["search_trends"];
    zeroResultsCollection_ = database_["zero_result_queries"];
    ensureIndexes();
}

Result<bool> PulseAnalyticsStorage::ensureIndexes() {
    try {
        {
            auto keys = make_document(kvp("timestamp", 1));
            mongocxx::options::index opts;
            opts.name("raw_timestamp_ttl");
            opts.expire_after(std::chrono::seconds(rawRetentionDays_ * 24 * 60 * 60));
            rawEventsCollection_.create_index(keys.view(), opts);
        }

        {
            auto keys = make_document(
                kvp("granularity", 1),
                kvp("bucket_start", 1),
                kvp("normalized_query_hash", 1),
                kvp("source", 1));
            mongocxx::options::index opts;
            opts.name("query_bucket_unique");
            opts.unique(true);
            queryBucketsCollection_.create_index(keys.view(), opts);
        }

        {
            auto keys = make_document(kvp("granularity", 1), kvp("bucket_start", -1), kvp("search_count", -1));
            mongocxx::options::index opts;
            opts.name("query_bucket_public_lookup");
            queryBucketsCollection_.create_index(keys.view(), opts);
        }

        {
            auto keys = make_document(kvp("date", 1), kvp("source", 1));
            mongocxx::options::index opts;
            opts.name("daily_stats_unique");
            opts.unique(true);
            dailyStatsCollection_.create_index(keys.view(), opts);
        }

        {
            auto keys = make_document(kvp("window", 1), kvp("updated_at", -1), kvp("trend_score", -1));
            mongocxx::options::index opts;
            opts.name("trend_score_lookup");
            trendsCollection_.create_index(keys.view(), opts);
        }

        {
            auto keys = make_document(kvp("date", 1), kvp("normalized_query_hash", 1), kvp("source", 1));
            mongocxx::options::index opts;
            opts.name("zero_result_unique");
            opts.unique(true);
            zeroResultsCollection_.create_index(keys.view(), opts);
        }

        {
            auto keys = make_document(kvp("date", -1), kvp("search_count", -1));
            mongocxx::options::index opts;
            opts.name("zero_result_public_lookup");
            zeroResultsCollection_.create_index(keys.view(), opts);
        }

        return Result<bool>::Success(true, "Pulse analytics indexes ensured");
    } catch (const std::exception& e) {
        return Result<bool>::Failure("Failed to ensure Pulse analytics indexes: " + std::string(e.what()));
    }
}

Result<size_t> PulseAnalyticsStorage::storeEventsBatch(const std::vector<pulse::PulseSearchAnalyticsEvent>& events) {
    if (events.empty()) {
        return Result<size_t>::Success(0, "No Pulse analytics events to store");
    }

    try {
        std::vector<bsoncxx::document::value> rawDocuments;
        std::vector<bsoncxx::document::view> rawViews;
        rawDocuments.reserve(events.size());
        rawViews.reserve(events.size());

        for (const auto& event : events) {
            rawDocuments.push_back(eventToBson(event));
        }
        for (const auto& doc : rawDocuments) {
            rawViews.push_back(doc.view());
        }

        mongocxx::options::insert insertOptions;
        insertOptions.ordered(false);
        rawEventsCollection_.insert_many(rawViews, insertOptions);

        for (const auto& event : events) {
            upsertEventAggregates(event);
        }

        return Result<size_t>::Success(events.size(), "Pulse analytics events stored");
    } catch (const std::exception& e) {
        return Result<size_t>::Failure("Failed to store Pulse analytics batch: " + std::string(e.what()));
    }
}

Result<PulseSummary> PulseAnalyticsStorage::getSummary(const std::string& rangeKey, int minEvents) {
    try {
        PulseSummary summary;
        auto range = buildDateRange(rangeKey);

        bsoncxx::builder::basic::document filter;
        filter.append(kvp("granularity", "hour"), kvp("source", "api_search"));
        bsoncxx::builder::basic::document bucketRange;
        if (!range.allTime) {
            bucketRange.append(kvp("$gte", toBsonDate(range.start)));
            bucketRange.append(kvp("$lt", toBsonDate(range.end)));
            filter.append(kvp("bucket_start", bucketRange.view()));
        }

        int64_t searchCount = 0;
        int64_t okCount = 0;
        int64_t emptyCount = 0;
        double latencyTotal = 0.0;

        auto cursor = queryBucketsCollection_.find(filter.view());
        for (const auto& doc : cursor) {
            searchCount += readInt64(doc, "search_count");
            okCount += readInt64(doc, "ok_count");
            emptyCount += readInt64(doc, "empty_count");
            latencyTotal += readDouble(doc, "latency_ms_total");
        }

        if (searchCount <= 0) {
            return Result<PulseSummary>::Success(summary, "Pulse summary has no data");
        }

        summary.enoughData = searchCount >= minEvents;
        summary.activityScore = pulse::PulseRelativeScore::clamp(std::min(100.0, static_cast<double>(searchCount) * 10.0));
        summary.successScore = searchCount > 0
            ? pulse::PulseRelativeScore::clamp((static_cast<double>(okCount) / static_cast<double>(searchCount)) * 100.0)
            : 0;
        summary.zeroResultOpportunityScore = searchCount > 0
            ? pulse::PulseRelativeScore::clamp((static_cast<double>(emptyCount) / static_cast<double>(searchCount)) * 100.0)
            : 0;

        double avgLatency = searchCount > 0 ? latencyTotal / static_cast<double>(searchCount) : 0.0;
        if (avgLatency <= 0.0) {
            summary.speedScore = 0;
        } else if (avgLatency <= 100.0) {
            summary.speedScore = 100;
        } else if (avgLatency >= 2000.0) {
            summary.speedScore = 20;
        } else {
            summary.speedScore = pulse::PulseRelativeScore::clamp(100.0 - ((avgLatency - 100.0) / 1900.0) * 80.0);
        }

        return Result<PulseSummary>::Success(summary, "Pulse summary loaded");
    } catch (const std::exception& e) {
        return Result<PulseSummary>::Failure("Failed to load Pulse summary: " + std::string(e.what()));
    }
}

Result<std::vector<PulseQueryMetric>> PulseAnalyticsStorage::getTopQueries(int limit, const std::string& rangeKey, int minEvents) {
    try {
        auto range = buildDateRange(rangeKey);
        auto metrics = loadWindowMetrics(range, candidateLimitForRange(range, limit));
        std::sort(metrics.begin(), metrics.end(), [](const PulseQueryMetric& a, const PulseQueryMetric& b) {
            return a.internalCount > b.internalCount;
        });

        if (metrics.size() > static_cast<size_t>(limit)) {
            metrics.resize(limit);
        }

        int64_t maxCount = 0;
        for (const auto& metric : metrics) {
            maxCount = std::max(maxCount, metric.internalCount);
        }
        for (auto& metric : metrics) {
            metric.score = pulse::PulseRelativeScore::fromCount(metric.internalCount, maxCount);
            metric.enoughData = metric.internalCount >= minEvents;
        }

        return Result<std::vector<PulseQueryMetric>>::Success(metrics, "Pulse top queries loaded");
    } catch (const std::exception& e) {
        return Result<std::vector<PulseQueryMetric>>::Failure("Failed to load Pulse top queries: " + std::string(e.what()));
    }
}

Result<std::vector<PulseQueryMetric>> PulseAnalyticsStorage::getRisingQueries(int limit, const std::string& rangeKey, int minEvents) {
    try {
        auto currentRange = buildDateRange(rangeKey);
        if (currentRange.allTime) {
            auto now = std::chrono::system_clock::now();
            currentRange.start = now - std::chrono::hours(24 * 30);
            currentRange.end = now;
            currentRange.allTime = false;
        }
        auto previousRange = previousDateRange(currentRange);
        auto current = loadWindowMetrics(currentRange, candidateLimitForRange(currentRange, limit));
        auto previous = loadWindowMetrics(previousRange, candidateLimitForRange(previousRange, limit));

        std::unordered_map<std::string, int64_t> previousCounts;
        for (const auto& metric : previous) {
            previousCounts[metric.normalizedQueryHash] = metric.internalCount;
        }

        for (auto& metric : current) {
            metric.trendScore = pulse::PulseTrendScore::calculate(metric.internalCount, previousCounts[metric.normalizedQueryHash]);
            metric.score = metric.trendScore;
            metric.enoughData = metric.internalCount >= minEvents;
        }

        current.erase(std::remove_if(current.begin(), current.end(), [](const PulseQueryMetric& metric) {
            return metric.trendScore <= 0;
        }), current.end());

        std::sort(current.begin(), current.end(), [](const PulseQueryMetric& a, const PulseQueryMetric& b) {
            if (a.trendScore == b.trendScore) {
                return a.internalCount > b.internalCount;
            }
            return a.trendScore > b.trendScore;
        });

        if (current.size() > static_cast<size_t>(limit)) {
            current.resize(limit);
        }

        return Result<std::vector<PulseQueryMetric>>::Success(current, "Pulse rising queries loaded");
    } catch (const std::exception& e) {
        return Result<std::vector<PulseQueryMetric>>::Failure("Failed to load Pulse rising queries: " + std::string(e.what()));
    }
}

Result<std::vector<PulseQueryMetric>> PulseAnalyticsStorage::getZeroResultQueries(int limit, const std::string& rangeKey, int minEvents) {
    try {
        auto range = buildDateRange(rangeKey);
        bsoncxx::builder::basic::document filter;
        filter.append(kvp("source", "api_search"));
        bsoncxx::builder::basic::document dateRange;
        if (!range.allTime) {
            dateRange.append(kvp("$gte", utcDateString(range.start)));
            dateRange.append(kvp("$lte", utcDateString(range.end)));
            filter.append(kvp("date", dateRange.view()));
        }

        mongocxx::options::find opts;
        auto sort = make_document(kvp("search_count", -1));
        opts.sort(sort.view());
        opts.limit(candidateLimitForRange(range, limit));

        struct Accumulator {
            std::string query;
            std::string hash;
            std::string language;
            int64_t count = 0;
        };

        std::unordered_map<std::string, Accumulator> byHash;
        auto cursor = zeroResultsCollection_.find(filter.view(), opts);
        for (const auto& doc : cursor) {
            std::string hash = readString(doc, "normalized_query_hash");
            std::string query = readString(doc, "display_query");
            if (hash.empty() || query.empty() || !pulse::PulseQueryNormalizer::isPublicSafeQuery(query, query)) {
                continue;
            }

            auto& acc = byHash[hash];
            if (acc.hash.empty()) {
                acc.hash = hash;
                acc.query = query;
                acc.language = readString(doc, "language_estimate", "unknown");
            }
            acc.count += readInt64(doc, "search_count");
        }

        std::vector<PulseQueryMetric> metrics;
        metrics.reserve(byHash.size());
        int64_t maxCount = 0;
        for (const auto& [_, acc] : byHash) {
            PulseQueryMetric metric;
            metric.query = acc.query;
            metric.normalizedQueryHash = acc.hash;
            metric.languageEstimate = acc.language;
            metric.internalCount = acc.count;
            metric.enoughData = metric.internalCount >= minEvents;
            maxCount = std::max(maxCount, metric.internalCount);
            metrics.push_back(metric);
        }

        std::sort(metrics.begin(), metrics.end(), [](const PulseQueryMetric& a, const PulseQueryMetric& b) {
            return a.internalCount > b.internalCount;
        });
        if (metrics.size() > static_cast<size_t>(limit)) {
            metrics.resize(limit);
        }

        for (auto& metric : metrics) {
            metric.score = pulse::PulseRelativeScore::fromCount(metric.internalCount, maxCount);
        }

        return Result<std::vector<PulseQueryMetric>>::Success(metrics, "Pulse zero-result queries loaded");
    } catch (const std::exception& e) {
        return Result<std::vector<PulseQueryMetric>>::Failure("Failed to load Pulse zero-result queries: " + std::string(e.what()));
    }
}

Result<std::vector<PulseTrendPoint>> PulseAnalyticsStorage::getQueryTrend(const std::string& rawQuery, const std::string& rangeKey, int minEvents) {
    try {
        std::string normalized = pulse::PulseQueryNormalizer::normalize(rawQuery);
        std::string hash = pulse::PulseQueryNormalizer::hashNormalizedQuery(normalized);
        auto now = std::chrono::system_clock::now();
        auto range = buildDateRange(rangeKey);
        if (range.key == "today") {
            range.start = floorToHour(now - std::chrono::hours(23));
            range.end = now;
            range.allTime = false;
        }

        if (range.allTime) {
            mongocxx::options::find earliestOpts;
            auto sort = make_document(kvp("bucket_start", 1));
            earliestOpts.sort(sort.view());
            earliestOpts.limit(1);
            auto earliestFilter = make_document(
                kvp("granularity", "hour"),
                kvp("source", "api_search"),
                kvp("normalized_query_hash", hash));
            auto earliest = queryBucketsCollection_.find_one(earliestFilter.view(), earliestOpts);
            if (earliest) {
                range.start = fromBsonDate((*earliest)["bucket_start"].get_date());
            } else {
                range.start = now - std::chrono::hours(24);
            }
            range.end = now;
            range.allTime = false;
        }

        auto bucketRange = make_document(
            kvp("$gte", toBsonDate(range.start)),
            kvp("$lt", toBsonDate(range.end)));
        auto filter = make_document(
            kvp("granularity", "hour"),
            kvp("source", "api_search"),
            kvp("normalized_query_hash", hash),
            kvp("bucket_start", bucketRange.view()));

        int pointCount = trendPointCountForRange(range);
        auto rangeMs = std::max<int64_t>(1, std::chrono::duration_cast<std::chrono::milliseconds>(range.end - range.start).count());
        auto binMs = std::max<int64_t>(1, rangeMs / pointCount);

        std::vector<int64_t> counts(pointCount, 0);
        int64_t maxCount = 0;
        auto cursor = queryBucketsCollection_.find(filter.view());
        for (const auto& doc : cursor) {
            auto bucket = fromBsonDate(doc["bucket_start"].get_date());
            int64_t offsetMs = std::chrono::duration_cast<std::chrono::milliseconds>(bucket - range.start).count();
            int index = static_cast<int>(std::min<int64_t>(pointCount - 1, std::max<int64_t>(0, offsetMs / binMs)));
            int64_t count = readInt64(doc, "search_count");
            counts[index] += count;
            maxCount = std::max(maxCount, counts[index]);
        }

        std::vector<PulseTrendPoint> points;
        points.reserve(pointCount);
        for (int i = 0; i < pointCount; ++i) {
            PulseTrendPoint point;
            point.bucketStart = range.start + std::chrono::milliseconds(binMs * i);
            point.score = maxCount >= minEvents ? pulse::PulseRelativeScore::fromCount(counts[i], maxCount) : 0;
            points.push_back(point);
        }

        return Result<std::vector<PulseTrendPoint>>::Success(points, "Pulse query trend loaded");
    } catch (const std::exception& e) {
        return Result<std::vector<PulseTrendPoint>>::Failure("Failed to load Pulse query trend: " + std::string(e.what()));
    }
}

bsoncxx::document::value PulseAnalyticsStorage::eventToBson(const pulse::PulseSearchAnalyticsEvent& event) const {
    bsoncxx::builder::basic::document doc;
    doc.append(kvp("timestamp", toBsonDate(event.timestamp)));
    doc.append(kvp("raw_query", event.rawQuery));
    doc.append(kvp("normalized_query", event.normalizedQuery));
    doc.append(kvp("normalized_query_hash", event.normalizedQueryHash));
    doc.append(kvp("result_count", event.resultCount));
    doc.append(kvp("latency_ms", event.latencyMs));
    doc.append(kvp("status", pulse::pulseSearchStatusToString(event.status)));
    doc.append(kvp("language_estimate", event.languageEstimate));
    doc.append(kvp("source", event.source));
    doc.append(kvp("public_safe", event.publicSafe));
    doc.append(kvp("device", make_document(
        kvp("browser", event.device.browser),
        kvp("os", event.device.os),
        kvp("type", event.device.type))));

    if (event.anonymousUserHash) {
        doc.append(kvp("anonymous_user_hash", *event.anonymousUserHash));
    } else {
        doc.append(kvp("anonymous_user_hash", bsoncxx::types::b_null{}));
    }
    if (event.requestHash) {
        doc.append(kvp("request_hash", *event.requestHash));
    } else {
        doc.append(kvp("request_hash", bsoncxx::types::b_null{}));
    }

    if (event.country) {
        doc.append(kvp("country", *event.country));
    } else {
        doc.append(kvp("country", bsoncxx::types::b_null{}));
    }
    if (event.province) {
        doc.append(kvp("province", *event.province));
    } else {
        doc.append(kvp("province", bsoncxx::types::b_null{}));
    }
    if (event.city) {
        doc.append(kvp("city", *event.city));
    } else {
        doc.append(kvp("city", bsoncxx::types::b_null{}));
    }

    return doc.extract();
}

void PulseAnalyticsStorage::upsertEventAggregates(const pulse::PulseSearchAnalyticsEvent& event) {
    if (!event.publicSafe || event.normalizedQuery.empty()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto bucketStart = floorToHour(event.timestamp);
    std::string date = utcDateString(event.timestamp);

    upsertQueryBucket(event, bucketStart, now);
    upsertDailyStats(event, date, now);
    if (event.status == pulse::PulseSearchStatus::Empty) {
        upsertZeroResultQuery(event, date, now);
    }
}

void PulseAnalyticsStorage::upsertQueryBucket(const pulse::PulseSearchAnalyticsEvent& event,
                                              const std::chrono::system_clock::time_point& bucketStart,
                                              const std::chrono::system_clock::time_point& now) {
    auto filter = make_document(
        kvp("granularity", "hour"),
        kvp("bucket_start", toBsonDate(bucketStart)),
        kvp("normalized_query_hash", event.normalizedQueryHash),
        kvp("source", event.source));

    bsoncxx::builder::basic::document inc;
    inc.append(kvp("search_count", static_cast<int64_t>(1)));
    inc.append(kvp("result_count_total", event.resultCount));
    inc.append(kvp("latency_ms_total", event.latencyMs));
    inc.append(kvp(statusCountField(event.status), static_cast<int64_t>(1)));

    auto setDoc = make_document(
        kvp("updated_at", toBsonDate(now)),
        kvp("last_seen_at", toBsonDate(event.timestamp)),
        kvp("trend_score", 0));

    auto setOnInsert = make_document(
        kvp("granularity", "hour"),
        kvp("bucket_start", toBsonDate(bucketStart)),
        kvp("normalized_query_hash", event.normalizedQueryHash),
        kvp("normalized_query", event.normalizedQuery),
        kvp("display_query", event.normalizedQuery),
        kvp("language_estimate", event.languageEstimate),
        kvp("source", event.source),
        kvp("created_at", toBsonDate(now)));

    auto update = make_document(
        kvp("$inc", inc.view()),
        kvp("$set", setDoc.view()),
        kvp("$setOnInsert", setOnInsert.view()));

    mongocxx::options::update opts;
    opts.upsert(true);
    queryBucketsCollection_.update_one(filter.view(), update.view(), opts);
}

void PulseAnalyticsStorage::upsertDailyStats(const pulse::PulseSearchAnalyticsEvent& event,
                                             const std::string& date,
                                             const std::chrono::system_clock::time_point& now) {
    auto filter = make_document(kvp("date", date), kvp("source", event.source));

    bsoncxx::builder::basic::document inc;
    inc.append(kvp("search_count", static_cast<int64_t>(1)));
    inc.append(kvp("latency_ms_total", event.latencyMs));
    inc.append(kvp(statusCountField(event.status), static_cast<int64_t>(1)));
    if (event.status == pulse::PulseSearchStatus::Empty) {
        inc.append(kvp("zero_result_count", static_cast<int64_t>(1)));
    }

    auto setDoc = make_document(kvp("updated_at", toBsonDate(now)));
    auto setOnInsert = make_document(
        kvp("date", date),
        kvp("source", event.source),
        kvp("created_at", toBsonDate(now)));
    auto update = make_document(kvp("$inc", inc.view()), kvp("$set", setDoc.view()), kvp("$setOnInsert", setOnInsert.view()));

    mongocxx::options::update opts;
    opts.upsert(true);
    dailyStatsCollection_.update_one(filter.view(), update.view(), opts);
}

void PulseAnalyticsStorage::upsertZeroResultQuery(const pulse::PulseSearchAnalyticsEvent& event,
                                                  const std::string& date,
                                                  const std::chrono::system_clock::time_point& now) {
    auto filter = make_document(
        kvp("date", date),
        kvp("normalized_query_hash", event.normalizedQueryHash),
        kvp("source", event.source));

    auto inc = make_document(kvp("search_count", static_cast<int64_t>(1)));
    auto setDoc = make_document(
        kvp("updated_at", toBsonDate(now)),
        kvp("last_seen_at", toBsonDate(event.timestamp)),
        kvp("opportunity_score", 0));
    auto setOnInsert = make_document(
        kvp("date", date),
        kvp("normalized_query_hash", event.normalizedQueryHash),
        kvp("normalized_query", event.normalizedQuery),
        kvp("display_query", event.normalizedQuery),
        kvp("language_estimate", event.languageEstimate),
        kvp("source", event.source),
        kvp("created_at", toBsonDate(now)));
    auto update = make_document(kvp("$inc", inc.view()), kvp("$set", setDoc.view()), kvp("$setOnInsert", setOnInsert.view()));

    mongocxx::options::update opts;
    opts.upsert(true);
    zeroResultsCollection_.update_one(filter.view(), update.view(), opts);
}

PulseAnalyticsStorage::DateRange PulseAnalyticsStorage::buildDateRange(const std::string& rangeKey) const {
    DateRange range;
    range.key = rangeKey;
    range.end = std::chrono::system_clock::now();
    range.start = startOfUtcDay(range.end);

    if (range.key == "week") {
        range.start = range.end - std::chrono::hours(24 * 7);
    } else if (range.key == "month") {
        range.start = range.end - std::chrono::hours(24 * 30);
    } else if (range.key == "year") {
        range.start = range.end - std::chrono::hours(24 * 365);
    } else if (range.key == "all") {
        range.allTime = true;
        range.start = std::chrono::system_clock::time_point{};
    } else {
        range.key = "today";
    }

    return range;
}

PulseAnalyticsStorage::DateRange PulseAnalyticsStorage::previousDateRange(const DateRange& range) const {
    DateRange previous;
    previous.key = range.key;
    previous.allTime = false;

    if (range.allTime) {
        auto now = std::chrono::system_clock::now();
        previous.end = now - std::chrono::hours(24 * 30);
        previous.start = now - std::chrono::hours(24 * 60);
        return previous;
    }

    auto duration = range.end - range.start;
    previous.end = range.start;
    previous.start = range.start - duration;
    return previous;
}

int PulseAnalyticsStorage::candidateLimitForRange(const DateRange& range, int limit) const {
    if (range.key == "today") {
        return std::max(limit * 20, 100);
    }
    if (range.key == "week") {
        return std::max(limit * 40, 300);
    }
    if (range.key == "month") {
        return std::max(limit * 60, 600);
    }
    if (range.key == "year") {
        return std::max(limit * 100, 1000);
    }
    return std::max(limit * 120, 1500);
}

int PulseAnalyticsStorage::trendPointCountForRange(const DateRange& range) const {
    if (range.key == "today") {
        return 24;
    }
    if (range.key == "week") {
        return 7;
    }
    if (range.key == "month") {
        return 30;
    }
    return 12;
}

std::vector<PulseQueryMetric> PulseAnalyticsStorage::loadWindowMetrics(const DateRange& range, int limit) {
    bsoncxx::builder::basic::document filter;
    filter.append(kvp("granularity", "hour"), kvp("source", "api_search"));
    bsoncxx::builder::basic::document bucketRange;
    if (!range.allTime) {
        bucketRange.append(kvp("$gte", toBsonDate(range.start)));
        bucketRange.append(kvp("$lt", toBsonDate(range.end)));
        filter.append(kvp("bucket_start", bucketRange.view()));
    }

    mongocxx::options::find opts;
    auto sort = make_document(kvp("search_count", -1));
    opts.sort(sort.view());
    opts.limit(limit);

    struct Accumulator {
        std::string query;
        std::string hash;
        std::string language;
        int64_t count = 0;
    };

    std::unordered_map<std::string, Accumulator> byHash;
    auto cursor = queryBucketsCollection_.find(filter.view(), opts);
    for (const auto& doc : cursor) {
        std::string hash = readString(doc, "normalized_query_hash");
        std::string query = readString(doc, "display_query");
        if (hash.empty() || query.empty() || !pulse::PulseQueryNormalizer::isPublicSafeQuery(query, query)) {
            continue;
        }

        auto& acc = byHash[hash];
        if (acc.hash.empty()) {
            acc.hash = hash;
            acc.query = query;
            acc.language = readString(doc, "language_estimate", "unknown");
        }
        acc.count += readInt64(doc, "search_count");
    }

    std::vector<PulseQueryMetric> metrics;
    metrics.reserve(byHash.size());
    for (const auto& [_, acc] : byHash) {
        PulseQueryMetric metric;
        metric.query = acc.query;
        metric.normalizedQueryHash = acc.hash;
        metric.languageEstimate = acc.language;
        metric.internalCount = acc.count;
        metrics.push_back(metric);
    }

    return metrics;
}

std::string PulseAnalyticsStorage::utcDateString(const std::chrono::system_clock::time_point& timePoint) const {
    auto t = std::chrono::system_clock::to_time_t(timePoint);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d");
    return out.str();
}

std::chrono::system_clock::time_point PulseAnalyticsStorage::floorToHour(const std::chrono::system_clock::time_point& timePoint) const {
    auto sinceEpoch = std::chrono::duration_cast<std::chrono::hours>(timePoint.time_since_epoch());
    return std::chrono::system_clock::time_point{sinceEpoch};
}

std::chrono::system_clock::time_point PulseAnalyticsStorage::startOfUtcDay(const std::chrono::system_clock::time_point& timePoint) const {
    auto t = std::chrono::system_clock::to_time_t(timePoint);
    std::tm tm{};
    gmtime_r(&t, &tm);
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    auto dayStart = timegm(&tm);
    return std::chrono::system_clock::from_time_t(dayStart);
}

bsoncxx::types::b_date PulseAnalyticsStorage::toBsonDate(const std::chrono::system_clock::time_point& timePoint) const {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch());
    return bsoncxx::types::b_date{ms};
}

std::chrono::system_clock::time_point PulseAnalyticsStorage::fromBsonDate(const bsoncxx::types::b_date& date) const {
    return std::chrono::system_clock::time_point{std::chrono::milliseconds{date.to_int64()}};
}

int64_t PulseAnalyticsStorage::readInt64(const bsoncxx::document::view& doc, const std::string& field, int64_t fallback) const {
    auto element = doc[field];
    if (!element) {
        return fallback;
    }
    if (element.type() == bsoncxx::type::k_int64) {
        return element.get_int64().value;
    }
    if (element.type() == bsoncxx::type::k_int32) {
        return element.get_int32().value;
    }
    if (element.type() == bsoncxx::type::k_double) {
        return static_cast<int64_t>(element.get_double().value);
    }
    return fallback;
}

double PulseAnalyticsStorage::readDouble(const bsoncxx::document::view& doc, const std::string& field, double fallback) const {
    auto element = doc[field];
    if (!element) {
        return fallback;
    }
    if (element.type() == bsoncxx::type::k_double) {
        return element.get_double().value;
    }
    if (element.type() == bsoncxx::type::k_int64) {
        return static_cast<double>(element.get_int64().value);
    }
    if (element.type() == bsoncxx::type::k_int32) {
        return static_cast<double>(element.get_int32().value);
    }
    return fallback;
}

std::string PulseAnalyticsStorage::readString(const bsoncxx::document::view& doc, const std::string& field, const std::string& fallback) const {
    auto element = doc[field];
    if (!element || element.type() != bsoncxx::type::k_string) {
        return fallback;
    }
    return std::string(element.get_string().value);
}

std::string PulseAnalyticsStorage::toIsoString(const std::chrono::system_clock::time_point& timePoint) {
    auto t = std::chrono::system_clock::to_time_t(timePoint);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

} // namespace storage
} // namespace search_engine
