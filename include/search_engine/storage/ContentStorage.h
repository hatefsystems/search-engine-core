#pragma once

#include "MongoDBStorage.h"
#include "RedisSearchStorage.h"
#include "IndexedPage.h"
#include "CrawlLog.h"
#include "../../infrastructure.h"
#include "../crawler/models/CrawlResult.h"
#include <memory>
#include <string>
#include <vector>
#include <mutex>

namespace search_engine {
namespace storage {

class ContentStorage {
private:
    std::unique_ptr<MongoDBStorage> mongoStorage_;
    std::unique_ptr<RedisSearchStorage> redisStorage_;
    
    // Connection parameters for lazy initialization
    std::string mongoConnectionString_;
    std::string mongoDatabaseName_;
    std::string redisConnectionString_;
    std::string redisIndexName_;
    
    // Connection state tracking
    bool mongoConnected_;
    bool redisConnected_;
    
    // Mutex for thread-safe MongoDB operations
    mutable std::mutex mongoMutex_;
    
    // Helper methods
    IndexedPage crawlResultToSiteProfile(const CrawlResult& crawlResult) const;
    std::string extractSearchableContent(const CrawlResult& crawlResult) const;
    
    // Lazy connection methods
    void ensureMongoConnection();
    void ensureMongoConnectionUnsafe(); // Internal method without locking
    void ensureRedisConnection();
    
public:
    // Constructor
    explicit ContentStorage(
        const std::string& mongoConnectionString = "mongodb://localhost:27017",
        const std::string& mongoDatabaseName = "search-engine",
        const std::string& redisConnectionString = "tcp://127.0.0.1:6379",
        const std::string& redisIndexName = "search_index"
    );
    
    // Destructor
    ~ContentStorage() = default;
    
    // Move semantics (disable copy)
    ContentStorage(ContentStorage&& other) noexcept = default;
    ContentStorage& operator=(ContentStorage&& other) noexcept = default;
    ContentStorage(const ContentStorage&) = delete;
    ContentStorage& operator=(const ContentStorage&) = delete;
    
    // High-level storage operations
    Result<std::string> storeCrawlResult(const CrawlResult& crawlResult);
    Result<bool> updateCrawlResult(const CrawlResult& crawlResult);
    
    // indexed page operations (MongoDB)
    Result<IndexedPage> getSiteProfile(const std::string& url);
    Result<std::vector<IndexedPage>> getSiteProfilesByDomain(const std::string& domain);
    Result<std::vector<IndexedPage>> getSiteProfilesByCrawlStatus(CrawlStatus status);
    Result<int64_t> getTotalSiteCount();
    
    // Search operations (RedisSearch)
    Result<SearchResponse> search(const SearchQuery& query);
    Result<SearchResponse> searchSimple(const std::string& query, int limit = 10);
    Result<std::vector<std::string>> suggest(const std::string& prefix, int limit = 5);
    
    // Batch operations
    Result<std::vector<std::string>> storeCrawlResults(const std::vector<CrawlResult>& crawlResults);
    
    // Index management
    Result<bool> initializeIndexes();
    Result<bool> reindexAll();
    Result<bool> dropIndexes();
    
    // Statistics and health checks
    Result<bool> testConnections();
    Result<std::unordered_map<std::string, std::string>> getStorageStats();
    
    // Utility methods
    Result<bool> deleteSiteData(const std::string& url);
    Result<bool> deleteDomainData(const std::string& domain);
    
    // CrawlLog operations
    Result<std::string> storeCrawlLog(const CrawlLog& log) { ensureMongoConnection(); return mongoStorage_->storeCrawlLog(log); }
    Result<std::vector<CrawlLog>> getCrawlLogsByDomain(const std::string& domain, int limit = 100, int skip = 0) { ensureMongoConnection(); return mongoStorage_->getCrawlLogsByDomain(domain, limit, skip); }
    Result<std::vector<CrawlLog>> getCrawlLogsByUrl(const std::string& url, int limit = 100, int skip = 0) { ensureMongoConnection(); return mongoStorage_->getCrawlLogsByUrl(url, limit, skip); }
    
    // ApiRequestLog operations
    Result<std::string> storeApiRequestLog(const search_engine::storage::ApiRequestLog& log) { ensureMongoConnection(); return mongoStorage_->storeApiRequestLog(log); }
    Result<std::vector<search_engine::storage::ApiRequestLog>> getApiRequestLogsByEndpoint(const std::string& endpoint, int limit = 100, int skip = 0) { ensureMongoConnection(); return mongoStorage_->getApiRequestLogsByEndpoint(endpoint, limit, skip); }
    Result<std::vector<search_engine::storage::ApiRequestLog>> getApiRequestLogsByIp(const std::string& ipAddress, int limit = 100, int skip = 0) { ensureMongoConnection(); return mongoStorage_->getApiRequestLogsByIp(ipAddress, limit, skip); }
    
    // Get direct access to storage layers (for advanced operations)
    MongoDBStorage* getMongoStorage() const { 
        // Ensure MongoDB connection is established before returning pointer
        // This prevents the "No MongoDB storage available" warning in Crawler
        const_cast<ContentStorage*>(this)->ensureMongoConnection(); 
        return mongoStorage_.get(); 
    }
    RedisSearchStorage* getRedisStorage() const { 
        // Ensure Redis connection is established before returning pointer
        const_cast<ContentStorage*>(this)->ensureRedisConnection(); 
        return redisStorage_.get(); 
    }
};

} // namespace storage
} // namespace search_engine 