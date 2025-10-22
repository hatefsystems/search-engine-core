# Content Storage Layer Documentation

## Overview

The Content Storage Layer is a critical component of the search engine that
manages the storage and retrieval of crawled web data. It provides a
dual-storage architecture that separates structured metadata (MongoDB) from
full-text searchable content (RedisSearch), enabling both fast search
capabilities and flexible data querying.

## Architecture

### Dual Storage Design

The Content Storage Layer implements a sophisticated dual-storage architecture:

1. **MongoDB**: Stores structured indexed pages with detailed metadata
2. **RedisSearch**: Handles full-text search indexing and real-time search
   queries
3. **ContentStorage**: Unified interface that coordinates both storage systems

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Crawler       │───▶│  ContentStorage  │───▶│   Search API    │
│   Module        │    │                  │    │                 │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │  MongoDBStorage │
                    │  (Metadata)     │
                    └─────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ RedisSearchStorage│
                    │ (Full-text)     │
                    └─────────────────┘
```

## Components

### 1. IndexedPage Schema

The `IndexedPage` struct defines the MongoDB schema for website metadata:

```cpp
struct IndexedPage {
    std::optional<std::string> id;           // MongoDB ObjectId
    std::string domain;                      // e.g., "example.com"
    std::string url;                         // Full URL
    std::string title;                       // Page title
    std::optional<std::string> description;  // Meta description

    // Content categorization
    std::vector<std::string> keywords;       // Extracted keywords
    std::optional<std::string> language;     // Detected language
    std::optional<std::string> category;     // Site category

    // Technical metadata
    CrawlMetadata crawlMetadata;             // Crawl-specific data

    // SEO and content metrics
    std::optional<int> pageRank;             // PageRank score
    std::optional<double> contentQuality;    // Quality score (0-1)
    std::optional<int> wordCount;            // Word count
    std::optional<bool> isMobile;            // Mobile-friendly
    std::optional<bool> hasSSL;              // SSL certificate

    // Link analysis
    std::vector<std::string> outboundLinks;  // Found links
    std::optional<int> inboundLinkCount;     // Backlink count

    // Search relevance
    bool isIndexed;                          // In search index
    std::chrono::system_clock::time_point lastModified;
    std::chrono::system_clock::time_point indexedAt;

    // Additional metadata
    std::optional<std::string> author;       // Content author
    std::optional<std::string> publisher;    // Publisher name
    std::optional<std::chrono::system_clock::time_point> publishDate;
};
```

### 2. MongoDBStorage

Handles structured data storage with the following features:

#### Key Features:

- **BSON Conversion**: Automatic conversion between C++ objects and MongoDB BSON
- **Index Management**: Automatic creation of performance indexes
- **Connection Pooling**: Efficient connection management
- **Error Handling**: Comprehensive error reporting

#### Core Operations:

```cpp
// Store and retrieve indexed pages
Result<std::string> storeIndexedPage(const IndexedPage& page);
Result<IndexedPage> getSiteProfile(const std::string& url);
Result<IndexedPage> getSiteProfileById(const std::string& id);
// Note: updateIndexedPage has been removed - use storeIndexedPage for both insert and update operations
Result<bool> deleteSiteProfile(const std::string& url);

// Batch operations
Result<std::vector<IndexedPage>> getSiteProfilesByDomain(const std::string& domain);
Result<std::vector<IndexedPage>> getSiteProfilesByCrawlStatus(CrawlStatus status);

