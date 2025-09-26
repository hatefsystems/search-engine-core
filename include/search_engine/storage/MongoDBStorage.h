#pragma once

#include "SiteProfile.h"
#include "CrawlLog.h"
#include "ApiRequestLog.h"
#include "../../infrastructure.h"
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/instance.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <memory>
#include <vector>
#include <optional>

namespace search_engine {
namespace storage {

class MongoDBStorage {
private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection siteProfilesCollection_;
    
    // Conversion methods between SiteProfile and BSON
    bsoncxx::document::value siteProfileToBson(const SiteProfile& profile) const;
    SiteProfile bsonToSiteProfile(const bsoncxx::document::view& doc) const;
    
    // Helper methods for BSON conversion
    bsoncxx::document::value crawlMetadataToBson(const CrawlMetadata& metadata) const;
    CrawlMetadata bsonToCrawlMetadata(const bsoncxx::document::view& doc) const;
    
    // CrawlLog BSON helpers
    bsoncxx::document::value crawlLogToBson(const CrawlLog& log) const;
    CrawlLog bsonToCrawlLog(const bsoncxx::document::view& doc) const;
    
    // ApiRequestLog BSON helpers
    bsoncxx::document::value apiRequestLogToBson(const ApiRequestLog& log) const;
    ApiRequestLog bsonToApiRequestLog(const bsoncxx::document::view& doc) const;
    
    static std::string crawlStatusToString(CrawlStatus status);
    static CrawlStatus stringToCrawlStatus(const std::string& status);

public:
    // Constructor with connection string
    explicit MongoDBStorage(const std::string& connectionString = "mongodb://localhost:27017",
                           const std::string& databaseName = "search-engine");
    
    // Destructor
    ~MongoDBStorage() = default;
    
    // Move constructor and assignment (delete copy operations for RAII)
    MongoDBStorage(MongoDBStorage&& other) noexcept = default;
    MongoDBStorage& operator=(MongoDBStorage&& other) noexcept = default;
    MongoDBStorage(const MongoDBStorage&) = delete;
    MongoDBStorage& operator=(const MongoDBStorage&) = delete;
    
    // Core storage operations
    Result<std::string> storeSiteProfile(const SiteProfile& profile);
    Result<SiteProfile> getSiteProfile(const std::string& url);
    Result<SiteProfile> getSiteProfileById(const std::string& id);
    Result<bool> updateSiteProfile(const SiteProfile& profile);
    Result<bool> deleteSiteProfile(const std::string& url);
    
    // Batch operations
    Result<std::vector<std::string>> storeSiteProfiles(const std::vector<SiteProfile>& profiles);
    Result<std::vector<SiteProfile>> getSiteProfilesByDomain(const std::string& domain);
    Result<std::vector<SiteProfile>> getSiteProfilesByCrawlStatus(CrawlStatus status);
    
    // Search and filtering
    Result<std::vector<SiteProfile>> searchSiteProfiles(
        const std::string& query,
        int limit = 100,
        int skip = 0
    );
    Result<int64_t> countSearchResults(const std::string& query);
    
    // Statistics and maintenance
    Result<int64_t> getTotalSiteCount();
    Result<int64_t> getSiteCountByStatus(CrawlStatus status);
    Result<bool> createIndexes();
    
    // CrawlLog operations
    Result<std::string> storeCrawlLog(const CrawlLog& log);
    Result<std::vector<CrawlLog>> getCrawlLogsByDomain(const std::string& domain, int limit = 100, int skip = 0);
    Result<std::vector<CrawlLog>> getCrawlLogsByUrl(const std::string& url, int limit = 100, int skip = 0);
    
    // ApiRequestLog operations
    Result<std::string> storeApiRequestLog(const ApiRequestLog& log);
    Result<std::vector<ApiRequestLog>> getApiRequestLogsByEndpoint(const std::string& endpoint, int limit = 100, int skip = 0);
    Result<std::vector<ApiRequestLog>> getApiRequestLogsByIp(const std::string& ipAddress, int limit = 100, int skip = 0);
    
    // Connection management
    Result<bool> testConnection();
    Result<bool> ensureIndexes();

    // Frontier (durable queue) operations stored in collection 'frontier_tasks'
    Result<bool> frontierUpsertTask(const std::string& sessionId,
                                    const std::string& url,
                                    const std::string& normalizedUrl,
                                    const std::string& domain,
                                    int depth,
                                    int priority,
                                    const std::string& status,
                                    const std::chrono::system_clock::time_point& readyAt,
                                    int retryCount);

    Result<bool> frontierMarkCompleted(const std::string& sessionId,
                                       const std::string& normalizedUrl);

    Result<bool> frontierUpdateRetry(const std::string& sessionId,
                                     const std::string& normalizedUrl,
                                     int retryCount,
                                     const std::chrono::system_clock::time_point& nextReadyAt);

    Result<std::vector<std::pair<std::string,int>>> frontierLoadPending(const std::string& sessionId,
                                                                        size_t limit = 2000);
};

} // namespace storage
} // namespace search_engine 