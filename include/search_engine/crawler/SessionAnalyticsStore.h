#pragma once

#include "SessionMetricsRecord.h"
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>
#include <algorithm>
#include <string>

/**
 * @brief Thread-safe in-memory store for SessionMetricsRecord (issue #15).
 *
 * Records are inserted at session completion and queried by:
 *   - sessionId
 *   - time window
 *   - "all" (with optional cap)
 *
 * A capacity bound keeps memory bounded; oldest records evict first.
 *
 * The store is an interface (virtual methods) so it can later be backed by
 * MongoDB or another durable backend without changing callers. The default
 * in-memory implementation is provided here for the API endpoints, tests,
 * and dev usage. A persistent backend can subclass and override.
 */
class ISessionAnalyticsStore {
public:
    virtual ~ISessionAnalyticsStore() = default;

    virtual void put(const SessionMetricsRecord& record) = 0;
    virtual std::optional<SessionMetricsRecord> get(const std::string& sessionId) const = 0;
    virtual std::vector<SessionMetricsRecord> getAll(size_t limit = 0) const = 0;
    virtual std::vector<SessionMetricsRecord> getInWindow(
        std::chrono::system_clock::time_point from,
        std::chrono::system_clock::time_point to) const = 0;
    virtual size_t size() const = 0;
    virtual void clear() = 0;
};

class InMemorySessionAnalyticsStore : public ISessionAnalyticsStore {
public:
    explicit InMemorySessionAnalyticsStore(size_t capacity = 10000)
        : capacity_(capacity) {}

    void put(const SessionMetricsRecord& record) override {
        std::lock_guard<std::mutex> lock(mutex_);
        // Overwrite on duplicate id (idempotent updates allowed).
        auto idxIt = idIndex_.find(record.sessionId);
        if (idxIt != idIndex_.end()) {
            records_[idxIt->second] = record;
            return;
        }
        records_.push_back(record);
        idIndex_[record.sessionId] = records_.size() - 1;
        evictIfNeededLocked();
    }

    std::optional<SessionMetricsRecord> get(const std::string& sessionId) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = idIndex_.find(sessionId);
        if (it == idIndex_.end()) return std::nullopt;
        return records_[it->second];
    }

    std::vector<SessionMetricsRecord> getAll(size_t limit = 0) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (limit == 0 || limit >= records_.size()) {
            return records_;
        }
        // Return the most recent `limit` entries.
        return std::vector<SessionMetricsRecord>(
            records_.end() - static_cast<ptrdiff_t>(limit), records_.end());
    }

    std::vector<SessionMetricsRecord> getInWindow(
        std::chrono::system_clock::time_point from,
        std::chrono::system_clock::time_point to) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<SessionMetricsRecord> out;
        out.reserve(records_.size());
        for (const auto& r : records_) {
            if (r.startedAt >= from && r.startedAt < to) {
                out.push_back(r);
            }
        }
        return out;
    }

    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_.size();
    }

    void clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        records_.clear();
        idIndex_.clear();
    }

private:
    void evictIfNeededLocked() {
        if (records_.size() <= capacity_) return;
        size_t toDrop = records_.size() - capacity_;
        // Erase oldest entries (front).
        for (size_t i = 0; i < toDrop; ++i) {
            idIndex_.erase(records_[i].sessionId);
        }
        records_.erase(records_.begin(), records_.begin() + static_cast<ptrdiff_t>(toDrop));
        // Rebuild index since positions shifted.
        idIndex_.clear();
        for (size_t i = 0; i < records_.size(); ++i) {
            idIndex_[records_[i].sessionId] = i;
        }
    }

    mutable std::mutex mutex_;
    std::vector<SessionMetricsRecord> records_;
    std::unordered_map<std::string, size_t> idIndex_;
    size_t capacity_;
};
