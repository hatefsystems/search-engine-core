# 🌐 **Phase 2a: Job API Controllers Implementation**

## 📋 **Issue Description**
Implement REST API endpoints for job management, including job submission, status checking, cancellation, and listing. This phase creates the external interface for the job system.

## 🎯 **Acceptance Criteria**
- [ ] RESTful API endpoints for job operations
- [ ] Job submission returns immediately with job ID
- [ ] Real-time job status and progress tracking
- [ ] User authentication and authorization
- [ ] Backward compatibility with existing crawler API
- [ ] API documentation and testing complete

## 📦 **Tasks**

### **Core API Controllers**
- [ ] **JobController Implementation** (`src/controllers/JobController.h/cpp`)
  - POST `/api/v2/jobs` - Submit new job
  - GET `/api/v2/jobs/{jobId}` - Get job status and details
  - DELETE `/api/v2/jobs/{jobId}` - Cancel job
  - PUT `/api/v2/jobs/{jobId}/retry` - Retry failed job
  - **Lazy Initialization Pattern** for service dependencies

- [ ] **JobListController** (`src/controllers/JobListController.h/cpp`)
  - GET `/api/v2/jobs` - List user's jobs with pagination
  - GET `/api/v2/jobs/active` - List only active/running jobs
  - GET `/api/v2/jobs/completed` - List completed jobs with filters
  - GET `/api/v2/jobs/failed` - List failed jobs with error details

- [ ] **CrawlJobController** (`src/controllers/CrawlJobController.h/cpp`)
  - POST `/api/v2/jobs/crawl` - Submit crawl job (domain-specific)
  - GET `/api/v2/jobs/crawl/{jobId}/progress` - Detailed crawl progress
  - GET `/api/v2/jobs/crawl/{jobId}/results` - Crawl results and data
  - POST `/api/v2/crawl` - Backward compatibility endpoint

### **Request/Response Models**
- [ ] **Job Request Models** (`src/models/requests/`)
  ```cpp
  // JobSubmitRequest.h/cpp
  struct JobSubmitRequest {
      std::string jobType;
      nlohmann::json configuration;
      std::string priority = "medium";
      std::chrono::system_clock::time_point scheduledAt;
      std::vector<std::string> tags;
  };

  // CrawlJobRequest.h/cpp  
  struct CrawlJobRequest {
      std::string domain;
      CrawlConfig crawlConfig;
      bool forceRecrawl = false;
      std::string sessionId;
  };
  ```

- [ ] **Job Response Models** (`src/models/responses/`)
  ```cpp
  // JobResponse.h/cpp
  struct JobResponse {
      std::string jobId;
      std::string status;
      int progress;
      std::string currentOperation;
      nlohmann::json result;
      std::vector<std::string> errors;
      std::chrono::system_clock::time_point createdAt;
      std::chrono::system_clock::time_point startedAt;
      std::chrono::system_clock::time_point completedAt;
  };
  ```

### **Controller Integration**
- [ ] **Route Registration** (`src/routing/JobRoutes.cpp`)
  ```cpp
  // Register all job-related routes
  REGISTER_ROUTE(HttpMethod::POST, "/api/v2/jobs", submitJob, JobController);
  REGISTER_ROUTE(HttpMethod::GET, "/api/v2/jobs/{jobId}", getJobStatus, JobController);
  REGISTER_ROUTE(HttpMethod::DELETE, "/api/v2/jobs/{jobId}", cancelJob, JobController);
  ```

- [ ] **Backward Compatibility Layer** (`src/controllers/BackwardCompatController.h/cpp`)
  - Maintain existing `/api/crawl` endpoint
  - Transform legacy requests to new job system
  - Return job ID in legacy response format
  - Graceful fallback if job system unavailable

### **Authentication & Authorization**
- [ ] **Job Ownership Validation**
  - Users can only access their own jobs
  - Admin users can access all jobs
  - API key authentication support
  - Rate limiting per user/API key

- [ ] **Permission System** (`src/auth/JobPermissions.h/cpp`)
  - Job creation permissions
  - Job cancellation permissions
  - Admin dashboard access
  - Tenant-based job isolation

## 🔧 **Technical Requirements**

### **uWebSockets Safety Implementation**
- [ ] **Safe POST Endpoint Pattern**
  ```cpp
  void JobController::submitJob(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
      std::string buffer;
      res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
          buffer.append(data.data(), data.length());
          if (last) {
              try {
                  auto request = nlohmann::json::parse(buffer);
                  // Process job submission
                  this->processJobSubmission(res, request);
              } catch (const std::exception& e) {
                  LOG_ERROR("Job submission error: " + std::string(e.what()));
                  this->serverError(res, "Job submission failed");
              }
          }
      });

      // CRITICAL: Always add onAborted
      res->onAborted([]() {
          LOG_WARNING("Job submission request aborted by client");
      });
  }
  ```

