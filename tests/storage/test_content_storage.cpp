#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "../../include/search_engine/storage/ContentStorage.h"
#include "../../include/search_engine/crawler/models/CrawlResult.h"
#include "../../include/Logger.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <cstdlib>

using namespace search_engine::storage;

// Test data helpers
namespace {
    CrawlResult createTestCrawlResult(const std::string& url = "https://example.com") {
        CrawlResult result;
        result.url = url;
        result.statusCode = 200;
        result.contentType = "text/html";
        result.rawContent = "<html><head><title>Test Page</title></head><body><h1>Test Content</h1><p>This is test content for the search engine.</p></body></html>";
        result.textContent = "Test Page Test Content This is test content for the search engine.";
        result.title = "Test Page";
        result.metaDescription = "A test page for unit testing";
        result.links = {"https://example.org", "https://test.com"};
        result.crawlTime = std::chrono::system_clock::now();
        result.contentSize = result.rawContent->length();
        result.success = true;
        
        return result;
    }
}

TEST_CASE("Content Storage - Initialization and Connection", "[content][storage]") {
    SECTION("Constructor with default parameters") {
        REQUIRE_NOTHROW(ContentStorage{});
    }
    
    SECTION("Constructor with custom parameters") {
        REQUIRE_NOTHROW(ContentStorage{
            "mongodb://localhost:27017",
            "test-search-engine",
            "tcp://127.0.0.1:6379",
            "test_content_index"
        });
    }
    
    SECTION("Test connections") {
        ContentStorage storage(
            "mongodb://localhost:27017",
            "test-search-engine",
            "tcp://127.0.0.1:6379",
            "test_content_index"
        );
        
        auto result = storage.testConnections();
        if (result.success) {
            REQUIRE(result.value == true);
            REQUIRE(result.message == "All connections are healthy");
        } else {
            WARN("Storage connections not available: " + result.message);
            return;
        }
    }
}

TEST_CASE("Content Storage - Crawl Result Processing", "[content][storage][crawl]") {
    ContentStorage storage(
        "mongodb://localhost:27017",
        "test-search-engine",
        "tcp://127.0.0.1:6379",
        "test_content_index"
    );
    
    // Skip tests if connections are not available
    auto connectionTest = storage.testConnections();
    if (!connectionTest.success) {
        WARN("Skipping content storage tests - connections not available");
        return;
    }
    
    SECTION("Store single crawl result") {
        CrawlResult testResult = createTestCrawlResult("https://test-content.com");
        
        // Store the crawl result
        auto storeResult = storage.storeCrawlResult(testResult);
        REQUIRE(storeResult.success);
        REQUIRE(!storeResult.value.empty());
        
        std::string profileId = storeResult.value;
        
        // Retrieve the indexed page
        auto profileResult = storage.getSiteProfile("https://test-content.com");
        REQUIRE(profileResult.success);
        
        IndexedPage page = profileResult.value;
        REQUIRE(page.url == testResult.url);
        REQUIRE(page.title == testResult.title.value_or(""));
        REQUIRE(page.description == testResult.metaDescription);
        REQUIRE(page.outboundLinks == testResult.links);
        REQUIRE(page.crawlMetadata.httpStatusCode == testResult.statusCode);
        REQUIRE(page.crawlMetadata.contentSize == testResult.contentSize);
        REQUIRE(page.crawlMetadata.lastCrawlStatus == CrawlStatus::SUCCESS);
        
        // Test search functionality
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        auto searchResult = storage.searchSimple("test content", 10);
        REQUIRE(searchResult.success);
        
        auto response = searchResult.value;
        bool found = false;
        for (const auto& result : response.results) {
            if (result.url == "https://test-content.com") {
                found = true;
                break;
            }
        }
        REQUIRE(found);
        
        // Clean up
        storage.deleteSiteData("https://test-content.com");
    }
    
    SECTION("Update existing crawl result") {
        CrawlResult initialResult = createTestCrawlResult("https://test-update-content.com");
        
        // Store initial result
        auto storeResult = storage.storeCrawlResult(initialResult);
        REQUIRE(storeResult.success);
        
        // Create updated result
        CrawlResult updatedResult = initialResult;
        updatedResult.title = "Updated Test Page";
        updatedResult.textContent = "Updated test content for the search engine.";
        updatedResult.crawlTime = std::chrono::system_clock::now();
        
        // Update the result
        auto updateResult = storage.updateCrawlResult(updatedResult);
        REQUIRE(updateResult.success);
        
        // Verify the update
        auto profileResult = storage.getSiteProfile("https://test-update-content.com");
        REQUIRE(profileResult.success);
        
        IndexedPage page = profileResult.value;
        REQUIRE(page.title == "Updated Test Page");
        REQUIRE(page.crawlMetadata.crawlCount == 2); // Should be incremented
        
        // Clean up
        storage.deleteSiteData("https://test-update-content.com");
    }
    
    SECTION("Store failed crawl result") {
        CrawlResult failedResult = createTestCrawlResult("https://test-failed.com");
        failedResult.success = false;
        failedResult.statusCode = 404;
        failedResult.errorMessage = "Page not found";
        failedResult.textContent = std::nullopt;
        failedResult.rawContent = std::nullopt;
        
        // Store the failed result
        auto storeResult = storage.storeCrawlResult(failedResult);
        REQUIRE(storeResult.success);
        
        // Verify the page
        auto profileResult = storage.getSiteProfile("https://test-failed.com");
        REQUIRE(profileResult.success);
        
        IndexedPage page = profileResult.value;
        REQUIRE(page.crawlMetadata.lastCrawlStatus == CrawlStatus::FAILED);
        REQUIRE(page.crawlMetadata.lastErrorMessage == "Page not found");
        REQUIRE(page.crawlMetadata.httpStatusCode == 404);
        REQUIRE(!page.isIndexed);
        
        // Clean up
        storage.deleteSiteData("https://test-failed.com");
    }
}

