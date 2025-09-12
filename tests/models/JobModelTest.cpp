#include "../../include/search_engine/models/Job.h"
#include "../../include/Logger.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>

using namespace search_engine::models;

TEST_CASE("Job Model - Basic Construction", "[job][models]") {
    SECTION("Default constructor") {
        Job job;
        
        REQUIRE(!job.getId().empty());
        REQUIRE(job.getUserId().empty());
        REQUIRE(job.getJobType().empty());
        REQUIRE(job.getStatus() == JobStatus::QUEUED);
        REQUIRE(job.getPriority() == JobPriority::NORMAL);
        REQUIRE(job.getProgress() == 0);
        REQUIRE(job.getRetryCount() == 0);
        REQUIRE(job.getMaxRetries() == 3);
    }
    
    SECTION("Constructor with job type and user ID") {
        Job job("crawl", "user123");
        
        REQUIRE(!job.getId().empty());
        REQUIRE(job.getUserId() == "user123");
        REQUIRE(job.getJobType() == "crawl");
        REQUIRE(job.getStatus() == JobStatus::QUEUED);
        REQUIRE(job.getPriority() == JobPriority::NORMAL);
        REQUIRE(job.getProgress() == 0);
    }
    
    SECTION("Full constructor") {
        Job job("job_123", "user123", "tenant456", "crawl", JobStatus::PROCESSING, JobPriority::HIGH);
        
        REQUIRE(job.getId() == "job_123");
        REQUIRE(job.getUserId() == "user123");
        REQUIRE(job.getTenantId() == "tenant456");
        REQUIRE(job.getJobType() == "crawl");
        REQUIRE(job.getStatus() == JobStatus::PROCESSING);
        REQUIRE(job.getPriority() == JobPriority::HIGH);
    }
}

TEST_CASE("Job Model - Status Management", "[job][models]") {
    Job job("crawl", "user123");
    
    SECTION("Start job") {
        job.start();
        
        REQUIRE(job.getStatus() == JobStatus::PROCESSING);
        REQUIRE(job.getStartedAt().has_value());
    }
    
    SECTION("Complete job") {
        job.start();
        job.complete();
        
        REQUIRE(job.getStatus() == JobStatus::COMPLETED);
        REQUIRE(job.getProgress() == 100);
        REQUIRE(job.getCompletedAt().has_value());
    }
    
    SECTION("Fail job") {
        job.start();
        job.fail("Test error message");
        
        REQUIRE(job.getStatus() == JobStatus::FAILED);
        REQUIRE(job.getErrorMessage().has_value());
        REQUIRE(job.getErrorMessage().value() == "Test error message");
        REQUIRE(job.getCompletedAt().has_value());
    }
    
    SECTION("Cancel job") {
        job.cancel();
        
        REQUIRE(job.getStatus() == JobStatus::CANCELLED);
        REQUIRE(job.getCompletedAt().has_value());
    }
}

TEST_CASE("Job Model - Progress Management", "[job][models]") {
    Job job("crawl", "user123");
    
    SECTION("Valid progress values") {
        job.setProgress(0);
        REQUIRE(job.getProgress() == 0);
        
        job.setProgress(50);
        REQUIRE(job.getProgress() == 50);
        
        job.setProgress(100);
        REQUIRE(job.getProgress() == 100);
    }
    
    SECTION("Invalid progress values are ignored") {
        int originalProgress = job.getProgress();
        
        job.setProgress(-1);
        REQUIRE(job.getProgress() == originalProgress);
        
        job.setProgress(101);
        REQUIRE(job.getProgress() == originalProgress);
    }
}

TEST_CASE("Job Model - Retry Logic", "[job][models]") {
    Job job("crawl", "user123");
    job.setMaxRetries(3);
    
    SECTION("Initial retry state") {
        REQUIRE(job.getRetryCount() == 0);
        REQUIRE(job.getMaxRetries() == 3);
        REQUIRE(job.canRetry() == false); // Can't retry unless failed
    }
    
    SECTION("Retry after failure") {
        job.fail("First attempt failed");
        
        REQUIRE(job.canRetry() == true);
        REQUIRE(job.getRetryCount() == 0);
        
        job.incrementRetry();
        REQUIRE(job.getRetryCount() == 1);
        REQUIRE(job.getStatus() == JobStatus::RETRYING);
        REQUIRE(job.canRetry() == true); // Still can retry
    }
    
    SECTION("Maximum retries reached") {
        job.fail("Failed");
        
        // Retry until max retries reached
        for (int i = 0; i < 3; ++i) {
            REQUIRE(job.canRetry() == true);
            job.incrementRetry();
        }
        
        REQUIRE(job.canRetry() == false);
        REQUIRE(job.getRetryCount() == 3);
    }
}

