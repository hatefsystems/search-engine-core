# Lazy Connection Handling Implementation

## Overview

This document describes the implementation of lazy/asynchronous connection
handling in the search engine core (MongoDB, Redis, Browserless, and Kafka),
following microservices best practices for **loose coupling**, **fault
tolerance**, **self-healing**, and **fast startup**.

## Problem Statement

The original implementation had tight coupling between the search engine core
and its service dependencies:

1. **Blocking Startup**: Service wouldn't start if MongoDB/Redis were
   unavailable
2. **Tight Coupling**: Direct dependency on service availability at startup
3. **No Self-Healing**: Required manual restart if databases recovered
4. **Slow Startup**: Waited for all database connections before starting

## Solution: Lazy Connection Handling

### Key Principles Implemented

| Principle           | Implementation                                          | Benefits                       |
| ------------------- | ------------------------------------------------------- | ------------------------------ |
| **Loose Coupling**  | Service starts immediately, connects to DBs when needed | Service can start without DBs  |
| **Fault Tolerance** | Graceful degradation when DBs are unavailable           | Service continues operating    |
| **Self-Healing**    | Automatic reconnection attempts on each operation       | No manual restart needed       |
| **Fast Startup**    | No blocking connection tests during initialization      | Immediate service availability |

### Implementation Details

#### 1. Modified Startup Script (`start.sh`)

**Before:**

```bash
# Blocking connection test
until mongosh --eval "db.runCommand('ping')" > /dev/null 2>&1; do
    echo "Waiting for MongoDB to be ready..."
    sleep 2
done
```

**After:**

```bash
# Non-blocking startup
echo "Starting search engine core..."
echo "MongoDB will be connected lazily at: ${MONGO_HOST}:${MONGO_PORT}"
./server &
```

#### 2. ContentStorage Lazy Initialization

**Connection State Tracking:**

```cpp
class ContentStorage {
private:
    // Connection parameters for lazy initialization
    std::string mongoConnectionString_;
    std::string mongoDatabaseName_;
    std::string redisConnectionString_;
    std::string redisIndexName_;

    // Connection state tracking
    bool mongoConnected_;
    bool redisConnected_;

    // Lazy connection methods
    void ensureMongoConnection();
    void ensureRedisConnection();
};
```

**Lazy Connection Methods:**

```cpp
void ContentStorage::ensureMongoConnection() {
    if (!mongoConnected_ || !mongoStorage_) {
        try {
            LOG_DEBUG("Initializing MongoDB connection...");
            mongoStorage_ = std::make_unique<MongoDBStorage>(
                mongoConnectionString_, mongoDatabaseName_);

            auto mongoTest = mongoStorage_->testConnection();
            if (mongoTest.success) {
                mongoConnected_ = true;
                LOG_INFO("MongoDB connection established successfully");
            } else {
                LOG_WARNING("MongoDB connection test failed: " + mongoTest.message);
                // Don't throw - allow service to continue
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize MongoDB connection: " + std::string(e.what()));
            // Don't throw - allow service to continue
        }
    }
}
```

#### 3. Operation-Level Connection Handling

#### 4. Kafka Frontier Lazy Initialization

Kafka producer and consumer clients are created on first use and re-used across
the process lifetime. Delivery reports and error callbacks are registered at
initialization time. Consumer subscribes lazily and commits offsets only after
successful processing.

```cpp
// Pseudocode
class KafkaFrontier {
  std::unique_ptr<KafkaProducer> producer; // uses librdkafka
  std::unique_ptr<KafkaConsumer> consumer; // uses librdkafka
  void ensureProducer();
  void ensureConsumer();
};
```

**Before:**

```cpp
Result<IndexedPage> ContentStorage::getSiteProfile(const std::string& url) {
    return mongoStorage_->getSiteProfile(url); // Could fail if not connected
}
```

**After:**

```cpp
Result<IndexedPage> ContentStorage::getSiteProfile(const std::string& url) {
    ensureMongoConnection();
    if (!mongoConnected_ || !mongoStorage_) {
        return Result<IndexedPage>::Failure("MongoDB not available");
    }
    return mongoStorage_->getSiteProfile(url);
}
```

## Benefits Achieved

### 1. Fast Startup

- **Before**: 60+ second startup delay waiting for DBs
- **After**: Immediate startup, DBs connect when needed

### 2. Fault Tolerance

