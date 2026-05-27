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
 * @brief Tier 1 Analytics - Profile view tracking (PRIVACY-FIRST)
 * 
 * What gets stored:
 * - City-level geographic data (NO IP addresses)
 * - Generic device/browser info (NO versions or fingerprinting)
 * - Profile view counts and timestamps
 * 
 * What is NOT stored:
 * - IP addresses (never enters this tier)
 * - Precise user-agent strings
 * - User identifiers (beyond profile being viewed)
 * - Any data that could identify individual users
 */
struct ProfileViewAnalytics {
    std::string viewId;              // Unique view identifier (links to Tier 2 if needed)
    std::string profileId;           // Profile being viewed
    std::chrono::system_clock::time_point timestamp;
    
    // Geographic data (city-level only, NO IP)
    std::string country = "Unknown";
    std::string province = "Unknown";
    std::string city = "Unknown";
    
    // Device info (generic, NO versions)
    std::string browser = "Unknown";      // e.g., "Chrome" (not "Chrome 120.0.6099.109")
    std::string os = "Unknown";           // e.g., "Android" (not "Android 14")
    std::string deviceType = "Desktop";   // "Mobile", "Tablet", or "Desktop"
};

/**
 * @brief Storage for Tier 1 analytics (privacy-first profile views)
 * 
 * Collection: profile_view_analytics
 * Indexes: profileId + timestamp for dashboard queries
 * 
 * User-facing analytics:
 * - "45 views from Tehran this month"
 * - "78% from mobile devices"
 * - "Peak viewing hours: 2-4 PM"
 */
class ProfileViewAnalyticsStorage {
private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection analyticsCollection_;
    
    // Ensure indexes are created
    void ensureIndexes();
    
    // BSON conversion helpers
    bsoncxx::document::value analyticsToBson(const ProfileViewAnalytics& analytics) const;
    ProfileViewAnalytics bsonToAnalytics(const bsoncxx::document::view& doc) const;
    
    // Time conversion helpers
    static bsoncxx::types::b_date timePointToDate(const std::chrono::system_clock::time_point& tp);
    static std::chrono::system_clock::time_point dateToTimePoint(const bsoncxx::types::b_date& date);
    
public:
    /**
     * @brief Constructor with MongoDB connection
     * @param connectionString MongoDB URI (default from MONGODB_URI env)
     * @param databaseName Database name (default: search-engine)
     */
    explicit ProfileViewAnalyticsStorage(
        const std::string& connectionString = "mongodb://localhost:27017",
        const std::string& databaseName = "search-engine"
    );
    
    ~ProfileViewAnalyticsStorage() = default;
    
    // Prevent copying (RAII pattern)
    ProfileViewAnalyticsStorage(const ProfileViewAnalyticsStorage&) = delete;
    ProfileViewAnalyticsStorage& operator=(const ProfileViewAnalyticsStorage&) = delete;
    
    // Allow moving
    ProfileViewAnalyticsStorage(ProfileViewAnalyticsStorage&&) = default;
    ProfileViewAnalyticsStorage& operator=(ProfileViewAnalyticsStorage&&) = default;
    
    /**
     * @brief Record a profile view (Tier 1 analytics)
     * @param analytics View data (no IP addresses!)
     * @return Result with success status and message
     */
    Result<std::string> recordView(const ProfileViewAnalytics& analytics);
    
    /**
     * @brief Get recent views for a profile (for privacy dashboard)
     * @param profileId Profile to query
     * @param limit Maximum number of views to return
     * @return Result with vector of recent views
     */
    Result<std::vector<ProfileViewAnalytics>> getRecentViewsByProfile(
        const std::string& profileId,
        int limit = 30
    );
    
    /**
     * @brief Count total views for a profile
     * @param profileId Profile to count
     * @return Result with view count
     */
    Result<int64_t> countViewsByProfile(const std::string& profileId);
};

} // namespace storage
} // namespace search_engine
