# 🕷️ **Phase 2b: Crawler Integration with Job System**

## 📋 **Issue Description**
Integrate the existing Crawler class with the job system to enable asynchronous crawling. This phase modifies the existing crawler to work within the job framework while maintaining all existing functionality.

## 🎯 **Acceptance Criteria**
- [ ] Existing Crawler class integrated with job system
- [ ] Crawler progress reporting during job execution
- [ ] Job-aware logging and session management
- [ ] Backward compatibility with direct crawler usage
- [ ] Crash recovery for interrupted crawl jobs
- [ ] Performance maintained or improved

## 📦 **Tasks**

### **Crawler Job Integration**
- [ ] **CrawlJobExecutor Implementation** (`src/job/executors/CrawlJobExecutor.h/cpp`)
  - Bridge between job system and existing Crawler class
  - Job configuration to CrawlConfig conversion
  - Progress reporting integration
  - Result collection and storage coordination
  - Error handling and retry logic

- [ ] **Crawler Adaptation** (`src/crawler/Crawler.h/cpp` modifications)
  - Add job context awareness (job ID, progress callbacks)
  - Integrate progress reporting without breaking existing API
  - Session-aware logging with job ID correlation
  - Support for job cancellation during crawl
  - Checkpoint system for crash recovery

- [ ] **Job Progress Reporter** (`src/job/CrawlProgressReporter.h/cpp`)
  - Real-time progress updates during crawling
  - Page count and URL discovery tracking
  - ETA calculations based on crawl speed
  - Error reporting and classification
  - Performance metrics collection

### **Crawler Job Configuration**
- [ ] **CrawlJobConfig Model** (`src/models/CrawlJobConfig.h/cpp`)
  ```cpp
  struct CrawlJobConfig {
      std::string domain;
      CrawlConfig crawlConfig;    // Existing crawler config
      bool forceRecrawl = false;
      std::string sessionId;
      bool enableSpaRendering = true;
      std::chrono::seconds timeout{3600};
      int maxRetries = 3;
  };
  ```

- [ ] **Configuration Validation** (`src/job/CrawlJobValidator.h/cpp`)
  - Domain format validation
  - CrawlConfig parameter validation
  - Resource limit checking
  - Duplicate job detection
  - Rate limiting per domain

### **Progress Tracking & Checkpointing**
- [ ] **Crawl Checkpoint System** (`src/crawler/CrawlCheckpoint.h/cpp`)
  - Save crawl state every 30 seconds
  - Visited URLs and queue state persistence
  - Current page processing status
  - Results accumulated so far
  - Error log and retry state

- [ ] **Progress Calculation** (`src/crawler/CrawlProgressCalculator.h/cpp`)
  - Smart progress estimation based on:
    - Pages crawled vs estimated total
    - URLs discovered and processed
    - Historical crawl data for similar domains
    - Current crawl speed and ETA

### **Job-Aware Crawler Features**
- [ ] **Crawler Session Management**
  - Extend existing session management for job integration
  - Job ID correlation in all log messages
  - Session-specific SPA detection (maintain existing logic)
  - Job-specific result storage coordination

- [ ] **Cancellation Support** (`src/crawler/CrawlCancellation.h/cpp`)
  - Graceful crawl cancellation on job termination
  - Partial results preservation
  - Resource cleanup (connections, threads)
  - Final status reporting

## 🔧 **Technical Requirements**

### **Crawler Class Modifications**
- [ ] **Minimal API Changes** (preserve existing interface)
  ```cpp
  class Crawler {
  public:
      // Existing constructors and methods remain unchanged
      Crawler(const CrawlConfig& config, 
              std::shared_ptr<search_engine::storage::ContentStorage> storage = nullptr, 
              const std::string& sessionId = "");
      
      // New job-aware methods
      void setJobContext(const std::string& jobId, 
                        std::function<void(int, const std::string&)> progressCallback);
      void enableCheckpointing(bool enable = true);
      CrawlCheckpoint getCheckpoint() const;
      void restoreFromCheckpoint(const CrawlCheckpoint& checkpoint);
      
  private:
      std::string jobId_;  // Optional job context
      std::function<void(int, const std::string&)> progressCallback_;
      bool checkpointingEnabled_ = false;
      std::atomic<bool> cancellationRequested_{false};
  };
  ```

### **Progress Reporting Integration**
- [ ] **Non-Intrusive Progress Updates**
  ```cpp
  // In existing crawl loop, add minimal progress reporting
  void Crawler::crawlLoop() {
      while (isRunning && !cancellationRequested_) {
          // Existing crawl logic...
          
          // New: Report progress if job context exists
          if (progressCallback_ && shouldReportProgress()) {
              int progress = calculateProgress();
              std::string operation = getCurrentOperation();
              progressCallback_(progress, operation);
          }
          
          // New: Save checkpoint if enabled
          if (checkpointingEnabled_ && shouldSaveCheckpoint()) {
              saveCheckpoint();
          }
      }
  }
  ```

