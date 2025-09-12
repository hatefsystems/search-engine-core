#include "search_engine/storage/JobStorage.h"
#include "Logger.h"
#include "mongodb.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/oid.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <chrono>
#include <algorithm>

using namespace bsoncxx::builder::stream;
using namespace search_engine::storage;
using namespace search_engine::models;

namespace {
    // Helper function to convert time_point to BSON date
    bsoncxx::types::b_date timePointToBsonDate(const std::chrono::system_clock::time_point& tp) {
        auto duration = tp.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        return bsoncxx::types::b_date{std::chrono::milliseconds{millis}};
    }
    
    // Helper function to convert BSON date to time_point
    std::chrono::system_clock::time_point bsonDateToTimePoint(const bsoncxx::types::b_date& date) {
        return std::chrono::system_clock::time_point{date.value};
    }
}

JobStorage::JobStorage(const std::string& connectionString, const std::string& databaseName) {
    LOG_DEBUG("JobStorage constructor called with database: " + databaseName);
    try {
        LOG_INFO("Initializing MongoDB connection for JobStorage to: " + connectionString);
        
        // Ensure instance is initialized - CRITICAL for avoiding crashes
        MongoDBInstance::getInstance();
        LOG_DEBUG("MongoDB instance initialized for JobStorage");
        
        // Create client and connect to database
        mongocxx::uri uri{connectionString};
        client_ = std::make_unique<mongocxx::client>(uri);
        database_ = (*client_)[databaseName];
        
        // Initialize collections
        jobsCollection_ = database_[JOBS_COLLECTION];
        jobConfigsCollection_ = database_[JOB_CONFIGS_COLLECTION];
        jobResultsCollection_ = database_[JOB_RESULTS_COLLECTION];
        jobQueueCollection_ = database_[JOB_QUEUE_COLLECTION];
        jobMetricsCollection_ = database_[JOB_METRICS_COLLECTION];
        jobHistoryCollection_ = database_[JOB_HISTORY_COLLECTION];
        
        LOG_INFO("Connected to MongoDB database for JobStorage: " + databaseName);
        
        // Test connection and create indexes
        auto testResult = testConnection();
        if (!testResult.success) {
            LOG_ERROR("Failed to establish JobStorage connection: " + testResult.message);
            throw std::runtime_error("JobStorage connection test failed");
        }
        
        auto indexResult = ensureIndexes();
        if (!indexResult.success) {
            LOG_WARNING("Failed to ensure JobStorage indexes: " + indexResult.message);
        }
        
        LOG_INFO("JobStorage initialized successfully");
        
    } catch (const mongocxx::exception& e) {
        LOG_ERROR("MongoDB exception in JobStorage constructor: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in JobStorage constructor: " + std::string(e.what()));
        throw;
    }
}

// ========================================
// Job CRUD Operations
// ========================================

Result<std::string> JobStorage::storeJob(const Job& job) {
    return handleException<std::string>([&]() -> std::string {
        logOperation("storeJob", "jobId: " + job.getId() + ", type: " + job.getJobType());
        
        // Validate job before storing
        auto validationResult = job.validate();
        if (!validationResult.success) {
            throw std::runtime_error("Job validation failed: " + validationResult.message);
        }
        
        auto bsonDoc = job.toBson();
        auto result = jobsCollection_.insert_one(bsonDoc.view());
        
        if (!result) {
            throw std::runtime_error("Failed to insert job into database");
        }
        
        // Record history entry
        recordJobHistory(job.getId(), "CREATED", "Job created with type: " + job.getJobType(), job.getUserId());
        
        LOG_DEBUG("Successfully stored job: " + job.getId());
        return job.getId();
        
    }, "storeJob");
}

Result<Job> JobStorage::getJob(const std::string& jobId) {
    return handleException<Job>([&]() -> Job {
        logOperation("getJob", "jobId: " + jobId);
        
        auto filter = document{} << "id" << jobId << finalize;
        auto result = jobsCollection_.find_one(filter.view());
        
        if (!result) {
            throw std::runtime_error("Job not found: " + jobId);
        }
        
        auto job = Job::fromBson(result->view());
        LOG_DEBUG("Successfully retrieved job: " + jobId);
        return job;
        
    }, "getJob");
}

Result<bool> JobStorage::updateJob(const Job& job) {
    return handleException<bool>([&]() -> bool {
        logOperation("updateJob", "jobId: " + job.getId());
        
        // Validate job before updating
        auto validationResult = job.validate();
        if (!validationResult.success) {
            throw std::runtime_error("Job validation failed: " + validationResult.message);
        }
        
        auto filter = document{} << "id" << job.getId() << finalize;
        auto bsonDoc = job.toBson();
        auto result = jobsCollection_.replace_one(filter.view(), bsonDoc.view());
        
        if (!result || result->modified_count() == 0) {
            throw std::runtime_error("Job not found or not modified: " + job.getId());
        }
        
        // Record history entry
        recordJobHistory(job.getId(), "UPDATED", "Job status: " + Job::statusToString(job.getStatus()), job.getUserId());
        
        LOG_DEBUG("Successfully updated job: " + job.getId());
        return true;
        
    }, "updateJob");
}

Result<bool> JobStorage::deleteJob(const std::string& jobId) {
    return handleException<bool>([&]() -> bool {
        logOperation("deleteJob", "jobId: " + jobId);
        
        auto filter = document{} << "id" << jobId << finalize;
        auto result = jobsCollection_.delete_one(filter.view());
        
        if (!result || result->deleted_count() == 0) {
            throw std::runtime_error("Job not found: " + jobId);
        }
        
        // Also delete associated result and queue entries
        auto resultFilter = document{} << "jobId" << jobId << finalize;
        jobResultsCollection_.delete_many(resultFilter.view());
        jobQueueCollection_.delete_many(resultFilter.view());
        
        // Record history entry
        recordJobHistory(jobId, "DELETED", "Job deleted from storage");
        
        LOG_DEBUG("Successfully deleted job: " + jobId);
        return true;
        
    }, "deleteJob");
}

