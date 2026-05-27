# Algorithms and Implementation Details - Task 01.3

Deep dive into the algorithms and implementation details of script-specific text processing system.

## 📋 Table of Contents

1. [ZWNJ Preservation Algorithm](#zwnj-preservation-algorithm)
   - [Overview](#overview)
   - [Detailed Algorithm Explanation](#detailed-algorithm-explanation)
   - [Why ZWNJ Matters](#why-zwnj-matters)
   - [Implementation Details](#implementation-details)
2. [CJK Segmentation Algorithms](#cjk-segmentation-algorithms)
   - [Chinese Segmentation (jieba)](#chinese-segmentation-jieba)
   - [Japanese Tokenization](#japanese-tokenization)
   - [Korean Segmentation](#korean-segmentation)
3. [Cyrillic Normalization Rules](#cyrillic-normalization-rules)
   - [Variant Unification Algorithm](#variant-unification-algorithm)
   - [Language-Specific Handling](#language-specific-handling)
4. [Latin Script Processing](#latin-script-processing)
   - [Diacritic Normalization](#diacritic-normalization)
   - [Ligature Handling](#ligature-handling)
5. [Bidirectional Text Handling](#bidirectional-text-handling)
   - [Unicode BiDi Algorithm](#unicode-bidi-algorithm)
   - [Script Boundary Detection](#script-boundary-detection)
6. [Performance Optimizations](#performance-optimizations)
7. [Unicode Script Detection Details](#unicode-script-detection-details)
8. [Learning Resources](#learning-resources)
9. [References](#references)

## ZWNJ Preservation Algorithm

### Overview

Zero-Width Non-Joiner (ZWNJ, U+200C) is a critical invisible character in Persian (Farsi) text that affects word meaning and grammar. Unlike spaces or visible punctuation, ZWNJ is a **zero-width** character that prevents character joining in Arabic scripts without adding visual spacing.

### Detailed Algorithm Explanation

#### What is ZWNJ?

ZWNJ (U+200C) is a Unicode control character that:
- Has **zero width** (invisible, no visual space)
- Prevents **character joining** in Arabic scripts
- Is **grammatically significant** in Persian
- Must be **preserved** during text processing

#### Character Properties

```python
ZWNJ = '\u200C'  # Unicode code point U+200C

# Properties:
# - Category: Cf (Format, other)
# - Bidirectional: BN (Boundary Neutral)
# - Width: 0 (zero-width)
# - Joining: Non-joining (prevents joining)
```

#### Algorithm Steps

**Step 1: Detection**
```python
def preserve_zwnj(text: str) -> str:
    """
    Preserve ZWNJ characters in text.
    
    Algorithm:
    1. Count ZWNJ occurrences
    2. Log for debugging
    3. Return text unchanged (preservation = no removal)
    """
    zwnj_count = text.count(ZWNJ)
    if zwnj_count > 0:
        logger.debug(f"Preserving {zwnj_count} ZWNJ characters")
    return text  # Critical: Return unchanged!
```

**Step 2: Integration in Processing Pipeline**
```python
def process_arabic(text: str, language_code: str) -> ProcessedText:
    # Step 1: ALWAYS preserve ZWNJ first (before any other processing)
    text = preserve_zwnj(text)  # No removal, just validation
    
    # Step 2: Other processing (diacritics, shapes, etc.)
    # ... but ZWNJ remains untouched
    
    return ProcessedText(text=text, ...)
```

### Why ZWNJ Matters

#### Grammatical Significance

ZWNJ changes word meaning in Persian:

**Example 1: Verb Forms**
```python
# With ZWNJ: "می‌خواهم" (mikhāham) = "I want"
text_with_zwnj = "می‌خواهم"  # Contains U+200C

# Without ZWNJ: "میخواهم" (mikhwāham) = Different verb form
text_without_zwnj = "میخواهم"  # No ZWNJ

# These are DIFFERENT words with DIFFERENT meanings!
```

**Example 2: Compound Words**
```python
# "خانه‌سازی" (khāne-sāzi) = "house-building" (with ZWNJ)
# "خانه‌سازی" without ZWNJ = incorrect spelling

# "نیم‌روز" (nim-ruz) = "noon" (with ZWNJ)
# Without ZWNJ = incorrect
```

#### Search and Indexing Impact

**Problem:** If ZWNJ is removed:
- Search for "می‌خواهم" won't find documents with "میخواهم"
- Indexing becomes inconsistent
- Users can't find content

**Solution:** Always preserve ZWNJ:
- Consistent indexing
- Accurate search results
- Preserves linguistic meaning

### Implementation Details

#### Critical Rules

1. **Never Remove ZWNJ**
   ```python
   # ❌ WRONG - Never do this!
   text = text.replace(ZWNJ, '')  # Removes ZWNJ - BREAKS MEANING!
   text = text.replace(ZWNJ, ' ')  # Converts to space - WRONG!
   
   # ✅ CORRECT - Always preserve
   text = preserve_zwnj(text)  # Returns unchanged
   ```

2. **Never Normalize ZWNJ**
   ```python
   # ❌ WRONG - Don't normalize
   normalized = unicodedata.normalize('NFC', text)  # May affect ZWNJ
   
   # ✅ CORRECT - Preserve as-is
   text = preserve_zwnj(text)  # No normalization
   ```

3. **Process Before Other Operations**
   ```python
   # ✅ CORRECT order:
   text = preserve_zwnj(text)  # First: preserve ZWNJ
   text = remove_diacritics(text)  # Then: other processing
   # ZWNJ remains intact
   ```

#### Validation and Logging

```python
def preserve_zwnj(text: str) -> str:
    """Preserve ZWNJ with validation."""
    original_count = text.count(ZWNJ)
    
    # Process (no changes, but validate)
    preserved = text  # No modifications
    
    # Verify preservation
    preserved_count = preserved.count(ZWNJ)
    if original_count != preserved_count:
        logger.error(f"ZWNJ count changed: {original_count} → {preserved_count}")
        raise ValueError("ZWNJ preservation failed!")
    
    if original_count > 0:
        logger.debug(f"Preserved {original_count} ZWNJ characters")
    
    return preserved
```

### Mathematical Formulation

**Preservation Function:**
\[
\text{preserve\_zwnj}(T) = T \quad \text{where} \quad |T|_{\text{ZWNJ}} = |\text{preserve\_zwnj}(T)|_{\text{ZWNJ}}
\]

Where:
- \( T \) = input text
- \( |T|_{\text{ZWNJ}} \) = count of ZWNJ characters in \( T \)
- Result: Text with identical ZWNJ count (preservation)

**Invariant:**
\[
\forall c \in T : c = \text{ZWNJ} \implies c \in \text{preserve\_zwnj}(T)
\]

All ZWNJ characters in input must appear in output.

## CJK Segmentation Algorithms

### Chinese Segmentation (jieba)

#### Overview

Chinese text has no spaces between words. Word segmentation is essential for:
- Search indexing
- Text analysis
- Information retrieval

#### jieba Algorithm

**jieba** uses a combination of:
1. **Prefix Dictionary:** Pre-built word dictionary
2. **HMM (Hidden Markov Model):** For unknown words
3. **Viterbi Algorithm:** For optimal segmentation

#### Detailed Process

**Step 1: Dictionary Loading**
```python
# jieba loads a dictionary of known words
# Format: word frequency part_of_speech
# Example:
# 你好 1000
# 世界 500
# 中国 800

# Dictionary size: ~200,000+ words
```

**Step 2: Text Segmentation**
```python
def segment_chinese(text: str) -> List[str]:
    """
    Segment Chinese text using jieba.
    
    Algorithm:
    1. Load jieba model (lazy loading)
    2. Use jieba.cut() for segmentation
    3. Return word list
    """
    jieba = _get_jieba()  # Lazy load
    
    # jieba.cut() uses:
    # - Dictionary matching (longest match)
    # - HMM for unknown words
    # - Viterbi for optimal path
    words = jieba.cut(text, cut_all=False)
    
    return list(words)
```

**Step 3: Word Boundary Extraction**
```python
def get_word_boundaries(text: str, words: List[str]) -> List[int]:
    """
    Extract character positions of word boundaries.
    
    Example:
    text = "你好世界"
    words = ["你好", "世界"]
    boundaries = [0, 2, 4]  # Start, end of "你好", end of "世界"
    """
    boundaries = [0]
    current_pos = 0
    
    for word in words:
        pos = text.find(word, current_pos)
        if pos != -1:
            boundaries.append(pos + len(word))
            current_pos = pos + len(word)
    
    return sorted(set(boundaries))
```

#### jieba Segmentation Modes

**1. Precise Mode (Default)**
```python
words = jieba.cut(text, cut_all=False)
# "你好世界" → ["你好", "世界"]
# Most accurate, recommended for search
```

**2. Full Mode**
```python
words = jieba.cut(text, cut_all=True)
# "你好世界" → ["你", "好", "世界", "你好", "好世", "世界"]
# All possible words (for analysis)
```

**3. Search Mode**
```python
words = jieba.cut_for_search(text)
# "你好世界" → ["你好", "世界", "你", "好"]
# Includes shorter words for search
```

#### Performance Characteristics

- **Accuracy:** ~85-95% on general text
- **Speed:** jieba-fast ~10x faster than standard jieba
- **Memory:** ~50MB dictionary
- **Latency:** <5ms per sentence

### Japanese Tokenization

#### Overview

Japanese text mixes three scripts:
- **Hiragana** (ひらがな): Phonetic script
- **Katakana** (カタカナ): Foreign words, emphasis
- **Kanji** (漢字): Chinese characters

#### MeCab Method (Preferred)

**MeCab** is a morphological analyzer:

**Step 1: MeCab Initialization**
```python
def _get_mecab():
    """Lazy load MeCab tagger."""
    global _mecab
    if _mecab is None:
        import MeCab
        # -Owakati: Output format (space-separated words)
        _mecab = MeCab.Tagger("-Owakati")
    return _mecab
```

**Step 2: Tokenization**
```python
def segment_japanese(text: str) -> List[str]:
    """Segment Japanese using MeCab."""
    mecab = _get_mecab()
    
    if mecab:
        # MeCab parses text and outputs tokens
        output = mecab.parse(text)
        # "こんにちは世界" → "こんにちは 世界"
        tokens = output.strip().split()
        return tokens
```

**MeCab Output Format:**
```
Input:  "こんにちは世界"
Output: "こんにちは 世界"
Tokens: ["こんにちは", "世界"]
```

#### Regex Fallback Method

When MeCab unavailable, use script-boundary detection:

**Algorithm:**
```python
def segment_japanese_regex(text: str) -> List[str]:
    """
    Regex-based Japanese segmentation.
    
    Strategy:
    1. Detect script boundaries (Hiragana/Katakana/Kanji)
    2. Split at script transitions
    3. Handle punctuation separately
    """
    tokens = []
    current_token = []
    
    for char in text:
        script = detect_script_type(char)
        
        if script_changed(current_token, script):
            tokens.append(''.join(current_token))
            current_token = [char]
        else:
            current_token.append(char)
    
    return tokens
```

**Script Detection:**
```python
def detect_script_type(char: str) -> str:
    """Detect script type of character."""
    if '\u3040' <= char <= '\u309F':  # Hiragana
        return 'hiragana'
    elif '\u30A0' <= char <= '\u30FF':  # Katakana
        return 'katakana'
    elif '\u4E00' <= char <= '\u9FAF':  # Kanji
        return 'kanji'
    else:
        return 'other'
```

**Limitations:**
- Less accurate than MeCab
- Doesn't handle compound words well
- No part-of-speech information

### Korean Segmentation

#### Overview

Korean uses **spaces** for word boundaries (unlike Chinese/Japanese), but also has:
- **Hangul syllables:** Composed characters
- **Compound words:** May need further segmentation

#### Algorithm

**Step 1: Space-Based Segmentation**
```python
def segment_korean(text: str) -> List[str]:
    """
    Segment Korean text.
    
    Korean uses spaces, so basic segmentation is simple:
    """
    # Split by spaces (primary method)
    words = text.split()
    
    # Further processing could include:
    # - Morphological analysis (KoNLPy)
    # - Compound word splitting
    # - Honorific handling
    
    return words
```

**Example:**
```python
text = "안녕하세요 세계"
words = segment_korean(text)
# ["안녕하세요", "세계"]
```

**Future Enhancement:**
- Use KoNLPy for morphological analysis
- Handle compound words
- Extract morphemes for better indexing

## Cyrillic Normalization Rules

### Variant Unification Algorithm

#### Overview

Cyrillic script has character variants:
- **ё (U+0451)** vs **е (U+0435)**: Often normalized for consistency
- **і (U+0456)** vs **и (U+0438)**: Ukrainian/Belarusian specific

#### Algorithm Steps

**Step 1: Character Detection**
```python
def unify_cyrillic_variants(text: str, normalize_yo: bool = True) -> str:
    """
    Unify Cyrillic variants.
    
    Algorithm:
    1. Iterate through characters
    2. Detect ё (U+0451)
    3. Replace with е (U+0435) if normalize_yo=True
    4. Preserve language-specific characters
    """
    result = []
    normalized_count = 0
    
    for char in text:
        if char == CYRILLIC_YO and normalize_yo:
            result.append(CYRILLIC_E)  # ё → е
            normalized_count += 1
        else:
            result.append(char)  # Preserve
    
    return ''.join(result)
```

**Step 2: Language-Specific Handling**
```python
# Russian: Usually normalize ё → е
text_ru = "ёлка"
normalized = unify_cyrillic_variants(text_ru, normalize_yo=True)
# "елка"

# Ukrainian: Preserve specific characters
# і (U+0456), ї (U+0457), ґ (U+0491) are preserved
```

#### Mathematical Formulation

**Normalization Function:**
\[
\text{normalize}(c) = \begin{cases}
е & \text{if } c = ё \text{ and } \text{normalize\_yo} = \text{True} \\
c & \text{otherwise}
\end{cases}
\]

**Text Transformation:**
\[
\text{unify}(T) = [\text{normalize}(c) \text{ for } c \in T]
\]

### Language-Specific Handling

#### Character Preservation Rules

**Ukrainian Characters:**
- **і (U+0456)**: Ukrainian/Belarusian i (preserve)
- **ї (U+0457)**: Ukrainian yi (preserve)
- **ґ (U+0491)**: Ukrainian ghe (preserve)

**Belarusian Characters:**
- **і (U+0456)**: Also Belarusian (preserve)

**Implementation:**
```python
UKRAINIAN_SPECIFIC = {
    '\u0456',  # і
    '\u0457',  # ї
    '\u0491',  # ґ
}

# These are NEVER normalized
# They are language-specific and must be preserved
```

## Latin Script Processing

### Diacritic Normalization

#### Overview

Latin script uses diacritics (accents) that may need normalization:
- **é → e**: French/Spanish
- **ñ → n**: Spanish
- **ü → u**: German/Turkish

#### Algorithm

**Step 1: Unicode Decomposition**
```python
def normalize_diacritics(text: str) -> str:
    """
    Normalize diacritics using Unicode NFD decomposition.
    
    Algorithm:
    1. Decompose characters (é → e + combining acute)
    2. Remove combining marks (diacritics)
    3. Reconstruct base characters
    """
    normalized = []
    
    for char in text:
        # NFD: Normalization Form Decomposed
        decomposed = unicodedata.normalize('NFD', char)
        # "é" → "e" + "\u0301" (combining acute)
        
        # Keep only base characters (remove combining marks)
        base_chars = [
            c for c in decomposed
            if unicodedata.category(c) != 'Mn'  # Mn = Nonspacing Mark
        ]
        
        normalized.extend(base_chars)
    
    return ''.join(normalized)
```

**Example:**
```python
text = "café"
normalized = normalize_diacritics(text)
# "cafe" (é → e)
```

#### Language-Specific Preservation

**Languages that preserve diacritics:**
- French: é, è, ê, ç
- Spanish: ñ, á, é, í, ó, ú
- German: ä, ö, ü, ß
- Polish: ą, ć, ę, ł, ń, ó, ś, ź, ż

**Implementation:**
```python
PRESERVE_DIACRITICS_LANGUAGES = {
    'fr', 'es', 'de', 'pl', 'cs', 'sk', 'hu', 'ro', 'tr', 'vi',
    'is', 'da', 'no', 'sv', 'fi', 'et', 'lv', 'lt'
}

def process_latin(text: str, language_code: str) -> ProcessedText:
    should_preserve = language_code in PRESERVE_DIACRITICS_LANGUAGES
    
    if not should_preserve:
        text = normalize_diacritics(text)
    
    return ProcessedText(text=text, ...)
```

### Ligature Handling

#### Overview

Ligatures are combined characters:
- **Semantic ligatures:** æ, œ (have meaning)
- **Non-semantic ligatures:** ﬁ, ﬂ (typographic)

#### Algorithm

**Step 1: Classification**
```python
SEMANTIC_LIGATURES = {
    'æ': 'ae',  # Latin ligature
    'œ': 'oe',  # French ligature
}

NON_SEMANTIC_LIGATURES = {
    'ﬁ': 'fi',  # Typographic
    'ﬂ': 'fl',
    'ﬀ': 'ff',
}
```

**Step 2: Normalization**
```python
def handle_ligatures(text: str, preserve_semantic: bool = True) -> str:
    """
    Handle ligatures.
    
    Strategy:
    - Preserve semantic ligatures (æ, œ) by default
    - Always normalize non-semantic ligatures
    """
    result = []
    
    for char in text:
        if char in SEMANTIC_LIGATURES:
            if preserve_semantic:
                result.append(char)  # Preserve
            else:
                result.append(SEMANTIC_LIGATURES[char])  # Normalize
        elif char in NON_SEMANTIC_LIGATURES:
            result.append(NON_SEMANTIC_LIGATURES[char])  # Always normalize
        else:
            result.append(char)
    
    return ''.join(result)
```

## Bidirectional Text Handling

### Unicode BiDi Algorithm

#### Overview

Bidirectional text mixes RTL (Right-to-Left) and LTR (Left-to-Right) scripts:
- **RTL:** Arabic, Hebrew
- **LTR:** Latin, Cyrillic, CJK

#### Unicode Bidirectional Algorithm

The Unicode BiDi algorithm determines text display order:

**Basic Rules:**
1. **Strong characters:** Determine direction (Arabic = RTL, Latin = LTR)
2. **Neutral characters:** Inherit direction from context
3. **Directional overrides:** Explicit direction markers

**Implementation:**
```python
def handle_bidirectional_text(text: str) -> str:
    """
    Handle bidirectional text.
    
    Note: Full BiDi algorithm is complex.
    We preserve logical order here.
    For rendering, use python-bidi library.
    """
    # Preserve logical order (as stored in memory)
    # Visual rendering handled by rendering system
    return text
```

### Script Boundary Detection

#### Algorithm

**Step 1: Character-by-Character Analysis**
```python
def detect_script_boundaries(text: str) -> List[Tuple[int, int, str]]:
    """
    Detect script boundaries in mixed-script text.
    
    Algorithm:
    1. Iterate character by character
    2. Skip whitespace/punctuation
    3. Match against script patterns
    4. Detect script transitions
    5. Create boundary segments
    """
    boundaries = []
    current_script = None
    start_pos = 0
    
    for i, char in enumerate(text):
        if char.isspace() or not char.isalnum():
            continue  # Skip
        
        # Detect script
        detected_script = detect_script_for_char(char)
        
        # If script changed, save segment
        if current_script and detected_script != current_script:
            boundaries.append((start_pos, i, current_script))
            start_pos = i
            current_script = detected_script
        elif not current_script:
            current_script = detected_script
            start_pos = i
    
    # Add final segment
    if current_script:
        boundaries.append((start_pos, len(text), current_script))
    
    return boundaries
```

**Step 2: Script Pattern Matching**
```python
SCRIPT_PATTERNS = {
    'Arab': re.compile(r'[\u0600-\u06FF...]'),  # Arabic ranges
    'Latn': re.compile(r'[a-zA-Z]'),
    'Cyrl': re.compile(r'[\u0400-\u04FF]'),
    # ... more patterns
}

def detect_script_for_char(char: str) -> str:
    """Detect script for single character."""
    for script_code, pattern in SCRIPT_PATTERNS.items():
        if pattern.match(char):
            return script_code
    return "Zyyy"  # Common/Unknown
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
- jieba: ~50MB dictionary
- MeCab: ~100MB+ model

**Implementation:**
```python
_jieba = None
_mecab = None

def _get_jieba():
    """Lazy load jieba."""
    global _jieba
    if _jieba is None:
        import jieba_fast as jieba
        _jieba = jieba
    return _jieba
```

**Benefits:**
- Faster startup (don't load unused libraries)
- Lower memory usage
- Better error handling (fail only when needed)

### Caching

**Compiled Regex Patterns:**
```python
# Module-level compilation (cached)
SCRIPT_PATTERNS = {
    'Arab': re.compile(r'[\u0600-\u06FF...]'),  # Compiled once
    'Latn': re.compile(r'[a-zA-Z]'),
    # ...
}
```

**Model Caching:**
- jieba dictionary loaded once
- MeCab tagger initialized once
- Reused across multiple calls

### Optimized Hot Paths

**Script Detection:**
- Fast regex matching (O(1) per character)
- Early exit for single-script text
- Minimal memory allocations

**ZWNJ Handling:**
- Simple character counting (O(n))
- No complex processing
- Zero allocations

## Unicode Script Detection Details

### Script Detection Patterns

**Unicode Ranges:**
```python
SCRIPT_PATTERNS = {
    # Arabic script ranges
    'Arab': re.compile(r'[\u0600-\u06FF'      # Basic Arabic
                       r'\u0750-\u077F'         # Supplement
                       r'\u08A0-\u08FF'         # Extended-A
                       r'\uFB50-\uFDFF'         # Presentation Forms-A
                       r'\uFE70-\uFEFF]'),      # Presentation Forms-B
    
    # Latin script
    'Latn': re.compile(r'[a-zA-Z]'),
    
    # Cyrillic script
    'Cyrl': re.compile(r'[\u0400-\u04FF]'),
    
    # CJK Unified Ideographs
    'Hans': re.compile(r'[\u4E00-\u9FFF]'),
    'Hant': re.compile(r'[\u4E00-\u9FFF]'),
    
    # Japanese scripts
    'Jpan': re.compile(r'[\u3040-\u309F'         # Hiragana
                       r'\u30A0-\u30FF'         # Katakana
                       r'\u4E00-\u9FFF]'),      # Kanji
    
    # Korean script
    'Kore': re.compile(r'[\uAC00-\uD7AF'         # Hangul Syllables
                       r'\u1100-\u11FF'         # Hangul Jamo
                       r'\u3130-\u318F]'),      # Compatibility Jamo
}
```

### ISO 15924 Script Codes

**Standard Script Codes:**
- `Arab`: Arabic script
- `Latn`: Latin script
- `Cyrl`: Cyrillic script
- `Hans`: Simplified Chinese (Han)
- `Hant`: Traditional Chinese (Han)
- `Jpan`: Japanese
- `Kore`: Korean
- `Zyyy`: Common (unknown script)

### Boundary Detection Complexity

**Time Complexity:** O(n) where n = text length
- Single pass through text
- Constant-time pattern matching per character

**Space Complexity:** O(k) where k = number of script segments
- Store boundary tuples
- Typically k << n

## Learning Resources

### ZWNJ and Arabic Script Processing

**ZWNJ Fundamentals:**
- **[Unicode ZWNJ Specification](https://unicode.org/charts/PDF/U2000.pdf)** - Official Unicode specification for ZWNJ (U+200C)
- **[Zero-Width Non-Joiner (Wikipedia)](https://en.wikipedia.org/wiki/Zero-width_non-joiner)** - Comprehensive explanation of ZWNJ
- **[Persian Text Processing Guide](https://www.unicode.org/reports/tr53/)** - Unicode Technical Report on Persian text handling

**Arabic Script Processing:**
- **[Arabic Text Processing (NLP Guide)](https://www.nltk.org/book/ch12.html)** - Natural Language Processing with Python chapter on Arabic
- **[python-arabic-reshaper](https://github.com/mpcabd/python-arabic-reshaper)** - Python library for Arabic text reshaping
- **[Arabic NLP Resources](https://github.com/ARBML/ARBML)** - Arabic NLP tools and resources

### CJK Text Segmentation

**Chinese Word Segmentation:**
- **[jieba GitHub Repository](https://github.com/fxsjy/jieba)** - Official jieba library with documentation
- **[jieba-fast Documentation](https://github.com/deepcs233/jieba_fast)** - Fast jieba implementation
- **[Chinese Word Segmentation Survey](https://arxiv.org/abs/1808.04911)** - Research paper on Chinese segmentation methods
- **[Stanford Chinese NLP](https://web.stanford.edu/class/cs224n/)** - Stanford NLP course covering Chinese processing

**Japanese Tokenization:**
- **[MeCab Official Site](https://taku910.github.io/mecab/)** - MeCab morphological analyzer documentation
- **[MeCab Python Tutorial](https://github.com/SamuraiT/mecab-python3)** - Python bindings for MeCab
- **[Japanese NLP Guide](https://www.nltk.org/book/ch12.html)** - NLTK book chapter on Japanese processing
- **[SudachiPy](https://github.com/WorksApplications/SudachiPy)** - Alternative Japanese tokenizer

**Korean Text Processing:**
- **[KoNLPy Documentation](https://konlpy.org/)** - Korean NLP library for Python
- **[Korean Morphological Analysis](https://github.com/hyunwoongko/kss)** - Korean sentence splitter
- **[Hangul Processing Guide](https://github.com/kaniblu/hangul-toolkit)** - Hangul text processing tools

### Cyrillic Script Processing

**Cyrillic Normalization:**
- **[Cyrillic Script (Wikipedia)](https://en.wikipedia.org/wiki/Cyrillic_script)** - Overview of Cyrillic script
- **[Unicode Cyrillic Blocks](https://unicode.org/charts/PDF/U0400.pdf)** - Unicode specification for Cyrillic
- **[Russian Text Processing](https://github.com/natasha/natasha)** - Russian NLP library

**Variant Handling:**
- **[ё vs е Normalization](https://en.wikipedia.org/wiki/Yo_(Cyrillic))** - Explanation of ё character
- **[Ukrainian Character Encoding](https://en.wikipedia.org/wiki/Ukrainian_alphabet)** - Ukrainian-specific characters

### Latin Script Processing

**Diacritic Normalization:**
- **[Unicode Normalization Forms](https://unicode.org/reports/tr15/)** - Unicode Technical Report on normalization
- **[NFD vs NFC Normalization](https://en.wikipedia.org/wiki/Unicode_equivalence)** - Understanding Unicode equivalence
- **[Diacritics in European Languages](https://en.wikipedia.org/wiki/Diacritic)** - Comprehensive guide to diacritics

**Ligature Handling:**
- **[Typographic Ligatures](https://en.wikipedia.org/wiki/Typographic_ligature)** - Explanation of ligatures
- **[Unicode Ligatures](https://unicode.org/charts/PDF/UFB00.pdf)** - Unicode ligature characters

### Bidirectional Text

**Unicode BiDi Algorithm:**
- **[Unicode Bidirectional Algorithm](https://unicode.org/reports/tr9/)** - Official Unicode BiDi specification
- **[python-bidi Library](https://github.com/MeirKhalili/python-bidi)** - Python implementation of BiDi algorithm
- **[RTL Text Rendering Guide](https://www.w3.org/International/questions/qa-html-dir)** - W3C guide to RTL text

**Mixed-Script Handling:**
- **[Script Detection Algorithms](https://unicode.org/reports/tr24/)** - Unicode script detection
- **[Multilingual Text Processing](https://www.nltk.org/book/ch12.html)** - NLTK multilingual processing

### Performance Optimization

**Lazy Loading Patterns:**
- **[Python Lazy Loading Patterns](https://realpython.com/python-import/)** - Real Python guide to imports
- **[Memory Optimization Techniques](https://docs.python.org/3/library/sys.html#sys.getsizeof)** - Python memory management

**Regex Optimization:**
- **[Python Regex Performance](https://docs.python.org/3/library/re.html)** - Official regex documentation
- **[Compiled Regex Patterns](https://docs.python.org/3/library/re.html#re.compile)** - Using compiled patterns

### General Unicode and Text Processing

**Unicode Fundamentals:**
- **[Unicode Standard](https://unicode.org/standard/standard.html)** - Official Unicode standard
- **[Unicode Tutorial (Joel Spolsky)](https://www.joelonsoftware.com/2003/10/08/the-absolute-minimum-every-software-developer-absolutely-positively-must-know-about-unicode-and-character-sets-no-excuses/)** - Classic Unicode tutorial
- **[Unicode Character Database](https://www.unicode.org/ucd/)** - Unicode character database

**Text Processing Libraries:**
- **[NLTK Book](https://www.nltk.org/book/)** - Natural Language Processing with Python
- **[spaCy Documentation](https://spacy.io/usage/linguistic-features)** - Modern NLP library
- **[Text Processing Best Practices](https://docs.python.org/3/library/string.html)** - Python string processing

### Implementation Examples

**Real-World Implementations:**
- **[Google's Text Normalization](https://github.com/google/text-normalization)** - Google's text normalization library
- **[Facebook's FastText](https://fasttext.cc/)** - Text classification and language detection
- **[OpenNLP](https://opennlp.apache.org/)** - Apache OpenNLP for text processing

**Code Examples:**
- **[Python Text Processing Cookbook](https://github.com/PacktPublishing/Python-Text-Processing-Cookbook)** - Practical examples
- **[NLP with Python Examples](https://github.com/nltk/nltk_book)** - NLTK book code examples

## References

1. **Unicode Standards:**
   - [Unicode Standard](https://unicode.org/standard/standard.html)
   - [ISO 15924 Script Codes](https://unicode.org/iso15924/)
   - [Unicode Technical Report #9: BiDi Algorithm](https://unicode.org/reports/tr9/)

2. **ZWNJ and Arabic:**
   - Unicode Character Database: U+200C (ZWNJ)
   - [Persian Text Processing (Unicode TR53)](https://www.unicode.org/reports/tr53/)

3. **CJK Segmentation:**
   - [jieba: Chinese Word Segmentation](https://github.com/fxsjy/jieba)
   - [MeCab: Japanese Morphological Analyzer](https://taku910.github.io/mecab/)
   - [KoNLPy: Korean NLP](https://konlpy.org/)

4. **Text Processing:**
   - [NLTK Book: Multilingual Text Processing](https://www.nltk.org/book/ch12.html)
   - [Unicode Normalization Forms (TR15)](https://unicode.org/reports/tr15/)

5. **Performance:**
   - [Python Performance Tips](https://wiki.python.org/moin/PythonSpeed/PerformanceTips)
   - [Regex Optimization Guide](https://docs.python.org/3/library/re.html)

---

**Technical implementation for script-specific multilingual text processing** 🔬
