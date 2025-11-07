---
name: '[M0][core] Multilingual text normalization & language detection'
about: 'Implement comprehensive text preprocessing pipeline for Persian, English, and mixed-language content'
title: '[M0][core] Multilingual text normalization & language detection'
labels: 'kind/feature, area/core, priority/P0, status/backlog'
assignees: ''
---

# Subtask 01: Multilingual Text Processing & Normalization (M0)

## Issue Title
`[M0][core] Multilingual text normalization & language detection`

## Summary
Implement comprehensive multilingual text preprocessing pipeline that automatically detects and processes any language/script. Handle Unicode normalization, script unification, and universal language detection to ensure consistent processing across all languages in the search engine.

## Implementation Language
**Primary: Python** (for universal linguistic processing)
**Integration: C++ API wrapper** (for performance)

## Technical Requirements
- Universal Unicode NFKC normalization (works for all scripts)
- Automatic script detection and character unification (Arabic→Persian, Cyrillic variants, etc.)
- Language-agnostic n-gram based language detection
- Script-specific handling (ZWNJ for Persian/Arabic, word boundaries for CJK, etc.)
- Universal stopword detection using corpus-based IDF analysis (automatic for all languages)
- Context-aware stopword identification with confidence scoring
- Efficient C++ integration for high-throughput processing
- Extensible architecture for adding new languages/scripts automatically

## Tasks
- [ ] Build universal Python text processor with `unicodedata`, `langdetect`, `polyglot`
- [ ] Implement script-agnostic character normalization (Arabic→Persian, Cyrillic variants, CJK unification)
- [ ] Create language-agnostic n-gram based detection supporting 100+ languages
- [ ] Implement script-specific preprocessing (ZWNJ for Arabic scripts, word segmentation for CJK, etc.)
- [ ] Add automatic language confidence scoring and fallback handling
- [ ] Build universal IDF-based stopword detector from corpus analysis (any language)
- [ ] Implement automatic stopword mining using document frequency statistics
- [ ] Export stopword lexicon to Redis with confidence scores per language
- [ ] Create nightly batch job for stopword list refresh from corpus updates
- [ ] Bootstrap with standard stopword lists for major languages as fallback
- [ ] Create C++ wrapper class with `pybind11` for seamless integration
- [ ] Add comprehensive unit tests with content from 20+ languages/scripts
- [ ] Performance benchmarking (1000 docs/sec target for any language)

## Acceptance Criteria
- Universal NFKC normalization works for all Unicode scripts
- Language detection accuracy ≥95% across 50+ languages on test corpus
- Script-specific handling works for Arabic, CJK, Cyrillic, Latin scripts
- Automatic language detection with confidence scoring
- Stopword detection accuracy ≥90% based on IDF analysis for any language
- Stopword lexicon automatically covers 100+ languages without manual configuration
- Nightly stopword refresh completes within 1 hour for 100M+ documents
- Redis stopword lookup latency <1ms per query
- C++ integration adds <5ms latency per document for any language
- Memory efficient for large-scale processing of any language content

## Dependencies
- Python 3.9+ with universal NLP libraries (langdetect, polyglot, icu)
- pybind11 for C++/Python binding
- Unicode character database with full script support

## API Interface
```cpp
// C++ usage - Universal for any language
TextProcessor processor;
std::string normalized = processor.normalize(text);  // Works for any Unicode text
LanguageInfo lang_info = processor.detect_language(text);  // Returns detected language + confidence

struct LanguageInfo {
    std::string language_code;  // ISO 639-1 (e.g., "fa", "en", "zh", "ar", ...)
    std::string script_code;    // ISO 15924 (e.g., "Latn", "Arab", "Cyrl", "Hani")
    float confidence;           // 0.0-1.0 confidence score
    bool is_mixed_content;      // True if multiple languages detected
};
```

## Files to Create/Modify
- `src/python/text_processor/`
- `src/python/text_processor/stopword_detector.py`
- `src/python/text_processor/idf_analyzer.py`
- `src/python/batch/nightly_stopword_refresh.py`
- `include/text/TextProcessor.h`
- `src/text/TextProcessor.cpp`
- `tests/text_processing_test.py`
- `tests/stopword_detection_test.py`

## Notes
- Universal design: supports any language/script automatically
- Extensible architecture for adding new language support without code changes
- C++ wrapper for integration with existing uWebSockets pipeline
- Support streaming processing for large documents in any language
- Automatic fallback to Latin script processing for unknown languages
