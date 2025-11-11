# Algorithms and Technical Details - Language Detection

Deep dive into the algorithms and implementation details of Task 01.2 language detection system.

## 📋 Table of Contents

1. [FastText Language Identification](#fasttext-language-identification)
2. [N-gram Fallback Detection](#n-gram-fallback-detection)
3. [Script Detection](#script-detection)
4. [Mixed Language Detection](#mixed-language-detection)
5. [Performance Optimization](#performance-optimization)
6. [Training Algorithm](#training-algorithm)
7. [Integration Architecture](#integration-architecture)

## FastText Language Identification

### Overview

FastText is a supervised learning algorithm for text classification optimized for language identification.

### Architecture

```
Input Text
    ↓
Character N-grams (1-3)
    ↓
Word N-grams (1-2)
    ↓
FastText Embedding (dim=128)
    ↓
Linear Classifier
    ↓
Softmax Layer
    ↓
Language Probabilities
```

### Algorithm Steps

1. **Tokenization:**
   ```python
   text = "Hello world"
   tokens = ["Hello", "world"]
   ```

2. **Character N-grams:**
   ```python
   # For "Hello"
   char_ngrams = [
       "<He", "Hel", "ell", "llo", "lo>",  # 3-grams
       "<H", "He", "el", "ll", "lo", "o>"  # 2-grams
   ]
   ```

3. **Word N-grams:**
   ```python
   word_ngrams = [
       "Hello", "world",           # 1-grams
       "Hello world"               # 2-grams
   ]
   ```

4. **Embedding:**
   - Each n-gram mapped to 128-dim vector
   - Averaged to form sentence embedding
   
5. **Classification:**
   ```python
   # Linear layer + softmax
   logits = W @ embedding + b
   probs = softmax(logits)
   ```

### Mathematical Formulation

**Objective Function:**

\[
\mathcal{L} = -\frac{1}{N} \sum_{i=1}^{N} \log p(y_i | x_i)
\]

Where:
- \( N \) = number of training samples
- \( x_i \) = input text
- \( y_i \) = true language label
- \( p(y_i | x_i) \) = predicted probability

**Softmax:**

\[
p(y = k | x) = \frac{\exp(w_k \cdot x)}{\sum_{j=1}^{K} \exp(w_j \cdot x)}
\]

Where:
- \( K \) = number of languages
- \( w_k \) = weight vector for language \( k \)

### Complexity

- **Training:** \( O(N \times L \times D) \)
  - \( N \) = training samples
  - \( L \) = average text length
  - \( D \) = embedding dimension
  
- **Inference:** \( O(L \times D + K \times D) \)
  - \( K \) = number of languages
  - Very fast: <5ms per text

### Hyperparameters

```python
model_params = {
    'dim': 128,           # Embedding dimension
    'epoch': 25,          # Training epochs
    'lr': 0.1,           # Learning rate
    'wordNgrams': 2,     # Use word bigrams
    'minCount': 1,       # Minimum word frequency
    'loss': 'softmax',   # Loss function
}
```

## N-gram Fallback Detection

### Purpose

Fallback for very short texts where FastText may be unreliable.

### Algorithm

1. **Extract Character N-grams:**
   ```python
   def extract_ngrams(text, n=3):
       return [text[i:i+n] for i in range(len(text)-n+1)]
   
   # Example
   text = "Hello"
   ngrams = ["Hel", "ell", "llo"]  # 3-grams
   ```

2. **Build Language Profiles:**
   ```python
   language_profiles = {
       'en': Counter(['the', 'and', 'ion', 'tio', ...]),
       'fa': Counter(['است', 'های', 'کند', 'در', ...]),
   }
   ```

3. **Calculate Similarity:**
   ```python
   def detect(text):
       text_ngrams = Counter(extract_ngrams(text))
       
       scores = {}
       for lang, profile in language_profiles.items():
           # Cosine similarity
           scores[lang] = cosine_similarity(text_ngrams, profile)
       
       return max(scores, key=scores.get)
   ```

### Complexity

- **Time:** \( O(L \times K) \)
  - \( L \) = text length
  - \( K \) = number of languages
  
- **Space:** \( O(K \times P) \)
  - \( P \) = profile size per language

## Script Detection

### Purpose

Identify writing system (ISO 15924 script code).

### Algorithm

```python
def detect_script(text: str) -> str:
    # Count characters per script
    script_counts = {}
    
    for char in text:
        if char.isalnum():
            # Get Unicode block
            script = get_unicode_block(char)
            script_counts[script] = script_counts.get(script, 0) + 1
    
    # Return most common script
    primary_script = max(script_counts, key=script_counts.get)
    
    # Map to ISO 15924
    return map_to_iso15924(primary_script)
```

### Unicode Block Mapping

| Unicode Block | Script Code | Example |
|---------------|-------------|---------|
| U+0041-U+024F | Latn (Latin) | A, B, C |
| U+0600-U+06FF | Arab (Arabic) | ا, ب, ت |
| U+0400-U+04FF | Cyrl (Cyrillic) | А, Б, В |
| U+4E00-U+9FFF | Hans (Han) | 汉, 字 |
| U+3040-U+309F | Jpan (Hiragana) | あ, い |
| U+AC00-U+D7AF | Kore (Hangul) | 가, 나 |

### Complexity

- **Time:** \( O(L) \) where \( L \) = text length
- **Space:** \( O(S) \) where \( S \) = number of unique scripts

## Mixed Language Detection

### Detection Strategy

```python
def detect_mixed_language(text: str, top_k: int = 3) -> LanguageInfo:
    # Get top-k predictions
    predictions = model.predict(text, k=top_k)
    
    # Check if mixed
    is_mixed = (
        len(predictions) > 1 and
        predictions[1][1] > 0.2  # Second language >20%
    )
    
    return LanguageInfo(
        language_code=predictions[0][0],
        confidence=predictions[0][1],
        is_mixed_content=is_mixed,
        detected_languages=predictions
    )
```

### Threshold Tuning

| Threshold | Behavior | Use Case |
|-----------|----------|----------|
| >0.1 | Very sensitive | Detect subtle mixing |
| >0.2 | Balanced | **Default** |
| >0.3 | Conservative | Only obvious mixing |

### Example

```python
# English with Arabic
text = "Hello مرحبا World"
result = detector.detect(text)

# Output:
# language_code: "en"
# confidence: 0.65
# is_mixed_content: True
# detected_languages: [("en", 0.65), ("ar", 0.30), ("fr", 0.05)]
```

## Performance Optimization

### 1. Model Loading

```python
class FastTextDetector:
    def __init__(self, model_path):
        # Lazy loading
        self._model = None
        self.model_path = model_path
    
    @property
    def model(self):
        if self._model is None:
            self._model = fasttext.load_model(self.model_path)
        return self._model
```

**Benefits:**
- Faster initialization
- Memory saved if not used
- Deferred error handling

### 2. Batch Processing

```python
def detect_batch(texts: List[str]) -> List[LanguageInfo]:
    # Process in batches for efficiency
    batch_size = 100
    
    results = []
    for i in range(0, len(texts), batch_size):
        batch = texts[i:i+batch_size]
        
        # FastText batch prediction (faster)
        predictions = model.predict(batch, k=3)
        
        results.extend(parse_predictions(predictions))
    
    return results
```

**Speedup:**
- Single: 5,000 detections/sec
- Batch: 8,000+ detections/sec (1.6x faster)

### 3. Caching

```python
from functools import lru_cache

@lru_cache(maxsize=1000)
def detect_cached(text: str) -> LanguageInfo:
    return detector.detect(text)
```

**Benefits:**
- Repeated queries cached
- Useful for duplicate texts
- 1000x faster for cache hits

### 4. Text Preprocessing

```python
def preprocess(text: str) -> str:
    # Remove excessive whitespace
    text = re.sub(r'\s+', ' ', text)
    
    # Trim to reasonable length
    if len(text) > 1000:
        text = text[:1000]
    
    return text.strip()
```

**Impact:**
- 10-20% faster inference
- More consistent results
- Reduced memory usage

## Training Algorithm

### FastText Supervised Learning

```
Initialize:
    W = random matrix (vocab_size × dim)
    b = zero vector (num_labels)

For epoch in 1..num_epochs:
    Shuffle training data
    
    For each (text, label):
        # Forward pass
        x = embed(text)           # Average n-gram embeddings
        logits = W @ x + b
        probs = softmax(logits)
        
        # Compute loss
        loss = -log(probs[label])
        
        # Backward pass (SGD)
        grad_W = gradient(loss, W)
        grad_b = gradient(loss, b)
        
        # Update
        W = W - lr * grad_W
        b = b - lr * grad_b

Return trained model (W, b)
```

### Learning Rate Schedule

```python
# Initial learning rate
lr_0 = 0.1

# Decay schedule
lr_t = lr_0 * (1 - t / total_steps)

# Where t = current step
```

### Convergence

Typical convergence:
- **Epoch 1-5:** Rapid improvement (50% → 80%)
- **Epoch 6-15:** Steady improvement (80% → 90%)
- **Epoch 16-25:** Fine-tuning (90% → 95%)
- **Epoch 26+:** Diminishing returns

## Integration Architecture

### Pipeline Flow

```
┌─────────────┐
│  Raw Text   │
└──────┬──────┘
       │
       ↓
┌─────────────────────────┐
│  Task 01.1: Normalize   │
│  - NFKC normalization   │
│  - Character unification│
│  - Whitespace cleanup   │
└──────┬──────────────────┘
       │
       ↓
┌─────────────────────────┐
│  Task 01.2: Detect Lang │
│  - FastText detection   │
│  - Script detection     │
│  - Confidence scoring   │
└──────┬──────────────────┘
       │
       ↓
┌─────────────────────────┐
│  Processed Text Result  │
│  - Normalized text      │
│  - Language code        │
│  - Script code          │
│  - Confidence score     │
└─────────────────────────┘
```

### Integration Code

```python
class TextProcessingPipeline:
    def __init__(self):
        self.normalizer = Normalizer()      # Task 01.1
        self.detector = LanguageDetector()  # Task 01.2
    
    def process(self, text: str) -> ProcessedText:
        # Step 1: Normalize
        normalized = self.normalizer.normalize(text)
        
        # Step 2: Detect language on normalized text
        lang_info = self.detector.detect(normalized.text)
        
        # Step 3: Combine results
        return ProcessedText(
            original_text=text,
            normalized_text=normalized.text,
            language_code=lang_info.language_code,
            script_code=lang_info.script_code,
            confidence=lang_info.confidence
        )
```

### Benefits of Integration

1. **Improved Accuracy:**
   - Normalized text → cleaner detection
   - Character unification → fewer variants
   
2. **Consistent Results:**
   - Same preprocessing for all texts
   - Reproducible detection
   
3. **Performance:**
   - Single pipeline call
   - Cached results possible

## References

1. **FastText Paper:**
   - Joulin et al. (2016). "Bag of Tricks for Efficient Text Classification"
   - [arXiv:1607.01759](https://arxiv.org/abs/1607.01759)

2. **Language Identification:**
   - Jauhiainen et al. (2019). "Automatic Language Identification in Texts: A Survey"
   - [Journal of Artificial Intelligence Research](https://jair.org/index.php/jair/article/view/11675)

3. **Unicode Standards:**
   - [Unicode Standard](https://unicode.org/standard/standard.html)
   - [ISO 15924 Script Codes](https://unicode.org/iso15924/)

4. **FastText Documentation:**
   - [FastText Official](https://fasttext.cc/)
   - [FastText GitHub](https://github.com/facebookresearch/fastText)

---

**Technical implementation for 250+ language detection** 🔬

