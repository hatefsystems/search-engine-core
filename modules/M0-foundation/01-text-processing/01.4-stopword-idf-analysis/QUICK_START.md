# Quick Start Guide - Stopword IDF Analysis

Get up and running with automatic stopword detection in **5 minutes**!

## Prerequisites

- Python 3.9+
- Redis (optional, for production use)
- MongoDB (optional, for corpus processing)

## Installation

### 1. Install Package

```bash
cd /root/search-engine-core/modules/M0-foundation/01-text-processing/01.4-stopword-idf-analysis
pip install -r requirements.txt
```

### 2. Verify Installation

```bash
python -c "from text_processing import IDFAnalyzer, StopwordDetector; print('✅ Installation successful!')"
```

## Basic Usage (No Redis/MongoDB Required)

### Example 1: Analyze Simple Corpus

```python
from text_processing import IDFAnalyzer

# Your document corpus
corpus = [
    "the quick brown fox jumps over the lazy dog",
    "the dog was very lazy and slept all day",
    "a quick brown fox is faster than the lazy dog",
    "the cat and the dog are best friends"
]

# Analyze corpus
analyzer = IDFAnalyzer(idf_threshold=2.0)
idf_scores = analyzer.analyze(corpus, language="en")

# Get stopword candidates
candidates = analyzer.get_stopword_candidates(min_confidence=0.7)

print("Detected Stopwords:")
for term, score in candidates:
    print(f"  {term}: confidence={score.confidence:.2f}, IDF={score.idf:.2f}")

# Output:
#   the: confidence=0.95, IDF=0.41
#   and: confidence=0.85, IDF=1.10
```

### Example 2: Check if Word is Stopword

```python
from text_processing import is_stopword

# Using bootstrap stopword lists (works offline)
result = is_stopword("the", "en")
print(f"Is 'the' a stopword? {result}")  # True

result = is_stopword("quantum", "en")
print(f"Is 'quantum' a stopword? {result}")  # False
```

### Example 3: Get Stopwords for Language

```python
from text_processing import get_stopwords

# Get top 10 English stopwords (from bootstrap)
stopwords = get_stopwords("en", limit=10)
print("Top 10 English stopwords:", stopwords)

# Output: ['the', 'a', 'an', 'and', 'or', 'but', 'in', 'on', 'at', 'to']
```

## With Redis (Production Setup)

### 1. Start Redis

```bash
# Using Docker
docker run -d -p 6379:6379 redis:latest

# Or install locally
sudo apt-get install redis-server
redis-server
```

### 2. Analyze and Export to Redis

```python
from text_processing import IDFAnalyzer, StopwordDetector

# Analyze corpus
corpus = ["your documents here..."]
analyzer = IDFAnalyzer(idf_threshold=2.0)
idf_scores = analyzer.analyze(corpus, language="en")

# Export to Redis
detector = StopwordDetector(redis_url="redis://localhost:6379")
exported = detector.export_to_redis(idf_scores, language="en")
print(f"✅ Exported {exported} stopwords to Redis")
```

### 3. Fast Lookup from Redis

```python
from text_processing import StopwordDetector

detector = StopwordDetector(redis_url="redis://localhost:6379")

# Check stopword (< 1ms)
result = detector.is_stopword("the", "en")
print(f"Is stopword: {result.is_stopword}")
print(f"Confidence: {result.confidence:.2f}")
print(f"IDF: {result.idf:.2f}")
```

## With MongoDB (Large Corpus)

### 1. Start MongoDB

```bash
# Using Docker
docker run -d -p 27017:27017 mongo:latest
```

### 2. Batch Processing

```python
from text_processing import IDFAnalyzer, StopwordDetector
from text_processing.corpus_processor import CorpusProcessor

# Connect to MongoDB
processor = CorpusProcessor(
    mongodb_uri="mongodb://localhost:27017",
    database="search-engine",
    collection="documents",
    text_field="content"
)

# Analyze corpus
analyzer = IDFAnalyzer(idf_threshold=2.0)
corpus_iter = processor.iterate_documents(language="en", limit=10000)
idf_scores = analyzer.analyze_batch(corpus_iter, batch_size=1000)

# Export to Redis
detector = StopwordDetector(redis_url="redis://localhost:6379")
detector.export_to_redis(idf_scores, language="en")

print("✅ Batch processing complete!")
processor.close()
```

## Command-Line Tools

### Interactive Testing

```bash
python interactive_test.py
```

Interactive menu:
1. IDF Analysis Test (analyze custom corpus)
2. Stopword Detection Test (check individual terms)
3. Multilingual Test (test multiple languages)
4. Performance Benchmark

