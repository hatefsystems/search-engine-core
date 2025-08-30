#include "../../include/search_engine/storage/MongoDBStorage.h"
#include "../../include/Logger.h"
#include "../../include/mongodb.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/oid.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <sstream>
#include <iomanip>
#include <mutex>

using namespace bsoncxx::builder::stream;
using namespace search_engine::storage;

namespace {
    
    // Helper function to convert time_point to BSON date
    bsoncxx::types::b_date timePointToBsonDate(const std::chrono::system_clock::time_point& tp) {
        auto duration = tp.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        return bsoncxx::types::b_date{std::chrono::milliseconds{millis}};
    }
    
    // Helper function to convert BSON date to time_point
    std::chrono::system_clock::time_point bsonDateToTimePoint(const bsoncxx::types::b_date& date) {
        return std::chrono::system_clock::time_point{date.value};
    }
}

MongoDBStorage::MongoDBStorage(const std::string& connectionString, const std::string& databaseName) {
    LOG_DEBUG("MongoDBStorage constructor called with database: " + databaseName);
    try {
        LOG_INFO("Initializing MongoDB connection to: " + connectionString);
        
        // Ensure instance is initialized
        MongoDBInstance::getInstance();
        LOG_DEBUG("MongoDB instance initialized");
        
        // Create client and connect to database
        mongocxx::uri uri{connectionString};
        client_ = std::make_unique<mongocxx::client>(uri);
        database_ = (*client_)[databaseName];
        siteProfilesCollection_ = database_["site_profiles"];
        LOG_INFO("Connected to MongoDB database: " + databaseName);
        
        // Ensure indexes are created
        ensureIndexes();
        LOG_DEBUG("MongoDB indexes ensured");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("Failed to initialize MongoDB connection: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize MongoDB connection: " + std::string(e.what()));
    }
}

std::string MongoDBStorage::crawlStatusToString(CrawlStatus status) {
    switch (status) {
        case CrawlStatus::SUCCESS: return "SUCCESS";
        case CrawlStatus::FAILED: return "FAILED";
        case CrawlStatus::PENDING: return "PENDING";
        case CrawlStatus::TIMEOUT: return "TIMEOUT";
        case CrawlStatus::ROBOT_BLOCKED: return "ROBOT_BLOCKED";
        case CrawlStatus::REDIRECT_LOOP: return "REDIRECT_LOOP";
        case CrawlStatus::CONTENT_TOO_LARGE: return "CONTENT_TOO_LARGE";
        case CrawlStatus::INVALID_CONTENT_TYPE: return "INVALID_CONTENT_TYPE";
        default: return "UNKNOWN";
    }
}

CrawlStatus MongoDBStorage::stringToCrawlStatus(const std::string& status) {
    if (status == "SUCCESS") return CrawlStatus::SUCCESS;
    if (status == "FAILED") return CrawlStatus::FAILED;
    if (status == "PENDING") return CrawlStatus::PENDING;
    if (status == "TIMEOUT") return CrawlStatus::TIMEOUT;
    if (status == "ROBOT_BLOCKED") return CrawlStatus::ROBOT_BLOCKED;
    if (status == "REDIRECT_LOOP") return CrawlStatus::REDIRECT_LOOP;
    if (status == "CONTENT_TOO_LARGE") return CrawlStatus::CONTENT_TOO_LARGE;
    if (status == "INVALID_CONTENT_TYPE") return CrawlStatus::INVALID_CONTENT_TYPE;
    return CrawlStatus::FAILED;
}

bsoncxx::document::value MongoDBStorage::crawlMetadataToBson(const CrawlMetadata& metadata) const {
    auto builder = document{};
    
    builder << "lastCrawlTime" << timePointToBsonDate(metadata.lastCrawlTime)
            << "firstCrawlTime" << timePointToBsonDate(metadata.firstCrawlTime)
            << "lastCrawlStatus" << crawlStatusToString(metadata.lastCrawlStatus)
            << "crawlCount" << metadata.crawlCount
            << "crawlIntervalHours" << metadata.crawlIntervalHours
            << "userAgent" << metadata.userAgent
            << "httpStatusCode" << metadata.httpStatusCode
            << "contentSize" << static_cast<int64_t>(metadata.contentSize)
            << "contentType" << metadata.contentType
            << "crawlDurationMs" << metadata.crawlDurationMs;
    
    if (metadata.lastErrorMessage) {
        builder << "lastErrorMessage" << *metadata.lastErrorMessage;
    }
    
    return builder << finalize;
}