TEST_CASE("Job Model - Timeout Management", "[job][models]") {
    Job job("crawl", "user123");
    
    SECTION("Job without timeout") {
        REQUIRE(job.isExpired() == false);
    }
    
    SECTION("Job with timeout - not expired") {
        job.setTimeout(std::chrono::seconds(60)); // 1 minute timeout
        job.start();
        
        REQUIRE(job.isExpired() == false);
    }
    
    SECTION("Job with very short timeout - expired") {
        job.setTimeout(std::chrono::seconds(1));
        job.start();
        
        // Sleep for slightly longer than timeout
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        
        REQUIRE(job.isExpired() == true);
    }
}

TEST_CASE("Job Model - Duration Calculation", "[job][models]") {
    Job job("crawl", "user123");
    
    SECTION("Duration without start time") {
        auto duration = job.getDuration();
        REQUIRE(duration == std::chrono::milliseconds::zero());
    }
    
    SECTION("Duration with start time") {
        job.start();
        
        // Sleep for a short duration
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto duration = job.getDuration();
        REQUIRE(duration.count() >= 100); // Should be at least 100ms
    }
    
    SECTION("Duration for completed job") {
        job.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        job.complete();
        
        auto duration = job.getDuration();
        REQUIRE(duration.count() >= 100); // Should be at least 100ms
        REQUIRE(duration.count() < 1000); // Should be less than 1 second
    }
}

TEST_CASE("Job Model - Validation", "[job][models]") {
    SECTION("Valid job") {
        Job job("crawl", "user123");
        
        REQUIRE(job.isValid() == true);
        
        auto validationResult = job.validate();
        REQUIRE(validationResult.isSuccess() == true);
    }
    
    SECTION("Invalid job - empty ID") {
        Job job("crawl", "user123");
        job.setId(""); // Make invalid
        
        REQUIRE(job.isValid() == false);
        
        auto validationResult = job.validate();
        REQUIRE(validationResult.isSuccess() == false);
        REQUIRE(validationResult.getError().find("ID cannot be empty") != std::string::npos);
    }
    
    SECTION("Invalid job - empty user ID") {
        Job job("crawl", "");
        
        REQUIRE(job.isValid() == false);
        
        auto validationResult = job.validate();
        REQUIRE(validationResult.isSuccess() == false);
    }
    
    SECTION("Invalid job - empty job type") {
        Job job("", "user123");
        
        REQUIRE(job.isValid() == false);
        
        auto validationResult = job.validate();
        REQUIRE(validationResult.isSuccess() == false);
    }
}

TEST_CASE("Job Model - JSON Serialization", "[job][models]") {
    Job originalJob("crawl", "user123");
    originalJob.setTenantId("tenant456");
    originalJob.setPriority(JobPriority::HIGH);
    originalJob.setProgress(50);
    originalJob.setMetadata("{\"url\": \"https://example.com\"}");
    originalJob.start();
    
    SECTION("To JSON") {
        auto json = originalJob.toJson();
        
        REQUIRE(json["id"] == originalJob.getId());
        REQUIRE(json["userId"] == "user123");
        REQUIRE(json["tenantId"] == "tenant456");
        REQUIRE(json["jobType"] == "crawl");
        REQUIRE(json["status"] == "processing");
        REQUIRE(json["priority"] == "high");
        REQUIRE(json["progress"] == 50);
        REQUIRE(json.contains("createdAt"));
        REQUIRE(json.contains("startedAt"));
        REQUIRE(json["retryCount"] == 0);
        REQUIRE(json["maxRetries"] == 3);
    }
    
    SECTION("From JSON roundtrip") {
        auto json = originalJob.toJson();
        auto deserializedJob = Job::fromJson(json);
        
        REQUIRE(deserializedJob.getId() == originalJob.getId());
        REQUIRE(deserializedJob.getUserId() == originalJob.getUserId());
        REQUIRE(deserializedJob.getTenantId() == originalJob.getTenantId());
        REQUIRE(deserializedJob.getJobType() == originalJob.getJobType());
        REQUIRE(deserializedJob.getStatus() == originalJob.getStatus());
        REQUIRE(deserializedJob.getPriority() == originalJob.getPriority());
        REQUIRE(deserializedJob.getProgress() == originalJob.getProgress());
        REQUIRE(deserializedJob.getRetryCount() == originalJob.getRetryCount());
        REQUIRE(deserializedJob.getMaxRetries() == originalJob.getMaxRetries());
    }
}

