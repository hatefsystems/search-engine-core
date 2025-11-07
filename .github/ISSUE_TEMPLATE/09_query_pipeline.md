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
- Context-aware stopword filtering with intelligent decision rules
- Multi-stage retrieval with stopword fallback strategy (filtered → original)
- Real-time query analysis (single-word, quoted phrases, entity detection)
- Hybrid spell correction with three-stage pipeline (edit distance → frequency → embeddings)
- Smart spell suggestion with confidence scoring
- Auto-correction threshold tuning to avoid false positives
- Multi-stage retrieval (BM25 + n-gram fallback)
- Real-time feature gathering from multiple sources
- Query expansion with semantic lexicon
- Comprehensive error handling and fallbacks

## Tasks
- [ ] Implement language detection and routing
- [ ] Add context-aware stopword filtering with intelligent rules
- [ ] Implement single-token query protection (never filter single-word queries)
- [ ] Add quoted phrase detection and exact-match preservation
- [ ] Build entity name detection to preserve all tokens in entity queries
- [ ] Implement multi-stage retrieval with stopword fallback (filtered → original)
- [ ] Build hybrid spell correction system (Stage 1: Edit Distance <1ms)
- [ ] Implement corpus frequency validation (Stage 2: Frequency Check 2-3ms)
- [ ] Add embedding-based semantic validation (Stage 3: Similarity 5-8ms)
- [ ] Create spell suggestion API ("Did you mean...?" functionality)
- [ ] Implement auto-correction for high-confidence cases
- [ ] Build query rewriting with spell-corrected variants
- [ ] Add query expansion from nightly lexicon (≤3 terms)
- [ ] Build multi-stage retrieval (BM25@200 + n-gram@100)
- [ ] Create feature gathering pipeline (embeddings, authority, quality)
- [ ] Implement re-ranking with FinalScore formula
- [ ] Add MMR diversification
- [ ] Build caching layer for hot queries
- [ ] Add comprehensive logging and metrics
- [ ] Implement graceful degradation on component failures
- [ ] Add debug mode showing stopword filtering decisions and reasoning

## Acceptance Criteria
- P95 end-to-end latency ≤300ms for head queries
- Context-aware stopword filtering never breaks single-word queries
- Entity name detection preserves query intent with ≥95% accuracy
- Quoted phrase handling provides exact match results without stopword filtering
- Multi-stage stopword fallback improves recall by ≥15% without precision loss
- Hybrid spell correction detects typos with ≥92% accuracy
- Stage 1 (edit distance) completes in <1ms for candidate generation
- Stage 2 (frequency validation) adds 2-3ms for filtering
- Stage 3 (embedding similarity) adds 5-8ms only when needed
- Spell suggestion precision ≥95% (minimal false positives)
- Auto-correction applied only for confidence >0.9
- Query expansion improves results without hurting precision
- Multi-stage retrieval increases recall by ≥25%
- Stopword filtering adds <5ms latency to query processing
- Graceful handling of component failures
- Comprehensive debug mode with feature traces and stopword decisions

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
- `src/pipeline/ContextAwareStopwordFilter.h`
- `src/pipeline/ContextAwareStopwordFilter.cpp`
- `src/pipeline/QueryAnalyzer.cpp`
- `src/pipeline/HybridSpellCorrector.h`
- `src/pipeline/HybridSpellCorrector.cpp`
- `src/pipeline/EditDistanceCalculator.cpp`
- `src/pipeline/QueryExpander.cpp`
- `src/pipeline/MultiStageRetriever.cpp`
- `tests/pipeline_test.cpp`
- `tests/stopword_filtering_test.cpp`
- `tests/spell_correction_pipeline_test.cpp`

## Notes
- C++ for performance-critical serving pipeline
- Context-aware stopword filtering uses rule-based approach (fast, deterministic)
- Hybrid spell correction uses adaptive staging (skip expensive stages when confident)
- Stage 1 (edit distance) always runs for fast candidate generation
- Stage 2 (frequency) filters candidates efficiently using Redis cache
- Stage 3 (embeddings) only for ambiguous cases requiring semantic validation
- Auto-correction threshold tuned conservatively (confidence >0.9) to avoid user frustration
- Multi-stage retrieval strategy ensures no loss of recall from aggressive filtering
- Entity detection and quoted phrase handling preserve user intent
- Default strategy is conservative: when uncertain, don't filter
- Asynchronous calls to Python ML services
- Comprehensive error handling and fallbacks
- Debug mode for development and analysis with stopword decision traces
