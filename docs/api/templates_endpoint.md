# Templates API Documentation

## Overview

The Templates API provides a comprehensive template system for common crawling patterns, enabling developers to standardize crawler behavior across similar site types. This system reduces repetitive configuration and improves developer productivity by offering pre-built templates for news sites, e-commerce platforms, blogs, and more.

## Key Features

### 🎯 **Pre-built Templates**
- **7 Common Site Types**: News, e-commerce, blog, corporate, documentation, forum, social media
- **Optimized Configurations**: Each template includes site-specific crawling parameters
- **Selector Patterns**: CSS selectors tailored for each site type's content structure

### ⚡ **Template Management**
- **CRUD Operations**: Create, read, update, and delete templates
- **Template Validation**: Input validation with proper error handling
- **Persistence**: Templates automatically saved to disk
- **Backward Compatibility**: Existing crawl API continues to work

### 🔧 **Template Application**
- **Config Overrides**: Apply template settings to CrawlConfig
- **Selector Patterns**: Automatically set article, title, and content selectors
- **Flexible Usage**: Use templates with optional parameter overrides

## Endpoints

### GET /api/templates

List all available templates in the system.

#### Response

```json
{
  "templates": [
    {
      "name": "news-site",
      "description": "Template for news websites",
      "config": {
        "maxPages": 500,
        "maxDepth": 3,
        "spaRenderingEnabled": true,
        "extractTextContent": true,
        "politenessDelay": 1000
      },
      "patterns": {
        "articleSelectors": ["article", ".post", ".story"],
        "titleSelectors": ["h1", ".headline", ".title"],
        "contentSelectors": [".content", ".body", ".article-body"]
      }
    }
  ]
}
```

#### Example

```bash
curl http://localhost:3000/api/templates
```

### GET /api/templates/:name

Get a specific template by name.

#### Parameters

| Parameter | Type   | Description                    |
|-----------|--------|--------------------------------|
| `name`    | string | Template name (URL encoded)    |

#### Response

```json
{
  "name": "news-site",
  "description": "Template for news websites",
  "config": {
    "maxPages": 500,
    "maxDepth": 3,
    "spaRenderingEnabled": true,
    "extractTextContent": true,
    "politenessDelay": 1000
  },
  "patterns": {
    "articleSelectors": ["article", ".post", ".story"],
    "titleSelectors": ["h1", ".headline", ".title"],
    "contentSelectors": [".content", ".body", ".article-body"]
  }
}
```

#### Example

```bash
curl http://localhost:3000/api/templates/news-site
```

### POST /api/templates

Create a new template.

#### Request Body

```json
{
  "name": "custom-site",
  "description": "Custom template for specific site type",
  "config": {
    "maxPages": 200,
    "maxDepth": 2,
    "spaRenderingEnabled": false,
    "extractTextContent": true,
    "politenessDelay": 1500
  },
  "patterns": {
    "articleSelectors": [".custom-article", ".post-item"],
    "titleSelectors": ["h1", ".custom-title"],
    "contentSelectors": [".custom-content", ".main-text"]
  }
}
```

#### Parameters

| Parameter | Type   | Required | Description                           |
|-----------|--------|----------|---------------------------------------|
| `name`    | string | Yes      | Template name (1-50 chars, alphanumeric with hyphens/underscores) |
| `description` | string | No    | Template description                   |
| `config`  | object | No       | Crawling configuration overrides      |
| `patterns` | object | No     | CSS selector patterns                  |

#### Config Parameters

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `maxPages` | integer | 1-10000 | Maximum pages to crawl |
| `maxDepth` | integer | 1-10 | Maximum crawl depth |
| `spaRenderingEnabled` | boolean | - | Enable SPA rendering |
| `extractTextContent` | boolean | - | Extract text content |
| `politenessDelay` | integer | 0-60000 | Delay between requests (ms) |

#### Patterns Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `articleSelectors` | array of strings | CSS selectors for article containers |
| `titleSelectors` | array of strings | CSS selectors for article titles |
| `contentSelectors` | array of strings | CSS selectors for article content |

#### Response