TEST_CASE("Content Storage - Batch Operations", "[content][storage][batch]") {
    ContentStorage storage(
        "mongodb://localhost:27017",
        "test-search-engine",
        "tcp://127.0.0.1:6379",
        "test_batch_index"
    );
    
    // Skip tests if connections are not available
    auto connectionTest = storage.testConnections();
    if (!connectionTest.success) {
        WARN("Skipping content storage tests - connections not available");
        return;
    }
    
    SECTION("Store multiple crawl results") {
        std::vector<CrawlResult> crawlResults;
        for (int i = 0; i < 3; ++i) {
            CrawlResult result = createTestCrawlResult("https://batch" + std::to_string(i) + ".com");
            result.title = "Batch Test Page " + std::to_string(i);
            result.textContent = "Batch test content number " + std::to_string(i) + " for testing.";
            crawlResults.push_back(result);
        }
        
        // Store all results
        auto batchResult = storage.storeCrawlResults(crawlResults);
        REQUIRE(batchResult.success);
        
        auto ids = batchResult.value;
        REQUIRE(ids.size() == 3);
        
        // Verify all were stored
        for (int i = 0; i < 3; ++i) {
            auto profileResult = storage.getSiteProfile("https://batch" + std::to_string(i) + ".com");
            REQUIRE(profileResult.success);
            
            IndexedPage page = profileResult.value;
            REQUIRE(page.title == "Batch Test Page " + std::to_string(i));
        }
        
        // Test search across all documents
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        auto searchResult = storage.searchSimple("batch test", 10);
        REQUIRE(searchResult.success);
        REQUIRE(searchResult.value.results.size() >= 3);
        
        // Clean up
        for (int i = 0; i < 3; ++i) {
            storage.deleteSiteData("https://batch" + std::to_string(i) + ".com");
        }
    }
}