CrawlMetadata MongoDBStorage::bsonToCrawlMetadata(const bsoncxx::document::view& doc) const {
    CrawlMetadata metadata;
    
    metadata.lastCrawlTime = bsonDateToTimePoint(doc["lastCrawlTime"].get_date());
    metadata.firstCrawlTime = bsonDateToTimePoint(doc["firstCrawlTime"].get_date());
    metadata.lastCrawlStatus = stringToCrawlStatus(std::string(doc["lastCrawlStatus"].get_string().value));
    metadata.crawlCount = doc["crawlCount"].get_int32().value;
    metadata.crawlIntervalHours = doc["crawlIntervalHours"].get_double().value;
    metadata.userAgent = std::string(doc["userAgent"].get_string().value);
    metadata.httpStatusCode = doc["httpStatusCode"].get_int32().value;
    metadata.contentSize = static_cast<size_t>(doc["contentSize"].get_int64().value);
    metadata.contentType = std::string(doc["contentType"].get_string().value);
    metadata.crawlDurationMs = doc["crawlDurationMs"].get_double().value;
    
    if (doc["lastErrorMessage"]) {
        metadata.lastErrorMessage = std::string(doc["lastErrorMessage"].get_string().value);
    }
    
    return metadata;
}

bsoncxx::document::value MongoDBStorage::siteProfileToBson(const SiteProfile& profile) const {
    auto builder = document{};
    
    if (profile.id) {
        builder << "_id" << bsoncxx::oid{*profile.id};
    }
    
    builder << "domain" << profile.domain
            << "url" << profile.url
            << "title" << profile.title
            << "isIndexed" << profile.isIndexed
            << "lastModified" << timePointToBsonDate(profile.lastModified)
            << "indexedAt" << timePointToBsonDate(profile.indexedAt);
    
    // Optional fields
    if (profile.description) {
        builder << "description" << *profile.description;
    }
    if (profile.textContent) {
        builder << "textContent" << *profile.textContent;
    }
    if (profile.language) {
        builder << "language" << *profile.language;
    }
    if (profile.category) {
        builder << "category" << *profile.category;
    }
    if (profile.pageRank) {
        builder << "pageRank" << *profile.pageRank;
    }
    if (profile.contentQuality) {
        builder << "contentQuality" << *profile.contentQuality;
    }
    if (profile.wordCount) {
        builder << "wordCount" << *profile.wordCount;
    }
    if (profile.isMobile) {
        builder << "isMobile" << *profile.isMobile;
    }
    if (profile.hasSSL) {
        builder << "hasSSL" << *profile.hasSSL;
    }
    if (profile.inboundLinkCount) {
        builder << "inboundLinkCount" << *profile.inboundLinkCount;
    }
    if (profile.author) {
        builder << "author" << *profile.author;
    }
    if (profile.publisher) {
        builder << "publisher" << *profile.publisher;
    }
    if (profile.publishDate) {
        builder << "publishDate" << timePointToBsonDate(*profile.publishDate);
    }
    
    // Arrays
    auto keywordsArray = bsoncxx::builder::stream::array{};
    for (const auto& keyword : profile.keywords) {
        keywordsArray << keyword;
    }
    builder << "keywords" << keywordsArray;
    
    auto outboundLinksArray = bsoncxx::builder::stream::array{};
    for (const auto& link : profile.outboundLinks) {
        outboundLinksArray << link;
    }
    builder << "outboundLinks" << outboundLinksArray;
    
    // Crawl metadata
    builder << "crawlMetadata" << crawlMetadataToBson(profile.crawlMetadata);
    
    return builder << finalize;
}

