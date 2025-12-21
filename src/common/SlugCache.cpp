#include "../../include/search_engine/common/SlugCache.h"
#include <algorithm>

namespace search_engine {
namespace common {

SlugCache::SlugCache(int ttlSeconds)
    : ttlSeconds_(ttlSeconds) {
}

std::optional<std::string> SlugCache::get(const std::string& slug) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(slug);
    if (it == cache_.end()) {
        return std::nullopt;
    }

    const CacheEntry& entry = it->second;
    if (!entry.isValid || isExpired(entry)) {
        // Remove expired/invalid entry
        cache_.erase(it);
        return std::nullopt;
    }

    return entry.profileId;
}

void SlugCache::put(const std::string& slug, const std::string& profileId) {
    std::lock_guard<std::mutex> lock(mutex_);

    CacheEntry entry(profileId, getExpirationTime());
    cache_[slug] = entry;
}

void SlugCache::remove(const std::string& slug) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(slug);
}

size_t SlugCache::cleanupExpired() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t removed = 0;
    auto now = std::chrono::system_clock::now();

    for (auto it = cache_.begin(); it != cache_.end(); ) {
        if (isExpired(it->second)) {
            it = cache_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    return removed;
}

std::pair<size_t, size_t> SlugCache::getStats() {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t total = cache_.size();
    size_t valid = 0;

    auto now = std::chrono::system_clock::now();
    for (const auto& pair : cache_) {
        if (pair.second.isValid && !isExpired(pair.second)) {
            ++valid;
        }
    }

    return {total, valid};
}

void SlugCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

bool SlugCache::isExpired(const CacheEntry& entry) const {
    auto now = std::chrono::system_clock::now();
    return now >= entry.expiresAt;
}

std::chrono::system_clock::time_point SlugCache::getExpirationTime() const {
    return std::chrono::system_clock::now() + std::chrono::seconds(ttlSeconds_);
}

} // namespace common
} // namespace search_engine
