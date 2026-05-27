#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "../../include/search_engine/storage/ProfileStorage.h"
#include "../../include/search_engine/storage/ProfileViewAnalytics.h"
#include "../../include/Logger.h"
#include <chrono>
#include <thread>
#include <random>

using namespace search_engine::storage;

// Performance test helper namespace
namespace {
    Profile createTestProfile(const std::string& slug, const std::string& name) {
        Profile profile;
        profile.slug = slug;
        profile.name = name;
        profile.type = ProfileType::PERSON;
        profile.bio = "Performance test profile";
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();
        return profile;
    }
    
    std::string generateRandomSlug(int index) {
        return "perf-test-" + std::to_string(index);
    }
}

TEST_CASE("ProfileStorage - Performance: Slug Lookup Speed", "[profilestorage][performance]") {
    ProfileStorage storage("mongodb://admin:password123@localhost:27017", "test-search-engine-perf");
    
    // Skip tests if MongoDB is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping performance tests - MongoDB not available");
        return;
    }
    
    SECTION("Slug lookup performance: 100 lookups should complete in < 100ms") {
        // Create 1000 test profiles
        LOG_INFO("Creating 1000 test profiles for performance testing...");
        std::vector<std::string> profileIds;
        
        for (int i = 0; i < 1000; i++) {
            auto profile = createTestProfile(generateRandomSlug(i), "Test User " + std::to_string(i));
            auto result = storage.store(profile);
            if (result.success) {
                profileIds.push_back(result.value);  // store() returns the ID directly
            }
        }
        
        REQUIRE(profileIds.size() >= 100);
        
        // Benchmark: 100 slug lookups
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 100; i++) {
            std::string slug = generateRandomSlug(i);
            auto result = storage.findBySlug(slug);
            REQUIRE(result.success);
            REQUIRE(result.value.has_value());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        LOG_INFO("100 slug lookups completed in " + std::to_string(duration.count()) + "ms");
        
        // Should be under 100ms for 100 lookups (as per task spec)
        REQUIRE(duration.count() < 100);
        
        // Cleanup
        for (const auto& id : profileIds) {
            storage.deleteProfile(id);
        }
    }
    
    SECTION("Profile ID lookup performance: 100 lookups should be fast") {
        // Create test profiles
        std::vector<std::string> profileIds;
        
        for (int i = 0; i < 100; i++) {
            auto profile = createTestProfile(generateRandomSlug(1000 + i), "Test User " + std::to_string(i));
            auto result = storage.store(profile);
            if (result.success) {
                profileIds.push_back(result.value);  // store() returns the ID directly
            }
        }
        
        REQUIRE(profileIds.size() == 100);
        
        // Benchmark: 100 ID lookups
        auto start = std::chrono::high_resolution_clock::now();
        
        for (const auto& id : profileIds) {
            auto result = storage.findById(id);
            REQUIRE(result.success);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        LOG_INFO("100 ID lookups completed in " + std::to_string(duration.count()) + "ms");
        
        // Should be under 50ms for 100 ID lookups (IDs have indexes)
        REQUIRE(duration.count() < 50);
        
        // Cleanup
        for (const auto& id : profileIds) {
            storage.deleteProfile(id);
        }
    }
}