### Batch Processing Script

```bash
# Basic usage
python scripts/compute_idf_batch.py \
    --mongodb-uri mongodb://localhost:27017 \
    --database search-engine \
    --collection documents \
    --language en

# With options
python scripts/compute_idf_batch.py \
    --mongodb-uri mongodb://localhost:27017 \
    --redis-url redis://localhost:6379 \
    --language en \
    --batch-size 10000 \
    --idf-threshold 2.0 \
    --clear-existing

# Dry run (analyze without exporting)
python scripts/compute_idf_batch.py \
    --mongodb-uri mongodb://localhost:27017 \
    --database search-engine \
    --collection documents \
    --dry-run
```

## Common Tasks

### Task 1: Analyze Multilingual Corpus

```python
from text_processing import IDFAnalyzer

languages = {
    "en": ["English documents..."],
    "fa": ["Persian documents..."],
    "es": ["Spanish documents..."]
}

analyzer = IDFAnalyzer(idf_threshold=2.0)

for lang, corpus in languages.items():
    idf_scores = analyzer.analyze(corpus, language=lang)
    candidates = analyzer.get_stopword_candidates()
    print(f"{lang}: {len(candidates)} stopwords detected")
```

### Task 2: Batch Check Multiple Terms

```python
from text_processing import is_stopword_batch

terms = ["the", "quantum", "and", "cryptocurrency", "in", "technology"]
results = is_stopword_batch(terms, "en")

for term, is_stop in zip(terms, results):
    print(f"{term}: {'✅ stopword' if is_stop else '❌ not stopword'}")
```

### Task 3: Get Statistics

```python
from text_processing import StopwordDetector

detector = StopwordDetector(redis_url="redis://localhost:6379")

# Get statistics
stats = detector.get_statistics("en")
print(f"English stopwords in Redis: {stats['stopword_count']}")

# Clear language
deleted = detector.clear_language("en")
print(f"Deleted {deleted} stopwords")
```

## Running Tests

```bash
# Quick test
pytest tests/test_idf_calculation.py -v

# All tests
pytest tests/ -v

# With coverage
pytest tests/ --cov=text_processing --cov-report=html

# Performance benchmarks
python benchmarks/stopword_perf.py
```

## Troubleshooting

### Issue: ModuleNotFoundError

```bash
# Make sure you're in the correct directory
cd /root/search-engine-core/modules/M0-foundation/01-text-processing/01.4-stopword-idf-analysis

# Install dependencies
pip install -r requirements.txt
```

### Issue: Redis Connection Failed

```python
# Use bootstrap-only mode
detector = StopwordDetector(
    redis_url="redis://localhost:9999",  # Invalid URL
    use_bootstrap=True  # Falls back to bootstrap lists
)
```

### Issue: MongoDB Timeout

```python
# Increase timeout
processor = CorpusProcessor(
    mongodb_uri="mongodb://localhost:27017?serverSelectionTimeoutMS=10000"
)
```

## Next Steps

- 📖 Read [README.md](README.md) for comprehensive documentation
- 🧮 Read [docs/ALGORITHMS.md](docs/ALGORITHMS.md) for algorithm details
- ✅ Check [PROJECT_STATUS.txt](PROJECT_STATUS.txt) for completion status
- 🔗 Integrate with Tasks 01.1, 01.2, 01.3 for full text processing pipeline

## Common Patterns

### Pattern 1: Offline Analysis

```python
# Analyze corpus once, export to JSON
analyzer = IDFAnalyzer()
idf_scores = analyzer.analyze(corpus)

import json
with open('stopwords_en.json', 'w') as f:
    json.dump({
        term: {'idf': score.idf, 'confidence': score.confidence}
        for term, score in idf_scores.items()
        if score.is_stopword_candidate
    }, f)
```

### Pattern 2: Production Pipeline

```python
# 1. Analyze corpus from MongoDB
# 2. Export to Redis
# 3. Use for real-time stopword filtering

# Step 1 & 2: Batch job (run nightly)
python scripts/compute_idf_batch.py --language en

# Step 3: Application code
from text_processing import is_stopword
if not is_stopword(term, "en"):
    # Process non-stopword term
    pass
```

### Pattern 3: Custom Threshold

```python
# Different thresholds for different use cases
strict = IDFAnalyzer(idf_threshold=2.5)  # Fewer stopwords
balanced = IDFAnalyzer(idf_threshold=2.0)  # Default
aggressive = IDFAnalyzer(idf_threshold=1.5)  # More stopwords
```

---

**Ready in 5 minutes! 🚀**

For more details, see [README.md](README.md)

