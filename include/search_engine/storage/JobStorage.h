#pragma once

#include "../models/Job.h"
#include "../models/JobConfig.h"
#include "../models/JobResult.h"
#include "../../infrastructure.h"
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/instance.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <memory>
#include <vector>
#include <optional>
#include <unordered_map>
#include <functional>

namespace search_engine {
namespace storage {

/**
 * Query options for job searches
 */
struct JobQueryOptions {
    int limit = 100;                                        // Maximum number of results
    int skip = 0;                                          // Number of results to skip
    std::optional<std::string> sortField = "createdAt";   // Field to sort by
    bool sortDescending = true;                            // Sort direction
    std::unordered_map<std::string, std::string> filters; // Additional filters
    
    JobQueryOptions() = default;
    JobQueryOptions(int limit, int skip = 0) : limit(limit), skip(skip) {}
};

/**
 * Batch operation result
 */
template<typename T>
struct BatchResult {
    std::vector<T> successful;    // Successfully processed items
    std::vector<std::string> failed;  // IDs of failed items
    std::vector<std::string> errors;  // Error messages for failed items
    
    bool hasFailures() const { return !failed.empty(); }
    size_t successCount() const { return successful.size(); }
    size_t failureCount() const { return failed.size(); }
    double successRate() const { 
        auto total = successCount() + failureCount();
        return total > 0 ? static_cast<double>(successCount()) / total : 0.0;
    }
};

/**
 * Job storage statistics
 */
struct JobStorageStats {
    int64_t totalJobs = 0;
    int64_t queuedJobs = 0;
    int64_t processingJobs = 0;
    int64_t completedJobs = 0;
    int64_t failedJobs = 0;
    int64_t cancelledJobs = 0;
    std::unordered_map<std::string, int64_t> jobsByType;
    std::unordered_map<std::string, int64_t> jobsByUser;
    double averageExecutionTime = 0.0;
    int64_t totalResults = 0;
    size_t totalOutputSize = 0;
    
    JobStorageStats() = default;
};

/**
 * JobStorage class providing CRUD operations for jobs, job configurations, and job results
 * 
 * This class handles all database operations related to the universal job manager system.
 * It follows the established patterns from the existing storage classes and uses proper
 * MongoDB integration with connection pooling and error handling.
 */
class JobStorage {
private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection jobsCollection_;
    mongocxx::collection jobConfigsCollection_;
    mongocxx::collection jobResultsCollection_;
    mongocxx::collection jobQueueCollection_;
    mongocxx::collection jobMetricsCollection_;
    mongocxx::collection jobHistoryCollection_;
    
    // Collection names
    static constexpr const char* JOBS_COLLECTION = "jobs";
    static constexpr const char* JOB_CONFIGS_COLLECTION = "job_configs";
    static constexpr const char* JOB_RESULTS_COLLECTION = "job_results";
    static constexpr const char* JOB_QUEUE_COLLECTION = "job_queue";
    static constexpr const char* JOB_METRICS_COLLECTION = "job_metrics";
    static constexpr const char* JOB_HISTORY_COLLECTION = "job_history";

public:
    /**
     * Constructor with connection string and database name
     */
    explicit JobStorage(const std::string& connectionString = "mongodb://admin:password123@mongodb:27017",
                       const std::string& databaseName = "search-engine-jobs");
    
    /**
     * Destructor
     */
    ~JobStorage() = default;
    
    // Move constructor and assignment (delete copy operations for RAII)
    JobStorage(JobStorage&& other) noexcept = default;
    JobStorage& operator=(JobStorage&& other) noexcept = default;
    JobStorage(const JobStorage&) = delete;
    JobStorage& operator=(const JobStorage&) = delete;

    // ========================================
    // Job CRUD Operations
    // ========================================
    
    /**
     * Store a new job
     */
    Result<std::string> storeJob(const models::Job& job);
    
    /**
     * Retrieve a job by ID
     */
    Result<models::Job> getJob(const std::string& jobId);
    
