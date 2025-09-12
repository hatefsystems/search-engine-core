# Template System Development Guide

## 🏗️ Development Overview

The Template System provides a reusable configuration framework for common crawling patterns. This guide covers development workflows, testing strategies, and best practices for working with the template system.

## 🚀 Quick Start

### Prerequisites

- C++17 compiler (g++ 11+ or clang++)
- CMake 3.16+
- nlohmann/json library
- Docker and Docker Compose (for containerized development)

### Development Environment Setup

1. **Clone and build the project:**

```bash
git clone <repository-url>
cd search-engine-core
mkdir build && cd build
cmake .. && make
```

2. **Start the development environment:**

```bash
docker-compose up --build -d
```

3. **Verify template system is working:**

```bash
curl http://localhost:3000/api/templates
```

## 📁 Project Structure

```
search-engine-core/
├── include/search_engine/crawler/templates/
│   ├── TemplateTypes.h              # Core data structures
│   ├── TemplateRegistry.h           # Singleton registry
│   ├── TemplateValidator.h          # Input validation
│   ├── TemplateApplier.h            # Template application
│   ├── TemplateStorage.h            # Persistence layer
│   └── PrebuiltTemplates.h          # Pre-built template definitions
├── src/controllers/
│   └── TemplatesController.cpp      # API endpoints
├── config/templates/                # Template JSON files
│   ├── news-site.json
│   ├── ecommerce-site.json
│   ├── blog-site.json
│   ├── corporate-site.json
│   ├── documentation-site.json
│   ├── forum-site.json
│   └── social-media.json
└── tests/crawler/                   # Test files
    ├── template_registry_tests.cpp
    ├── template_validator_tests.cpp
    ├── template_applier_tests.cpp
    ├── template_storage_tests.cpp
    ├── template_integration_tests.cpp
    └── template_performance_tests.cpp
```

## 🔧 Development Workflow

### 1. Adding New Templates

#### Method 1: JSON File (Recommended)

1. **Create template file:**

```bash
# Create new template file
touch config/templates/my-template.json
```

2. **Define template structure:**

```json
{
  "name": "my-template",
  "description": "Template for my specific site type",
  "config": {
    "maxPages": 200,
    "maxDepth": 2,
    "spaRenderingEnabled": false,
    "extractTextContent": true,
    "politenessDelay": 1500
  },
  "patterns": {
    "articleSelectors": [".my-article", ".content-item"],
    "titleSelectors": ["h1", ".my-title"],
    "contentSelectors": [".my-content", ".main-text"]
  }
}
```

3. **Restart the server to load the template:**

```bash
docker-compose restart search-engine
```

#### Method 2: Programmatic (For Development)

1. **Edit `PrebuiltTemplates.h`:**

```cpp
// Add to seedPrebuiltTemplates() function
TemplateDefinition myTemplate;
myTemplate.name = "my-template";
myTemplate.description = "Template for my specific site type";
myTemplate.config.maxPages = 200;
myTemplate.config.maxDepth = 2;
myTemplate.config.spaRenderingEnabled = false;
myTemplate.config.extractTextContent = true;
myTemplate.config.politenessDelayMs = 1500;
myTemplate.patterns.articleSelectors = {".my-article", ".content-item"};
myTemplate.patterns.titleSelectors = {"h1", ".my-title"};
myTemplate.patterns.contentSelectors = {".my-content", ".main-text"};
TemplateRegistry::instance().upsertTemplate(myTemplate);
```

2. **Rebuild and restart:**

```bash
cd build && make && cd ..
docker-compose restart search-engine
```

### 2. Modifying Existing Templates

#### Update Template Configuration

1. **Edit the JSON file:**

```bash
# Edit existing template
vim config/templates/news-site.json
```

2. **Modify the configuration:**

```json
{
  "name": "news-site",
  "description": "Template for news websites",
  "config": {
    "maxPages": 1000,  // Increased from 500
    "maxDepth": 4,     // Increased from 3
    "spaRenderingEnabled": true,
    "extractTextContent": true,
    "politenessDelay": 800  // Decreased from 1000
  },
  "patterns": {
    "articleSelectors": ["article", ".post", ".story", ".news-item"],
    "titleSelectors": ["h1", ".headline", ".title", ".article-title"],
    "contentSelectors": [".content", ".body", ".article-body", ".post-content"]
  }
}
```

3. **Restart to apply changes:**

```bash
docker-compose restart search-engine
```

### 3. Testing Templates

#### Unit Testing

```bash
# Run template-specific tests
cd build
make crawler_tests
./crawler_tests --gtest_filter="*Template*"
```