// Statistics
Result<int64_t> getTotalSiteCount();
Result<int64_t> getSiteCountByStatus(CrawlStatus status);
```

### Crawl Logs Schema (MongoDB `crawl_logs` collection)

The crawler writes a lightweight history record for each processed URL. Fields
include:

- url: string
- domain: string
- crawlTime: date
- status: string (e.g., SUCCESS, FAILED)
- httpStatusCode: int
- errorMessage: string? (optional)
- contentSize: int64
- contentType: string
- links: string[]
- title: string? (optional)
- description: string? (optional)
- downloadTimeMs: int64? (optional) — total time in milliseconds from download
  start to completion

This record is used by the `/api/crawl/details` endpoint and for operational
monitoring.

#### MongoDB Indexes:

- `url`: Unique index for fast URL lookups
- `domain`: Index for domain-based queries
- `crawlMetadata.lastCrawlStatus`: Index for status filtering
- `lastModified`: Descending index for recent content

#### Data Validation and Quality Control

The MongoDBStorage layer implements comprehensive validation to ensure only high-quality, relevant content is stored:

**Content Type Validation:**

- Only saves pages with text-based content types
- Allowed types: `text/html`, `text/plain`, `application/json`, `application/xml`, `text/xml`, `application/rss+xml`, `application/atom+xml`
- Blocks media files: images (`image/*`), videos (`video/*`), audio (`audio/*`), PDFs (`application/pdf`), archives (`application/zip`)

**Content Quality Validation:**

- Requires both `title` and `textContent` to be present and non-empty
- Skips pages without meaningful content (redirect pages, error pages, empty pages)
- Prevents storage of incomplete or malformed content

**URL Validation:**

- Filters out invalid URL schemes: `mailto:`, `tel:`, `javascript:`, `data:`, `ftp:`, `file:`, browser extensions
- Validates HTTP/HTTPS URL format using regex patterns
- Prevents crawling of non-web resources

**Redirect Handling:**

- Automatically follows HTTP redirects and stores the final destination URL
- Uses canonical URLs for deduplication to prevent duplicate content
- Maintains redirect chains in crawl metadata

**Validation Flow:**

1. Content type check (HTML/text only)
2. Title and text content validation (both required)
3. URL scheme validation (HTTP/HTTPS only)
4. Canonical URL generation for deduplication
5. Final storage with upsert logic

### 3. RedisSearchStorage

Manages full-text search capabilities with RediSearch:

#### Key Features:

- **Full-Text Indexing**: Advanced text search with ranking
- **Real-Time Search**: Sub-millisecond search response times
- **Faceted Search**: Filter by domain, language, category
- **Highlighting**: Search term highlighting in results
- **Suggestions**: Autocomplete functionality

#### Search Schema

```
Field         Type      Weight  Features
url           TEXT      -       SORTABLE, NOINDEX
title         TEXT      5.0     High weight for relevance
content       TEXT      1.0     Main content body
domain        TAG       -       SORTABLE, filterable
keywords      TAG       -       Filterable
description   TEXT      2.0     Medium weight
language      TAG       -       Filterable
category      TAG       -       Filterable
indexed_at    NUMERIC   -       SORTABLE
score         NUMERIC   -       SORTABLE
```

#### Search Operations:

```cpp
// Document management
Result<bool> indexDocument(const SearchDocument& document);
Result<bool> indexSiteProfile(const IndexedPage& page, const std::string& content);
Result<bool> updateDocument(const SearchDocument& document);
Result<bool> deleteDocument(const std::string& url);

// Search operations
Result<SearchResponse> search(const SearchQuery& query);
Result<SearchResponse> searchSimple(const std::string& query, int limit = 10);
Result<std::vector<std::string>> suggest(const std::string& prefix, int limit = 5);

// Index management
Result<bool> initializeIndex();
Result<int64_t> getDocumentCount();
```

### 4. ContentStorage (Unified Interface)

The `ContentStorage` class provides a high-level interface that coordinates both
storage systems:

#### Key Features:

- **Automatic Conversion**: Converts `CrawlResult` to `IndexedPage` and
  `SearchDocument`
- **Dual Storage**: Automatically stores in both MongoDB and RedisSearch
- **Consistency Management**: Ensures data consistency across storage systems
- **Error Recovery**: Graceful handling of partial failures

#### High-Level Operations:

```cpp
// Primary operations
Result<std::string> storeCrawlResult(const CrawlResult& crawlResult);
Result<bool> updateCrawlResult(const CrawlResult& crawlResult);

// Retrieval operations
Result<IndexedPage> getSiteProfile(const std::string& url);
Result<SearchResponse> search(const SearchQuery& query);
Result<SearchResponse> searchSimple(const std::string& query, int limit = 10);

// Batch operations
Result<std::vector<std::string>> storeCrawlResults(const std::vector<CrawlResult>& crawlResults);

// Management operations
Result<bool> initializeIndexes();
Result<bool> testConnections();
Result<std::unordered_map<std::string, std::string>> getStorageStats();
```

## Data Flow

### 1. Storing Crawl Results

```
CrawlResult → ContentStorage → {
    ├─ Convert to IndexedPage → MongoDBStorage
    └─ Extract searchable content → RedisSearchStorage
}
```

1. **Input**: `CrawlResult` from crawler
2. **Conversion**: Transform to `IndexedPage` with metadata extraction
3. **MongoDB Storage**: Store structured data with indexes
4. **Content Extraction**: Create searchable text from title, description, and
   content
5. **Redis Indexing**: Index for full-text search
6. **Result**: Return MongoDB ObjectId

### 2. Search Process

```
Search Query → RedisSearchStorage → {
    ├─ Parse query and filters
    ├─ Execute FT.SEARCH command
    ├─ Rank and score results
    └─ Return SearchResponse
}
```

### 3. Profile Retrieval

```
URL → MongoDBStorage → {
    ├─ Query by URL index
    ├─ Convert BSON to IndexedPage
    └─ Return structured data
}
```

## Configuration

### MongoDB Configuration

```cpp
MongoDBStorage storage(
    "mongodb://localhost:27017",  // Connection string
    "search-engine"               // Database name
);
```

**Connection String Examples:**

- Local: `mongodb://localhost:27017`
- Authenticated: `mongodb://user:pass@localhost:27017/dbname`
- Replica Set: `mongodb://host1:27017,host2:27017/dbname?replicaSet=rs0`

### Redis Configuration

```cpp
RedisSearchStorage storage(
    "tcp://127.0.0.1:6379",  // Connection string
    "search_index",          // Index name
    "doc:"                   // Key prefix
);
```

**Connection String Examples:**

- Local: `tcp://127.0.0.1:6379`
- Authenticated: `tcp://user:pass@127.0.0.1:6379`
- SSL: `tls://127.0.0.1:6380`

## Performance Considerations

### MongoDB Optimization

1. **Indexes**: Automatic creation of performance indexes
2. **Connection Pooling**: Reuse connections for efficiency
3. **Batch Operations**: Support for bulk inserts and updates
4. **Query Optimization**: Efficient BSON conversion

### Redis Optimization

1. **Memory Usage**: Efficient document storage with compression
2. **Search Performance**: Optimized FT.SEARCH queries
3. **Index Size**: Configurable field weights and options
4. **Concurrent Access**: Thread-safe operations

### Recommended Settings

**MongoDB:**

- Use SSD storage for better I/O performance
- Configure appropriate connection pool size
- Enable compression for network traffic
- Use read preferences for scaling

**Redis:**

- Allocate sufficient memory for search index
- Configure appropriate maxmemory policy
- Use Redis persistence for data durability
- Consider Redis Cluster for scaling

## Error Handling

The storage layer implements comprehensive error handling:

### Error Types

1. **Connection Errors**: Database/Redis unavailable
2. **Validation Errors**: Invalid data format
3. **Constraint Errors**: Duplicate keys, missing required fields
4. **Resource Errors**: Insufficient memory, disk space
5. **Timeout Errors**: Operation timeouts

### Error Recovery

```cpp
// Example error handling
auto result = storage.storeCrawlResult(crawlResult);
if (!result.isSuccess()) {
    logger.error("Storage failed: " + result.getErrorMessage());
    // Implement retry logic or fallback
}
```

### Monitoring

```cpp
// Health checks
auto mongoHealth = storage.getMongoStorage()->testConnection();
auto redisHealth = storage.getRedisStorage()->testConnection();

// Statistics
auto stats = storage.getStorageStats();
// Returns: mongodb_total_documents, redis_indexed_documents, etc.
```

## Testing

### Unit Tests

The storage layer includes comprehensive unit tests:

1. **MongoDB Tests** (`test_mongodb_storage.cpp`):
   - Connection and initialization
   - CRUD operations
   - Batch operations
   - Error handling
   - Data validation

2. **Redis Tests** (`test_redis_search_storage.cpp`):
   - Index creation and management
   - Document indexing and search
   - Advanced search features
   - Batch operations
   - Error scenarios

3. **Integration Tests** (`test_content_storage.cpp`):
   - End-to-end crawl result processing
   - Dual storage consistency
   - Search functionality
   - Statistics and monitoring

### Running Tests

```bash
# Build tests
cmake --build . --target storage_tests

# Run all storage tests
./storage_tests

# Run specific test categories
./storage_tests "[mongodb]"
./storage_tests "[redis]"
./storage_tests "[content]"
```

### Test Environment

Tests are designed to work with:

- Local MongoDB instance (localhost:27017)
- Local Redis instance with RediSearch (localhost:6379)
- Automatic test database/index cleanup
- Graceful handling of unavailable services

## Usage Examples

### Basic Usage

```cpp
#include "search_engine/storage/ContentStorage.h"

// Initialize storage
ContentStorage storage;

// Store crawl result
CrawlResult result = /* ... from crawler ... */;
auto storeResult = storage.storeCrawlResult(result);

