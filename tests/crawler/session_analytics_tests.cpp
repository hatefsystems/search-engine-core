#include <catch2/catch_test_macros.hpp>
#include "search_engine/crawler/SessionAnalytics.h"
#include "search_engine/crawler/SessionAnalyticsStore.h"

#include <chrono>
#include <vector>

using namespace std::chrono_literals;

// ---------- Helpers ----------

static CrawlResult makeResult(const std::string& url,
                              bool success,
                              int statusCode,
                              int64_t latencyMs,
                              int retryCount = 0,
                              size_t bytes = 0) {
    CrawlResult r;
    r.url = url;
    r.statusCode = statusCode;
    r.success = success;
    r.crawlStatus = success ? "downloaded" : "failed";
    r.retryCount = retryCount;
    r.contentSize = bytes;
    r.domain = "example.com";
    auto now = std::chrono::system_clock::now();
    r.startedAt = now;
    r.finishedAt = now + std::chrono::milliseconds(latencyMs);
    return r;
}

static SessionMetricsRecord makeRecord(const std::string& id,
                                       size_t total,
                                       size_t success,
                                       size_t failed,
                                       size_t retried,
                                       int64_t durationMs,
                                       int64_t avgLatencyMs,
                                       std::chrono::system_clock::time_point startedAt) {
    SessionMetricsRecord r;
    r.sessionId = id;
    r.startedAt = startedAt;
    r.finishedAt = startedAt + std::chrono::milliseconds(durationMs);
    r.durationMs = durationMs;
    r.totalUrls = total;
    r.successfulUrls = success;
    r.failedUrls = failed;
    r.retriedUrls = retried;
    r.avgLatencyMs = avgLatencyMs;
    return r;
}

// ---------- buildFromResults ----------

TEST_CASE("buildFromResults computes counts and rates", "[SessionAnalytics]") {
    auto now = std::chrono::system_clock::now();
    std::vector<CrawlResult> results = {
        makeResult("u1", true,  200, 100, 0, 1000),
        makeResult("u2", true,  200, 200, 0, 2000),
        makeResult("u3", false, 500, 300, 1, 0),
        makeResult("u4", true,  200, 400, 2, 500),
        makeResult("u5", false, 404, 50,  0, 0),
    };

    auto r = SessionAnalytics::buildFromResults(
        "s1", "https://example.com", "example.com",
        now, now + 5s, results);

    REQUIRE(r.sessionId == "s1");
    REQUIRE(r.totalUrls == 5);
    REQUIRE(r.successfulUrls == 3);
    REQUIRE(r.failedUrls == 2);
    REQUIRE(r.retriedUrls == 2);                  // u3 and u4
    REQUIRE(r.totalRetryAttempts == 3);           // 1 + 2
    REQUIRE(r.totalBytes == 3500);                // 1000+2000+500
    REQUIRE(r.statusCodeCounts[200] == 3);
    REQUIRE(r.statusCodeCounts[500] == 1);
    REQUIRE(r.statusCodeCounts[404] == 1);
    REQUIRE(r.durationMs == 5000);
    REQUIRE(r.getSuccessRate() == 0.6);
    REQUIRE(r.getFailureRate() == 0.4);
}

TEST_CASE("buildFromResults computes latency percentiles", "[SessionAnalytics]") {
    auto now = std::chrono::system_clock::now();
    std::vector<CrawlResult> results = {
        makeResult("u1", true, 200, 10),
        makeResult("u2", true, 200, 20),
        makeResult("u3", true, 200, 30),
        makeResult("u4", true, 200, 40),
        makeResult("u5", true, 200, 50),
        makeResult("u6", true, 200, 60),
        makeResult("u7", true, 200, 70),
        makeResult("u8", true, 200, 80),
        makeResult("u9", true, 200, 90),
        makeResult("u10", true, 200, 100),
    };
    auto r = SessionAnalytics::buildFromResults("s", "u", "d", now, now + 1s, results);
    REQUIRE(r.avgLatencyMs == 55);            // (10+20+...+100)/10
    REQUIRE(r.p50LatencyMs == 50);            // 50th percentile, nearest-rank
    REQUIRE(r.p95LatencyMs == 100);           // ceil(0.95*10) = 10 -> idx 10 -> 100
    REQUIRE(r.maxLatencyMs == 100);
}