Result<std::vector<Job>> JobStorage::getJobsByUser(const std::string& userId, const JobQueryOptions& options) {
    return handleException<std::vector<Job>>([&]() -> std::vector<Job> {
        logOperation("getJobsByUser", "userId: " + userId + ", limit: " + std::to_string(options.limit));
        
        auto filter = document{} << "userId" << userId << finalize;
        auto sortDoc = buildSort(options);
        
        mongocxx::options::find findOptions;
        findOptions.limit(options.limit);
        findOptions.skip(options.skip);
        findOptions.sort(sortDoc.view());
        
        auto cursor = jobsCollection_.find(filter.view(), findOptions);
        auto jobs = cursorToJobs(cursor);
        
        LOG_DEBUG("Retrieved " + std::to_string(jobs.size()) + " jobs for user: " + userId);
        return jobs;
        
    }, "getJobsByUser");
}

Result<std::vector<Job>> JobStorage::getJobsByStatus(JobStatus status, const JobQueryOptions& options) {
    return handleException<std::vector<Job>>([&]() -> std::vector<Job> {
        logOperation("getJobsByStatus", "status: " + Job::statusToString(status));
        
        auto filter = document{} << "status" << Job::statusToString(status) << finalize;
        auto sortDoc = buildSort(options);
        
        mongocxx::options::find findOptions;
        findOptions.limit(options.limit);
        findOptions.skip(options.skip);
        findOptions.sort(sortDoc.view());
        
        auto cursor = jobsCollection_.find(filter.view(), findOptions);
        auto jobs = cursorToJobs(cursor);
        
        LOG_DEBUG("Retrieved " + std::to_string(jobs.size()) + " jobs with status: " + Job::statusToString(status));
        return jobs;
        
    }, "getJobsByStatus");
}

Result<std::vector<Job>> JobStorage::getJobsByType(const std::string& jobType, const JobQueryOptions& options) {
    return handleException<std::vector<Job>>([&]() -> std::vector<Job> {
        logOperation("getJobsByType", "jobType: " + jobType);
        
        auto filter = document{} << "jobType" << jobType << finalize;
        auto sortDoc = buildSort(options);
        
        mongocxx::options::find findOptions;
        findOptions.limit(options.limit);
        findOptions.skip(options.skip);
        findOptions.sort(sortDoc.view());
        
        auto cursor = jobsCollection_.find(filter.view(), findOptions);
        auto jobs = cursorToJobs(cursor);
        
        LOG_DEBUG("Retrieved " + std::to_string(jobs.size()) + " jobs of type: " + jobType);
        return jobs;
        
    }, "getJobsByType");
}

Result<std::vector<Job>> JobStorage::getJobsByPriority(JobPriority priority, const JobQueryOptions& options) {
    return handleException<std::vector<Job>>([&]() -> std::vector<Job> {
        logOperation("getJobsByPriority", "priority: " + Job::priorityToString(priority));
        
        auto filter = document{} << "priority" << Job::priorityToString(priority) << finalize;
        auto sortDoc = buildSort(options);
        
        mongocxx::options::find findOptions;
        findOptions.limit(options.limit);
        findOptions.skip(options.skip);
        findOptions.sort(sortDoc.view());
        
        auto cursor = jobsCollection_.find(filter.view(), findOptions);
        auto jobs = cursorToJobs(cursor);
        
        LOG_DEBUG("Retrieved " + std::to_string(jobs.size()) + " jobs with priority: " + Job::priorityToString(priority));
        return jobs;
        
    }, "getJobsByPriority");
}

Result<std::vector<Job>> JobStorage::searchJobs(const std::unordered_map<std::string, std::string>& filters, const JobQueryOptions& options) {
    return handleException<std::vector<Job>>([&]() -> std::vector<Job> {
        logOperation("searchJobs", "filters: " + std::to_string(filters.size()));
        
        auto filterDoc = buildFilter(filters);
        auto sortDoc = buildSort(options);
        
        mongocxx::options::find findOptions;
        findOptions.limit(options.limit);
        findOptions.skip(options.skip);
        findOptions.sort(sortDoc.view());
        
        auto cursor = jobsCollection_.find(filterDoc.view(), findOptions);
        auto jobs = cursorToJobs(cursor);
        
        LOG_DEBUG("Search returned " + std::to_string(jobs.size()) + " jobs");
        return jobs;
        
    }, "searchJobs");
}

// ========================================
// Job Configuration CRUD Operations
// ========================================

Result<std::string> JobStorage::storeJobConfig(const JobConfig& config) {
    return handleException<std::string>([&]() -> std::string {
        logOperation("storeJobConfig", "jobType: " + config.getJobType());
        
        // Validate config before storing
        auto validationResult = config.validate();
        if (!validationResult.success) {
            throw std::runtime_error("JobConfig validation failed: " + validationResult.message);
        }
        
        auto bsonDoc = config.toBson();
        
        // Use upsert to replace existing config
        auto filter = document{} << "jobType" << config.getJobType() << finalize;
        mongocxx::options::replace replaceOptions;
        replaceOptions.upsert(true);
        
        auto result = jobConfigsCollection_.replace_one(filter.view(), bsonDoc.view(), replaceOptions);
        
        if (!result) {
            throw std::runtime_error("Failed to store job config");
        }
        
        LOG_DEBUG("Successfully stored job config: " + config.getJobType());
        return config.getJobType();
        
    }, "storeJobConfig");
}

Result<JobConfig> JobStorage::getJobConfig(const std::string& jobType) {
    return handleException<JobConfig>([&]() -> JobConfig {
        logOperation("getJobConfig", "jobType: " + jobType);
        
        auto filter = document{} << "jobType" << jobType << finalize;
        auto result = jobConfigsCollection_.find_one(filter.view());
        
        if (!result) {
            throw std::runtime_error("Job config not found: " + jobType);
        }
        
        auto config = JobConfig::fromBson(result->view());
        LOG_DEBUG("Successfully retrieved job config: " + jobType);
        return config;
        
    }, "getJobConfig");
}

