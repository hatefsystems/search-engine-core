---
name: '[M1][core] BM25 weighted retrieval + character n-gram fallback index'
about: 'Build the core lexical retrieval system with weighted BM25 scoring and character-level n-gram fallback'
title: '[M1][core] BM25 weighted retrieval + character n-gram fallback index'
labels: 'kind/feature, area/core, priority/P0, status/backlog'
assignees: ''
---

# Subtask 02: Core Retrieval Engine (BM25 + n-gram) (M1)

## Issue Title
`[M1][core] BM25 weighted retrieval + character n-gram fallback index`

## Summary
Build the core multilingual lexical retrieval system with weighted BM25 scoring and character-level n-gram fallback that works for any language automatically. Implement efficient indexing and querying using RedisSearch or similar high-performance search backend.

## Implementation Language
**Primary: C++** (performance-critical core)
**Data Processing: Python** (index building, analysis)

## Technical Requirements
- Universal weighted BM25 with field-specific scoring (works for any language)
- Language-agnostic character n-gram fallback indexing (3-5 grams)
- RedisSearch integration or custom C++ implementation
- Automatic language detection and routing (no manual configuration needed)
- Cross-language deduplication and quality filtering

## Tasks
- [ ] Design document schema with weighted fields (title:5x, anchors:3x, body:1x)
- [ ] Implement BM25 scorer in C++ with field weights
- [ ] Build character n-gram tokenizer (3-5 grams)
- [ ] Integrate with RedisSearch for indexing/querying
- [ ] Implement query routing (BM25 → n-gram fallback)
- [ ] Add URL-level deduplication (simhash/shingles)
- [ ] Quality gate filtering (short pages, boilerplate)
- [ ] Python scripts for batch indexing and analysis
- [ ] Performance optimization (P95 <80ms for top-200)

## Acceptance Criteria
- BM25 retrieval works correctly for any detected language
- n-gram fallback improves recall by ≥20% for unknown/mixed language content
- Cross-language deduplication reduces near-duplicates by ≥60%
- Universal quality filtering removes ≥80% low-quality pages regardless of language
- P95 retrieval latency ≤80ms for top-200 results across all languages
- Index supports 100M+ documents efficiently for any language content

## Dependencies
- RedisSearch module or custom C++ search library
- MongoDB for metadata storage
- Python for batch processing (pandas, numpy)

## API Interface
```cpp
// C++ retrieval interface
class RetrievalEngine {
    std::vector<ScoredDocument> retrieve(const std::string& query,
                                       const std::string& language,
                                       size_t top_k = 200);
    void add_document(const Document& doc);
};
```

## Files to Create/Modify
- `include/search/RetrievalEngine.h`
- `src/search/RetrievalEngine.cpp`
- `src/search/BM25Scorer.cpp`
- `src/search/NGramTokenizer.cpp`
- `src/python/index_builder/`
- `tests/retrieval_test.cpp`

## Notes
- Core retrieval must be C++ for low latency
- Python for offline index building and analysis
- Support both RedisSearch and custom implementation
- Memory-efficient for large-scale indexing
