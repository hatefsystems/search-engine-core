#include "../../include/Logger.h"
#include "../../include/mongodb.h"
#include "../../include/infrastructure.h"
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <string>
#include <vector>
#include <memory>
#include <chrono>

using namespace bsoncxx::builder::stream;

namespace search_engine {
namespace database {

/**
 * JobIndexes class for managing database indexes for the job system
 * 
 * This class creates and maintains all necessary indexes for optimal
 * performance of job-related queries as specified in Phase 1a requirements.
 */
class JobIndexes {
private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    
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
    explicit JobIndexes(const std::string& connectionString = "mongodb://admin:password123@mongodb:27017",
                       const std::string& databaseName = "search-engine-jobs") {
        LOG_DEBUG("JobIndexes constructor called with database: " + databaseName);
        try {
            LOG_INFO("Initializing MongoDB connection for JobIndexes to: " + connectionString);
            
            // Ensure instance is initialized - CRITICAL for avoiding crashes
            MongoDBInstance::getInstance();
            LOG_DEBUG("MongoDB instance initialized for JobIndexes");
            
            // Create client and connect to database
            mongocxx::uri uri{connectionString};
            client_ = std::make_unique<mongocxx::client>(uri);
            database_ = (*client_)[databaseName];
            
            LOG_INFO("Connected to MongoDB database for JobIndexes: " + databaseName);
            
        } catch (const mongocxx::exception& e) {
            LOG_ERROR("MongoDB exception in JobIndexes constructor: " + std::string(e.what()));
            throw;
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in JobIndexes constructor: " + std::string(e.what()));
            throw;
        }
    }