    /**
     * Update an existing job
     */
    Result<bool> updateJob(const models::Job& job);
    
    /**
     * Delete a job by ID
     */
    Result<bool> deleteJob(const std::string& jobId);
    
    /**
     * Get jobs by user ID
     */
    Result<std::vector<models::Job>> getJobsByUser(const std::string& userId, 
                                                  const JobQueryOptions& options = {});
    
    /**
     * Get jobs by status
     */
    Result<std::vector<models::Job>> getJobsByStatus(models::JobStatus status,
                                                    const JobQueryOptions& options = {});
    
    /**
     * Get jobs by type
     */
    Result<std::vector<models::Job>> getJobsByType(const std::string& jobType,
                                                  const JobQueryOptions& options = {});
    
    /**
     * Get jobs by priority
     */
    Result<std::vector<models::Job>> getJobsByPriority(models::JobPriority priority,
                                                      const JobQueryOptions& options = {});
    
    /**
     * Search jobs with complex filters
     */
    Result<std::vector<models::Job>> searchJobs(const std::unordered_map<std::string, std::string>& filters,
                                               const JobQueryOptions& options = {});

    // ========================================
    // Job Configuration CRUD Operations
    // ========================================
    
    /**
     * Store a job configuration
     */
    Result<std::string> storeJobConfig(const models::JobConfig& config);
    
    /**
     * Retrieve a job configuration by job type
     */
    Result<models::JobConfig> getJobConfig(const std::string& jobType);
    
    /**
     * Update a job configuration
     */
    Result<bool> updateJobConfig(const models::JobConfig& config);
    
    /**
     * Delete a job configuration
     */
    Result<bool> deleteJobConfig(const std::string& jobType);
    
    /**
     * Get all job configurations
     */
    Result<std::vector<models::JobConfig>> getAllJobConfigs();
    
    /**
     * Get enabled job configurations
     */
    Result<std::vector<models::JobConfig>> getEnabledJobConfigs();

    // ========================================
    // Job Result CRUD Operations
    // ========================================
    
    /**
     * Store a job result
     */
    Result<std::string> storeJobResult(const models::JobResult& result);
    
    /**
     * Retrieve a job result by ID
     */
    Result<models::JobResult> getJobResult(const std::string& resultId);
    
    /**
     * Retrieve job result by job ID
     */
    Result<models::JobResult> getJobResultByJobId(const std::string& jobId);
    
    /**
     * Update a job result
     */
    Result<bool> updateJobResult(const models::JobResult& result);
    
    /**
     * Delete a job result
     */
    Result<bool> deleteJobResult(const std::string& resultId);
    
    /**
     * Get job results by user ID
     */
    Result<std::vector<models::JobResult>> getJobResultsByUser(const std::string& userId,
                                                              const JobQueryOptions& options = {});
    
    /**
     * Get job results by status
     */
    Result<std::vector<models::JobResult>> getJobResultsByStatus(models::JobStatus status,
                                                                const JobQueryOptions& options = {});

    // ========================================
    // Batch Operations
    // ========================================
    
    /**
     * Store multiple jobs in a batch
     */
    Result<BatchResult<std::string>> storeJobs(const std::vector<models::Job>& jobs);
    
    /**
     * Update multiple jobs in a batch
     */
    Result<BatchResult<bool>> updateJobs(const std::vector<models::Job>& jobs);
    
    /**
     * Store multiple job results in a batch
     */
    Result<BatchResult<std::string>> storeJobResults(const std::vector<models::JobResult>& results);

    // ========================================
    // Queue Management Operations
    // ========================================
    
    /**
     * Add job to queue
     */
    Result<bool> enqueueJob(const std::string& jobId, models::JobPriority priority = models::JobPriority::NORMAL,
                           const std::optional<std::chrono::system_clock::time_point>& scheduledAt = std::nullopt);
    
    /**
     * Get next job from queue
     */
    Result<std::optional<models::Job>> dequeueJob(const std::string& workerId = "");
    
