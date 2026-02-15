#pragma once

#include "../../infrastructure.h"
#include <string>
#include <chrono>
#include <vector>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <memory>

namespace search_engine {
namespace storage {

/**
 * @brief Link click analytics (PRIVACY-FIRST)
 * 
 * What gets stored:
 * - City-level geographic data (NO IP addresses)
 * - Generic device/browser info (NO versions or fingerprinting)
 * - Link click counts and timestamps
 * - Referrer (optional, sanitized)
 * 
 * What is NOT stored:
 * - IP addresses (never enters this tier)
 * - Precise user-agent strings
 * - User identifiers (beyond profile/link being clicked)
 * - Any data that could identify individual users
 */
struct LinkClickAnalytics {
    std::string clickId;              // Unique click identifier
    std::string linkId;               // Link being clicked
    std::string profileId;            // Profile owning the link
    std::chrono::system_clock::time_point timestamp;
    
    // Geographic data (city-level only, NO IP)
    std::string country = "Unknown";
    std::string province = "Unknown";
    std::string city = "Unknown";
    
    // Device info (generic, NO versions)
    std::string browser = "Unknown";      // e.g., "Chrome" (not "Chrome 120.0.6099.109")
    std::string os = "Unknown";           // e.g., "Android" (not "Android 14")
    std::string deviceType = "Desktop";   // "Mobile", "Tablet", or "Desktop"
    
    // Optional referrer (sanitized)
    std::string referrer = "Direct";      // e.g., "https://example.com" or "Direct"
};

/**
 * @brief Storage for link click analytics (privacy-first)
 * 
 * Collection: link_click_analytics
 * Indexes: linkId + timestamp, profileId + timestamp for dashboard queries
 * 
 * User-facing analytics:
 * - "45 clicks from Tehran this month"
 * - "78% from mobile devices"
 * - "Top referrer: social media"
 */
class LinkClickAnalyticsStorage {
private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection analyticsCollection_;
    
    // Ensure indexes are created
    void ensureIndexes();
    
    // BSON conversion helpers
    bsoncxx::document::value analyticsToBson(const LinkClickAnalytics& analytics) const;
    LinkClickAnalytics bsonToAnalytics(const bsoncxx::document::view& doc) const;
    
    // Time conversion helpers
    static bsoncxx::types::b_date timePointToDate(const std::chrono::system_clock::time_point& tp);
    static std::chrono::system_clock::time_point dateToTimePoint(const bsoncxx::types::b_date& date);
    
public:
    /**
     * @brief Constructor with MongoDB connection
     * @param connectionString MongoDB URI (default from MONGODB_URI env)
     * @param databaseName Database name (default: search-engine)
     */
    explicit LinkClickAnalyticsStorage(
        const std::string& connectionString = "mongodb://localhost:27017",
        const std::string& databaseName = "search-engine"
    );
    
    ~LinkClickAnalyticsStorage() = default;
    
    // Prevent copying (RAII pattern)
    LinkClickAnalyticsStorage(const LinkClickAnalyticsStorage&) = delete;
    LinkClickAnalyticsStorage& operator=(const LinkClickAnalyticsStorage&) = delete;
    
    // Allow moving
    LinkClickAnalyticsStorage(LinkClickAnalyticsStorage&&) = default;
    LinkClickAnalyticsStorage& operator=(LinkClickAnalyticsStorage&&) = default;
    
    /**
     * @brief Record a link click (privacy-first analytics)
     * @param analytics Click data (no IP addresses!)
     * @return Result with success status and message
     */
    Result<std::string> recordClick(const LinkClickAnalytics& analytics);
    
    /**
     * @brief Get recent clicks for a link
     * @param linkId Link to query
     * @param limit Maximum number of clicks to return
     * @return Result with vector of recent clicks
     */
    Result<std::vector<LinkClickAnalytics>> getRecentClicksByLink(
        const std::string& linkId,
        int limit = 100
    );
    
    /**
     * @brief Get recent clicks for a profile (all links)
     * @param profileId Profile to query
     * @param limit Maximum number of clicks to return
     * @return Result with vector of recent clicks
     */
    Result<std::vector<LinkClickAnalytics>> getRecentClicksByProfile(
        const std::string& profileId,
        int limit = 100
    );
    
    /**
     * @brief Count total clicks for a link
     * @param linkId Link to count
     * @return Result with click count
     */
    Result<int64_t> countClicksByLink(const std::string& linkId);
    
    /**
     * @brief Count total clicks for a profile (all links)
     * @param profileId Profile to count
     * @return Result with click count
     */
    Result<int64_t> countClicksByProfile(const std::string& profileId);
    
    /**
     * @brief Delete old analytics data (for retention policy)
     * @param olderThan Delete clicks older than this timestamp
     * @return Result with number of deleted documents
     */
    Result<int64_t> deleteOldClicks(const std::chrono::system_clock::time_point& olderThan);
};

} // namespace storage
} // namespace search_engine
