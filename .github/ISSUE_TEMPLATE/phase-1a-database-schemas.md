---
name: 🗄️ Phase 1a - Core Database Schemas & Models
about: Implement foundational database schemas and basic job models
title: '[Phase 1a] Core Database Schemas & Models Implementation'
labels: 'phase-1a, database, models, foundation, testable, mongodb'
assignees: ''
---

# 🗄️ **Phase 1a: Core Database Schemas & Models**

## 📋 **Issue Description**
Implement the foundational database schemas and basic job models for the Universal Job Manager System. This is the first buildable and testable component that establishes the data persistence layer.

## 🎯 **Acceptance Criteria**
- [ ] MongoDB collections created with proper indexes
- [ ] Basic job models implemented and tested
- [ ] Database connection and CRUD operations working
- [ ] Unit tests passing for all models
- [ ] Docker integration functional

## 📦 **Tasks**

### **Database Schema Implementation**
- [ ] **MongoDB Collections Design**
  ```
  - jobs (job metadata and state)
  - job_results (crawl results and output data)  
  - job_queue (active queue management)
  - job_metrics (performance analytics)
  - job_history (state transition audit trail)
  ```

- [ ] **Database Indexes** (`src/database/JobIndexes.cpp`)
  ```
  - jobs: userId, status, priority, createdAt, jobType
  - job_queue: priority, status, scheduledAt
  - job_results: jobId, userId, domain
  - job_history: jobId, timestamp
  - job_metrics: timestamp, jobType, userId
  ```

### **Core Models Implementation**
- [ ] **Base Job Model** (`src/models/Job.h/cpp`)
  - Job ID generation and validation
  - Status management (QUEUED, PROCESSING, COMPLETED, FAILED)
  - Progress tracking (0-100%)
  - Timestamp fields (created, started, completed)
  - User and tenant association

- [ ] **Job Configuration Model** (`src/models/JobConfig.h/cpp`)
  - Job type definition
  - Timeout and retry policies
  - Resource requirements (CPU, memory)
  - Priority levels and scheduling

- [ ] **Job Result Model** (`src/models/JobResult.h/cpp`)
  - Result data structure
  - Error information and stack traces
  - Performance metrics (duration, memory usage)
  - Output file references

### **Storage Layer Foundation**
- [ ] **JobStorage Base Class** (`src/storage/JobStorage.h/cpp`)
  - CRUD operations for jobs
  - Batch operations for performance
  - Query builders for complex searches
  - Connection pooling integration
  - **MongoDB Instance Integration** (use `MongoDBInstance::getInstance()`)

- [ ] **Database Migration System** (`src/database/JobMigrations.cpp`)
  - Schema versioning
  - Index creation scripts
  - Data migration utilities
  - Rollback capabilities

## 🔧 **Technical Requirements**

### **Dependencies**
- MongoDB C++ driver (mongocxx)
- nlohmann/json for JSON serialization
- Existing MongoDB singleton (`include/mongodb.h`)
- UUID generation library

### **Configuration**
- [ ] **Environment Variables**
  ```
  JOB_DB_NAME=search-engine-jobs
  JOB_COLLECTION_PREFIX=job_
  JOB_INDEX_BACKGROUND=true
  JOB_TTL_COMPLETED_JOBS=2592000  # 30 days
  ```

- [ ] **CMakeLists.txt Integration**
  ```cpp
  # Add job models library
  add_subdirectory(src/models/job)
  add_subdirectory(src/storage/job)
  target_link_libraries(server job_models job_storage)
  ```

## 🧪 **Testing Strategy**

### **Unit Tests** (`tests/models/`)
- [ ] **JobModelTest.cpp**
  - Job creation and validation
  - Status transition logic
  - Progress tracking accuracy
  - JSON serialization/deserialization

- [ ] **JobStorageTest.cpp**
  - Database connection testing
  - CRUD operations validation
  - Index performance verification
  - Concurrent access testing

### **Integration Tests** (`tests/integration/`)
- [ ] **DatabaseIntegrationTest.cpp**
  - End-to-end database operations
  - Schema migration testing
  - Data consistency verification
  - Performance benchmarking

## 🐳 **Docker Integration**

### **Database Setup**
- [ ] **Update docker-compose.yml**
  ```yaml
  mongodb_jobs:
    extends:
      service: mongodb_test
    environment:
      - MONGO_INITDB_DATABASE=search-engine-jobs
    volumes:
      - ./docker/init-job-db.js:/docker-entrypoint-initdb.d/init-job-db.js
  ```

- [ ] **Database Initialization Script** (`docker/init-job-db.js`)
  ```javascript
  // Create job collections with proper settings
  // Set up initial indexes
  // Configure TTL for cleanup
  ```

## 📊 **Success Criteria**

### **Functional Tests**
```bash
# Test database connection
./build/test_job_storage --test=connection

# Test job model operations
./build/test_job_models --test=crud

# Test schema migration
./build/test_migrations --test=schema_v1

# Performance benchmark
./build/benchmark_job_storage --operations=1000
```

### **Performance Targets**
- Job creation: < 10ms per operation
- Job query: < 50ms for complex filters
- Batch operations: > 100 jobs/second
- Index performance: < 5ms for status queries

## 🔗 **Dependencies**
- **Blocks**: Phase 1b (JobQueue implementation)
- **Requires**: MongoDB container running
- **Enables**: All subsequent job system components

## 📝 **Implementation Notes**

### **Critical Implementation Rules**
- **ALWAYS** use `MongoDBInstance::getInstance()` before creating MongoDB clients
- **ALWAYS** implement proper exception handling for database operations
- **ALWAYS** use connection pooling for performance
- **ALWAYS** add proper logging with `LOG_DEBUG()` for database operations

### **Schema Design Principles**
- **Normalize** job metadata vs job results
- **Index** all frequently queried fields
- **TTL** indexes for automatic cleanup
- **Sharding** considerations for future scaling

## 🏷️ **Labels**
`phase-1a` `database` `models` `foundation` `testable` `mongodb`

## ⏱️ **Estimated Timeline**
**3-5 days** for complete implementation and testing

## 📋 **Definition of Done**
- [ ] All MongoDB collections created with indexes
- [ ] Core job models implemented and tested
- [ ] Database CRUD operations functional
- [ ] Unit tests passing (>95% coverage)
- [ ] Integration tests with MongoDB container working
- [ ] Docker setup complete and documented
- [ ] Performance benchmarks meeting targets
- [ ] Code review completed
- [ ] Documentation updated

---
**Next Phase**: Phase 1b - JobQueue & WorkerService Implementation
