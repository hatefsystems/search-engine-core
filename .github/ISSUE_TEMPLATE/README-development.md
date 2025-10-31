# 🔧 Universal Multilingual Search Engine - Development Guidelines

## Overview
This guide provides development standards, integration patterns, and best practices for implementing the universal multilingual search engine components.

## 🏗️ Architecture Principles

### Universal Design
- **Language Agnostic:** All components must work with any Unicode language/script
- **Zero Configuration:** No manual language setup or hardcoded language lists
- **Auto-Detection:** Language/script detection happens automatically
- **Graceful Degradation:** Fallback to basic processing for unknown languages

### Performance Requirements
- **Latency:** P95 ≤300ms for any language queries
- **Throughput:** Handle 1000+ queries/second
- **Memory:** Efficient for billion-scale document processing
- **Scale:** Horizontal scaling for any language corpus

## 💻 Technology Stack

### Core Components (C++)
```cpp
// High-performance serving layer
- uWebSockets: HTTP/WebSocket server
- Custom C++: Core retrieval, ranking, caching
- Redis: Feature storage, caching, indexing
- MongoDB: Document metadata, user data
```

### ML Components (Python)
```python
# Advanced AI/ML processing
- NumPy/SciPy: Numerical computing
- scikit-learn/XGBoost: ML models
- Gensim/FastText: Embeddings
- Pandas: Data processing
- gRPC/FastAPI: ML service APIs
```

### Integration Patterns
```cpp
// C++/Python integration via gRPC
class EmbeddingService {
    // Synchronous calls for low latency
    std::vector<float> getEmbedding(const std::string& text);

    // Asynchronous batch processing
    std::future<std::vector<std::vector<float>>> getBatchEmbeddings(
        const std::vector<std::string>& texts);
};
```

## 📋 Development Workflow

### 1. Component Selection
```bash
# Choose based on your expertise
Priority P0: 01, 02, 09, 12, 13, 14  # Core infrastructure
Priority P1: 03-08, 10, 11           # ML and features
```

### 2. Environment Setup
```bash
# Clone and setup
git clone <repo>
cd search-engine-core

# Docker development environment
docker-compose up -d mongodb redis

# Build C++ components
mkdir build && cd build
cmake .. && make -j$(nproc)

# Python virtual environment
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### 3. Testing Standards
```cpp
// C++ unit tests
TEST(TextProcessorTest, UniversalNormalization) {
    // Test with multiple languages
    EXPECT_TRUE(processor.normalize("Hello") == "hello");
    EXPECT_TRUE(processor.normalize("مرحبا") == "مرحبا"); // Arabic
    EXPECT_TRUE(processor.normalize("你好") == "你好");   // Chinese
}
```

```python
# Python integration tests
def test_universal_embeddings():
    # Test embeddings work for any language
    assert len(service.get_embedding("hello")) == 300
    assert len(service.get_embedding("مرحبا")) == 300
    assert len(service.get_embedding("你好")) == 300
```

## 🔌 API Standards

### REST API Endpoints
```javascript
// Universal search endpoint
POST /api/v1/search
{
    "query": "any language text",  // Auto-detected
    "num_results": 10,
    "debug": false
}

// Response format
{
    "success": true,
    "results": [...],
    "metadata": {
        "detected_language": "fa",
        "script": "Arab",
        "confidence": 0.95,
        "processing_time_ms": 45
    }
}
```

### Internal Service APIs
```cpp
// Feature store interface
class FeatureStore {
public:
    // Language-agnostic feature access
    virtual DocumentFeatures getFeatures(const std::string& docId) = 0;

    // Universal feature storage
    virtual void storeFeatures(const std::string& docId,
                              const DocumentFeatures& features) = 0;
};
```

## 📊 Data Formats

### Document Schema (Universal)
```json
{
    "_id": "doc_123",
    "url": "https://example.com/page",
    "title": "Page Title",
    "content": "Page content in any language",
    "language": "fa",      // Auto-detected ISO 639-1
    "script": "Arab",      // Auto-detected ISO 15924
    "features": {
        "bm25_score": 0.85,
        "host_rank": 0.72,
        "quality_score": 0.91,
        "spam_score": 0.05,
        // ... universal features
    },
    "embeddings": [0.1, 0.2, ...],  // 300-dim vector
    "timestamp": "2024-01-01T00:00:00Z"
}
```

### Query Processing Flow
```cpp
struct QueryRequest {
    std::string query;        // Any language
    size_t numResults = 10;
    bool debug = false;
};

