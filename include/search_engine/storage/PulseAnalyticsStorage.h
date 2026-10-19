#pragma once

#include "../../infrastructure.h"
#include "../pulse/PulseSearchAnalyticsEvent.h"
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/types.hpp>
#include <chrono>
#include <memory>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace search_engine {
namespace storage {

struct PulseQueryMetric {
    std::string query;
    std::string normalizedQueryHash;
    std::string languageEstimate = "unknown";
    int64_t internalCount = 0;
    int score = 0;
    int trendScore = 0;
    bool enoughData = false;
};

struct PulseTrendPoint {
    std::chrono::system_clock::time_point bucketStart;
    int score = 0;
};

struct PulseSummary {
    bool enoughData = false;
    int activityScore = 0;
    int successScore = 0;
    int zeroResultOpportunityScore = 0;
    int speedScore = 0;
};

class PulseAnalyticsStorage {
public:
    explicit PulseAnalyticsStorage(const std::string& connectionString = "mongodb://localhost:27017",
                                   const std::string& databaseName = "hatef_pulse",
                                   int rawRetentionDays = 14);

    Result<bool> ensureIndexes();
    Result<size_t> storeEventsBatch(const std::vector<pulse::PulseSearchAnalyticsEvent>& events);
    Result<PulseSummary> getSummary(const std::string& rangeKey = "today", int minEvents = 3);
    Result<std::vector<PulseQueryMetric>> getTopQueries(int limit = 10, const std::string& rangeKey = "today", int minEvents = 3);
    Result<std::vector<PulseQueryMetric>> getRisingQueries(int limit = 10, const std::string& rangeKey = "today", int minEvents = 3);
    Result<std::vector<PulseQueryMetric>> getZeroResultQueries(int limit = 10, const std::string& rangeKey = "today", int minEvents = 3);
    Result<std::vector<PulseTrendPoint>> getQueryTrend(const std::string& rawQuery, const std::string& rangeKey = "today", int minEvents = 3);

    static std::string toIsoString(const std::chrono::system_clock::time_point& timePoint);

private:
    struct DateRange {
        std::string key = "today";
        std::chrono::system_clock::time_point start;
        std::chrono::system_clock::time_point end;
        bool allTime = false;
    };

    bsoncxx::document::value eventToBson(const pulse::PulseSearchAnalyticsEvent& event) const;
    void upsertEventAggregates(const pulse::PulseSearchAnalyticsEvent& event);
    void upsertQueryBucket(const pulse::PulseSearchAnalyticsEvent& event,
                           const std::chrono::system_clock::time_point& bucketStart,
                           const std::chrono::system_clock::time_point& now);
    void upsertDailyStats(const pulse::PulseSearchAnalyticsEvent& event,
                          const std::string& date,
                          const std::chrono::system_clock::time_point& now);
    void upsertZeroResultQuery(const pulse::PulseSearchAnalyticsEvent& event,
                               const std::string& date,
                               const std::chrono::system_clock::time_point& now);

    DateRange buildDateRange(const std::string& rangeKey) const;
    DateRange previousDateRange(const DateRange& range) const;
    int candidateLimitForRange(const DateRange& range, int limit) const;
    int trendPointCountForRange(const DateRange& range) const;
    std::vector<PulseQueryMetric> loadWindowMetrics(const DateRange& range,
                                                    int limit);
    std::string utcDateString(const std::chrono::system_clock::time_point& timePoint) const;
    std::chrono::system_clock::time_point floorToHour(const std::chrono::system_clock::time_point& timePoint) const;
    std::chrono::system_clock::time_point startOfUtcDay(const std::chrono::system_clock::time_point& timePoint) const;
    bsoncxx::types::b_date toBsonDate(const std::chrono::system_clock::time_point& timePoint) const;
    std::chrono::system_clock::time_point fromBsonDate(const bsoncxx::types::b_date& date) const;
    int64_t readInt64(const bsoncxx::document::view& doc, const std::string& field, int64_t fallback = 0) const;
    double readDouble(const bsoncxx::document::view& doc, const std::string& field, double fallback = 0.0) const;
    std::string readString(const bsoncxx::document::view& doc, const std::string& field, const std::string& fallback = "") const;

    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection rawEventsCollection_;
    mongocxx::collection queryBucketsCollection_;
    mongocxx::collection dailyStatsCollection_;
    mongocxx::collection trendsCollection_;
    mongocxx::collection zeroResultsCollection_;
    int rawRetentionDays_ = 14;
};

} // namespace storage
} // namespace search_engine
