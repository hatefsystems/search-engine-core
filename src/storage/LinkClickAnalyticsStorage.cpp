#include "../../include/search_engine/storage/LinkClickAnalytics.h"
#include "../../include/mongodb.h"
#include "../../include/Logger.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <cstdlib>

namespace search_engine {
namespace storage {

using bsoncxx::builder::basic::kvp;

// Helper to convert time_point to BSON date
bsoncxx::types::b_date LinkClickAnalyticsStorage::timePointToDate(
    const std::chrono::system_clock::time_point& tp) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
    return bsoncxx::types::b_date{ms};
}

// Helper to convert BSON date to time_point
std::chrono::system_clock::time_point LinkClickAnalyticsStorage::dateToTimePoint(
    const bsoncxx::types::b_date& date) {
    return std::chrono::system_clock::time_point{std::chrono::milliseconds{date.to_int64()}};
}

LinkClickAnalyticsStorage::LinkClickAnalyticsStorage(
    const std::string& connectionString,
    const std::string& databaseName)
{
    LOG_DEBUG("LinkClickAnalyticsStorage constructor called");
    
    try {
        // Initialize MongoDB instance singleton
        MongoDBInstance::getInstance();
        
        // Get MongoDB connection string from environment or use default
        const char* mongo_uri = std::getenv("MONGODB_URI");
        std::string uri = mongo_uri ? mongo_uri : connectionString;
        
        LOG_DEBUG("Connecting to MongoDB: " + uri);
        mongocxx::uri mongo_uri_obj{uri};
        client_ = std::make_unique<mongocxx::client>(mongo_uri_obj);
        
        // Get database and collection
        database_ = (*client_)[databaseName];
        analyticsCollection_ = database_["link_click_analytics"];
        
        LOG_INFO("LinkClickAnalyticsStorage initialized successfully");
        
        // Ensure indexes exist
        ensureIndexes();
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("Failed to initialize LinkClickAnalyticsStorage: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize LinkClickAnalyticsStorage: " + std::string(e.what()));
    }
}

void LinkClickAnalyticsStorage::ensureIndexes() {
    LOG_DEBUG("Creating indexes for link_click_analytics collection");
    
    try {
        // Index 1: { linkId: 1, timestamp: -1 } - recent clicks for link
        auto linkTimeIndex = bsoncxx::builder::basic::document{};
        linkTimeIndex.append(kvp("linkId", 1));
        linkTimeIndex.append(kvp("timestamp", -1));
        
        mongocxx::options::index linkTimeOptions{};
        linkTimeOptions.name("link_clicks_timeline");
        analyticsCollection_.create_index(linkTimeIndex.view(), linkTimeOptions);
        
        // Index 2: { profileId: 1, timestamp: -1 } - recent clicks for profile
        auto profileTimeIndex = bsoncxx::builder::basic::document{};
        profileTimeIndex.append(kvp("profileId", 1));
        profileTimeIndex.append(kvp("timestamp", -1));
        
        mongocxx::options::index profileTimeOptions{};
        profileTimeOptions.name("profile_clicks_timeline");
        analyticsCollection_.create_index(profileTimeIndex.view(), profileTimeOptions);
        
        // Index 3: { timestamp: -1 } - for retention cleanup
        auto timestampIndex = bsoncxx::builder::basic::document{};
        timestampIndex.append(kvp("timestamp", -1));
        
        mongocxx::options::index timestampOptions{};
        timestampOptions.name("timestamp_cleanup");
        analyticsCollection_.create_index(timestampIndex.view(), timestampOptions);
        
        LOG_INFO("LinkClickAnalyticsStorage indexes created successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_WARNING("Failed to create indexes (may already exist): " + std::string(e.what()));
    }
}

bsoncxx::document::value LinkClickAnalyticsStorage::analyticsToBson(
    const LinkClickAnalytics& analytics) const {
    auto builder = bsoncxx::builder::basic::document{};
    
    builder.append(kvp("clickId", analytics.clickId));
    builder.append(kvp("linkId", analytics.linkId));
    builder.append(kvp("profileId", analytics.profileId));
    builder.append(kvp("timestamp", timePointToDate(analytics.timestamp)));
    
    // Geographic data (city-level, no IP)
    builder.append(kvp("country", analytics.country));
    builder.append(kvp("province", analytics.province));
    builder.append(kvp("city", analytics.city));
    
    // Device info (generic, no versions)
    builder.append(kvp("browser", analytics.browser));
    builder.append(kvp("os", analytics.os));
    builder.append(kvp("deviceType", analytics.deviceType));
    
    // Optional referrer
    builder.append(kvp("referrer", analytics.referrer));
    
    return builder.extract();
}

LinkClickAnalytics LinkClickAnalyticsStorage::bsonToAnalytics(
    const bsoncxx::document::view& doc) const {
    LinkClickAnalytics analytics;
    
    analytics.clickId = std::string(doc["clickId"].get_string().value);
    analytics.linkId = std::string(doc["linkId"].get_string().value);
    analytics.profileId = std::string(doc["profileId"].get_string().value);
    analytics.timestamp = dateToTimePoint(doc["timestamp"].get_date());
    
    // Geographic data
    if (doc["country"]) {
        analytics.country = std::string(doc["country"].get_string().value);
    }
    if (doc["province"]) {
        analytics.province = std::string(doc["province"].get_string().value);
    }
    if (doc["city"]) {
        analytics.city = std::string(doc["city"].get_string().value);
    }
    
    // Device info
    if (doc["browser"]) {
        analytics.browser = std::string(doc["browser"].get_string().value);
    }
    if (doc["os"]) {
        analytics.os = std::string(doc["os"].get_string().value);
    }
    if (doc["deviceType"]) {
        analytics.deviceType = std::string(doc["deviceType"].get_string().value);
    }
    
    // Referrer
    if (doc["referrer"]) {
        analytics.referrer = std::string(doc["referrer"].get_string().value);
    }
    
    return analytics;
}

Result<std::string> LinkClickAnalyticsStorage::recordClick(
    const LinkClickAnalytics& analytics) {
    try {
        auto doc = analyticsToBson(analytics);
        
        auto result = analyticsCollection_.insert_one(doc.view());
        
        if (result) {
            LOG_DEBUG("Link click recorded: " + analytics.clickId);
            return Result<std::string>::Success(
                analytics.clickId, "Click recorded successfully");
        } else {
            LOG_ERROR("Failed to record link click: insert_one returned no result");
            return Result<std::string>::Failure("Failed to record click");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error recording click: " + std::string(e.what()));
        return Result<std::string>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<std::vector<LinkClickAnalytics>> LinkClickAnalyticsStorage::getRecentClicksByLink(
    const std::string& linkId,
    int limit) {
    try {
        auto filter = bsoncxx::builder::basic::document{};
        filter.append(kvp("linkId", linkId));
        
        mongocxx::options::find opts{};
        auto sortDoc = bsoncxx::builder::basic::document{};
        sortDoc.append(kvp("timestamp", -1));
        opts.sort(sortDoc.view());
        opts.limit(limit);
        
        auto cursor = analyticsCollection_.find(filter.view(), opts);
        
        std::vector<LinkClickAnalytics> clicks;
        for (auto&& doc : cursor) {
            clicks.push_back(bsonToAnalytics(doc));
        }
        
        return Result<std::vector<LinkClickAnalytics>>::Success(
            clicks, "Clicks retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error retrieving clicks: " + std::string(e.what()));
        return Result<std::vector<LinkClickAnalytics>>::Failure(
            "Database error: " + std::string(e.what()));
    }
}

Result<std::vector<LinkClickAnalytics>> LinkClickAnalyticsStorage::getRecentClicksByProfile(
    const std::string& profileId,
    int limit) {
    try {
        auto filter = bsoncxx::builder::basic::document{};
        filter.append(kvp("profileId", profileId));
        
        mongocxx::options::find opts{};
        auto sortDoc = bsoncxx::builder::basic::document{};
        sortDoc.append(kvp("timestamp", -1));
        opts.sort(sortDoc.view());
        opts.limit(limit);
        
        auto cursor = analyticsCollection_.find(filter.view(), opts);
        
        std::vector<LinkClickAnalytics> clicks;
        for (auto&& doc : cursor) {
            clicks.push_back(bsonToAnalytics(doc));
        }
        
        return Result<std::vector<LinkClickAnalytics>>::Success(
            clicks, "Clicks retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error retrieving clicks: " + std::string(e.what()));
        return Result<std::vector<LinkClickAnalytics>>::Failure(
            "Database error: " + std::string(e.what()));
    }
}

Result<int64_t> LinkClickAnalyticsStorage::countClicksByLink(const std::string& linkId) {
    try {
        auto filter = bsoncxx::builder::basic::document{};
        filter.append(kvp("linkId", linkId));
        
        int64_t count = analyticsCollection_.count_documents(filter.view());
        
        return Result<int64_t>::Success(count, "Count retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error counting clicks: " + std::string(e.what()));
        return Result<int64_t>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<int64_t> LinkClickAnalyticsStorage::countClicksByProfile(const std::string& profileId) {
    try {
        auto filter = bsoncxx::builder::basic::document{};
        filter.append(kvp("profileId", profileId));
        
        int64_t count = analyticsCollection_.count_documents(filter.view());
        
        return Result<int64_t>::Success(count, "Count retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error counting clicks: " + std::string(e.what()));
        return Result<int64_t>::Failure("Database error: " + std::string(e.what()));
    }
}

Result<int64_t> LinkClickAnalyticsStorage::deleteOldClicks(
    const std::chrono::system_clock::time_point& olderThan) {
    try {
        auto filter = bsoncxx::builder::basic::document{};
        auto ltDoc = bsoncxx::builder::basic::document{};
        ltDoc.append(kvp("$lt", timePointToDate(olderThan)));
        filter.append(kvp("timestamp", ltDoc.extract()));
        
        auto result = analyticsCollection_.delete_many(filter.view());
        
        if (result) {
            int64_t deletedCount = result->deleted_count();
            LOG_INFO("Deleted " + std::to_string(deletedCount) + " old click analytics");
            return Result<int64_t>::Success(
                deletedCount, "Old clicks deleted successfully");
        } else {
            return Result<int64_t>::Success(0, "No clicks to delete");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error deleting old clicks: " + std::string(e.what()));
        return Result<int64_t>::Failure("Database error: " + std::string(e.what()));
    }
}

} // namespace storage
} // namespace search_engine
