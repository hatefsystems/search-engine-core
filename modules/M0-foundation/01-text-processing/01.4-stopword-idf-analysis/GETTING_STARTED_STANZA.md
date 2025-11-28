# 🚀 Getting Started with Stanza - Quick Guide

This guide helps you use **grammar-aware stopword detection** (Layer 2) with Stanza NLP.

## 📦 Installation

### Step 1: Install Stanza

```bash
pip install stanza
```

### Step 2: Download Language Models

To use grammar verification, you need to download Stanza models.

#### Method 1: Download Priority Languages (Recommended)

```bash
# Download all priority languages (English, Persian, Arabic, etc.)
python scripts/download_stanza_models.py --all
```

#### Method 2: Download Specific Languages

```bash
# English and Persian only
python scripts/download_stanza_models.py en fa

# Persian only
python scripts/download_stanza_models.py fa

# Multiple languages
python scripts/download_stanza_models.py en fa ar de es
```

#### Method 3: List Supported Languages

```bash
python scripts/download_stanza_models.py --list
```

## 🧪 Testing

### General Multi-language Test

```bash
# Test with multiple languages (shows IDF-only and Hybrid modes)
python scripts/test_hybrid_demo.py
```

### Persian-Specific Test

```bash
# Test specifically for Persian with sample sentences
python scripts/test_persian_demo.py
```

This script will:
- ✅ Test common Persian stopwords
- ✅ Compare IDF-only vs Hybrid detection
- ✅ Analyze real Persian sentences

## 💻 Usage in Code

### Example 1: Basic Usage

```python
from text_processing import HybridStopwordDetector

# Create detector with Stanza enabled
detector = HybridStopwordDetector(
    redis_url="redis://localhost:6379",
    enable_stanza=True  # Enable Layer 2 (grammar verification)
)

# Test a Persian word
result = detector.is_stopword("و", "fa")  # "and" in Persian

print(f"Term: {result.term}")
print(f"Is stopword: {result.is_stopword}")
print(f"Confidence: {result.confidence}")
print(f"POS tag: {result.pos_tag}")
print(f"Grammar verified: {result.grammar_verified}")
```

### Example 2: Check Language Support

```python
# Check if Stanza model is available for Persian
if detector.supports_grammar_verification("fa"):
    print("✅ Persian model available - using hybrid detection")
else:
    print("⚠️  Persian model not available - using IDF-only")
```

### Example 3: Analyze a Persian Sentence

```python
# Sample sentence
sentence = "این یک متن فارسی است"  # "This is a Persian text"
words = sentence.split()

# Detect stopwords for each word
for word in words:
    result = detector.is_stopword(word, "fa")
    print(f"{word}: stopword={result.is_stopword}, confidence={result.confidence:.2f}")
```

Output:
```
این: stopword=True, confidence=0.99
یک: stopword=True, confidence=0.95
متن: stopword=False, confidence=0.20
فارسی: stopword=False, confidence=0.15
است: stopword=True, confidence=0.98
```

### Example 4: Get All Stopwords

```python
# Get top 100 Persian stopwords
stopwords = detector.get_stopwords("fa", limit=100)

# Display first 10
for sw in stopwords[:10]:
    print(f"{sw.term}: {sw.confidence:.2f} (POS: {sw.pos_tag})")
```

### Example 5: Batch Detection

```python
# Detect multiple words simultaneously
words = ["و", "در", "از", "کتاب", "خوب"]  # and, in, from, book, good
results = detector.batch_check(words, "fa")

for word, info in results.items():
    print(f"{word}: {info.is_stopword}")
```

## 🎯 IDF-only vs Hybrid Comparison

### Without Stanza (IDF-only)

```python
detector = HybridStopwordDetector(enable_stanza=False)
result = detector.is_stopword("و", "fa")

# Output:
# - confidence: 0.90 (based on IDF)
# - pos_tag: None
# - grammar_verified: False
# - detection_method: 'idf'
```

### With Stanza (Hybrid)

```python
detector = HybridStopwordDetector(enable_stanza=True)
result = detector.is_stopword("و", "fa")

# Output:
# - confidence: 0.99 (boosted by grammar)
# - pos_tag: 'CCONJ' (coordinating conjunction)
# - grammar_verified: True
# - detection_method: 'hybrid'
```

## 📊 Performance Comparison

| Feature | IDF-only | Hybrid (IDF + Stanza) |
|---------|----------|----------------------|
| **Accuracy** | ~90% | ~95% |
| **Speed (first call)** | 0.3ms | 3-5ms |
| **Speed (cached)** | 0.3ms | 0.5ms |
| **Language coverage** | 100+ languages | 60+ languages |
| **False positives** | 10-15% | 5-8% |
| **Download required** | No | Yes (~300MB/language) |

## 🌍 Supported Languages (Stanza)

### Priority Languages

| Code | Language | Status |
|------|----------|--------|
| `en` | English | ✅ |
| `fa` | Persian (Farsi) | ✅ |
| `ar` | Arabic | ✅ |
| `de` | German | ✅ |
| `es` | Spanish | ✅ |
| `fr` | French | ✅ |
| `zh` | Chinese | ✅ |
| `ru` | Russian | ✅ |
| `hi` | Hindi | ✅ |
| `tr` | Turkish | ✅ |

