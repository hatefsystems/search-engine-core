#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "../../include/search_engine/storage/RedisSearchStorage.h"
#include "../../include/Logger.h"
#include <chrono>
#include <thread>

using namespace search_engine::storage;

// Test data helpers
namespace {
    SearchDocument createTestSearchDocument(const std::string& url = "https://example.com") {
        SearchDocument doc;
        doc.url = url;
        doc.title = "Test Document";
        doc.content = "This is a test document with some sample content for testing full-text search capabilities.";
        doc.domain = "example.com";
        doc.keywords = {"test", "document", "search", "example"};
        doc.description = "A sample document for testing";
        doc.language = "en";
        doc.category = "test";
        doc.indexedAt = std::chrono::system_clock::now();
        doc.score = 0.8;
        
        return doc;
    }
    
    IndexedPage createTestSiteProfile(const std::string& url = "https://example.com") {
        IndexedPage page;
        page.domain = "example.com";
        page.url = url;
        page.title = "Test Site";
        page.description = "A test website for unit testing";
        page.keywords = {"test", "example", "website"};
        page.language = "en";
        page.category = "technology";
        
        // Set required timestamps
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
        
        page.isIndexed = true;
        page.lastModified = now;
        page.indexedAt = now;
        page.contentQuality = 0.8;
        
        return page;
    }
}

TEST_CASE("RedisSearch Storage - Connection and Initialization", "[redis][storage]") {
    SECTION("Constructor with default parameters") {
        REQUIRE_NOTHROW(RedisSearchStorage{});
    }
    
    SECTION("Constructor with custom parameters") {
        REQUIRE_NOTHROW(RedisSearchStorage{"tcp://127.0.0.1:6379", "test_index", "test:"});
    }
    
    SECTION("Test connection") {
        RedisSearchStorage storage("tcp://127.0.0.1:6379", "test_index");
        auto result = storage.testConnection();
        
        if (result.success) {
            REQUIRE(result.value == true);
            REQUIRE(result.message == "Redis connection is healthy");
        } else {
            // If Redis is not available, skip the rest of the tests
            WARN("Redis not available: " + result.message);
            return;
        }
    }
    
    SECTION("Initialize index") {
        RedisSearchStorage storage("tcp://127.0.0.1:6379", "test_init_index");
        auto connectionTest = storage.testConnection();
        
        if (!connectionTest.success) {
            WARN("Skipping Redis tests - Redis not available");
            return;
        }
        
        // Drop index first (ignore if fails)
        storage.dropIndex();
        
        auto result = storage.initializeIndex();
        REQUIRE(result.success);
        
        // Clean up
        storage.dropIndex();
    }
}