TEST_CASE("Content Storage - Domain Operations", "[content][storage][domain]") {
    ContentStorage storage(
        "mongodb://localhost:27017",
        "test-search-engine",
        "tcp://127.0.0.1:6379",
        "test_domain_index"
    );
    
    // Skip tests if connections are not available
    auto connectionTest = storage.testConnections();
    if (!connectionTest.success) {
        WARN("Skipping content storage tests - connections not available");
        return;
    }
    
    SECTION("Get profiles by domain") {
        // Create multiple pages for the same domain
        std::vector<CrawlResult> domainResults;
        for (int i = 0; i < 3; ++i) {
            CrawlResult result = createTestCrawlResult("https://testdomain.com/page" + std::to_string(i));
            result.title = "Domain Page " + std::to_string(i);
            domainResults.push_back(result);
        }
        
        // Store all results
        for (const auto& result : domainResults) {
            auto storeResult = storage.storeCrawlResult(result);
            REQUIRE(storeResult.success);
        }
        
        // Get profiles by domain
        auto domainProfilesResult = storage.getSiteProfilesByDomain("testdomain.com");
        REQUIRE(domainProfilesResult.success);
        REQUIRE(domainProfilesResult.value.size() >= 3);
        
        // Clean up domain data
        auto deleteResult = storage.deleteDomainData("testdomain.com");
        REQUIRE(deleteResult.success);
        
        // Verify deletion
        auto verifyResult = storage.getSiteProfilesByDomain("testdomain.com");
        REQUIRE(verifyResult.success);
        REQUIRE(verifyResult.value.empty());
    }
}

TEST_CASE("Content Storage - Search Operations", "[content][storage][search]") {
    ContentStorage storage(
        "mongodb://localhost:27017",
        "test-search-engine",
        "tcp://127.0.0.1:6379",
        "test_search_index"
    );
    
    // Skip tests if connections are not available
    auto connectionTest = storage.testConnections();
    if (!connectionTest.success) {
        WARN("Skipping content storage tests - connections not available");
        return;
    }
    
    SECTION("Advanced search with filters") {
        // Create documents with different categories
        CrawlResult techResult = createTestCrawlResult("https://tech-search.com");
        techResult.title = "Technology News";
        techResult.textContent = "Latest technology trends and innovations.";
        
        CrawlResult newsResult = createTestCrawlResult("https://news-search.com");
        newsResult.title = "Breaking News";
        newsResult.textContent = "Latest news updates from around the world.";
        
        // Store both results
        storage.storeCrawlResult(techResult);
        storage.storeCrawlResult(newsResult);
        
        // Give time for indexing
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // Test simple search
        auto simpleResult = storage.searchSimple("technology", 10);
        REQUIRE(simpleResult.success);
        REQUIRE(simpleResult.value.results.size() >= 1);
        
        // Test advanced search with query object
        SearchQuery query;
        query.query = "news";
        query.limit = 5;
        query.highlight = true;
        
        auto advancedResult = storage.search(query);
        REQUIRE(advancedResult.success);
        REQUIRE(advancedResult.value.results.size() >= 1);
        
        // Clean up
        storage.deleteSiteData("https://tech-search.com");
        storage.deleteSiteData("https://news-search.com");
    }
    
    SECTION("Search suggestions") {
        // Create a document with specific terms
        CrawlResult suggestionResult = createTestCrawlResult("https://suggestion-test.com");
        suggestionResult.title = "Suggestion Testing Document";
        suggestionResult.textContent = "This document is for testing search suggestions and autocomplete.";
        
        storage.storeCrawlResult(suggestionResult);
        
        // Give time for indexing
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Test suggestions (note: this might not work with all Redis configurations)
        auto suggestResult = storage.suggest("sugg", 5);
        // Suggestions might not be available in all Redis setups, so we just check it doesn't crash
        REQUIRE_NOTHROW(storage.suggest("test", 5));
        
        // Clean up
        storage.deleteSiteData("https://suggestion-test.com");
    }
}

