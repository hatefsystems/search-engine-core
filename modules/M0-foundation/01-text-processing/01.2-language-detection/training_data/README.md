# Training Data

This directory is for custom language detection training data.

## Corpus Format

Training data should be in JSON format mapping language codes to text samples:

```json
{
    "en": [
        "This is an English text sample.",
        "Another English sentence here.",
        "More English content..."
    ],
    "fa": [
        "این یک نمونه متن فارسی است.",
        "جمله دیگری به فارسی.",
        "محتوای بیشتر فارسی..."
    ],
    "custom_lang": [
        "Custom language sample 1",
        "Custom language sample 2"
    ]
}
```

## Requirements

For good model performance:
- **Minimum 100 samples per language**
- **Recommended: 1000+ samples per language**
- **For 250+ languages: ~250,000 total samples**

## Sample Size Guidelines

| Languages | Samples/Lang | Total Samples | Expected Accuracy |
|-----------|--------------|---------------|-------------------|
| 10 | 100 | 1,000 | ~85% |
| 50 | 500 | 25,000 | ~90% |
| 100 | 1,000 | 100,000 | ~93% |
| 250 | 1,000 | 250,000 | ~95% |

## Training

Once you have your corpus:

```bash
python scripts/train_custom_model.py \
    --corpus training_data/my_corpus.json \
    --output models/custom/250lang_model.bin \
    --min-samples 100 \
    --epoch 25
```

## Data Sources

Good sources for multilingual training data:
- [Tatoeba](https://tatoeba.org/) - Multilingual sentence database
- [Wikipedia dumps](https://dumps.wikimedia.org/) - All languages
- [Common Crawl](https://commoncrawl.org/) - Web data
- [OPUS](https://opus.nlpl.eu/) - Parallel corpora
- [Leipzig Corpora](https://wortschatz.uni-leipzig.de/) - Monolingual data

## Data Preparation Tips

1. **Balance your dataset**: Similar number of samples per language
2. **Diverse content**: Mix of topics and styles
3. **Clean data**: Remove URLs, HTML tags, excessive punctuation
4. **Text length**: 20-200 characters per sample (ideal)
5. **Encoding**: UTF-8 encoding required

## Example Corpus

See `sample_corpus.json` for a small example with 5 languages.

