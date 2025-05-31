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
CrawlResult createTestCrawlResult(const std::string& url, bool success = true) {
    CrawlResult result;
    result.url = url;
    result.statusCode = 200;
    result.contentType = "text/html";
    result.title = "Test Page Title";
    result.metaDescription = "This is a test page description for unit testing";
    result.textContent = "This is the main content of the test page. It contains searchable text.";
    result.links = {"https://example.com/page1", "https://example.com/page2"};
    result.crawlTime = std::chrono::system_clock::now();
    result.contentSize = 1024;
    result.success = success;
    
    return result;
}

// Test fixture for MongoDB tests
class MongoDBTestFixture {
protected:
    WebsiteProfileStorage storage;
    std::string testUrl;
    
public:
    MongoDBTestFixture() 
        : storage("mongodb://localhost:27017", "search-engine-test", "website-profiles-test"),
          testUrl("https://test-example.com/test-page") {
        // Clean up any existing test data
        storage.deleteWebsiteProfile(testUrl);
    }
    
    ~MongoDBTestFixture() {
        // Clean up test data
        storage.deleteWebsiteProfile(testUrl);
    }
};

TEST_CASE_METHOD(MongoDBTestFixture, "MongoDB Website Profile Storage", "[mongodb]") {
    SECTION("Store and retrieve website profile") {
        // Create test data
        auto testResult = createTestCrawlResult(testUrl);
        
        // Store the profile
        auto storeResult = storage.storeWebsiteProfile(testResult);
        REQUIRE(storeResult.success);
        
        // Retrieve the profile
        auto getResult = storage.getWebsiteProfile(testUrl);
        REQUIRE(getResult.success);
        
        // Parse the JSON result
        auto profileJson = json::parse(getResult.value);
        
        // Verify the retrieved data
        REQUIRE(profileJson["url"] == testUrl);
        REQUIRE(profileJson["statusCode"] == 200);
        REQUIRE(profileJson["contentType"] == "text/html");
        REQUIRE(profileJson["title"] == "Test Page Title");
        REQUIRE(profileJson["metaDescription"] == "This is a test page description for unit testing");
        
        // Verify links array
        REQUIRE(profileJson["links"].is_array());
        REQUIRE(profileJson["links"].size() == 2);
        REQUIRE(profileJson["links"][0] == "https://example.com/page1");
        REQUIRE(profileJson["links"][1] == "https://example.com/page2");
    }
    
    SECTION("Update website profile") {
        // Create and store initial test data
        auto testResult = createTestCrawlResult(testUrl);
        auto storeResult = storage.storeWebsiteProfile(testResult);
        REQUIRE(storeResult.success);
        
        // Update the profile with new data
        std::string updateJson = R"({
            "title": "Updated Title",
            "metaDescription": "Updated description"
        })";
        
        auto updateResult = storage.updateWebsiteProfile(testUrl, updateJson);
        REQUIRE(updateResult.success);
        
        // Retrieve the updated profile
        auto getResult = storage.getWebsiteProfile(testUrl);
        REQUIRE(getResult.success);
        
        // Parse the JSON result
        auto profileJson = json::parse(getResult.value);
        
        // Verify the updated data
        REQUIRE(profileJson["url"] == testUrl);
        REQUIRE(profileJson["title"] == "Updated Title");
        REQUIRE(profileJson["metaDescription"] == "Updated description");
    }
    
    SECTION("Delete website profile") {
        // Create and store test data
        auto testResult = createTestCrawlResult(testUrl);
        auto storeResult = storage.storeWebsiteProfile(testResult);
        REQUIRE(storeResult.success);
        
        // Delete the profile
        auto deleteResult = storage.deleteWebsiteProfile(testUrl);
        REQUIRE(deleteResult.success);
        
        // Try to retrieve the deleted profile
        auto getResult = storage.getWebsiteProfile(testUrl);
        REQUIRE_FALSE(getResult.success);
    }
}