TEST_CASE("RedisSearch Storage - Document Indexing and Retrieval", "[redis][storage][indexing]") {
    RedisSearchStorage storage("tcp://127.0.0.1:6379", "test_doc_index");
    
    // Skip tests if Redis is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping Redis tests - Redis not available");
        return;
    }
    
    // Initialize clean index
    storage.dropIndex();
    auto initResult = storage.initializeIndex();
    REQUIRE(initResult.success);
    
    SECTION("Index and search single document") {
        SearchDocument testDoc = createTestSearchDocument("https://hatef.ir");
        LOG_DEBUG("Created test document for URL: https://hatef.ir");
        
        testDoc.title = "سند آزمایشی منحصر به فرد";
        testDoc.language = "fa";  // Set language to Persian
        testDoc.domain = "hatef.ir";
        LOG_DEBUG("Set document title: " + testDoc.title);
        
        testDoc.content = "این یک سند آزمایشی برای موتور جستجوی هاتف است که قابلیت‌های جستجوی متن کامل را آزمایش می‌کند.";
        LOG_DEBUG("Set document content length: " + std::to_string(testDoc.content.length()) + " characters");
        
        // Index the document
        auto indexResult = storage.indexDocument(testDoc);
        LOG_DEBUG("Indexing result - Success: " + std::string(indexResult.success ? "true" : "false"));
        REQUIRE(indexResult.success);
        
        // Give Redis a moment to process
        LOG_DEBUG("Waiting for Redis to process...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Search for the document
        auto searchResult = storage.searchSimple("موتور جستجوی هاتف", 10);
        LOG_DEBUG("Search result - Success: " + std::string(searchResult.success ? "true" : "false"));
        REQUIRE(searchResult.success);
        
        auto response = searchResult.value;
        LOG_DEBUG("Number of search results: " + std::to_string(response.results.size()));
        REQUIRE(response.results.size() >= 1);
        
        // Verify the result
        bool found = false;
        for (const auto& result : response.results) {
            LOG_DEBUG("Checking result URL: " + result.url);
            if (result.url == "https://hatef.ir") {
                LOG_DEBUG("Found matching URL");
                REQUIRE(result.title == "سند آزمایشی منحصر به فرد");
                LOG_DEBUG("Title verification passed");
                
                REQUIRE(!result.snippet.empty());
                LOG_DEBUG("Snippet verification passed");
                
                REQUIRE(result.domain == "hatef.ir");
                LOG_DEBUG("Domain verification passed");
                
                found = true;
                break;
            }
        }
        LOG_DEBUG("Document found in results: " + std::string(found ? "true" : "false"));
        REQUIRE(found);
        
        // Clean up
        storage.deleteDocument("https://hatef.ir");
        LOG_DEBUG("Deleted document from storage");
    }
    
    SECTION("Index indexed page") {
        IndexedPage testProfile = createTestSiteProfile("https://hatef.ir");
        testProfile.title = "Profile Test Site";
        
        std::string content = "This is the main content of the page test site with searchable text.";
        
        // Index the indexed page
        auto indexResult = storage.indexSiteProfile(testProfile, content);
        REQUIRE(indexResult.success);
        
        // Give Redis a moment to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Search for the page
        auto searchResult = storage.searchSimple("page test", 10);
        REQUIRE(searchResult.success);
        
        auto response = searchResult.value;
        REQUIRE(response.results.size() >= 1);
        
        // Clean up
        storage.deleteDocument("https://hatef.ir");
    }
    
    SECTION("Update document") {
        SearchDocument testDoc = createTestSearchDocument("https://hatef.ir");
        testDoc.title = "Original Title";
        
        // Index original document
        auto indexResult = storage.indexDocument(testDoc);
        REQUIRE(indexResult.success);
        
        // Update the document
        testDoc.title = "Updated Title";
        testDoc.content = "Updated content for testing document updates.";
        
        auto updateResult = storage.updateDocument(testDoc);
        REQUIRE(updateResult.success);
        
        // Give Redis a moment to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Search for the updated document
        auto searchResult = storage.searchSimple("updated", 10);
        REQUIRE(searchResult.success);
        
        auto response = searchResult.value;
        REQUIRE(response.results.size() >= 1);
        
        // Clean up
        storage.deleteDocument("https://hatef.ir");
    }
    
    SECTION("Delete document") {
        SearchDocument testDoc = createTestSearchDocument("https://hatef.ir");
        testDoc.title = "Document to Delete";
        
        // Index the document
        auto indexResult = storage.indexDocument(testDoc);
        REQUIRE(indexResult.success);
        
        // Give Redis a moment to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify it's searchable
        auto searchResult = storage.searchSimple("delete", 10);
        REQUIRE(searchResult.success);
        REQUIRE(searchResult.value.results.size() >= 1);
        
        // Delete the document
        auto deleteResult = storage.deleteDocument("https://hatef.ir");
        REQUIRE(deleteResult.success);
        
        // Give Redis a moment to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Verify it's no longer searchable
        auto searchAfterDelete = storage.searchSimple("delete document", 10);
        if (searchAfterDelete.success) {
            bool found = false;
            for (const auto& result : searchAfterDelete.value.results) {
                if (result.url == "https://hatef.ir") {
                    found = true;
                    break;
                }
            }
            REQUIRE(!found);
        }
    }
    
    // Clean up the test index
    storage.dropIndex();
}

TEST_CASE("RedisSearch Storage - Advanced Search Features", "[redis][storage][search]") {
    RedisSearchStorage storage("tcp://127.0.0.1:6379", "test_advanced_index");
    
    // Skip tests if Redis is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping Redis tests - Redis not available");
        return;
    }
    
    // Initialize clean index
    storage.dropIndex();
    auto initResult = storage.initializeIndex();
    REQUIRE(initResult.success);
    
    // Index multiple test documents
    std::vector<SearchDocument> testDocs = {
        createTestSearchDocument("https://tech.com"),
        createTestSearchDocument("https://science.com"),
        createTestSearchDocument("https://news.com")
    };
    
    testDocs[0].title = "Technology News";
    testDocs[0].content = "Latest technology trends and innovations in software development.";
    testDocs[0].category = "technology";
    testDocs[0].domain = "tech.com";
    
    testDocs[1].title = "Science Discovery";
    testDocs[1].content = "New scientific discoveries in physics and chemistry research.";
    testDocs[1].category = "science";
    testDocs[1].domain = "science.com";
    
    testDocs[2].title = "Breaking News";
    testDocs[2].content = "Latest news updates from around the world covering various topics.";
    testDocs[2].category = "news";
    testDocs[2].domain = "news.com";
    
    // Index all documents
    for (const auto& doc : testDocs) {
        auto result = storage.indexDocument(doc);
        REQUIRE(result.success);
    }
    
    // Give Redis a moment to process
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    SECTION("Advanced search with filters") {
        SearchQuery query;
        query.query = "news";
        query.limit = 10;
        query.category = "technology";
        
        auto searchResult = storage.search(query);
        REQUIRE(searchResult.success);
        
        auto response = searchResult.value;
        // Should find technology news
        REQUIRE(response.results.size() >= 1);
    }
    
    SECTION("Search with highlighting") {
        SearchQuery query;
        query.query = "technology";
        query.limit = 10;
        query.highlight = true;
        
        auto searchResult = storage.search(query);
        REQUIRE(searchResult.success);
        
        auto response = searchResult.value;
        REQUIRE(response.results.size() >= 1);
        REQUIRE(response.queryTime >= 0);
    }
    
    SECTION("Get document count") {
        auto countResult = storage.getDocumentCount();
        REQUIRE(countResult.success);
        REQUIRE(countResult.value >= 3);
    }
    
    SECTION("Get index info") {
        auto infoResult = storage.getIndexInfo();
        REQUIRE(infoResult.success);
        
        auto info = infoResult.value;
        REQUIRE(!info.empty());
    }
    
    // Clean up the test index
    storage.dropIndex();
}

