# Search Engine Core Documentation

Welcome to the Search Engine Core documentation. This directory contains comprehensive documentation for the C++ search engine implementation.

## 📚 Documentation Index

### 🚀 Getting Started

- **[README.md](../README.md)** - Main project overview and quick start guide
- **[LICENSE](../LICENSE)** - Project license information

### 🔧 Development Documentation

#### API Documentation

- **[api/README.md](./api/README.md)** - API documentation index
- **[api/crawler_endpoint.md](./api/crawler_endpoint.md)** - Web crawler API endpoints
- **[api/search_endpoint.md](./api/search_endpoint.md)** - Search API endpoints
- **[api/sponsor_endpoint.md](./api/sponsor_endpoint.md)** - Sponsor management API
- **[api/website_profile_endpoint.md](./api/website_profile_endpoint.md)** - Website profile API
- **[api/WEBSITE_PROFILE_API_SUMMARY.md](./api/WEBSITE_PROFILE_API_SUMMARY.md)** - Implementation summary

#### Architecture Documentation

- **[architecture/content-storage-layer.md](./architecture/content-storage-layer.md)** - MongoDB and Redis storage architecture
- **[architecture/PERFORMANCE_OPTIMIZATIONS_SUMMARY.md](./architecture/PERFORMANCE_OPTIMIZATIONS_SUMMARY.md)** - Complete performance optimization summary
  - 99.6% faster JavaScript file serving
  - Redis-based caching implementation
  - Production-grade HTTP headers
  - Comprehensive monitoring and testing
- **[architecture/SCHEDULER_INTEGRATION_SUMMARY.md](./architecture/SCHEDULER_INTEGRATION_SUMMARY.md)** - Crawler scheduler integration
- **[architecture/SCORING_AND_RANKING.md](./architecture/SCORING_AND_RANKING.md)** - Search result scoring system
- **[architecture/SPA_RENDERING.md](./architecture/SPA_RENDERING.md)** - Single Page Application rendering

#### User Guides

- **[guides/PRODUCTION_JS_MINIFICATION.md](./guides/PRODUCTION_JS_MINIFICATION.md)** - Production deployment guide for JS minification
  - Pre-built Docker images from GitHub Container Registry
  - Production environment configuration
  - Monitoring, scaling, and troubleshooting
  - Security best practices and performance optimization
- **[guides/DOCKER_HEALTH_CHECK_BEST_PRACTICES.md](./guides/DOCKER_HEALTH_CHECK_BEST_PRACTICES.md)** - Docker health check implementation
- **[guides/JS_CACHING_BEST_PRACTICES.md](./guides/JS_CACHING_BEST_PRACTICES.md)** - Production caching best practices
- **[guides/JS_CACHING_HEADERS_BEST_PRACTICES.md](./guides/JS_CACHING_HEADERS_BEST_PRACTICES.md)** - HTTP caching headers guide
- **[guides/README_STORAGE_TESTING.md](./guides/README_STORAGE_TESTING.md)** - Storage layer testing guide

#### Development Guides

- **[development/JS_MINIFIER_CLIENT_CHANGELOG.md](./development/JS_MINIFIER_CLIENT_CHANGELOG.md)** - Detailed changelog for JsMinifierClient improvements
  - Enhanced JSON parsing with robust escape sequence handling
  - Size-based method selection (JSON ≤100KB, File Upload >100KB)
  - Improved error handling and debugging output
  - Performance optimizations and bug fixes
- **[development/MONGODB_CPP_GUIDE.md](./development/MONGODB_CPP_GUIDE.md)** - MongoDB C++ driver usage guide
- **[development/template-development.md](./development/template-development.md)** - Template development guide
- **[development/cmake-version-options.md](./development/cmake-version-options.md)** - CMake configuration options

#### Troubleshooting

- **[troubleshooting/README.md](./troubleshooting/README.md)** - Troubleshooting guide index
- **[troubleshooting/FIX_MONGODB_WARNING.md](./troubleshooting/FIX_MONGODB_WARNING.md)** - Fix for MongoDB storage warning
  - Root cause analysis
  - Implementation fix
  - Testing and verification
  - Deployment guide

#### Project Organization

- **[DOCUMENTATION_CLEANUP.md](./DOCUMENTATION_CLEANUP.md)** - Documentation organization and cleanup guidelines
- **[DOCUMENTATION_ORGANIZATION_SUMMARY.md](./DOCUMENTATION_ORGANIZATION_SUMMARY.md)** - Documentation structure summary

### 📁 Directory Structure

