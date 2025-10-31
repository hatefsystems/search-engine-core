---
name: '[M2][extraction] Schema.org structured data parsing + regex extraction'
about: 'Implement automated extraction of structured data from web pages including schema.org markup'
title: '[M2][extraction] Schema.org structured data parsing + regex extraction'
labels: 'kind/feature, area/extraction, priority/P1, status/backlog'
assignees: ''
---

# Subtask 04: Structured Data Extraction (Schema.org) (M2)

## Issue Title
`[M2][extraction] Schema.org structured data parsing + regex extraction`

## Summary
Implement universal automated extraction of structured data from web pages in any language including schema.org markup, JSON-LD, Microdata, and RDFa. Extract key fields for books, products, articles, and other structured content types regardless of content language.

## Implementation Language
**Primary: Python** (HTML parsing, schema extraction)
**Integration: C++** (fast feature storage/retrieval)

## Technical Requirements
- Parse JSON-LD, Microdata, RDFa formats
- Extract structured fields (ISBN, author, price, etc.)
- Regex-based fallback for missing schema markup
- Type detection and validation
- Efficient storage in feature store

## Tasks
- [ ] Implement schema.org parser (JSON-LD/Microdata/RDFa)
- [ ] Extract key fields by type (Book/Product/Article)
- [ ] Build regex patterns for ISBN-10/13, prices, dates
- [ ] Create type detection (Product/Offer/Book/etc.)
- [ ] Add validation and normalization of extracted data
- [ ] Store structured features in database
- [ ] Python processing pipeline for batch extraction
- [ ] Handle malformed/missing markup gracefully

## Acceptance Criteria
- ≥90% precision for ISBN extraction on test pages
- ≥80% coverage for structured pages with schema markup
- Regex fallbacks work for ≥60% of non-schema content
- Processing adds minimal latency to indexing pipeline
- Structured data improves vertical search quality

## Dependencies
- BeautifulSoup4 or lxml for HTML parsing
- extruct library for schema.org extraction
- Python regex for pattern matching
- MongoDB for structured feature storage

## API Interface
```python
# Python extraction service
class StructuredExtractor:
    def extract_structured_data(self, html_content, url):
        """Extract schema.org and regex-based structured data"""
        return {
            'schema_type': 'Book|Product|Article|None',
            'isbn': '978-0-123456-78-9',
            'author': 'Author Name',
            'price': 29.99,
            'currency': 'USD',
            # ... other fields
        }
```

```cpp
// C++ feature access
class StructuredDataStore {
    StructuredData get_structured_data(const std::string& doc_id);
    bool has_structured_boost(const std::string& doc_id);
};
```

## Files to Create/Modify
- `src/python/structured_extractor/`
- `include/features/StructuredDataStore.h`
- `src/features/StructuredDataStore.cpp`
- `tests/structured_extraction_test.py`

## Notes
- Python excels at HTML parsing and complex extraction logic
- C++ for fast serving-time feature access
- Support multiple schema formats (JSON-LD preferred)
- Graceful degradation when markup is malformed