TEST_CASE("Content Storage - Statistics and Monitoring", "[content][storage][stats]") {
    ContentStorage storage(
        "mongodb://localhost:27017",
        "test-search-engine",
        "tcp://127.0.0.1:6379",
        "test_stats_index"
    );
    
    // Skip tests if connections are not available
    auto connectionTest = storage.testConnections();
    if (!connectionTest.success) {
        WARN("Skipping content storage tests - connections not available");
        return;
    }
    
    SECTION("Get storage statistics") {
        std::cout << "DEBUG: Getting storage stats..." << std::endl;
        auto statsResult = storage.getStorageStats();
        std::cout << "DEBUG: Storage stats result success: " << (statsResult.success ? "true" : "false") << std::endl;
        REQUIRE(statsResult.success);
        
        std::cout << "DEBUG: Extracting stats value..." << std::endl;
        auto stats = statsResult.value;
        std::cout << "DEBUG: Stats empty check: " << (!stats.empty() ? "false" : "true") << std::endl;
        REQUIRE(!stats.empty());
        
        // Debug: Print all statistics
        std::cout << "\n=== DEBUG: All statistics ===" << std::endl;
        for (const auto& [key, value] : stats) {
            std::cout << "DEBUG: Stat entry - " << key << " = " << value << std::endl;
        }
        std::cout << "==========================\n" << std::endl;
        
        // Debug: Count Redis fields
        std::cout << "DEBUG: Starting Redis field count..." << std::endl;
        int redisFieldCount = 0;
        for (const auto& [key, value] : stats) {
            if (key.find("redis_") == 0) {
                redisFieldCount++;
                std::cout << "DEBUG: Found Redis field: " << key << std::endl;
            }
        }
        std::cout << "DEBUG: Total Redis fields found: " << redisFieldCount << std::endl;
        
        // Should have MongoDB stats
        std::cout << "DEBUG: Checking for MongoDB stats..." << std::endl;
        auto mongoStatsIt = stats.find("mongodb_total_documents");
        std::cout << "DEBUG: MongoDB stats found: " << (mongoStatsIt != stats.end() ? "true" : "false") << std::endl;
        REQUIRE(mongoStatsIt != stats.end());
        
        // Should have Redis stats (either successful count or error info)
        std::cout << "DEBUG: Checking for Redis stats..." << std::endl;
        auto redisStatsIt = stats.find("redis_indexed_documents");
        auto redisErrorIt = stats.find("redis_count_error");
        if (redisStatsIt == stats.end() && redisErrorIt == stats.end()) {
            std::cout << "DEBUG: Neither Redis stats nor Redis error found. Available keys:" << std::endl;
            for (const auto& [key, value] : stats) {
                std::cout << "DEBUG: Available stat - " << key << " = " << value << std::endl;
            }
            REQUIRE(false); // Fail if no Redis-related stats are found at all
        } else {
            if (redisStatsIt != stats.end()) {
                std::cout << "DEBUG: Redis stats found successfully: " << redisStatsIt->second << std::endl;
            } else {
                std::cout << "DEBUG: Redis error found: " << redisErrorIt->second << std::endl;
            }
        }
        // Accept either successful Redis stats or error information
        REQUIRE((redisStatsIt != stats.end() || redisErrorIt != stats.end()));
    }
    
    SECTION("Get total site count") {
        auto initialCount = storage.getTotalSiteCount();
        REQUIRE(initialCount.success);
        
        // Add a test site
        CrawlResult testResult = createTestCrawlResult("https://hatef.ir");
        auto storeResult = storage.storeCrawlResult(testResult);
        REQUIRE(storeResult.success);
        
        // Check count increased
        auto newCount = storage.getTotalSiteCount();
        REQUIRE(newCount.success);
        REQUIRE(newCount.value == initialCount.value + 1);
        
        // Clean up
        storage.deleteSiteData("https://hatef.ir");
    }
    
    SECTION("Get profiles by crawl status") {
        // Create profiles with different statuses
        CrawlResult successResult = createTestCrawlResult("https://hatef.ir");
        CrawlResult failedResult = createTestCrawlResult("https://hatef2.ir");
        failedResult.success = false;
        failedResult.statusCode = 500;
        failedResult.errorMessage = "Server error";
        
        storage.storeCrawlResult(successResult);
        storage.storeCrawlResult(failedResult);
        
        // Get profiles by status
        auto successProfiles = storage.getSiteProfilesByCrawlStatus(CrawlStatus::SUCCESS);
        auto failedProfiles = storage.getSiteProfilesByCrawlStatus(CrawlStatus::FAILED);
        
        REQUIRE(successProfiles.success);
        REQUIRE(failedProfiles.success);
        REQUIRE(successProfiles.value.size() >= 1);
        REQUIRE(failedProfiles.value.size() >= 1);
        
        // Clean up
        storage.deleteSiteData("https://hatef.ir");
        storage.deleteSiteData("https://hatef2.ir");
    }
}

