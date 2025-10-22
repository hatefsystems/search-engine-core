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
#include <algorithm>
#include <thread>
#include <chrono>

using namespace bsoncxx::builder::stream;
using namespace search_engine::storage;

// Global mutex to serialize all MongoDB operations to prevent socket conflicts
static std::mutex g_mongoOperationMutex;

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
        
        // Use shared client to prevent connection pool exhaustion
        static std::mutex clientMutex;
        static std::unique_ptr<mongocxx::client> sharedClient;
        static std::string lastConnectionString;
        
        std::lock_guard<std::mutex> lock(clientMutex);
        
        // Create shared client if not exists or connection string changed
        if (!sharedClient || lastConnectionString != connectionString) {
            LOG_DEBUG("Creating shared MongoDB client for connection: " + connectionString);
            mongocxx::uri uri{connectionString};
            sharedClient = std::make_unique<mongocxx::client>(uri);
            lastConnectionString = connectionString;
            LOG_INFO("Shared MongoDB client created successfully");
        }
        
        // Use shared client
        client_ = sharedClient.get();
        database_ = (*client_)[databaseName];
        siteProfilesCollection_ = database_["indexed_pages"];
        LOG_INFO("Connected to MongoDB database: " + databaseName);
        LOG_DEBUG("MongoDBStorage instance created - using shared client: " + connectionString);
        
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

bsoncxx::document::value MongoDBStorage::siteProfileToBson(const IndexedPage& page) const {
    auto builder = document{};
    
    // === SYSTEM IDENTIFIERS ===
    if (page.id) {
        builder << "_id" << bsoncxx::oid{*page.id};
    }
    
    builder << "domain" << page.domain
            << "url" << page.url;
    
    // Canonical URL fields
    if (!page.canonicalUrl.empty()) {
        builder << "canonicalUrl" << page.canonicalUrl;
    }
    if (!page.canonicalHost.empty()) {
        builder << "canonicalHost" << page.canonicalHost;
    }
    if (!page.canonicalPath.empty()) {
        builder << "canonicalPath" << page.canonicalPath;
    }
    if (!page.canonicalQuery.empty()) {
        builder << "canonicalQuery" << page.canonicalQuery;
    }
    
    // === CONTENT INFORMATION ===
    builder << "title" << page.title;
    
    if (page.description) {
        builder << "description" << *page.description;
    }
    if (page.textContent) {
        builder << "textContent" << *page.textContent;
    }
    if (page.wordCount) {
        builder << "wordCount" << *page.wordCount;
    }
    if (page.category) {
        builder << "category" << *page.category;
    }
    if (page.language) {
        builder << "language" << *page.language;
    }
    
    // === AUTHORSHIP & PUBLISHING ===
    if (page.author) {
        builder << "author" << *page.author;
    }
    if (page.publisher) {
        builder << "publisher" << *page.publisher;
    }
    if (page.publishDate) {
        builder << "publishDate" << timePointToBsonDate(*page.publishDate);
    }
    builder << "lastModified" << timePointToBsonDate(page.lastModified);
    
    // === TECHNICAL METADATA ===
    if (page.hasSSL) {
        builder << "hasSSL" << *page.hasSSL;
    }
    if (page.isMobile) {
        builder << "isMobile" << *page.isMobile;
    }
    if (page.contentQuality) {
        builder << "contentQuality" << *page.contentQuality;
    }
    if (page.pageRank) {
        builder << "pageRank" << *page.pageRank;
    }
    if (page.inboundLinkCount) {
        builder << "inboundLinkCount" << *page.inboundLinkCount;
    }
    
    // === SEARCH & INDEXING ===
    builder << "isIndexed" << page.isIndexed
            << "indexedAt" << timePointToBsonDate(page.indexedAt);
    
    // Arrays (keywords and outbound links)
    auto keywordsArray = bsoncxx::builder::stream::array{};
    for (const auto& keyword : page.keywords) {
        keywordsArray << keyword;
    }
    builder << "keywords" << keywordsArray;
    
    auto outboundLinksArray = bsoncxx::builder::stream::array{};
    for (const auto& link : page.outboundLinks) {
        outboundLinksArray << link;
    }
    builder << "outboundLinks" << outboundLinksArray;
    
    // === CRAWL METADATA ===
    builder << "crawlMetadata" << crawlMetadataToBson(page.crawlMetadata);
    
    return builder << finalize;
}

