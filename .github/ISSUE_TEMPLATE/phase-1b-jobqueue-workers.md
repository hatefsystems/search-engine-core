---
name: ⚙️ Phase 1b - JobQueue & WorkerService
about: Implement core job processing engine with queue management and worker pools
title: '[Phase 1b] JobQueue & WorkerService Implementation'
labels: 'phase-1b, job-queue, workers, redis, core-engine, testable'
assignees: ''
---

# ⚙️ **Phase 1b: JobQueue & WorkerService Implementation**

## 📋 **Issue Description**
Implement the core job processing engine including job queue management, worker pools, and job execution. This builds on Phase 1a and creates the foundation for asynchronous job processing.

## 🎯 **Acceptance Criteria**
- [ ] JobQueue with priority-based processing functional
- [ ] WorkerService with dynamic pool management working
- [ ] Job lifecycle management (queued → processing → completed/failed)
- [ ] Redis integration for hot queue data
- [ ] Crash recovery and job persistence
- [ ] Unit and integration tests passing

## 📦 **Tasks**

### **JobQueue Implementation**
- [ ] **Core JobQueue Class** (`src/job/JobQueue.h/cpp`)
  - Priority-based queue (high/medium/low priority)
  - Atomic job claiming by workers
  - Dead letter queue for failed jobs
  - Queue size limits and backpressure
  - Thread-safe operations

- [ ] **Redis Integration** (`src/job/RedisJobQueue.h/cpp`)
  - Redis-backed queue persistence
  - Queue state recovery after restart
  - Distributed queue access (multiple server instances)
  - Pub/Sub for queue notifications

- [ ] **MongoDB Queue Backup** (`src/job/PersistentJobQueue.h/cpp`)
  - MongoDB backup for Redis queue data
  - Queue recovery from MongoDB if Redis fails
  - Long-term queue analytics and history

### **WorkerService Implementation**
- [ ] **JobWorkerService Class** (`src/job/JobWorkerService.h/cpp`)
  - Dynamic worker pool management
  - Worker health monitoring and heartbeats
  - Load balancing across available workers
  - Configurable worker pool sizes per job type
  - Graceful worker shutdown and restart

- [ ] **JobWorker Class** (`src/job/JobWorker.h/cpp`)
  - Individual worker thread implementation
  - Job execution context and isolation
  - Progress reporting back to queue
  - Error handling and retry logic
  - Resource cleanup after job completion

- [ ] **Worker Pool Manager** (`src/job/WorkerPoolManager.h/cpp`)
  - Multiple worker pools for different job types
  - Resource allocation and quotas
  - Auto-scaling based on queue depth
  - Worker failure detection and replacement

### **Job Execution Framework**
- [ ] **Job Executor Interface** (`src/job/JobExecutor.h`)
  - Generic job execution interface
  - Job type registration system
  - Plugin architecture for different job types
  - Progress callback mechanisms

- [ ] **CrawlJobExecutor** (`src/job/executors/CrawlJobExecutor.h/cpp`)
  - Integration with existing Crawler class
  - Progress reporting during crawl
  - Result storage coordination
  - Error handling and retry policies

### **Job Lifecycle Management**
- [ ] **Job State Manager** (`src/job/JobStateManager.h/cpp`)
  - State transitions (QUEUED → PROCESSING → COMPLETED/FAILED)
  - Progress tracking and updates
  - Timeout detection and handling
  - Job cancellation support

- [ ] **Crash Recovery System** (`src/job/JobRecoveryService.h/cpp`)
  - Load active jobs on application restart
  - Detect orphaned jobs from dead workers
  - Resume jobs from last checkpoint
  - Worker failure detection and job reassignment

## 🔧 **Technical Requirements**

### **Dependencies**
- Redis C++ client (hiredis or redis-plus-plus)
- Thread pool library or custom implementation
- Phase 1a (Database schemas and models)
- Existing MongoDB singleton

### **Configuration**
- [ ] **Job System Configuration** (`include/job/JobConfig.h`)
  ```cpp
  struct JobConfig {
      size_t maxWorkers = 10;
      size_t queueSizeLimit = 1000;
      std::chrono::seconds jobTimeout{3600};  // 1 hour
      std::chrono::seconds workerHeartbeat{30};
      std::chrono::seconds recoveryInterval{60};
      size_t maxRetries = 3;
      std::string redisUrl = "redis://localhost:6379";
  };
  ```

- [ ] **Environment Variables**
  ```
  JOB_MAX_WORKERS=10
  JOB_QUEUE_SIZE_LIMIT=1000
  JOB_TIMEOUT_SECONDS=3600
  JOB_WORKER_HEARTBEAT_SECONDS=30
  JOB_REDIS_URL=redis://localhost:6379
  JOB_RECOVERY_INTERVAL_SECONDS=60
  ```

