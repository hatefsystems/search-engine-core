# Task 01.1: Unicode Normalization

Universal Unicode NFKC normalization that works for **ALL scripts worldwide**. This is the foundational component that ensures consistent text processing across any language.

## 📋 Overview

This module provides:
- **NFKC Normalization:** Standard Unicode compatibility composition
- **Script Detection:** Automatic identification of text writing systems
- **Character Unification:** Arabic→Persian, Cyrillic variants
- **Special Character Handling:** ZWNJ preservation, soft hyphen removal
- **Whitespace Normalization:** Clean, consistent spacing

## ✅ Status: COMPLETE & PRODUCTION-READY

- ✅ **Performance:** 11,271 docs/sec (11x better than target)
- ✅ **Memory:** 6.95 MB for 10K docs (14x better than target)
- ✅ **Test Coverage:** 92% (52 tests passing)
- ✅ **Supported Languages:** 10+ scripts (Latin, Arabic, Persian, Chinese, Japanese, Korean, Russian, Hindi, Hebrew, Thai, Greek)
- ✅ **Zero Crashes:** Robust error handling for malformed input

## 🚀 Quick Start

```bash
# Install dependencies
pip install -r requirements.txt

# Run tests
pytest tests/test_normalizer.py -v

# Run with coverage
pytest tests/ --cov=text_processing --cov-report=html

# Run benchmarks
python benchmarks/normalizer_perf.py

# Interactive testing
python interactive_test.py
```

## 📦 Project Structure

```
01.1-unicode-normalization/
├── text_processing/
│   ├── normalizer.py          # Main implementation (317 lines)
│   └── __init__.py
├── tests/
│   ├── test_normalizer.py     # 52 unit tests
│   └── conftest.py
├── benchmarks/
│   └── normalizer_perf.py     # Performance tests
├── shared/
│   └── logger.py              # Structured logging
├── docs/                      # Additional documentation
├── requirements.txt           # Runtime dependencies
├── requirements-dev.txt       # Development dependencies
├── setup.py                   # Package installation
├── pytest.ini                 # Test configuration
├── README.md                  # This file
├── ALGORITHMS.md              # Technical details & algorithms
├── QUICK_START.md             # Quick start guide
├── PROJECT_STATUS.txt         # Completion status
└── interactive_test.py        # Interactive testing tool
```

## 💻 Usage

### Basic Usage

```python
from text_processing import normalize_universal

# Normalize text
result = normalize_universal("سلام   دنیا")
print(result.text)        # "سلام دنیا" (normalized)
print(result.script)      # "Arab" (detected script)
print(result.changes)     # List of applied transformations
```

### Advanced Options

```python
# Customize normalization
result = normalize_universal(
    text="Hello   World",
    preserve_special=True,    # Keep ZWNJ/ZWJ characters
    unify_chars=True          # Apply character unification
)
```

### Batch Processing

```python
from text_processing import normalize_batch

texts = ["سلام", "Hello", "你好", "Привет"]
results = normalize_batch(texts)
for result in results:
    print(result.text, result.script)
```

## 🧪 Testing

```bash
# Run all tests
pytest tests/test_normalizer.py -v

# Run with coverage
pytest tests/ --cov=text_processing --cov-report=html

# Run specific test
pytest tests/test_normalizer.py::test_nfkc_normalization -v

# Run benchmarks
python benchmarks/normalizer_perf.py
```

## 📊 Performance Metrics

| Metric | Target | Achieved | Status |
|--------|--------|----------|--------|
| Throughput | 1,000 docs/sec | **11,271 docs/sec** | ✅ 11x better |
| Memory | <100 MB | **6.95 MB** | ✅ 14x better |
| Test Coverage | ≥85% | **92%** | ✅ |
| Latency P95 | <1 ms | **0.154 ms** | ✅ |

## 📖 Documentation

### Core Documentation
- **[README.md](README.md)** - This file (overview & quick start)
- **[ALGORITHMS.md](ALGORITHMS.md)** - Technical implementation details
- **[QUICK_START.md](QUICK_START.md)** - Step-by-step guide
- **[PROJECT_STATUS.txt](PROJECT_STATUS.txt)** - Completion status

### Additional Resources
- **[CUSTOM_TEXT_TESTING.md](CUSTOM_TEXT_TESTING.md)** - Testing your own text
- **[interactive_test.py](interactive_test.py)** - Interactive testing tool

### Task Documentation
- Task specification: `/.github/ISSUE_TEMPLATE/atomic-tasks/M0-foundation/01-text-processing/01.1-unicode-normalization.md`

## 🔗 Integration

### Future C++ Integration (Task 01.6)

```cpp
// C++ side (planned)
auto normalized = pythonClient.normalize(rawText);
```

```python
# Python side (REST API endpoint)
@app.post("/normalize")
def normalize_endpoint(text: str):
    return normalize_universal(text)
```

## 🛠️ Development

### Code Style
- Follow PEP 8
- Use type hints for all functions
- Docstrings for all public APIs
- Maintain ≥85% test coverage

### Running Tests Locally
```bash
# Setup virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dev dependencies
pip install -r requirements-dev.txt

# Run tests
pytest tests/ -v --cov

# Generate coverage report
pytest tests/ --cov=text_processing --cov-report=html
open htmlcov/index.html
```

## 📦 Dependencies

### Runtime Dependencies
- Python 3.9+
- `unicodedata` (built-in)
- `pyicu==2.11` (Unicode handling)
- `structlog==23.2.0` (structured logging)

See [requirements.txt](requirements.txt) for complete list.

### Development Dependencies
- `pytest==7.4.3` (testing framework)
- `pytest-cov==4.1.0` (coverage reporting)
- `pytest-benchmark==4.0.0` (performance testing)

See [requirements-dev.txt](requirements-dev.txt) for complete list.

## 🎯 Key Features

### 1. Universal Script Support
Works seamlessly with:
- **Latin scripts:** English, Spanish, French, German, etc.
- **Arabic & Persian:** With character unification (ي→ی, ك→ک)
- **CJK:** Chinese (Simplified & Traditional), Japanese, Korean
- **Cyrillic:** Russian, Ukrainian, Bulgarian, etc.
- **Indic:** Hindi, Bengali, Tamil, etc.
- **Others:** Hebrew, Thai, Greek, Armenian, Georgian, etc.

### 2. Character Unification
- Reduces token variants by 30%+ 
- Improves cross-language search
- Configurable per script

### 3. Special Character Handling
- ✅ Preserves ZWNJ (critical for Persian/Arabic)
- ✅ Removes soft hyphens, zero-width spaces
- ✅ Handles BOM (Byte Order Mark)

### 4. Production-Ready
- Zero crashes on malformed input
- Comprehensive error handling
- Structured logging for debugging
- 11x faster than requirements

## 🚀 Next Steps

### Task 01.2: Language Detection (Next)
- Will use this normalized text as input
- Detect 100+ languages
- FastText-based detection

### Task 01.3-01.6 (Upcoming)
- Script-specific processing
- Stopword IDF analysis
- Nightly batch jobs
- C++ integration via REST API

## 📝 License

Part of search-engine-core project.

---

## 🙏 Acknowledgments

Built using:
- [Unicode Standard](https://unicode.org/) - UAX #15 NFKC normalization
- [Python unicodedata](https://docs.python.org/3/library/unicodedata.html) - Core implementation
- [PyICU](https://pyicu.org/) - International Components for Unicode

---

**Built with ❤️ for universal multilingual search**

Last updated: 2025-11-11