Result<bool> JobStorage::updateJobConfig(const JobConfig& config) {
    return handleException<bool>([&]() -> bool {
        logOperation("updateJobConfig", "jobType: " + config.getJobType());
        
        // Validate config before updating
        auto validationResult = config.validate();
        if (!validationResult.success) {
            throw std::runtime_error("JobConfig validation failed: " + validationResult.message);
        }
        
        auto filter = document{} << "jobType" << config.getJobType() << finalize;
        auto bsonDoc = config.toBson();
        auto result = jobConfigsCollection_.replace_one(filter.view(), bsonDoc.view());
        
        if (!result || result->modified_count() == 0) {
            throw std::runtime_error("Job config not found or not modified: " + config.getJobType());
        }
        
        LOG_DEBUG("Successfully updated job config: " + config.getJobType());
        return true;
        
    }, "updateJobConfig");
}

Result<bool> JobStorage::deleteJobConfig(const std::string& jobType) {
    return handleException<bool>([&]() -> bool {
        logOperation("deleteJobConfig", "jobType: " + jobType);
        
        auto filter = document{} << "jobType" << jobType << finalize;
        auto result = jobConfigsCollection_.delete_one(filter.view());
        
        if (!result || result->deleted_count() == 0) {
            throw std::runtime_error("Job config not found: " + jobType);
        }
        
        LOG_DEBUG("Successfully deleted job config: " + jobType);
        return true;
        
    }, "deleteJobConfig");
}

Result<std::vector<JobConfig>> JobStorage::getAllJobConfigs() {
    return handleException<std::vector<JobConfig>>([&]() -> std::vector<JobConfig> {
        logOperation("getAllJobConfigs", "");
        
        auto cursor = jobConfigsCollection_.find({});
        auto configs = cursorToJobConfigs(cursor);
        
        LOG_DEBUG("Retrieved " + std::to_string(configs.size()) + " job configs");
        return configs;
        
    }, "getAllJobConfigs");
}

Result<std::vector<JobConfig>> JobStorage::getEnabledJobConfigs() {
    return handleException<std::vector<JobConfig>>([&]() -> std::vector<JobConfig> {
        logOperation("getEnabledJobConfigs", "");
        
        auto filter = document{} << "enabled" << true << finalize;
        auto cursor = jobConfigsCollection_.find(filter.view());
        auto configs = cursorToJobConfigs(cursor);
        
        LOG_DEBUG("Retrieved " + std::to_string(configs.size()) + " enabled job configs");
        return configs;
        
    }, "getEnabledJobConfigs");
}

// ========================================
// Job Result CRUD Operations
// ========================================

Result<std::string> JobStorage::storeJobResult(const JobResult& result) {
    return handleException<std::string>([&]() -> std::string {
        logOperation("storeJobResult", "resultId: " + result.getId() + ", jobId: " + result.getJobId());
        
        // Validate result before storing
        auto validationResult = result.validate();
        if (!validationResult.success) {
            throw std::runtime_error("JobResult validation failed: " + validationResult.message);
        }
        
        auto bsonDoc = result.toBson();
        auto insertResult = jobResultsCollection_.insert_one(bsonDoc.view());
        
        if (!insertResult) {
            throw std::runtime_error("Failed to insert job result into database");
        }
        
        LOG_DEBUG("Successfully stored job result: " + result.getId());
        return result.getId();
        
    }, "storeJobResult");
}

Result<JobResult> JobStorage::getJobResult(const std::string& resultId) {
    return handleException<JobResult>([&]() -> JobResult {
        logOperation("getJobResult", "resultId: " + resultId);
        
        auto filter = document{} << "id" << resultId << finalize;
        auto result = jobResultsCollection_.find_one(filter.view());
        
        if (!result) {
            throw std::runtime_error("Job result not found: " + resultId);
        }
        
        auto jobResult = JobResult::fromBson(result->view());
        LOG_DEBUG("Successfully retrieved job result: " + resultId);
        return jobResult;
        
    }, "getJobResult");
}

Result<JobResult> JobStorage::getJobResultByJobId(const std::string& jobId) {
    return handleException<JobResult>([&]() -> JobResult {
        logOperation("getJobResultByJobId", "jobId: " + jobId);
        
        auto filter = document{} << "jobId" << jobId << finalize;
        auto result = jobResultsCollection_.find_one(filter.view());
        
        if (!result) {
            throw std::runtime_error("Job result not found for job: " + jobId);
        }
        
        auto jobResult = JobResult::fromBson(result->view());
        LOG_DEBUG("Successfully retrieved job result for job: " + jobId);
        return jobResult;
        
    }, "getJobResultByJobId");
}

Result<bool> JobStorage::updateJobResult(const JobResult& result) {
    return handleException<bool>([&]() -> bool {
        logOperation("updateJobResult", "resultId: " + result.getId());
        
        // Validate result before updating
        auto validationResult = result.validate();
        if (!validationResult.success) {
            throw std::runtime_error("JobResult validation failed: " + validationResult.message);
        }
        
        auto filter = document{} << "id" << result.getId() << finalize;
        auto bsonDoc = result.toBson();
        auto updateResult = jobResultsCollection_.replace_one(filter.view(), bsonDoc.view());
        
        if (!updateResult || updateResult->modified_count() == 0) {
            throw std::runtime_error("Job result not found or not modified: " + result.getId());
        }
        
        LOG_DEBUG("Successfully updated job result: " + result.getId());
        return true;
        
    }, "updateJobResult");
}

Result<bool> JobStorage::deleteJobResult(const std::string& resultId) {
    return handleException<bool>([&]() -> bool {
        logOperation("deleteJobResult", "resultId: " + resultId);
        
        auto filter = document{} << "id" << resultId << finalize;
        auto result = jobResultsCollection_.delete_one(filter.view());
        
        if (!result || result->deleted_count() == 0) {
            throw std::runtime_error("Job result not found: " + resultId);
        }
        
        LOG_DEBUG("Successfully deleted job result: " + resultId);
        return true;
        
    }, "deleteJobResult");
}

Result<std::vector<JobResult>> JobStorage::getJobResultsByUser(const std::string& userId, const JobQueryOptions& options) {
    return handleException<std::vector<JobResult>>([&]() -> std::vector<JobResult> {
        logOperation("getJobResultsByUser", "userId: " + userId);
        
        auto filter = document{} << "userId" << userId << finalize;
        auto sortDoc = buildSort(options);
        
        mongocxx::options::find findOptions;
        findOptions.limit(options.limit);
        findOptions.skip(options.skip);
        findOptions.sort(sortDoc.view());
        
        auto cursor = jobResultsCollection_.find(filter.view(), findOptions);
        auto results = cursorToJobResults(cursor);
        
        LOG_DEBUG("Retrieved " + std::to_string(results.size()) + " job results for user: " + userId);
        return results;
        
    }, "getJobResultsByUser");
}

