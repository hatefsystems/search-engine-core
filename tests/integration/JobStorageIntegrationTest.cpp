#include "../../include/search_engine/storage/JobStorage.h"
#include "../../include/search_engine/models/Job.h"
#include "../../include/search_engine/models/JobResult.h"
#include "../../include/Logger.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include <chrono>
#include <thread>
#include <memory>

using namespace search_engine::storage;
using namespace search_engine::models;

// Test configuration - these should match your Docker setup
const std::string TEST_CONNECTION_STRING = "mongodb://admin:password123@localhost:27017";
const std::string TEST_DATABASE_NAME = "search-engine-jobs-test";

/**
 * Helper function to create a test JobStorage instance
 * This will only work if MongoDB is running (e.g., in Docker)
 */
std::unique_ptr<JobStorage> createTestJobStorage() {
    try {
        return std::make_unique<JobStorage>(TEST_CONNECTION_STRING, TEST_DATABASE_NAME);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to create JobStorage for testing: " + std::string(e.what()));
        return nullptr;
    }
}

/**
 * Helper function to clean up test data
 */
void cleanupTestData(JobStorage* storage) {
    if (!storage) return;
    
    try {
        // In a real implementation, you'd have methods to clean up test data
        // For now, we'll just log that cleanup would happen here
        LOG_DEBUG("Cleaning up test data (implementation would go here)");
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to cleanup test data: " + std::string(e.what()));
    }
}

TEST_CASE("JobStorage Integration - Connection Test", "[jobstorage][integration][mongodb]") {
    auto storage = createTestJobStorage();
    
    if (!storage) {
        SKIP("MongoDB not available for integration testing");
        return;
    }
    
    SECTION("Test database connection") {
        auto result = storage->testConnection();
        REQUIRE(result.isSuccess());
    }
    
    SECTION("Create indexes") {
        auto result = storage->createIndexes();
        REQUIRE(result.isSuccess());
    }
    
    cleanupTestData(storage.get());
}

TEST_CASE("JobStorage Integration - Basic CRUD Operations", "[jobstorage][integration][mongodb]") {
    auto storage = createTestJobStorage();
    
    if (!storage) {
        SKIP("MongoDB not available for integration testing");
        return;
    }
    
    // Ensure indexes exist
    auto indexResult = storage->createIndexes();
    REQUIRE(indexResult.isSuccess());
    
    SECTION("Store and retrieve job") {
        Job originalJob("test_crawl", "test_user_123");
        originalJob.setTenantId("test_tenant");
        originalJob.setPriority(JobPriority::HIGH);
        originalJob.setMetadata("{\"testData\": true}");
        
        // Store the job
        auto storeResult = storage->storeJob(originalJob);
        REQUIRE(storeResult.isSuccess());
        REQUIRE(storeResult.getValue() == originalJob.getId());
        
        // Retrieve the job
        auto retrieveResult = storage->getJob(originalJob.getId());
        REQUIRE(retrieveResult.isSuccess());
        
        auto retrievedJob = retrieveResult.getValue();
        REQUIRE(retrievedJob.getId() == originalJob.getId());
        REQUIRE(retrievedJob.getUserId() == originalJob.getUserId());
        REQUIRE(retrievedJob.getTenantId() == originalJob.getTenantId());
        REQUIRE(retrievedJob.getJobType() == originalJob.getJobType());
        REQUIRE(retrievedJob.getStatus() == originalJob.getStatus());
        REQUIRE(retrievedJob.getPriority() == originalJob.getPriority());
        
        // Clean up
        auto deleteResult = storage->deleteJob(originalJob.getId());
        REQUIRE(deleteResult.isSuccess());
    }
    
    SECTION("Update job") {
        Job job("test_analysis", "test_user_456");
        job.setProgress(25);
        
        // Store the job
        auto storeResult = storage->storeJob(job);
        REQUIRE(storeResult.isSuccess());
        
        // Modify the job
        job.setProgress(75);
        job.start(); // This changes status to PROCESSING
        
        // Update the job
        auto updateResult = storage->updateJob(job);
        REQUIRE(updateResult.isSuccess());
        
        // Retrieve and verify the update
        auto retrieveResult = storage->getJob(job.getId());
        REQUIRE(retrieveResult.isSuccess());
        
        auto updatedJob = retrieveResult.getValue();
        REQUIRE(updatedJob.getProgress() == 75);
        REQUIRE(updatedJob.getStatus() == JobStatus::PROCESSING);
        REQUIRE(updatedJob.getStartedAt().has_value());
        
        // Clean up
        storage->deleteJob(job.getId());
    }
    
    cleanupTestData(storage.get());
}

