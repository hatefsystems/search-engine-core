# Training Guide: Custom Language Detection Models

Complete guide for training custom FastText models to extend language detection beyond 176 languages to 250+ languages.

## 📋 Table of Contents

1. [Overview](#overview)
2. [Requirements](#requirements)
3. [Data Preparation](#data-preparation)
4. [Training Process](#training-process)
5. [Evaluation](#evaluation)
6. [Deployment](#deployment)
7. [Best Practices](#best-practices)
8. [Troubleshooting](#troubleshooting)

## Overview

### Why Custom Training?

- **Extend Beyond 176 Languages:** Add rare or regional languages
- **Domain-Specific Detection:** Train on your specific text types
- **Improved Accuracy:** Fine-tune for your use case
- **Control:** Full control over supported languages

### What You Need

- Training corpus: 100+ samples per language
- Computing resources: CPU sufficient (GPU not required)
- Time: 10-30 minutes for 250 languages
- Storage: 50-500MB for final model

## Requirements

### Software

```bash
# Python 3.9+
python3 --version

# Required packages
pip install fasttext-wheel structlog pydantic

# Optional: progress monitoring
pip install tqdm
```

### Hardware

| Languages | RAM | Disk Space | Training Time |
|-----------|-----|------------|---------------|
| 10 | 2 GB | 10 MB | 1 min |
| 50 | 4 GB | 50 MB | 5 min |
| 100 | 8 GB | 100 MB | 10 min |
| 250 | 16 GB | 250 MB | 30 min |

## Data Preparation

### Step 1: Collect Training Data

You need text samples in each language you want to detect.

**Minimum Requirements:**
- **100 samples per language** (minimum)
- **1,000 samples per language** (recommended)
- **10,000 samples per language** (optimal)

**Data Sources:**

```python
# Example sources
sources = {
    "Wikipedia": "https://dumps.wikimedia.org/",
    "Tatoeba": "https://tatoeba.org/",
    "Common Crawl": "https://commoncrawl.org/",
    "Leipzig Corpora": "https://wortschatz.uni-leipzig.de/",
    "OPUS": "https://opus.nlpl.eu/"
}
```

### Step 2: Format Corpus

Create a JSON file mapping language codes to text samples:

```json
{
    "en": [
        "The quick brown fox jumps over the lazy dog.",
        "This is another English sentence.",
        "More English text here..."
    ],
    "fa": [
        "روباه قهوه‌ای سریع از روی سگ تنبل می‌پرد.",
        "این یک جمله فارسی دیگر است.",
        "متن بیشتر فارسی اینجا..."
    ],
    "custom_lang": [
        "Your custom language text",
        "Another sample",
        "More samples..."
    ]
}
```

**Save as:** `training_data/corpus.json`

### Step 3: Validate Corpus

```python
from text_processing.model_trainer import ModelTrainer

trainer = ModelTrainer()
stats = trainer.validate_corpus(corpus)

print(f"Languages: {stats['num_languages']}")
print(f"Total samples: {stats['total_samples']}")
print(f"Min samples: {stats['min_samples']}")
print(f"Max samples: {stats['max_samples']}")

if stats.get('imbalanced'):
    print(f"⚠️  {stats['warning']}")
```

## Training Process

### Method 1: Using Training Script (Recommended)

```bash
python scripts/train_custom_model.py \
    --corpus training_data/corpus.json \
    --output models/custom/my_model.bin \
    --epoch 25 \
    --lr 0.1 \
    --min-samples 100 \
    --max-samples 10000
```

**Parameters:**
- `--corpus`: Path to corpus JSON file
- `--output`: Output model path
- `--epoch`: Number of training epochs (default: 25)
- `--lr`: Learning rate (default: 0.1)
- `--min-samples`: Minimum samples per language (default: 100)
- `--max-samples`: Maximum samples for balancing (default: 10000)

### Method 2: Python API

```python
from text_processing.model_trainer import ModelTrainer

# Load corpus
with open('training_data/corpus.json') as f:
    corpus = json.load(f)

# Train model
trainer = ModelTrainer()
model = trainer.prepare_and_train(
    corpus=corpus,
    output_model=Path('models/custom/my_model.bin'),
    epoch=25,
    lr=0.1,
    dim=128
)

print(f"Model trained with {len(model.get_labels())} languages")
```

### Training Parameters

| Parameter | Default | Description | Tuning Tips |
|-----------|---------|-------------|-------------|
| `epoch` | 25 | Training epochs | Increase for better accuracy |
| `lr` | 0.1 | Learning rate | Decrease if overfitting |
| `dim` | 128 | Vector dimension | Increase for complex languages |
| `word_ngrams` | 2 | Word n-grams | 2-3 works best |
| `min_count` | 1 | Min word frequency | Keep at 1 for rare languages |

### Training Output

```
🎯 Custom Language Detection Model Training
============================================================
📊 Languages: 250
📝 Total samples: 250,000
📉 Min samples per language: 100
📈 Max samples per language: 10,000
📁 Output model: models/custom/my_model.bin
============================================================

⚙️  Training in progress...

Read 0M words
Number of words:  150234
Number of labels: 250
Progress: 100.0% words/sec: 234512 lr: 0.000000 avg.loss: 1.234567

============================================================
✅ Training Complete!
============================================================
📦 Model saved to: models/custom/my_model.bin
🌍 Languages supported: 250
💾 Model size: 89.3 MB
============================================================

🎉 Your custom model is ready for 250+ language detection!
```

## Evaluation

### Evaluate Model Accuracy

```python
from text_processing.model_trainer import ModelTrainer

trainer = ModelTrainer()
metrics = trainer.evaluate_model(
    model_path=Path('models/custom/my_model.bin'),
    test_file=Path('training_data/test_corpus.txt')
)

print(f"Accuracy: {metrics['precision']:.2%}")
print(f"F1 Score: {metrics['f1_score']:.2%}")
```

### Test Custom Model

```python
from text_processing import UniversalLanguageDetector

# Load custom model
detector = UniversalLanguageDetector(
    model_path='models/custom/my_model.bin'
)

# Test detection
result = detector.detect("Your test text")
print(f"Language: {result.language_code}")
print(f"Confidence: {result.confidence:.2%}")
```

### Benchmark Performance

```bash
python benchmarks/detector_perf.py
```

## Deployment

### Using Custom Model

```python
from text_processing import UniversalLanguageDetector

detector = UniversalLanguageDetector(
    model_path="models/custom/my_model.bin"
)

result = detector.detect("Text in any of your 250 languages")
```

### Model Versioning

```
models/
├── custom/
│   ├── v1_100lang_model.bin
│   ├── v2_200lang_model.bin
│   └── v3_250lang_model.bin  # Latest
```

### Model Deployment Checklist

- [ ] Train model on full corpus
- [ ] Evaluate accuracy (≥90% target)
- [ ] Benchmark performance
- [ ] Version model file
- [ ] Document supported languages
- [ ] Update production configuration
- [ ] Monitor accuracy in production

## Best Practices

### Data Quality

1. **Diverse Samples:** Mix of topics, styles, lengths
2. **Clean Data:** Remove URLs, HTML, excessive punctuation
3. **Balanced Dataset:** Similar samples per language
4. **Representative:** Match your production data distribution

### Training Tips

```python
# Good practices
trainer = ModelTrainer(seed=42)  # Reproducibility

# Prepare with validation split
train_file, val_file = trainer.prepare_training_data(
    corpus=corpus,
    train_split=0.8  # 80% train, 20% validation
)

# Train with validation
model = trainer.train_model(
    training_file=train_file,
    validation_file=val_file,  # Monitor accuracy
    epoch=25,
    verbose=2  # Show progress
)
```

### Sample Size Guidelines

| Use Case | Samples/Lang | Expected Accuracy |
|----------|--------------|-------------------|
| Development/Testing | 100 | 80-85% |
| Production (Basic) | 500 | 88-92% |
| Production (Standard) | 1,000 | 92-95% |
| Production (High Quality) | 10,000 | 95-98% |

### Text Length

- **Optimal:** 20-200 characters per sample
- **Too short:** <10 characters (less reliable)
- **Too long:** >500 characters (unnecessary)

## Troubleshooting

### Low Accuracy

**Problem:** Model accuracy <90%

**Solutions:**
1. Add more training samples per language
2. Increase training epochs
3. Balance dataset
4. Clean training data
5. Use validation split to monitor

### Imbalanced Dataset

**Problem:** Some languages have many samples, others have few

**Solution:**
```python
trainer.prepare_training_data(
    corpus=corpus,
    min_samples=100,      # Drop languages with <100 samples
    max_samples=10000     # Cap at 10,000 per language
)
```

### Memory Issues

**Problem:** Out of memory during training

**Solutions:**
1. Reduce `dim` parameter (e.g., 64 instead of 128)
2. Limit `max_samples` per language
3. Train in batches (split corpus)
4. Use more RAM

### Slow Training

**Problem:** Training takes too long

**Solutions:**
1. Reduce `epoch` (e.g., 15 instead of 25)
2. Use fewer samples (but maintain min 100/lang)
3. Reduce `dim` parameter
4. Use faster hardware

## Advanced Topics

### Fine-Tuning Pre-trained Model

```python
# Load existing model
base_model = fasttext.load_model('models/lid.176.bin')

# Continue training with new languages
# (Requires FastText advanced API)
```

### Incremental Training

Add new languages without retraining from scratch:

```python
# Step 1: Load existing corpus + new languages
full_corpus = {**existing_corpus, **new_languages}

# Step 2: Retrain
model = trainer.prepare_and_train(
    corpus=full_corpus,
    output_model='models/custom/extended_model.bin'
)
```

### Multi-Model Ensemble

For highest accuracy, combine multiple models:

```python
class EnsembleDetector:
    def __init__(self):
        self.model1 = UniversalLanguageDetector(model_path='model1.bin')
        self.model2 = UniversalLanguageDetector(model_path='model2.bin')
    
    def detect(self, text):
        result1 = self.model1.detect(text)
        result2 = self.model2.detect(text)
        
        # Vote or average
        return merge_results(result1, result2)
```

## Example: Training 250 Languages

Complete example training a model with 250 languages:

```python
#!/usr/bin/env python3
import json
from pathlib import Path
from text_processing.model_trainer import ModelTrainer

# 1. Load corpus (250 languages)
with open('training_data/250lang_corpus.json') as f:
    corpus = json.load(f)

# 2. Validate
trainer = ModelTrainer(seed=42)
stats = trainer.validate_corpus(corpus)
print(f"Training {stats['num_languages']} languages")
print(f"Total samples: {stats['total_samples']:,}")

# 3. Train model
model = trainer.prepare_and_train(
    corpus=corpus,
    output_model=Path('models/custom/250lang_model.bin'),
    training_data_path=Path('training_data/250lang_prepared.txt'),
    dim=128,
    epoch=25,
    lr=0.1,
    min_count=1,
    word_ngrams=2
)

# 4. Test
from text_processing import UniversalLanguageDetector
detector = UniversalLanguageDetector(model_path='models/custom/250lang_model.bin')

test_cases = {
    'en': 'Hello world',
    'fa': 'سلام دنیا',
    # ... add your languages
}

for lang, text in test_cases.items():
    result = detector.detect(text)
    correct = "✅" if result.language_code == lang else "❌"
    print(f"{correct} {lang}: {result.language_code} ({result.confidence:.2%})")

print(f"\n🎉 Model ready with {model.get_num_languages()} languages!")
```

## Resources

- **FastText Documentation:** https://fasttext.cc/
- **Language Codes (ISO 639):** https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes
- **Training Data Sources:** See [training_data/README.md](../training_data/README.md)

---

**Ready to train your custom model with 250+ languages!** 🚀

