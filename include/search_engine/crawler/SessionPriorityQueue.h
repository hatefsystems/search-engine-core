#pragma once

#include "CrawlPriority.h"
#include <algorithm>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct PendingSessionEntry {
    std::string sessionId;
    CrawlPriority priority = CrawlPriority::NORMAL;
};

// Bounded admission queue. Insertion order defines FIFO within a priority;
// it does not depend on wall-clock timestamps. URLFrontier owns retry/backoff.
class SessionPriorityQueue {
  public:
    explicit SessionPriorityQueue(size_t capacity = 100) : capacity_(capacity) {}

    bool push(const PendingSessionEntry &entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (entries_.size() >= capacity_ ||
            std::any_of(entries_.begin(), entries_.end(),
                        [&](const auto &e) { return e.sessionId == entry.sessionId; }))
            return false;
        // Insert after peers, including simultaneous arrivals.
        auto position = std::find_if(entries_.begin(), entries_.end(), [&](const auto &e) {
            return static_cast<int>(e.priority) < static_cast<int>(entry.priority);
        });
        entries_.insert(position, entry);
        return true;
    }

    std::optional<PendingSessionEntry> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (entries_.empty())
            return std::nullopt;
        auto entry = entries_.front();
        entries_.erase(entries_.begin());
        return entry;
    }

    bool remove(const std::string &id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(entries_.begin(), entries_.end(),
                               [&](const auto &e) { return e.sessionId == id; });
        if (it == entries_.end())
            return false;
        entries_.erase(it);
        return true;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }
    bool empty() const { return size() == 0; }
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

  private:
    const size_t capacity_;
    mutable std::mutex mutex_;
    std::vector<PendingSessionEntry> entries_;
};
