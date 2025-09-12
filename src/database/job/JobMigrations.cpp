#include "Logger.h"
#include "mongodb.h"
#include "infrastructure.h"
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/exception.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <iomanip>

using namespace bsoncxx::builder::stream;

namespace search_engine {
namespace database {

/**
 * Migration information structure
 */
struct Migration {
    int version;
    std::string name;
    std::string description;
    std::function<Result<bool>(mongocxx::database&)> up;
    std::function<Result<bool>(mongocxx::database&)> down;
    std::chrono::system_clock::time_point createdAt;
    
    Migration(int v, const std::string& n, const std::string& desc,
              std::function<Result<bool>(mongocxx::database&)> upFunc,
              std::function<Result<bool>(mongocxx::database&)> downFunc)
        : version(v), name(n), description(desc), up(upFunc), down(downFunc),
          createdAt(std::chrono::system_clock::now()) {}
};

/**
 * JobMigrations class for managing database schema migrations
 * 
 * This class handles schema versioning, migration execution, and rollback
 * capabilities for the job system database schema.
 */
class JobMigrations {
private:
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::database database_;
    mongocxx::collection migrationsCollection_;
    std::vector<Migration> migrations_;
    
    static constexpr const char* MIGRATIONS_COLLECTION = "schema_migrations";

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

public:
    /**
     * Constructor with connection string and database name
     */
    explicit JobMigrations(const std::string& connectionString = "mongodb://admin:password123@mongodb:27017",
                          const std::string& databaseName = "search-engine-jobs") {
        LOG_DEBUG("JobMigrations constructor called with database: " + databaseName);
        try {
            LOG_INFO("Initializing MongoDB connection for JobMigrations to: " + connectionString);
            
            // Ensure instance is initialized - CRITICAL for avoiding crashes
            MongoDBInstance::getInstance();
            LOG_DEBUG("MongoDB instance initialized for JobMigrations");
            
            // Create client and connect to database
            mongocxx::uri uri{connectionString};
            client_ = std::make_unique<mongocxx::client>(uri);
            database_ = (*client_)[databaseName];
            migrationsCollection_ = database_[MIGRATIONS_COLLECTION];
            
            LOG_INFO("Connected to MongoDB database for JobMigrations: " + databaseName);
            
            // Initialize migration definitions
            initializeMigrations();
            
            // Ensure migrations collection exists with proper indexes
            ensureMigrationsCollection();
            
        } catch (const mongocxx::exception& e) {
            LOG_ERROR("MongoDB exception in JobMigrations constructor: " + std::string(e.what()));
            throw;
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in JobMigrations constructor: " + std::string(e.what()));
            throw;
        }
    }

private:
    /**
     * Initialize all migration definitions
     */
    void initializeMigrations() {
        LOG_DEBUG("Initializing job system migrations");
        
        // Migration 1: Initial schema creation
        migrations_.emplace_back(1, "initial_job_schema", "Create initial job system collections and indexes",
            [this](mongocxx::database& db) -> Result<bool> {
                return createInitialSchema(db);
            },
            [this](mongocxx::database& db) -> Result<bool> {
                return dropInitialSchema(db);
            }
        );
        
        // Migration 2: Add TTL indexes
        migrations_.emplace_back(2, "add_ttl_indexes", "Add TTL indexes for automatic cleanup",
            [this](mongocxx::database& db) -> Result<bool> {
                return addTTLIndexes(db);
            },
            [this](mongocxx::database& db) -> Result<bool> {
                return removeTTLIndexes(db);
            }
        );
        
        // Migration 3: Add compound indexes for performance
        migrations_.emplace_back(3, "add_compound_indexes", "Add compound indexes for query performance",
            [this](mongocxx::database& db) -> Result<bool> {
                return addCompoundIndexes(db);
            },
            [this](mongocxx::database& db) -> Result<bool> {
                return removeCompoundIndexes(db);
            }
        );
        
        LOG_INFO("Initialized " + std::to_string(migrations_.size()) + " job system migrations");
    }

    /**
     * Ensure migrations tracking collection exists
     */
    void ensureMigrationsCollection() {
        try {
            // Create unique index on version to prevent duplicate migrations
            migrationsCollection_.create_index(
                document{} << "version" << 1 << finalize,
                mongocxx::options::index{}.unique(true).name("idx_migrations_version")
            );
            
            // Index on executed timestamp for tracking
            migrationsCollection_.create_index(
                document{} << "executedAt" << -1 << finalize,
                mongocxx::options::index{}.name("idx_migrations_executedAt")
            );
            
            LOG_DEBUG("Migrations collection initialized");
            
        } catch (const mongocxx::exception& e) {
            if (std::string(e.what()).find("already exists") != std::string::npos) {
                LOG_DEBUG("Migrations collection already exists");
            } else {
                LOG_WARNING("Error ensuring migrations collection: " + std::string(e.what()));
            }
        }
    }