Result<std::vector<JobResult>> JobStorage::getJobResultsByStatus(JobStatus status, const JobQueryOptions& options) {
    return handleException<std::vector<JobResult>>([&]() -> std::vector<JobResult> {
        logOperation("getJobResultsByStatus", "status: " + Job::statusToString(status));
        
        auto filter = document{} << "finalStatus" << Job::statusToString(status) << finalize;
        auto sortDoc = buildSort(options);
        
        mongocxx::options::find findOptions;
        findOptions.limit(options.limit);
        findOptions.skip(options.skip);
        findOptions.sort(sortDoc.view());
        
        auto cursor = jobResultsCollection_.find(filter.view(), findOptions);
        auto results = cursorToJobResults(cursor);
        
        LOG_DEBUG("Retrieved " + std::to_string(results.size()) + " job results with status: " + Job::statusToString(status));
        return results;
        
    }, "getJobResultsByStatus");
}

// ========================================
// Connection and Index Management
// ========================================

Result<bool> JobStorage::testConnection() {
    return handleException<bool>([&]() -> bool {
        logOperation("testConnection", "");
        
        // Test basic connectivity
        auto result = database_.run_command(document{} << "ping" << 1 << finalize);
        if (result.empty()) {
            throw std::runtime_error("Database ping failed");
        }
        
        LOG_DEBUG("JobStorage connection test successful");
        return true;
        
    }, "testConnection");
}

Result<bool> JobStorage::createIndexes() {
    return handleException<bool>([&]() -> bool {
        logOperation("createIndexes", "");
        
        try {
            // Jobs collection indexes
            jobsCollection_.create_index(document{} << "id" << 1 << finalize);
            jobsCollection_.create_index(document{} << "userId" << 1 << finalize);
            jobsCollection_.create_index(document{} << "status" << 1 << finalize);
            jobsCollection_.create_index(document{} << "priority" << 1 << finalize);
            jobsCollection_.create_index(document{} << "createdAt" << 1 << finalize);
            jobsCollection_.create_index(document{} << "jobType" << 1 << finalize);
            jobsCollection_.create_index(document{} << "tenantId" << 1 << finalize);
            
            // Compound indexes for common queries
            jobsCollection_.create_index(document{} << "userId" << 1 << "status" << 1 << finalize);
            jobsCollection_.create_index(document{} << "jobType" << 1 << "status" << 1 << finalize);
            jobsCollection_.create_index(document{} << "priority" << -1 << "createdAt" << 1 << finalize);
            
            // Job configs collection indexes
            jobConfigsCollection_.create_index(document{} << "jobType" << 1 << finalize);
            jobConfigsCollection_.create_index(document{} << "enabled" << 1 << finalize);
            
            // Job results collection indexes
            jobResultsCollection_.create_index(document{} << "id" << 1 << finalize);
            jobResultsCollection_.create_index(document{} << "jobId" << 1 << finalize);
            jobResultsCollection_.create_index(document{} << "userId" << 1 << finalize);
            jobResultsCollection_.create_index(document{} << "finalStatus" << 1 << finalize);
            jobResultsCollection_.create_index(document{} << "createdAt" << 1 << finalize);
            
            // Job queue collection indexes
            jobQueueCollection_.create_index(document{} << "priority" << -1 << "scheduledAt" << 1 << finalize);
            jobQueueCollection_.create_index(document{} << "status" << 1 << finalize);
            jobQueueCollection_.create_index(document{} << "jobId" << 1 << finalize);
            
            // Job history collection indexes
            jobHistoryCollection_.create_index(document{} << "jobId" << 1 << "timestamp" << -1 << finalize);
            jobHistoryCollection_.create_index(document{} << "timestamp" << -1 << finalize);
            
            // Job metrics collection indexes
            jobMetricsCollection_.create_index(document{} << "timestamp" << -1 << finalize);
            jobMetricsCollection_.create_index(document{} << "jobType" << 1 << finalize);
            jobMetricsCollection_.create_index(document{} << "userId" << 1 << finalize);
            
            LOG_INFO("Successfully created all JobStorage indexes");
            return true;
            
        } catch (const mongocxx::exception& e) {
            LOG_WARNING("Some indexes may already exist: " + std::string(e.what()));
            return true; // Indexes already existing is not an error
        }
        
    }, "createIndexes");
}

Result<bool> JobStorage::ensureIndexes() {
    return createIndexes(); // For now, just call createIndexes
}

// ========================================
// Private Helper Methods
// ========================================

bsoncxx::document::value JobStorage::buildFilter(const std::unordered_map<std::string, std::string>& filters) {
    auto builder = document{};
    
    for (const auto& [key, value] : filters) {
        builder << key << value;
    }
    
    return builder << finalize;
}

bsoncxx::document::value JobStorage::buildSort(const JobQueryOptions& options) {
    auto builder = document{};
    
    if (options.sortField.has_value()) {
        builder << options.sortField.value() << (options.sortDescending ? -1 : 1);
    } else {
        builder << "createdAt" << -1; // Default sort
    }
    
    return builder << finalize;
}

std::vector<Job> JobStorage::cursorToJobs(mongocxx::cursor& cursor) {
    std::vector<Job> jobs;
    
    for (const auto& doc : cursor) {
        try {
            jobs.push_back(Job::fromBson(doc));
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse job from BSON: " + std::string(e.what()));
        }
    }
    
    return jobs;
}

std::vector<JobResult> JobStorage::cursorToJobResults(mongocxx::cursor& cursor) {
    std::vector<JobResult> results;
    
    for (const auto& doc : cursor) {
        try {
            results.push_back(JobResult::fromBson(doc));
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse job result from BSON: " + std::string(e.what()));
        }
    }
    
    return results;
}

std::vector<JobConfig> JobStorage::cursorToJobConfigs(mongocxx::cursor& cursor) {
    std::vector<JobConfig> configs;
    
    for (const auto& doc : cursor) {
        try {
            configs.push_back(JobConfig::fromBson(doc));
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse job config from BSON: " + std::string(e.what()));
        }
    }
    
    return configs;
}