if (storeResult.isSuccess()) {
    std::string profileId = storeResult.getValue();
    std::cout << "Stored with ID: " << profileId << std::endl;
}

// Search content
auto searchResult = storage.searchSimple("technology news", 10);
if (searchResult.isSuccess()) {
    auto response = searchResult.getValue();
    for (const auto& result : response.results) {
        std::cout << result.title << " - " << result.url << std::endl;
    }
}
```

### Advanced Search

```cpp
// Create advanced search query
SearchQuery query;
query.query = "artificial intelligence";
query.filters = {"domain:tech.com"};
query.language = "en";
query.limit = 20;
query.highlight = true;

auto searchResult = storage.search(query);
if (searchResult.isSuccess()) {
    auto response = searchResult.getValue();
    std::cout << "Found " << response.totalResults << " results in "
              << response.queryTime << "ms" << std::endl;
}
```

### Batch Processing

```cpp
// Process multiple crawl results
std::vector<CrawlResult> crawlResults = /* ... */;
auto batchResult = storage.storeCrawlResults(crawlResults);

if (batchResult.isSuccess()) {
    auto ids = batchResult.getValue();
    std::cout << "Stored " << ids.size() << " documents" << std::endl;
}
```

## Best Practices

### Data Modeling

1. **Normalize URLs**: Ensure consistent URL formatting
2. **Extract Keywords**: Use meaningful keyword extraction
3. **Set Quality Scores**: Implement content quality metrics
4. **Handle Duplicates**: Check for existing URLs before storing

### Performance

1. **Batch Operations**: Use batch methods for multiple documents
2. **Index Management**: Regularly monitor index performance
3. **Connection Pooling**: Reuse database connections
4. **Caching**: Implement application-level caching for frequent queries

### Monitoring

1. **Health Checks**: Regular connection testing
2. **Statistics**: Monitor document counts and search performance
3. **Error Logging**: Comprehensive error tracking
4. **Metrics**: Track storage and search latencies

### Security

1. **Authentication**: Use authenticated database connections
2. **Encryption**: Enable SSL/TLS for network traffic
3. **Access Control**: Implement proper database permissions
4. **Input Validation**: Sanitize all input data

## Future Enhancements

### Planned Features

1. **Elasticsearch Support**: Alternative to RedisSearch
2. **Distributed Storage**: Sharding and replication
3. **Advanced Analytics**: Content analysis and insights
4. **Machine Learning**: Automated categorization and quality scoring
5. **Real-time Updates**: Live index updates
6. **Backup and Recovery**: Automated backup strategies

### Scalability Improvements

1. **Horizontal Scaling**: Support for multiple storage nodes
2. **Load Balancing**: Distribute queries across instances
3. **Caching Layer**: Redis cache for frequent queries
4. **Compression**: Reduce storage footprint
5. **Archival**: Move old data to cold storage

This documentation provides a comprehensive overview of the Content Storage
Layer, enabling developers to effectively use and maintain this critical
component of the search engine.
