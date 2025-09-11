# 📡 **Phase 3a: Real-time Status System**

## 📋 **Issue Description**
Implement real-time job status updates using WebSocket, Server-Sent Events (SSE), and HTTP polling fallbacks. This phase enables live progress tracking in web browsers with automatic fallback mechanisms.

## 🎯 **Acceptance Criteria**
- [ ] WebSocket-based real-time job status updates
- [ ] SSE fallback for WebSocket-incompatible networks
- [ ] HTTP long-polling as final fallback
- [ ] Redis pub/sub for multi-instance scaling
- [ ] User-specific job subscriptions
- [ ] Automatic reconnection and error handling

## 📦 **Tasks**

### **WebSocket Implementation**
- [ ] **JobStatusWebSocket Handler** (`src/websocket/JobStatusWebSocket.h/cpp`)
  - WebSocket connection management
  - User authentication and session validation
  - Job subscription management (user can only see their jobs)
  - Real-time status broadcasting
  - Connection heartbeat and health monitoring

- [ ] **WebSocket Route Integration** (`src/routing/WebSocketRoutes.cpp`)
  ```cpp
  // Register WebSocket endpoint
  app.ws<PerSocketData>("/ws/jobs", {
      .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
          JobStatusWebSocket::handleMessage(ws, message);
      },
      .open = [](auto *ws) {
          JobStatusWebSocket::handleConnection(ws);
      },
      .close = [](auto *ws, int code, std::string_view message) {
          JobStatusWebSocket::handleDisconnection(ws, code);
      }
  });
  ```

- [ ] **WebSocket Message Protocol** (`src/websocket/JobStatusProtocol.h`)
  ```json
  // Client subscription message
  {
    "type": "subscribe",
    "jobIds": ["job_123", "job_456"],
    "userId": "user_789"
  }

  // Server status update message  
  {
    "type": "status_update",
    "jobId": "job_123",
    "status": "processing",
    "progress": 45,
    "currentOperation": "Processing page 120/300",
    "timestamp": "2025-09-11T10:15:30Z"
  }
  ```

### **Server-Sent Events (SSE) Implementation**
- [ ] **SSE Controller** (`src/controllers/JobSSEController.h/cpp`)
  - SSE endpoint for job status streaming
  - User authentication and job filtering
  - Connection management and cleanup
  - Automatic reconnection support

- [ ] **SSE Route Registration**
  ```cpp
  REGISTER_ROUTE(HttpMethod::GET, "/api/v2/jobs/stream", streamJobStatus, JobSSEController);
  ```

- [ ] **SSE Response Format**
  ```
  data: {"type":"status_update","jobId":"job_123","status":"processing","progress":45}
  
  data: {"type":"heartbeat","timestamp":"2025-09-11T10:15:30Z"}
  
  data: {"type":"job_completed","jobId":"job_123","result":{"pages":150,"errors":0}}
  ```

### **HTTP Long-Polling Fallback**
- [ ] **Long-Polling Controller** (`src/controllers/JobPollingController.h/cpp`)
  - Long-polling endpoint with timeout handling
  - Change detection and efficient querying
  - Batch status updates for multiple jobs
  - Graceful timeout and reconnection

- [ ] **Polling Optimization** (`src/job/JobStatusCache.h/cpp`)
  - Redis-based status change detection
  - Efficient querying of job status changes
  - Batch updates to reduce database load
  - Client-specific last-seen timestamps

### **Redis Pub/Sub Integration**
- [ ] **Job Status Publisher** (`src/job/JobStatusPublisher.h/cpp`)
  - Publish job status changes to Redis
  - Multi-instance job status synchronization
  - Event deduplication and filtering
  - Connection pooling and error handling

- [ ] **Job Status Subscriber** (`src/job/JobStatusSubscriber.h/cpp`)
  - Subscribe to job status changes from Redis
  - Route updates to connected WebSocket clients
  - Handle Redis connection failures gracefully
  - Message queuing for offline clients

## 🔧 **Technical Requirements**

### **Redis Integration**
- [ ] **Redis Pub/Sub Configuration**
  ```cpp
  // Redis channels for job status
  const std::string JOB_STATUS_CHANNEL = "job_status_updates";
  const std::string JOB_PROGRESS_CHANNEL = "job_progress_updates";
  const std::string JOB_COMPLETION_CHANNEL = "job_completion_updates";
  ```

- [ ] **Status Update Message Format**
  ```json
  {
    "jobId": "job_123",
    "userId": "user_789", 
    "status": "processing",
    "progress": 45,
    "currentOperation": "Processing page 120/300",
    "timestamp": "2025-09-11T10:15:30Z",
    "workerId": "worker_3"
  }
  ```

