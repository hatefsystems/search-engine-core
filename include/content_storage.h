#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <memory>
#include <unordered_map>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

#include <sw/redis++/redis++.hpp>

#include "infrastructure.h"
#include "../src/crawler/models/CrawlResult.h"

// Forward declarations
namespace sw {
namespace redis {
class Redis;
}
}

/**
 * @brief Class responsible for storing and retrieving website profiles in MongoDB
 */
class WebsiteProfileStorage {
public:
    /**
     * @brief Constructor
     * @param uri MongoDB connection URI
     * @param dbName Database name
     * @param collectionName Collection name for website profiles
     */
    WebsiteProfileStorage(const std::string& uri, 
                         const std::string& dbName = "search-engine", 
                         const std::string& collectionName = "website-profiles");
    
    /**
     * @brief Store a website profile from crawl result
     * @param result The crawl result containing website data
     * @return Result indicating success or failure
     */
    Result<bool> storeWebsiteProfile(const CrawlResult& result);
    
    /**
     * @brief Retrieve a website profile by URL
     * @param url The URL of the website
     * @return Result containing the website profile as a JSON string
     */
    Result<std::string> getWebsiteProfile(const std::string& url);
    
    /**
     * @brief Update a website profile
     * @param url The URL of the website to update
     * @param updateJson JSON string containing fields to update
     * @return Result indicating success or failure
     */
    Result<bool> updateWebsiteProfile(const std::string& url, const std::string& updateJson);
    
    /**
     * @brief Delete a website profile
     * @param url The URL of the website to delete
     * @return Result indicating success or failure
     */
    Result<bool> deleteWebsiteProfile(const std::string& url);

private:
    mongocxx::uri _uri;
    mongocxx::client _client;
    std::string _dbName;
    std::string _collectionName;
};

/**
 * @brief Class responsible for storing and searching content in RedisSearch
 */
class ContentSearchStorage {
public:
    /**
     * @brief Constructor
     * @param uri Redis connection URI
     * @param indexName Name of the search index
     */
    ContentSearchStorage(const std::string& uri, const std::string& indexName = "page-content");
    
    /**
     * @brief Initialize the search index if it doesn't exist
     * @return Result indicating success or failure
     */
    Result<bool> initializeIndex();
    
    /**
     * @brief Store page content for full-text search
     * @param result The crawl result containing page content
     * @return Result indicating success or failure
     */
    Result<bool> storePageContent(const CrawlResult& result);
    
    /**
     * @brief Search for content using keywords
     * @param query The search query
     * @param limit Maximum number of results to return
     * @return Result containing search results as a JSON string
     */
    Result<std::string> search(const std::string& query, int limit = 10);
    
    /**
     * @brief Delete page content by URL
     * @param url The URL of the page to delete
     * @return Result indicating success or failure
     */
    Result<bool> deletePageContent(const std::string& url);

private:
    std::unique_ptr<sw::redis::Redis> _redis;
    std::string _indexName;
    
    /**
     * @brief Create a document ID from URL
     * @param url The URL to convert to document ID
     * @return Document ID string
     */
    std::string createDocId(const std::string& url);
};

/**
 * @brief Facade class for content storage operations
 */
class ContentStorage {
public:
    /**
     * @brief Constructor
     * @param mongoUri MongoDB connection URI
     * @param redisUri Redis connection URI
     */
    ContentStorage(const std::string& mongoUri = "mongodb://localhost:27017", 
                  const std::string& redisUri = "redis://localhost:6379");
    
    /**
     * @brief Store crawl result in both MongoDB and RedisSearch
     * @param result The crawl result to store
     * @return Result indicating success or failure
     */
    Result<bool> storeCrawlResult(const CrawlResult& result);
    
    /**
     * @brief Search for content using keywords
     * @param query The search query
     * @param limit Maximum number of results to return
     * @return Result containing search results as a JSON string
     */
    Result<std::string> search(const std::string& query, int limit = 10);
    
    /**
     * @brief Get website profile by URL
     * @param url The URL of the website
     * @return Result containing the website profile as a JSON string
     */
    Result<std::string> getWebsiteProfile(const std::string& url);
    
    /**
     * @brief Delete content and profile by URL
     * @param url The URL to delete
     * @return Result indicating success or failure
     */
    Result<bool> deleteContent(const std::string& url);

private:
    WebsiteProfileStorage _profileStorage;
    ContentSearchStorage _contentStorage;
};