IndexedPage MongoDBStorage::bsonToSiteProfile(const bsoncxx::document::view& doc) const {
    IndexedPage page;
    
    if (doc["_id"]) {
        page.id = std::string(doc["_id"].get_oid().value.to_string());
    }
    
    page.domain = std::string(doc["domain"].get_string().value);
    page.url = std::string(doc["url"].get_string().value);
    
    // Canonical URL fields
    if (doc["canonicalUrl"]) {
        page.canonicalUrl = std::string(doc["canonicalUrl"].get_string().value);
    } else {
        page.canonicalUrl = page.url; // Fallback to original URL
    }
    if (doc["canonicalHost"]) {
        page.canonicalHost = std::string(doc["canonicalHost"].get_string().value);
    } else {
        page.canonicalHost = page.domain; // Fallback to domain
    }
    if (doc["canonicalPath"]) {
        page.canonicalPath = std::string(doc["canonicalPath"].get_string().value);
    } else {
        page.canonicalPath = "/"; // Default path
    }
    if (doc["canonicalQuery"]) {
        page.canonicalQuery = std::string(doc["canonicalQuery"].get_string().value);
    } else {
        page.canonicalQuery = ""; // Empty query
    }
    
    page.title = std::string(doc["title"].get_string().value);
    page.isIndexed = doc["isIndexed"].get_bool().value;
    page.lastModified = bsonDateToTimePoint(doc["lastModified"].get_date());
    page.indexedAt = bsonDateToTimePoint(doc["indexedAt"].get_date());
    
    // Optional fields
    if (doc["description"]) {
        page.description = std::string(doc["description"].get_string().value);
    }
    if (doc["textContent"]) {
        page.textContent = std::string(doc["textContent"].get_string().value);
    }
    if (doc["language"]) {
        page.language = std::string(doc["language"].get_string().value);
    }
    if (doc["category"]) {
        page.category = std::string(doc["category"].get_string().value);
    }
    if (doc["pageRank"]) {
        page.pageRank = doc["pageRank"].get_int32().value;
    }
    if (doc["contentQuality"]) {
        page.contentQuality = doc["contentQuality"].get_double().value;
    }
    if (doc["wordCount"]) {
        page.wordCount = doc["wordCount"].get_int32().value;
    }
    if (doc["isMobile"]) {
        page.isMobile = doc["isMobile"].get_bool().value;
    }
    if (doc["hasSSL"]) {
        page.hasSSL = doc["hasSSL"].get_bool().value;
    }
    if (doc["inboundLinkCount"]) {
        page.inboundLinkCount = doc["inboundLinkCount"].get_int32().value;
    }
    if (doc["author"]) {
        page.author = std::string(doc["author"].get_string().value);
    }
    if (doc["publisher"]) {
        page.publisher = std::string(doc["publisher"].get_string().value);
    }
    if (doc["publishDate"]) {
        page.publishDate = bsonDateToTimePoint(doc["publishDate"].get_date());
    }
    
    // Arrays
    if (doc["keywords"]) {
        for (const auto& keyword : doc["keywords"].get_array().value) {
            page.keywords.push_back(std::string(keyword.get_string().value));
        }
    }
    
    if (doc["outboundLinks"]) {
        for (const auto& link : doc["outboundLinks"].get_array().value) {
            page.outboundLinks.push_back(std::string(link.get_string().value));
        }
    }
    
    // Crawl metadata
    if (doc["crawlMetadata"]) {
        page.crawlMetadata = bsonToCrawlMetadata(doc["crawlMetadata"].get_document().view());
    }
    
    return page;
}