void JobStorage::logOperation(const std::string& operation, const std::string& details) {
    LOG_DEBUG("JobStorage::" + operation + (details.empty() ? "" : " - " + details));
}

template<typename T>
Result<T> JobStorage::handleException(const std::function<T()>& operation, const std::string& context) {
    try {
        return Result<T>::Success(operation(), "Operation completed successfully");
    } catch (const mongocxx::exception& e) {
        std::string error = "MongoDB error in " + context + ": " + std::string(e.what());
        LOG_ERROR(error);
        return Result<T>::Failure(error);
    } catch (const std::exception& e) {
        std::string error = "Error in " + context + ": " + std::string(e.what());
        LOG_ERROR(error);
        return Result<T>::Failure(error);
    }
}

// ========================================
// Additional operations - Part 2 in next chunk
// ========================================

Result<bool> JobStorage::recordJobHistory(const std::string& jobId, const std::string& event,
                                         const std::string& details, const std::string& userId) {
    return handleException<bool>([&]() -> bool {
        auto now = std::chrono::system_clock::now();
        
        auto builder = document{};
        builder << "jobId" << jobId
                << "event" << event
                << "details" << details
                << "userId" << userId
                << "timestamp" << timePointToBsonDate(now);
        
        auto result = jobHistoryCollection_.insert_one(builder.view());
        
        if (!result) {
            throw std::runtime_error("Failed to record job history");
        }
        
        return true;
        
    }, "recordJobHistory");
}

Result<std::vector<std::unordered_map<std::string, std::string>>> JobStorage::getJobHistory(const std::string& jobId) {
    return handleException<std::vector<std::unordered_map<std::string, std::string>>>([&]() -> std::vector<std::unordered_map<std::string, std::string>> {
        auto filter = document{} << "jobId" << jobId << finalize;
        auto sort = document{} << "timestamp" << -1 << finalize;
        
        mongocxx::options::find findOptions;
        findOptions.sort(sort.view());
        
        auto cursor = jobHistoryCollection_.find(filter.view(), findOptions);
        std::vector<std::unordered_map<std::string, std::string>> history;
        
        for (const auto& doc : cursor) {
            std::unordered_map<std::string, std::string> entry;
            
            if (auto event = doc["event"]) {
                entry["event"] = std::string(event.get_string().value);
            }
            if (auto details = doc["details"]) {
                entry["details"] = std::string(details.get_string().value);
            }
            if (auto userId = doc["userId"]) {
                entry["userId"] = std::string(userId.get_string().value);
            }
            if (auto timestamp = doc["timestamp"]) {
                auto tp = bsonDateToTimePoint(timestamp.get_date());
                entry["timestamp"] = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count());
            }
            
            history.push_back(entry);
        }
        
        return history;
        
    }, "getJobHistory");
}

// ========================================
// Batch Operations
// ========================================

Result<BatchResult<std::string>> JobStorage::storeJobs(const std::vector<Job>& jobs) {
    return handleException<BatchResult<std::string>>([&]() -> BatchResult<std::string> {
        logOperation("storeJobs", "count: " + std::to_string(jobs.size()));
        
        BatchResult<std::string> batchResult;
        
        if (jobs.empty()) {
            return batchResult;
        }
        
        std::vector<bsoncxx::document::value> documents;
        for (const auto& job : jobs) {
            try {
                auto validationResult = job.validate();
                if (!validationResult.success) {
                    batchResult.failed.push_back(job.getId());
                    batchResult.errors.push_back("Validation failed: " + validationResult.message);
                    continue;
                }
                
                documents.push_back(job.toBson());
                
            } catch (const std::exception& e) {
                batchResult.failed.push_back(job.getId());
                batchResult.errors.push_back(std::string(e.what()));
            }
        }
        
        if (!documents.empty()) {
            try {
                auto result = jobsCollection_.insert_many(documents);
                if (result) {
                    for (size_t i = 0; i < documents.size(); ++i) {
                        batchResult.successful.push_back(jobs[i].getId());
                    }
                }
            } catch (const mongocxx::bulk_write_exception& e) {
                LOG_ERROR("Bulk write error: " + std::string(e.what()));
                // Handle partial success/failure
                for (const auto& job : jobs) {
                    batchResult.failed.push_back(job.getId());
                    batchResult.errors.push_back("Bulk write failed");
                }
            }
        }
        
        LOG_DEBUG("Batch store completed: " + std::to_string(batchResult.successCount()) + 
                  " successful, " + std::to_string(batchResult.failureCount()) + " failed");
        
        return batchResult;
        
    }, "storeJobs");
}

Result<BatchResult<bool>> JobStorage::updateJobs(const std::vector<Job>& jobs) {
    return handleException<BatchResult<bool>>([&]() -> BatchResult<bool> {
        logOperation("updateJobs", "count: " + std::to_string(jobs.size()));
        
        BatchResult<bool> batchResult;
        
        for (const auto& job : jobs) {
            try {
                auto result = updateJob(job);
                if (result.success) {
                    batchResult.successful.push_back(true);
                } else {
                    batchResult.failed.push_back(job.getId());
                    batchResult.errors.push_back(result.message);
                }
            } catch (const std::exception& e) {
                batchResult.failed.push_back(job.getId());
                batchResult.errors.push_back(std::string(e.what()));
            }
        }
        
        LOG_DEBUG("Batch update completed: " + std::to_string(batchResult.successCount()) + 
                  " successful, " + std::to_string(batchResult.failureCount()) + " failed");
        
        return batchResult;
        
    }, "updateJobs");
}

