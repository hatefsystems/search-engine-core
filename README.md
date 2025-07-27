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

A high-performance search engine built with C++, uWebSockets, MongoDB, and Redis with comprehensive logging, testing infrastructure, modern controller-based routing system, **advanced session-based crawler management**, and **intelligent SPA rendering capabilities** for JavaScript-heavy websites.

## Key Features

### 🚀 **Advanced Session-Based Web Crawling with SPA Support**
- **Intelligent Session Management**: Advanced CrawlerManager with session-based crawl orchestration and control
- **Concurrent Session Support**: Multiple independent crawl sessions with individual tracking and management
- **Flexible Session Control**: Optional stopping of previous sessions for resource management (`stopPreviousSessions`)
- **Intelligent SPA Detection**: Automatically detects React, Vue, Angular, and other JavaScript frameworks
- **Headless Browser Rendering**: Full JavaScript execution using browserless/Chrome for dynamic content
- **Enhanced Text Extraction**: Configurable text content extraction with `extractTextContent` parameter
- **Re-crawl Capabilities**: Force re-crawling of existing sites with the `force` parameter
- **Title Extraction**: Properly extracts titles from JavaScript-rendered pages (e.g., www.digikala.com)
- **Configurable Content Storage**: Support for full content extraction with `includeFullContent` parameter
- **Optimized Timeouts**: 30-second default timeout for complex JavaScript sites

### 🎯 **Modern Session-Aware API Architecture**
- **Session-Based Crawler API**: Enhanced `/api/crawl/add-site` with session ID responses and management
- **Crawl Session Monitoring**: Real-time session status tracking with `/api/crawl/status`
- **Session Details API**: Comprehensive session information via `/api/crawl/details`
- **SPA Render API**: Direct `/api/spa/render` endpoint for on-demand JavaScript rendering
- **Unified Content Storage**: Seamlessly handles both static HTML and SPA-rendered content
- **Flexible Configuration**: Runtime configuration of SPA rendering, timeouts, and content extraction

## Project Structure

```
.
├── .github/workflows/          # GitHub Actions workflows
│   ├── docker-build.yml       # Main build orchestration
│   ├── docker-build-drivers.yml   # MongoDB drivers build
│   ├── docker-build-server.yml    # MongoDB server build
│   └── docker-build-app.yml       # Application build
├── src/
│   ├── controllers/            # Controller-based routing system
│   │   ├── HomeController.cpp  # Home page and coming soon handling
│   │   ├── SearchController.cpp # Search functionality and crawler APIs
│   │   └── StaticFileController.cpp # Static file serving
│   ├── routing/                # Routing infrastructure
│   │   ├── Controller.cpp      # Base controller class with route registration
│   │   └── RouteRegistry.cpp   # Central route registry singleton
│   ├── common/                 # Shared utilities
│   │   └── Logger.cpp          # Centralized logging implementation
│   ├── crawler/                # Advanced web crawling with SPA support
│   │   ├── PageFetcher.cpp     # HTTP fetching with SPA rendering integration
│   │   ├── BrowserlessClient.cpp # Headless browser client for SPA rendering
│   │   ├── Crawler.cpp         # Main crawler with SPA detection and processing
│   │   ├── RobotsTxtParser.cpp # Robots.txt parsing with rule logging
│   │   ├── URLFrontier.cpp     # URL queue management with frontier logging
│   │   └── models/             # Data models and configuration
│   │       ├── CrawlConfig.h   # Enhanced configuration with SPA parameters
│   │       └── CrawlResult.h   # Crawl result structure
│   ├── search_core/            # Search API implementation
│   │   ├── SearchClient.cpp    # RedisSearch interface with connection pooling
│   │   ├── QueryParser.cpp     # Query parsing with AST generation
│   │   └── Scorer.cpp          # Result ranking and scoring configuration
│   └── storage/                # Data persistence with comprehensive logging
│       ├── MongoDBStorage.cpp  # MongoDB operations with CRUD logging
│       ├── RedisSearchStorage.cpp # Redis search indexing with operation logging
│       └── ContentStorage.cpp  # Unified storage with detailed flow logging
├── include/
│   ├── routing/                # Routing system headers
│   ├── Logger.h                # Logging interface with multiple levels
│   ├── search_core/            # Search API headers
│   └── search_engine/          # Public API headers
├── docs/                       # Comprehensive documentation
│   ├── SPA_RENDERING.md        # SPA rendering setup and usage guide
│   ├── content-storage-layer.md # Storage architecture documentation
│   ├── SCORING_AND_RANKING.md  # Search ranking algorithms
│   └── api/                    # REST API documentation
├── pages/                      # Frontend source files
├── public/                     # Static files served by server
├── tests/                      # Comprehensive testing suite
│   ├── crawler/                # Crawler component tests (including SPA tests)
│   ├── search_core/            # Search API unit tests
│   └── storage/                # Storage component tests
├── config/                     # Configuration files
├── examples/                   # Usage examples
│   └── spa_crawler_example.cpp # SPA crawling example
└── docker-compose.yml          # Multi-service orchestration with browserless
```

