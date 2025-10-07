# Crawler API Documentation

## Overview

The crawler API provides comprehensive web crawling capabilities with enhanced
SPA (Single Page Application) rendering support. The system automatically
detects JavaScript-heavy websites and uses headless browser rendering to extract
fully rendered content.

## Performance Optimizations (Latest)

### ⚡ **Speed Improvements**

- **Render Time**: 8-12 seconds per page (vs 22-24 seconds before)
- **Wait Times**: 8s network idle, 2s simple wait (60% faster)
- **Timeouts**: 15s max SPA rendering (50% faster)
- **Concurrent Sessions**: 10 Chrome instances (100% more)
- **Memory**: 2GB allocation (100% more)

### 📊 **Expected Performance**

- **Before**: 3+ minutes for 5 pages
- **After**: 1-2 minutes for 5 pages (50-70% faster)

## Endpoints

### POST /api/crawl/add-site

Add a new site to the crawl queue with optimized SPA rendering.

#### Request Body

```json
{
  "url": "https://www.digikala.com",
  "maxPages": 100,
  "maxDepth": 3,
  "spaRenderingEnabled": true,
  "includeFullContent": false,
  "browserlessUrl": "http://browserless:3000",
  "timeout": 15000,
  "politenessDelay": 500
}
```

#### Parameters

| Parameter             | Type    | Default                   | Description                            |
| --------------------- | ------- | ------------------------- | -------------------------------------- |
| `url`                 | string  | **required**              | Seed URL to start crawling             |
| `maxPages`            | integer | 1000                      | Maximum pages to crawl                 |
| `maxDepth`            | integer | 5                         | Maximum crawl depth                    |
| `spaRenderingEnabled` | boolean | true                      | Enable SPA rendering                   |
| `includeFullContent`  | boolean | false                     | Store full HTML content                |
| `browserlessUrl`      | string  | "http://browserless:3000" | Browserless service URL                |
| `timeout`             | integer | 15000                     | Request timeout in milliseconds        |
| `politenessDelay`     | integer | 500                       | Delay between requests in milliseconds |

#### Response

```json
{
  "success": true,
  "message": "Site added to crawl queue successfully",
  "data": {
    "sessionId": "crawl_1754960920522_0",
    "url": "https://www.digikala.com",
    "maxPages": 100,
    "spaRenderingEnabled": true,
    "includeFullContent": false,
    "status": "queued"
  }
}
```

### GET /api/crawl/status

Get the current status of all crawl sessions.

#### Response

```json
{
  "success": true,
  "data": {
    "activeSessions": 1,
    "totalSessions": 5,
    "sessions": [
      {
        "sessionId": "crawl_1754960920522_0",
        "url": "https://www.digikala.com",
        "status": "running",
        "pagesCrawled": 3,
        "maxPages": 100,
        "startTime": "2025-08-12T02:38:40.522Z",
        "spaRenderingEnabled": true
      }
    ]
  }
}
```

### GET /api/crawl/details

Get detailed information about a specific crawl session or URL.

#### Query Parameters

| Parameter   | Type   | Description                   |
| ----------- | ------ | ----------------------------- |
| `sessionId` | string | Session ID to get details for |
| `url`       | string | URL to get details for        |

#### Response

```json
{
  "success": true,
  "data": {
    "sessionId": "crawl_1754960920522_0",
    "url": "https://www.digikala.com",
    "status": "completed",
    "pagesCrawled": 5,
    "maxPages": 100,
    "startTime": "2025-08-12T02:38:40.522Z",
    "endTime": "2025-08-12T02:42:14.000Z",
    "duration": "00:03:34",
    "spaRenderingEnabled": true,
    "logs": [
      {
        "url": "https://www.digikala.com",
        "status": "SUCCESS",
        "httpStatusCode": 200,
        "contentSize": 509673,
        "title": "فروشگاه اینترنتی دیجی‌کالا",
        "renderingMethod": "headless_browser",
        "fetchDuration": 23715,
        "timestamp": "2025-08-12T02:39:04.000Z"
      }
    ]
  }
}
```

## SPA Rendering Configuration

### Browserless Service (Optimized)

The crawler uses an optimized browserless configuration for maximum performance:

```yaml
# docker-compose.yml
browserless:
  image: browserless/chrome:latest
  environment:
    - MAX_CONCURRENT_SESSIONS=10 # Doubled from 5
    - PREBOOT_CHROME=true # Enabled for faster startup
    - CONNECTION_TIMEOUT=15000 # Reduced from 30000
    - CHROME_REFRESH_TIME=60000 # Increased from 30000
    - QUEUE_LIMIT=100 # Doubled from 50
    - MAX_CPU_PERCENT=90 # Increased from 80
    - MAX_MEMORY_PERCENT=90 # Increased from 80
    - KEEP_ALIVE=true # Enabled for connection reuse
  deploy:
    resources:
      limits:
        memory: 2G # Doubled from 1G
      reservations:
        memory: 1G # Doubled from 512M
```

### Optimized Payload

The browserless client sends optimized payloads for faster rendering:

```cpp
// Optimized browserless payload
json payload = {
    {"url", cleanedUrl},
    {"waitFor", wait_for_network_idle ? 8000 : 2000},  // 8s/2s (vs 20s/3s)
    {"rejectResourceTypes", json::array({"image", "media", "font"})},
    {"gotoOptions", {
        {"waitUntil", "domcontentloaded"},
        {"timeout", 10000}
    }},
    {"viewport", {
        {"width", 1280},
        {"height", 720}
    }}
};
```

## Performance Metrics

### Content Size Comparison