TEST_CASE("buildFromResults with empty results yields empty record", "[SessionAnalytics]") {
    auto now = std::chrono::system_clock::now();
    auto r = SessionAnalytics::buildFromResults("s", "u", "d", now, now + 1s, {});
    REQUIRE(r.totalUrls == 0);
    REQUIRE(r.successfulUrls == 0);
    REQUIRE(r.getSuccessRate() == 0.0);
    REQUIRE(r.getThroughput() == 0.0);
    REQUIRE(r.avgLatencyMs == 0);
}

// ---------- summarize ----------

TEST_CASE("summarize rolls up multiple records", "[SessionAnalytics]") {
    auto now = std::chrono::system_clock::now();
    std::vector<SessionMetricsRecord> recs = {
        makeRecord("a", 10, 8, 2, 1, 1000, 100, now),
        makeRecord("b", 20, 18, 2, 4, 2000, 200, now + 1s),
    };
    // Manually set rates so summarize averages something meaningful.
    auto s = SessionAnalytics::summarize(recs);
    REQUIRE(s.sessionCount == 2);
    REQUIRE(s.totalUrls == 30);
    REQUIRE(s.successfulUrls == 26);
    REQUIRE(s.failedUrls == 4);
    REQUIRE(s.retriedUrls == 5);
    REQUIRE(s.avgDurationMs == 1500);
    REQUIRE(s.avgLatencyMs == 150);
    // avgSuccessRate = (0.8 + 0.9) / 2 = 0.85
    REQUIRE(s.avgSuccessRate == 0.85);
}

TEST_CASE("summarize handles empty input", "[SessionAnalytics]") {
    auto s = SessionAnalytics::summarize({});
    REQUIRE(s.sessionCount == 0);
    REQUIRE(s.totalUrls == 0);
    REQUIRE(s.avgSuccessRate == 0.0);
}

// ---------- compare ----------

TEST_CASE("compare reports B-A deltas", "[SessionAnalytics]") {
    auto now = std::chrono::system_clock::now();
    auto a = makeRecord("a", 10, 5, 5, 1, 2000, 200, now);   // success 0.5, retry 0.1
    auto b = makeRecord("b", 10, 9, 1, 3, 1000, 100, now);   // success 0.9, retry 0.3
    auto c = SessionAnalytics::compare(a, b);
    REQUIRE(c.sessionA == "a");
    REQUIRE(c.sessionB == "b");
    REQUIRE(c.successRateDelta == 0.4);     // 0.9 - 0.5
    REQUIRE(c.retryRateDelta == 0.2);       // 0.3 - 0.1
    REQUIRE(c.durationDeltaMs == -1000);    // b is 1s shorter
    REQUIRE(c.avgLatencyDeltaMs == -100);   // b is 100ms faster
}

// ---------- trends ----------

TEST_CASE("trends buckets sessions into hourly slices", "[SessionAnalytics]") {
    // Pin a known start so bucket boundaries are predictable.
    auto base = std::chrono::system_clock::time_point{} + std::chrono::hours(1000);

    std::vector<SessionMetricsRecord> recs = {
        makeRecord("h0_a", 10, 8, 2, 0, 500, 50, base + 5min),
        makeRecord("h0_b", 20, 16, 4, 0, 700, 70, base + 30min),
        makeRecord("h1_a", 5, 5, 0, 0, 200, 20, base + 1h + 10min),
        makeRecord("h2_a", 8, 4, 4, 0, 800, 80, base + 2h + 1min),
        // Outside window (will be filtered out).
        makeRecord("out", 1, 1, 0, 0, 100, 10, base + 24h),
    };

    auto buckets = SessionAnalytics::trends(
        recs, base, base + 3h, std::chrono::hours(1));

    REQUIRE(buckets.size() == 3);
    REQUIRE(buckets[0].summary.sessionCount == 2);   // h0
    REQUIRE(buckets[0].summary.totalUrls == 30);
    REQUIRE(buckets[1].summary.sessionCount == 1);   // h1
    REQUIRE(buckets[1].summary.totalUrls == 5);
    REQUIRE(buckets[2].summary.sessionCount == 1);   // h2
    REQUIRE(buckets[2].summary.totalUrls == 8);
}

