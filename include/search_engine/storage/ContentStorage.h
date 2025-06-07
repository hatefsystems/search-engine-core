#pragma once

#include "MongoDBStorage.h"
#ifdef REDIS_AVAILABLE
#include "RedisSearchStorage.h"
#endif
#include "SiteProfile.h"
#include "../../infrastructure.h"
#include "../../src/crawler/models/CrawlResult.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace search_engine {
namespace storage {

class ContentStorage {
private:
    std::unique_ptr<MongoDBStorage> mongoStorage_;
#ifdef REDIS_AVAILABLE
    std::unique_ptr<RedisSearchStorage> redisStorage_;
#endif
    
    // Helper methods
    SiteProfile crawlResultToSiteProfile(const CrawlResult& crawlResult) const;
    std::string extractSearchableContent(const CrawlResult& crawlResult) const;
    
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
    
    // Get direct access to storage layers (for advanced operations)
    MongoDBStorage* getMongoStorage() const { return mongoStorage_.get(); }
#ifdef REDIS_AVAILABLE
    RedisSearchStorage* getRedisStorage() const { return redisStorage_.get(); }
#endif
};

} // namespace storage
} // namespace search_engine 