TEST_CASE("JobStorage Integration - Query Operations", "[jobstorage][integration][mongodb]") {
    auto storage = createTestJobStorage();
    
    if (!storage) {
        SKIP("MongoDB not available for integration testing");
        return;
    }
    
    // Ensure indexes exist
    storage->createIndexes();
    
    SECTION("Get jobs by user") {
        const std::string testUserId = "query_test_user";
        
        // Create multiple jobs for the same user
        std::vector<Job> testJobs;
        for (int i = 0; i < 3; ++i) {
            Job job("test_type_" + std::to_string(i), testUserId);
            job.setTenantId("test_tenant");
            testJobs.push_back(job);
            
            auto storeResult = storage->storeJob(job);
            REQUIRE(storeResult.isSuccess());
        }
        
        // Query jobs by user
        JobQueryOptions options;
        options.limit = 10;
        auto queryResult = storage->getJobsByUser(testUserId, options);
        REQUIRE(queryResult.isSuccess());
        
        auto retrievedJobs = queryResult.getValue();
        REQUIRE(retrievedJobs.size() == 3);
        
        // Verify all jobs belong to the correct user
        for (const auto& job : retrievedJobs) {
            REQUIRE(job.getUserId() == testUserId);
        }
        
        // Clean up
        for (const auto& job : testJobs) {
            storage->deleteJob(job.getId());
        }
    }
    
    SECTION("Get jobs by status") {
        const JobStatus testStatus = JobStatus::PROCESSING;
        
        // Create jobs with different statuses
        Job job1("test_type", "user1");
        job1.start(); // Sets status to PROCESSING
        auto store1 = storage->storeJob(job1);
        REQUIRE(store1.isSuccess());
        
        Job job2("test_type", "user2");
        // Keep default QUEUED status
        auto store2 = storage->storeJob(job2);
        REQUIRE(store2.isSuccess());
        
        // Query by status
        JobQueryOptions options;
        options.limit = 10;
        auto queryResult = storage->getJobsByStatus(testStatus, options);
        REQUIRE(queryResult.isSuccess());
        
        auto retrievedJobs = queryResult.getValue();
        REQUIRE(retrievedJobs.size() >= 1); // At least our test job
        
        // Verify all jobs have the correct status
        bool foundOurJob = false;
        for (const auto& job : retrievedJobs) {
            REQUIRE(job.getStatus() == testStatus);
            if (job.getId() == job1.getId()) {
                foundOurJob = true;
            }
        }
        REQUIRE(foundOurJob);
        
        // Clean up
        storage->deleteJob(job1.getId());
        storage->deleteJob(job2.getId());
    }
    
    cleanupTestData(storage.get());
}

TEST_CASE("JobStorage Integration - Job Results", "[jobstorage][integration][mongodb]") {
    auto storage = createTestJobStorage();
    
    if (!storage) {
        SKIP("MongoDB not available for integration testing");
        return;
    }
    
    storage->createIndexes();
    
    SECTION("Store and retrieve job result") {
        // First create a job
        Job job("test_crawl", "result_test_user");
        auto storeJobResult = storage->storeJob(job);
        REQUIRE(storeJobResult.isSuccess());
        
        // Create a job result
        JobResult result(job.getId(), job.getUserId(), job.getTenantId(), JobStatus::COMPLETED);
        result.setResultData("{\"pages_crawled\": 42, \"urls_found\": 156}");
        
        // Add some metrics
        JobMetrics metrics;
        metrics.executionDuration = std::chrono::milliseconds(5000);
        metrics.itemsProcessed = 42;
        metrics.peakMemoryUsage = 1024 * 1024; // 1MB
        result.updateMetrics(metrics);
        
        // Add an output file
        result.addOutputFile("crawl_results.json", "/tmp/crawl_results.json", "application/json", 1024);
        
        // Store the result
        auto storeResultResult = storage->storeJobResult(result);
        REQUIRE(storeResultResult.isSuccess());
        REQUIRE(storeResultResult.getValue() == result.getId());
        
        // Retrieve by result ID
        auto retrieveResult = storage->getJobResult(result.getId());
        REQUIRE(retrieveResult.isSuccess());
        
        auto retrievedResult = retrieveResult.getValue();
        REQUIRE(retrievedResult.getId() == result.getId());
        REQUIRE(retrievedResult.getJobId() == result.getJobId());
        REQUIRE(retrievedResult.getUserId() == result.getUserId());
        REQUIRE(retrievedResult.getFinalStatus() == JobStatus::COMPLETED);
        REQUIRE(retrievedResult.getResultData().has_value());
        REQUIRE(retrievedResult.getOutputFileCount() == 1);
        
        // Retrieve by job ID
        auto retrieveByJobIdResult = storage->getJobResultByJobId(job.getId());
        REQUIRE(retrieveByJobIdResult.isSuccess());
        REQUIRE(retrieveByJobIdResult.getValue().getId() == result.getId());
        
        // Clean up
        storage->deleteJobResult(result.getId());
        storage->deleteJob(job.getId());
    }
    
    cleanupTestData(storage.get());
}