TEST_CASE("trends with zero-width bucket returns empty", "[SessionAnalytics]") {
    auto now = std::chrono::system_clock::now();
    auto buckets = SessionAnalytics::trends({}, now, now + 1h, 0ms);
    REQUIRE(buckets.empty());
}

// ---------- InMemorySessionAnalyticsStore ----------

TEST_CASE("InMemoryStore put/get/getAll", "[SessionAnalyticsStore]") {
    InMemorySessionAnalyticsStore store;
    auto now = std::chrono::system_clock::now();
    store.put(makeRecord("a", 1, 1, 0, 0, 100, 10, now));
    store.put(makeRecord("b", 2, 2, 0, 0, 200, 20, now + 1s));
    REQUIRE(store.size() == 2);

    auto g = store.get("a");
    REQUIRE(g.has_value());
    REQUIRE(g->totalUrls == 1);
    REQUIRE_FALSE(store.get("missing").has_value());

    auto all = store.getAll();
    REQUIRE(all.size() == 2);

    // limit returns most recent
    auto recent = store.getAll(1);
    REQUIRE(recent.size() == 1);
    REQUIRE(recent[0].sessionId == "b");
}

TEST_CASE("InMemoryStore put is idempotent on same id", "[SessionAnalyticsStore]") {
    InMemorySessionAnalyticsStore store;
    auto now = std::chrono::system_clock::now();
    store.put(makeRecord("a", 1, 1, 0, 0, 100, 10, now));
    store.put(makeRecord("a", 5, 5, 0, 0, 500, 50, now));   // overwrite
    REQUIRE(store.size() == 1);
    auto g = store.get("a");
    REQUIRE(g.has_value());
    REQUIRE(g->totalUrls == 5);
}

TEST_CASE("InMemoryStore evicts oldest beyond capacity", "[SessionAnalyticsStore]") {
    InMemorySessionAnalyticsStore store(3);
    auto now = std::chrono::system_clock::now();
    store.put(makeRecord("a", 1, 1, 0, 0, 100, 10, now));
    store.put(makeRecord("b", 1, 1, 0, 0, 100, 10, now));
    store.put(makeRecord("c", 1, 1, 0, 0, 100, 10, now));
    store.put(makeRecord("d", 1, 1, 0, 0, 100, 10, now));   // pushes a out
    REQUIRE(store.size() == 3);
    REQUIRE_FALSE(store.get("a").has_value());
    REQUIRE(store.get("b").has_value());
    REQUIRE(store.get("c").has_value());
    REQUIRE(store.get("d").has_value());
}

TEST_CASE("InMemoryStore getInWindow filters by startedAt", "[SessionAnalyticsStore]") {
    InMemorySessionAnalyticsStore store;
    auto base = std::chrono::system_clock::now();
    store.put(makeRecord("old",  1, 1, 0, 0, 100, 10, base - 2h));
    store.put(makeRecord("mid1", 1, 1, 0, 0, 100, 10, base - 30min));
    store.put(makeRecord("mid2", 1, 1, 0, 0, 100, 10, base - 10min));
    store.put(makeRecord("future", 1, 1, 0, 0, 100, 10, base + 10min));

    auto inWin = store.getInWindow(base - 1h, base);
    REQUIRE(inWin.size() == 2);
}

TEST_CASE("InMemoryStore clear empties the store", "[SessionAnalyticsStore]") {
    InMemorySessionAnalyticsStore store;
    auto now = std::chrono::system_clock::now();
    store.put(makeRecord("a", 1, 1, 0, 0, 100, 10, now));
    REQUIRE(store.size() == 1);
    store.clear();
    REQUIRE(store.size() == 0);
    REQUIRE_FALSE(store.get("a").has_value());
}