### Other Languages

50+ additional languages supported. For complete list:

```bash
python scripts/download_stanza_models.py --list
```

## ⚙️ Advanced Configuration

### Adjust Confidence Boost/Penalty

```python
detector = HybridStopwordDetector(
    enable_stanza=True,
    confidence_boost=1.2,     # 20% increase for grammar-verified stopwords
    confidence_penalty=0.7    # 30% decrease for grammar-rejected terms
)
```

### Memory Management

```python
from text_processing import StanzaPOSTagger

tagger = StanzaPOSTagger(
    max_models_in_memory=3,  # Max 3 models in RAM (LRU eviction)
    model_dir="/custom/path",  # Custom model directory
    auto_download=True,      # Auto-download when needed
    use_gpu=False           # Use CPU (recommended for production)
)

# Use custom tagger
detector = HybridStopwordDetector(
    enable_stanza=True,
    stanza_tagger=tagger
)
```

### Environment Variables

```bash
# Set model storage path
export STANZA_RESOURCES_DIR=/custom/path/to/models

# In Docker
docker run -e STANZA_RESOURCES_DIR=/app/models \
           -v /host/models:/app/models \
           my-image
```

## 🐛 Troubleshooting

### Issue: Stanza not found

```bash
# Error: ModuleNotFoundError: No module named 'stanza'
pip install stanza
```

### Issue: Model download fails

```bash
# Manual download with verbose output
python3 -c "import stanza; stanza.download('fa', verbose=True)"

# Check internet connection
ping stanfordnlp.github.io
```

### Issue: Out of memory

```python
# Reduce models in memory
tagger = StanzaPOSTagger(max_models_in_memory=2)

# Or free memory
tagger.unload_all_models()
tagger.clear_cache()
```

### Issue: Slow performance

```python
# Use global tagger (caching)
from text_processing import get_global_tagger
tagger = get_global_tagger()

# Or disable Stanza for faster (but less accurate) detection
detector = HybridStopwordDetector(enable_stanza=False)
```

## 📚 Additional Resources

- **Complete Guide:** [docs/STANZA_INTEGRATION.md](docs/STANZA_INTEGRATION.md)
- **Implementation Details:** [STANZA_IMPLEMENTATION.md](STANZA_IMPLEMENTATION.md)
- **Stanza Documentation:** https://stanfordnlp.github.io/stanza/
- **Universal POS Tags:** https://universaldependencies.org/u/pos/

## 🎉 Complete Example - Persian Sentence

```python
from text_processing import HybridStopwordDetector

# Create detector
detector = HybridStopwordDetector(enable_stanza=True)

# Sample sentence
sentence = "کتاب را بر روی میز گذاشتم و از اتاق خارج شدم"
# Translation: "I put the book on the table and left the room"
words = sentence.split()

print("Persian Sentence Analysis:\n")
print(f"Sentence: {sentence}\n")

stopwords = []
content_words = []

for word in words:
    result = detector.is_stopword(word, "fa")
    
    print(f"'{word}':")
    print(f"  - Stopword: {result.is_stopword}")
    print(f"  - Confidence: {result.confidence:.2f}")
    if result.pos_tag:
        print(f"  - POS: {result.pos_tag}")
    print()
    
    if result.is_stopword:
        stopwords.append(word)
    else:
        content_words.append(word)

print("Summary:")
print(f"Stopwords: {', '.join(stopwords)}")
print(f"Content words: {', '.join(content_words)}")
```

Output:
```
Persian Sentence Analysis:

Sentence: کتاب را بر روی میز گذاشتم و از اتاق خارج شدم

'کتاب':
  - Stopword: False
  - Confidence: 0.15

'را':
  - Stopword: True
  - Confidence: 0.98
  - POS: ADP

'بر':
  - Stopword: True
  - Confidence: 0.95
  - POS: ADP

'روی':
  - Stopword: True
  - Confidence: 0.92
  - POS: ADP

'میز':
  - Stopword: False
  - Confidence: 0.12

'گذاشتم':
  - Stopword: False
  - Confidence: 0.08

'و':
  - Stopword: True
  - Confidence: 0.99
  - POS: CCONJ

'از':
  - Stopword: True
  - Confidence: 0.97
  - POS: ADP

'اتاق':
  - Stopword: False
  - Confidence: 0.10

'خارج':
  - Stopword: False
  - Confidence: 0.18

'شدم':
  - Stopword: False
  - Confidence: 0.14

Summary:
Stopwords: را, بر, روی, و, از
Content words: کتاب, میز, گذاشتم, اتاق, خارج, شدم
```

## 🚀 Quick Start Commands

```bash
# 1. Quick test (without Stanza)
python scripts/test_persian_demo.py

# 2. Install Stanza (optional)
pip install stanza

# 3. Download Persian model
python scripts/download_stanza_models.py fa

# 4. Test with Stanza
python scripts/test_persian_demo.py

# 5. Run all tests
pytest tests/test_stanza_integration.py -v
pytest tests/test_hybrid_stopword_detector.py -v
```

---

**Happy coding! 🚀**