## 🧪 **Testing Strategy**

### **Unit Tests** (`tests/crawler/`)
- [ ] **CrawlJobExecutorTest.cpp**
  - Job execution workflow testing
  - Progress reporting accuracy
  - Error handling and retry logic
  - Configuration validation

- [ ] **CrawlProgressTest.cpp**
  - Progress calculation accuracy
  - ETA estimation validation
  - Checkpoint system functionality
  - Cancellation handling

### **Integration Tests** (`tests/integration/`)
- [ ] **CrawlerJobIntegrationTest.cpp**
  - End-to-end crawl job execution
  - Database storage coordination
  - Job system integration
  - Session management validation

### **Regression Tests** (`tests/regression/`)
- [ ] **CrawlerBackwardCompatibilityTest.cpp**
  - Ensure existing Crawler usage still works
  - Performance regression testing
  - API compatibility verification
  - Legacy session behavior preservation

## 🔍 **Crash Recovery Testing**

### **Recovery Scenarios**
- [ ] **Mid-Crawl Recovery Test**
  ```bash
  # Start crawl job
  curl -X POST /api/v2/jobs/crawl -d '{"domain": "example.com"}'
  
  # Wait for partial progress
  sleep 30
  
  # Simulate crash and restart
  docker restart core
  
  # Verify job resumes from checkpoint
  curl -X GET /api/v2/jobs/{jobId}
  ```

- [ ] **Worker Failure Recovery**
  - Kill worker process during crawl
  - Verify job reassignment to new worker
  - Ensure no data loss or duplication
  - Validate progress continuation

## 📊 **Success Criteria**

### **Performance Targets**
- No performance degradation in direct crawler usage
- Job-based crawling within 5% of direct crawler performance
- Progress updates every 5-10 seconds during active crawling
- Checkpoint save/restore operations < 1 second

### **Functional Validation**
```bash
# Test direct crawler usage (backward compatibility)
./build/test_crawler_direct --domain=example.com

# Test job-based crawler
./build/test_crawler_job --domain=example.com --job-id=test123

# Test crash recovery
./build/test_crawler_recovery --simulate-crash-after=30s

# Performance comparison
./build/benchmark_crawler_modes --domain=test-site.com --duration=300s
```

### **Integration Validation**
- Crawl jobs appear in job dashboard
- Real-time progress updates in web interface
- Results stored in both job system and content storage
- Error handling and retry logic functional

## 🔗 **Dependencies**
- **Requires**: Phase 2a (Job API Controllers)
- **Modifies**: Existing Crawler class (`src/crawler/Crawler.h/cpp`)
- **Integrates**: ContentStorage and MongoDB systems
- **Enables**: Phase 3a (Real-time Status System)

## 📝 **Implementation Notes**

### **Critical Implementation Rules**
- **PRESERVE** all existing Crawler functionality and API
- **MINIMIZE** changes to existing crawler code
- **ENSURE** backward compatibility with direct crawler usage
- **MAINTAIN** existing session management logic
- **PRESERVE** existing SPA detection and rendering features

### **Integration Strategy**
1. **Adapter Pattern**: CrawlJobExecutor wraps existing Crawler
2. **Optional Features**: Job-awareness is optional, not required
3. **Incremental Progress**: Add job features without breaking existing code
4. **Graceful Fallback**: Job features degrade gracefully if not available

### **Testing Strategy**
- Test both job-based and direct crawler usage
- Extensive regression testing for existing functionality
- Performance benchmarking to ensure no degradation
- Stress testing for crash recovery scenarios

## 🏷️ **Labels**
`phase-2b` `crawler` `integration` `backward-compatibility` `progress-tracking`

## ⏱️ **Estimated Timeline**
**5-7 days** for complete implementation and testing

## 📋 **Definition of Done**
- [ ] CrawlJobExecutor implemented and functional
- [ ] Existing Crawler class enhanced with job awareness
- [ ] Progress reporting system working
- [ ] Checkpoint and recovery system functional
- [ ] Backward compatibility preserved and tested
- [ ] All unit tests passing (>90% coverage)
- [ ] Integration tests with job system working
- [ ] Regression tests passing for existing functionality
- [ ] Performance benchmarks meeting targets
- [ ] Crash recovery scenarios tested and working
- [ ] Code review completed
- [ ] Documentation updated for new job-aware features

---
**Previous Phase**: Phase 2a - Job API Controllers  
**Next Phase**: Phase 3a - Real-time Status System