Result<std::string> MongoDBStorage::storeIndexedPage(const IndexedPage& page) {
    LOG_DEBUG("MongoDBStorage::storeIndexedPage called for URL: " + page.url);
    
    // Validate content type - only save HTML/text content
    std::string contentType = page.crawlMetadata.contentType;
    std::string lowerContentType = contentType;
    std::transform(lowerContentType.begin(), lowerContentType.end(), lowerContentType.begin(), ::tolower);
    
    // List of allowed content types for saving
    bool isAllowedContentType = (
        lowerContentType.find("text/html") == 0 ||
        lowerContentType.find("text/plain") == 0 ||
        lowerContentType.find("application/json") == 0 ||
        lowerContentType.find("application/xml") == 0 ||
        lowerContentType.find("text/xml") == 0 ||
        lowerContentType.find("application/rss+xml") == 0 ||
        lowerContentType.find("application/atom+xml") == 0
    );
    
    if (!isAllowedContentType) {
        LOG_INFO("Skipping page save - unsupported content type: " + contentType + " for URL: " + page.url);
        return Result<std::string>::Failure("Page skipped - unsupported content type: " + contentType);
    }
    
    // Validate that page has both title and textContent before saving
    bool hasTitle = !page.title.empty();
    bool hasTextContent = page.textContent.has_value() && !page.textContent->empty();
    
    if (!hasTitle && !hasTextContent) {
        std::string reason = "missing both title and textContent";
        
        LOG_INFO("Skipping page save - " + reason + " for URL: " + page.url);
        return Result<std::string>::Failure("Page skipped - " + reason);
    }
    
    try {
        // Serialize all MongoDB operations to prevent socket conflicts
        std::lock_guard<std::mutex> lock(g_mongoOperationMutex);
        
        // Use canonical URL for upsert to prevent duplicates
        auto filter = document{} << "canonicalUrl" << page.canonicalUrl << finalize;

        
        // Build the document to insert/update with improved field ordering
        auto now = std::chrono::system_clock::now();
        auto documentToUpsert = document{}
            // === SYSTEM IDENTIFIERS ===
            << "domain" << page.domain
            << "url" << page.url
            << "canonicalUrl" << page.canonicalUrl
            << "canonicalHost" << page.canonicalHost
            << "canonicalPath" << page.canonicalPath
            << "canonicalQuery" << page.canonicalQuery
            
            // === CONTENT INFORMATION ===
            << "title" << page.title
            << "description" << (page.description ? *page.description : "")
            << "textContent" << (page.textContent ? *page.textContent : "")
            << "wordCount" << (page.wordCount ? *page.wordCount : 0)
            << "category" << (page.category ? *page.category : "")
            
            // === AUTHORSHIP & PUBLISHING ===
            << "author" << (page.author ? *page.author : "")
            << "publisher" << (page.publisher ? *page.publisher : "")
            << "publishDate" << (page.publishDate ? timePointToBsonDate(*page.publishDate) : timePointToBsonDate(now))
            << "lastModified" << timePointToBsonDate(page.lastModified)
            
            // === TECHNICAL METADATA ===
            << "hasSSL" << (page.hasSSL ? *page.hasSSL : false)
            << "isMobile" << (page.isMobile ? *page.isMobile : false)
            << "contentQuality" << (page.contentQuality ? *page.contentQuality : 0.0)
            << "pageRank" << (page.pageRank ? *page.pageRank : 0)
            << "inboundLinkCount" << (page.inboundLinkCount ? *page.inboundLinkCount : 0)
            
            // === SEARCH & INDEXING ===
            << "isIndexed" << page.isIndexed
            << "indexedAt" << timePointToBsonDate(page.indexedAt)
            
            // === CRAWL METADATA ===
            << "crawlMetadata" << open_document
                << "firstCrawlTime" << timePointToBsonDate(page.crawlMetadata.firstCrawlTime)
                << "lastCrawlTime" << timePointToBsonDate(page.crawlMetadata.lastCrawlTime)
                << "lastCrawlStatus" << crawlStatusToString(page.crawlMetadata.lastCrawlStatus)
                << "lastErrorMessage" << (page.crawlMetadata.lastErrorMessage ? *page.crawlMetadata.lastErrorMessage : "")
                << "crawlCount" << page.crawlMetadata.crawlCount
                << "crawlIntervalHours" << page.crawlMetadata.crawlIntervalHours
                << "userAgent" << page.crawlMetadata.userAgent
                << "httpStatusCode" << page.crawlMetadata.httpStatusCode
                << "contentSize" << static_cast<int64_t>(page.crawlMetadata.contentSize)
                << "contentType" << page.crawlMetadata.contentType
                << "crawlDurationMs" << page.crawlMetadata.crawlDurationMs
            << close_document
            
            // === SYSTEM TIMESTAMPS ===
            << "updatedAt" << timePointToBsonDate(now)
        << finalize;
        
        // Create the upsert operation with $setOnInsert for fields that should only be set on insert
        auto upsertDoc = document{}
            << "$set" << documentToUpsert.view()
            << "$setOnInsert" << open_document
                << "createdAt" << timePointToBsonDate(now)
            << close_document
        << finalize;
        
        // Perform atomic upsert operation - this handles both insert and update in one command
        auto result = siteProfilesCollection_.find_one_and_update(
            filter.view(),
            upsertDoc.view(),
            mongocxx::options::find_one_and_update{}
                .upsert(true)
                .return_document(mongocxx::options::return_document::k_after)
        );
        
        if (result) {
            std::string id = result->view()["_id"].get_oid().value.to_string();
            LOG_INFO("indexed page upserted successfully with ID: " + id + " for canonical URL: " + page.canonicalUrl);
            return Result<std::string>::Success(id, "indexed page upserted successfully");
        } else {
            LOG_ERROR("Failed to upsert indexed page for canonical URL: " + page.canonicalUrl);
            return Result<std::string>::Failure("Failed to upsert indexed page");
        }
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error upserting indexed page for canonical URL: " + page.canonicalUrl + " - " + std::string(e.what()));
        return Result<std::string>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<IndexedPage> MongoDBStorage::getSiteProfile(const std::string& url) {
    LOG_DEBUG("MongoDBStorage::getSiteProfile called for canonicalUrl: " + url);
    try {
        // Serialize all MongoDB operations to prevent socket conflicts
        std::lock_guard<std::mutex> lock(g_mongoOperationMutex);
        
        auto filter = document{} << "canonicalUrl" << url << finalize;
        LOG_TRACE("MongoDB query filter created for canonicalUrl: " + url);
        
        auto result = siteProfilesCollection_.find_one(filter.view());
        
        if (result) {
            LOG_INFO("indexed page found and retrieved for URL: " + url);
            return Result<IndexedPage>::Success(
                bsonToSiteProfile(result->view()),
                "indexed page retrieved successfully"
            );
        } else {
            LOG_WARNING("indexed page not found for URL: " + url);
            return Result<IndexedPage>::Failure("indexed page not found for URL: " + url);
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error retrieving indexed page for URL: " + url + " - " + std::string(e.what()));
        return Result<IndexedPage>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<IndexedPage> MongoDBStorage::getSiteProfileById(const std::string& id) {
    try {
        // Serialize all MongoDB operations to prevent socket conflicts
        std::lock_guard<std::mutex> lock(g_mongoOperationMutex);
        
        auto filter = document{} << "_id" << bsoncxx::oid{id} << finalize;
        auto result = siteProfilesCollection_.find_one(filter.view());
        
        if (result) {
            return Result<IndexedPage>::Success(
                bsonToSiteProfile(result->view()),
                "indexed page retrieved successfully"
            );
        } else {
            return Result<IndexedPage>::Failure("indexed page not found for ID: " + id);
        }
    } catch (const mongocxx::exception& e) {
        return Result<IndexedPage>::Failure("MongoDB error: " + std::string(e.what()));
    }
}


Result<bool> MongoDBStorage::deleteSiteProfile(const std::string& url) {
    LOG_DEBUG("MongoDBStorage::deleteSiteProfile called for URL: " + url);
    try {
        // Serialize all MongoDB operations to prevent socket conflicts
        std::lock_guard<std::mutex> lock(g_mongoOperationMutex);
        
        auto filter = document{} << "url" << url << finalize;
        LOG_TRACE("Delete filter created for URL: " + url);
        
        auto result = siteProfilesCollection_.delete_one(filter.view());
        
        if (result && result->deleted_count() > 0) {
            LOG_INFO("indexed page deleted successfully for URL: " + url);
            return Result<bool>::Success(true, "indexed page deleted successfully");
        } else {
            LOG_WARNING("indexed page not found for deletion, URL: " + url);
            return Result<bool>::Failure("indexed page not found for URL: " + url);
        }
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error deleting indexed page for URL: " + url + " - " + std::string(e.what()));
        return Result<bool>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<std::vector<IndexedPage>> MongoDBStorage::getSiteProfilesByDomain(const std::string& domain) {
    LOG_DEBUG("MongoDBStorage::getSiteProfilesByDomain called for domain: " + domain);
    try {
        auto filter = document{} << "domain" << domain << finalize;
        auto cursor = siteProfilesCollection_.find(filter.view());
        
        std::vector<IndexedPage> profiles;
        for (const auto& doc : cursor) {
            profiles.push_back(bsonToSiteProfile(doc));
        }
        
        LOG_INFO("Retrieved " + std::to_string(profiles.size()) + " indexed pages for domain: " + domain);
        return Result<std::vector<IndexedPage>>::Success(
            std::move(profiles),
            "indexed pages retrieved successfully for domain: " + domain
        );
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error retrieving indexed pages for domain: " + domain + " - " + std::string(e.what()));
        return Result<std::vector<IndexedPage>>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<std::vector<IndexedPage>> MongoDBStorage::getSiteProfilesByCrawlStatus(CrawlStatus status) {
    try {
        auto filter = document{} << "crawlMetadata.lastCrawlStatus" << crawlStatusToString(status) << finalize;
        auto cursor = siteProfilesCollection_.find(filter.view());
        
        std::vector<IndexedPage> profiles;
        for (const auto& doc : cursor) {
            profiles.push_back(bsonToSiteProfile(doc));
        }
        
        return Result<std::vector<IndexedPage>>::Success(
            std::move(profiles),
            "indexed pages retrieved successfully for status"
        );
    } catch (const mongocxx::exception& e) {
        return Result<std::vector<IndexedPage>>::Failure("MongoDB error: " + std::string(e.what()));
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
    
    // Serialize all MongoDB operations to prevent socket conflicts
    std::lock_guard<std::mutex> lock(g_mongoOperationMutex);
    
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
        
        // Create unique index for canonical URL to prevent duplicates
        try {
            auto canonicalUrlIndex = document{} << "canonicalUrl" << 1 << finalize;
            mongocxx::options::index canonicalUrlIndexOptions;
            canonicalUrlIndexOptions.unique(true);
            siteProfilesCollection_.create_index(canonicalUrlIndex.view(), canonicalUrlIndexOptions);
            LOG_INFO("Unique canonical URL index created successfully");
        } catch (const mongocxx::exception& e) {
            LOG_WARNING("Canonical URL index may already exist or failed to create: " + std::string(e.what()));
        }
        
        // Create compound index for canonical host + path for efficient domain-based queries
        try {
            auto canonicalHostPathIndex = document{} << "canonicalHost" << 1 << "canonicalPath" << 1 << finalize;
            siteProfilesCollection_.create_index(canonicalHostPathIndex.view());
            LOG_INFO("Canonical host+path index created successfully");
        } catch (const mongocxx::exception& e) {
            LOG_WARNING("Canonical host+path index may already exist or failed to create: " + std::string(e.what()));
        }
        
        // Create text index for full-text search with UTF-8/Unicode support
        try {
            auto textIndex = document{} 
                << "title" << "text" 
                << "description" << "text" 
                << "textContent" << "text" 
                << "url" << "text" 
            << finalize;
            
            mongocxx::options::index textIndexOptions;
            // Set language to 'none' for better multilingual support including Persian/Farsi
            textIndexOptions.default_language("none");
            
            siteProfilesCollection_.create_index(textIndex.view(), textIndexOptions);
            LOG_INFO("Text search index created successfully with multilingual support");
        } catch (const mongocxx::exception& e) {
            LOG_WARNING("Text index may already exist or failed to create: " + std::string(e.what()));
        }

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
        // Serialize all MongoDB operations to prevent socket conflicts
        std::lock_guard<std::mutex> lock(g_mongoOperationMutex);
        
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
        // Serialize all MongoDB operations to prevent socket conflicts
        std::lock_guard<std::mutex> lock(g_mongoOperationMutex);
        
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
        // Serialize all MongoDB operations to prevent socket conflicts
        std::lock_guard<std::mutex> lock(g_mongoOperationMutex);
        
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
    LOG_DEBUG("MongoDBStorage::frontierLoadPending called for sessionId: " + sessionId + ", limit: " + std::to_string(limit));
    
    // Serialize all MongoDB operations to prevent socket conflicts
    std::lock_guard<std::mutex> lock(g_mongoOperationMutex);
    
    // Retry logic for connection issues
    int maxRetries = 3;
    int retryDelay = 100; // milliseconds
    
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        try {
            LOG_DEBUG("MongoDBStorage::frontierLoadPending - Attempt " + std::to_string(attempt + 1) + "/" + std::to_string(maxRetries));
            
            auto frontier = database_["frontier_tasks"];
            using namespace bsoncxx::builder::stream;
            auto now = timePointToBsonDate(std::chrono::system_clock::now());
            auto filter = document{} << "sessionId" << sessionId << "status" << "queued" << "readyAt" << open_document << "$lte" << now << close_document << finalize;
            mongocxx::options::find opts;
            opts.limit(static_cast<int64_t>(limit));
            opts.sort(document{} << "priority" << -1 << "readyAt" << 1 << finalize);
            
            LOG_DEBUG("MongoDBStorage::frontierLoadPending - Executing find query");
            auto cursor = frontier.find(filter.view(), opts);
            
            std::vector<std::pair<std::string,int>> items;
            size_t count = 0;
            for (const auto& doc : cursor) {
                std::string url = std::string(doc["url"].get_string().value);
                int depth = doc["depth"].get_int32().value;
                items.emplace_back(url, depth);
                count++;
            }
            
            LOG_DEBUG("MongoDBStorage::frontierLoadPending - Successfully loaded " + std::to_string(count) + " pending tasks");
            return Result<std::vector<std::pair<std::string,int>>>::Success(std::move(items), "Loaded pending tasks");
            
        } catch (const mongocxx::exception& e) {
            LOG_ERROR("MongoDB frontierLoadPending error (attempt " + std::to_string(attempt + 1) + "/" + std::to_string(maxRetries) + "): " + std::string(e.what()));
            
            // If this is the last attempt, return failure
            if (attempt == maxRetries - 1) {
                return Result<std::vector<std::pair<std::string,int>>>::Failure("MongoDB frontierLoadPending error after " + std::to_string(maxRetries) + " attempts: " + std::string(e.what()));
            }
            
            // Wait before retrying
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
            retryDelay *= 2; // Exponential backoff
        } catch (const std::exception& e) {
            LOG_ERROR("Unexpected error in frontierLoadPending: " + std::string(e.what()));
            return Result<std::vector<std::pair<std::string,int>>>::Failure("Unexpected error: " + std::string(e.what()));
        }
    }
    
    return Result<std::vector<std::pair<std::string,int>>>::Failure("Failed to load pending tasks after all retries");
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
        // Serialize all MongoDB operations to prevent socket conflicts
        std::lock_guard<std::mutex> lock(g_mongoOperationMutex);
        
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

// ApiRequestLog BSON helpers
bsoncxx::document::value MongoDBStorage::apiRequestLogToBson(const ApiRequestLog& log) const {
    using namespace bsoncxx::builder::stream;
    auto builder = document{};
    if (log.id) builder << "_id" << bsoncxx::oid{*log.id};
    builder << "endpoint" << log.endpoint
            << "method" << log.method
            << "ipAddress" << log.ipAddress
            << "userAgent" << log.userAgent
            << "createdAt" << timePointToBsonDate(log.createdAt)
            << "status" << log.status
            << "responseTimeMs" << log.responseTimeMs;
    if (log.requestBody) builder << "requestBody" << *log.requestBody;
    if (log.sessionId) builder << "sessionId" << *log.sessionId;
    if (log.userId) builder << "userId" << *log.userId;
    if (log.errorMessage) builder << "errorMessage" << *log.errorMessage;
    return builder << finalize;
}

ApiRequestLog MongoDBStorage::bsonToApiRequestLog(const bsoncxx::document::view& doc) const {
    ApiRequestLog log;
    if (doc["_id"]) log.id = std::string(doc["_id"].get_oid().value.to_string());
    log.endpoint = std::string(doc["endpoint"].get_string().value);
    log.method = std::string(doc["method"].get_string().value);
    log.ipAddress = std::string(doc["ipAddress"].get_string().value);
    log.userAgent = std::string(doc["userAgent"].get_string().value);
    log.createdAt = bsonDateToTimePoint(doc["createdAt"].get_date());
    log.status = std::string(doc["status"].get_string().value);
    log.responseTimeMs = doc["responseTimeMs"].get_int32().value;
    if (doc["requestBody"]) log.requestBody = std::string(doc["requestBody"].get_string().value);
    if (doc["sessionId"]) log.sessionId = std::string(doc["sessionId"].get_string().value);
    if (doc["userId"]) log.userId = std::string(doc["userId"].get_string().value);
    if (doc["errorMessage"]) log.errorMessage = std::string(doc["errorMessage"].get_string().value);
    return log;
}

Result<std::string> MongoDBStorage::storeApiRequestLog(const ApiRequestLog& log) {
    LOG_DEBUG("MongoDBStorage::storeApiRequestLog called for endpoint: " + log.endpoint);
    
    // Serialize all MongoDB operations to prevent socket conflicts
    std::lock_guard<std::mutex> lock(g_mongoOperationMutex);
    
    // Retry logic for connection issues
    int maxRetries = 3;
    int retryDelay = 100; // milliseconds
    
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        try {
            auto doc = apiRequestLogToBson(log);
            auto apiRequestLogsCollection = database_["api_request_logs"];
            auto result = apiRequestLogsCollection.insert_one(doc.view());
            if (result) {
                std::string id = result->inserted_id().get_oid().value.to_string();
                LOG_INFO("API request log stored successfully with ID: " + id + " for endpoint: " + log.endpoint);
                return Result<std::string>::Success(id, "API request log stored successfully");
            } else {
                LOG_ERROR("Failed to insert API request log for endpoint: " + log.endpoint);
                return Result<std::string>::Failure("Failed to insert API request log");
            }
        } catch (const mongocxx::exception& e) {
            LOG_ERROR("MongoDB error storing API request log for endpoint: " + log.endpoint + " (attempt " + std::to_string(attempt + 1) + "/" + std::to_string(maxRetries) + ") - " + std::string(e.what()));
            
            // If this is the last attempt, return failure
            if (attempt == maxRetries - 1) {
                return Result<std::string>::Failure("MongoDB error after " + std::to_string(maxRetries) + " attempts: " + std::string(e.what()));
            }
            
            // Wait before retrying
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay));
            retryDelay *= 2; // Exponential backoff
        } catch (const std::exception& e) {
            LOG_ERROR("Unexpected error storing API request log for endpoint: " + log.endpoint + " - " + std::string(e.what()));
            return Result<std::string>::Failure("Unexpected error: " + std::string(e.what()));
        }
    }
    
    return Result<std::string>::Failure("Failed to store API request log after all retries");
}

Result<std::vector<ApiRequestLog>> MongoDBStorage::getApiRequestLogsByEndpoint(const std::string& endpoint, int limit, int skip) {
    LOG_DEBUG("MongoDBStorage::getApiRequestLogsByEndpoint called for endpoint: " + endpoint);
    try {
        using namespace bsoncxx::builder::stream;
        auto apiRequestLogsCollection = database_["api_request_logs"];
        auto filter = document{} << "endpoint" << endpoint << finalize;
        mongocxx::options::find opts;
        opts.limit(limit);
        opts.skip(skip);
        opts.sort(document{} << "createdAt" << -1 << finalize); // newest first
        auto cursor = apiRequestLogsCollection.find(filter.view(), opts);
        std::vector<ApiRequestLog> logs;
        for (const auto& doc : cursor) logs.push_back(bsonToApiRequestLog(doc));
        LOG_INFO("Retrieved " + std::to_string(logs.size()) + " API request logs for endpoint: " + endpoint);
        return Result<std::vector<ApiRequestLog>>::Success(std::move(logs), "API request logs retrieved successfully");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error retrieving API request logs for endpoint: " + endpoint + " - " + std::string(e.what()));
        return Result<std::vector<ApiRequestLog>>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<std::vector<ApiRequestLog>> MongoDBStorage::getApiRequestLogsByIp(const std::string& ipAddress, int limit, int skip) {
    LOG_DEBUG("MongoDBStorage::getApiRequestLogsByIp called for IP: " + ipAddress);
    try {
        using namespace bsoncxx::builder::stream;
        auto apiRequestLogsCollection = database_["api_request_logs"];
        auto filter = document{} << "ipAddress" << ipAddress << finalize;
        mongocxx::options::find opts;
        opts.limit(limit);
        opts.skip(skip);
        opts.sort(document{} << "createdAt" << -1 << finalize); // newest first
        auto cursor = apiRequestLogsCollection.find(filter.view(), opts);
        std::vector<ApiRequestLog> logs;
        for (const auto& doc : cursor) logs.push_back(bsonToApiRequestLog(doc));
        LOG_INFO("Retrieved " + std::to_string(logs.size()) + " API request logs for IP: " + ipAddress);
        return Result<std::vector<ApiRequestLog>>::Success(std::move(logs), "API request logs retrieved successfully");
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error retrieving API request logs for IP: " + ipAddress + " - " + std::string(e.what()));
        return Result<std::vector<ApiRequestLog>>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<std::vector<IndexedPage>> MongoDBStorage::searchSiteProfiles(const std::string& query, int limit, int skip) {
    LOG_DEBUG("MongoDBStorage::searchSiteProfiles called with query: " + query + ", limit: " + std::to_string(limit) + ", skip: " + std::to_string(skip));
    
    try {
        using namespace bsoncxx::builder::stream;
        
        // Use aggregation pipeline for deduplication by canonical URL
        mongocxx::pipeline pipeline;
        
        // Stage 1: Match documents based on search criteria
        bsoncxx::document::value matchFilter = document{} << finalize;
        bool useTextSearch = false;
        
        try {
            // Try MongoDB text search first
            matchFilter = document{} << "$text" << open_document << "$search" << query << close_document << finalize;
            LOG_DEBUG("Using MongoDB text search for query: " + query);
            useTextSearch = true;
        } catch (const std::exception& e) {
            LOG_DEBUG("Text search not available, using regex search: " + std::string(e.what()));
            useTextSearch = false;
        }
        
        if (!useTextSearch) {
            // Fallback to regex search with better Unicode support
            auto searchRegex = document{} << "$regex" << query << "$options" << "iu" << finalize;
            
            matchFilter = document{} 
                << "$or" << open_array
                    << open_document << "title" << searchRegex.view() << close_document
                    << open_document << "url" << searchRegex.view() << close_document
                    << open_document << "description" << searchRegex.view() << close_document
                    << open_document << "textContent" << searchRegex.view() << close_document
                << close_array
            << finalize;
        }
        
        pipeline.match(matchFilter.view());
        
        // Stage 2: Sort by relevance and freshness
        if (useTextSearch) {
            // Sort by text score first, then by last modified
            auto sortDoc = document{} 
                << "score" << open_document << "$meta" << "textScore" << close_document 
                << "lastModified" << -1 
            << finalize;
            pipeline.sort(sortDoc.view());
        } else {
            // Sort by last modified
            auto sortDoc = document{} << "lastModified" << -1 << finalize;
            pipeline.sort(sortDoc.view());
        }
        
        // Stage 3: Group by canonical URL to deduplicate, keeping the best document
        auto groupDoc = document{}
            << "_id" << "$canonicalUrl"
            << "bestDoc" << open_document
                << "$first" << "$$ROOT"
            << close_document
        << finalize;
        pipeline.group(groupDoc.view());
        
        // Stage 4: Replace root with the best document
        auto replaceRootDoc = document{}
            << "newRoot" << "$bestDoc"
        << finalize;
        pipeline.replace_root(replaceRootDoc.view());
        
        // Stage 5: Sort again after deduplication
        if (useTextSearch) {
            auto sortDoc = document{} 
                << "score" << open_document << "$meta" << "textScore" << close_document 
                << "lastModified" << -1 
            << finalize;
            pipeline.sort(sortDoc.view());
        } else {
            auto sortDoc = document{} << "lastModified" << -1 << finalize;
            pipeline.sort(sortDoc.view());
        }
        
        // Stage 6: Skip and limit for pagination
        pipeline.skip(skip);
        pipeline.limit(limit);
        
        auto cursor = siteProfilesCollection_.aggregate(pipeline);
        
        std::vector<IndexedPage> profiles;
        for (const auto& doc : cursor) {
            profiles.push_back(bsonToSiteProfile(doc));
        }
        
        LOG_INFO("Retrieved " + std::to_string(profiles.size()) + " deduplicated indexed pages for search query: " + query);
        return Result<std::vector<IndexedPage>>::Success(
            std::move(profiles),
            "indexed pages search completed successfully with deduplication"
        );
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error searching indexed pages for query: " + query + " - " + std::string(e.what()));
        return Result<std::vector<IndexedPage>>::Failure("MongoDB error: " + std::string(e.what()));
    }
}

Result<int64_t> MongoDBStorage::countSearchResults(const std::string& query) {
    LOG_DEBUG("MongoDBStorage::countSearchResults called with query: " + query);
    
    try {
        using namespace bsoncxx::builder::stream;
        
        // Use aggregation pipeline for deduplicated count
        mongocxx::pipeline pipeline;
        
        // Stage 1: Match documents based on search criteria (same as searchSiteProfiles)
        bsoncxx::document::value matchFilter = document{} << finalize;
        bool useTextSearch = false;
        
        try {
            // Try MongoDB text search first
            matchFilter = document{} << "$text" << open_document << "$search" << query << close_document << finalize;
            LOG_DEBUG("Using MongoDB text search for count query: " + query);
            useTextSearch = true;
        } catch (const std::exception& e) {
            LOG_DEBUG("Text search for count not available, using regex search: " + std::string(e.what()));
            useTextSearch = false;
        }
        
        if (!useTextSearch) {
            // Fallback to regex search with Unicode support
            auto searchRegex = document{} << "$regex" << query << "$options" << "iu" << finalize;
            
            matchFilter = document{} 
                << "$or" << open_array
                    << open_document << "title" << searchRegex.view() << close_document
                    << open_document << "url" << searchRegex.view() << close_document
                    << open_document << "description" << searchRegex.view() << close_document
                    << open_document << "textContent" << searchRegex.view() << close_document
                << close_array
            << finalize;
        }
        
        pipeline.match(matchFilter.view());
        
        // Stage 2: Group by canonical URL to deduplicate
        auto groupDoc = document{}
            << "_id" << "$canonicalUrl"
        << finalize;
        pipeline.group(groupDoc.view());
        
        // Stage 3: Count the deduplicated results
        auto countDoc = document{}
            << "_id" << bsoncxx::types::b_null{}
            << "count" << open_document << "$sum" << 1 << close_document
        << finalize;
        pipeline.group(countDoc.view());
        
        auto cursor = siteProfilesCollection_.aggregate(pipeline);
        
        int64_t count = 0;
        for (const auto& doc : cursor) {
            if (doc["count"]) {
                auto countElement = doc["count"];
                if (countElement.type() == bsoncxx::type::k_int32) {
                    count = countElement.get_int32().value;
                } else if (countElement.type() == bsoncxx::type::k_int64) {
                    count = countElement.get_int64().value;
                } else if (countElement.type() == bsoncxx::type::k_double) {
                    count = static_cast<int64_t>(countElement.get_double().value);
                }
            }
        }
        
        LOG_INFO("Found " + std::to_string(count) + " deduplicated results for search query: " + query);
        return Result<int64_t>::Success(count, "Deduplicated search result count retrieved successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB error counting search results for query: " + query + " - " + std::string(e.what()));
        return Result<int64_t>::Failure("MongoDB error: " + std::string(e.what()));
    }
} 