    /**
     * Get queued jobs count
     */
    Result<int64_t> getQueuedJobCount();
    
    /**
     * Get queued jobs by priority
     */
    Result<std::vector<models::Job>> getQueuedJobsByPriority(models::JobPriority priority,
                                                            const JobQueryOptions& options = {});

    // ========================================
    // Statistics and Analytics
    // ========================================
    
    /**
     * Get comprehensive storage statistics
     */
    Result<JobStorageStats> getStorageStats();
    
    /**
     * Get job count by status
     */
    Result<int64_t> getJobCountByStatus(models::JobStatus status);
    
    /**
     * Get job count by type
     */
    Result<int64_t> getJobCountByType(const std::string& jobType);
    
    /**
     * Get job count by user
     */
    Result<int64_t> getJobCountByUser(const std::string& userId);
    
    /**
     * Get average execution time for completed jobs
     */
    Result<double> getAverageExecutionTime(const std::string& jobType = "");

    // ========================================
    // History and Audit Trail
    // ========================================
    
    /**
     * Record job history entry (status changes, etc.)
     */
    Result<bool> recordJobHistory(const std::string& jobId, const std::string& event,
                                 const std::string& details = "", const std::string& userId = "");
    
    /**
     * Get job history
     */
    Result<std::vector<std::unordered_map<std::string, std::string>>> getJobHistory(const std::string& jobId);

    // ========================================
    // Maintenance Operations
    // ========================================
    
    /**
     * Clean up expired jobs and results
     */
    Result<int64_t> cleanupExpiredData();
    
    /**
     * Clean up completed jobs older than specified days
     */
    Result<int64_t> cleanupOldCompletedJobs(int daysOld = 30);
    
    /**
     * Clean up failed jobs older than specified days
     */
    Result<int64_t> cleanupOldFailedJobs(int daysOld = 7);

    // ========================================
    // Connection and Index Management
    // ========================================
    
    /**
     * Test database connection
     */
    Result<bool> testConnection();
    
    /**
     * Create all necessary indexes
     */
    Result<bool> createIndexes();
    
    /**
     * Ensure indexes are up to date
     */
    Result<bool> ensureIndexes();

    // ========================================
    // Advanced Query Operations
    // ========================================
    
    /**
     * Execute custom aggregation pipeline
     */
    Result<std::vector<bsoncxx::document::value>> executeAggregation(
        const std::string& collectionName,
        const std::vector<bsoncxx::document::value>& pipeline);
    
    /**
     * Count documents with filters
     */
    Result<int64_t> countJobs(const std::unordered_map<std::string, std::string>& filters = {});
    
    /**
     * Find jobs with custom query
     */
    Result<std::vector<models::Job>> findJobs(const bsoncxx::document::view& query,
                                             const JobQueryOptions& options = {});

private:
    // ========================================
    // Private Helper Methods
    // ========================================
    
    /**
     * Build BSON filter from string map
     */
    bsoncxx::document::value buildFilter(const std::unordered_map<std::string, std::string>& filters);
    
    /**
     * Build sort document from query options
     */
    bsoncxx::document::value buildSort(const JobQueryOptions& options);
    
    /**
     * Convert MongoDB cursor to job vector
     */
    std::vector<models::Job> cursorToJobs(mongocxx::cursor& cursor);
    
    /**
     * Convert MongoDB cursor to job result vector
     */
    std::vector<models::JobResult> cursorToJobResults(mongocxx::cursor& cursor);
    
    /**
     * Convert MongoDB cursor to job config vector
     */
    std::vector<models::JobConfig> cursorToJobConfigs(mongocxx::cursor& cursor);
    
    /**
     * Log database operation
     */
    void logOperation(const std::string& operation, const std::string& details = "");
    
    /**
     * Handle MongoDB exceptions
     */
    template<typename T>
    Result<T> handleException(const std::function<T()>& operation, const std::string& context);
};

} // namespace storage
} // namespace search_engine