```
docs/
├── README.md                              # This documentation index
├── DOCUMENTATION_CLEANUP.md               # Documentation organization guidelines
├── DOCUMENTATION_ORGANIZATION_SUMMARY.md  # Documentation organization summary
├── api/                                   # API endpoint documentation
│   ├── README.md                          # API documentation index
│   ├── crawler_endpoint.md                # Crawler API documentation
│   ├── search_endpoint.md                 # Search API documentation
│   ├── sponsor_endpoint.md                # Sponsor API documentation
│   ├── website_profile_endpoint.md        # Website profile API
│   └── WEBSITE_PROFILE_API_SUMMARY.md     # Website profile implementation summary
├── architecture/                          # System architecture documentation
│   ├── content-storage-layer.md           # Storage layer architecture
│   ├── lazy-connection-handling.md        # Lazy connection initialization
│   ├── PERFORMANCE_OPTIMIZATIONS.md       # Performance architecture
│   ├── PERFORMANCE_OPTIMIZATIONS_SUMMARY.md # Performance summary
│   ├── RETRY_SYSTEM_SUMMARY.md            # Retry mechanism architecture
│   ├── SCHEDULER_INTEGRATION_SUMMARY.md   # Crawler scheduler integration
│   ├── SCORING_AND_RANKING.md             # Search scoring system
│   └── SPA_RENDERING.md                   # SPA rendering architecture
├── guides/                                # User and deployment guides
│   ├── DOCKER_HEALTH_CHECK_BEST_PRACTICES.md # Docker health checks
│   ├── JS_CACHING_BEST_PRACTICES.md       # Production caching best practices
│   ├── JS_CACHING_HEADERS_BEST_PRACTICES.md # HTTP caching headers guide
│   ├── JS_MINIFICATION_CACHING_STRATEGY.md # Minification caching strategy
│   ├── PRODUCTION_JS_MINIFICATION.md      # Production JS minification deployment
│   ├── README_JS_MINIFICATION.md          # JavaScript minification features
│   ├── README_SEARCH_CORE.md              # Search core usage guide
│   └── README_STORAGE_TESTING.md          # Storage testing guide
├── development/                           # Technical development documentation
│   ├── cmake-version-options.md           # CMake configuration options
│   ├── FILE_RECEIVING_METHODS.md          # File upload implementation
│   ├── JS_MINIFICATION_STRATEGY_ANALYSIS.md # JS minification strategy
│   ├── JS_MINIFIER_CLIENT_CHANGELOG.md    # JsMinifierClient version history
│   ├── MONGODB_CPP_GUIDE.md               # MongoDB C++ driver guide
│   └── template-development.md            # Template development guide
└── troubleshooting/                       # Problem-solving and fix guides
    ├── README.md                          # Troubleshooting guide index
    ├── FIX_MONGODB_WARNING.md             # MongoDB storage warning fix
    └── MONGODB_WARNING_ANALYSIS.md        # MongoDB initialization analysis
```

### 🎯 Quick Navigation

#### For Developers

- **New to the project?** Start with [../README.md](../README.md)
- **API endpoints?** See [api/README.md](./api/README.md)
- **Architecture overview?** See [architecture/](./architecture/)
- **Working on JS minification?** See [development/JS_MINIFIER_CLIENT_CHANGELOG.md](./development/JS_MINIFIER_CLIENT_CHANGELOG.md)
- **Implementing caching?** See [guides/JS_CACHING_BEST_PRACTICES.md](./guides/JS_CACHING_BEST_PRACTICES.md)
- **MongoDB C++ development?** See [development/MONGODB_CPP_GUIDE.md](./development/MONGODB_CPP_GUIDE.md)
- **Troubleshooting issues?** Check [troubleshooting/](./troubleshooting/)
- **Contributing documentation?** Check [DOCUMENTATION_CLEANUP.md](./DOCUMENTATION_CLEANUP.md)

#### For Operations