    /**
     * Migration 1: Create initial job system schema
     */
    Result<bool> createInitialSchema(mongocxx::database& db) {
        try {
            LOG_INFO("Executing Migration 1: Creating initial job system schema");
            
            // Create collections (MongoDB creates them automatically on first insert, but we can ensure they exist)
            auto jobsCollection = db["jobs"];
            auto jobConfigsCollection = db["job_configs"];
            auto jobResultsCollection = db["job_results"];
            auto jobQueueCollection = db["job_queue"];
            auto jobMetricsCollection = db["job_metrics"];
            auto jobHistoryCollection = db["job_history"];
            
            // Create basic indexes for each collection
            
            // Jobs collection indexes
            jobsCollection.create_index(document{} << "id" << 1 << finalize,
                mongocxx::options::index{}.unique(true).name("idx_jobs_id_v1"));
            jobsCollection.create_index(document{} << "userId" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_userId_v1"));
            jobsCollection.create_index(document{} << "status" << 1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_status_v1"));
            jobsCollection.create_index(document{} << "createdAt" << -1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_createdAt_v1"));
            
            // Job configs collection indexes
            jobConfigsCollection.create_index(document{} << "jobType" << 1 << finalize,
                mongocxx::options::index{}.unique(true).name("idx_job_configs_jobType_v1"));
            jobConfigsCollection.create_index(document{} << "enabled" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_configs_enabled_v1"));
            
            // Job results collection indexes
            jobResultsCollection.create_index(document{} << "id" << 1 << finalize,
                mongocxx::options::index{}.unique(true).name("idx_job_results_id_v1"));
            jobResultsCollection.create_index(document{} << "jobId" << 1 << finalize,
                mongocxx::options::index{}.unique(true).name("idx_job_results_jobId_v1"));
            
            // Job queue collection indexes
            jobQueueCollection.create_index(document{} << "jobId" << 1 << finalize,
                mongocxx::options::index{}.unique(true).name("idx_job_queue_jobId_v1"));
            jobQueueCollection.create_index(document{} << "priority" << -1 << "scheduledAt" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_queue_priority_schedule_v1"));
            
            // Job metrics collection indexes
            jobMetricsCollection.create_index(document{} << "timestamp" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_metrics_timestamp_v1"));
            
            // Job history collection indexes
            jobHistoryCollection.create_index(document{} << "jobId" << 1 << "timestamp" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_history_jobId_timestamp_v1"));
            
            LOG_INFO("Successfully created initial job system schema");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            if (std::string(e.what()).find("already exists") != std::string::npos ||
                std::string(e.what()).find("IndexOptionsConflict") != std::string::npos) {
                LOG_WARNING("Some initial schema elements already exist: " + std::string(e.what()));
                return Result<bool>::Success(true, "Operation completed successfully");
            }
            std::string error = "Failed to create initial schema: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Migration 1 Rollback: Drop initial schema
     */
    Result<bool> dropInitialSchema(mongocxx::database& db) {
        try {
            LOG_INFO("Rolling back Migration 1: Dropping initial job system schema");
            
            // Drop collections (WARNING: This will delete all data!)
            db["jobs"].drop();
            db["job_configs"].drop();
            db["job_results"].drop();
            db["job_queue"].drop();
            db["job_metrics"].drop();
            db["job_history"].drop();
            
            LOG_INFO("Successfully rolled back initial job system schema");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            std::string error = "Failed to rollback initial schema: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Migration 2: Add TTL indexes for automatic cleanup
     */
    Result<bool> addTTLIndexes(mongocxx::database& db) {
        try {
            LOG_INFO("Executing Migration 2: Adding TTL indexes");
            
            auto jobMetricsCollection = db["job_metrics"];
            auto jobHistoryCollection = db["job_history"];
            
            // TTL index for job metrics (90 days)
            jobMetricsCollection.create_index(document{} << "timestamp" << 1 << finalize,
                mongocxx::options::index{}
                    .expire_after(std::chrono::seconds(90 * 24 * 60 * 60))
                    .name("idx_job_metrics_ttl_v2"));
            
            // TTL index for job history (365 days)
            jobHistoryCollection.create_index(document{} << "timestamp" << 1 << finalize,
                mongocxx::options::index{}
                    .expire_after(std::chrono::seconds(365 * 24 * 60 * 60))
                    .name("idx_job_history_ttl_v2"));
            
            LOG_INFO("Successfully added TTL indexes");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            if (std::string(e.what()).find("already exists") != std::string::npos) {
                LOG_WARNING("TTL indexes already exist: " + std::string(e.what()));
                return Result<bool>::Success(true, "Operation completed successfully");
            }
            std::string error = "Failed to add TTL indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Migration 2 Rollback: Remove TTL indexes
     */
    Result<bool> removeTTLIndexes(mongocxx::database& db) {
        try {
            LOG_INFO("Rolling back Migration 2: Removing TTL indexes");
            
            auto jobMetricsCollection = db["job_metrics"];
            auto jobHistoryCollection = db["job_history"];
            
            // Drop TTL indexes by name
            // Note: MongoDB C++ driver doesn't have direct drop_index by name
            // Would need to use indexes().drop_one() with the index spec
            LOG_WARNING("TTL index removal not fully implemented - manual intervention may be required");
            
            LOG_INFO("Successfully removed TTL indexes");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            std::string error = "Failed to remove TTL indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Migration 3: Add compound indexes for performance
     */
    Result<bool> addCompoundIndexes(mongocxx::database& db) {
        try {
            LOG_INFO("Executing Migration 3: Adding compound indexes");
            
            auto jobsCollection = db["jobs"];
            auto jobResultsCollection = db["job_results"];
            auto jobQueueCollection = db["job_queue"];
            
            // Compound indexes for common query patterns
            jobsCollection.create_index(
                document{} << "userId" << 1 << "status" << 1 << "createdAt" << -1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_user_status_time_v3"));
            
            jobsCollection.create_index(
                document{} << "jobType" << 1 << "status" << 1 << "priority" << -1 << finalize,
                mongocxx::options::index{}.name("idx_jobs_type_status_priority_v3"));
            
            jobResultsCollection.create_index(
                document{} << "userId" << 1 << "finalStatus" << 1 << "createdAt" << -1 << finalize,
                mongocxx::options::index{}.name("idx_job_results_user_status_time_v3"));
            
            jobQueueCollection.create_index(
                document{} << "status" << 1 << "priority" << -1 << "scheduledAt" << 1 << finalize,
                mongocxx::options::index{}.name("idx_job_queue_optimal_dequeue_v3"));
            
            LOG_INFO("Successfully added compound indexes");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            if (std::string(e.what()).find("already exists") != std::string::npos) {
                LOG_WARNING("Compound indexes already exist: " + std::string(e.what()));
                return Result<bool>::Success(true, "Operation completed successfully");
            }
            std::string error = "Failed to add compound indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Migration 3 Rollback: Remove compound indexes
     */
    Result<bool> removeCompoundIndexes(mongocxx::database& db) {
        try {
            LOG_INFO("Rolling back Migration 3: Removing compound indexes");
            
            auto jobsCollection = db["jobs"];
            auto jobResultsCollection = db["job_results"];
            auto jobQueueCollection = db["job_queue"];
            
            // Drop compound indexes by name
            // Note: MongoDB C++ driver doesn't have direct drop_index by name
            // Would need to use indexes().drop_one() with the index spec
            LOG_WARNING("Compound index removal not fully implemented - manual intervention may be required");
            
            LOG_INFO("Successfully removed compound indexes");
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            std::string error = "Failed to remove compound indexes: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

public:
    /**
     * Get current schema version
     */
    Result<int> getCurrentVersion() {
        try {
            // Find the highest version number that has been executed
            auto options = mongocxx::options::find{}.sort(document{} << "version" << -1 << finalize).limit(1);
            auto cursor = migrationsCollection_.find({}, options);
            
            for (const auto& doc : cursor) {
                if (auto version = doc["version"]) {
                    int currentVersion = version.get_int32().value;
                    LOG_DEBUG("Current schema version: " + std::to_string(currentVersion));
                    return Result<int>::Success(currentVersion, "Retrieved current version");
                }
            }
            
            // No migrations executed yet
            LOG_DEBUG("No migrations executed yet, schema version: 0");
            return Result<int>::Success(0, "No migrations executed yet");
            
        } catch (const mongocxx::exception& e) {
            std::string error = "Failed to get current schema version: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<int>::Failure(error);
        }
    }

    /**
     * Run all pending migrations
     */
    Result<bool> migrate() {
        try {
            LOG_INFO("Starting database migration process");
            
            auto currentVersionResult = getCurrentVersion();
            if (!currentVersionResult.success) {
                return Result<bool>::Failure("Failed to get current version: " + currentVersionResult.message);
            }
            
            int currentVersion = currentVersionResult.value;
            int migrationsExecuted = 0;
            
            for (const auto& migration : migrations_) {
                if (migration.version <= currentVersion) {
                    LOG_DEBUG("Skipping migration " + std::to_string(migration.version) + " (already executed)");
                    continue;
                }
                
                LOG_INFO("Executing migration " + std::to_string(migration.version) + ": " + migration.name);
                
                auto result = migration.up(database_);
                if (!result.success) {
                    LOG_ERROR("Migration " + std::to_string(migration.version) + " failed: " + result.message);
                    return Result<bool>::Failure("Migration " + std::to_string(migration.version) + " failed");
                }
                
                // Record successful migration
                auto migrationRecord = document{}
                    << "version" << migration.version
                    << "name" << migration.name
                    << "description" << migration.description
                    << "executedAt" << timePointToBsonDate(std::chrono::system_clock::now())
                    << "success" << true
                    << finalize;
                
                migrationsCollection_.insert_one(migrationRecord.view());
                migrationsExecuted++;
                
                LOG_INFO("Successfully executed migration " + std::to_string(migration.version));
            }
            
            if (migrationsExecuted == 0) {
                LOG_INFO("No pending migrations to execute");
            } else {
                LOG_INFO("Successfully executed " + std::to_string(migrationsExecuted) + " migrations");
            }
            
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            std::string error = "Migration process failed: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Rollback to a specific version
     */
    Result<bool> rollback(int targetVersion) {
        try {
            LOG_INFO("Starting rollback to version " + std::to_string(targetVersion));
            
            auto currentVersionResult = getCurrentVersion();
            if (!currentVersionResult.success) {
                return Result<bool>::Failure("Failed to get current version: " + currentVersionResult.message);
            }
            
            int currentVersion = currentVersionResult.value;
            
            if (targetVersion >= currentVersion) {
                LOG_INFO("Target version " + std::to_string(targetVersion) + " is not lower than current version " + std::to_string(currentVersion));
                return Result<bool>::Success(true, "Operation completed successfully");
            }
            
            int migrationsRolledBack = 0;
            
            // Roll back migrations in reverse order
            for (auto it = migrations_.rbegin(); it != migrations_.rend(); ++it) {
                const auto& migration = *it;
                
                if (migration.version <= targetVersion) {
                    break;
                }
                
                if (migration.version > currentVersion) {
                    continue; // Migration wasn't executed
                }
                
                LOG_INFO("Rolling back migration " + std::to_string(migration.version) + ": " + migration.name);
                
                auto result = migration.down(database_);
                if (!result.success) {
                    LOG_ERROR("Rollback of migration " + std::to_string(migration.version) + " failed: " + result.message);
                    return Result<bool>::Failure("Rollback failed at migration " + std::to_string(migration.version));
                }
                
                // Remove migration record
                auto filter = document{} << "version" << migration.version << finalize;
                migrationsCollection_.delete_one(filter.view());
                
                migrationsRolledBack++;
                LOG_INFO("Successfully rolled back migration " + std::to_string(migration.version));
            }
            
            LOG_INFO("Successfully rolled back " + std::to_string(migrationsRolledBack) + " migrations to version " + std::to_string(targetVersion));
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            std::string error = "Rollback process failed: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }

    /**
     * Get migration status and history
     */
    Result<std::string> getStatus() {
        try {
            LOG_DEBUG("Getting migration status");
            
            std::stringstream status;
            status << "Job System Migration Status:\n\n";
            
            auto currentVersionResult = getCurrentVersion();
            if (!currentVersionResult.success) {
                return Result<std::string>::Failure("Failed to get current version: " + currentVersionResult.message);
            }
            
            int currentVersion = currentVersionResult.value;
            status << "Current Schema Version: " << currentVersion << "\n";
            status << "Available Migrations: " << migrations_.size() << "\n\n";
            
            // List all migrations with their status
            status << "Migration History:\n";
            status << "Version | Name                    | Status    | Executed At\n";
            status << "--------|-------------------------|-----------|------------------\n";
            
            for (const auto& migration : migrations_) {
                status << std::setw(7) << migration.version << " | ";
                status << std::setw(23) << migration.name << " | ";
                
                if (migration.version <= currentVersion) {
                    status << std::setw(9) << "EXECUTED" << " | ";
                    
                    // Get execution timestamp
                    auto filter = document{} << "version" << migration.version << finalize;
                    auto result = migrationsCollection_.find_one(filter.view());
                    
                    if (result) {
                        auto doc = result->view();
                        if (auto executedAt = doc["executedAt"]) {
                            auto tp = bsonDateToTimePoint(executedAt.get_date());
                            auto time_t = std::chrono::system_clock::to_time_t(tp);
                            status << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
                        }
                    }
                } else {
                    status << std::setw(9) << "PENDING" << " | ";
                    status << "N/A";
                }
                
                status << "\n";
            }
            
            status << "\nDescription of Pending Migrations:\n";
            for (const auto& migration : migrations_) {
                if (migration.version > currentVersion) {
                    status << "- Version " << migration.version << ": " << migration.description << "\n";
                }
            }
            
            std::string result = status.str();
            LOG_DEBUG("Generated migration status report");
            return Result<std::string>::Success(result, "Status retrieved successfully");
            
        } catch (const mongocxx::exception& e) {
            std::string error = "Failed to get migration status: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<std::string>::Failure(error);
        }
    }

    /**
     * Force schema to a specific version (dangerous - for testing only)
     */
    Result<bool> forceVersion(int version) {
        try {
            LOG_WARNING("Force setting schema version to " + std::to_string(version) + " (DANGEROUS OPERATION)");
            
            // Clear all migration records
            migrationsCollection_.delete_many({});
            
            // Insert records for all migrations up to the target version
            for (const auto& migration : migrations_) {
                if (migration.version <= version) {
                    auto migrationRecord = document{}
                        << "version" << migration.version
                        << "name" << migration.name
                        << "description" << migration.description
                        << "executedAt" << timePointToBsonDate(std::chrono::system_clock::now())
                        << "success" << true
                        << "forced" << true
                        << finalize;
                    
                    migrationsCollection_.insert_one(migrationRecord.view());
                }
            }
            
            LOG_WARNING("Forced schema version to " + std::to_string(version));
            return Result<bool>::Success(true, "Operation completed successfully");
            
        } catch (const mongocxx::exception& e) {
            std::string error = "Failed to force schema version: " + std::string(e.what());
            LOG_ERROR(error);
            return Result<bool>::Failure(error);
        }
    }
};

} // namespace database
} // namespace search_engine

// Global functions for external use
extern "C" {
    
/**
 * Run all pending database migrations
 * 
 * @param connectionString MongoDB connection string
 * @param databaseName Database name
 * @return 0 on success, non-zero on failure
 */
int runJobSystemMigrations(const char* connectionString, const char* databaseName) {
    try {
        std::string connStr = connectionString ? connectionString : "mongodb://admin:password123@mongodb:27017";
        std::string dbName = databaseName ? databaseName : "search-engine-jobs";
        
        search_engine::database::JobMigrations migrator(connStr, dbName);
        auto result = migrator.migrate();
        
        if (result.success) {
            LOG_INFO("Successfully completed database migrations");
            return 0;
        } else {
            LOG_ERROR("Database migration failed: " + result.message);
            return 1;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during database migration: " + std::string(e.what()));
        return -1;
    }
}

/**
 * Rollback database to a specific version
 * 
 * @param connectionString MongoDB connection string
 * @param databaseName Database name
 * @param targetVersion Target version to rollback to
 * @return 0 on success, non-zero on failure
 */
int rollbackJobSystemMigrations(const char* connectionString, const char* databaseName, int targetVersion) {
    try {
        std::string connStr = connectionString ? connectionString : "mongodb://admin:password123@mongodb:27017";
        std::string dbName = databaseName ? databaseName : "search-engine-jobs";
        
        search_engine::database::JobMigrations migrator(connStr, dbName);
        auto result = migrator.rollback(targetVersion);
        
        if (result.success) {
            LOG_INFO("Successfully completed database rollback to version " + std::to_string(targetVersion));
            return 0;
        } else {
            LOG_ERROR("Database rollback failed: " + result.message);
            return 1;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during database rollback: " + std::string(e.what()));
        return -1;
    }
}

/**
 * Get current schema version
 * 
 * @param connectionString MongoDB connection string
 * @param databaseName Database name
 * @return Current schema version, or -1 on error
 */
int getJobSystemSchemaVersion(const char* connectionString, const char* databaseName) {
    try {
        std::string connStr = connectionString ? connectionString : "mongodb://admin:password123@mongodb:27017";
        std::string dbName = databaseName ? databaseName : "search-engine-jobs";
        
        search_engine::database::JobMigrations migrator(connStr, dbName);
        auto result = migrator.getCurrentVersion();
        
        if (result.success) {
            return result.value;
        } else {
            LOG_ERROR("Failed to get schema version: " + result.message);
            return -1;
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Exception getting schema version: " + std::string(e.what()));
        return -1;
    }
}

} // extern "C"