```json
{
  "status": "accepted",
  "message": "Template stored",
  "name": "custom-site"
}
```

#### Example

```bash
curl -X POST http://localhost:3000/api/templates \
  -H "Content-Type: application/json" \
  -d '{
    "name": "custom-site",
    "description": "Custom template",
    "config": {
      "maxPages": 200,
      "maxDepth": 2
    },
    "patterns": {
      "articleSelectors": [".custom-article"],
      "titleSelectors": ["h1"],
      "contentSelectors": [".content"]
    }
  }'
```

### DELETE /api/templates/:name

Delete a template by name.

#### Parameters

| Parameter | Type   | Description                    |
|-----------|--------|--------------------------------|
| `name`    | string | Template name (URL encoded)    |

#### Response

```json
{
  "status": "deleted",
  "name": "custom-site"
}
```

#### Example

```bash
curl -X DELETE http://localhost:3000/api/templates/custom-site
```

### POST /api/crawl/add-site-with-template

Start crawling using a template configuration.

#### Request Body

```json
{
  "url": "https://example-news.com",
  "template": "news-site",
  "force": true,
  "stopPreviousSessions": false,
  "browserlessUrl": "http://browserless:3000",
  "requestTimeout": 15000
}
```

#### Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `url` | string | Yes | - | URL to crawl |
| `template` | string | Yes | - | Template name to apply |
| `force` | boolean | No | true | Force crawl even if already exists |
| `stopPreviousSessions` | boolean | No | false | Stop previous crawl sessions |
| `browserlessUrl` | string | No | template default | Browserless service URL |
| `requestTimeout` | integer | No | template default | Request timeout in ms |

#### Response

```json
{
  "success": true,
  "message": "Crawl session started with template",
  "data": {
    "sessionId": "crawl_1757365587937_0",
    "url": "https://example-news.com",
    "template": "news-site",
    "appliedConfig": {
      "maxPages": 500,
      "maxDepth": 3,
      "politenessDelay": 1000,
      "spaRenderingEnabled": true,
      "extractTextContent": true,
      "requestTimeout": 15000,
      "browserlessUrl": "http://browserless:3000"
    }
  }
}
```

#### Example

```bash
curl -X POST http://localhost:3000/api/crawl/add-site-with-template \
  -H "Content-Type: application/json" \
  -d '{
    "url": "https://example-news.com",
    "template": "news-site"
  }'
```

## Pre-built Templates

### News Site Template (`news-site`)

Optimized for news websites and online publications.

```json
{
  "config": {
    "maxPages": 500,
    "maxDepth": 3,
    "spaRenderingEnabled": true,
    "extractTextContent": true,
    "politenessDelay": 1000
  },
  "patterns": {
    "articleSelectors": ["article", ".post", ".story", ".news-item"],
    "titleSelectors": ["h1", ".headline", ".title", ".article-title"],
    "contentSelectors": [".content", ".body", ".article-body", ".post-content"]
  }
}
```

### E-commerce Site Template (`ecommerce-site`)

Designed for online stores and product listings.

```json
{
  "config": {
    "maxPages": 800,
    "maxDepth": 4,
    "extractTextContent": true,
    "politenessDelay": 800
  },
  "patterns": {
    "articleSelectors": [".product", ".product-item", ".product-card"],
    "titleSelectors": ["h1", ".product-title", ".title"],
    "contentSelectors": [".description", ".product-description", ".details"]
  }
}
```

### Blog Site Template (`blog-site`)

Perfect for personal blogs and content management systems.

```json
{
  "config": {
    "maxPages": 300,
    "maxDepth": 2,
    "spaRenderingEnabled": false,
    "extractTextContent": true,
    "politenessDelay": 1200
  },
  "patterns": {
    "articleSelectors": ["article", ".post", ".blog-post", ".entry"],
    "titleSelectors": ["h1", ".post-title", ".entry-title", ".blog-title"],
    "contentSelectors": [".content", ".post-content", ".entry-content", ".blog-content"]
  }
}
```

### Corporate Site Template (`corporate-site`)

Suitable for business websites and corporate pages.

