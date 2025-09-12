// MongoDB initialization script for Job System Database
// This script runs when the MongoDB container starts for the first time

print("Starting Job System Database initialization...");

// Connect to the job database
db = db.getSiblingDB('search-engine-jobs');

print("Connected to database: search-engine-jobs");

// Create collections with proper settings
print("Creating job system collections...");

// Jobs collection - main job metadata
db.createCollection("jobs", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["id", "userId", "jobType", "status", "createdAt"],
            properties: {
                id: { bsonType: "string", description: "Unique job identifier" },
                userId: { bsonType: "string", description: "User who created the job" },
                tenantId: { bsonType: "string", description: "Multi-tenant identifier" },
                jobType: { bsonType: "string", description: "Type of job" },
                status: { 
                    bsonType: "string", 
                    enum: ["queued", "processing", "completed", "failed", "cancelled", "retrying"],
                    description: "Current job status" 
                },
                priority: { 
                    bsonType: "string", 
                    enum: ["low", "normal", "high", "critical"],
                    description: "Job priority level" 
                },
                progress: { 
                    bsonType: "int", 
                    minimum: 0, 
                    maximum: 100,
                    description: "Progress percentage (0-100)" 
                },
                createdAt: { bsonType: "date", description: "Creation timestamp" },
                startedAt: { bsonType: "date", description: "Start timestamp" },
                completedAt: { bsonType: "date", description: "Completion timestamp" },
                scheduledAt: { bsonType: "date", description: "Scheduled execution time" },
                retryCount: { bsonType: "int", minimum: 0, description: "Number of retry attempts" },
                maxRetries: { bsonType: "int", minimum: 0, description: "Maximum allowed retries" },
                timeout: { bsonType: "long", description: "Job timeout in seconds" }
            }
        }
    }
});

// Job configurations collection
db.createCollection("job_configs", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["jobType", "name", "timeout", "enabled", "createdAt"],
            properties: {
                jobType: { bsonType: "string", description: "Unique job type identifier" },
                name: { bsonType: "string", description: "Human-readable job name" },
                description: { bsonType: "string", description: "Job description" },
                timeout: { bsonType: "long", minimum: 1, description: "Job execution timeout in seconds" },
                defaultPriority: { 
                    bsonType: "string", 
                    enum: ["low", "normal", "high", "critical"],
                    description: "Default priority level" 
                },
                enabled: { bsonType: "bool", description: "Whether job type is enabled" },
                concurrencyLimit: { bsonType: "int", minimum: 1, description: "Max concurrent jobs of this type" },
                createdAt: { bsonType: "date", description: "Configuration creation time" },
                updatedAt: { bsonType: "date", description: "Last update time" }
            }
        }
    }
});

// Job results collection
db.createCollection("job_results", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["id", "jobId", "finalStatus", "createdAt"],
            properties: {
                id: { bsonType: "string", description: "Unique result identifier" },
                jobId: { bsonType: "string", description: "Associated job ID" },
                userId: { bsonType: "string", description: "User who owns the job" },
                tenantId: { bsonType: "string", description: "Multi-tenant identifier" },
                finalStatus: { 
                    bsonType: "string", 
                    enum: ["completed", "failed", "cancelled"],
                    description: "Final job status" 
                },
                createdAt: { bsonType: "date", description: "Result creation time" },
                expiresAt: { bsonType: "date", description: "Result expiration time" }
            }
        }
    }
});

// Job queue collection - for active queue management
db.createCollection("job_queue", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["jobId", "priority", "status", "scheduledAt", "enqueuedAt"],
            properties: {
                jobId: { bsonType: "string", description: "Reference to job ID" },
                priority: { 
                    bsonType: "string", 
                    enum: ["low", "normal", "high", "critical"],
                    description: "Job priority level" 
                },
                status: { 
                    bsonType: "string", 
                    enum: ["queued", "processing", "completed", "failed"],
                    description: "Queue entry status" 
                },
                scheduledAt: { bsonType: "date", description: "When job should be executed" },
                enqueuedAt: { bsonType: "date", description: "When job was added to queue" },
                processingStartedAt: { bsonType: "date", description: "When processing started" },
                workerId: { bsonType: "string", description: "ID of worker processing the job" }
            }
        }
    }
});

// Job metrics collection - for performance analytics
db.createCollection("job_metrics", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["timestamp", "jobType"],
            properties: {
                timestamp: { bsonType: "date", description: "Metric timestamp" },
                jobType: { bsonType: "string", description: "Type of job" },
                userId: { bsonType: "string", description: "User identifier" },
                tenantId: { bsonType: "string", description: "Tenant identifier" },
                executionDuration: { bsonType: "long", description: "Execution duration in milliseconds" },
                itemsProcessed: { bsonType: "int", description: "Number of items processed" },
                memoryUsage: { bsonType: "long", description: "Memory usage in bytes" },
                cpuUsage: { bsonType: "double", description: "CPU usage percentage" }
            }
        }
    }
});

