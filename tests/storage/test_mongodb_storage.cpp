#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "../../include/search_engine/storage/MongoDBStorage.h"
#include <chrono>
#include <thread>

using namespace search_engine::storage;

// Test data helpers
namespace {
    IndexedPage createTestSiteProfile(const std::string& url = "https://example.com") {
        IndexedPage page;
        page.domain = "example.com";
        page.url = url;
        page.title = "Test Site";
        page.description = "A test website for unit testing";
        page.keywords = {"test", "example", "website"};
        page.language = "en";
        page.category = "technology";
        
        // Crawl metadata
        auto now = std::chrono::system_clock::now();
        page.crawlMetadata.lastCrawlTime = now;
        page.crawlMetadata.firstCrawlTime = now;
        page.crawlMetadata.lastCrawlStatus = CrawlStatus::SUCCESS;
        page.crawlMetadata.crawlCount = 1;
        page.crawlMetadata.crawlIntervalHours = 24.0;
        page.crawlMetadata.userAgent = "TestBot/1.0";
        page.crawlMetadata.httpStatusCode = 200;
        page.crawlMetadata.contentSize = 5000;
        page.crawlMetadata.contentType = "text/html";
        page.crawlMetadata.crawlDurationMs = 250.5;
        
        // SEO metrics
        page.pageRank = 5;
        page.contentQuality = 0.8;
        page.wordCount = 500;
        page.isMobile = true;
        page.hasSSL = true;
        
        // Links
        page.outboundLinks = {"https://example.org", "https://test.com"};
        page.inboundLinkCount = 10;
        
        // Search relevance
        page.isIndexed = true;
        page.lastModified = now;
        page.indexedAt = now;
        
        // Additional metadata
        page.author = "John Doe";
        page.publisher = "Example Corp";
        page.publishDate = now - std::chrono::hours(24);
        
        return page;
    }
}

TEST_CASE("MongoDB Storage - Connection and Initialization", "[mongodb][storage]") {
    SECTION("Constructor with default parameters") {
        REQUIRE_NOTHROW(MongoDBStorage());
    }
    
    SECTION("Constructor with custom parameters") {
        REQUIRE_NOTHROW(MongoDBStorage("mongodb://localhost:27017", "test-db"));
    }
    
    SECTION("Test connection") {
        MongoDBStorage storage("mongodb://localhost:27017", "test-search-engine");
        auto result = storage.testConnection();
        
        if (result.success) {
            REQUIRE(result.value == true);
            REQUIRE(result.message == "MongoDB connection is healthy");
        } else {
            // If MongoDB is not available, skip the rest of the tests
            WARN("MongoDB not available: " + result.message);
            return;
        }
    }
}

TEST_CASE("MongoDB Storage - indexed page CRUD Operations", "[mongodb][storage][crud]") {
    MongoDBStorage storage("mongodb://localhost:27017", "test-search-engine");
    
    // Skip tests if MongoDB is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping MongoDB tests - MongoDB not available");
        return;
    }
    
    SECTION("Store and retrieve indexed page") {
        IndexedPage testProfile = createTestSiteProfile("https://hatef.ir");
        
        // Store the page
        auto storeResult = storage.storeIndexedPage(testProfile);
        REQUIRE(storeResult.success);
        REQUIRE(!storeResult.value.empty());
        
        std::string profileId = storeResult.value;
        
        // Retrieve by URL
        auto retrieveResult = storage.getSiteProfile("https://hatef.ir");
        REQUIRE(retrieveResult.success);
        
        IndexedPage retrieved = retrieveResult.value;
        REQUIRE(retrieved.url == testProfile.url);
        REQUIRE(retrieved.domain == testProfile.domain);
        REQUIRE(retrieved.title == testProfile.title);
        REQUIRE(retrieved.description == testProfile.description);
        REQUIRE(retrieved.keywords == testProfile.keywords);
        REQUIRE(retrieved.language == testProfile.language);
        REQUIRE(retrieved.category == testProfile.category);
        
        // Retrieve by ID
        auto retrieveByIdResult = storage.getSiteProfileById(profileId);
        REQUIRE(retrieveByIdResult.success);
        REQUIRE(retrieveByIdResult.value.url == testProfile.url);
        
        // Clean up
        storage.deleteSiteProfile("https://hatef.ir");
    }
    
    SECTION("Update indexed page") {
        IndexedPage testProfile = createTestSiteProfile("https://hatef.ir");
        
        // Store the page
        auto storeResult = storage.storeIndexedPage(testProfile);
        REQUIRE(storeResult.success);
        
        // Retrieve and modify
        auto retrieveResult = storage.getSiteProfile("https://hatef.ir");
        REQUIRE(retrieveResult.success);
        
        IndexedPage retrieved = retrieveResult.value;
        retrieved.title = "Updated Title";
        retrieved.crawlMetadata.crawlCount = 2;
        retrieved.contentQuality = 0.9;
        
        // Update
        auto updateResult = storage.storeIndexedPage(retrieved);
        REQUIRE(updateResult.success);
        
        // Retrieve again and verify changes
        auto verifyResult = storage.getSiteProfile("https://hatef.ir");
        REQUIRE(verifyResult.success);
        
        IndexedPage verified = verifyResult.value;
        REQUIRE(verified.title == "Updated Title");
        REQUIRE(verified.crawlMetadata.crawlCount == 2);
        REQUIRE(verified.contentQuality == 0.9);
        
        // Clean up
        storage.deleteSiteProfile("https://hatef.ir");
    }
    
    SECTION("Delete indexed page") {
        IndexedPage testProfile = createTestSiteProfile("https://test-delete.com");
        
        // Store the page
        auto storeResult = storage.storeIndexedPage(testProfile);
        REQUIRE(storeResult.success);
        
        // Verify it exists
        auto retrieveResult = storage.getSiteProfile("https://test-delete.com");
        REQUIRE(retrieveResult.success);
        
        // Delete
        auto deleteResult = storage.deleteSiteProfile("https://test-delete.com");
        REQUIRE(deleteResult.success);
        
        // Verify it's gone
        auto verifyResult = storage.getSiteProfile("https://test-delete.com");
        REQUIRE(!verifyResult.success);
    }
    
    SECTION("Non-existent page retrieval") {
        auto result = storage.getSiteProfile("https://non-existent.com");
        REQUIRE(!result.success);
        REQUIRE(result.message.find("not found") != std::string::npos);
    }
} 