```json
{
  "config": {
    "maxPages": 150,
    "maxDepth": 2,
    "spaRenderingEnabled": false,
    "extractTextContent": true,
    "politenessDelay": 1000
  },
  "patterns": {
    "articleSelectors": [".page-content", ".main-content", ".content", ".page"],
    "titleSelectors": ["h1", ".page-title", ".title", ".heading"],
    "contentSelectors": [".content", ".main-content", ".page-content", ".body"]
  }
}
```

### Documentation Site Template (`documentation-site`)

Optimized for technical documentation and API references.

```json
{
  "config": {
    "maxPages": 1000,
    "maxDepth": 5,
    "spaRenderingEnabled": true,
    "extractTextContent": true,
    "politenessDelay": 600
  },
  "patterns": {
    "articleSelectors": [".documentation", ".doc-content", ".content", ".page"],
    "titleSelectors": ["h1", ".page-title", ".doc-title", ".title"],
    "contentSelectors": [".content", ".doc-content", ".main-content", ".body"]
  }
}
```

### Forum Site Template (`forum-site`)

Designed for discussion forums and community sites.

```json
{
  "config": {
    "maxPages": 400,
    "maxDepth": 3,
    "spaRenderingEnabled": false,
    "extractTextContent": true,
    "politenessDelay": 1500
  },
  "patterns": {
    "articleSelectors": [".post", ".topic", ".thread", ".message"],
    "titleSelectors": ["h1", ".post-title", ".topic-title", ".thread-title"],
    "contentSelectors": [".content", ".post-content", ".message-content", ".body"]
  }
}
```

### Social Media Template (`social-media`)

Suitable for social platforms and user-generated content.

```json
{
  "config": {
    "maxPages": 200,
    "maxDepth": 2,
    "spaRenderingEnabled": true,
    "extractTextContent": true,
    "politenessDelay": 2000
  },
  "patterns": {
    "articleSelectors": [".post", ".tweet", ".status", ".update"],
    "titleSelectors": ["h1", ".post-title", ".status-title", ".title"],
    "contentSelectors": [".content", ".post-content", ".status-content", ".body"]
  }
}
```

## Error Handling

### Validation Errors

```json
{
  "error": "Bad Request",
  "message": "name must be 1-50 characters, alphanumeric with hyphens/underscores only"
}
```

### Template Not Found

```json
{
  "error": "Not Found",
  "message": "template not found"
}
```

### Invalid JSON

```json
{
  "error": "Bad Request",
  "message": "Invalid JSON format"
}
```

## Quick Start Examples

### 1. List Available Templates

```bash
curl http://localhost:3000/api/templates | jq '.templates[].name'
```

### 2. Use News Template for Crawling

```bash
curl -X POST http://localhost:3000/api/crawl/add-site-with-template \
  -H "Content-Type: application/json" \
  -d '{
    "url": "https://www.bbc.com/news",
    "template": "news-site"
  }'
```

### 3. Create Custom Template

```bash
curl -X POST http://localhost:3000/api/templates \
  -H "Content-Type: application/json" \
  -d '{
    "name": "my-blog-template",
    "description": "Template for my personal blog",
    "config": {
      "maxPages": 100,
      "maxDepth": 2,
      "politenessDelay": 2000
    },
    "patterns": {
      "articleSelectors": [".blog-post"],
      "titleSelectors": ["h1.blog-title"],
      "contentSelectors": [".blog-content"]
    }
  }'
```

### 4. Use Custom Template

```bash
curl -X POST http://localhost:3000/api/crawl/add-site-with-template \
  -H "Content-Type: application/json" \
  -d '{
    "url": "https://myblog.com",
    "template": "my-blog-template"
  }'
```

## Integration with Existing API

The template system is fully backward compatible with the existing crawler API. You can continue using:

- `POST /api/crawl/add-site` - Original crawl API (still works)
- `GET /api/crawl/status` - Crawl status (works with template-based crawls)
- `GET /api/crawl/details` - Crawl details (works with template-based crawls)

Templates simply provide a standardized way to configure these existing endpoints.