// Job history collection - for audit trail
db.createCollection("job_history", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["jobId", "event", "timestamp"],
            properties: {
                jobId: { bsonType: "string", description: "Job identifier" },
                event: { bsonType: "string", description: "Event type (CREATED, STARTED, COMPLETED, etc.)" },
                details: { bsonType: "string", description: "Event details" },
                userId: { bsonType: "string", description: "User who triggered the event" },
                timestamp: { bsonType: "date", description: "Event timestamp" }
            }
        }
    }
});

print("Created job system collections successfully");

// Create indexes for optimal performance
print("Creating indexes for job system collections...");

// Jobs collection indexes
db.jobs.createIndex({ "id": 1 }, { unique: true, name: "idx_jobs_id" });
db.jobs.createIndex({ "userId": 1 }, { name: "idx_jobs_userId" });
db.jobs.createIndex({ "status": 1 }, { name: "idx_jobs_status" });
db.jobs.createIndex({ "priority": -1 }, { name: "idx_jobs_priority_desc" });
db.jobs.createIndex({ "createdAt": -1 }, { name: "idx_jobs_createdAt_desc" });
db.jobs.createIndex({ "jobType": 1 }, { name: "idx_jobs_jobType" });
db.jobs.createIndex({ "tenantId": 1 }, { name: "idx_jobs_tenantId" });

// Compound indexes for common query patterns
db.jobs.createIndex({ "userId": 1, "status": 1 }, { name: "idx_jobs_userId_status" });
db.jobs.createIndex({ "jobType": 1, "status": 1 }, { name: "idx_jobs_jobType_status" });
db.jobs.createIndex({ "tenantId": 1, "userId": 1 }, { name: "idx_jobs_tenantId_userId" });
db.jobs.createIndex({ "priority": -1, "createdAt": 1 }, { name: "idx_jobs_priority_createdAt" });
db.jobs.createIndex({ "scheduledAt": 1, "status": 1 }, { name: "idx_jobs_scheduledAt_status" });

// Job configs collection indexes
db.job_configs.createIndex({ "jobType": 1 }, { unique: true, name: "idx_job_configs_jobType" });
db.job_configs.createIndex({ "enabled": 1 }, { name: "idx_job_configs_enabled" });
db.job_configs.createIndex({ "createdAt": -1 }, { name: "idx_job_configs_createdAt" });
db.job_configs.createIndex({ "updatedAt": -1 }, { name: "idx_job_configs_updatedAt" });

// Job results collection indexes
db.job_results.createIndex({ "id": 1 }, { unique: true, name: "idx_job_results_id" });
db.job_results.createIndex({ "jobId": 1 }, { unique: true, name: "idx_job_results_jobId" });
db.job_results.createIndex({ "userId": 1 }, { name: "idx_job_results_userId" });
db.job_results.createIndex({ "tenantId": 1 }, { name: "idx_job_results_tenantId" });
db.job_results.createIndex({ "finalStatus": 1 }, { name: "idx_job_results_finalStatus" });
db.job_results.createIndex({ "createdAt": -1 }, { name: "idx_job_results_createdAt" });
db.job_results.createIndex({ "expiresAt": 1 }, { name: "idx_job_results_expiresAt" });

// Job queue collection indexes - CRITICAL for performance
db.job_queue.createIndex({ "jobId": 1 }, { unique: true, name: "idx_job_queue_jobId" });
db.job_queue.createIndex({ "status": 1 }, { name: "idx_job_queue_status" });
db.job_queue.createIndex({ "priority": -1, "scheduledAt": 1 }, { name: "idx_job_queue_priority_schedule" });
db.job_queue.createIndex({ "scheduledAt": 1 }, { name: "idx_job_queue_scheduledAt" });
db.job_queue.createIndex({ "workerId": 1, "status": 1 }, { name: "idx_job_queue_worker_status" });

// Optimal dequeue index - CRITICAL for queue performance
db.job_queue.createIndex({ 
    "status": 1, 
    "priority": -1, 
    "scheduledAt": 1 
}, { name: "idx_job_queue_optimal_dequeue" });

// Job metrics collection indexes
db.job_metrics.createIndex({ "timestamp": -1 }, { name: "idx_job_metrics_timestamp" });
db.job_metrics.createIndex({ "jobType": 1, "timestamp": -1 }, { name: "idx_job_metrics_jobType_timestamp" });
db.job_metrics.createIndex({ "userId": 1, "timestamp": -1 }, { name: "idx_job_metrics_userId_timestamp" });
db.job_metrics.createIndex({ "tenantId": 1, "timestamp": -1 }, { name: "idx_job_metrics_tenantId_timestamp" });

// TTL index for automatic cleanup (retain metrics for 90 days)
db.job_metrics.createIndex({ "timestamp": 1 }, { 
    expireAfterSeconds: 90 * 24 * 60 * 60, 
    name: "idx_job_metrics_ttl" 
});

// Job history collection indexes
db.job_history.createIndex({ "jobId": 1, "timestamp": -1 }, { name: "idx_job_history_jobId_timestamp" });
db.job_history.createIndex({ "timestamp": -1 }, { name: "idx_job_history_timestamp" });
db.job_history.createIndex({ "event": 1, "timestamp": -1 }, { name: "idx_job_history_event_timestamp" });
db.job_history.createIndex({ "userId": 1, "timestamp": -1 }, { name: "idx_job_history_userId_timestamp" });