Result<BatchResult<std::string>> JobStorage::storeJobResults(const std::vector<JobResult>& results) {
    return handleException<BatchResult<std::string>>([&]() -> BatchResult<std::string> {
        logOperation("storeJobResults", "count: " + std::to_string(results.size()));
        
        BatchResult<std::string> batchResult;
        
        if (results.empty()) {
            return batchResult;
        }
        
        std::vector<bsoncxx::document::value> documents;
        for (const auto& result : results) {
            try {
                auto validationResult = result.validate();
                if (!validationResult.success) {
                    batchResult.failed.push_back(result.getId());
                    batchResult.errors.push_back("Validation failed: " + validationResult.message);
                    continue;
                }
                
                documents.push_back(result.toBson());
                
            } catch (const std::exception& e) {
                batchResult.failed.push_back(result.getId());
                batchResult.errors.push_back(std::string(e.what()));
            }
        }
        
        if (!documents.empty()) {
            try {
                auto insertResult = jobResultsCollection_.insert_many(documents);
                if (insertResult) {
                    for (size_t i = 0; i < documents.size(); ++i) {
                        batchResult.successful.push_back(results[i].getId());
                    }
                }
            } catch (const mongocxx::bulk_write_exception& e) {
                LOG_ERROR("Bulk write error for results: " + std::string(e.what()));
                for (const auto& result : results) {
                    batchResult.failed.push_back(result.getId());
                    batchResult.errors.push_back("Bulk write failed");
                }
            }
        }
        
        LOG_DEBUG("Batch store results completed: " + std::to_string(batchResult.successCount()) + 
                  " successful, " + std::to_string(batchResult.failureCount()) + " failed");
        
        return batchResult;
        
    }, "storeJobResults");
}

// ========================================
// Queue Management Operations
// ========================================

Result<bool> JobStorage::enqueueJob(const std::string& jobId, JobPriority priority, 
                                   const std::optional<std::chrono::system_clock::time_point>& scheduledAt) {
    return handleException<bool>([&]() -> bool {
        logOperation("enqueueJob", "jobId: " + jobId);
        
        auto now = std::chrono::system_clock::now();
        auto scheduleTime = scheduledAt.value_or(now);
        
        auto builder = document{};
        builder << "jobId" << jobId
                << "priority" << Job::priorityToString(priority)
                << "status" << "queued"
                << "scheduledAt" << timePointToBsonDate(scheduleTime)
                << "enqueuedAt" << timePointToBsonDate(now);
        
        // Use upsert to handle re-queuing
        auto filter = document{} << "jobId" << jobId << finalize;
        mongocxx::options::replace replaceOptions;
        replaceOptions.upsert(true);
        
        auto result = jobQueueCollection_.replace_one(filter.view(), builder.view(), replaceOptions);
        
        if (!result) {
            throw std::runtime_error("Failed to enqueue job: " + jobId);
        }
        
        // Record history entry
        recordJobHistory(jobId, "ENQUEUED", "Job added to queue with priority: " + Job::priorityToString(priority));
        
        LOG_DEBUG("Successfully enqueued job: " + jobId);
        return true;
        
    }, "enqueueJob");
}

Result<std::optional<Job>> JobStorage::dequeueJob(const std::string& workerId) {
    return handleException<std::optional<Job>>([&]() -> std::optional<Job> {
        logOperation("dequeueJob", "workerId: " + workerId);
        
        auto now = std::chrono::system_clock::now();
        
        // Find the highest priority job that's ready to run
        auto filter = document{} 
            << "status" << "queued"
            << "scheduledAt" << open_document 
                << "$lte" << timePointToBsonDate(now) 
            << close_document 
            << finalize;
            
        auto sort = document{} 
            << "priority" << -1  // Higher priority first (CRITICAL=3, HIGH=2, etc.)
            << "scheduledAt" << 1  // Earlier scheduled time first
            << finalize;
        
        mongocxx::options::find_one_and_update options;
        options.sort(sort.view());
        
        // Mark as being processed
        auto update = document{} 
            << "$set" << open_document
                << "status" << "processing"
                << "workerId" << workerId
                << "processingStartedAt" << timePointToBsonDate(now)
            << close_document 
            << finalize;
        
        auto result = jobQueueCollection_.find_one_and_update(filter.view(), update.view(), options);
        
        if (!result) {
            return std::nullopt; // No jobs available
        }
        
        // Get the actual job details
        auto jobIdElement = result->view()["jobId"];
        if (!jobIdElement) {
            throw std::runtime_error("Queue entry missing jobId");
        }
        
        std::string jobId = std::string(jobIdElement.get_string().value);
        auto jobResult = getJob(jobId);
        
        if (!jobResult.success) {
            throw std::runtime_error("Failed to get job details for dequeue: " + jobResult.message);
        }
        
        // Record history entry
        recordJobHistory(jobId, "DEQUEUED", "Job assigned to worker: " + workerId);
        
        LOG_DEBUG("Successfully dequeued job: " + jobId + " for worker: " + workerId);
        return jobResult.value;
        
    }, "dequeueJob");
}

Result<int64_t> JobStorage::getQueuedJobCount() {
    return handleException<int64_t>([&]() -> int64_t {
        logOperation("getQueuedJobCount", "");
        
        auto filter = document{} << "status" << "queued" << finalize;
        auto count = jobQueueCollection_.count_documents(filter.view());
        
        LOG_DEBUG("Queued job count: " + std::to_string(count));
        return static_cast<int64_t>(count);
        
    }, "getQueuedJobCount");
}

Result<std::vector<Job>> JobStorage::getQueuedJobsByPriority(JobPriority priority, const JobQueryOptions& options) {
    return handleException<std::vector<Job>>([&]() -> std::vector<Job> {
        logOperation("getQueuedJobsByPriority", "priority: " + Job::priorityToString(priority));
        
        auto filter = document{} 
            << "status" << "queued"
            << "priority" << Job::priorityToString(priority)
            << finalize;
            
        auto sort = document{} << "scheduledAt" << 1 << finalize;
        
        mongocxx::options::find findOptions;
        findOptions.limit(options.limit);
        findOptions.skip(options.skip);
        findOptions.sort(sort.view());
        
        auto cursor = jobQueueCollection_.find(filter.view(), findOptions);
        
        // Get job IDs from queue and fetch actual job details
        std::vector<Job> jobs;
        for (const auto& queueDoc : cursor) {
            auto jobIdElement = queueDoc["jobId"];
            if (!jobIdElement) {
                continue;
            }
            
            std::string jobId = std::string(jobIdElement.get_string().value);
            auto jobResult = getJob(jobId);
            if (jobResult.success) {
                jobs.push_back(jobResult.value);
            }
        }
        
        LOG_DEBUG("Retrieved " + std::to_string(jobs.size()) + " queued jobs with priority: " + Job::priorityToString(priority));
        return jobs;
        
    }, "getQueuedJobsByPriority");
}