#### Integration Testing

```bash
# Test template API endpoints
curl http://localhost:3000/api/templates
curl http://localhost:3000/api/templates/news-site

# Test template application
curl -X POST http://localhost:3000/api/crawl/add-site-with-template \
  -H "Content-Type: application/json" \
  -d '{
    "url": "https://example.com",
    "template": "news-site"
  }'
```

#### Performance Testing

```bash
# Run performance tests
cd build
make crawler_tests
./crawler_tests --gtest_filter="*Performance*"
```

## 🧪 Testing Strategies

### 1. Unit Tests

#### Template Registry Tests

```cpp
TEST_CASE("Template Registry Operations", "[templates]") {
    auto& registry = TemplateRegistry::instance();
    
    // Test template creation
    TemplateDefinition testTemplate;
    testTemplate.name = "test-template";
    testTemplate.config.maxPages = 100;
    
    registry.upsertTemplate(testTemplate);
    auto retrieved = registry.getTemplate("test-template");
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->config.maxPages.value() == 100);
    
    // Test template removal
    bool removed = registry.removeTemplate("test-template");
    REQUIRE(removed == true);
    
    auto notFound = registry.getTemplate("test-template");
    REQUIRE(!notFound.has_value());
}
```

#### Template Validator Tests

```cpp
TEST_CASE("Template Validation", "[templates]") {
    nlohmann::json validTemplate = {
        {"name", "test-template"},
        {"description", "Test template"},
        {"config", {
            {"maxPages", 100},
            {"maxDepth", 2}
        }},
        {"patterns", {
            {"articleSelectors", {"article"}},
            {"titleSelectors", {"h1"}},
            {"contentSelectors", {".content"}}
        }}
    };
    
    auto result = validateTemplateJson(validTemplate);
    REQUIRE(result.valid == true);
    REQUIRE(result.message == "ok");
}
```

#### Template Applier Tests

```cpp
TEST_CASE("Template Application", "[templates]") {
    TemplateDefinition template;
    template.config.maxPages = 500;
    template.config.maxDepth = 3;
    template.patterns.articleSelectors = {"article", ".post"};
    
    CrawlConfig config;
    applyTemplateToConfig(template, config);
    
    REQUIRE(config.maxPages == 500);
    REQUIRE(config.maxDepth == 3);
    REQUIRE(config.articleSelectors.size() == 2);
    REQUIRE(config.articleSelectors[0] == "article");
}
```

### 2. Integration Tests

#### API Endpoint Testing

```bash
#!/bin/bash
# test_templates_api.sh

echo "Testing template API endpoints..."

# Test list templates
echo "1. Testing GET /api/templates"
curl -s http://localhost:3000/api/templates | jq '.templates | length'

# Test get specific template
echo "2. Testing GET /api/templates/news-site"
curl -s http://localhost:3000/api/templates/news-site | jq '.name'

# Test create template
echo "3. Testing POST /api/templates"
curl -s -X POST http://localhost:3000/api/templates \
  -H "Content-Type: application/json" \
  -d '{"name": "test-template", "config": {"maxPages": 100}}' | jq '.status'

# Test crawl with template
echo "4. Testing POST /api/crawl/add-site-with-template"
curl -s -X POST http://localhost:3000/api/crawl/add-site-with-template \
  -H "Content-Type: application/json" \
  -d '{"url": "https://example.com", "template": "news-site"}' | jq '.success'

# Test delete template
echo "5. Testing DELETE /api/templates/test-template"
curl -s -X DELETE http://localhost:3000/api/templates/test-template | jq '.status'

echo "All tests completed!"
```

### 3. Performance Tests

#### Load Testing

```cpp
TEST_CASE("Template Registry Performance", "[templates][performance]") {
    auto& registry = TemplateRegistry::instance();
    
    const int iterations = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        auto template = registry.getTemplate("news-site");
        REQUIRE(template.has_value());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should be able to retrieve templates very quickly
    REQUIRE(duration.count() < 100000); // Less than 100ms for 10k operations
}
```

## 🐛 Debugging

### 1. Enable Debug Logging

```cpp
// In main.cpp, set log level to DEBUG
Logger::getInstance().init(LogLevel::DEBUG, true, "server.log");
```

### 2. Check Template Loading

```bash
# Check if templates are loaded
curl http://localhost:3000/api/templates | jq '.templates[].name'

# Check specific template
curl http://localhost:3000/api/templates/news-site | jq '.'
```

### 3. Monitor Server Logs

