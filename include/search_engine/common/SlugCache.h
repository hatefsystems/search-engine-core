#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <optional>

namespace search_engine {
namespace common {

/**
 * SlugCache - In-memory cache for slug-to-profile-id mapping
 *
 * Provides fast lookups for profile slugs with TTL-based expiration
 * and automatic cleanup of expired entries.
 */
class SlugCache {
public:
    /**
     * Cache entry structure
     */
    struct CacheEntry {
        std::string profileId;
        std::chrono::system_clock::time_point expiresAt;
        bool isValid = true;

        CacheEntry() = default;
        CacheEntry(const std::string& id, std::chrono::system_clock::time_point expiry)
            : profileId(id), expiresAt(expiry), isValid(true) {}
    };

    /**
     * Constructor
     * @param ttlSeconds Time-to-live in seconds (default: 300 = 5 minutes)
     * @param maxSize Maximum number of entries before eviction (default: 10000)
     */
    explicit SlugCache(int ttlSeconds = 300, size_t maxSize = 10000);

    /**
     * Get cached profile ID for slug
     * @param slug The slug to look up
     * @return Optional profile ID if found and not expired
     */
    std::optional<std::string> get(const std::string& slug);

    /**
     * Store slug-to-profile-id mapping in cache
     * @param slug The slug
     * @param profileId The profile ID
     */
    void put(const std::string& slug, const std::string& profileId);

    /**
     * Remove slug from cache
     * @param slug The slug to remove
     */
    void remove(const std::string& slug);

    /**
     * Clear all expired entries from cache
     * @return Number of entries removed
     */
    size_t cleanupExpired();

    /**
     * Get cache statistics
     * @return Pair of (total_entries, valid_entries)
     */
    std::pair<size_t, size_t> getStats();

    /**
     * Clear entire cache
     */
    void clear();

private:
    std::unordered_map<std::string, CacheEntry> cache_;
    mutable std::mutex mutex_;
    int ttlSeconds_;
    size_t maxSize_;
    size_t putCount_ = 0;

    /**
     * Check if cache entry is expired
     * @param entry The cache entry to check
     * @return true if expired
     */
    bool isExpired(const CacheEntry& entry) const;

    /**
     * Calculate expiration time for new entries
     * @return Expiration time point
     */
    std::chrono::system_clock::time_point getExpirationTime() const;
};

} // namespace common
} // namespace search_engine
