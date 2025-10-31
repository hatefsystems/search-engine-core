---
name: '[M1-M6][pipeline] Complete query pipeline: detect → expand → retrieve → re-rank → diversify'
about: 'Build the complete online query processing pipeline orchestrating all ranking components'
title: '[M1-M6][pipeline] Complete query pipeline: detect → expand → retrieve → re-rank → diversify'
labels: 'kind/feature, area/pipeline, priority/P0, status/backlog'
assignees: ''
---

# Subtask 09: End-to-End Query Processing Pipeline (M1-M6)

## Issue Title
`[M1-M6][pipeline] Complete query pipeline: detect → expand → retrieve → re-rank → diversify`

## Summary
Build the complete universal online query processing pipeline that automatically handles any language and orchestrates all components: language detection, query expansion, multi-stage retrieval, feature gathering, re-ranking, and diversification.

## Implementation Language
**Primary: C++** (high-performance serving)
**Components: Python services** (ML components via HTTP/gRPC)

## Technical Requirements
- Low-latency end-to-end processing (P95 ≤300ms)
- Multi-stage retrieval (BM25 + n-gram fallback)
- Real-time feature gathering from multiple sources
- Query expansion with semantic lexicon
- Comprehensive error handling and fallbacks

## Tasks
- [ ] Implement language detection and routing
- [ ] Add query expansion from nightly lexicon (≤3 terms)
- [ ] Build multi-stage retrieval (BM25@200 + n-gram@100)
- [ ] Create feature gathering pipeline (embeddings, authority, quality)
- [ ] Implement re-ranking with FinalScore formula
- [ ] Add MMR diversification
- [ ] Build caching layer for hot queries
- [ ] Add comprehensive logging and metrics
- [ ] Implement graceful degradation on component failures

## Acceptance Criteria
- P95 end-to-end latency ≤300ms for head queries
- Query expansion improves results without hurting precision
- Multi-stage retrieval increases recall by ≥25%
- Graceful handling of component failures
- Comprehensive debug mode with feature traces

## Dependencies
- uWebSockets for HTTP serving
- Redis for caching and feature storage
- MongoDB for metadata
- gRPC/HTTP clients for Python ML services
- Comprehensive logging framework

## API Interface
```cpp
// Main query pipeline - Universal for any language
class QueryPipeline {
    QueryResult process_query(const QueryRequest& request);

    struct QueryRequest {
        std::string query;  // Any language automatically detected
        size_t num_results = 10;
        bool debug_mode = false;
    };

    struct QueryResult {
        std::vector<ScoredDocument> results;
        QueryMetadata metadata;  // Includes detected language info
        std::string debug_info;  // When debug_mode=true
    };
};

// Pipeline stages
class LanguageDetector {
    std::string detect_language(const std::string& query);
};

class QueryExpander {
    std::vector<std::string> expand_query(const std::string& query,
                                        const std::string& language);
};

class MultiStageRetriever {
    std::vector<Candidate> retrieve_candidates(const std::vector<std::string>& query_terms,
                                             const std::string& language);
};
```

## Files to Create/Modify
- `include/pipeline/QueryPipeline.h`
- `src/pipeline/QueryPipeline.cpp`
- `src/pipeline/LanguageDetector.cpp`
- `src/pipeline/QueryExpander.cpp`
- `src/pipeline/MultiStageRetriever.cpp`
- `tests/pipeline_test.cpp`

## Notes
- C++ for performance-critical serving pipeline
- Asynchronous calls to Python ML services
- Comprehensive error handling and fallbacks
- Debug mode for development and analysis
