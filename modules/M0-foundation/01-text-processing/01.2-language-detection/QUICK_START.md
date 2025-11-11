# Quick Start Guide - Language Detection

Get started with language detection in 5 minutes! 🚀

## ⚡ 5-Minute Setup

### 1. Install Dependencies (1 min)

```bash
cd /root/search-engine-core/modules/M0-foundation/01-text-processing/01.2-language-detection
pip install -r requirements.txt
```

### 2. Download Models (2 min)

```bash
./scripts/download_models.sh
```

This downloads `lid.176.ftz` (126 MB) - supports 176 languages.

### 3. Test It! (2 min)

```bash
python interactive_test.py
```

Type some text and watch it detect the language!

## 🎯 Quick Examples

### Example 1: Detect English

```python
from text_processing import detect_language

result = detect_language("The quick brown fox")
print(result.language_code)  # "en"
print(result.confidence)      # 0.99
```

### Example 2: Detect Persian

```python
result = detect_language("روباه قهوه‌ای سریع")
print(result.language_code)  # "fa"
print(result.script_code)     # "Arab"
```

### Example 3: Batch Detection

```python
from text_processing import detect_language_batch

texts = ["Hello", "سلام", "你好", "Привет"]
results = detect_language_batch(texts)

for text, result in zip(texts, results):
    print(f"{text} → {result.language_code}")
```

## 🔧 Common Tasks

### Check Supported Languages

```python
from text_processing import UniversalLanguageDetector

detector = UniversalLanguageDetector()
num_langs = detector.fasttext_detector.get_num_languages()
print(f"Supports {num_langs} languages")  # 176
```

### Use Custom Model

```python
detector = UniversalLanguageDetector(
    model_path="models/custom/my_model.bin"
)
```

### Integration with Normalization

```python
from text_processing.integration import process_text

result = process_text("messy  text   here")
print(result.normalized_text)  # Clean text
print(result.language_code)    # Detected language
```

## 📊 Run Tests

```bash
# Quick test
pytest tests/test_language_detector.py::TestBasicDetection -v

# Full tests
pytest tests/ -v

# With coverage
pytest tests/ --cov=text_processing --cov-report=html
```

## 🎓 Training Custom Model

```bash
# Prepare your corpus.json
python scripts/train_custom_model.py \
    --corpus training_data/corpus.json \
    --output models/custom/my_model.bin
```

See [TRAINING_GUIDE.md](docs/TRAINING_GUIDE.md) for details.

## 💡 Tips

- **Use lid.176.ftz** for best accuracy
- **Normalize text first** for better detection
- **Check confidence** for reliability
- **Batch process** for performance

## ❓ Troubleshooting

**Model not found?**
```bash
./scripts/download_models.sh
```

**Import error?**
```bash
pip install -r requirements.txt
```

**Low accuracy?**
- Use lid.176.ftz instead of lid.176.bin
- Normalize text first
- Check text length (>10 chars recommended)

## 🎉 You're Ready!

Now you can detect 176 languages with 95%+ accuracy!

For more details, see [README.md](README.md).

Happy detecting! 🌍

