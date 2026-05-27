# Task 01.3: Script-Specific Processing

Script-specific text processing module that processes text based on detected script codes from Task 01.2. Handles Arabic scripts (ZWNJ preservation), CJK (word segmentation), Cyrillic (variant unification), Latin (diacritic handling), and mixed-script scenarios.

## 📋 Overview

This module provides:
- **Arabic Processing:** ZWNJ preservation, diacritic removal, character shape normalization
- **CJK Processing:** Chinese (jieba segmentation), Japanese tokenization, Korean word boundaries
- **Cyrillic Processing:** Variant unification (ё → е), case folding
- **Latin Processing:** Diacritic normalization, ligature handling
- **Mixed-Script Support:** Handles bidirectional text and mixed scripts
- **High Performance:** 1000+ docs/sec throughput

## 🚀 Quick Start

### 1. Install Dependencies

```bash
pip install -r requirements.txt
```

### 2. Basic Usage

```python
from text_processing import process_by_script

# Process Arabic text (preserves ZWNJ)
result = process_by_script("می‌خواهم", language_info)
print(result.text)  # "می‌خواهم" (ZWNJ preserved)
print(result.applied_rules)  # ["preserve_zwnj", "remove_diacritics"]

# Process Chinese text (word segmentation)
result = process_by_script("你好世界", language_info)
print(result.word_boundaries)  # [0, 2, 4] (word boundaries)
```

### 3. Run Tests

```bash
pytest tests/ -v --cov=text_processing
```

## 💻 Usage

### Basic Processing

```python
from text_processing import ScriptHandler

handler = ScriptHandler()

# Process text with LanguageInfo from Task 01.2
result = handler.process_by_script(
    text="می‌خواهم",
    language_info=LanguageInfo(
        language_code="fa",
        script_code="Arab",
        confidence=0.98
    )
)

print(result.text)           # Processed text
print(result.script_code)    # "Arab"
print(result.applied_rules)  # ["preserve_zwnj", ...]
```

### Mixed-Script Processing

```python
# Handle bidirectional text
result = handler.process_mixed_script(
    text="Hello سلام World",
    language_info=LanguageInfo(...)
)

# Each script segment processed separately
```

## 📦 Project Structure

```
01.3-script-specific-processing/
├── text_processing/
│   ├── script_handler.py      # Main orchestrator (300-400 lines)
│   ├── arabic_processor.py     # Arabic/Persian/Urdu (150 lines)
│   ├── cjk_processor.py        # Chinese/Japanese/Korean (200 lines)
│   ├── cyrillic_processor.py   # Cyrillic scripts (100 lines)
│   ├── latin_processor.py      # Latin scripts (100 lines)
│   └── __init__.py
├── tests/
│   └── test_script_processing.py  # Comprehensive tests (200+ cases)
├── docs/
│   ├── ALGORITHMS.md            # Technical implementation details
│   └── api/
│       └── script-processing.md # API documentation
├── examples/
│   └── integration_example.py  # Integration examples
├── shared/
│   └── logger.py               # Logging utilities
├── requirements.txt
├── requirements-dev.txt
├── setup.py
├── pytest.ini
└── README.md
```

## 🎯 Supported Scripts

### Arabic Script (Arab)
- **ZWNJ Preservation:** Critical for Persian grammar
- **Diacritic Removal:** Normalizes Arabic diacritics
- **Shape Normalization:** Handles contextual forms

### CJK Scripts
- **Chinese (Hans/Hant):** jieba word segmentation
- **Japanese (Jpan):** Tokenization (MeCab optional)
- **Korean (Kore):** Hangul syllable handling

### Cyrillic Script (Cyrl)
- **Variant Unification:** ё → е (configurable)
- **Case Folding:** Proper Cyrillic case handling

### Latin Script (Latn)
- **Diacritic Normalization:** é → e (configurable)
- **Ligature Handling:** æ, œ semantic preservation

## 📊 Performance Requirements

- **Throughput:** 1000+ docs/sec
- **Latency:** <10ms per document
- **Accuracy:** CJK segmentation ≥85%, ZWNJ preservation 100%

## 🧪 Testing

```bash
# Run all tests
pytest tests/ -v

# Run with coverage
pytest tests/ --cov=text_processing --cov-report=html

# Run specific script tests
pytest tests/test_script_processing.py::TestArabicProcessor -v
pytest tests/test_script_processing.py::TestCJKProcessor -v
```

## 🔗 Integration

### With Task 01.2 (Language Detection)

```python
from pathlib import Path
import sys

# Import LanguageInfo from Task 01.2
task_01_2_path = Path(__file__).parent.parent / "01.2-language-detection"
sys.path.insert(0, str(task_01_2_path))
from text_processing import LanguageInfo

# Use LanguageInfo for processing
result = process_by_script(text, language_info)
```

## 📖 Documentation

- **[README.md](README.md)** - This file (overview & quick start)
- **[ALGORITHMS.md](docs/ALGORITHMS.md)** - Technical implementation details
- **[script-processing.md](docs/api/script-processing.md)** - API reference

## 🐛 Troubleshooting

### Import Error: LanguageInfo

**Solution:** Ensure Task 01.2 is in the correct path or install it as a package.

### jieba Not Found

**Solution:** Install dependencies:
```bash
pip install -r requirements.txt
```

## 📝 License

Part of search-engine-core project.

---

**Built with ❤️ for universal multilingual search**

Last updated: 2025-01-XX
