#pragma once

#include "SessionMetricsRecord.h"
#include "models/CrawlResult.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <unordered_map>
#include <map>

/**
 * @brief Pure functions for session-metrics aggregation, comparison, and
 *        trend reporting (issue #15).
 *
 * None of these touch storage, threads, network — they take values in and
 * return values out. That keeps everything trivially unit-testable.
 */
namespace SessionAnalytics {

/**
 * Build a SessionMetricsRecord from a finished session's CrawlResult vector
 * plus session-level metadata (id, seed url/domain, start/finish times).
 */
inline SessionMetricsRecord buildFromResults(const std::string& sessionId,
                                             const std::string& seedUrl,
                                             const std::string& seedDomain,
                                             std::chrono::system_clock::time_point startedAt,
                                             std::chrono::system_clock::time_point finishedAt,
                                             const std::vector<CrawlResult>& results) {
    SessionMetricsRecord r;
    r.sessionId = sessionId;
    r.seedUrl = seedUrl;
    r.seedDomain = seedDomain;
    r.startedAt = startedAt;
    r.finishedAt = finishedAt;
    r.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(finishedAt - startedAt).count();
    if (r.durationMs < 0) r.durationMs = 0;

    std::vector<int64_t> latencies;
    latencies.reserve(results.size());

    for (const auto& cr : results) {
        r.totalUrls++;
        if (cr.success) {
            r.successfulUrls++;
            r.totalBytes += cr.contentSize;
        } else if (cr.crawlStatus == "failed") {
            r.failedUrls++;
        }
        if (cr.retryCount > 0) {
            r.retriedUrls++;
            r.totalRetryAttempts += static_cast<size_t>(cr.retryCount);
        }
        if (cr.statusCode != 0) {
            r.statusCodeCounts[cr.statusCode]++;
        }
        r.failureTypeCounts[static_cast<int>(cr.failureType)]++;

        // Compute per-URL latency only if we have a sane started/finished
        // timestamp pair on the result.
        auto perUrlMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            cr.finishedAt - cr.startedAt).count();
        if (perUrlMs > 0) latencies.push_back(perUrlMs);
    }

    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        int64_t sum = 0;
        for (auto v : latencies) sum += v;
        r.avgLatencyMs = sum / static_cast<int64_t>(latencies.size());
        auto percentile = [&](double p) -> int64_t {
            // Nearest-rank percentile; safe for small N.
            if (latencies.empty()) return 0;
            size_t idx = static_cast<size_t>(std::ceil(p * latencies.size())) ;
            if (idx == 0) idx = 1;
            if (idx > latencies.size()) idx = latencies.size();
            return latencies[idx - 1];
        };
        r.p50LatencyMs = percentile(0.50);
        r.p95LatencyMs = percentile(0.95);
        r.p99LatencyMs = percentile(0.99);
        r.maxLatencyMs = latencies.back();
    }

    return r;
}

/**
 * Summary across many sessions — used as the response for "list all" and
 * for compare. Sums and averages roll up cleanly.
 */
struct AggregateSummary {
    size_t sessionCount{0};
    size_t totalUrls{0};
    size_t successfulUrls{0};
    size_t failedUrls{0};
    size_t retriedUrls{0};
    size_t totalRetryAttempts{0};
    size_t totalBytes{0};
    // Averaged across sessions (so a 0-URL session still pulls the average
    // toward 0; callers can filter empty sessions if they want).
    double avgSuccessRate{0.0};
    double avgRetryRate{0.0};
    double avgThroughput{0.0};
    int64_t avgDurationMs{0};
    int64_t avgLatencyMs{0};
};

