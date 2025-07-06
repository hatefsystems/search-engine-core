# search-engine-core

[![CI/CD Pipeline](https://github.com/hatefsystems/search-engine-core/actions/workflows/main.yml/badge.svg?branch=master)](https://github.com/hatefsystems/search-engine-core/actions/workflows/main.yml)

We are working toward a future where internet search is more 
open, reliable, and aligned with the values and needs of people 
everywhere. This community-oriented initiative encourages 
inclusive participation and shared benefit, aiming to complement 
existing structures by enhancing access, strengthening privacy 
protections, and promoting constructive global collaboration. 
Together, we can help shape a digital environment that is 
transparent, respectful, and supportive of all.

# Search Engine Core

A high-performance search engine built with C++, uWebSockets, MongoDB, and Redis with comprehensive logging and testing infrastructure.

## Project Structure

```
.
├── .github/workflows/          # GitHub Actions workflows
│   ├── docker-build.yml       # Main build orchestration
│   ├── docker-build-drivers.yml   # MongoDB drivers build
│   ├── docker-build-server.yml    # MongoDB server build
│   └── docker-build-app.yml       # Application build
├── src/
│   ├── common/                # Shared utilities
│   │   └── Logger.cpp         # Centralized logging implementation
│   ├── crawler/               # Web crawling components with full logging
│   │   ├── PageFetcher.cpp    # HTTP fetching with request/response logging
│   │   ├── RobotsTxtParser.cpp # Robots.txt parsing with rule logging
│   │   └── URLFrontier.cpp    # URL queue management with frontier logging
│   ├── search_core/           # Search API implementation
│   │   ├── SearchClient.cpp   # RedisSearch interface with connection pooling
│   │   ├── QueryParser.cpp    # Query parsing with AST generation
│   │   └── Scorer.cpp         # Result ranking and scoring configuration
│   └── storage/               # Data persistence with comprehensive logging
│       ├── MongoDBStorage.cpp # MongoDB operations with CRUD logging
│       ├── RedisSearchStorage.cpp # Redis search indexing with operation logging
│       └── ContentStorage.cpp # Unified storage with detailed flow logging
├── include/
│   ├── Logger.h               # Logging interface with multiple levels
│   ├── search_core/           # Search API headers
│   │   ├── SearchClient.hpp   # RedisSearch client interface
│   │   ├── QueryParser.hpp    # Query parsing and AST definitions
│   │   └── Scorer.hpp         # Scoring configuration interface
│   └── search_engine/         # Public API headers
├── tests/
│   ├── crawler/               # Crawler component unit tests (25 tests)
│   │   ├── crawler_tests.cpp  # Core crawler functionality
│   │   ├── page_fetcher_tests.cpp # HTTP fetching tests
│   │   └── url_frontier_tests.cpp # URL queue tests
│   ├── search_core/           # Search API unit tests
│   │   ├── TestSearchClient.cpp    # Redis connection and search tests
│   │   ├── TestQueryParser.cpp     # Query parsing tests (10 test cases)
│   │   ├── TestScorer.cpp          # Scoring configuration tests
│   │   └── TestExactSearchE2E.cpp  # End-to-end integration tests
│   └── storage/               # Storage component unit tests (64 total tests)
│       ├── test_mongodb_storage.cpp # MongoDB CRUD operations (25 tests)
│       ├── test_redis_search_storage.cpp # Redis search functionality
│       └── test_content_storage.cpp # Unified storage tests
├── config/                    # Configuration files
│   ├── redis.json            # Redis connection configuration
│   └── scoring.json          # Search result scoring weights
├── public/                    # Static files
├── build.sh                   # Enhanced build script with test support
├── build_and_test.sh         # New comprehensive build and test runner
├── build_search_core.sh      # Search API specific build script
├── Dockerfile                # Main application Dockerfile
├── Dockerfile.mongodb        # MongoDB drivers Dockerfile
└── Dockerfile.mongodb-server # MongoDB server Dockerfile
```

## Search Engine API (search_core)

### Overview

The `search_core` module provides a high-performance, thread-safe search API built on RedisSearch with the following key components:

- **SearchClient**: RAII-compliant RedisSearch interface with connection pooling
- **QueryParser**: Advanced query parsing with AST generation and Redis syntax conversion
- **Scorer**: Configurable result ranking system with JSON-based field weights

### Features

**SearchClient**:
- Connection pooling with round-robin load distribution
- Thread-safe concurrent search operations
- Modern C++20 implementation with PIMPL pattern
- Comprehensive error handling with custom exceptions

**QueryParser**:
- Exact phrase matching: `"quick brown fox"`
- Boolean operators: `AND`, `OR` with implicit AND between terms
- Domain filtering: `site:example.com` → `@domain:{example.com}`
- Text normalization: lowercase conversion, punctuation stripping
- Abstract Syntax Tree (AST) generation for complex query structures

**Scorer**:
- JSON-configurable field weights (title: 2.0, body: 1.0 by default)
- RedisSearch TFIDF scoring integration
- Hot-reloadable configuration for runtime tuning
- Extensible design for custom ranking algorithms

### Testing Coverage

**Search Core Tests**:
- **Unit Tests**: Component-level testing for SearchClient, QueryParser, and Scorer
- **Integration Tests**: End-to-end search scenarios with Redis
- **Performance Tests**: Latency measurements under load (100+ operations)
- **Thread Safety Tests**: Concurrent operation validation (10 threads × 10 searches)

### Configuration

**Redis Configuration** (`config/redis.json`):
```json
{
  "uri": "tcp://127.0.0.1:6379",
  "pool_size": 4
}
```

**Scoring Configuration** (`config/scoring.json`):
```json
{
  "field_weights": {
    "title": 2.0,
    "body": 1.0
  },
  "offset_boost": 0.1
}
```

## Logging Infrastructure

### Comprehensive Logging System

The search engine now features a centralized logging system with multiple levels:

- **LOG_DEBUG**: Detailed debugging information (method entry/exit, parameter values)
- **LOG_INFO**: General operational information (successful operations, connections)
- **LOG_TRACE**: Fine-grained execution details (intermediate steps, data transformations)
- **LOG_WARNING**: Non-critical issues (not found conditions, fallback operations)
- **LOG_ERROR**: Error conditions (failed operations, exceptions)

### Logging Coverage

**Storage Components** (Recently Enhanced):
- **MongoDBStorage**: Full CRUD operation logging including connection establishment, document operations, and error handling
- **RedisSearchStorage**: Redis connection, index creation, and document indexing with detailed context
- **ContentStorage**: Dual MongoDB+Redis operations with update vs. insert decision logging

**Crawler Components** (Existing):
- **PageFetcher**: HTTP request/response logging with success/failure details
- **RobotsTxtParser**: robots.txt parsing and rule evaluation logging
- **URLFrontier**: URL queue management and domain-based queuing logging

### Log Configuration

Logging levels can be controlled via environment variable:
```bash
export LOG_LEVEL=DEBUG  # DEBUG, INFO, TRACE, WARNING, ERROR, NONE
```

Log output includes both console and file output with detailed contextual information.

## Unit Testing Infrastructure

### Test Framework

The project uses **Catch2** testing framework with comprehensive coverage:

**Total Test Count**: 80+ tests across all components (including search_core tests)

### Test Categories

**Search Core Tests** (Recently Implemented):
- **SearchClient Tests**: Connection pooling, error handling, and search operations
- **QueryParser Tests**: 10 test cases covering query DSL features
- **Scorer Tests**: Configuration loading and scoring parameter tests
- **Integration Tests**: End-to-end search scenarios with performance benchmarks

**Storage Tests** (Recently Enhanced):
- **MongoDB Storage Tests**: 25 assertions covering CRUD operations
  - Site profile creation, retrieval, updates, and deletion
  - Connection testing and error handling
  - Bulk operations and domain-based queries
- **Redis Search Storage Tests**: Search indexing and retrieval
- **Content Storage Tests**: Unified storage workflows

**Crawler Tests** (Existing):
- **Core Crawler Tests**: Web crawling functionality
- **Page Fetcher Tests**: HTTP client behavior, redirects, timeouts
- **URL Frontier Tests**: URL queue management and politeness policies

### Running Tests

**Individual Test Suites**:
```bash
# Search core tests
./tests/search_core/test_search_client
./tests/search_core/test_query_parser

# MongoDB storage tests only
./tests/storage/test_mongodb_storage

# All storage tests
./tests/storage/storage_tests

# All crawler tests  
./tests/crawler/crawler_tests
```

**Comprehensive Test Runner**:
```bash
# Build and run all tests with detailed logging
./build_and_test.sh

# Run with specific log level
LOG_LEVEL=DEBUG ./build_and_test.sh
```

**Using CTest**:
```bash
cd build && ctest --output-on-failure
```

### Test Infrastructure Features

- **MongoDB Test Environment**: Automated Docker container setup for testing
- **Redis Test Environment**: Redis container management for search tests
- **Logging Integration**: Full logging output during test execution
- **Error Reporting**: Detailed failure analysis with context
- **Parallel Test Execution**: Independent test suites for faster execution

## Build System Improvements

### Enhanced CMake Configuration

**Recent Fixes**:
- **Logger Linking**: Resolved undefined symbol errors by properly linking `Logger.cpp` to storage libraries
- **Linux-Only Build**: Simplified build system by removing Windows-specific configurations
- **Dependency Management**: Improved handling of MongoDB and Redis dependencies

**Build Architecture**:
- **Modular Libraries**: Individual libraries for MongoDB, Redis, and Content storage
- **Shared Components**: Centralized logger and infrastructure components
- **Test Integration**: Proper test executable linking with all dependencies

### Build Process

**Standard Build**:
```bash
./build.sh
```

**Build with Testing**:
```bash
./build_and_test.sh
```

**Docker Build Process**:

1. **MongoDB Drivers Stage**
   - Builds MongoDB C++ drivers with enhanced caching
   - Optimized for development and CI environments

2. **MongoDB Server Stage**
   - Complete MongoDB server with driver integration
   - Test-ready environment configuration

3. **Application Stage**
   - Final application with all components
   - Logging and testing infrastructure included

## Infrastructure Requirements

### Development Environment

**Required Services**:
```bash
# MongoDB (for storage tests)
docker run -d --name mongodb -p 27017:27017 \
  mongodb/mongodb-enterprise-server:latest \
  mongod --noauth --bind_ip_all

# Redis (for search tests)  
# Redis with RedisSearch module (for search_core tests)
docker run -d --name redis -p 6379:6379 redis/redis-stack-server:latest
```

**Build Dependencies**:
- C++20 compiler (GCC/Clang)
- CMake 3.15+
- MongoDB C++ Driver
- Redis C++ Client (optional)
- Redis C++ Client (redis-plus-plus, hiredis)
- Catch2 testing framework

### Configuration

**Environment Variables**:

| Variable | Description | Default | Test Impact |
|----------|-------------|---------|-------------|
| LOG_LEVEL | Logging verbosity level | INFO | Test output detail |
| MONGODB_URI | MongoDB connection string | mongodb://localhost:27017 | Storage tests |
| REDIS_URI | Redis connection string | redis://localhost:6379 | Search tests |

## Recent Major Improvements

### 1. Search Engine API Implementation (search_core)
- **Implemented complete RedisSearch interface** with connection pooling and thread safety
- **Built advanced query parser** supporting exact phrases, boolean operators, and domain filters
- **Created configurable scoring system** with JSON-based field weights
- **Comprehensive test coverage** including unit, integration, and performance tests

### 2. Comprehensive Logging Implementation
- **Added detailed logging to all storage components** matching existing crawler patterns
- **Implemented proper error context** with operation-specific details
- **Enhanced debugging capabilities** with trace-level logging for fine-grained analysis

### 3. Build System Resolution
- **Fixed Logger linking issues** that prevented storage library compilation
- **Streamlined CMake configuration** for Linux-only development
- **Resolved undefined symbol errors** in storage component tests

### 4. MongoDB Testing Infrastructure
- **Resolved authentication conflicts** by configuring proper test environment
- **Established reliable test database** with clean state management
- **Implemented comprehensive CRUD testing** with detailed operational verification

### 5. Enhanced Test Coverage
- **Expanded from basic crawler tests to comprehensive storage testing**
- **Implemented individual and combined test executables** for flexible testing
- **Added auto-discovery of test cases** for better CI integration

## Performance and Reliability

**Search Performance**:
- Sub-5ms p95 latency for local Redis operations
- Connection pooling eliminates connection overhead
- Lock-free round-robin connection selection

**Logging Performance**:
- Minimal overhead logging implementation
- Configurable log levels for production optimization
- Structured logging format for analysis tools

**Test Reliability**:
- Isolated test environments with proper cleanup
- Deterministic test execution with proper setup/teardown
- Comprehensive error reporting for debugging failed tests

**Build Reliability**:
- Proper dependency resolution and linking
- Platform-specific optimizations for Linux development
- Comprehensive error handling in build scripts

## Dependencies

- **Core**: C++20, CMake 3.15+
- **Web**: uWebSockets, libuv
- **Storage**: MongoDB C++ Driver, Redis C++ Client
- **Testing**: Catch2, Docker (for test infrastructure)
- **Logging**: Custom centralized logging system

## License

Apache-2.0

## Future Roadmap

### REST API Integration
The search_core library is designed for seamless integration with the existing uWebSockets server:

```cpp
// Example endpoint (to be implemented)
GET /search?q="machine learning"&limit=10&domain=arxiv.org
```

This will provide a complete HTTP REST API for search functionality while maintaining the modular architecture.