| Mode                                               | Content Size | Storage    | Use Case           |
| -------------------------------------------------- | ------------ | ---------- | ------------------ |
| **Preview Mode** (`includeFullContent: false`)     | ~500 bytes   | Minimal    | Discovery crawling |
| **Full Content Mode** (`includeFullContent: true`) | ~580KB       | 74x larger | Search indexing    |

### Rendering Performance (Optimized)

| Metric               | Static HTML | SPA Rendered (Before) | SPA Rendered (After) | Improvement      |
| -------------------- | ----------- | --------------------- | -------------------- | ---------------- |
| **Content Size**     | ~7KB        | ~580KB                | ~580KB               | **74x larger**   |
| **Title Extraction** | Often empty | ✅ Dynamic titles     | ✅ Dynamic titles    | **Success**      |
| **Render Time**      | 1-2s        | 25-35s                | **8-12s**            | **60% faster**   |
| **Success Rate**     | 100%        | 95% (with fallback)   | 95% (with fallback)  | High reliability |

## Configuration Options

### Environment Variables

| Variable                | Description                    | Default                 | Optimized Value         |
| ----------------------- | ------------------------------ | ----------------------- | ----------------------- |
| `BROWSERLESS_URL`       | Browserless service URL        | http://browserless:3000 | http://browserless:3000 |
| `SPA_RENDERING_ENABLED` | Global SPA rendering toggle    | true                    | true                    |
| `DEFAULT_TIMEOUT`       | Default rendering timeout (ms) | 30000                   | **15000**               |
| `POLITENESS_DELAY`      | Delay between requests (ms)    | 1000                    | **500**                 |

### Docker Compose Configuration

```yaml
services:
  search-engine:
    environment:
      - BROWSERLESS_URL=http://browserless:3000
      - SPA_RENDERING_ENABLED=true
      - DEFAULT_TIMEOUT=15000
      - POLITENESS_DELAY=500

  browserless:
    image: browserless/chrome:latest
    environment:
      - MAX_CONCURRENT_SESSIONS=10
      - PREBOOT_CHROME=true
      - CONNECTION_TIMEOUT=15000
      - CHROME_REFRESH_TIME=60000
      - QUEUE_LIMIT=100
      - MAX_CPU_PERCENT=90
      - MAX_MEMORY_PERCENT=90
      - KEEP_ALIVE=true
    deploy:
      resources:
        limits:
          memory: 2G
        reservations:
          memory: 1G
```

## Best Practices

### 1. Timeout Configuration

- **Standard sites**: 15 seconds (optimized)
- **Complex SPAs**: 15 seconds (capped for speed)
- **Heavy JavaScript sites**: 15 seconds (capped for speed)

### 2. Content Storage Strategy

- **Discovery crawling**: `includeFullContent: false`
- **Search indexing**: `includeFullContent: true`
- **Monitor storage**: 580KB vs 7KB per page

### 3. Error Handling

- Always implements graceful fallback to static HTML
- Detailed error logging for debugging
- Non-blocking operation when browserless unavailable

### 4. Performance Monitoring

- Track SPA detection accuracy
- Monitor rendering success rates
- Watch browserless resource usage
- Monitor render times (target: 8-12 seconds)

## Troubleshooting

### Common Issues

1. **HTTP 400 Validation Errors**
   - Ensure browserless payload doesn't include invalid fields
   - Use only supported browserless parameters

2. **Slow Rendering**
   - Check browserless resource limits
   - Verify concurrent session count
   - Monitor memory usage

3. **Connection Timeouts**
   - Verify browserless service is running
   - Check network connectivity
   - Review timeout settings

### Performance Tuning

1. **For Faster Crawling**
   - Reduce `waitFor` times (8s/2s recommended)
   - Increase concurrent sessions (10 recommended)
   - Enable resource blocking

2. **For Better Quality**
   - Increase `waitFor` times if content is missing
   - Disable resource blocking for complete rendering
   - Increase timeout for complex SPAs

## Migration Guide

### From Previous Version

1. **Update Docker Compose**: Use new browserless configuration
2. **Rebuild Services**: Apply optimized settings
3. **Test Performance**: Expect 50-70% faster crawling
4. **Monitor Logs**: Verify no validation errors

### Performance Expectations

- **Before Optimization**: 3+ minutes for 5 pages
- **After Optimization**: 1-2 minutes for 5 pages
- **Render Time**: 8-12 seconds per page (vs 22-24 seconds)
- **Success Rate**: Maintained at 95%+

## Content Validation & Quality Control

The crawler implements comprehensive validation to ensure only high-quality, searchable content is stored:

### Content Type Validation

Only pages with text-based content types are saved:

- ✅ **Allowed**: `text/html`, `text/plain`, `application/json`, `application/xml`, `text/xml`, `application/rss+xml`, `application/atom+xml`
- ❌ **Blocked**: `image/*`, `video/*`, `audio/*`, `application/pdf`, `application/zip`, binary files

### Content Quality Validation

Pages must have both meaningful content:

- ✅ **Required**: Non-empty title AND text content
- ❌ **Skipped**: Empty pages, redirect-only pages, error pages without content

### URL Validation

Only valid web URLs are processed:

- ✅ **Allowed**: HTTP and HTTPS URLs
- ❌ **Blocked**: `mailto:`, `tel:`, `javascript:`, `data:`, `ftp:`, `file:`, browser extensions

### Redirect Handling

- **Automatic Following**: HTTP redirects are followed to final destination
- **Final URL Storage**: Stores the final redirected URL, not the original
- **Canonical URLs**: Uses canonical URLs for deduplication

### Validation Benefits

- **Storage Efficiency**: Prevents storing binary files and media content
- **Search Quality**: Only text-based content is indexed for search
- **Performance**: Reduces database size and improves search speed
- **Resource Management**: Avoids wasting storage on non-searchable content