TEST_CASE("Content Storage - Index Management", "[content][storage][index]") {
    ContentStorage storage(
        "mongodb://localhost:27017",
        "test-search-engine",
        "tcp://127.0.0.1:6379",
        "test_index_mgmt"
    );
    
    // Skip tests if connections are not available
    auto connectionTest = storage.testConnections();
    if (!connectionTest.success) {
        WARN("Skipping content storage tests - connections not available");
        return;
    }
    
    SECTION("Initialize indexes") {
        auto initResult = storage.initializeIndexes();
        REQUIRE(initResult.success);
    }
    
    SECTION("Test direct storage access") {
        // Test that we can get direct access to underlying storage layers
        auto mongoStorage = storage.getMongoStorage();
        auto redisStorage = storage.getRedisStorage();
        
        REQUIRE(mongoStorage != nullptr);
        REQUIRE(redisStorage != nullptr);
        
        // Test direct operations
        auto mongoTest = mongoStorage->testConnection();
        auto redisTest = redisStorage->testConnection();
        
        if (mongoTest.success) {
            REQUIRE(mongoTest.value == true);
        }
        if (redisTest.success) {
            REQUIRE(redisTest.value == true);
        }
    }
}

TEST_CASE("Content Storage - Error Handling", "[content][storage][errors]") {
    ContentStorage storage(
        "mongodb://localhost:27017",
        "test-search-engine",
        "tcp://127.0.0.1:6379",
        "test_error_index"
    );
    
    // Skip tests if connections are not available
    auto connectionTest = storage.testConnections();
    if (!connectionTest.success) {
        WARN("Skipping content storage tests - connections not available");
        return;
    }
    
    SECTION("Delete non-existent site data") {
        auto deleteResult = storage.deleteSiteData("https://non-existent-site.com");
        REQUIRE(!deleteResult.success);
    }
    
    SECTION("Get non-existent indexed page") {
        auto profileResult = storage.getSiteProfile("https://non-existent-page.com");
        REQUIRE(!profileResult.success);
    }
    
    SECTION("Get profiles for non-existent domain") {
        auto domainResult = storage.getSiteProfilesByDomain("non-existent-domain.com");
        REQUIRE(domainResult.success);
        REQUIRE(domainResult.value.empty());
    }
}

int main(int argc, char* argv[]) {
    // Set up the global logger before any tests run
    LogLevel logLevel = LogLevel::INFO;
    
    // Check for LOG_LEVEL environment variable
    std::string levelStr;
    const char* logLevelEnv = std::getenv("LOG_LEVEL");
    if (logLevelEnv) {
        levelStr = logLevelEnv;
    }
    
    if (!levelStr.empty()) {
        if (levelStr == "NONE") {
            logLevel = LogLevel::NONE;
        } else if (levelStr == "ERROR") {
            logLevel = LogLevel::ERR; 
        } else if (levelStr == "WARNING") {
            logLevel = LogLevel::WARNING;
        } else if (levelStr == "INFO") {
            logLevel = LogLevel::INFO;
        } else if (levelStr == "DEBUG") {
            logLevel = LogLevel::DEBUG;
        } else if (levelStr == "TRACE") {
            logLevel = LogLevel::TRACE;
        }
    }
    
    // Initialize the global logger
    std::string logFilePath = "./tests/logs/storage_test.log";
    
    Logger::getInstance().init(logLevel, true, logFilePath);
    
    
    // Run the tests
    return Catch::Session().run(argc, argv);
} 