## 🧪 **Testing Strategy**

### **Unit Tests** (`tests/job/`)
- [ ] **JobQueueTest.cpp**
  - Queue operations (enqueue, dequeue, peek)
  - Priority ordering verification
  - Thread safety testing
  - Dead letter queue functionality

- [ ] **JobWorkerTest.cpp**
  - Worker lifecycle (start, stop, restart)
  - Job execution and progress reporting
  - Error handling and retry logic
  - Resource cleanup verification

- [ ] **WorkerServiceTest.cpp**
  - Worker pool management
  - Load balancing algorithms
  - Auto-scaling behavior
  - Health monitoring accuracy

### **Integration Tests** (`tests/integration/`)
- [ ] **JobProcessingIntegrationTest.cpp**
  - End-to-end job processing flow
  - Redis and MongoDB integration
  - Crash recovery testing
  - Multi-worker coordination

- [ ] **CrashRecoveryTest.cpp**
  - Application restart scenarios
  - Orphaned job detection
  - Queue state recovery
  - Job resumption accuracy

## 🐳 **Docker Integration**

### **Redis Service Addition**
- [ ] **Update docker-compose.yml**
  ```yaml
  redis_jobs:
    image: redis:7-alpine
    ports:
      - "6380:6379"
    environment:
      - REDIS_PASSWORD=job_redis_pass
    volumes:
      - redis_job_data:/data
    command: redis-server --requirepass job_redis_pass
  ```

### **Service Dependencies**
- [ ] **Update main service dependencies**
  ```yaml
  core:
    depends_on:
      - mongodb_test
      - redis_jobs
    environment:
      - JOB_REDIS_URL=redis://:job_redis_pass@redis_jobs:6379
  ```

## 📊 **Success Criteria**

### **Functional Tests**
```bash
# Test job queue operations
./build/test_job_queue --test=enqueue_dequeue

# Test worker service
./build/test_worker_service --test=worker_lifecycle

# Test crash recovery
./build/test_crash_recovery --test=restart_recovery

# Integration test
./build/test_job_integration --test=end_to_end
```

### **Performance Targets**
- Job enqueue: < 5ms per operation
- Job dequeue: < 10ms with priority ordering
- Worker startup: < 2 seconds
- Queue recovery: < 30 seconds after restart
- Concurrent jobs: Support 100+ simultaneous processing

### **Load Testing**
```bash
# Queue performance under load
./build/benchmark_job_queue --jobs=10000 --workers=10

# Worker pool scaling test
./build/test_worker_scaling --max_workers=50 --load_pattern=spike

# Recovery performance test
./build/test_recovery_perf --jobs=1000 --restart_interval=60
```

## 🔗 **Dependencies**
- **Requires**: Phase 1a (Database schemas and models)
- **Blocks**: Phase 2a (Job API Controllers)
- **Integrates**: Redis container and existing MongoDB

## 📝 **Implementation Notes**

### **Critical Implementation Rules**
- **ALWAYS** use lazy initialization in service constructors
- **ALWAYS** implement proper thread synchronization
- **ALWAYS** handle Redis connection failures gracefully
- **ALWAYS** use `LOG_DEBUG()` for job processing debug output
- **ALWAYS** save job progress every 30 seconds during execution

### **Thread Safety Considerations**
- Use atomic operations for counters and flags
- Implement proper mutex protection for shared data
- Avoid deadlocks with consistent lock ordering
- Use lock-free structures where possible for performance

### **Error Handling Strategy**
- Exponential backoff for failed jobs
- Circuit breaker pattern for external dependencies
- Graceful degradation when Redis is unavailable
- Comprehensive logging for debugging

## 🏷️ **Labels**
`phase-1b` `job-queue` `workers` `redis` `core-engine` `testable`

## ⏱️ **Estimated Timeline**
**5-7 days** for complete implementation and testing

## 📋 **Definition of Done**
- [ ] JobQueue implemented with priority support
- [ ] WorkerService managing worker pools dynamically
- [ ] Redis integration functional
- [ ] Crash recovery system working
- [ ] All unit tests passing (>90% coverage)
- [ ] Integration tests with Redis and MongoDB working
- [ ] Performance benchmarks meeting targets
- [ ] Load testing completed successfully
- [ ] Docker integration functional
- [ ] Code review completed
- [ ] Documentation updated

---
**Previous Phase**: Phase 1a - Database Schemas & Models  
**Next Phase**: Phase 2a - Job API Controllers
