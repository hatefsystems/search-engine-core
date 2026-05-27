# Stopword Data Directory

This directory contains bootstrap stopword lists used as fallback when Redis is not available or for cold-start scenarios.

## Directory Structure

```
data/stopwords/
├── README.md (this file)
└── bootstrap/
    ├── en.txt         # English stopwords
    ├── fa.txt         # Persian/Farsi stopwords
    ├── es.txt         # Spanish stopwords
    ├── fr.txt         # French stopwords
    ├── de.txt         # German stopwords
    └── ...            # More languages
```

## Bootstrap Stopword Lists

Bootstrap lists are used when:
- Redis is not available
- No corpus analysis has been performed yet
- As a fallback for languages without IDF-analyzed stopwords

These lists are sourced from NLTK and other standard stopword collections.

## Adding New Languages

To add bootstrap stopwords for a new language:

1. Create a text file named `{language_code}.txt` in the `bootstrap/` directory
2. Add one stopword per line (lowercase)
3. Use ISO 639-1 language codes (e.g., `en`, `fa`, `es`)

Example (`bootstrap/en.txt`):
```
the
a
an
and
or
but
...
```

## Usage

The `StopwordDetector` automatically loads bootstrap lists on demand:

```python
from text_processing import StopwordDetector

detector = StopwordDetector(use_bootstrap=True)
result = detector.is_stopword("the", "en")
# Falls back to bootstrap list if Redis unavailable
```

## Sources

Bootstrap stopword lists are derived from:
- NLTK (https://www.nltk.org/)
- Stopwords ISO (https://github.com/stopwords-iso)
- Language-specific standard collections

## License

Bootstrap stopword lists are provided for convenience and are sourced from publicly available collections. Original licenses apply.

