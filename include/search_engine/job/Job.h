#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <memory>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/oid.hpp>
#include <nlohmann/json.hpp>
#include "../../infrastructure.h"

namespace search_engine {
namespace models {

/**
 * Job status enumeration representing the lifecycle of a job
 */
enum class JobStatus {
    QUEUED,       // Job has been queued for processing
    PROCESSING,   // Job is currently being processed
    COMPLETED,    // Job has completed successfully
    FAILED,       // Job has failed
    CANCELLED,    // Job was cancelled
    RETRYING      // Job is being retried after failure
};

/**
 * Job priority levels for scheduling
 */
enum class JobPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

/**
 * Core Job model representing a job in the universal job manager system
 * 
 * This class follows RAII principles and provides proper serialization
 * to/from MongoDB BSON and JSON formats.
 */
class Job {
private:
    std::string id_;                                          // Unique job identifier
    std::string userId_;                                      // User who created the job
    std::string tenantId_;                                    // Multi-tenant identifier
    std::string jobType_;                                     // Type of job (e.g., "crawl", "analysis")
    JobStatus status_;                                        // Current job status
    JobPriority priority_;                                    // Job priority level
    int progress_;                                           // Progress percentage (0-100)
    std::chrono::system_clock::time_point createdAt_;       // Creation timestamp
    std::optional<std::chrono::system_clock::time_point> startedAt_;   // Start timestamp
    std::optional<std::chrono::system_clock::time_point> completedAt_; // Completion timestamp
    std::optional<std::chrono::system_clock::time_point> scheduledAt_; // Scheduled execution time
    std::optional<std::string> errorMessage_;                // Error message if failed
    std::optional<std::string> metadata_;                    // Additional job-specific metadata (JSON)
    int retryCount_;                                         // Number of retry attempts
    int maxRetries_;                                         // Maximum allowed retries
    std::optional<std::chrono::seconds> timeout_;           // Job timeout duration

public:
    /**
     * Default constructor - creates a new job with generated ID
     */
    Job();

    /**
     * Constructor with job type and user ID
     */
    Job(const std::string& jobType, const std::string& userId);

    /**
     * Constructor with full parameters
     */
    Job(const std::string& id,
        const std::string& userId,
        const std::string& tenantId,
        const std::string& jobType,
        JobStatus status = JobStatus::QUEUED,
        JobPriority priority = JobPriority::NORMAL);

    // Copy and move constructors/assignment operators
    Job(const Job& other) = default;
    Job& operator=(const Job& other) = default;
    Job(Job&& other) noexcept = default;
    Job& operator=(Job&& other) noexcept = default;

    // Destructor
    ~Job() = default;

    // Getters
    const std::string& getId() const { return id_; }
    const std::string& getUserId() const { return userId_; }
    const std::string& getTenantId() const { return tenantId_; }
    const std::string& getJobType() const { return jobType_; }
    JobStatus getStatus() const { return status_; }
    JobPriority getPriority() const { return priority_; }
    int getProgress() const { return progress_; }
    const std::chrono::system_clock::time_point& getCreatedAt() const { return createdAt_; }
    const std::optional<std::chrono::system_clock::time_point>& getStartedAt() const { return startedAt_; }
    const std::optional<std::chrono::system_clock::time_point>& getCompletedAt() const { return completedAt_; }
    const std::optional<std::chrono::system_clock::time_point>& getScheduledAt() const { return scheduledAt_; }
    const std::optional<std::string>& getErrorMessage() const { return errorMessage_; }
    const std::optional<std::string>& getMetadata() const { return metadata_; }
    int getRetryCount() const { return retryCount_; }
    int getMaxRetries() const { return maxRetries_; }
    const std::optional<std::chrono::seconds>& getTimeout() const { return timeout_; }

    // Setters
    void setId(const std::string& id) { id_ = id; }
    void setUserId(const std::string& userId) { userId_ = userId; }
    void setTenantId(const std::string& tenantId) { tenantId_ = tenantId; }
    void setJobType(const std::string& jobType) { jobType_ = jobType; }
    void setStatus(JobStatus status) { status_ = status; }
    void setPriority(JobPriority priority) { priority_ = priority; }
    void setProgress(int progress);
    void setStartedAt(const std::chrono::system_clock::time_point& time) { startedAt_ = time; }
    void setCompletedAt(const std::chrono::system_clock::time_point& time) { completedAt_ = time; }
    void setScheduledAt(const std::chrono::system_clock::time_point& time) { scheduledAt_ = time; }
    void setErrorMessage(const std::string& error) { errorMessage_ = error; }
    void setMetadata(const std::string& metadata) { metadata_ = metadata; }
    void setRetryCount(int count) { retryCount_ = count; }
    void setMaxRetries(int maxRetries) { maxRetries_ = maxRetries; }
    void setTimeout(const std::chrono::seconds& timeout) { timeout_ = timeout; }

    // Business logic methods
    void start();                                            // Mark job as started
    void complete();                                         // Mark job as completed
    void fail(const std::string& errorMessage);             // Mark job as failed
    void cancel();                                           // Mark job as cancelled
    bool canRetry() const;                                   // Check if job can be retried
    void incrementRetry();                                   // Increment retry count
    bool isExpired() const;                                  // Check if job has timed out
    std::chrono::milliseconds getDuration() const;          // Get job duration

    // Validation methods
    bool isValid() const;                                    // Validate job data
    Result<bool> validate() const;                           // Detailed validation with errors

    // Serialization methods
    bsoncxx::document::value toBson() const;                 // Convert to BSON for MongoDB
    static Job fromBson(const bsoncxx::document::view& doc); // Create from BSON
    nlohmann::json toJson() const;                          // Convert to JSON
    static Job fromJson(const nlohmann::json& json);       // Create from JSON

    // Utility methods
    std::string toString() const;                           // String representation for logging
    bool operator==(const Job& other) const;               // Equality comparison
    bool operator!=(const Job& other) const { return !(*this == other); }

    // Static helper methods
    static std::string generateJobId();                     // Generate unique job ID
    static std::string statusToString(JobStatus status);   // Convert status to string
    static JobStatus stringToStatus(const std::string& status); // Convert string to status
    static std::string priorityToString(JobPriority priority); // Convert priority to string
    static JobPriority stringToPriority(const std::string& priority); // Convert string to priority
};

} // namespace models
} // namespace search_engine