```bash
# Follow server logs
docker-compose logs -f search-engine

# Check for template-related errors
docker-compose logs search-engine | grep -i template
```

### 4. Validate Template JSON

```bash
# Validate template JSON syntax
python3 -m json.tool config/templates/news-site.json

# Check for common JSON errors
jq . config/templates/news-site.json
```

## 🔧 Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `TEMPLATES_PATH` | `/app/config/templates` | Path to template directory |
| `LOG_LEVEL` | `INFO` | Logging level (DEBUG, INFO, WARN, ERROR) |

### Template Validation Rules

| Field | Rules | Example |
|-------|-------|---------|
| `name` | 1-50 chars, alphanumeric with hyphens/underscores | `news-site`, `ecommerce-site` |
| `description` | Max 500 chars | `Template for news websites` |
| `maxPages` | 1-10000 | `500` |
| `maxDepth` | 1-10 | `3` |
| `politenessDelay` | 0-60000 ms | `1000` |
| `articleSelectors` | Array of non-empty strings | `["article", ".post"]` |

## 📊 Performance Optimization

### 1. Template Registry Optimization

```cpp
// Use const references to avoid copying
const auto& template = registry.getTemplate("news-site");
if (template.has_value()) {
    applyTemplateToConfig(*template, config);
}
```

### 2. Memory Management

```cpp
// Reserve space for known template count
templates.reserve(7); // Pre-built templates count
```

### 3. JSON Parsing Optimization

```cpp
// Use move semantics for large JSON objects
auto template = fromJson(std::move(jsonData));
```

## 🚀 Deployment

### 1. Development Deployment

```bash
# Build and deploy
cd build && make
docker-compose up --build -d

# Verify deployment
curl http://localhost:3000/api/templates
```

### 2. Production Deployment

```bash
# Build production image
docker build -t search-engine-core:latest .

# Deploy with production configuration
docker-compose -f docker-compose.prod.yml up -d
```

### 3. Template Migration

```bash
# Backup existing templates
cp -r config/templates config/templates.backup

# Deploy new templates
cp -r new-templates/* config/templates/

# Restart services
docker-compose restart search-engine
```

## 🔍 Troubleshooting

### Common Issues

#### 1. Template Not Loading

**Symptoms:**
- Template not appearing in `/api/templates` response
- 404 error when accessing specific template

**Solutions:**
```bash
# Check file permissions
ls -la config/templates/

# Validate JSON syntax
jq . config/templates/news-site.json

# Restart service
docker-compose restart search-engine
```

#### 2. Template Validation Errors

**Symptoms:**
- 400 Bad Request when creating templates
- Validation error messages

**Solutions:**
```bash
# Check template name format
echo "news-site" | grep -E '^[a-zA-Z0-9_-]+$'

# Validate numeric ranges
echo "500" | awk '{if($1 >= 1 && $1 <= 10000) print "valid"; else print "invalid"}'
```

#### 3. Template Application Issues

**Symptoms:**
- Template applied but configuration not changed
- Selector patterns not working

**Solutions:**
```bash
# Check template content
curl http://localhost:3000/api/templates/news-site | jq '.config'

# Verify template application
curl -X POST http://localhost:3000/api/crawl/add-site-with-template \
  -H "Content-Type: application/json" \
  -d '{"url": "https://example.com", "template": "news-site"}' | jq '.data.appliedConfig'
```

## 📚 Best Practices

### 1. Template Design

- **Use descriptive names**: `news-site` instead of `template1`
- **Include comprehensive selectors**: Cover common variations
- **Set reasonable limits**: Don't overwhelm target sites
- **Document template purpose**: Clear descriptions

### 2. Testing

- **Test with real websites**: Verify selectors work
- **Test edge cases**: Empty fields, invalid data
- **Performance test**: Ensure scalability
- **Integration test**: End-to-end workflows

### 3. Maintenance

- **Regular template updates**: Keep selectors current
- **Monitor usage**: Track which templates are used
- **Version control**: Track template changes
- **Documentation**: Keep docs up to date

## 🎯 Future Development

### Planned Features

1. **Template Inheritance**: Base templates with specialization
2. **Template Analytics**: Usage statistics and optimization
3. **Template Marketplace**: Community-shared templates
4. **Template Versioning**: Version control for templates

### Contributing

1. **Fork the repository**
2. **Create feature branch**: `git checkout -b feature/new-template`
3. **Add tests**: Ensure comprehensive test coverage
4. **Submit pull request**: Include description and tests

This development guide provides comprehensive coverage of working with the template system, from basic usage to advanced development and troubleshooting.