SiteProfile MongoDBStorage::bsonToSiteProfile(const bsoncxx::document::view& doc) const {
    SiteProfile profile;
    
    if (doc["_id"]) {
        profile.id = std::string(doc["_id"].get_oid().value.to_string());
    }
    
    profile.domain = std::string(doc["domain"].get_string().value);
    profile.url = std::string(doc["url"].get_string().value);
    profile.title = std::string(doc["title"].get_string().value);
    profile.isIndexed = doc["isIndexed"].get_bool().value;
    profile.lastModified = bsonDateToTimePoint(doc["lastModified"].get_date());
    profile.indexedAt = bsonDateToTimePoint(doc["indexedAt"].get_date());
    
    // Optional fields
    if (doc["description"]) {
        profile.description = std::string(doc["description"].get_string().value);
    }
    if (doc["textContent"]) {
        profile.textContent = std::string(doc["textContent"].get_string().value);
    }
    if (doc["language"]) {
        profile.language = std::string(doc["language"].get_string().value);
    }
    if (doc["category"]) {
        profile.category = std::string(doc["category"].get_string().value);
    }
    if (doc["pageRank"]) {
        profile.pageRank = doc["pageRank"].get_int32().value;
    }
    if (doc["contentQuality"]) {
        profile.contentQuality = doc["contentQuality"].get_double().value;
    }
    if (doc["wordCount"]) {
        profile.wordCount = doc["wordCount"].get_int32().value;
    }
    if (doc["isMobile"]) {
        profile.isMobile = doc["isMobile"].get_bool().value;
    }
    if (doc["hasSSL"]) {
        profile.hasSSL = doc["hasSSL"].get_bool().value;
    }
    if (doc["inboundLinkCount"]) {
        profile.inboundLinkCount = doc["inboundLinkCount"].get_int32().value;
    }
    if (doc["author"]) {
        profile.author = std::string(doc["author"].get_string().value);
    }
    if (doc["publisher"]) {
        profile.publisher = std::string(doc["publisher"].get_string().value);
    }
    if (doc["publishDate"]) {
        profile.publishDate = bsonDateToTimePoint(doc["publishDate"].get_date());
    }
    
    // Arrays
    if (doc["keywords"]) {
        for (const auto& keyword : doc["keywords"].get_array().value) {
            profile.keywords.push_back(std::string(keyword.get_string().value));
        }
    }
    
    if (doc["outboundLinks"]) {
        for (const auto& link : doc["outboundLinks"].get_array().value) {
            profile.outboundLinks.push_back(std::string(link.get_string().value));
        }
    }
    
    // Crawl metadata
    if (doc["crawlMetadata"]) {
        profile.crawlMetadata = bsonToCrawlMetadata(doc["crawlMetadata"].get_document().view());
    }
    
    return profile;
}