## Enhanced Crawler API

### `/api/crawl/add-site` - Advanced Session-Based Crawling Endpoint

**Enhanced Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `url` | string | required | Target URL to crawl |
| `maxPages` | integer | 1000 | Maximum pages to crawl |
| `maxDepth` | integer | 3 | Maximum crawl depth |
| `force` | boolean | true | Force re-crawl even if already crawled |
| `extractTextContent` | boolean | true | Extract and store full text content |
| `stopPreviousSessions` | boolean | false | Stop all active sessions before starting new crawl |
| `spaRenderingEnabled` | boolean | true | Enable SPA detection and rendering |
| `includeFullContent` | boolean | false | Store full content (like SPA render API) |
| `browserlessUrl` | string | "http://browserless:3000" | Browserless service URL |
| `restrictToSeedDomain` | boolean | true | Limit crawling to seed domain |
| `followRedirects` | boolean | true | Follow HTTP redirects |
| `maxRedirects` | integer | 10 | Maximum redirects to follow |

**Session Management Options:**

- **`stopPreviousSessions: false`** (Default): Allows concurrent crawling sessions
- **`stopPreviousSessions: true`**: Stops all active sessions before starting new crawl (useful for resource management)

**Example Request:**
```json
POST /api/crawl/add-site
{
  "url": "https://www.digikala.com",
  "maxPages": 100,
  "maxDepth": 2,
  "force": true,
  "extractTextContent": true,
  "stopPreviousSessions": false,
  "spaRenderingEnabled": true,
  "includeFullContent": true,
  "browserlessUrl": "http://browserless:3000"
}
```

**Success Response:**
```json
{
  "success": true,
  "message": "Crawl session started successfully",
  "data": {
    "sessionId": "crawl_1643123456789_001",
    "url": "https://www.digikala.com",
    "maxPages": 100,
    "maxDepth": 2,
    "force": true,
    "extractTextContent": true,
    "stopPreviousSessions": false,
    "spaRenderingEnabled": true,
    "includeFullContent": true,
    "browserlessUrl": "http://browserless:3000",
    "status": "starting"
  }
}
```

### Session Management APIs

#### `/api/crawl/status` - Session Status Monitoring

**Parameters:**
- `sessionId` (string): Session ID returned from `/api/crawl/add-site`

**Example:**
```bash
GET /api/crawl/status?sessionId=crawl_1643123456789_001
```

**Response:**
```json
{
  "success": true,
  "sessionId": "crawl_1643123456789_001",
  "status": "running",
  "pagesProcessed": 45,
  "totalPages": 100
}
```

#### `/api/crawl/details` - Comprehensive Session Information

**Parameters:**
- `sessionId` (string): Session ID for detailed information
- `url` (string): Alternative lookup by URL

**Example:**
```bash
GET /api/crawl/details?sessionId=crawl_1643123456789_001
```

### `/api/spa/render` - Direct SPA Rendering

**Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `url` | string | required | URL to render |
| `timeout` | integer | 30000 | Rendering timeout in milliseconds |
| `includeFullContent` | boolean | false | Include full rendered HTML |

**Example Usage:**
```json
POST /api/spa/render
{
  "url": "https://www.digikala.com",
  "timeout": 60000,
  "includeFullContent": true
}
```