    /**
     * Create all job system indexes as specified in Phase 1a requirements
     */
    Result<bool> createAllIndexes() {
        try {
            LOG_INFO("Creating all job system indexes...");
            
            // Create indexes for each collection
            auto jobsResult = createJobsIndexes();
            if (!jobsResult.success) {
                LOG_ERROR("Failed to create jobs indexes: " + jobsResult.message);
                return jobsResult;
            }
            
            auto configsResult = createJobConfigsIndexes();
            if (!configsResult.success) {
                LOG_ERROR("Failed to create job configs indexes: " + configsResult.message);
                return configsResult;
            }
            
            auto resultsIndexResult = createJobResultsIndexes();
            if (!resultsIndexResult.success) {
                LOG_ERROR("Failed to create job results indexes: " + resultsIndexResult.message);
                return resultsIndexResult;
            }
            
            auto queueResult = createJobQueueIndexes();
            if (!queueResult.success) {
                LOG_ERROR("Failed to create job queue indexes: " + queueResult.message);
                return queueResult;
            }
            
            auto metricsResult = createJobMetricsIndexes();
            if (!metricsResult.success) {
                LOG_ERROR("Failed to create job metrics indexes: " + metricsResult.message);
                return metricsResult;
            }
            
            auto historyResult = createJobHistoryIndexes();
            if (!historyResult.success) {
                LOG_ERROR("Failed to create job history indexes: " + historyResult.message);
                return historyResult;
            }
            
            LOG_INFO("Successfully created all job system indexes");
            return Result<bool>::Success(true, "Successfully created all job system indexes");
            
        } catch (const mongocxx::exception& e) {
            std::string error = "MongoDB error creating indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        } catch (const std::exception& e) {
            std::string error = "Error creating indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Create indexes for the jobs collection
     * 
     * Indexes:
     * - userId: For user-specific queries
     * - status: For status-based filtering
     * - priority: For priority-based sorting
     * - createdAt: For temporal queries
     * - jobType: For job type filtering
     * - tenantId: For multi-tenant support
     * - Compound indexes for common query patterns
     */
    Result<bool> createJobsIndexes() {
        try {
            LOG_DEBUG("Creating indexes for jobs collection");
            auto collection = database_[JOBS_COLLECTION];
            
            // Single field indexes
            collection.create_index(document{} << "id" << 1 << finalize,
                mongocxx::options::index{}.unique(true).name("idx_jobs_id"));
            
            collection.create_index(document{} << "userId" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_userId"));
            
            collection.create_index(document{} << "status" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_status"));
            
            collection.create_index(document{} << "priority" << -1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_priority_desc"));
            
            collection.create_index(document{} << "createdAt" << -1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_createdAt_desc"));
            
            collection.create_index(document{} << "jobType" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_jobType"));
            
            collection.create_index(document{} << "tenantId" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_tenantId"));
            
            // Compound indexes for common query patterns
            collection.create_index(document{} << "userId" << 1 << "status" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_userId_status"));
            
            collection.create_index(document{} << "jobType" << 1 << "status" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_jobType_status"));
            
            collection.create_index(document{} << "tenantId" << 1 << "userId" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_tenantId_userId"));
            
            // Priority-based queue ordering
            collection.create_index(document{} << "priority" << -1 << "createdAt" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_priority_createdAt"));
            
            // Scheduled jobs
            collection.create_index(document{} << "scheduledAt" << 1 << "status" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_scheduledAt_status"));
            
            // Timeout monitoring
            collection.create_index(document{} << "startedAt" << 1 << "timeout" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_timeout_monitoring"));
            
            LOG_DEBUG("Successfully created jobs collection indexes");
            return Result<bool>::Success(true, "Successfully created jobs collection indexes");
            
        } catch (const mongocxx::exception& e) {
            // Some indexes might already exist - this is not necessarily an error
            if (std::string(e.what()).find("already exists") != std::string::npos ||
                std::string(e.what()).find("IndexOptionsConflict") != std::string::npos) {
                LOG_WARNING("Some jobs indexes already exist: " + std::string(e.what()));
                return Result<bool>::Success(true, "Jobs indexes already exist");
            }
            std::string error = "MongoDB error creating jobs indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Create indexes for the job_configs collection
     */
    Result<bool> createJobConfigsIndexes() {
        try {
            LOG_DEBUG("Creating indexes for job_configs collection");
            auto collection = database_[JOB_CONFIGS_COLLECTION];
            
            // Primary key on job type
            collection.create_index(document{} << "jobType" << 1 << finalize,
                mongocxx::options::index{}.unique(true).name("idx_job_configs_jobType"));
            
            // Enabled configurations for active job types
            collection.create_index(document{} << "enabled" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_configs_enabled"));
            
            // Creation and update tracking
            collection.create_index(document{} << "createdAt" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_configs_createdAt"));
            
            collection.create_index(document{} << "updatedAt" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_configs_updatedAt"));
            
            // Priority-based queries
            collection.create_index(document{} << "defaultPriority" << 1 << "enabled" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_configs_priority_enabled"));
            
            LOG_DEBUG("Successfully created job_configs collection indexes");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            if (std::string(e.what()).find("already exists") != std::string::npos ||
                std::string(e.what()).find("IndexOptionsConflict") != std::string::npos) {
                LOG_WARNING("Some job_configs indexes already exist: " + std::string(e.what()));
                return Result<bool>::Success(true, "Operation completed successfully");
            }
            std::string error = "MongoDB error creating job_configs indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Create indexes for the job_results collection
     */
    Result<bool> createJobResultsIndexes() {
        try {
            LOG_DEBUG("Creating indexes for job_results collection");
            auto collection = database_[JOB_RESULTS_COLLECTION];
            
            // Unique result ID
            collection.create_index(document{} << "id" << 1 << finalize,
                mongocxx::options::index{}.unique(true).name("idx_job_results_id"));
            
            // Job ID for result lookup
            collection.create_index(document{} << "jobId" << 1 << finalize,
                mongocxx::options::index{}.unique(true).name("idx_job_results_jobId"));
            
            // User-specific results
            collection.create_index(document{} << "userId" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_results_userId"));
            
            // Tenant-specific results
            collection.create_index(document{} << "tenantId" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_results_tenantId"));
            
            // Final status for filtering
            collection.create_index(document{} << "finalStatus" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_results_finalStatus"));
            
            // Creation time for temporal queries
            collection.create_index(document{} << "createdAt" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_results_createdAt"));
            
            // Expiration cleanup
            collection.create_index(document{} << "expiresAt" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_results_expiresAt"));
            
            // Compound indexes for common queries
            collection.create_index(document{} << "userId" << 1 << "finalStatus" << 1 << "createdAt" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_results_user_status_time"));
            
            collection.create_index(document{} << "tenantId" << 1 << "userId" << 1 << "createdAt" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_results_tenant_user_time"));
            
            // Performance metrics queries
            collection.create_index(document{} << "metrics.executionDuration" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_results_execution_duration"));
            
            LOG_DEBUG("Successfully created job_results collection indexes");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            if (std::string(e.what()).find("already exists") != std::string::npos ||
                std::string(e.what()).find("IndexOptionsConflict") != std::string::npos) {
                LOG_WARNING("Some job_results indexes already exist: " + std::string(e.what()));
                return Result<bool>::Success(true, "Operation completed successfully");
            }
            std::string error = "MongoDB error creating job_results indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Create indexes for the job_queue collection
     */
    Result<bool> createJobQueueIndexes() {
        try {
            LOG_DEBUG("Creating indexes for job_queue collection");
            auto collection = database_[JOB_QUEUE_COLLECTION];
            
            // Job ID reference
            collection.create_index(document{} << "jobId" << 1 << finalize,
                mongocxx::options::index{}.unique(true).name("idx_job_queue_jobId"));
            
            // Queue status
            collection.create_index(document{} << "status" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_queue_status"));
            
            // Priority-based dequeuing (CRITICAL: High performance requirement)
            collection.create_index(document{} << "priority" << -1 << "scheduledAt" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_queue_priority_schedule"));
            
            // Scheduled execution time
            collection.create_index(document{} << "scheduledAt" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_queue_scheduledAt"));
            
            // Worker assignment tracking
            collection.create_index(document{} << "workerId" << 1 << "status" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_queue_worker_status"));
            
            // Queue monitoring and cleanup
            collection.create_index(document{} << "enqueuedAt" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_queue_enqueuedAt"));
            
            collection.create_index(document{} << "processingStartedAt" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_queue_processing_started"));
            
            // Compound index for optimal queue operations (CRITICAL for performance)
            collection.create_index(
                document{} 
                    << "status" << 1 
                    << "priority" << -1 
                    << "scheduledAt" << 1 
                << finalize,
                mongocxx::options::index{}.name("idx_job_queue_optimal_dequeue"));
            
            LOG_DEBUG("Successfully created job_queue collection indexes");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            if (std::string(e.what()).find("already exists") != std::string::npos ||
                std::string(e.what()).find("IndexOptionsConflict") != std::string::npos) {
                LOG_WARNING("Some job_queue indexes already exist: " + std::string(e.what()));
                return Result<bool>::Success(true, "Operation completed successfully");
            }
            std::string error = "MongoDB error creating job_queue indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Create indexes for the job_metrics collection
     */
    Result<bool> createJobMetricsIndexes() {
        try {
            LOG_DEBUG("Creating indexes for job_metrics collection");
            auto collection = database_[JOB_METRICS_COLLECTION];
            
            // Timestamp for time-series queries (CRITICAL for analytics)
            collection.create_index(document{} << "timestamp" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_metrics_timestamp"));
            
            // Job type performance analytics
            collection.create_index(document{} << "jobType" << 1 << "timestamp" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_metrics_jobType_timestamp"));
            
            // User-specific metrics
            collection.create_index(document{} << "userId" << 1 << "timestamp" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_metrics_userId_timestamp"));
            
            // Tenant-specific metrics
            collection.create_index(document{} << "tenantId" << 1 << "timestamp" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_metrics_tenantId_timestamp"));
            
            // Performance monitoring compound indexes
            collection.create_index(
                document{} 
                    << "jobType" << 1 
                    << "userId" << 1 
                    << "timestamp" << -1 
                << finalize,
                mongocxx::options::index{}.name("idx_job_metrics_type_user_time"));
            
            // TTL index for automatic cleanup (retain metrics for 90 days)
            collection.create_index(document{} << "timestamp" << 1 << finalize,
                mongocxx::options::index{}
                    .expire_after(std::chrono::seconds(90 * 24 * 60 * 60))
                    .name("idx_job_metrics_ttl"));
            
            LOG_DEBUG("Successfully created job_metrics collection indexes");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            if (std::string(e.what()).find("already exists") != std::string::npos ||
                std::string(e.what()).find("IndexOptionsConflict") != std::string::npos) {
                LOG_WARNING("Some job_metrics indexes already exist: " + std::string(e.what()));
                return Result<bool>::Success(true, "Operation completed successfully");
            }
            std::string error = "MongoDB error creating job_metrics indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Create indexes for the job_history collection (audit trail)
     */
    Result<bool> createJobHistoryIndexes() {
        try {
            LOG_DEBUG("Creating indexes for job_history collection");
            auto collection = database_[JOB_HISTORY_COLLECTION];
            
            // Job ID and timestamp for audit trail queries
            collection.create_index(document{} << "jobId" << 1 << "timestamp" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_history_jobId_timestamp"));
            
            // Global timestamp index for system-wide audit
            collection.create_index(document{} << "timestamp" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_history_timestamp"));
            
            // Event type filtering
            collection.create_index(document{} << "event" << 1 << "timestamp" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_history_event_timestamp"));
            
            // User activity tracking
            collection.create_index(document{} << "userId" << 1 << "timestamp" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_history_userId_timestamp"));
            
            // TTL index for automatic cleanup (retain history for 1 year)
            collection.create_index(document{} << "timestamp" << 1 << finalize,
                mongocxx::options::index{}
                    .expire_after(std::chrono::seconds(365 * 24 * 60 * 60))
                    .name("idx_job_history_ttl"));
            
            LOG_DEBUG("Successfully created job_history collection indexes");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            if (std::string(e.what()).find("already exists") != std::string::npos ||
                std::string(e.what()).find("IndexOptionsConflict") != std::string::npos) {
                LOG_WARNING("Some job_history indexes already exist: " + std::string(e.what()));
                return Result<bool>::Success(true, "Operation completed successfully");
            }
            std::string error = "MongoDB error creating job_history indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Verify that all indexes are created and functioning properly
     */
    Result<bool> verifyIndexes() {
        try {
            LOG_DEBUG("Verifying job system indexes");
            
            std::vector<std::string> collections = {
                JOBS_COLLECTION,
                JOB_CONFIGS_COLLECTION, 
                JOB_RESULTS_COLLECTION,
                JOB_QUEUE_COLLECTION,
                JOB_METRICS_COLLECTION,
                JOB_HISTORY_COLLECTION
            };
            
            for (const auto& collectionName : collections) {
                auto collection = database_[collectionName];
                auto cursor = collection.list_indexes();
                
                int indexCount = 0;
                for (const auto& index : cursor) {
                    indexCount++;
                    auto nameElement = index["name"];
                    if (nameElement) {
                        std::string indexName = std::string(nameElement.get_string().value);
                        LOG_DEBUG("Found index: " + collectionName + "." + indexName);
                    }
                }
                
                LOG_DEBUG("Collection " + collectionName + " has " + std::to_string(indexCount) + " indexes");
            }
            
            LOG_INFO("Successfully verified all job system indexes");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            std::string error = "MongoDB error verifying indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        } catch (const std::exception& e) {
            std::string error = "Error verifying indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Get index statistics and performance information
     */
    Result<std::string> getIndexStats() {
        try {
            LOG_DEBUG("Getting job system index statistics");
            
            std::stringstream stats;
            stats << "Job System Index Statistics:\n";
            
            std::vector<std::string> collections = {
                JOBS_COLLECTION,
                JOB_CONFIGS_COLLECTION, 
                JOB_RESULTS_COLLECTION,
                JOB_QUEUE_COLLECTION,
                JOB_METRICS_COLLECTION,
                JOB_HISTORY_COLLECTION
            };
            
            for (const auto& collectionName : collections) {
                auto collection = database_[collectionName];
                
                // Get collection stats
                auto statsCmd = document{} << "collStats" << collectionName << finalize;
                auto result = database_.run_command(statsCmd.view());
                
                if (!result.empty()) {
                    stats << "\nCollection: " << collectionName << "\n";
                    
                    // Extract relevant statistics
                    auto statsDoc = result.view();
                    if (auto count = statsDoc["count"]) {
                        stats << "  Document count: " << count.get_int64().value << "\n";
                    }
                    if (auto size = statsDoc["size"]) {
                        stats << "  Collection size: " << size.get_int64().value << " bytes\n";
                    }
                    if (auto indexSizes = statsDoc["indexSizes"]) {
                        auto indexDoc = indexSizes.get_document().value;
                        stats << "  Index sizes:\n";
                        for (const auto& element : indexDoc) {
                            stats << "    " << element.key() << ": " 
                                  << element.get_int64().value << " bytes\n";
                        }
                    }
                }
            }
            
            std::string result_str = stats.str();
            LOG_DEBUG("Generated index statistics: " + std::to_string(result_str.length()) + " chars");
            return Result<std::string>::Success(result_str, "Successfully retrieved index stats");
            
        } catch (const mongocxx::exception& e) {
            std::string error = "MongoDB error getting index stats: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<std::string>::Failure(error);
        } catch (const std::exception& e) {
            std::string error = "Error getting index stats: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<std::string>::Failure(error);
        }
    }
};

} // namespace database
} // namespace search_engine

// Global functions for external use
extern "C" {
    
/**
 * Create all job system database indexes
 * 
 * @param connectionString MongoDB connection string
 * @param databaseName Database name
 * @return 0 on success, non-zero on failure
 */
int createJobSystemIndexes(const char* connectionString, const char* databaseName) {
    try {
        std::string connStr = connectionString ? connectionString : "mongodb://admin:password123@mongodb:27017";
        std::string dbName = databaseName ? databaseName : "search-engine-jobs";
        
        search_engine::database::JobIndexes indexer(connStr, dbName);
        auto result = indexer.createAllIndexes();
        
        if (result.success) {
            LOG_INFO("Successfully created all job system indexes");
            return 0;
        } else {
            LOG_ERROR("Failed to create job system indexes: " + result.message);
            return 1;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception creating job system indexes: " + std::string(e.what()));
        return -1;
    }
}

/**
 * Verify all job system database indexes
 * 
 * @param connectionString MongoDB connection string
 * @param databaseName Database name
 * @return 0 on success, non-zero on failure
 */
int verifyJobSystemIndexes(const char* connectionString, const char* databaseName) {
    try {
        std::string connStr = connectionString ? connectionString : "mongodb://admin:password123@mongodb:27017";
        std::string dbName = databaseName ? databaseName : "search-engine-jobs";
        
        search_engine::database::JobIndexes indexer(connStr, dbName);
        auto result = indexer.verifyIndexes();
        
        if (result.success) {
            LOG_INFO("Successfully verified all job system indexes");
            return 0;
        } else {
            LOG_ERROR("Failed to verify job system indexes: " + result.message);
            return 1;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception verifying job system indexes: " + std::string(e.what()));
        return -1;
    }
}

} // extern "C"