// ========================================
// Statistics and Analytics
// ========================================

Result<JobStorageStats> JobStorage::getStorageStats() {
    return handleException<JobStorageStats>([&]() -> JobStorageStats {
        logOperation("getStorageStats", "");
        
        JobStorageStats stats;
        
        // Total jobs
        stats.totalJobs = static_cast<int64_t>(jobsCollection_.count_documents({}));
        
        // Jobs by status
        stats.queuedJobs = static_cast<int64_t>(jobsCollection_.count_documents(
            document{} << "status" << "queued" << finalize));
        stats.processingJobs = static_cast<int64_t>(jobsCollection_.count_documents(
            document{} << "status" << "processing" << finalize));
        stats.completedJobs = static_cast<int64_t>(jobsCollection_.count_documents(
            document{} << "status" << "completed" << finalize));
        stats.failedJobs = static_cast<int64_t>(jobsCollection_.count_documents(
            document{} << "status" << "failed" << finalize));
        stats.cancelledJobs = static_cast<int64_t>(jobsCollection_.count_documents(
            document{} << "status" << "cancelled" << finalize));
        
        // Total results
        stats.totalResults = static_cast<int64_t>(jobResultsCollection_.count_documents({}));
        
        // Aggregate statistics using MongoDB aggregation pipeline
        mongocxx::pipeline pipeline;
        pipeline.group(document{}
            << "_id" << "$jobType"
            << "count" << open_document << "$sum" << 1 << close_document
            << finalize);
        
        try {
            auto cursor = jobsCollection_.aggregate(pipeline);
            for (const auto& doc : cursor) {
                auto idElement = doc["_id"];
                auto countElement = doc["count"];
                if (idElement && countElement) {
                    std::string jobType = std::string(idElement.get_string().value);
                    int64_t count = countElement.get_int64().value;
                    stats.jobsByType[jobType] = count;
                }
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to get job type statistics: " + std::string(e.what()));
        }
        
        LOG_DEBUG("Retrieved storage statistics - Total jobs: " + std::to_string(stats.totalJobs));
        return stats;
        
    }, "getStorageStats");
}

Result<int64_t> JobStorage::getJobCountByStatus(JobStatus status) {
    return handleException<int64_t>([&]() -> int64_t {
        logOperation("getJobCountByStatus", "status: " + Job::statusToString(status));
        
        auto filter = document{} << "status" << Job::statusToString(status) << finalize;
        auto count = jobsCollection_.count_documents(filter.view());
        
        return static_cast<int64_t>(count);
        
    }, "getJobCountByStatus");
}

Result<int64_t> JobStorage::getJobCountByType(const std::string& jobType) {
    return handleException<int64_t>([&]() -> int64_t {
        logOperation("getJobCountByType", "jobType: " + jobType);
        
        auto filter = document{} << "jobType" << jobType << finalize;
        auto count = jobsCollection_.count_documents(filter.view());
        
        return static_cast<int64_t>(count);
        
    }, "getJobCountByType");
}

Result<int64_t> JobStorage::getJobCountByUser(const std::string& userId) {
    return handleException<int64_t>([&]() -> int64_t {
        logOperation("getJobCountByUser", "userId: " + userId);
        
        auto filter = document{} << "userId" << userId << finalize;
        auto count = jobsCollection_.count_documents(filter.view());
        
        return static_cast<int64_t>(count);
        
    }, "getJobCountByUser");
}

Result<double> JobStorage::getAverageExecutionTime(const std::string& jobType) {
    return handleException<double>([&]() -> double {
        logOperation("getAverageExecutionTime", "jobType: " + jobType);
        
        // Build aggregation pipeline to calculate average execution time
        auto matchBuilder = document{};
        matchBuilder << "finalStatus" << "completed";
        if (!jobType.empty()) {
            // Note: We'd need to join with jobs collection to get jobType from results
            // For now, just calculate overall average for completed jobs
        }
        
        mongocxx::pipeline pipeline;
        pipeline.match(matchBuilder.view());
        pipeline.group(document{}
            << "_id" << bsoncxx::types::b_null{}
            << "avgDuration" << open_document 
                << "$avg" << "$metrics.executionDuration" 
            << close_document
            << finalize);
        
        try {
            auto cursor = jobResultsCollection_.aggregate(pipeline);
            for (const auto& doc : cursor) {
                auto avgElement = doc["avgDuration"];
                if (avgElement) {
                    return avgElement.get_double().value;
                }
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to calculate average execution time: " + std::string(e.what()));
        }
        
        return 0.0; // Default if no data or calculation fails
        
    }, "getAverageExecutionTime");
}

// ========================================
// Maintenance Operations
// ========================================

Result<int64_t> JobStorage::cleanupExpiredData() {
    return handleException<int64_t>([&]() -> int64_t {
        logOperation("cleanupExpiredData", "");
        
        auto now = std::chrono::system_clock::now();
        int64_t totalDeleted = 0;
        
        // Clean up expired job results
        auto expiredFilter = document{} 
            << "expiresAt" << open_document 
                << "$lt" << timePointToBsonDate(now) 
            << close_document 
            << finalize;
        
        auto resultDeleteResult = jobResultsCollection_.delete_many(expiredFilter.view());
        if (resultDeleteResult) {
            totalDeleted += static_cast<int64_t>(resultDeleteResult->deleted_count());
        }
        
        LOG_DEBUG("Cleaned up " + std::to_string(totalDeleted) + " expired records");
        return totalDeleted;
        
    }, "cleanupExpiredData");
}

Result<int64_t> JobStorage::cleanupOldCompletedJobs(int daysOld) {
    return handleException<int64_t>([&]() -> int64_t {
        logOperation("cleanupOldCompletedJobs", "daysOld: " + std::to_string(daysOld));
        
        auto cutoffTime = std::chrono::system_clock::now() - std::chrono::hours(24 * daysOld);
        int64_t totalDeleted = 0;
        
        // Find old completed jobs
        auto filter = document{} 
            << "status" << "completed"
            << "completedAt" << open_document 
                << "$lt" << timePointToBsonDate(cutoffTime) 
            << close_document 
            << finalize;
        
        // Get job IDs first to clean up related data
        std::vector<std::string> jobIds;
        auto cursor = jobsCollection_.find(filter.view(), mongocxx::options::find{}.projection(
            document{} << "id" << 1 << finalize));
        
        for (const auto& doc : cursor) {
            auto idElement = doc["id"];
            if (idElement) {
                jobIds.push_back(std::string(idElement.get_string().value));
            }
        }
        
        // Delete jobs
        auto jobDeleteResult = jobsCollection_.delete_many(filter.view());
        if (jobDeleteResult) {
            totalDeleted += static_cast<int64_t>(jobDeleteResult->deleted_count());
        }
        
        // Clean up related data
        for (const auto& jobId : jobIds) {
            auto relatedFilter = document{} << "jobId" << jobId << finalize;
            jobResultsCollection_.delete_many(relatedFilter.view());
            jobQueueCollection_.delete_many(relatedFilter.view());
            jobHistoryCollection_.delete_many(relatedFilter.view());
        }
        
        LOG_DEBUG("Cleaned up " + std::to_string(totalDeleted) + " old completed jobs");
        return totalDeleted;
        
    }, "cleanupOldCompletedJobs");
}

Result<int64_t> JobStorage::cleanupOldFailedJobs(int daysOld) {
    return handleException<int64_t>([&]() -> int64_t {
        logOperation("cleanupOldFailedJobs", "daysOld: " + std::to_string(daysOld));
        
        auto cutoffTime = std::chrono::system_clock::now() - std::chrono::hours(24 * daysOld);
        int64_t totalDeleted = 0;
        
        // Find old failed jobs
        auto filter = document{} 
            << "status" << "failed"
            << "completedAt" << open_document 
                << "$lt" << timePointToBsonDate(cutoffTime) 
            << close_document 
            << finalize;
        
        // Get job IDs first to clean up related data
        std::vector<std::string> jobIds;
        auto cursor = jobsCollection_.find(filter.view(), mongocxx::options::find{}.projection(
            document{} << "id" << 1 << finalize));
        
        for (const auto& doc : cursor) {
            auto idElement = doc["id"];
            if (idElement) {
                jobIds.push_back(std::string(idElement.get_string().value));
            }
        }
        
        // Delete jobs
        auto jobDeleteResult = jobsCollection_.delete_many(filter.view());
        if (jobDeleteResult) {
            totalDeleted += static_cast<int64_t>(jobDeleteResult->deleted_count());
        }
        
        // Clean up related data
        for (const auto& jobId : jobIds) {
            auto relatedFilter = document{} << "jobId" << jobId << finalize;
            jobResultsCollection_.delete_many(relatedFilter.view());
            jobQueueCollection_.delete_many(relatedFilter.view());
            jobHistoryCollection_.delete_many(relatedFilter.view());
        }
        
        LOG_DEBUG("Cleaned up " + std::to_string(totalDeleted) + " old failed jobs");
        return totalDeleted;
        
    }, "cleanupOldFailedJobs");
}

// ========================================
// Advanced Query Operations
// ========================================

Result<std::vector<bsoncxx::document::value>> JobStorage::executeAggregation(
    const std::string& collectionName,
    const std::vector<bsoncxx::document::value>& pipeline) {
    return handleException<std::vector<bsoncxx::document::value>>([&]() -> std::vector<bsoncxx::document::value> {
        logOperation("executeAggregation", "collection: " + collectionName);
        
        mongocxx::collection collection;
        if (collectionName == JOBS_COLLECTION) {
            collection = jobsCollection_;
        } else if (collectionName == JOB_RESULTS_COLLECTION) {
            collection = jobResultsCollection_;
        } else if (collectionName == JOB_QUEUE_COLLECTION) {
            collection = jobQueueCollection_;
        } else {
            throw std::runtime_error("Unknown collection: " + collectionName);
        }
        
        mongocxx::pipeline mongoPipeline;
        for (const auto& stage : pipeline) {
            // Add each stage to the pipeline
            // Note: This is a simplified approach - in practice you might need to 
            // parse the BSON document to determine the stage type and use appropriate methods
            auto stageView = stage.view();
            for (const auto& element : stageView) {
                std::string stageType = std::string(element.key());
                if (stageType == "$match") {
                    mongoPipeline.match(element.get_document().view());
                } else if (stageType == "$group") {
                    mongoPipeline.group(element.get_document().view());
                } else if (stageType == "$sort") {
                    mongoPipeline.sort(element.get_document().view());
                } else if (stageType == "$limit") {
                    mongoPipeline.limit(element.get_int32().value);
                } else if (stageType == "$skip") {
                    mongoPipeline.skip(element.get_int32().value);
                } else {
                    // Generic stage addition (might not work for all stage types)
                    LOG_WARNING("Unknown aggregation stage type: " + stageType);
                }
            }
        }
        
        auto cursor = collection.aggregate(mongoPipeline);
        std::vector<bsoncxx::document::value> results;
        
        for (const auto& doc : cursor) {
            results.push_back(bsoncxx::document::value(doc));
        }
        
        LOG_DEBUG("Aggregation returned " + std::to_string(results.size()) + " documents");
        return results;
        
    }, "executeAggregation");
}

Result<int64_t> JobStorage::countJobs(const std::unordered_map<std::string, std::string>& filters) {
    return handleException<int64_t>([&]() -> int64_t {
        logOperation("countJobs", "filters: " + std::to_string(filters.size()));
        
        auto filterDoc = buildFilter(filters);
        auto count = jobsCollection_.count_documents(filterDoc.view());
        
        return static_cast<int64_t>(count);
        
    }, "countJobs");
}

Result<std::vector<Job>> JobStorage::findJobs(const bsoncxx::document::view& query, const JobQueryOptions& options) {
    return handleException<std::vector<Job>>([&]() -> std::vector<Job> {
        logOperation("findJobs", "custom query");
        
        auto sortDoc = buildSort(options);
        
        mongocxx::options::find findOptions;
        findOptions.limit(options.limit);
        findOptions.skip(options.skip);
        findOptions.sort(sortDoc.view());
        
        auto cursor = jobsCollection_.find(query, findOptions);
        auto jobs = cursorToJobs(cursor);
        
        LOG_DEBUG("Custom query returned " + std::to_string(jobs.size()) + " jobs");
        return jobs;
        
    }, "findJobs");
}