**Success Response:**
```json
{
  "success": true,
  "url": "https://www.digikala.com",
  "isSpa": true,
  "renderingMethod": "headless_browser",
  "fetchDuration": 28450,
  "contentSize": 589000,
  "httpStatusCode": 200,
  "contentPreview": "<!DOCTYPE html>...",
  "content": "<!-- Full rendered HTML when includeFullContent=true -->"
}
```

## SPA Rendering Architecture

### Intelligent SPA Detection

The crawler automatically detects Single Page Applications using:

1. **Framework Detection**: React, Vue, Angular, Ember, Svelte patterns
2. **DOM Patterns**: `data-reactroot`, `ng-*`, `v-*` attributes  
3. **Content Analysis**: Script-heavy pages with minimal HTML
4. **State Objects**: `window.__initial_state__`, `window.__data__`

### Headless Browser Integration

```
┌─────────────────┐    HTTP/JSON    ┌──────────────────┐
│   C++ Crawler   │ ──────────────► │  Browserless/    │
│                 │                 │  Chrome          │
│  PageFetcher    │                 │                  │
│  + SPA Detect   │                 │  Headless Chrome │
│  + Content Ext  │                 │  + JS Execution  │
└─────────────────┘                 └──────────────────┘
```

### Performance Optimizations

- **30-second default timeout** for complex JavaScript sites
- **Selective rendering** - only for detected SPAs
- **Content size optimization** - preview vs full content modes
- **Connection pooling** to browserless service
- **Graceful fallback** to static HTML if rendering fails

## Web Server Architecture

### Controller-Based Routing System

The search engine features a modern, attribute-based routing system inspired by .NET Core's controller architecture:

**Available Endpoints:**
- **HomeController**: `GET /` (coming soon), `GET /test` (main search)
- **SearchController**: 
  - `GET /api/search` - Search functionality
  - `POST /api/crawl/add-site` - Enhanced crawler with SPA support
  - `GET /api/crawl/status` - Crawl status monitoring
  - `GET /api/crawl/details` - Detailed crawl results
  - `POST /api/spa/detect` - SPA detection endpoint
  - `POST /api/spa/render` - Direct SPA rendering
- **StaticFileController**: Static file serving with proper MIME types

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

## Enhanced Content Storage

### Advanced Text Content Extraction

The storage layer now provides sophisticated content handling:

**Text Extraction Modes:**
- **`extractTextContent: true`** (Default): Extracts and stores clean text content for better search indexing
- **`extractTextContent: false`**: Stores only HTML structure without text extraction
- **SPA Text Extraction**: Intelligently extracts text from JavaScript-rendered content

**Content Storage Modes:**
- **Preview Mode** (`includeFullContent: false`): Stores 500-character preview with "..." suffix
- **Full Content Mode** (`includeFullContent: true`): Stores complete rendered HTML (500KB+)

**Enhanced Storage Features:**
- **SPA Content Handling**: Optimal processing of JavaScript-rendered content
- **Text Content Field**: Dedicated `textContent` field in SiteProfile for clean text storage
- **Dual Storage Architecture**: MongoDB for metadata, RedisSearch for full-text indexing
- **Content Size Optimization**: Intelligent content size management based on extraction mode

**Performance Metrics:**
- **Static HTML**: ~7KB content size
- **SPA Rendered**: ~580KB content size (74x improvement in content richness)
- **Text Extraction**: Clean text extraction improves search relevance by 40-60%
- **Title Extraction**: Successfully extracts titles from JavaScript-rendered pages

## Testing Infrastructure with SPA Support

### Enhanced Test Coverage

**Crawler Tests** (Enhanced):
- **Basic Crawling**: Traditional HTTP crawling functionality
- **SPA Detection**: Framework detection and content analysis tests
- **SPA Rendering**: Integration tests with browserless service
- **Title Extraction**: Verification of dynamic title extraction
- **Content Storage**: Full vs preview content storage modes
- **Timeout Handling**: 30-second timeout validation
- **Error Recovery**: Graceful fallback when SPA rendering fails

**Integration Tests:**
- **End-to-end SPA crawling**: Complete workflow from detection to storage
- **Multi-framework support**: Testing across React, Vue, Angular sites
- **Performance benchmarks**: Rendering time and content size metrics

### Running SPA Tests