inline AggregateSummary summarize(const std::vector<SessionMetricsRecord>& records) {
    AggregateSummary s;
    s.sessionCount = records.size();
    if (records.empty()) return s;

    double sumSuccessRate = 0, sumRetryRate = 0, sumThroughput = 0;
    int64_t sumDuration = 0, sumLatency = 0;
    for (const auto& r : records) {
        s.totalUrls += r.totalUrls;
        s.successfulUrls += r.successfulUrls;
        s.failedUrls += r.failedUrls;
        s.retriedUrls += r.retriedUrls;
        s.totalRetryAttempts += r.totalRetryAttempts;
        s.totalBytes += r.totalBytes;
        sumSuccessRate += r.getSuccessRate();
        sumRetryRate += r.getRetryRate();
        sumThroughput += r.getThroughput();
        sumDuration += r.durationMs;
        sumLatency += r.avgLatencyMs;
    }
    s.avgSuccessRate = sumSuccessRate / static_cast<double>(records.size());
    s.avgRetryRate = sumRetryRate / static_cast<double>(records.size());
    s.avgThroughput = sumThroughput / static_cast<double>(records.size());
    s.avgDurationMs = sumDuration / static_cast<int64_t>(records.size());
    s.avgLatencyMs = sumLatency / static_cast<int64_t>(records.size());
    return s;
}

/**
 * Pairwise comparison result between two sessions — used by the compare API.
 */
struct ComparisonResult {
    std::string sessionA;
    std::string sessionB;
    double successRateDelta{0.0};       // B - A
    double retryRateDelta{0.0};         // B - A
    double throughputDelta{0.0};        // B - A
    int64_t durationDeltaMs{0};         // B - A
    int64_t avgLatencyDeltaMs{0};       // B - A
    // > 0 means B is faster / better depending on the field.
};

inline ComparisonResult compare(const SessionMetricsRecord& a, const SessionMetricsRecord& b) {
    ComparisonResult c;
    c.sessionA = a.sessionId;
    c.sessionB = b.sessionId;
    c.successRateDelta = b.getSuccessRate() - a.getSuccessRate();
    c.retryRateDelta = b.getRetryRate() - a.getRetryRate();
    c.throughputDelta = b.getThroughput() - a.getThroughput();
    c.durationDeltaMs = b.durationMs - a.durationMs;
    c.avgLatencyDeltaMs = b.avgLatencyMs - a.avgLatencyMs;
    return c;
}

/**
 * Trend bucket — a time slice (e.g. one hour, one day) with rolled-up stats.
 * Buckets are keyed by the start of the bucket (Unix epoch ms).
 */
struct TrendBucket {
    int64_t bucketStartMs{0};
    int64_t bucketEndMs{0};
    AggregateSummary summary;
};

/**
 * Bucket sessions by their `startedAt` into fixed-size time slices and
 * summarize each bucket. Returns buckets in ascending time order.
 *
 * - `bucketWidth` controls the slice size (e.g. 1h, 24h, 7d).
 * - Sessions outside [windowStart, windowEnd) are skipped.
 */
inline std::vector<TrendBucket> trends(const std::vector<SessionMetricsRecord>& records,
                                       std::chrono::system_clock::time_point windowStart,
                                       std::chrono::system_clock::time_point windowEnd,
                                       std::chrono::milliseconds bucketWidth) {
    std::vector<TrendBucket> out;
    if (bucketWidth.count() <= 0) return out;
    if (windowEnd <= windowStart) return out;

    auto winStartMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        windowStart.time_since_epoch()).count();
    auto winEndMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        windowEnd.time_since_epoch()).count();
    int64_t bw = bucketWidth.count();

    // Group records into bucket indices.
    std::map<int64_t, std::vector<SessionMetricsRecord>> buckets;
    for (const auto& r : records) {
        auto startedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            r.startedAt.time_since_epoch()).count();
        if (startedMs < winStartMs || startedMs >= winEndMs) continue;
        int64_t idx = (startedMs - winStartMs) / bw;
        buckets[idx].push_back(r);
    }

    // Emit one bucket per index that actually has data.
    for (const auto& [idx, recs] : buckets) {
        TrendBucket tb;
        tb.bucketStartMs = winStartMs + idx * bw;
        tb.bucketEndMs = tb.bucketStartMs + bw;
        tb.summary = summarize(recs);
        out.push_back(tb);
    }
    return out;
}

}  // namespace SessionAnalytics
