#pragma once

#include "MongoDBStorage.h"
#ifdef REDIS_AVAILABLE
#include "RedisSearchStorage.h"
#endif
#include "SiteProfile.h"
#include "CrawlLog.h"
#include "../../infrastructure.h"
#include "../crawler/models/CrawlResult.h"
#include <memory>
#include <string>
#include <vector>

namespace search_engine {
namespace storage {

class ContentStorage {
private:
    std::unique_ptr<MongoDBStorage> mongoStorage_;
#ifdef REDIS_AVAILABLE
    std::unique_ptr<RedisSearchStorage> redisStorage_;
#endif
    
    // Connection parameters for lazy initialization
    std::string mongoConnectionString_;
    std::string mongoDatabaseName_;
#ifdef REDIS_AVAILABLE
    std::string redisConnectionString_;
    std::string redisIndexName_;
#endif
    
    // Connection state tracking
    bool mongoConnected_;
#ifdef REDIS_AVAILABLE
    bool redisConnected_;
#endif
    
    // Helper methods
    SiteProfile crawlResultToSiteProfile(const CrawlResult& crawlResult) const;
    std::string extractSearchableContent(const CrawlResult& crawlResult) const;
    
    // Lazy connection methods
    void ensureMongoConnection();
#ifdef REDIS_AVAILABLE
    void ensureRedisConnection();
#endif
    
public:
    // Constructor
    explicit ContentStorage(
        const std::string& mongoConnectionString = "mongodb://localhost:27017",
        const std::string& mongoDatabaseName = "search-engine"
#ifdef REDIS_AVAILABLE
        ,const std::string& redisConnectionString = "tcp://127.0.0.1:6379",
        const std::string& redisIndexName = "search_index"
#endif
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
    
    // Site profile operations (MongoDB)
    Result<SiteProfile> getSiteProfile(const std::string& url);
    Result<std::vector<SiteProfile>> getSiteProfilesByDomain(const std::string& domain);
    Result<std::vector<SiteProfile>> getSiteProfilesByCrawlStatus(CrawlStatus status);
    Result<int64_t> getTotalSiteCount();
    
#ifdef REDIS_AVAILABLE
    // Search operations (RedisSearch)
    Result<SearchResponse> search(const SearchQuery& query);
    Result<SearchResponse> searchSimple(const std::string& query, int limit = 10);
    Result<std::vector<std::string>> suggest(const std::string& prefix, int limit = 5);
#endif
    
    // Batch operations
    Result<std::vector<std::string>> storeCrawlResults(const std::vector<CrawlResult>& crawlResults);
    
    // Index management
    Result<bool> initializeIndexes();
#ifdef REDIS_AVAILABLE
    Result<bool> reindexAll();
    Result<bool> dropIndexes();
#endif
    
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
    MongoDBStorage* getMongoStorage() const { return mongoStorage_.get(); }
#ifdef REDIS_AVAILABLE
    RedisSearchStorage* getRedisStorage() const { return redisStorage_.get(); }
#endif
};

} // namespace storage
} // namespace search_engine 