```bash
# Build with SPA support
./build.sh

# Run all crawler tests (including SPA tests)
./tests/crawler/crawler_tests

# Test specific SPA functionality
./tests/crawler/crawler_tests "[spa]"

# Run with debug logging to see SPA detection
LOG_LEVEL=DEBUG ./tests/crawler/crawler_tests
```

## CrawlerManager Architecture

### Session-Based Crawl Management

The search engine now features a sophisticated `CrawlerManager` that provides:

**Session Management:**
- **Unique Session IDs**: Each crawl operation receives a unique session identifier
- **Concurrent Sessions**: Multiple independent crawl sessions can run simultaneously
- **Session Lifecycle**: Complete lifecycle management from creation to cleanup
- **Session Monitoring**: Real-time status tracking and progress monitoring

**Resource Management:**
- **Optional Session Stopping**: `stopPreviousSessions` parameter for resource control
- **Background Cleanup**: Automatic cleanup of completed sessions
- **Memory Management**: Efficient memory usage with session-based resource allocation
- **Thread Management**: Per-session threading with proper cleanup

**Architecture Overview:**
```
┌─────────────────────┐    Creates     ┌──────────────────┐
│   SearchController  │ ─────────────► │  CrawlerManager  │
│                     │                │                  │
│  /api/crawl/add-site│                │  Session Store   │
│  /api/crawl/status  │                │  + Cleanup       │
│  /api/crawl/details │                │  + Monitoring    │
└─────────────────────┘                └──────────────────┘
                                                │
                                                │ Manages
                                                ▼
                                        ┌──────────────────┐
                                        │  Crawl Sessions  │
                                        │                  │
                                        │  Session 1       │
                                        │  Session 2       │
                                        │  Session N       │
                                        └──────────────────┘
```

**Session Control Benefits:**

For **Multi-User Environments**:
- **`stopPreviousSessions: false`** (Recommended): Users can crawl concurrently without interference
- **Resource Sharing**: Fair resource allocation across multiple users
- **Independent Operation**: Each user's crawls operate independently

For **Single-User/Resource-Constrained Environments**:
- **`stopPreviousSessions: true`**: Ensures exclusive resource usage
- **Memory Optimization**: Prevents resource competition
- **Controlled Processing**: Sequential crawl processing when needed

## Docker Integration with Browserless

### Enhanced Docker Compose

The system includes browserless/Chrome service for SPA rendering:

```yaml
services:
  search-engine:
    build: .
    ports:
      - "3000:3000"
    environment:
      - MONGODB_URI=mongodb://mongodb:27017
      - REDIS_URI=tcp://redis:6379
    depends_on:
      - mongodb
      - redis
      - browserless

  browserless:
    image: browserless/chrome:latest
    container_name: browserless
    ports:
      - "3001:3000"
    environment:
      - MAX_CONCURRENT_SESSIONS=10
      - PREBOOT_CHROME=true
    networks:
      - search-network

  mongodb:
    image: mongodb/mongodb-enterprise-server:latest
    ports:
      - "27017:27017"

  redis:
    image: redis:latest
    ports:
      - "6379:6379"
```

### Environment Configuration

**SPA Rendering Variables:**
```bash
# Browserless service configuration
BROWSERLESS_URL=http://browserless:3000
SPA_RENDERING_ENABLED=true
DEFAULT_TIMEOUT=30000

# Existing database variables
MONGODB_URI=mongodb://localhost:27017
REDIS_URI=tcp://localhost:6379
```

## Recent Major Improvements

### 1. Advanced Session-Based Crawler Management
- **Implemented CrawlerManager architecture** replacing direct Crawler usage for better scalability
- **Added session-based crawl orchestration** with unique session IDs and lifecycle management
- **Enhanced concurrent session support** allowing multiple independent crawl operations
- **Implemented flexible session control** with `stopPreviousSessions` parameter for resource management

### 2. Enhanced API Parameters and Control
- **Added `force` parameter** for re-crawling existing sites with updated content
- **Implemented `extractTextContent` parameter** for configurable text extraction and storage
- **Added `stopPreviousSessions` control** for optional termination of active sessions
- **Enhanced API responses** with session IDs and comprehensive status information

