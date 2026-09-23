#pragma once

#include "CrawlPriority.h"
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <optional>

/**
 * @brief A pending crawl session that is waiting for a free slot.
 *
 * Carries everything CrawlerManager needs to actually start the crawl later.
 * The crawler instance / thread are NOT created until the session is dequeued
 * and dispatched — keeping pending sessions cheap.
 */
struct PendingSessionEntry {
    std::string sessionId;
    std::string url;
    bool force{false};
    CrawlPriority priority{CrawlPriority::NORMAL};
    std::chrono::system_clock::time_point queuedAt{std::chrono::system_clock::now()};
    // For session-level retry: don't dispatch until this time is reached.
    std::chrono::system_clock::time_point readyAt{std::chrono::system_clock::now()};
    // Retry attempt counter (0 for first attempt).
    int retryCount{0};
};

/**
 * @brief Thread-safe priority queue for pending crawl sessions.
 *
 * Ordering: higher CrawlPriority value first; ties broken by earlier queuedAt
 * (FIFO within a priority level). Entries whose readyAt is still in the future
 * are skipped by tryPopReady() until their delay elapses (used for retry
 * backoff at session level).
 *
 * Exposes inspection (size, snapshot) for the GET /api/crawl/queue endpoint.
 */
class SessionPriorityQueue {
public:
    SessionPriorityQueue() = default;

    // Add a pending session. Returns its position (0-indexed) in priority order.
    void push(const PendingSessionEntry& entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back(entry);
        sortLocked();
    }

    // Pop the highest-priority entry whose readyAt <= now. Returns nullopt if
    // queue is empty or no entry is ready yet.
    std::optional<PendingSessionEntry> tryPopReady(
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->readyAt <= now) {
                PendingSessionEntry out = *it;
                entries_.erase(it);
                return out;
            }
        }
        return std::nullopt;
    }

    // Remove (cancel) a pending session by id. Returns true if removed.
    bool remove(const std::string& sessionId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(entries_.begin(), entries_.end(),
            [&](const PendingSessionEntry& e) { return e.sessionId == sessionId; });
        if (it == entries_.end()) return false;
        entries_.erase(it);
        return true;
    }

    // Check whether a session id is pending.
    bool contains(const std::string& sessionId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::any_of(entries_.begin(), entries_.end(),
            [&](const PendingSessionEntry& e) { return e.sessionId == sessionId; });
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.empty();
    }

    // Returns a snapshot of the queue in priority order. Safe for read-only
    // inspection (e.g. for the GET /api/crawl/queue endpoint).
    std::vector<PendingSessionEntry> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

private:
    void sortLocked() {
        std::sort(entries_.begin(), entries_.end(),
            [](const PendingSessionEntry& a, const PendingSessionEntry& b) {
                // Higher priority value comes first.
                if (a.priority != b.priority) {
                    return static_cast<int>(a.priority) > static_cast<int>(b.priority);
                }
                // FIFO within the same priority level.
                return a.queuedAt < b.queuedAt;
            });
    }

    mutable std::mutex mutex_;
    std::vector<PendingSessionEntry> entries_;
};