TEST_CASE("ProfileStorage - Performance: Batch Operations", "[profilestorage][performance]") {
    ProfileStorage storage("mongodb://admin:password123@localhost:27017", "test-search-engine-perf");
    
    // Skip tests if MongoDB is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping performance tests - MongoDB not available");
        return;
    }
    
    SECTION("Batch profile creation: 100 profiles") {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::string> profileIds;
        for (int i = 0; i < 100; i++) {
            auto profile = createTestProfile(generateRandomSlug(2000 + i), "Batch Test " + std::to_string(i));
            auto result = storage.store(profile);
            if (result.success) {
                profileIds.push_back(result.value);  // store() returns the ID directly
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        LOG_INFO("100 profile creations completed in " + std::to_string(duration.count()) + "ms");
        
        REQUIRE(profileIds.size() == 100);
        // Should create 100 profiles in reasonable time
        REQUIRE(duration.count() < 1000);  // < 1 second for 100 creates
        
        // Cleanup
        for (const auto& id : profileIds) {
            storage.deleteProfile(id);
        }
    }
}

TEST_CASE("ProfileViewAnalyticsStorage - Performance: Analytics Queries", "[analytics][performance]") {
    ProfileViewAnalyticsStorage analyticsStorage(
        "mongodb://admin:password123@localhost:27017", 
        "test-search-engine"
    );
    
    SECTION("Analytics query performance: Recent views retrieval") {
        // Create a test profile ID
        std::string testProfileId = "perf-test-profile-analytics";
        
        // Insert 1000 test analytics records
        LOG_INFO("Creating 1000 test analytics records...");
        for (int i = 0; i < 1000; i++) {
            ProfileViewAnalytics analytics;
            analytics.viewId = "perf-view-" + std::to_string(i);
            analytics.profileId = testProfileId;
            analytics.timestamp = std::chrono::system_clock::now();
            analytics.country = "Iran";
            analytics.province = "Tehran";
            analytics.city = "Tehran";
            analytics.browser = "Chrome";
            analytics.os = "Linux";
            analytics.deviceType = "Desktop";
            
            analyticsStorage.recordView(analytics);
        }
        
        // Benchmark: Recent views query
        auto start = std::chrono::high_resolution_clock::now();
        
        auto result = analyticsStorage.getRecentViewsByProfile(testProfileId, 100);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        LOG_INFO("Recent views query (100 records) completed in " + std::to_string(duration.count()) + "ms");
        
        REQUIRE(result.success);
        REQUIRE(result.value.size() == 100);
        
        // Should be under 200ms as per task spec
        REQUIRE(duration.count() < 200);
    }
    
    SECTION("Analytics count performance") {
        std::string testProfileId = "perf-test-profile-count";
        
        // Insert test analytics
        for (int i = 0; i < 500; i++) {
            ProfileViewAnalytics analytics;
            analytics.viewId = "perf-count-" + std::to_string(i);
            analytics.profileId = testProfileId;
            analytics.timestamp = std::chrono::system_clock::now();
            analytics.country = "Iran";
            analytics.city = "Tehran";
            analytics.browser = "Chrome";
            analytics.os = "Linux";
            analytics.deviceType = "Desktop";
            
            analyticsStorage.recordView(analytics);
        }
        
        // Benchmark: Count views query
        auto start = std::chrono::high_resolution_clock::now();
        
        auto result = analyticsStorage.countViewsByProfile(testProfileId);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        LOG_INFO("Count views query completed in " + std::to_string(duration.count()) + "ms");
        
        REQUIRE(result.success);
        REQUIRE(result.value >= 500);
        
        // Count should be very fast with indexes
        REQUIRE(duration.count() < 100);
    }
}

TEST_CASE("ProfileStorage - Performance: Concurrent Operations", "[profilestorage][performance][concurrent]") {
    ProfileStorage storage("mongodb://admin:password123@localhost:27017", "test-search-engine-perf");
    
    // Skip tests if MongoDB is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping concurrent performance tests - MongoDB not available");
        return;
    }
    
    SECTION("Concurrent slug lookups") {
        // Create test profiles
        std::vector<std::string> slugs;
        for (int i = 0; i < 50; i++) {
            auto profile = createTestProfile(generateRandomSlug(3000 + i), "Concurrent Test " + std::to_string(i));
            auto result = storage.store(profile);
            if (result.success) {
                slugs.push_back(profile.slug);
            }
        }
        
        REQUIRE(slugs.size() == 50);
        
        // Simulate concurrent lookups
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        std::atomic<int> successCount{0};
        
        for (int i = 0; i < 10; i++) {
            threads.emplace_back([&storage, &slugs, &successCount]() {
                for (const auto& slug : slugs) {
                    auto result = storage.findBySlug(slug);
                    if (result.success && result.value.has_value()) {
                        successCount++;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        LOG_INFO("Concurrent slug lookups (10 threads x 50 lookups) completed in " + 
                 std::to_string(duration.count()) + "ms");
        
        REQUIRE(successCount == 500);  // 10 threads * 50 slugs
        
        // Should handle concurrent load well
        REQUIRE(duration.count() < 2000);  // < 2 seconds for 500 concurrent lookups
        
        // Cleanup
        for (const auto& slug : slugs) {
            auto result = storage.findBySlug(slug);
            if (result.success && result.value.has_value()) {
                storage.deleteProfile(result.value.value().id.value_or(""));
            }
        }
    }
}