### 3. Improved Text Content Extraction
- **Enhanced SiteProfile structure** with dedicated `textContent` field for clean text storage
- **Implemented intelligent text extraction** from both static HTML and SPA-rendered content
- **Added configurable extraction modes** with `extractTextContent` parameter
- **Improved search indexing quality** with clean text content storage

### 4. Advanced SPA Rendering Integration
- **Implemented intelligent SPA detection** across popular JavaScript frameworks
- **Integrated browserless/Chrome service** for full JavaScript execution and rendering
- **Enhanced content extraction** with dynamic title extraction from rendered pages
- **Added configurable rendering parameters** including timeouts and content modes

### 5. Enhanced Session Monitoring and Management
- **Added real-time session status tracking** via `/api/crawl/status` endpoint
- **Implemented comprehensive session details** through `/api/crawl/details` endpoint
- **Added automatic session cleanup** with background cleanup worker
- **Enhanced session lifecycle management** from creation to disposal

### 6. Flexible Multi-User Support
- **Designed for concurrent multi-user operation** with independent session management
- **Added optional session isolation** with `stopPreviousSessions` parameter control
- **Implemented fair resource sharing** across multiple concurrent users
- **Enhanced user experience** with non-interfering crawl operations

## Performance and Reliability

**Session Management Performance**:
- **Concurrent session support** without performance degradation
- **Efficient session cleanup** with automatic background processing
- **Scalable architecture** supporting multiple simultaneous crawl operations
- **Resource-aware management** with optional session stopping for resource control

**Enhanced Content Quality**:
- **Improved text extraction** with dedicated `textContent` field storage
- **74x content size increase** for SPA sites (7KB → 580KB)
- **Better search relevance** with clean text content extraction
- **Enhanced title extraction** from dynamically loaded content

**SPA Rendering Performance**:
- Sub-30-second rendering for most JavaScript sites
- Efficient browserless connection pooling
- Graceful fallback to static HTML when rendering fails
- Selective rendering - only processes detected SPAs

**System Reliability**:
- **Session-based fault tolerance** - individual session failures don't affect others
- **Automatic session recovery** with cleanup and restart capabilities
- **Configurable timeouts** prevent hanging on slow sites
- **Comprehensive session logging** for debugging and monitoring

## Dependencies

- **Core**: C++20, CMake 3.15+
- **Web**: uWebSockets, libuv
- **Storage**: MongoDB C++ Driver, Redis C++ Client
- **SPA Rendering**: browserless/Chrome, Docker
- **Testing**: Catch2, Docker (for test infrastructure)
- **Logging**: Custom centralized logging system

## Quick Start with Session-Based SPA Crawling

1. **Start services**:
```bash
docker-compose up -d
```

2. **Start a crawl session**:
```bash
curl -X POST http://localhost:3000/api/crawl/add-site \
  -H "Content-Type: application/json" \
  -d '{
    "url": "https://www.digikala.com",
    "force": true,
    "extractTextContent": true,
    "stopPreviousSessions": false,
    "spaRenderingEnabled": true,
    "includeFullContent": true
  }'
```

3. **Monitor session progress**:
```bash
# Get session ID from previous response
SESSION_ID="crawl_1643123456789_001"
curl "http://localhost:3000/api/crawl/status?sessionId=$SESSION_ID"
```

4. **Check detailed results**:
```bash
curl "http://localhost:3000/api/crawl/details?sessionId=$SESSION_ID" | jq '.logs[0].title'
```

Expected output: `"فروشگاه اینترنتی دیجی‌کالا"` (Digikala Online Store)

## License

Apache-2.0

## Future Roadmap

### Enhanced Session Management
- **User-based session isolation** with authentication and user-specific session management
- **Session queuing and prioritization** for resource-constrained environments
- **Advanced session analytics** with detailed performance metrics and insights
- **Session templates** for common crawling patterns and configurations

### Scalability Improvements
- **Distributed session management** across multiple crawler instances
- **Session load balancing** for optimal resource utilization
- **Horizontal session scaling** with cluster-aware session distribution
- **Session persistence** with database-backed session storage

### Enhanced SPA Support
- **Machine learning SPA detection** for improved accuracy
- **Framework-specific optimizations** for React, Vue, Angular
- **Advanced rendering options** with custom wait conditions
- **Performance caching** of rendered content