TEST_CASE("JobStorage Integration - Statistics", "[jobstorage][integration][mongodb]") {
    auto storage = createTestJobStorage();
    
    if (!storage) {
        SKIP("MongoDB not available for integration testing");
        return;
    }
    
    storage->createIndexes();
    
    SECTION("Get storage statistics") {
        // Create some test jobs with different statuses
        std::vector<Job> testJobs;
        
        // Create jobs with different statuses
        for (int i = 0; i < 2; ++i) {
            Job job("stats_test", "stats_user_" + std::to_string(i));
            if (i == 0) {
                job.start();
                job.complete();
            } else {
                job.fail("Test failure");
            }
            testJobs.push_back(job);
            storage->storeJob(job);
        }
        
        // Get statistics
        auto statsResult = storage->getStorageStats();
        REQUIRE(statsResult.isSuccess());
        
        auto stats = statsResult.getValue();
        REQUIRE(stats.totalJobs >= 2); // At least our test jobs
        
        // Clean up
        for (const auto& job : testJobs) {
            storage->deleteJob(job.getId());
        }
    }
    
    SECTION("Count jobs by status") {
        const JobStatus testStatus = JobStatus::COMPLETED;
        
        // Get initial count
        auto initialCountResult = storage->getJobCountByStatus(testStatus);
        REQUIRE(initialCountResult.isSuccess());
        int64_t initialCount = initialCountResult.getValue();
        
        // Create a completed job
        Job job("count_test", "count_user");
        job.start();
        job.complete();
        storage->storeJob(job);
        
        // Get count after adding job
        auto newCountResult = storage->getJobCountByStatus(testStatus);
        REQUIRE(newCountResult.isSuccess());
        int64_t newCount = newCountResult.getValue();
        
        REQUIRE(newCount == initialCount + 1);
        
        // Clean up
        storage->deleteJob(job.getId());
    }
    
    cleanupTestData(storage.get());
}

TEST_CASE("JobStorage Integration - Error Handling", "[jobstorage][integration][mongodb]") {
    auto storage = createTestJobStorage();
    
    if (!storage) {
        SKIP("MongoDB not available for integration testing");
        return;
    }
    
    SECTION("Retrieve non-existent job") {
        auto result = storage->getJob("non_existent_job_id");
        REQUIRE(!result.isSuccess());
        REQUIRE(result.getError().find("not found") != std::string::npos);
    }
    
    SECTION("Update non-existent job") {
        Job job("test_type", "test_user");
        job.setId("non_existent_job_id");
        
        auto result = storage->updateJob(job);
        REQUIRE(!result.isSuccess());
    }
    
    SECTION("Delete non-existent job") {
        auto result = storage->deleteJob("non_existent_job_id");
        REQUIRE(!result.isSuccess());
    }
    
    cleanupTestData(storage.get());
}

// Test helper to verify all required methods exist and are callable
TEST_CASE("JobStorage Integration - API Completeness", "[jobstorage][integration]") {
    auto storage = createTestJobStorage();
    
    if (!storage) {
        SKIP("MongoDB not available for integration testing");
        return;
    }
    
    SECTION("All CRUD methods are callable") {
        // This test just verifies that all the main API methods exist and are callable
        // without necessarily executing them (some might fail due to missing data)
        
        Job testJob("api_test", "api_user");
        JobQueryOptions options;
        
        // Job CRUD operations
        REQUIRE_NOTHROW(storage->getJob("test_id"));
        REQUIRE_NOTHROW(storage->getJobsByUser("test_user", options));
        REQUIRE_NOTHROW(storage->getJobsByStatus(JobStatus::QUEUED, options));
        REQUIRE_NOTHROW(storage->getJobsByType("test_type", options));
        REQUIRE_NOTHROW(storage->getJobsByPriority(JobPriority::NORMAL, options));
        
        // Statistics operations  
        REQUIRE_NOTHROW(storage->getStorageStats());
        REQUIRE_NOTHROW(storage->getJobCountByStatus(JobStatus::QUEUED));
        REQUIRE_NOTHROW(storage->getJobCountByType("test_type"));
        REQUIRE_NOTHROW(storage->getJobCountByUser("test_user"));
        
        // Connection operations
        REQUIRE_NOTHROW(storage->testConnection());
        REQUIRE_NOTHROW(storage->createIndexes());
        REQUIRE_NOTHROW(storage->ensureIndexes());
    }
    
    cleanupTestData(storage.get());
}
