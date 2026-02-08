#include "../../include/search_engine/storage/ProfileViewAnalytics.h"
#include "../../include/Logger.h"
#include "../../include/mongodb.h"
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

namespace search_engine {
namespace storage {

bsoncxx::types::b_date ProfileViewAnalyticsStorage::timePointToDate(
    const std::chrono::system_clock::time_point& tp) {
    auto duration = tp.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return bsoncxx::types::b_date{millis};
}

std::chrono::system_clock::time_point ProfileViewAnalyticsStorage::dateToTimePoint(
    const bsoncxx::types::b_date& date) {
    return std::chrono::system_clock::time_point{date.value};
}

ProfileViewAnalyticsStorage::ProfileViewAnalyticsStorage(
    const std::string& connectionString,
    const std::string& databaseName) {
    
    LOG_DEBUG("ProfileViewAnalyticsStorage constructor called");
    
    try {
        // Use MONGODB_URI environment variable if available
        std::string actualConnectionString = connectionString;
        const char* envUri = std::getenv("MONGODB_URI");
        if (envUri) {
            actualConnectionString = std::string(envUri);
            LOG_DEBUG("Using MONGODB_URI from environment: " + actualConnectionString);
        }
        
        LOG_INFO("Initializing analytics MongoDB connection to: " + actualConnectionString);
        
        // Use existing MongoDB instance singleton
        mongocxx::instance& instance = MongoDBInstance::getInstance();
        (void)instance;
        
        // Create client and connect to database
        mongocxx::uri uri{actualConnectionString};
        client_ = std::make_unique<mongocxx::client>(uri);
        database_ = (*client_)[databaseName];
        analyticsCollection_ = database_["profile_view_analytics"];
        
        LOG_INFO("Connected to analytics database: " + databaseName);
        
        // Ensure indexes are created
        ensureIndexes();
        LOG_DEBUG("Analytics indexes ensured");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("Failed to initialize ProfileViewAnalyticsStorage: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize ProfileViewAnalyticsStorage: " + std::string(e.what()));
    }
}

void ProfileViewAnalyticsStorage::ensureIndexes() {
    try {
        using bsoncxx::builder::basic::kvp;
        using bsoncxx::builder::basic::make_document;
        
        // 1. profile_views_timeline: Index on profileId + timestamp for dashboard queries
        {
            mongocxx::options::index opts{};
            opts.name("profile_views_timeline");
            auto profileTimestampIndex = make_document(
                kvp("profileId", 1),
                kvp("timestamp", -1)  // Descending for recent views
            );
            analyticsCollection_.create_index(profileTimestampIndex.view(), opts);
        }
        
        // 2. location_analytics: Index on city + timestamp for geographic analytics
        {
            mongocxx::options::index opts{};
            opts.name("location_analytics");
            auto cityTimestampIndex = make_document(
                kvp("city", 1),
                kvp("timestamp", -1)
            );
            analyticsCollection_.create_index(cityTimestampIndex.view(), opts);
        }
        
        // 3. device_analytics: Index on deviceType + timestamp for device analytics
        {
            mongocxx::options::index opts{};
            opts.name("device_analytics");
            auto deviceTimestampIndex = make_document(
                kvp("deviceType", 1),
                kvp("timestamp", -1)
            );
            analyticsCollection_.create_index(deviceTimestampIndex.view(), opts);
        }
        
        // Index on timestamp only for cleanup/aggregation queries (unnamed helper index)
        {
            auto timestampIndex = make_document(kvp("timestamp", -1));
            analyticsCollection_.create_index(timestampIndex.view());
        }
        
        LOG_INFO("ProfileViewAnalyticsStorage indexes created successfully with named indexes");
        
    } catch (const mongocxx::exception& e) {
        LOG_WARNING("Failed to create analytics indexes (may already exist): " + std::string(e.what()));
    }
}

bsoncxx::document::value ProfileViewAnalyticsStorage::analyticsToBson(
    const ProfileViewAnalytics& analytics) const {
    
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    
    auto builder = bsoncxx::builder::basic::document{};
    
    builder.append(kvp("viewId", analytics.viewId));
    builder.append(kvp("profileId", analytics.profileId));
    builder.append(kvp("timestamp", timePointToDate(analytics.timestamp)));
    
    // Geographic data (city-level only, NO IP)
    builder.append(kvp("country", analytics.country));
    builder.append(kvp("province", analytics.province));
    builder.append(kvp("city", analytics.city));
    
    // Device info (generic, NO versions)
    builder.append(kvp("browser", analytics.browser));
    builder.append(kvp("os", analytics.os));
    builder.append(kvp("deviceType", analytics.deviceType));
    
    return builder.extract();
}

ProfileViewAnalytics ProfileViewAnalyticsStorage::bsonToAnalytics(
    const bsoncxx::document::view& doc) const {
    
    ProfileViewAnalytics analytics;
    
    analytics.viewId = std::string(doc["viewId"].get_string().value);
    analytics.profileId = std::string(doc["profileId"].get_string().value);
    analytics.timestamp = dateToTimePoint(doc["timestamp"].get_date());
    
    // Geographic data
    analytics.country = std::string(doc["country"].get_string().value);
    analytics.province = std::string(doc["province"].get_string().value);
    analytics.city = std::string(doc["city"].get_string().value);
    
    // Device info
    analytics.browser = std::string(doc["browser"].get_string().value);
    analytics.os = std::string(doc["os"].get_string().value);
    analytics.deviceType = std::string(doc["deviceType"].get_string().value);
    
    return analytics;
}

Result<std::string> ProfileViewAnalyticsStorage::recordView(
    const ProfileViewAnalytics& analytics) {
    
    try {
        auto doc = analyticsToBson(analytics);
        auto result = analyticsCollection_.insert_one(doc.view());
        
        if (result) {
            LOG_INFO("Recorded profile view: viewId=" + analytics.viewId + 
                    ", profileId=" + analytics.profileId);
            return Result<std::string>::Success(analytics.viewId, "View recorded successfully");
        } else {
            LOG_ERROR("Failed to record profile view");
            return Result<std::string>::Failure("Failed to record view");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error recording view: " + std::string(e.what()));
        return Result<std::string>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::vector<ProfileViewAnalytics>> ProfileViewAnalyticsStorage::getRecentViewsByProfile(
    const std::string& profileId,
    int limit) {
    
    try {
        auto filter = document{} << "profileId" << profileId << finalize;
        
        mongocxx::options::find options{};
        options.sort(document{} << "timestamp" << -1 << finalize);
        options.limit(limit);
        
        auto cursor = analyticsCollection_.find(filter.view(), options);
        
        std::vector<ProfileViewAnalytics> views;
        for (const auto& doc : cursor) {
            views.push_back(bsonToAnalytics(doc));
        }
        
        LOG_INFO("Retrieved " + std::to_string(views.size()) + " recent views for profile: " + profileId);
        return Result<std::vector<ProfileViewAnalytics>>::Success(views, "Views retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error retrieving views: " + std::string(e.what()));
        return Result<std::vector<ProfileViewAnalytics>>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<int64_t> ProfileViewAnalyticsStorage::countViewsByProfile(const std::string& profileId) {
    try {
        auto filter = document{} << "profileId" << profileId << finalize;
        int64_t count = analyticsCollection_.count_documents(filter.view());
        
        LOG_INFO("Profile " + profileId + " has " + std::to_string(count) + " total views");
        return Result<int64_t>::Success(count, "Count retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error counting views: " + std::string(e.what()));
        return Result<int64_t>::Failure("Database error: " + std::string(e.what()));
    }
}

} // namespace storage
} // namespace search_engine