// TTL index for automatic cleanup (retain history for 1 year)
db.job_history.createIndex({ "timestamp": 1 }, { 
    expireAfterSeconds: 365 * 24 * 60 * 60, 
    name: "idx_job_history_ttl" 
});

print("Created indexes for job system collections successfully");

// Create default job configurations
print("Creating default job configurations...");

// Default job configurations for common job types
var defaultConfigs = [
    {
        jobType: "crawl",
        name: "Web Crawling Job",
        description: "Crawl websites and extract content",
        timeout: 3600, // 1 hour
        defaultPriority: "normal",
        enabled: true,
        concurrencyLimit: 10,
        retryPolicy: {
            maxRetries: 3,
            initialDelay: 30,
            maxDelay: 3600,
            backoffMultiplier: 2.0,
            exponentialBackoff: true
        },
        resourceRequirements: {
            cpuCores: 1,
            memoryMB: 512,
            diskSpaceMB: 1024
        },
        createdAt: new Date(),
        updatedAt: new Date()
    },
    {
        jobType: "analysis",
        name: "Content Analysis Job",
        description: "Analyze crawled content for insights",
        timeout: 1800, // 30 minutes
        defaultPriority: "normal",
        enabled: true,
        concurrencyLimit: 5,
        retryPolicy: {
            maxRetries: 2,
            initialDelay: 60,
            maxDelay: 1800,
            backoffMultiplier: 2.0,
            exponentialBackoff: true
        },
        resourceRequirements: {
            cpuCores: 2,
            memoryMB: 1024,
            diskSpaceMB: 512
        },
        createdAt: new Date(),
        updatedAt: new Date()
    },
    {
        jobType: "indexing",
        name: "Search Index Update Job",
        description: "Update search indexes with new content",
        timeout: 7200, // 2 hours
        defaultPriority: "high",
        enabled: true,
        concurrencyLimit: 2,
        retryPolicy: {
            maxRetries: 1,
            initialDelay: 120,
            maxDelay: 3600,
            backoffMultiplier: 2.0,
            exponentialBackoff: true
        },
        resourceRequirements: {
            cpuCores: 4,
            memoryMB: 2048,
            diskSpaceMB: 4096
        },
        createdAt: new Date(),
        updatedAt: new Date()
    }
];

// Insert default configurations
db.job_configs.insertMany(defaultConfigs);

print("Created default job configurations successfully");

// Create schema migrations tracking collection
print("Creating schema migrations tracking collection...");

db.createCollection("schema_migrations", {
    validator: {
        $jsonSchema: {
            bsonType: "object",
            required: ["version", "name", "executedAt", "success"],
            properties: {
                version: { bsonType: "int", minimum: 1, description: "Migration version number" },
                name: { bsonType: "string", description: "Migration name" },
                description: { bsonType: "string", description: "Migration description" },
                executedAt: { bsonType: "date", description: "When migration was executed" },
                success: { bsonType: "bool", description: "Whether migration was successful" },
                forced: { bsonType: "bool", description: "Whether migration was forced (for testing)" }
            }
        }
    }
});

// Create index on migration version
db.schema_migrations.createIndex({ "version": 1 }, { unique: true, name: "idx_migrations_version" });
db.schema_migrations.createIndex({ "executedAt": -1 }, { name: "idx_migrations_executedAt" });

print("Created schema migrations tracking collection successfully");

// Record initial schema version
db.schema_migrations.insertOne({
    version: 0,
    name: "initial_setup",
    description: "Initial database setup with collections and indexes",
    executedAt: new Date(),
    success: true,
    forced: false
});

print("Recorded initial schema version successfully");

// Verify setup
print("Verifying job system database setup...");

var collections = db.getCollectionNames();
var expectedCollections = [
    "jobs", 
    "job_configs", 
    "job_results", 
    "job_queue", 
    "job_metrics", 
    "job_history", 
    "schema_migrations"
];

var missingCollections = [];
for (var i = 0; i < expectedCollections.length; i++) {
    if (collections.indexOf(expectedCollections[i]) === -1) {
        missingCollections.push(expectedCollections[i]);
    }
}

if (missingCollections.length > 0) {
    print("ERROR: Missing collections: " + missingCollections.join(", "));
} else {
    print("✓ All expected collections created");
}

// Check indexes
var jobsIndexes = db.jobs.getIndexes().length;
var queueIndexes = db.job_queue.getIndexes().length;
var resultIndexes = db.job_results.getIndexes().length;

print("✓ Jobs collection has " + jobsIndexes + " indexes");
print("✓ Job queue collection has " + queueIndexes + " indexes");
print("✓ Job results collection has " + resultIndexes + " indexes");

// Check configurations
var configCount = db.job_configs.count();
print("✓ Created " + configCount + " default job configurations");

print("Job System Database initialization completed successfully!");
print("Database: search-engine-jobs");
print("Collections: " + expectedCollections.length);
print("Total indexes: " + (jobsIndexes + queueIndexes + resultIndexes));
print("Default configurations: " + configCount);
