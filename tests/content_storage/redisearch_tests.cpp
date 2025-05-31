#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <chrono>
#include <thread>
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

#include "../../include/content_storage.h"
#include "../../src/crawler/models/CrawlResult.h"

using json = nlohmann::json;

// Helper function to create a test CrawlResult
CrawlResult createTestCrawlResult(const std::string& url, const std::string& title, 
                                 const std::string& description, const std::string& content) {
    CrawlResult result;
    result.url = url;
    result.statusCode = 200;
    result.contentType = "text/html";
    result.title = title;
    result.metaDescription = description;
    result.textContent = content;
    result.links = {"https://example.com/page1", "https://example.com/page2"};
    result.crawlTime = std::chrono::system_clock::now();
    result.contentSize = 1024;
    result.success = true;
    
    return result;
}

// Test fixture for RedisSearch tests
class RedisSearchTestFixture {
protected:
    ContentSearchStorage storage;
    std::string testIndexName;
    std::vector<std::string> testUrls;
    
public:
    RedisSearchTestFixture() 
        : storage("redis://localhost:6379", "test-page-content"),
          testIndexName("test-page-content") {
        // Initialize test URLs
        testUrls = {
            "https://test-example.com/page1",
            "https://test-example.com/page2",
            "https://test-example.com/page3"
        };
        
        // Clean up any existing test data
        for (const auto& url : testUrls) {
            storage.deletePageContent(url);
        }
        
        // Initialize the index
        storage.initializeIndex();
    }
    
    ~RedisSearchTestFixture() {
        // Clean up test data
        for (const auto& url : testUrls) {
            storage.deletePageContent(url);
        }
    }
};

TEST_CASE_METHOD(RedisSearchTestFixture, "RedisSearch Content Storage", "[redisearch]") {
    SECTION("Store and search page content") {
        // Create and store test data
        auto result1 = createTestCrawlResult(
            testUrls[0], 
            "Python Programming Guide", 
            "Learn Python programming from scratch", 
            "Python is a popular programming language. It is used for web development, data analysis, and artificial intelligence."
        );
        
        auto result2 = createTestCrawlResult(
            testUrls[1], 
            "JavaScript Basics", 
            "Introduction to JavaScript programming", 
            "JavaScript is the language of the web. It powers interactive websites and modern web applications."
        );
        
        auto result3 = createTestCrawlResult(
            testUrls[2], 
            "C++ Advanced Topics", 
            "Deep dive into C++ programming", 
            "C++ is a powerful language used for system programming, game development, and performance-critical applications."
        );
        
        // Store the content
        REQUIRE(storage.storePageContent(result1).success);
        REQUIRE(storage.storePageContent(result2).success);
        REQUIRE(storage.storePageContent(result3).success);
        
        // Wait a moment for indexing to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Search for Python
        auto pythonResult = storage.search("Python");
        REQUIRE(pythonResult.success);
        
        auto pythonJson = json::parse(pythonResult.value);
        REQUIRE(pythonJson["total"].get<int>() > 0);
        
        bool foundPython = false;
        for (const auto& result : pythonJson["results"]) {
            if (result.contains("url") && result["url"] == testUrls[0]) {
                foundPython = true;
                break;
            }
        }
        REQUIRE(foundPython);
        
        // Search for JavaScript
        auto jsResult = storage.search("JavaScript");
        REQUIRE(jsResult.success);
        
        auto jsJson = json::parse(jsResult.value);
        REQUIRE(jsJson["total"].get<int>() > 0);
        
        bool foundJS = false;
        for (const auto& result : jsJson["results"]) {
            if (result.contains("url") && result["url"] == testUrls[1]) {
                foundJS = true;
                break;
            }
        }
        REQUIRE(foundJS);
        
        // Search for programming (should find all)
        auto programmingResult = storage.search("programming");
        REQUIRE(programmingResult.success);
        
        auto programmingJson = json::parse(programmingResult.value);
        REQUIRE(programmingJson["total"].get<int>() >= 3);
    }
    
    SECTION("Delete page content") {
        // Create and store test data
        auto result = createTestCrawlResult(
            testUrls[0], 
            "Test Title", 
            "Test Description", 
            "Test Content with unique identifier xyzabc123"
        );
        
        REQUIRE(storage.storePageContent(result).success);
        
        // Wait a moment for indexing to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Search for the unique identifier
        auto searchResult = storage.search("xyzabc123");
        REQUIRE(searchResult.success);
        
        auto searchJson = json::parse(searchResult.value);
        REQUIRE(searchJson["total"].get<int>() > 0);
        
        // Delete the content
        REQUIRE(storage.deletePageContent(testUrls[0]).success);
        
        // Wait a moment for deletion to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Search again - should not find it
        auto afterDeleteResult = storage.search("xyzabc123");
        auto afterDeleteJson = json::parse(afterDeleteResult.value);
        REQUIRE(afterDeleteJson["total"].get<int>() == 0);
    }
}

TEST_CASE("ContentStorage Facade", "[content-storage]") {
    // Create the facade with test database names
    ContentStorage storage("mongodb://localhost:27017", "redis://localhost:6379");
    
    // Create test data
    std::string testUrl = "https://test-facade-example.com/test-page";
    auto testResult = createTestCrawlResult(
        testUrl,
        "Facade Test Title",
        "Facade Test Description",
        "This is test content for the ContentStorage facade class."
    );
    
    // Clean up any existing test data
    storage.deleteContent(testUrl);
    
    SECTION("Store and retrieve content") {
        // Store the content
        auto storeResult = storage.storeCrawlResult(testResult);
        REQUIRE(storeResult.success);
        
        // Get the website profile
        auto profileResult = storage.getWebsiteProfile(testUrl);
        REQUIRE(profileResult.success);
        
        // Parse the JSON result
        auto profileJson = json::parse(profileResult.value);
        REQUIRE(profileJson["url"] == testUrl);
        REQUIRE(profileJson["title"] == "Facade Test Title");
        
        // Search for the content
        auto searchResult = storage.search("facade");
        REQUIRE(searchResult.success);
        
        auto searchJson = json::parse(searchResult.value);
        bool found = false;
        for (const auto& result : searchJson["results"]) {
            if (result.contains("url") && result["url"] == testUrl) {
                found = true;
                break;
            }
        }
        REQUIRE(found);
        
        // Clean up
        storage.deleteContent(testUrl);
    }
}