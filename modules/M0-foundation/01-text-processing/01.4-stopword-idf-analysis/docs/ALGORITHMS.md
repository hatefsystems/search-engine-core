# Stopword IDF Analysis - Algorithm Documentation

Comprehensive technical documentation for IDF-based stopword detection algorithms.

## Table of Contents

1. [IDF (Inverse Document Frequency)](#idf-inverse-document-frequency)
2. [Stopword Detection Algorithm](#stopword-detection-algorithm)
3. [Confidence Scoring](#confidence-scoring)
4. [Multi-language Support](#multi-language-support)
5. [Performance Optimizations](#performance-optimizations)
6. [Learning Resources](#learning-resources)

---

## IDF (Inverse Document Frequency)

### Definition

**IDF** measures how rare or common a term is across a document corpus. It's a key component of TF-IDF (Term Frequency-Inverse Document Frequency) and is fundamental to information retrieval.

### Mathematical Formula

#### Standard IDF

```
IDF(t) = log(N / df(t))
```

Where:
- `t` = term
- `N` = total number of documents in corpus
- `df(t)` = document frequency (number of documents containing term t)
- `log` = natural logarithm (base e)

#### Smoothed IDF (Used in Implementation)

```
IDF(t) = log((N + 1) / (df(t) + 1)) + 1
```

**Why smoothing?**
- Prevents division by zero
- Ensures non-negative IDF values
- Provides more stable scores for small corpora

### Interpretation

| IDF Value | Interpretation | Example Terms |
|-----------|----------------|---------------|
| 0.0 - 1.0 | Very common (stopword) | "the", "a", "and" |
| 1.0 - 2.0 | Common (borderline) | "would", "could" |
| 2.0 - 4.0 | Moderately rare | "technology", "research" |
| 4.0+ | Very rare (content word) | "quantum", "cryptocurrency" |

### Example Calculation

**Corpus:**
```python
corpus = [
    "the cat sat on the mat",      # doc 1
    "the dog sat on the log",      # doc 2
    "a cat and a dog"              # doc 3
]
```

**Calculations:**

```
N = 3 documents

"the": appears in docs 1, 2 → df("the") = 2
IDF("the") = log((3 + 1) / (2 + 1)) + 1
           = log(4/3) + 1
           = 0.2877 + 1
           = 1.2877

"cat": appears in docs 1, 3 → df("cat") = 2
IDF("cat") = log((3 + 1) / (2 + 1)) + 1
           = 1.2877

"quantum": appears in 0 docs → df("quantum") = 0
IDF("quantum") = log((3 + 1) / (0 + 1)) + 1
                = log(4) + 1
                = 1.3863 + 1
                = 2.3863
```

### Code Implementation

```python
import math

def calculate_idf(df: int, N: int, smoothing: bool = True) -> float:
    """
    Calculate IDF score for a term
    
    Args:
        df: Document frequency (number of docs containing term)
        N: Total number of documents
        smoothing: Use smoothed IDF calculation
        
    Returns:
        IDF score (float)
    """
    if df == 0:
        return 0.0
    
    if smoothing:
        # Smoothed IDF: prevents zero division and negative values
        idf = math.log((N + 1) / (df + 1)) + 1
    else:
        # Standard IDF
        idf = math.log(N / df)
    
    return max(0.0, idf)  # Ensure non-negative
```

---

## Stopword Detection Algorithm

### Core Principle

**Observation:** Stopwords appear in most documents and have **LOW IDF scores**.

### Algorithm Steps

1. **Count Document Frequencies**
   ```
   For each document in corpus:
       For each unique term in document:
           df[term] += 1
   ```

2. **Calculate IDF Scores**
   ```
   For each term in vocabulary:
       idf[term] = calculate_idf(df[term], N)
   ```

3. **Identify Stopword Candidates**
   ```
   For each term with idf[term] < threshold:
       confidence = calculate_confidence(idf[term], df[term])
       if confidence >= min_confidence:
           stopwords.add(term)
   ```

### Threshold Selection

The IDF threshold determines the boundary between stopwords and content words:

```python
idf_threshold = 2.0  # Default

if idf < idf_threshold:
    # Likely stopword
    is_stopword = True
else:
    # Content word
    is_stopword = False
```

**Threshold Guidelines:**

| Threshold | Description | Use Case |
|-----------|-------------|----------|
| 1.5 | Aggressive | Maximizing recall, removing more stopwords |
| 2.0 | **Balanced (default)** | General purpose, good precision/recall |
| 2.5 | Conservative | Maximizing precision, keeping borderline words |

### Pseudocode

```
ALGORITHM: StopwordDetection

INPUT: 
    corpus: List of document strings
    threshold: IDF threshold for stopword detection
    min_df: Minimum document frequency filter
    max_df_ratio: Maximum document frequency ratio
    
OUTPUT:
    stopwords: Dictionary mapping terms to confidence scores

STEPS:
1. Initialize:
     term_df = {}  # Document frequency counter
     N = len(corpus)
     
2. Count document frequencies:
     FOR EACH document IN corpus:
         unique_terms = set(document.split())
         FOR EACH term IN unique_terms:
             term_df[term] += 1
             
3. Calculate IDF and detect stopwords:
     stopwords = {}
     FOR EACH term, df IN term_df.items():
         # Apply filters
         IF df < min_df:
             CONTINUE  # Ignore rare terms
             
         df_ratio = df / N
         IF df_ratio > max_df_ratio:
             idf = 0.0  # Very common terms
         ELSE:
             idf = log((N + 1) / (df + 1)) + 1
             
         # Detect stopword
         IF idf < threshold:
             confidence = calculate_confidence(idf, df_ratio)
             stopwords[term] = {
                 'idf': idf,
                 'df': df,
                 'confidence': confidence
             }
             
4. RETURN stopwords
```

---

## Confidence Scoring

### Purpose

Not all stopwords are equally "stopword-like". Confidence scoring quantifies how certain we are that a term is a stopword.

### Algorithm

```python
def calculate_confidence(idf: float, df_ratio: float, threshold: float = 2.0) -> float:
    """
    Calculate confidence score for stopword detection
    
    Args:
        idf: IDF score
        df_ratio: Document frequency ratio (0.0-1.0)
        threshold: IDF threshold
        
    Returns:
        Confidence score (0.0-1.0)
    """
    if idf >= threshold:
        return 0.0  # Not a stopword
    
    # Base confidence from IDF (lower IDF = higher confidence)
    idf_confidence = 1.0 - (idf / threshold)
    
    # Boost confidence for very common terms (appearing in >70% docs)
    if df_ratio > 0.7:
        frequency_boost = (df_ratio - 0.7) / 0.3  # Normalize to 0-1
        idf_confidence = min(1.0, idf_confidence + frequency_boost * 0.2)
    
    return max(0.0, min(1.0, idf_confidence))
```

### Confidence Ranges

| Confidence | Interpretation | Example |
|------------|----------------|---------|
| 0.95 - 1.0 | Very high confidence stopword | "the", "a", "and" (appears in 90%+ docs) |
| 0.8 - 0.95 | High confidence stopword | "in", "on", "at" |
| 0.6 - 0.8 | Medium confidence stopword | "would", "could" |
| 0.4 - 0.6 | Low confidence stopword | Domain-specific common terms |
| 0.0 - 0.4 | Not a stopword | Content words |

### Example

```python
# Term "the": IDF = 0.5, df_ratio = 0.95
confidence = 1.0 - (0.5 / 2.0) = 0.75 (base)
frequency_boost = (0.95 - 0.7) / 0.3 = 0.833
confidence = min(1.0, 0.75 + 0.833 * 0.2) = 0.917

# Result: High confidence stopword (0.917)
```

---

## Multi-language Support

### Language-Agnostic Algorithm

The IDF algorithm is inherently **language-agnostic** because it's based on statistical frequency, not linguistic rules.

### Why It Works Universally

1. **Statistical Foundation:** All languages have high-frequency function words
2. **No Rules Required:** No need for grammar rules or dictionaries
3. **Corpus-Based:** Learns from actual language usage

### Language-Specific Considerations

#### Tokenization

Different languages require different tokenization strategies:

```python
# English, Spanish, French: whitespace tokenization
tokens = text.split()

# Chinese, Japanese: character-level or segmentation required
import jieba
tokens = jieba.cut(text)  # For Chinese

# Arabic, Persian: handle right-to-left text
tokens = arabic_tokenizer(text)
```

#### Threshold Tuning

Some languages may benefit from adjusted thresholds:

```python
thresholds = {
    'en': 2.0,    # English: standard
    'zh': 1.5,    # Chinese: more aggressive (characters, not words)
    'ar': 2.2,    # Arabic: slightly conservative
    'de': 2.0,    # German: standard
}

threshold = thresholds.get(language, 2.0)  # Default to 2.0
```

### Cross-Language Examples

#### English Stopwords
```python
corpus_en = [
    "the quick brown fox",
    "the lazy dog sleeps",
    "a cat and a dog"
]
# Detected: "the" (IDF: 0.41), "a" (IDF: 1.29)
```

#### Persian Stopwords
```python
corpus_fa = [
    "این یک متن فارسی است",
    "این متن برای تست است",
    "یک متن دیگر"
]
# Detected: "این" (this), "است" (is), "یک" (one)
```

#### Spanish Stopwords
```python
corpus_es = [
    "el rápido zorro marrón",
    "el perro perezoso duerme",
    "un gato y un perro"
]
# Detected: "el" (the), "un" (a)
```

---

## Performance Optimizations

### 1. Sparse Matrix Representation

For large vocabularies, use sparse matrices to save memory:

```python
from scipy.sparse import lil_matrix

# Instead of dense matrix
# term_doc_matrix = [[0] * num_docs for _ in range(vocab_size)]

# Use sparse matrix
term_doc_matrix = lil_matrix((vocab_size, num_docs), dtype=int)
```

**Memory Savings:**
- Dense matrix: `vocab_size * num_docs * 8 bytes`
- Sparse matrix: `num_nonzero * 16 bytes` (typically 1% of dense)

### 2. Batch Processing

Process large corpora in batches to avoid memory overflow:

```python
def analyze_batch(corpus_iterator, batch_size=10000):
    term_df = {}
    total_docs = 0
    batch = []
    
    for doc in corpus_iterator:
        batch.append(doc)
        
        if len(batch) >= batch_size:
            process_batch(batch, term_df)
            total_docs += len(batch)
            batch = []
    
    # Process remaining
    if batch:
        process_batch(batch, term_df)
        total_docs += len(batch)
    
    return calculate_idf_scores(term_df, total_docs)
```

### 3. Redis Caching

Cache IDF scores in Redis for fast lookup:

```python
# Redis key format: stopword:{lang}:{term}
key = f"stopword:en:the"

# Store as hash
redis.hset(key, mapping={
    'confidence': '0.95',
    'df': '95000',
    'idf': '0.82'
})

# Fast lookup: O(1)
data = redis.hgetall(key)  # <1ms
```

### 4. Incremental Updates

Update stopwords incrementally without re-analyzing entire corpus:

```python
def incremental_update(new_docs, existing_idf_scores):
    # Add new document frequencies
    for doc in new_docs:
        for term in doc.split():
            existing_idf_scores[term].df += 1
    
    # Recalculate IDF for affected terms
    N_new = N_old + len(new_docs)
    for term in affected_terms:
        idf_scores[term].idf = calculate_idf(
            idf_scores[term].df,
            N_new
        )
```

### Complexity Analysis

| Operation | Time Complexity | Space Complexity |
|-----------|----------------|------------------|
| Document frequency counting | O(N * M) | O(V) |
| IDF calculation | O(V) | O(V) |
| Stopword detection | O(V) | O(S) |
| Redis lookup | O(1) | O(1) |

Where:
- N = number of documents
- M = average document length
- V = vocabulary size
- S = number of stopwords

---

## Learning Resources

### Official Documentation & Specifications

- [TF-IDF - Wikipedia](https://en.wikipedia.org/wiki/Tf%E2%80%93idf) - Comprehensive overview of TF-IDF
- [Information Retrieval - Stanford NLP](https://nlp.stanford.edu/IR-book/) - Classic IR textbook
- [Redis Documentation](https://redis.io/docs/) - Redis data structures and commands

### Research Papers & Academic Resources

- Manning, C. D., Raghavan, P., & Schütze, H. (2008). *Introduction to Information Retrieval*. Cambridge University Press.
  - Chapter 6: Scoring, term weighting, and the vector space model
  - Available online: https://nlp.stanford.edu/IR-book/

- Salton, G., & Buckley, C. (1988). "Term-weighting approaches in automatic text retrieval". *Information Processing & Management*, 24(5), 513-523.
  - Original TF-IDF paper

- Robertson, S. (2004). "Understanding inverse document frequency: On theoretical arguments for IDF". *Journal of Documentation*, 60(5), 503-520.
  - Theoretical foundation of IDF

### Tutorials & Hands-on Guides

- [Scikit-learn TF-IDF Tutorial](https://scikit-learn.org/stable/modules/feature_extraction.html#tfidf-term-weighting)
  - Practical TF-IDF implementation in Python

- [NLTK Stopwords](https://www.nltk.org/howto/corpus.html#word-lists-and-lexicons)
  - Standard stopword lists for bootstrapping

- [Redis Python Tutorial](https://redis-py.readthedocs.io/)
  - Redis integration patterns

### Implementation Examples & Libraries

- **Scikit-learn:**
  ```python
  from sklearn.feature_extraction.text import TfidfVectorizer
  vectorizer = TfidfVectorizer()
  tfidf_matrix = vectorizer.fit_transform(corpus)
  ```

- **Gensim:**
  ```python
  from gensim.models import TfidfModel
  from gensim.corpora import Dictionary
  dictionary = Dictionary(corpus)
  tfidf = TfidfModel(dictionary=dictionary)
  ```

- **NLTK:**
  ```python
  from nltk.corpus import stopwords
  stop_words = set(stopwords.words('english'))
  ```

### Related Concepts & Advanced Topics

- **TF-IDF (Term Frequency-Inverse Document Frequency)**
  - Combines term frequency with IDF for document ranking
  - Used in search engines and text mining

- **BM25 (Best Matching 25)**
  - Probabilistic ranking function based on TF-IDF
  - Industry-standard for search relevance

- **Word Embeddings**
  - Modern alternative to TF-IDF for semantic similarity
  - Word2Vec, GloVe, BERT

- **Zipf's Law**
  - Explains word frequency distribution
  - Foundation for stopword detection

### Multilingual NLP Resources

- [Universal Dependencies](https://universaldependencies.org/)
  - Cross-lingual linguistic resources

- [Stopwords ISO](https://github.com/stopwords-iso/stopwords-iso)
  - Stopword lists for 50+ languages

- [Polyglot](https://polyglot.readthedocs.io/)
  - Multilingual NLP library

---

## References

1. Salton, G., & McGill, M. J. (1983). *Introduction to Modern Information Retrieval*. McGraw-Hill.

2. Robertson, S. E., & Jones, K. S. (1976). "Relevance weighting of search terms". *Journal of the American Society for Information Science*, 27(3), 129-146.

3. Luhn, H. P. (1958). "The Automatic Creation of Literature Abstracts". *IBM Journal of Research and Development*, 2(2), 159-165.

4. Zipf, G. K. (1949). *Human Behavior and the Principle of Least Effort*. Addison-Wesley.

---

**Document Version:** 1.0  
**Last Updated:** 2025-01-18  
**Task:** 01.4 - Stopword IDF Analysis

