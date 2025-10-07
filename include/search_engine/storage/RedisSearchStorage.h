#pragma once

#include "IndexedPage.h"
#include "../../infrastructure.h"
#include <sw/redis++/redis++.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace search_engine {
namespace storage {

struct SearchDocument {
    std::string url;
    std::string title;
    std::string content;
    std::string domain;
    std::vector<std::string> keywords;
    std::optional<std::string> description;
    std::optional<std::string> language;
    std::optional<std::string> category;
    std::chrono::system_clock::time_point indexedAt;
    double score;  // For ranking/relevance scoring
};

struct SearchQuery {
    std::string query;
    std::vector<std::string> filters;  // e.g., "domain:example.com"
    std::optional<std::string> language;
    std::optional<std::string> category;
    int limit = 10;
    int offset = 0;
    bool highlight = false;
};

struct SearchResult {
    std::string url;
    std::string title;
    std::string snippet;
    std::string domain;
    double score;
    std::unordered_map<std::string, std::string> highlights;  // field -> highlighted_text
};

struct SearchResponse {
    std::vector<SearchResult> results;
    int totalResults;
    double queryTime;
    std::string indexName;
};

class RedisSearchStorage {
private:
    std::unique_ptr<sw::redis::Redis> redis_;
    std::string indexName_;
    std::string keyPrefix_;
    
    // Index management
    bool indexExists() const;
    Result<bool> createSearchIndex();
    
    // Document key generation
    std::string generateDocumentKey(const std::string& url) const;
    
    // Helper methods for Redis commands
    std::vector<std::string> buildSearchCommand(const SearchQuery& query) const;
    SearchResult parseSearchResult(const std::vector<std::string>& result) const;
    
public:
    // Constructor
    explicit RedisSearchStorage(
        const std::string& connectionString = "tcp://127.0.0.1:6379",
        const std::string& indexName = "search_index",
        const std::string& keyPrefix = "doc:"
    );
    
    // Destructor
    ~RedisSearchStorage() = default;
    
    // Move semantics (disable copy)
    RedisSearchStorage(RedisSearchStorage&& other) noexcept = default;
    RedisSearchStorage& operator=(RedisSearchStorage&& other) noexcept = default;
    RedisSearchStorage(const RedisSearchStorage&) = delete;
    RedisSearchStorage& operator=(const RedisSearchStorage&) = delete;
    
    // Document indexing operations
    Result<bool> indexDocument(const SearchDocument& document);
    Result<bool> indexSiteProfile(const IndexedPage& page, const std::string& content);
    Result<bool> updateDocument(const SearchDocument& document);
    Result<bool> deleteDocument(const std::string& url);
    
    // Batch operations
    Result<std::vector<bool>> indexDocuments(const std::vector<SearchDocument>& documents);
    Result<bool> deleteDocumentsByDomain(const std::string& domain);
    
    // Search operations
    Result<SearchResponse> search(const SearchQuery& query);
    Result<SearchResponse> searchSimple(const std::string& query, int limit = 10);
    Result<std::vector<std::string>> suggest(const std::string& prefix, int limit = 5);
    
    // Index management
    Result<bool> initializeIndex();
    Result<bool> dropIndex();
    Result<bool> reindexAll();
    Result<int64_t> getDocumentCount();
    
    // Index statistics and maintenance
    Result<std::unordered_map<std::string, std::string>> getIndexInfo();
    Result<bool> optimizeIndex();
    
    // Connection management
    Result<bool> testConnection();
    Result<bool> ping();
    
    // Utility methods
    static SearchDocument siteProfileToSearchDocument(
        const IndexedPage& page, 
        const std::string& content
    );
};

} // namespace storage
} // namespace search_engine 