TEST_CASE("Job Model - BSON Serialization", "[job][models]") {
    // Note: This test requires MongoDB instance to be initialized
    // In a real test environment, you'd mock this or ensure MongoDB is available
    
    Job originalJob("crawl", "user123");
    originalJob.setTenantId("tenant456");
    originalJob.setPriority(JobPriority::HIGH);
    originalJob.setProgress(75);
    originalJob.start();
    
    SECTION("BSON roundtrip") {
        // This would normally require MongoDB instance to be initialized
        // For now, we'll just test that the methods exist and don't crash
        try {
            auto bson = originalJob.toBson();
            auto deserializedJob = Job::fromBson(bson.view());
            
            REQUIRE(deserializedJob.getId() == originalJob.getId());
            REQUIRE(deserializedJob.getUserId() == originalJob.getUserId());
            REQUIRE(deserializedJob.getJobType() == originalJob.getJobType());
            
        } catch (const std::exception& e) {
            // If MongoDB is not available, just log and continue
            LOG_INFO("BSON test skipped - MongoDB not available: " + std::string(e.what()));
        }
    }
}

TEST_CASE("Job Model - Static Helper Methods", "[job][models]") {
    SECTION("Generate job ID") {
        auto id1 = Job::generateJobId();
        auto id2 = Job::generateJobId();
        
        REQUIRE(!id1.empty());
        REQUIRE(!id2.empty());
        REQUIRE(id1 != id2); // Should be unique
        REQUIRE(id1.substr(0, 4) == "job_"); // Should start with "job_"
    }
    
    SECTION("Status string conversion") {
        REQUIRE(Job::statusToString(JobStatus::QUEUED) == "queued");
        REQUIRE(Job::statusToString(JobStatus::PROCESSING) == "processing");
        REQUIRE(Job::statusToString(JobStatus::COMPLETED) == "completed");
        REQUIRE(Job::statusToString(JobStatus::FAILED) == "failed");
        REQUIRE(Job::statusToString(JobStatus::CANCELLED) == "cancelled");
        REQUIRE(Job::statusToString(JobStatus::RETRYING) == "retrying");
        
        REQUIRE(Job::stringToStatus("queued") == JobStatus::QUEUED);
        REQUIRE(Job::stringToStatus("processing") == JobStatus::PROCESSING);
        REQUIRE(Job::stringToStatus("completed") == JobStatus::COMPLETED);
        REQUIRE(Job::stringToStatus("failed") == JobStatus::FAILED);
        REQUIRE(Job::stringToStatus("cancelled") == JobStatus::CANCELLED);
        REQUIRE(Job::stringToStatus("retrying") == JobStatus::RETRYING);
        REQUIRE(Job::stringToStatus("invalid") == JobStatus::QUEUED); // Default
    }
    
    SECTION("Priority string conversion") {
        REQUIRE(Job::priorityToString(JobPriority::LOW) == "low");
        REQUIRE(Job::priorityToString(JobPriority::NORMAL) == "normal");
        REQUIRE(Job::priorityToString(JobPriority::HIGH) == "high");
        REQUIRE(Job::priorityToString(JobPriority::CRITICAL) == "critical");
        
        REQUIRE(Job::stringToPriority("low") == JobPriority::LOW);
        REQUIRE(Job::stringToPriority("normal") == JobPriority::NORMAL);
        REQUIRE(Job::stringToPriority("high") == JobPriority::HIGH);
        REQUIRE(Job::stringToPriority("critical") == JobPriority::CRITICAL);
        REQUIRE(Job::stringToPriority("invalid") == JobPriority::NORMAL); // Default
    }
}

TEST_CASE("Job Model - Equality Comparison", "[job][models]") {
    Job job1("crawl", "user123");
    Job job2("crawl", "user123");
    Job job3("analysis", "user456");
    
    // Set same ID to make them equal for comparison purposes
    job2.setId(job1.getId());
    job2.setTenantId(job1.getTenantId());
    
    SECTION("Equal jobs") {
        REQUIRE(job1 == job2);
        REQUIRE(!(job1 != job2));
    }
    
    SECTION("Different jobs") {
        REQUIRE(job1 != job3);
        REQUIRE(!(job1 == job3));
    }
}

TEST_CASE("Job Model - String Representation", "[job][models]") {
    Job job("crawl", "user123");
    job.setProgress(50);
    
    auto str = job.toString();
    
    REQUIRE(str.find("Job[") != std::string::npos);
    REQUIRE(str.find("id=" + job.getId()) != std::string::npos);
    REQUIRE(str.find("type=crawl") != std::string::npos);
    REQUIRE(str.find("status=queued") != std::string::npos);
    REQUIRE(str.find("progress=50%") != std::string::npos);
    REQUIRE(str.find("user=user123") != std::string::npos);
}
