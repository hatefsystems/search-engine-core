#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <cstdint>

/**
 * @brief Immutable per-session metrics record (issue #15).
 *
 * Produced from CrawlResult vectors at session completion and persisted
 * via SessionAnalyticsStore for historical reporting and comparisons.
 *
 * The struct is intentionally a plain value type — no atomics, no
 * threading — so it can be passed around freely, serialized to JSON,
 * compared, and bucketed for trend reports.
 */
struct SessionMetricsRecord {
    // Identity
    std::string sessionId;
    std::string seedUrl;
    std::string seedDomain;

    // Timing
    std::chrono::system_clock::time_point startedAt{};
    std::chrono::system_clock::time_point finishedAt{};
    // Wall clock duration of the session in milliseconds.
    int64_t durationMs{0};

    // Counts
    size_t totalUrls{0};
    size_t successfulUrls{0};
    size_t failedUrls{0};
    size_t retriedUrls{0};
    size_t totalRetryAttempts{0};

    // Bytes downloaded (sum over successful results).
    size_t totalBytes{0};

    // Latency aggregates per-URL in milliseconds (finishedAt - startedAt).
    int64_t avgLatencyMs{0};
    int64_t p50LatencyMs{0};
    int64_t p95LatencyMs{0};
    int64_t p99LatencyMs{0};
    int64_t maxLatencyMs{0};

    // HTTP status code distribution (e.g. {200: 42, 404: 3, 500: 1}).
    std::unordered_map<int, size_t> statusCodeCounts;

    // Failure type distribution (FailureType enum as int, since storing
    // the enum keeps this header free of the enum dependency).
    std::unordered_map<int, size_t> failureTypeCounts;

    // Derived rates (computed at build time for convenience).
    double getSuccessRate() const {
        return totalUrls > 0 ? static_cast<double>(successfulUrls) / static_cast<double>(totalUrls) : 0.0;
    }
    double getFailureRate() const {
        return totalUrls > 0 ? static_cast<double>(failedUrls) / static_cast<double>(totalUrls) : 0.0;
    }
    double getRetryRate() const {
        return totalUrls > 0 ? static_cast<double>(retriedUrls) / static_cast<double>(totalUrls) : 0.0;
    }
    // Throughput: URLs per second across session duration.
    double getThroughput() const {
        return durationMs > 0 ? (static_cast<double>(totalUrls) * 1000.0) / static_cast<double>(durationMs) : 0.0;
    }
};