### **Response Format Standardization**
- [ ] **Success Response Format**
  ```json
  {
    "success": true,
    "message": "Job submitted successfully",
    "data": {
      "jobId": "job_12345",
      "status": "queued",
      "estimatedCompletion": "2025-09-11T11:30:00Z"
    }
  }
  ```

- [ ] **Error Response Format**
  ```json
  {
    "success": false,
    "message": "Job submission failed",
    "error": "INVALID_CONFIGURATION", 
    "details": {
      "field": "domain",
      "reason": "Invalid domain format"
    }
  }
  ```

## 🧪 **Testing Strategy**

### **Unit Tests** (`tests/controllers/`)
- [ ] **JobControllerTest.cpp**
  - Job submission validation
  - Status retrieval accuracy
  - Job cancellation functionality
  - Error handling scenarios

- [ ] **CrawlJobControllerTest.cpp**
  - Crawl job configuration validation
  - Progress reporting accuracy
  - Result retrieval functionality
  - Backward compatibility testing

### **Integration Tests** (`tests/integration/`)
- [ ] **JobAPIIntegrationTest.cpp**
  - End-to-end API workflow testing
  - Database integration verification
  - Job queue integration testing
  - Authentication flow validation

### **API Testing** (`tests/api/`)
- [ ] **cURL Test Scripts** (`tests/api/test_job_api.sh`)
  ```bash
  # Test job submission
  curl -X POST http://localhost:3000/api/v2/jobs \
    -H "Content-Type: application/json" \
    -d '{"jobType": "crawl", "configuration": {"domain": "example.com"}}'

  # Test job status
  curl -X GET http://localhost:3000/api/v2/jobs/{jobId}

  # Test job cancellation  
  curl -X DELETE http://localhost:3000/api/v2/jobs/{jobId}
  ```

## 📊 **Success Criteria**

### **Performance Targets**
- Job submission: < 100ms response time
- Job status query: < 50ms response time
- Job listing: < 200ms for 100 jobs with pagination
- Concurrent API requests: Support 500+ requests/second

### **Functional Tests**
```bash
# Test all endpoints
./tests/api/test_job_endpoints.sh

# Load test API performance
./tests/performance/load_test_job_api.sh --concurrent=50 --duration=60s

# Test backward compatibility
./tests/compatibility/test_legacy_crawl_api.sh
```

### **API Documentation Validation**
- [ ] OpenAPI/Swagger documentation complete
- [ ] All endpoints documented with examples
- [ ] Response schemas validated
- [ ] Error codes documented

## 🔗 **Dependencies**
- **Requires**: Phase 1b (JobQueue & WorkerService)
- **Integrates**: Existing authentication system
- **Enables**: Phase 2b (Crawler Integration)

## 📝 **Implementation Notes**

### **Critical Implementation Rules**
- **ALWAYS** use lazy initialization in controllers (no service init in constructors)
- **ALWAYS** pair `res->onData()` with `res->onAborted()` for uWebSockets safety
- **ALWAYS** validate input data before job submission
- **ALWAYS** use proper HTTP status codes (200, 201, 400, 404, 500)
- **ALWAYS** log API requests with job IDs for traceability

### **Controller Lazy Initialization Pattern**
```cpp
class JobController : public routing::Controller {
private:
    mutable std::unique_ptr<JobService> jobService_;
    
    JobService* getJobService() const {
        if (!jobService_) {
            LOG_INFO("Lazy initializing JobService");
            jobService_ = std::make_unique<JobService>();
        }
        return jobService_.get();
    }
};
```

### **Error Handling Strategy**
- Input validation with clear error messages
- Proper HTTP status codes for different error types
- Detailed logging for debugging
- Graceful degradation when dependencies unavailable

## 🏷️ **Labels**
`phase-2a` `api` `controllers` `rest` `authentication` `testable`

## ⏱️ **Estimated Timeline**
**4-6 days** for complete implementation and testing

## 📋 **Definition of Done**
- [ ] All API endpoints implemented and functional
- [ ] Request/response models properly defined
- [ ] Controller lazy initialization implemented
- [ ] Authentication and authorization working
- [ ] Backward compatibility maintained
- [ ] All unit tests passing (>90% coverage)
- [ ] Integration tests with job system working
- [ ] API documentation complete
- [ ] Performance targets met
- [ ] cURL test scripts functional
- [ ] Code review completed

---
**Previous Phase**: Phase 1b - JobQueue & WorkerService  
**Next Phase**: Phase 2b - Crawler Integration
