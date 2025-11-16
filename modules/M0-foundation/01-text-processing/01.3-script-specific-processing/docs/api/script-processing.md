# API Reference - Script-Specific Processing

Complete API documentation for Task 01.3 script-specific text processing.

## Table of Contents

1. [Main API](#main-api)
2. [Arabic Processor](#arabic-processor)
3. [CJK Processor](#cjk-processor)
4. [Cyrillic Processor](#cyrillic-processor)
5. [Latin Processor](#latin-processor)
6. [Script Handler](#script-handler)
7. [Data Structures](#data-structures)

## Main API

### `process_by_script`

Convenience function to process text by script.

```python
from text_processing import process_by_script

result = process_by_script(
    text: str,
    language_info: LanguageInfo,
    **kwargs
) -> ProcessedText
```

**Parameters:**
- `text` (str): Input text to process
- `language_info` (LanguageInfo): Language detection result from Task 01.2
- `**kwargs`: Processor-specific options (see individual processors)

**Returns:**
- `ProcessedText`: Processed text with metadata

**Example:**
```python
from text_processing import process_by_script

language_info = LanguageInfo(
    language_code="fa",
    script_code="Arab",
    confidence=0.98
)

result = process_by_script("می‌خواهم", language_info)
print(result.text)  # "می‌خواهم" (ZWNJ preserved)
```

### `process_mixed_script`

Process mixed-script text by detecting boundaries.

```python
from text_processing import process_mixed_script

result = process_mixed_script(
    text: str,
    language_info: LanguageInfo,
    **kwargs
) -> ProcessedText
```

**Parameters:**
- `text` (str): Input text (may contain multiple scripts)
- `language_info` (LanguageInfo): Language detection result
- `**kwargs`: Processor-specific options

**Returns:**
- `ProcessedText`: Processed text with metadata

**Example:**
```python
result = process_mixed_script("Hello سلام", language_info)
# Processes each script segment separately
```

## Arabic Processor

### `process_arabic`

Process Arabic script text (Arabic, Persian, Urdu).

```python
from text_processing.arabic_processor import process_arabic

result = process_arabic(
    text: str,
    language_code: str,
    preserve_diacritics: bool = False,
    normalize_shapes: bool = True
) -> ProcessedText
```

**Parameters:**
- `text` (str): Arabic script text
- `language_code` (str): ISO 639-1 code (ar, fa, ur, etc.)
- `preserve_diacritics` (bool): If True, keep Arabic diacritics
- `normalize_shapes` (bool): If True, normalize character shapes

**Returns:**
- `ProcessedText`: Processed text with ZWNJ preserved

**Example:**
```python
result = process_arabic("می‌خواهم", "fa")
# ZWNJ preserved, diacritics removed
```

### `preserve_zwnj`

Preserve ZWNJ characters (critical for Persian).

```python
from text_processing.arabic_processor import preserve_zwnj

text = preserve_zwnj(text: str) -> str
```

### `remove_arabic_diacritics`

Remove Arabic diacritics (tashkeel).

```python
from text_processing.arabic_processor import remove_arabic_diacritics

text = remove_arabic_diacritics(text: str) -> str
```

### `normalize_arabic_shapes`

Normalize Arabic character shapes.

```python
from text_processing.arabic_processor import normalize_arabic_shapes

text = normalize_arabic_shapes(text: str) -> str
```

## CJK Processor

### `process_cjk`

Process CJK script text (Chinese, Japanese, Korean).

```python
from text_processing.cjk_processor import process_cjk

result = process_cjk(
    text: str,
    language_code: str,
    script_code: str
) -> ProcessedText
```

**Parameters:**
- `text` (str): CJK text
- `language_code` (str): ISO 639-1 code (zh, ja, ko)
- `script_code` (str): ISO 15924 code (Hans, Hant, Jpan, Kore)

**Returns:**
- `ProcessedText`: Segmented text with word boundaries

**Example:**
```python
result = process_cjk("你好世界", "zh", "Hans")
print(result.word_boundaries)  # [0, 2, 4]
```

### `segment_chinese`

Segment Chinese text using jieba.

```python
from text_processing.cjk_processor import segment_chinese

words = segment_chinese(text: str) -> List[str]
```

**Requires:** jieba or jieba-fast

### `segment_japanese`

Segment Japanese text (MeCab or regex fallback).

```python
from text_processing.cjk_processor import segment_japanese

tokens = segment_japanese(text: str) -> List[str]
```

**Optional:** MeCab for better accuracy

### `segment_korean`

Segment Korean text.

```python
from text_processing.cjk_processor import segment_korean

words = segment_korean(text: str) -> List[str]
```

### `get_word_boundaries`

Get character positions of word boundaries.

```python
from text_processing.cjk_processor import get_word_boundaries

boundaries = get_word_boundaries(text: str, words: List[str]) -> List[int]
```

## Cyrillic Processor

### `process_cyrillic`

Process Cyrillic script text.

```python
from text_processing.cyrillic_processor import process_cyrillic

result = process_cyrillic(
    text: str,
    language_code: str,
    normalize_yo: bool = True
) -> ProcessedText
```

**Parameters:**
- `text` (str): Cyrillic text
- `language_code` (str): ISO 639-1 code (ru, uk, be, etc.)
- `normalize_yo` (bool): If True, normalize ё to е

**Returns:**
- `ProcessedText`: Processed text with variants unified

**Example:**
```python
result = process_cyrillic("ёлка", "ru", normalize_yo=True)
# "елка" (ё normalized to е)
```

### `unify_cyrillic_variants`

Unify Cyrillic character variants.

```python
from text_processing.cyrillic_processor import unify_cyrillic_variants

text = unify_cyrillic_variants(
    text: str,
    normalize_yo: bool = True,
    language_code: str = ""
) -> str
```

## Latin Processor

### `process_latin`

Process Latin script text.

```python
from text_processing.latin_processor import process_latin

result = process_latin(
    text: str,
    language_code: str,
    normalize_diacritics_flag: bool = False,
    preserve_semantic_ligatures: bool = True
) -> ProcessedText
```

**Parameters:**
- `text` (str): Latin text
- `language_code` (str): ISO 639-1 code (en, fr, es, etc.)
- `normalize_diacritics_flag` (bool): If True, normalize diacritics
- `preserve_semantic_ligatures` (bool): If True, preserve æ, œ

**Returns:**
- `ProcessedText`: Processed text

**Example:**
```python
result = process_latin("café", "fr", normalize_diacritics_flag=False)
# Diacritics preserved for French
```

### `normalize_diacritics`

Normalize diacritics in Latin text.

```python
from text_processing.latin_processor import normalize_diacritics

text = normalize_diacritics(
    text: str,
    preserve_for_languages: List[str] = None
) -> str
```

### `handle_ligatures`

Handle ligatures in Latin text.

```python
from text_processing.latin_processor import handle_ligatures

text = handle_ligatures(
    text: str,
    preserve_semantic: bool = True
) -> str
```

## Script Handler

### `ScriptHandler`

Main handler class for script-specific processing.

```python
from text_processing import ScriptHandler

handler = ScriptHandler()
```

### Methods

#### `process_by_script`

Process text based on script code.

```python
result = handler.process_by_script(
    text: str,
    language_info: LanguageInfo,
    **kwargs
) -> ProcessedText
```

#### `process_mixed_script`

Process mixed-script text.

```python
result = handler.process_mixed_script(
    text: str,
    language_info: LanguageInfo,
    **kwargs
) -> ProcessedText
```

## Data Structures

### `ProcessedText`

Result dataclass for processed text.

```python
@dataclass
class ProcessedText:
    text: str                    # Script-processed text
    original: str                # Original text
    script_code: str            # ISO 15924 script
    language_code: str          # ISO 639-1 language
    applied_rules: List[str]   # Processing rules applied
    word_boundaries: List[int]  # Word boundaries (for CJK)
    confidence: float           # Language detection confidence
```

**Example:**
```python
result = ProcessedText(
    text="processed",
    original="original",
    script_code="Arab",
    language_code="fa",
    applied_rules=["preserve_zwnj", "remove_diacritics"],
    word_boundaries=[],
    confidence=0.98
)
```

### `LanguageInfo`

Language detection result from Task 01.2.

```python
@dataclass
class LanguageInfo:
    language_code: str
    script_code: str
    confidence: float
    is_mixed_content: bool = False
    detected_languages: List[Tuple[str, float]] = None
```

## Configuration Options

### Arabic Processing

- `preserve_diacritics`: Keep Arabic diacritics (default: False)
- `normalize_shapes`: Normalize character shapes (default: True)

### Cyrillic Processing

- `normalize_yo`: Normalize ё to е (default: True)

### Latin Processing

- `normalize_diacritics`: Normalize diacritics (default: False)
- `preserve_semantic_ligatures`: Preserve æ, œ (default: True)

## Error Handling

### Missing Dependencies

**jieba:** Required for Chinese segmentation
```python
ImportError: jieba or jieba-fast required for Chinese processing
```

**MeCab:** Optional for Japanese (falls back to regex)

### Invalid Input

**Empty String:** Returns empty `ProcessedText`

**Unknown Script:** Returns text as-is with `no_processing` rule

## Performance Notes

- **Lazy Loading:** Heavy dependencies loaded on-demand
- **Caching:** Compiled patterns and models cached
- **Throughput:** 1000+ docs/sec target
- **Latency:** <10ms per document target

## Integration Examples

### With Task 01.2

```python
# Import LanguageInfo from Task 01.2
from pathlib import Path
import sys

task_01_2_path = Path(__file__).parent.parent / "01.2-language-detection"
sys.path.insert(0, str(task_01_2_path))
from text_processing import LanguageInfo

# Use for processing
language_info = LanguageInfo(
    language_code="fa",
    script_code="Arab",
    confidence=0.98
)

result = process_by_script("می‌خواهم", language_info)
```

### Full Pipeline

```python
# Task 01.1: Normalize
normalized = normalize_text(text)

# Task 01.2: Detect language
language_info = detect_language(normalized)

# Task 01.3: Process by script
processed = process_by_script(normalized, language_info)
```