TEST_CASE("RedisSearch Storage - Batch Operations", "[redis][storage][batch]") {
    RedisSearchStorage storage("tcp://127.0.0.1:6379", "test_batch_index");
    
    // Skip tests if Redis is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping Redis tests - Redis not available");
        return;
    }
    
    // Initialize clean index
    storage.dropIndex();
    auto initResult = storage.initializeIndex();
    REQUIRE(initResult.success);
    
    SECTION("Index multiple documents") {
        std::vector<SearchDocument> testDocs;
        for (int i = 0; i < 5; ++i) {
            SearchDocument doc = createTestSearchDocument("https://batch" + std::to_string(i) + ".com");
            doc.title = "Batch Document " + std::to_string(i);
            doc.content = "Content for batch document number " + std::to_string(i) + " with unique text.";
            doc.domain = "batch" + std::to_string(i) + ".com";
            testDocs.push_back(doc);
        }
        
        // Index documents in batch
        auto batchResult = storage.indexDocuments(testDocs);
        REQUIRE(batchResult.success);
        
        auto results = batchResult.value;
        REQUIRE(results.size() == 5);
        
        // Verify all succeeded
        for (bool success : results) {
            REQUIRE(success);
        }
        
        // Give Redis a moment to process
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Verify documents are searchable
        auto searchResult = storage.searchSimple("batch document", 10);
        REQUIRE(searchResult.success);
        REQUIRE(searchResult.value.results.size() >= 5);
        
        // Clean up
        for (int i = 0; i < 5; ++i) {
            storage.deleteDocument("https://batch" + std::to_string(i) + ".com");
        }
    }
    
    // Clean up the test index
    storage.dropIndex();
}

TEST_CASE("RedisSearch Storage - Error Handling", "[redis][storage][errors]") {
    SECTION("Delete non-existent document") {
        RedisSearchStorage storage("tcp://127.0.0.1:6379", "test_error_index");
        
        auto connectionTest = storage.testConnection();
        if (!connectionTest.success) {
            WARN("Skipping Redis tests - Redis not available");
            return;
        }
        
        auto deleteResult = storage.deleteDocument("https://non-existent.com");
        REQUIRE(!deleteResult.success);
        REQUIRE(deleteResult.message.find("not found") != std::string::npos);
    }
    
    SECTION("Search on non-existent index") {
        RedisSearchStorage storage("tcp://127.0.0.1:6379", "non_existent_index");
        
        auto connectionTest = storage.testConnection();
        if (!connectionTest.success) {
            WARN("Skipping Redis tests - Redis not available");
            return;
        }
        
        auto searchResult = storage.searchSimple("test", 10);
        // This might succeed with 0 results or fail - either is acceptable
        // The important thing is that it doesn't crash
        REQUIRE_NOTHROW(storage.searchSimple("test", 10));
    }
}

TEST_CASE("RedisSearch Storage - Utility Functions", "[redis][storage][utils]") {
    SECTION("IndexedPage to SearchDocument conversion") {
        IndexedPage page = createTestSiteProfile("https://convert-test.com");
        page.title = "Conversion Test Site";
        page.description = "Site for testing conversion";
        
        std::string content = "This is the main content of the site.";
        
        SearchDocument doc = RedisSearchStorage::siteProfileToSearchDocument(page, content);
        
        REQUIRE(doc.url == page.url);
        REQUIRE(doc.title == page.title);
        REQUIRE(doc.content == content);
        REQUIRE(doc.domain == page.domain);
        REQUIRE(doc.keywords == page.keywords);
        REQUIRE(doc.description == page.description);
        REQUIRE(doc.language == page.language);
        REQUIRE(doc.category == page.category);
        REQUIRE(doc.score == page.contentQuality.value_or(0.0));
    }
} 