### **Connection Management**
- [ ] **WebSocket Connection Pool** (`src/websocket/ConnectionPool.h/cpp`)
  - Track active WebSocket connections per user
  - Connection health monitoring and cleanup
  - Subscription management (which jobs each connection watches)
  - Rate limiting and abuse prevention

- [ ] **Progressive Enhancement Strategy**
  ```cpp
  enum class StatusUpdateMethod {
      WEBSOCKET,      // Primary: Real-time bidirectional
      SSE,           // Fallback: Server-sent events  
      LONG_POLLING,  // Backup: HTTP long-polling
      REGULAR_POLLING // Final: Regular HTTP polling
  };
  ```

## 🧪 **Testing Strategy**

### **Unit Tests** (`tests/websocket/`)
- [ ] **JobStatusWebSocketTest.cpp**
  - WebSocket connection handling
  - Message protocol validation
  - User authentication and authorization
  - Subscription management accuracy

- [ ] **JobSSETest.cpp**
  - SSE connection management
  - Event streaming functionality
  - Reconnection handling
  - Format compliance

### **Integration Tests** (`tests/integration/`)
- [ ] **RealTimeStatusIntegrationTest.cpp**
  - End-to-end status update flow
  - Redis pub/sub integration
  - Multi-client status distribution
  - Fallback mechanism testing

### **Load Testing** (`tests/performance/`)
- [ ] **WebSocket Load Test**
  ```bash
  # Test concurrent WebSocket connections
  ./tests/performance/websocket_load_test.js --connections=1000 --duration=300s
  
  # Test status update throughput
  ./tests/performance/status_update_throughput.js --updates_per_second=10000
  ```

## 📊 **Success Criteria**

### **Performance Targets**
- WebSocket connection establishment: < 100ms
- Status update delivery latency: < 500ms
- Concurrent WebSocket connections: Support 1000+ per server
- SSE fallback latency: < 2 seconds
- Long-polling response time: < 5 seconds

### **Reliability Targets**
- WebSocket connection success rate: > 98%
- Automatic reconnection success rate: > 95%
- Message delivery guarantee: 99.9% (with fallbacks)
- Redis failover time: < 10 seconds

### **Functional Validation**
```bash
# Test WebSocket connectivity
node tests/websocket/test_connection.js --url=ws://localhost:3000/ws/jobs

# Test SSE fallback
curl -N -H "Accept: text/event-stream" http://localhost:3000/api/v2/jobs/stream

# Test long-polling
curl -X GET "http://localhost:3000/api/v2/jobs/poll?timeout=30&last_seen=1234567890"

# Load test all methods
./tests/performance/realtime_load_test.sh --concurrent=500 --duration=300s
```

## 🔗 **Dependencies**
- **Requires**: Phase 2b (Crawler Integration) for status updates
- **Integrates**: Redis for pub/sub messaging
- **Enables**: Phase 3b (Frontend Dashboard)

## 📝 **Implementation Notes**

### **Critical Implementation Rules**
- **ALWAYS** validate user permissions before sending job updates
- **ALWAYS** handle WebSocket disconnections gracefully
- **ALWAYS** implement proper fallback mechanisms
- **ALWAYS** use Redis for status distribution in multi-instance setup
- **ALWAYS** rate limit status updates to prevent spam

### **Security Considerations**
- User can only subscribe to their own jobs
- Admin users can subscribe to all jobs
- WebSocket authentication using existing session system
- Rate limiting to prevent WebSocket abuse
- Input validation for all subscription messages

### **Scalability Design**
- Redis pub/sub for horizontal scaling
- Connection pooling for database and Redis
- Efficient message routing to reduce CPU usage
- Memory-efficient connection management

### **Error Handling Strategy**
- Graceful degradation from WebSocket → SSE → Polling
- Automatic reconnection with exponential backoff
- Client-side retry logic with jitter
- Comprehensive logging for debugging connection issues

## 🏷️ **Labels**
`phase-3a` `real-time` `websocket` `sse` `redis` `scalability`

## ⏱️ **Estimated Timeline**
**6-8 days** for complete implementation and testing

## 📋 **Definition of Done**
- [ ] WebSocket job status updates functional
- [ ] SSE fallback implemented and working
- [ ] HTTP long-polling backup functional
- [ ] Redis pub/sub integration complete
- [ ] User authentication and authorization working
- [ ] Progressive enhancement fallback chain working
- [ ] All unit tests passing (>90% coverage)
- [ ] Integration tests with Redis and job system working
- [ ] Load testing completed successfully (1000+ connections)
- [ ] Connection management and cleanup working
- [ ] Error handling and reconnection logic functional
- [ ] Performance targets met
- [ ] Security validation complete
- [ ] Code review completed

---
**Previous Phase**: Phase 2b - Crawler Integration  
**Next Phase**: Phase 3b - Frontend Dashboard