struct QueryResult {
    std::vector<ScoredDocument> results;
    QueryMetadata metadata = {
        .detectedLanguage = "auto",
        .processingTimeMs = 0,
        .cacheHit = false
    };
};
```

## 🔒 Security Guidelines

### Input Validation
```cpp
// Universal input sanitization
class InputValidator {
public:
    std::string sanitizeQuery(const std::string& query) {
        // Remove dangerous characters regardless of language
        // Validate length limits
        // Check for injection attempts
        return sanitized;
    }
};
```

### Privacy Protection
```cpp
// PII detection and anonymization
class PIIAnonymizer {
public:
    std::string anonymize(const std::string& text) {
        // Detect PII patterns in any language
        // Replace with placeholders
        // Log anonymized version only
        return anonymized;
    }
};
```

## 📈 Performance Optimization

### Caching Strategy
```cpp
// Multi-layer caching
class CacheManager {
    // L1: Memory cache (fastest)
    std::unordered_map<std::string, CachedResult> memoryCache;

    // L2: Redis cache (distributed)
    // L3: Disk cache (fallback)

    CachedResult getCached(const std::string& key) {
        // Check L1 → L2 → L3 → compute
    }
};
```

### Async Processing
```cpp
// Non-blocking ML inference
std::future<std::vector<float>> EmbeddingService::getEmbeddingAsync(
    const std::string& text) {

    return std::async(std::launch::async, [this, text]() {
        // Heavy computation off main thread
        return computeEmbedding(text);
    });
}
```

## 🧪 Testing Strategy

### Unit Tests
- **C++:** Google Test framework
- **Python:** pytest
- **Coverage:** Minimum 80% for critical paths

### Integration Tests
```bash
# Test multilingual queries
curl -X POST http://localhost:3000/api/search \
  -d '{"query": "hello"}'           # English

curl -X POST http://localhost:3000/api/search \
  -d '{"query": "مرحبا"}'           # Arabic

curl -X POST http://localhost:3000/api/search \
  -d '{"query": "你好"}'             # Chinese
```

### Load Testing
```bash
# Performance benchmarks
wrk -t12 -c400 -d30s http://localhost:3000/api/search \
  -s multilingual-payloads.lua

# Target: P95 < 300ms for any language
```

## 🚀 Deployment Standards

### Containerization
```dockerfile
# Multi-stage build for C++/Python
FROM gcc:11 AS cpp-builder
# Build C++ components

FROM python:3.9-slim AS python-builder
# Build Python components

FROM ubuntu:20.04 AS runtime
# Combine and run
```

### Configuration Management
```yaml
# Environment-based config
search:
  redis_url: ${REDIS_URL}
  mongodb_uri: ${MONGODB_URI}
  embedding_service_url: ${EMBEDDING_SERVICE_URL}

performance:
  cache_ttl_seconds: ${CACHE_TTL:-3600}
  max_results: ${MAX_RESULTS:-100}
  timeout_ms: ${TIMEOUT_MS:-5000}

languages:
  # Auto-detected - no manual config needed
  supported_scripts: auto
  fallback_script: Latn
```

## 📚 Code Quality

### C++ Standards
```cpp
// Modern C++17+ practices
class TextProcessor {
public:
    // RAII resource management
    TextProcessor();
    ~TextProcessor();

    // Const-correctness
    std::string normalize(const std::string& text) const;

    // Exception safety
    LanguageInfo detectLanguage(const std::string& text) noexcept;
};
```

### Python Standards
```python
# Type hints and modern Python
from typing import List, Dict, Optional
import asyncio

class EmbeddingService:
    async def get_embeddings_batch(
        self,
        texts: List[str]
    ) -> List[List[float]]:
        """Batch embedding computation with async support"""
        pass
```

## 🎯 Success Metrics

### Performance Targets
- **Latency:** P95 ≤300ms for any language
- **Throughput:** 1000+ queries/second
- **Accuracy:** 95%+ language detection
- **Quality:** 85%+ ranking precision

### Reliability Targets
- **Uptime:** 99.9% SLA
- **Error Rate:** <0.1% for valid queries
- **Data Loss:** Zero tolerance
- **Security:** No breaches

---

## 📞 Support

- **Architecture Questions:** Main epic document
- **Implementation Issues:** Component-specific files
- **Performance Issues:** Profile and optimize
- **Integration Issues:** Check API contracts

*Follow these guidelines to build a world-class, universal multilingual search engine that scales globally.*