- **Before**: Service crashes if DBs unavailable
- **After**: Service starts and operates with graceful degradation

### 3. Self-Healing

- **Before**: Manual restart required when DBs recover
- **After**: Automatic reconnection on next operation

### 4. Loose Coupling

- **Before**: Tight dependency on DB availability
- **After**: Service independent of DB availability

## Usage Examples

### Service Startup

```bash
# Service starts immediately
./start.sh
# Output: "Starting search engine core..."
# Output: "MongoDB will be connected lazily at: localhost:27017"
# Output: "Search engine core is running."
```

### Database Operations

```cpp
// First operation - establishes connection
auto page = storage.getSiteProfile("https://example.com");
// Output: "Initializing MongoDB connection..."
// Output: "MongoDB connection established successfully"

// Subsequent operations - uses existing connection
auto profiles = storage.getSiteProfilesByDomain("example.com");
// No connection messages - reuses existing connection
```

### Graceful Degradation

```cpp
// When MongoDB is down
auto result = storage.getSiteProfile("https://example.com");
// Returns: Result<IndexedPage>::Failure("MongoDB not available")
// Service continues operating for other features

// When MongoDB recovers
auto result = storage.getSiteProfile("https://example.com");
// Output: "Initializing MongoDB connection..."
// Output: "MongoDB connection established successfully"
// Returns: Success with page data
```

## Configuration

### Environment Variables

```bash
# MongoDB connection (optional - defaults to localhost:27017)
MONGO_HOST=mongodb-container
MONGO_PORT=27017
MONGO_DB=search_engine
MONGO_USER=admin
MONGO_PASS=password

# Redis connection (optional - defaults to localhost:6379)
REDIS_HOST=redis-container
REDIS_PORT=6379
```

### Docker Compose Example

```yaml
services:
  search-engine:
    build: .
    environment:
      - MONGO_HOST=mongodb
      - REDIS_HOST=redis
    # No depends_on - loose coupling
    ports:
      - "3000:3000"

  mongodb:
    image: mongo:latest
    # Can be started independently

  redis:
    image: redis:latest
    # Can be started independently
```

## Monitoring and Observability

### Connection Status

```cpp
// Check connection health
auto stats = storage.getStorageStats();
// Returns connection status and error messages if applicable
```

### Logging

- **DEBUG**: Connection initialization attempts
- **INFO**: Successful connections established
- **WARNING**: Connection failures (non-blocking)
- **ERROR**: Connection errors (non-blocking)

### Health Checks

```cpp
// Health check endpoint
auto health = storage.testConnections();
// Returns overall health status without blocking startup
```

## Best Practices

### 1. Error Handling

- Always check operation results for connection failures
- Implement retry logic for transient failures
- Provide fallback behavior when databases are unavailable

### 2. Monitoring

- Monitor connection success/failure rates
- Alert on persistent connection issues
- Track operation latency with/without database connections

### 3. Configuration

- Use environment variables for connection parameters
- Provide sensible defaults for local development
- Support different connection strings for different environments

### 4. Testing

- Test with databases available and unavailable
- Verify graceful degradation behavior
- Test automatic reconnection scenarios

## Migration Guide

### From Blocking to Lazy Connections

1. **Update startup script**: Remove blocking connection tests
2. **Modify constructors**: Store connection parameters instead of connecting
3. **Add lazy connection methods**: Implement `ensureConnection()` methods
4. **Update operations**: Call lazy connection before each database operation
5. **Add connection state tracking**: Track connection status
6. **Update error handling**: Return failure instead of throwing exceptions

### Testing Changes

```cpp
// Test with database available
TEST_CASE("Database operations with available DB") {
    ContentStorage storage;
    auto result = storage.getSiteProfile("https://example.com");
    REQUIRE(result.success);
}

// Test with database unavailable
TEST_CASE("Database operations with unavailable DB") {
    ContentStorage storage;
    auto result = storage.getSiteProfile("https://example.com");
    REQUIRE_FALSE(result.success);
    REQUIRE(result.message == "MongoDB not available");
}
```

## Conclusion

The lazy connection handling implementation transforms the search engine core
from a tightly coupled, database-dependent service into a resilient,
fault-tolerant microservice that can:

- Start immediately regardless of database availability
- Operate with graceful degradation when databases are down
- Automatically recover when databases become available
- Provide better user experience with faster startup times

This approach follows modern microservices best practices and makes the system
more robust and maintainable.
