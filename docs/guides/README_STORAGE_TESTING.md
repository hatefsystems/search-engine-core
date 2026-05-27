# Storage Layer Testing Guide

This guide covers building and testing the storage layer of the search engine,
which includes MongoDB storage, RedisSearch integration, and unified content
storage.

## Overview

The storage layer provides a dual-storage architecture:

- **MongoDB**: Structured metadata storage with rich querying capabilities
- **RedisSearch**: Full-text search indexing with real-time search performance
- **ContentStorage**: Unified interface coordinating both storage systems

## Prerequisites

### Required Software

1. **GCC/Clang** C++ compiler with C++17 support
2. **CMake 3.12+** (preferably latest version)
3. **vcpkg** package manager
4. **MongoDB Community Server** (for MongoDB tests)
5. **Redis with RediSearch module** (for Redis tests)
6. **Apache Kafka + Zookeeper** (Docker, for Kafka-backed frontier tests)

### MongoDB Setup

#### Linux Installation

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install mongodb

# Or install via Docker
docker run -d -p 27017:27017 --name mongodb mongo:latest

# Start MongoDB service (if installed locally)
sudo systemctl start mongod
sudo systemctl enable mongod
```

#### Verification

```bash
# Test MongoDB connection
mongosh --eval "db.runCommand({ping: 1})"
```

### Redis Setup

#### Linux Installation

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install redis-server

# Start Redis service
sudo systemctl start redis-server
sudo systemctl enable redis-server

# Install with RediSearch via Docker (recommended)
docker run -d -p 6379:6379 --name redis-search redislabs/redisearch:latest
```

#### Verification

```bash
# Test Redis connection
redis-cli ping
# Should return "PONG"

# Test RediSearch module
redis-cli "FT.INFO" "test_index"
```

## Build Scripts

### Shell Script (Linux/Unix)

**File**: `build_and_test_storage.sh`

**Usage**:

```bash
# Run all tests
./build_and_test_storage.sh

# Clean build and run all tests
./build_and_test_storage.sh clean

# Run specific test categories
./build_and_test_storage.sh mongodb
./build_and_test_storage.sh redis
./build_and_test_storage.sh content

# Verbose output
./build_and_test_storage.sh verbose

# Parallel build
./build_and_test_storage.sh all --parallel

# Release configuration
./build_and_test_storage.sh all --release
```

**Advanced Options**:

```bash
# Run with code coverage
./build_and_test_storage.sh all --coverage

# Skip dependency checks
./build_and_test_storage.sh quick --skip-deps

# Custom timeout
./build_and_test_storage.sh content --timeout=600
```

## Test Categories

### MongoDB Storage Tests (`test_mongodb_storage`)

Tests the MongoDB storage layer functionality:

- **Connection and Initialization**: Database connection, index creation
- **CRUD Operations**: Create, read, update, delete indexed pages
- **Batch Operations**: Bulk inserts and queries
- **Error Handling**: Connection failures, validation errors
- **Data Integrity**: BSON conversion, data consistency

**Key Test Cases**:

- indexed page storage and retrieval
- Domain-based queries
- Crawl status filtering
- Index performance
- Connection resilience

### Redis Storage Tests (`test_redis_search_storage`)

Tests the RedisSearch integration:

- **Index Management**: Creation, deletion, schema validation
- **Document Indexing**: Adding, updating, removing documents
- **Search Operations**: Full-text search, filtering, ranking
- **Advanced Features**: Highlighting, suggestions, faceted search
- **Batch Operations**: Bulk indexing and deletion

**Key Test Cases**:

- Document indexing and search
- Search query parsing
- Result ranking and scoring
- Index statistics
- Memory management

### Content Storage Tests (`test_content_storage`)

Integration tests for the unified storage interface:

- **Crawl Result Processing**: Converting crawler output to storage format
- **Dual Storage Coordination**: MongoDB and Redis synchronization (handled by Redis Sync Service)
- **Search Integration**: End-to-end search functionality
- **Batch Processing**: Multiple document handling
- **Statistics and Monitoring**: Health checks, performance metrics

**Note**: The Redis Sync Service (`redis-sync`) is a background microservice that continuously synchronizes MongoDB `indexed_pages` to Redis for optimal search performance. It runs automatically in both development and production environments. See `redis-sync-service/README.md` for configuration details.

**Key Test Cases**:

- CrawlResult to IndexedPage conversion
- Dual storage consistency
- Search result ranking
- Error recovery
- Performance monitoring

## Configuration

### Test Configuration File

**File**: `storage_test_config.json`

```json
{
  "mongodb": {
    "connection_string": "mongodb://localhost:27017",
    "database_name": "test-search-engine",
    "test_timeout_seconds": 30
  },
  "redis": {
    "connection_string": "tcp://127.0.0.1:6379",
    "index_name": "test_search_index",
    "test_timeout_seconds": 30
  }
}
```

### Environment Variables

Set these environment variables for custom configuration:

```bash
export MONGODB_URI="mongodb://localhost:27017"
export REDIS_URI="tcp://127.0.0.1:6379"
export LOG_LEVEL="DEBUG"  # TRACE, DEBUG, INFO, WARN, ERROR
```

## Troubleshooting

### Common Build Issues

#### 1. vcpkg Dependencies

**Problem**: Redis dependencies fail to build

```
Error: Failed to build redis-plus-plus, hiredis
```

**Solution**:

```bash
# Update vcpkg
git -C vcpkg pull
./vcpkg/bootstrap-vcpkg.sh

# Clean and reinstall
./vcpkg/vcpkg remove redis-plus-plus hiredis
./vcpkg/vcpkg install redis-plus-plus hiredis --triplet x64-linux
```

#### 2. MongoDB Driver Issues

**Problem**: MongoDB C++ driver not found

```
Error: Could not find mongocxx, bsoncxx
```

**Solution**:

```bash
# Install MongoDB C++ driver via vcpkg
./vcpkg/vcpkg install mongo-cxx-driver --triplet x64-linux

# Or install via package manager
sudo apt install libmongocxx-dev libbsoncxx-dev  # Ubuntu/Debian

# Or download and build manually from:
# https://github.com/mongodb/mongo-cxx-driver
```

#### 3. CMake Configuration Failures

**Problem**: CMake cannot find dependencies

```
Error: Could not find package mongocxx
```

**Solution**:

```bash
# Set vcpkg toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake

# Or set environment variable
export VCPKG_ROOT="/path/to/vcpkg"
```

### Common Runtime Issues

#### 1. MongoDB Connection Failed

**Problem**: Tests skip with "MongoDB not available"

**Solutions**:

1. **Check MongoDB service**:

   ```bash
   sudo systemctl status mongod
   # or
   sudo service mongodb status
   ```

2. **Manual start**:

   ```bash
   mongod --dbpath "/var/lib/mongodb"
   ```

3. **Connection string**:
   ```
   mongodb://localhost:27017  # Default
   mongodb://user:pass@host:port/db  # Authenticated
   ```

#### 2. Redis Connection Failed

**Problem**: Redis tests fail with connection errors

**Solutions**:

1. **Check Redis process**:

   ```bash
   netstat -an | grep 6379
   redis-cli ping
   ```

2. **Start Redis**:

   ```bash
   redis-server
   # or
   sudo systemctl start redis-server
   # or via Docker
   docker run -d -p 6379:6379 redis:latest
   ```

3. **RediSearch module**:

   ```bash
   # Check if RediSearch is loaded
   redis-cli MODULE LIST

   # Use Redis with RediSearch
   docker run -d -p 6379:6379 redislabs/redisearch:latest
   ```

#### 3. Permission Issues

**Problem**: Access denied errors

**Solutions**:

1. **Run with sudo**: Use `sudo` for system operations if needed
2. **File permissions**: Ensure build directory is writable (`chmod +w build/`)
3. **User permissions**: Make sure user has write access to project directory

### Test-Specific Issues

#### 1. Tests Skip or Timeout

**Problem**: Tests complete too quickly or timeout

**Solutions**:

- Increase timeout: `-Timeout 600`
- Check service availability
- Verify test data cleanup
- Monitor resource usage

#### 2. Memory Issues

**Problem**: Out of memory during large batch tests

**Solutions**:

- Reduce batch size in tests
- Increase available RAM
- Check for memory leaks
- Use Release build configuration

#### 3. Index Creation Failures

**Problem**: Search index creation fails

**Solutions**:

- Verify Redis memory settings
- Check RediSearch module availability
- Ensure proper cleanup between tests
- Monitor Redis memory usage

## Performance Optimization

### Build Performance

1. **Parallel Building**:

   ```bash
   ./build_and_test_storage.sh all --parallel
   ```

2. **Incremental Builds**:
   - Avoid `clean` unless necessary
   - Use existing CMake cache
   - Build specific targets only

3. **Release Configuration**:
   ```bash
   ./build_and_test_storage.sh all --release
   ```

### Test Performance

1. **Service Optimization**:
   - Use SSD storage for databases
   - Allocate sufficient RAM
   - Close unnecessary applications

2. **Test Parallelization**:
   - Enable parallel test execution
   - Use separate test databases
   - Avoid resource contention

3. **Targeted Testing**:

   ```bash
   # Test specific components
   ./build_and_test_storage.sh mongodb
   ./build_and_test_storage.sh redis

   # Quick essential tests only
   ./build_and_test_storage.sh quick
   ```

## Continuous Integration

### GitHub Actions Example

```yaml
name: Storage Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    services:
      mongodb:
        image: mongo:latest
        ports:
          - 27017:27017
      redis:
        image: redislabs/redisearch:latest
        ports:
          - 6379:6379
    steps:
      - uses: actions/checkout@v3
      - name: Setup vcpkg
        run: |
          git clone https://github.com/Microsoft/vcpkg.git
          ./vcpkg/bootstrap-vcpkg.sh
      - name: Run Storage Tests
        run: ./build_and_test_storage.sh all --skip-deps
```

### Local Development Workflow

1. **Initial Setup**:

   ```bash
   # One-time setup
   ./build_and_test_storage.sh clean
   ```

2. **Development Cycle**:

   ```bash
   # Quick iteration
   ./build_and_test_storage.sh mongodb --skip-build

   # Full validation
   ./build_and_test_storage.sh all
   ```

3. **Pre-commit**:
   ```bash
   # Comprehensive testing
   ./build_and_test_storage.sh all --release --parallel
   ```

## Monitoring and Logging

### Test Results

- **XML Report**: `build/test_results.xml` (JUnit format)
- **Console Output**: Real-time progress and results
- **Build Logs**: CMake and compiler output

### Performance Metrics

- **Build Time**: Component and test build duration
- **Test Execution Time**: Individual and total test runtime
- **Resource Usage**: Memory and CPU utilization

### Debug Information

Enable verbose logging:

```bash
# Shell script
./build_and_test_storage.sh verbose

# Environment variable
export LOG_LEVEL="DEBUG"
```

## Support

### Getting Help

1. **Script Help**:

   ```bash
   ./build_and_test_storage.sh help
   ```

2. **Documentation**: See `docs/content-storage-layer.md`

3. **Issues**: Check build logs and test output for specific error messages

### Common Solutions

- **Clean build**: Use `clean` parameter to start fresh
- **Update dependencies**: Refresh vcpkg and rebuild
- **Check services**: Verify MongoDB and Redis are running
- **Environment**: Ensure proper GCC/Clang and CMake setup

This guide should help you successfully build and test the storage layer
components. For additional support, refer to the main project documentation or
create an issue with specific error details.
