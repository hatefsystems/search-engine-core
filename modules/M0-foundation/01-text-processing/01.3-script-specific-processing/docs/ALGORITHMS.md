# Algorithms and Implementation Details - Task 01.3

Technical documentation for script-specific text processing algorithms.

## Table of Contents

1. [ZWNJ Preservation Algorithm](#zwnj-preservation-algorithm)
2. [CJK Segmentation Algorithms](#cjk-segmentation-algorithms)
3. [Cyrillic Normalization Rules](#cyrillic-normalization-rules)
4. [Bidirectional Text Handling](#bidirectional-text-handling)
5. [Performance Optimizations](#performance-optimizations)
6. [Unicode Script Detection Details](#unicode-script-detection-details)

## ZWNJ Preservation Algorithm

### Overview

Zero-Width Non-Joiner (ZWNJ, U+200C) is a critical character in Persian (Farsi) text that affects word meaning and grammar. It must **never** be removed or normalized.

### Algorithm

```python
def preserve_zwnj(text: str) -> str:
    """
    Preserve ZWNJ characters in text.
    
    ZWNJ is grammatically significant:
    - "می‌خواهم" (I want) vs "میخواهم" (different meaning)
    - Must never be removed or normalized
    """
    # ZWNJ is already in the text, just ensure it's not removed
    # This function serves as documentation and validation
    zwnj_count = text.count(ZWNJ)
    if zwnj_count > 0:
        logger.debug(f"Preserving {zwnj_count} ZWNJ characters")
    return text
```

### Critical Rules

1. **Never remove ZWNJ** - It's grammatically significant
2. **Never normalize ZWNJ** - Don't convert to space or remove
3. **Preserve in all Arabic script processing** - Applies to Persian, Urdu, etc.

### Examples

- ✅ Correct: "می‌خواهم" → "می‌خواهم" (ZWNJ preserved)
- ❌ Wrong: "می‌خواهم" → "میخواهم" (ZWNJ removed - changes meaning)

## CJK Segmentation Algorithms

### Chinese Segmentation (jieba)

**Algorithm:** Uses jieba-fast library for word segmentation.

**Process:**
1. Load jieba model (lazy loading for performance)
2. Segment text into words using jieba.cut()
3. Extract word boundaries for indexing
4. Join words with spaces for processed text

**Performance:**
- jieba-fast: ~10x faster than standard jieba
- Lazy loading: Only loads when needed
- Caching: jieba model cached after first load

**Example:**
```python
text = "你好世界"
words = segment_chinese(text)  # ["你好", "世界"]
boundaries = get_word_boundaries(text, words)  # [0, 2, 4]
```

### Japanese Tokenization

**Algorithm:** Uses MeCab if available, otherwise regex-based segmentation.

**MeCab Method:**
1. Load MeCab tagger (lazy loading)
2. Parse text with MeCab
3. Extract tokens from output

**Regex Fallback:**
1. Detect script boundaries (Hiragana, Katakana, Kanji)
2. Split at script transitions
3. Handle punctuation separately

**Example:**
```python
text = "こんにちは世界"
tokens = segment_japanese(text)  # ["こんにちは", "世界"]
```

### Korean Segmentation

**Algorithm:** Space-based word segmentation with Hangul syllable handling.

**Process:**
1. Split by spaces (Korean uses spaces for word boundaries)
2. Handle Hangul syllables
3. Preserve compound words

**Example:**
```python
text = "안녕하세요 세계"
words = segment_korean(text)  # ["안녕하세요", "세계"]
```

## Cyrillic Normalization Rules

### Variant Unification (ё → е)

**Algorithm:** Normalize ё (U+0451) to е (U+0435) for consistency.

**Rules:**
1. Normalize ё → е by default (configurable)
2. Preserve ё for specific languages if needed
3. Handle Ukrainian/Belarusian specific characters

**Configuration:**
```python
# Normalize by default
text = "ёлка"
result = unify_cyrillic_variants(text, normalize_yo=True)  # "елка"

# Preserve for specific language
result = unify_cyrillic_variants(text, normalize_yo=False, language_code="ru")
```

**Language-Specific Handling:**
- Russian: Usually normalize ё → е
- Ukrainian: Preserve specific characters (і, ї, ґ)
- Belarusian: Preserve specific characters

## Bidirectional Text Handling

### Unicode BiDi Algorithm

**Algorithm:** Uses Unicode bidirectional algorithm implicitly.

**Process:**
1. Detect script boundaries in mixed text
2. Process each script segment separately
3. Preserve logical order (not visual order)
4. Combine processed segments

**Implementation:**
```python
def handle_bidirectional_text(text: str) -> str:
    """
    Handle bidirectional text (RTL + LTR mixing).
    
    Unicode bidirectional algorithm is handled by rendering system.
    We preserve logical order here.
    """
    return text  # Logical order preserved
```

**Script Boundary Detection:**
```python
def detect_script_boundaries(text: str) -> List[Tuple[int, int, str]]:
    """
    Detect script boundaries in mixed-script text.
    
    Returns: List of (start, end, script_code) tuples
    """
    # Uses regex patterns for each script
    # Detects transitions between scripts
    # Handles whitespace and punctuation
```

**Example:**
```python
text = "Hello سلام World"
boundaries = detect_script_boundaries(text)
# [(0, 6, "Latn"), (6, 10, "Arab"), (10, 16, "Latn")]
```

## Performance Optimizations

### Lazy Loading

**Heavy Dependencies:**
- jieba: Only loaded when processing Chinese
- MeCab: Only loaded when processing Japanese (if available)

**Implementation:**
```python
_jieba = None

def _get_jieba():
    """Lazy load jieba for Chinese segmentation."""
    global _jieba
    if _jieba is None:
        import jieba_fast as jieba
        _jieba = jieba
    return _jieba
```

### Caching

**Compiled Regex Patterns:**
- Script detection patterns cached at module level
- Word boundary patterns cached

**Model Caching:**
- jieba model loaded once and reused
- MeCab tagger initialized once

### Optimized Hot Paths

**Script Detection:**
- Fast regex matching
- Early exit for single-script text
- Minimal allocations

**ZWNJ Handling:**
- Simple character counting
- No complex processing needed

## Unicode Script Detection Details

### Script Detection Patterns

**Regex Patterns:**
```python
SCRIPT_PATTERNS = {
    'Arab': re.compile(r'[\u0600-\u06FF\u0750-\u077F\u08A0-\u08FF\uFB50-\uFDFF\uFE70-\uFEFF]'),
    'Latn': re.compile(r'[a-zA-Z]'),
    'Cyrl': re.compile(r'[\u0400-\u04FF]'),
    'Hans': re.compile(r'[\u4E00-\u9FFF]'),
    'Hant': re.compile(r'[\u4E00-\u9FFF]'),
    'Jpan': re.compile(r'[\u3040-\u309F\u30A0-\u30FF\u4E00-\u9FFF]'),
    'Kore': re.compile(r'[\uAC00-\uD7AF\u1100-\u11FF\u3130-\u318F]'),
}
```

### Script Code Mapping

**ISO 15924 Codes:**
- `Arab`: Arabic script
- `Latn`: Latin script
- `Cyrl`: Cyrillic script
- `Hans`: Simplified Chinese
- `Hant`: Traditional Chinese
- `Jpan`: Japanese
- `Kore`: Korean
- `Zyyy`: Common (unknown script)

### Boundary Detection Algorithm

1. **Iterate through text character by character**
2. **Skip whitespace and punctuation**
3. **Match character against script patterns**
4. **Detect script transitions**
5. **Create boundary segments**

**Complexity:** O(n) where n is text length

## Error Handling

### Malformed Unicode

**Handling:**
- Graceful degradation
- Preserve original text if processing fails
- Log warnings for debugging

### Missing Dependencies

**Handling:**
- jieba: Raises ImportError with helpful message
- MeCab: Falls back to regex-based segmentation
- Log warnings for missing optional dependencies

### Edge Cases

**Empty Strings:**
- Return empty ProcessedText
- No processing applied

**Whitespace Only:**
- Preserve whitespace
- No script-specific processing

**Numbers and Punctuation:**
- Preserved as-is
- Not processed by script-specific rules

## Performance Metrics

### Target Requirements

- **Throughput:** 1000+ docs/sec
- **Latency:** <10ms per document
- **Accuracy:** CJK segmentation ≥85%, ZWNJ preservation 100%

### Optimization Strategies

1. **Lazy loading** of heavy dependencies
2. **Caching** of compiled patterns and models
3. **Early exit** for simple cases
4. **Minimal allocations** in hot paths
5. **Batch processing** support (future enhancement)

## Future Enhancements

1. **Advanced Arabic Shaping:** Use python-arabic-reshaper
2. **Better Japanese Tokenization:** Improve regex fallback
3. **Korean Morphological Analysis:** Use KoNLPy if available
4. **Traditional/Simplified Chinese Conversion:** Use opencc
5. **Bidirectional Text:** Use python-bidi library