- **Production deployment?** See [guides/PRODUCTION_JS_MINIFICATION.md](./guides/PRODUCTION_JS_MINIFICATION.md)
- **Docker health checks?** See [guides/DOCKER_HEALTH_CHECK_BEST_PRACTICES.md](./guides/DOCKER_HEALTH_CHECK_BEST_PRACTICES.md)
- **Deployment guide** - See [../README.md](../README.md#deployment)
- **Configuration** - See [../config/](../config/) directory
- **Docker setup** - See [../docker/](../docker/) directory
- **Troubleshooting?** See [troubleshooting/README.md](./troubleshooting/README.md)

### 🔍 Search Engine Components

#### Core Components

- **Search Engine** - C++20 implementation with RedisSearch integration
- **Web Crawler** - Multi-threaded web crawler with content parsing
- **Storage Layer** - MongoDB and Redis storage backends
- **API Server** - uWebSockets-based HTTP/WebSocket server

#### Microservices

- **JS Minifier** - Node.js microservice for JavaScript minification
- **Redis Sync** - Python microservice for syncing MongoDB indexed_pages to Redis
- **Crawler Scheduler** - Celery-based task scheduler for progressive warm-up crawling
- **Browserless** - Headless Chrome for dynamic content rendering
- **MongoDB** - Document database for content storage
- **Redis** - In-memory database for search indexing

### 📊 Architecture Overview

```
┌─────────────────┐    HTTP/JSON    ┌──────────────────┐
│   C++ Search    │ ──────────────► │  JS Minifier     │
│    Engine       │                 │   Microservice   │
│                 │ ◄────────────── │   (Node.js)      │
└─────────────────┘    Response     └──────────────────┘
         │
         │ WebSocket
         ▼
┌─────────────────┐    HTTP/JSON    ┌──────────────────┐
│   Web Crawler   │ ──────────────► │   Browserless    │
│                 │                 │   (Chrome)       │
└─────────────────┘                 └──────────────────┘
         │
         │ Storage
         ▼
┌─────────────────┐    ┌──────────────────┐
│     MongoDB     │    │      Redis       │
│   (Content)     │    │   (Search Index) │
└─────────────────┘    └──────────────────┘
         │                       ▲
         │                       │
         └───────────────────────┘
                    │
                    ▼
         ┌──────────────────────┐
         │   Redis Sync        │
         │   (Background Sync) │
         └──────────────────────┘
```

### 🛠️ Development Workflow

#### Building the Project

```bash
# Build with Docker
docker-compose -f docker/docker-compose.yml up --build

# Build locally
./build.sh
```

#### Running Tests

```bash
# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test categories
ctest -L "search_core"
ctest -L "integration"
```

#### Development Tools

- **Code Formatting** - Prettier configuration in [../.prettierrc.json](../.prettierrc.json)
- **Git Hooks** - See [../.github/](../.github/) directory
- **VS Code** - Configuration in [../.vscode/](../.vscode/) directory

### 📈 Performance & Monitoring

#### Key Metrics

- **Search Latency** - Target <5ms p95 for local Redis operations
- **Crawl Throughput** - Configurable via domain managers
- **Memory Usage** - Optimized for large-scale crawling
- **Storage Efficiency** - Compressed content storage

#### Monitoring

- **WebSocket Logs** - Real-time crawl progress
- **Health Checks** - Service availability monitoring
- **Performance Metrics** - Built-in timing and statistics

### 🔒 Security & Best Practices

#### Security Features

- **CSP Headers** - Content Security Policy implementation
- **Input Validation** - Comprehensive URL and content sanitization
- **Rate Limiting** - Configurable crawl rate limits
- **Error Handling** - Graceful failure recovery

#### Development Best Practices

- **Code Quality** - C++20 standards with comprehensive testing
- **Documentation** - Inline comments and detailed changelogs
- **Error Handling** - Robust error recovery and logging
- **Performance** - Optimized algorithms and data structures

### 🤝 Contributing

#### Documentation Standards

- **Markdown Format** - Use standard markdown with proper headings
- **Code Examples** - Include working code snippets
- **Screenshots** - Add visual aids where helpful
- **Version History** - Maintain detailed changelogs

#### Code Standards

- **C++20** - Use modern C++ features
- **Testing** - Comprehensive unit and integration tests
- **Documentation** - Clear inline comments and API docs
- **Performance** - Optimize for speed and memory efficiency

### 📞 Support & Resources

#### Getting Help

- **Issues** - Report bugs via GitHub issues
- **Discussions** - Join project discussions
- **Documentation** - Check this directory for guides
- **Examples** - See [../examples/](../examples/) directory

#### External Resources

- **uWebSockets** - [https://github.com/uNetworking/uWebSockets](https://github.com/uNetworking/uWebSockets)
- **RedisSearch** - [https://redis.io/docs/stack/search/](https://redis.io/docs/stack/search/)
- **MongoDB** - [https://docs.mongodb.com/](https://docs.mongodb.com/)
- **Terser** - [https://terser.org/](https://terser.org/)

---

**Last Updated**: October 2025  
**Version**: 2.1  
**Maintainer**: Search Engine Core Team