Result<std::string> MongoDBStorage::storeSiteProfile(const SiteProfile& profile) {
    LOG_DEBUG("MongoDBStorage::storeSiteProfile called for URL: " + profile.url);
    try {
        auto doc = siteProfileToBson(profile);
        LOG_TRACE("Site profile converted to BSON document");
        
        auto result = siteProfilesCollection_.insert_one(doc.view());
        
        if (result) {
            std::string id = result->inserted_id().get_oid().value.to_string();
            LOG_INFO("Site profile stored successfully with ID: " + id + " for URL: " + profile.url);
            return Result<std::string>::Success(
                id,
                "Site profile stored successfully"
            );
        } else {
            LOG_ERROR("Failed to insert site profile for URL: " + profile.url);
            return Result<std::string>::Failure("Failed to insert site profile");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error storing site profile for URL: " + profile.url + " - " + std::string(e.what()));
        return Result<std::string>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<SiteProfile> MongoDBStorage::getSiteProfile(const std::string& url) {
    LOG_DEBUG("MongoDBStorage::getSiteProfile called for URL: " + url);
    try {
        auto filter = document{} << "url" << url << finalize;
        LOG_TRACE("MongoDB query filter created for URL: " + url);
        
        auto result = siteProfilesCollection_.find_one(filter.view());
        
        if (result) {
            LOG_INFO("Site profile found and retrieved for URL: " + url);
            return Result<SiteProfile>::Success(
                bsonToSiteProfile(result->view()),
                "Site profile retrieved successfully"
            );
        } else {
            LOG_WARNING("Site profile not found for URL: " + url);
            return Result<SiteProfile>::Failure("Site profile not found for URL: " + url);
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error retrieving site profile for URL: " + url + " - " + std::string(e.what()));
        return Result<SiteProfile>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<SiteProfile> MongoDBStorage::getSiteProfileById(const std::string& id) {
    try {
        auto filter = document{} << "_id" << bsoncxx::oid{id} << finalize;
        auto result = siteProfilesCollection_.find_one(filter.view());
        
        if (result) {
            return Result<SiteProfile>::Success(
                bsonToSiteProfile(result->view()),
                "Site profile retrieved successfully"
            );
        } else {
            return Result<SiteProfile>::Failure("Site profile not found for ID: " + id);
        }
    } catch (const mongocxx::exception& e) {
        return Result<SiteProfile>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<bool> MongoDBStorage::updateSiteProfile(const SiteProfile& profile) {
    LOG_DEBUG("MongoDBStorage::updateSiteProfile called for URL: " + profile.url);
    try {
        if (!profile.id) {
            LOG_ERROR("Cannot update site profile without ID for URL: " + profile.url);
            return Result<bool>::Failure("Cannot update site profile without ID");
        }
        
        LOG_TRACE("Updating site profile with ID: " + *profile.id);
        auto filter = document{} << "_id" << bsoncxx::oid{*profile.id} << finalize;
        auto update = document{} << "$set" << siteProfileToBson(profile) << finalize;
        
        auto result = siteProfilesCollection_.update_one(filter.view(), update.view());
        
        if (result && result->modified_count() > 0) {
            LOG_INFO("Site profile updated successfully for URL: " + profile.url + " (ID: " + *profile.id + ")");
            return Result<bool>::Success(true, "Site profile updated successfully");
        } else {
            LOG_WARNING("Site profile not found or no changes made for URL: " + profile.url);
            return Result<bool>::Failure("Site profile not found or no changes made");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error updating site profile for URL: " + profile.url + " - " + std::string(e.what()));
        return Result<bool>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<bool> MongoDBStorage::deleteSiteProfile(const std::string& url) {
    LOG_DEBUG("MongoDBStorage::deleteSiteProfile called for URL: " + url);
    try {
        auto filter = document{} << "url" << url << finalize;
        LOG_TRACE("Delete filter created for URL: " + url);
        
        auto result = siteProfilesCollection_.delete_one(filter.view());
        
        if (result && result->deleted_count() > 0) {
            LOG_INFO("Site profile deleted successfully for URL: " + url);
            return Result<bool>::Success(true, "Site profile deleted successfully");
        } else {
            LOG_WARNING("Site profile not found for deletion, URL: " + url);
            return Result<bool>::Failure("Site profile not found for URL: " + url);
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error deleting site profile for URL: " + url + " - " + std::string(e.what()));
        return Result<bool>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<std::vector<SiteProfile>> MongoDBStorage::getSiteProfilesByDomain(const std::string& domain) {
    LOG_DEBUG("MongoDBStorage::getSiteProfilesByDomain called for domain: " + domain);
    try {
        auto filter = document{} << "domain" << domain << finalize;
        auto cursor = siteProfilesCollection_.find(filter.view());
        
        std::vector<SiteProfile> profiles;
        for (const auto& doc : cursor) {
            profiles.push_back(bsonToSiteProfile(doc));
        }
        
        LOG_INFO("Retrieved " + std::to_string(profiles.size()) + " site profiles for domain: " + domain);
        return Result<std::vector<SiteProfile>>::Success(
            std::move(profiles),
            "Site profiles retrieved successfully for domain: " + domain
        );
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error retrieving site profiles for domain: " + domain + " - " + std::string(e.what()));
        return Result<std::vector<SiteProfile>>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<std::vector<SiteProfile>> MongoDBStorage::getSiteProfilesByCrawlStatus(CrawlStatus status) {
    try {
        auto filter = document{} << "crawlMetadata.lastCrawlStatus" << crawlStatusToString(status) << finalize;
        auto cursor = siteProfilesCollection_.find(filter.view());
        
        std::vector<SiteProfile> profiles;
        for (const auto& doc : cursor) {
            profiles.push_back(bsonToSiteProfile(doc));
        }
        
        return Result<std::vector<SiteProfile>>::Success(
            std::move(profiles),
            "Site profiles retrieved successfully for status"
        );
    } catch (const mongocxx::exception& e) {
        return Result<std::vector<SiteProfile>>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<int64_t> MongoDBStorage::getTotalSiteCount() {
    try {
        auto count = siteProfilesCollection_.count_documents({});
        return Result<int64_t>::Success(count, "Site count retrieved successfully");
    } catch (const mongocxx::exception& e) {
        return Result<int64_t>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<int64_t> MongoDBStorage::getSiteCountByStatus(CrawlStatus status) {
    try {
        auto filter = document{} << "crawlMetadata.lastCrawlStatus" << crawlStatusToString(status) << finalize;
        auto count = siteProfilesCollection_.count_documents(filter.view());
        return Result<int64_t>::Success(count, "Site count by status retrieved successfully");
    } catch (const mongocxx::exception& e) {
        return Result<int64_t>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<bool> MongoDBStorage::testConnection() {
    LOG_DEBUG("MongoDBStorage::testConnection called");
    try {
        // Simple ping to test connection
        auto result = database_.run_command(document{} << "ping" << 1 << finalize);
        LOG_INFO("MongoDB connection test successful");
        return Result<bool>::Success(true, "MongoDB connection is healthy");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB connection test failed: " + std::string(e.what()));
        return Result<bool>::Failure("MongoDB connection test failed: " + std::string(e.what()));
    }
}

Result<bool> MongoDBStorage::ensureIndexes() {
    try {
        // Create indexes for efficient querying
        auto urlIndex = document{} << "url" << 1 << finalize;
        auto domainIndex = document{} << "domain" << 1 << finalize;
        auto statusIndex = document{} << "crawlMetadata.lastCrawlStatus" << 1 << finalize;
        auto lastModifiedIndex = document{} << "lastModified" << -1 << finalize;
        
        siteProfilesCollection_.create_index(urlIndex.view());
        siteProfilesCollection_.create_index(domainIndex.view());
        siteProfilesCollection_.create_index(statusIndex.view());
        siteProfilesCollection_.create_index(lastModifiedIndex.view());

        // Frontier tasks indexes
        auto frontier = database_["frontier_tasks"];
        // Unique per-session normalized_url
        {
            bsoncxx::builder::stream::document keys;
            keys << "sessionId" << 1 << "normalizedUrl" << 1;
            mongocxx::options::index idx_opts{};
            idx_opts.unique(true);
            frontier.create_index(keys.view(), idx_opts);
        }
        // Status + readyAt + priority for fast claims
        {
            bsoncxx::builder::stream::document keys;
            keys << "status" << 1 << "readyAt" << 1 << "priority" << -1;
            frontier.create_index(keys.view());
        }
        // Domain filter
        {
            bsoncxx::builder::stream::document keys;
            keys << "domain" << 1;
            frontier.create_index(keys.view());
        }
        
        return Result<bool>::Success(true, "Indexes created successfully");
    } catch (const mongocxx::exception& e) {
        return Result<bool>::Failure("Failed to create indexes: " + std::string(e.what()));
    }
} 

Result<bool> MongoDBStorage::frontierUpsertTask(const std::string& sessionId,
                                    const std::string& url,
                                    const std::string& normalizedUrl,
                                    const std::string& domain,
                                    int depth,
                                    int priority,
                                    const std::string& status,
                                    const std::chrono::system_clock::time_point& readyAt,
                                    int retryCount) {
    try {
        auto frontier = database_["frontier_tasks"];
        auto filter = document{} << "sessionId" << sessionId << "normalizedUrl" << normalizedUrl << finalize;
        auto update = document{}
            << "$set" << open_document
                << "sessionId" << sessionId
                << "url" << url
                << "normalizedUrl" << normalizedUrl
                << "domain" << domain
                << "depth" << depth
                << "priority" << priority
                << "status" << status
                << "readyAt" << timePointToBsonDate(readyAt)
                << "retryCount" << retryCount
                << "updatedAt" << timePointToBsonDate(std::chrono::system_clock::now())
            << close_document
            << "$setOnInsert" << open_document
                << "createdAt" << timePointToBsonDate(std::chrono::system_clock::now())
            << close_document
        << finalize;
        mongocxx::options::update opts;
        opts.upsert(true);
        frontier.update_one(filter.view(), update.view(), opts);
        return Result<bool>::Success(true, "Frontier task upserted");
    } catch (const mongocxx::exception& e) {
        return Result<bool>::Failure("MongoDB frontierUpsertTask error: " + std::string(e.what()));
    }
}

Result<bool> MongoDBStorage::frontierMarkCompleted(const std::string& sessionId,
                                       const std::string& normalizedUrl) {
    try {
        auto frontier = database_["frontier_tasks"];
        auto filter = document{} << "sessionId" << sessionId << "normalizedUrl" << normalizedUrl << finalize;
        auto update = document{} << "$set" << open_document
            << "status" << "completed"
            << "updatedAt" << timePointToBsonDate(std::chrono::system_clock::now())
        << close_document << finalize;
        frontier.update_one(filter.view(), update.view());
        return Result<bool>::Success(true, "Frontier task completed");
    } catch (const mongocxx::exception& e) {
        return Result<bool>::Failure("MongoDB frontierMarkCompleted error: " + std::string(e.what()));
    }
}

Result<bool> MongoDBStorage::frontierUpdateRetry(const std::string& sessionId,
                                     const std::string& normalizedUrl,
                                     int retryCount,
                                     const std::chrono::system_clock::time_point& nextReadyAt) {
    try {
        auto frontier = database_["frontier_tasks"];
        auto filter = document{} << "sessionId" << sessionId << "normalizedUrl" << normalizedUrl << finalize;
        auto update = document{} << "$set" << open_document
            << "status" << "queued"
            << "retryCount" << retryCount
            << "readyAt" << timePointToBsonDate(nextReadyAt)
            << "updatedAt" << timePointToBsonDate(std::chrono::system_clock::now())
        << close_document << finalize;
        frontier.update_one(filter.view(), update.view());
        return Result<bool>::Success(true, "Frontier task retry updated");
    } catch (const mongocxx::exception& e) {
        return Result<bool>::Failure("MongoDB frontierUpdateRetry error: " + std::string(e.what()));
    }
}

Result<std::vector<std::pair<std::string,int>>> MongoDBStorage::frontierLoadPending(const std::string& sessionId,
                                                                        size_t limit) {
    try {
        auto frontier = database_["frontier_tasks"];
        using namespace bsoncxx::builder::stream;
        auto now = timePointToBsonDate(std::chrono::system_clock::now());
        auto filter = document{} << "sessionId" << sessionId << "status" << "queued" << "readyAt" << open_document << "$lte" << now << close_document << finalize;
        mongocxx::options::find opts;
        opts.limit(static_cast<int64_t>(limit));
        opts.sort(document{} << "priority" << -1 << "readyAt" << 1 << finalize);
        auto cursor = frontier.find(filter.view(), opts);
        std::vector<std::pair<std::string,int>> items;
        for (const auto& doc : cursor) {
            std::string url = std::string(doc["url"].get_string().value);
            int depth = doc["depth"].get_int32().value;
            items.emplace_back(url, depth);
        }
        return Result<std::vector<std::pair<std::string,int>>>::Success(std::move(items), "Loaded pending tasks");
    } catch (const mongocxx::exception& e) {
        return Result<std::vector<std::pair<std::string,int>>>::Failure("MongoDB frontierLoadPending error: " + std::string(e.what()));
    }
}

// CrawlLog BSON helpers
bsoncxx::document::value MongoDBStorage::crawlLogToBson(const CrawlLog& log) const {
    using namespace bsoncxx::builder::stream;
    auto builder = document{};
    if (log.id) builder << "_id" << bsoncxx::oid{*log.id};
    builder << "url" << log.url
            << "domain" << log.domain
            << "crawlTime" << timePointToBsonDate(log.crawlTime)
            << "status" << log.status
            << "httpStatusCode" << log.httpStatusCode
            << "contentSize" << static_cast<int64_t>(log.contentSize)
            << "contentType" << log.contentType;
    if (log.errorMessage) builder << "errorMessage" << *log.errorMessage;
    if (log.title) builder << "title" << *log.title;
    if (log.description) builder << "description" << *log.description;
    if (log.downloadTimeMs) builder << "downloadTimeMs" << *log.downloadTimeMs;
    auto linksArray = bsoncxx::builder::stream::array{};
    for (const auto& link : log.links) linksArray << link;
    builder << "links" << linksArray;
    return builder << finalize;
}

CrawlLog MongoDBStorage::bsonToCrawlLog(const bsoncxx::document::view& doc) const {
    CrawlLog log;
    if (doc["_id"]) log.id = std::string(doc["_id"].get_oid().value.to_string());
    log.url = std::string(doc["url"].get_string().value);
    log.domain = std::string(doc["domain"].get_string().value);
    log.crawlTime = bsonDateToTimePoint(doc["crawlTime"].get_date());
    log.status = std::string(doc["status"].get_string().value);
    log.httpStatusCode = doc["httpStatusCode"].get_int32().value;
    log.contentSize = static_cast<size_t>(doc["contentSize"].get_int64().value);
    log.contentType = std::string(doc["contentType"].get_string().value);
    if (doc["errorMessage"]) log.errorMessage = std::string(doc["errorMessage"].get_string().value);
    if (doc["title"]) log.title = std::string(doc["title"].get_string().value);
    if (doc["description"]) log.description = std::string(doc["description"].get_string().value);
    if (doc["downloadTimeMs"]) log.downloadTimeMs = doc["downloadTimeMs"].get_int64().value;
    if (doc["links"]) {
        for (const auto& link : doc["links"].get_array().value) {
            log.links.push_back(std::string(link.get_string().value));
        }
    }
    return log;
}

Result<std::string> MongoDBStorage::storeCrawlLog(const CrawlLog& log) {
    LOG_DEBUG("MongoDBStorage::storeCrawlLog called for URL: " + log.url);
    try {
        auto doc = crawlLogToBson(log);
        auto crawlLogsCollection = database_["crawl_logs"];
        auto result = crawlLogsCollection.insert_one(doc.view());
        if (result) {
            std::string id = result->inserted_id().get_oid().value.to_string();
            LOG_INFO("Crawl log stored successfully with ID: " + id + " for URL: " + log.url);
            return Result<std::string>::Success(id, "Crawl log stored successfully");
        } else {
            LOG_ERROR("Failed to insert crawl log for URL: " + log.url);
            return Result<std::string>::Failure("Failed to insert crawl log");
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error storing crawl log for URL: " + log.url + " - " + std::string(e.what()));
        return Result<std::string>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<std::vector<CrawlLog>> MongoDBStorage::getCrawlLogsByDomain(const std::string& domain, int limit, int skip) {
    LOG_DEBUG("MongoDBStorage::getCrawlLogsByDomain called for domain: " + domain);
    try {
        using namespace bsoncxx::builder::stream;
        auto crawlLogsCollection = database_["crawl_logs"];
        auto filter = document{} << "domain" << domain << finalize;
        mongocxx::options::find opts;
        opts.limit(limit);
        opts.skip(skip);
        opts.sort(document{} << "crawlTime" << -1 << finalize); // newest first
        auto cursor = crawlLogsCollection.find(filter.view(), opts);
        std::vector<CrawlLog> logs;
        for (const auto& doc : cursor) logs.push_back(bsonToCrawlLog(doc));
        LOG_INFO("Retrieved " + std::to_string(logs.size()) + " crawl logs for domain: " + domain);
        return Result<std::vector<CrawlLog>>::Success(std::move(logs), "Crawl logs retrieved successfully");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error retrieving crawl logs for domain: " + domain + " - " + std::string(e.what()));
        return Result<std::vector<CrawlLog>>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<std::vector<CrawlLog>> MongoDBStorage::getCrawlLogsByUrl(const std::string& url, int limit, int skip) {
    LOG_DEBUG("MongoDBStorage::getCrawlLogsByUrl called for URL: " + url);
    try {
        using namespace bsoncxx::builder::stream;
        auto crawlLogsCollection = database_["crawl_logs"];
        auto filter = document{} << "url" << url << finalize;
        mongocxx::options::find opts;
        opts.limit(limit);
        opts.skip(skip);
        opts.sort(document{} << "crawlTime" << -1 << finalize); // newest first
        auto cursor = crawlLogsCollection.find(filter.view(), opts);
        std::vector<CrawlLog> logs;
        for (const auto& doc : cursor) logs.push_back(bsonToCrawlLog(doc));
        LOG_INFO("Retrieved " + std::to_string(logs.size()) + " crawl logs for URL: " + url);
        return Result<std::vector<CrawlLog>>::Success(std::move(logs), "Crawl logs retrieved successfully");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error retrieving crawl logs for URL: " + url + " - " + std::string(e.what()));
        return Result<std::vector<CrawlLog>>::Failure("MongoDB error: " + std::string(e.what()));
    }
} 