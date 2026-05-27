# Language Detection Models

This directory contains FastText models for language detection.

## Quick Start

Download pre-trained models:

```bash
cd /root/search-engine-core/modules/M0-foundation/01-text-processing/01.2-language-detection
./scripts/download_models.sh
```

## Available Models

### Pre-trained Models (Facebook AI)

1. **lid.176.bin** (917 KB) - Compressed model
   - 176 languages supported
   - Fast inference
   - Good for production
   
2. **lid.176.ftz** (126 MB) - Full accuracy model ⭐ **Recommended**
   - 176 languages supported
   - Best accuracy (~95%)
   - Slightly slower but more accurate

### Custom Models

Train your own model for 250+ languages:

```bash
python scripts/train_custom_model.py \
    --corpus training_data/my_corpus.json \
    --output models/custom/my_model.bin
```

Custom models will be saved in `models/custom/` directory.

## Model Performance

| Model | Size | Languages | Accuracy | Speed |
|-------|------|-----------|----------|-------|
| lid.176.bin | 917 KB | 176 | ~93% | Very Fast |
| lid.176.ftz | 126 MB | 176 | ~95% | Fast |
| Custom | Varies | 250+ | Depends on training | Fast |

## Supported Languages (176 default)

Includes: English, Spanish, French, German, Russian, Arabic, Persian, Chinese, Japanese, Korean, Hindi, Portuguese, Italian, Turkish, Vietnamese, Thai, Hebrew, Greek, and 158 more languages.

Full list: [FastText Language Codes](https://fasttext.cc/docs/en/language-identification.html)

## Model Format

FastText models use binary format (.bin) or compressed format (.ftz).

## License

Pre-trained models are from Facebook AI Research and are available under Creative Commons Attribution-Share-Alike License 3.0.

## References

- [FastText Language Identification](https://fasttext.cc/docs/en/language-identification.html)
- [FastText Models](https://dl.fbaipublicfiles.com/fasttext/supervised-models/)

