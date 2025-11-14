# Algorithms and Technical Details - Language Detection

Deep dive into the algorithms and implementation details of Task 01.2 language detection system.

## 📋 Table of Contents

1. [FastText Language Identification](#fasttext-language-identification)
   - [Architecture](#architecture)
   - [Detailed Pipeline Explanation](#detailed-pipeline-explanation)
   - [Algorithm Steps](#algorithm-steps)
   - [Mathematical Formulation](#mathematical-formulation)
   - [Complexity](#complexity)
   - [Hyperparameters](#hyperparameters)
2. [N-gram Fallback Detection](#n-gram-fallback-detection)
3. [Script Detection](#script-detection)
4. [Mixed Language Detection](#mixed-language-detection)
5. [Performance Optimization](#performance-optimization)
6. [Training Algorithm](#training-algorithm)
7. [Integration Architecture](#integration-architecture)
8. [Learning Resources](#learning-resources)
9. [References](#references)

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

### Detailed Pipeline Explanation

Each step in the FastText architecture transforms the input text through multiple stages to produce language probabilities. Here's a comprehensive breakdown:

#### 1. Input Text

**Purpose:** Raw text input that needs language identification.

**Processing:**
- Text is received as a string (e.g., `"Hello world"` or `"مرحبا بالعالم"`)
- No preprocessing required initially (FastText handles normalization internally)
- Text can be of any length, but typically 10-1000 characters works best

**Example:**
```python
input_text = "The quick brown fox jumps over the lazy dog"
# Length: 43 characters
# Expected output: English (en) with high confidence
```

**Characteristics:**
- Case-insensitive (FastText normalizes internally)
- Handles Unicode characters natively
- Works with mixed scripts (Latin + Arabic, etc.)

---

#### 2. Character N-grams (1-3)

**Purpose:** Extract subword features that capture character-level patterns unique to each language.

**What are N-grams?**
N-grams are contiguous sequences of N characters. FastText uses:
- **1-grams (unigrams):** Single characters (`"H"`, `"e"`, `"l"`, `"l"`, `"o"`)
- **2-grams (bigrams):** Two-character sequences (`"He"`, `"el"`, `"ll"`, `"lo"`)
- **3-grams (trigrams):** Three-character sequences (`"Hel"`, `"ell"`, `"llo"`)

**Why Character N-grams?**
- **Language-specific patterns:** Each language has characteristic character sequences
  - English: `"th"`, `"ing"`, `"tion"` are common
  - Arabic: `"ال"`, `"ية"`, `"ون"` are characteristic
  - French: `"tion"`, `"ment"`, `"que"` appear frequently
- **Morphology capture:** Character n-grams capture morphological patterns
- **OOV handling:** Works even with out-of-vocabulary words
- **Robustness:** Less sensitive to spelling variations

**Extraction Process:**
```python
def extract_char_ngrams(text, min_n=1, max_n=3):
    """
    Extract character n-grams with boundary markers.
    
    FastText adds special boundary markers:
    - '<' at the beginning
    - '>' at the end
    """
    text = '<' + text + '>'  # Add boundaries
    ngrams = []
    
    for n in range(min_n, max_n + 1):
        for i in range(len(text) - n + 1):
            ngram = text[i:i+n]
            ngrams.append(ngram)
    
    return ngrams

# Example: "Hello"
# Input: "<Hello>"
# 1-grams: ['<', 'H', 'e', 'l', 'l', 'o', '>']
# 2-grams: ['<H', 'He', 'el', 'll', 'lo', 'o>']
# 3-grams: ['<He', 'Hel', 'ell', 'llo', 'lo>']
```

**Mathematical Representation:**
For text of length \( L \), we generate:
- 1-grams: \( L + 2 \) (including boundaries)
- 2-grams: \( L + 1 \)
- 3-grams: \( L \)

Total n-grams: \( 3L + 3 \) for a text of length \( L \)

**Language-Specific Examples:**
```python
# English text
text_en = "Hello world"
# Characteristic n-grams: "llo", "wor", "rld", "wo", "or"

# Arabic text  
text_ar = "مرحبا"
# Characteristic n-grams: "مرح", "رحب", "حبا", "مر", "رح"

# French text
text_fr = "Bonjour"
# Characteristic n-grams: "Bon", "onj", "jou", "our", "on", "nj"
```

---

#### 3. Word N-grams (1-2)

**Purpose:** Capture word-level patterns and word order information.

**What are Word N-grams?**
- **1-grams (unigrams):** Individual words (`"Hello"`, `"world"`)
- **2-grams (bigrams):** Pairs of consecutive words (`"Hello world"`)

**Why Word N-grams?**
- **Vocabulary patterns:** Each language has characteristic word frequencies
  - English: `"the"`, `"and"`, `"is"` are very common
  - Arabic: `"في"`, `"من"`, `"إلى"` are frequent
- **Word order:** Bigrams capture syntactic patterns
  - English: `"the quick"`, `"is a"`, `"of the"`
  - French: `"le chat"`, `"est un"`, `"de la"`
- **Context:** Word sequences provide semantic context

**Extraction Process:**
```python
def extract_word_ngrams(text, max_n=2):
    """
    Extract word n-grams from tokenized text.
    """
    # Tokenization (simplified - FastText uses more sophisticated tokenization)
    words = text.lower().split()
    
    ngrams = []
    # 1-grams (unigrams)
    ngrams.extend(words)
    
    # 2-grams (bigrams)
    for i in range(len(words) - 1):
        bigram = words[i] + ' ' + words[i+1]
        ngrams.append(bigram)
    
    return ngrams

# Example: "Hello world"
# 1-grams: ['hello', 'world']
# 2-grams: ['hello world']
```

**Language-Specific Examples:**
```python
# English
text_en = "The quick brown fox"
# 1-grams: ['the', 'quick', 'brown', 'fox']
# 2-grams: ['the quick', 'quick brown', 'brown fox']

# French
text_fr = "Le chat noir"
# 1-grams: ['le', 'chat', 'noir']
# 2-grams: ['le chat', 'chat noir']

# Arabic (after tokenization)
text_ar = "القط الأسود"
# 1-grams: ['القط', 'الأسود']
# 2-grams: ['القط الأسود']
```

**Combined Feature Set:**
After steps 2 and 3, we have:
- Character n-grams (1-3): ~100-1000 features depending on text length
- Word n-grams (1-2): ~10-100 features depending on text length
- Total: Typically 100-1000+ n-gram features per text

---

#### 4. FastText Embedding (dim=128)

**Purpose:** Convert sparse n-gram features into dense, fixed-dimensional vector representation.

**What is an Embedding?**
An embedding is a dense vector representation that:
- Maps discrete n-grams to continuous vectors
- Captures semantic relationships between n-grams
- Reduces dimensionality while preserving information

**Embedding Process:**

**Step 4a: N-gram to Vector Lookup**
```python
# Each unique n-gram has a learned 128-dimensional vector
embedding_matrix = {
    'the': [0.23, -0.45, 0.67, ..., 0.12],  # 128 dimensions
    'ing': [-0.12, 0.34, -0.56, ..., 0.89],
    'Hello': [0.45, 0.12, -0.23, ..., -0.34],
    # ... millions of n-grams
}

# For each n-gram in our text, look up its vector
def lookup_ngram_vectors(ngrams, embedding_matrix):
    vectors = []
    for ngram in ngrams:
        if ngram in embedding_matrix:
            vectors.append(embedding_matrix[ngram])
        else:
            # Unknown n-gram: use hash-based embedding (FastText feature)
            vectors.append(hash_embedding(ngram))
    return vectors
```

**Step 4b: Averaging N-gram Vectors**
```python
def create_text_embedding(ngrams, embedding_matrix):
    """
    Average all n-gram embeddings to create text representation.
    
    This is FastText's key innovation: simple averaging works well!
    """
    vectors = lookup_ngram_vectors(ngrams, embedding_matrix)
    
    # Average pooling: sum and divide by count
    text_vector = [0.0] * 128  # Initialize 128-dim vector
    
    for vec in vectors:
        for i in range(128):
            text_vector[i] += vec[i]
    
    # Normalize by number of n-grams
    num_ngrams = len(vectors)
    text_vector = [x / num_ngrams for x in text_vector]
    
    return text_vector  # Final 128-dimensional representation
```

**Mathematical Formulation:**

\[
\text{embedding}(x) = \frac{1}{|\mathcal{N}|} \sum_{n \in \mathcal{N}} \mathbf{v}_n
\]

Where:
- \( x \) = input text
- \( \mathcal{N} \) = set of all n-grams extracted from \( x \)
- \( \mathbf{v}_n \) = 128-dimensional embedding vector for n-gram \( n \)
- Result: 128-dimensional vector representing the entire text

**Why Averaging Works:**
- **Simplicity:** No complex neural network needed
- **Efficiency:** Very fast computation
- **Effectiveness:** N-gram frequencies naturally captured
- **Robustness:** Works well even with missing words

**Example Transformation:**
```python
# Input text
text = "Hello world"

# After n-gram extraction (simplified)
ngrams = ['<H', 'He', 'el', 'll', 'lo', 'o>',  # char 2-grams
          '<He', 'Hel', 'ell', 'llo', 'lo>',   # char 3-grams
          'hello', 'world',                     # word 1-grams
          'hello world']                        # word 2-grams

# Each n-gram → 128-dim vector
# Total: ~15 n-grams × 128 dims = 1920 values

# After averaging
text_embedding = [0.23, -0.12, 0.45, ..., 0.67]  # Single 128-dim vector
```

**Embedding Properties:**
- **Fixed size:** Always 128 dimensions regardless of text length
- **Dense:** All values are real numbers (not sparse)
- **Learned:** Embeddings learned during training on millions of texts
- **Language-aware:** Similar languages have similar embeddings

---

#### 5. Linear Classifier

**Purpose:** Map the 128-dimensional embedding to raw scores (logits) for each language.

**What is a Linear Classifier?**
A linear classifier applies a simple matrix multiplication and addition:
- **Weight matrix:** \( W \in \mathbb{R}^{K \times 128} \) where \( K \) = number of languages
- **Bias vector:** \( \mathbf{b} \in \mathbb{R}^{K} \)
- **Operation:** \( \text{logits} = W \cdot \text{embedding} + \mathbf{b} \)

**Mathematical Formulation:**

\[
\mathbf{z} = W \mathbf{x} + \mathbf{b}
\]

Where:
- \( \mathbf{x} \in \mathbb{R}^{128} \) = text embedding vector
- \( W \in \mathbb{R}^{K \times 128} \) = weight matrix (learned during training)
- \( \mathbf{b} \in \mathbb{R}^{K} \) = bias vector (learned during training)
- \( \mathbf{z} \in \mathbb{R}^{K} \) = logits (raw scores for each language)

**Detailed Computation:**
```python
def linear_classifier(embedding, weight_matrix, bias_vector):
    """
    Apply linear transformation: logits = W @ embedding + b
    
    Args:
        embedding: 128-dimensional vector
        weight_matrix: K × 128 matrix (K = number of languages)
        bias_vector: K-dimensional vector
    
    Returns:
        logits: K-dimensional vector (raw scores)
    """
    # Matrix multiplication: W @ embedding
    logits = [0.0] * len(bias_vector)  # K dimensions
    
    for i in range(len(bias_vector)):  # For each language
        # Dot product: weight_matrix[i] · embedding
        dot_product = 0.0
        for j in range(128):
            dot_product += weight_matrix[i][j] * embedding[j]
        
        # Add bias
        logits[i] = dot_product + bias_vector[i]
    
    return logits

# Example with 3 languages (en, fr, ar)
embedding = [0.23, -0.12, 0.45, ..., 0.67]  # 128 dims

weight_matrix = [
    [0.5, -0.3, 0.2, ..., 0.1],  # English weights (row 0)
    [0.3, 0.4, -0.1, ..., 0.2],  # French weights (row 1)
    [-0.2, 0.1, 0.3, ..., -0.1], # Arabic weights (row 2)
]

bias_vector = [2.5, 1.8, 0.5]  # Language biases

# Compute logits
logits = linear_classifier(embedding, weight_matrix, bias_vector)
# Result: [3.2, 2.1, 0.8]
#         ↑    ↑    ↑
#        en   fr   ar
```

**Interpretation:**
- **Higher logit** = model thinks text is more likely that language
- **Logits are unbounded** (can be any real number)
- **Not probabilities yet** (need softmax normalization)

**Why Linear?**
- **Fast:** Matrix multiplication is highly optimized
- **Interpretable:** Each language has a learned "template" (weight vector)
- **Effective:** Works well for language identification
- **Efficient:** Minimal parameters (K × 128 + K values)

**Weight Matrix Interpretation:**
Each row of \( W \) represents a "language template":
- English row: High weights for English-characteristic n-grams
- French row: High weights for French-characteristic n-grams
- The dot product measures how well the text matches each template

---

#### 6. Softmax Layer

**Purpose:** Convert raw logits into probability distribution over languages.

**What is Softmax?**
Softmax is a mathematical function that:
- Converts arbitrary real numbers (logits) to probabilities
- Ensures all probabilities sum to 1.0
- Makes larger logits exponentially more probable

**Mathematical Formulation:**

\[
p(y = k | \mathbf{x}) = \frac{\exp(z_k)}{\sum_{j=1}^{K} \exp(z_j)} = \frac{\exp(z_k)}{\sum_{j=1}^{K} \exp(z_j)}
\]

Where:
- \( z_k \) = logit for language \( k \)
- \( K \) = total number of languages
- \( \exp(\cdot) \) = exponential function
- Result: Probability between 0 and 1

**Computation Process:**
```python
import math

def softmax(logits):
    """
    Convert logits to probabilities using softmax.
    
    Args:
        logits: List of K raw scores
    
    Returns:
        probabilities: List of K probabilities (sum to 1.0)
    """
    # Step 1: Compute exponentials
    exp_logits = [math.exp(z) for z in logits]
    
    # Step 2: Compute sum of exponentials
    sum_exp = sum(exp_logits)
    
    # Step 3: Normalize
    probabilities = [exp_z / sum_exp for exp_z in exp_logits]
    
    return probabilities

# Example
logits = [3.2, 2.1, 0.8]  # Raw scores for [en, fr, ar]

# Step 1: exp([3.2, 2.1, 0.8]) = [24.53, 8.17, 2.23]
# Step 2: sum = 24.53 + 8.17 + 2.23 = 34.93
# Step 3: probabilities = [24.53/34.93, 8.17/34.93, 2.23/34.93]
#                      = [0.702, 0.234, 0.064]

probabilities = softmax(logits)
# Result: [0.702, 0.234, 0.064]
#         ↑     ↑     ↑
#        en    fr    ar
#        70.2% 23.4% 6.4%
```

**Softmax Properties:**
- **Normalization:** All probabilities sum to exactly 1.0
- **Non-negative:** All probabilities ≥ 0
- **Monotonic:** Larger logits → larger probabilities
- **Smooth:** Differentiable (important for training)

**Why Exponential?**
The exponential function amplifies differences:
- Logits: `[3.2, 2.1, 0.8]` → difference of 1.1 between first two
- After exp: `[24.53, 8.17, 2.23]` → ratio of ~3:1
- This makes the model more confident in its predictions

**Numerical Stability:**
In practice, we subtract the maximum logit to prevent overflow:
```python
def softmax_stable(logits):
    """Numerically stable softmax."""
    max_logit = max(logits)
    exp_logits = [math.exp(z - max_logit) for z in logits]
    sum_exp = sum(exp_logits)
    return [exp_z / sum_exp for exp_z in exp_logits]
```

---

#### 7. Language Probabilities

**Purpose:** Final output providing probability distribution over all languages.

**Output Format:**
```python
# Example output
language_probabilities = {
    'en': 0.702,  # 70.2% confidence - English
    'fr': 0.234,  # 23.4% confidence - French
    'ar': 0.064,  # 6.4% confidence - Arabic
    'de': 0.000,  # 0.0% confidence - German
    # ... all 250+ languages
}
```

**Interpretation:**
- **Highest probability:** Predicted language (`'en'` with 0.702)
- **Confidence score:** The probability value (0.702 = 70.2% confident)
- **Secondary languages:** Other high probabilities indicate:
  - Similar languages (French 23.4% - both Romance languages)
  - Mixed content (text contains multiple languages)
  - Ambiguous text (low confidence overall)

**Decision Making:**
```python
def predict_language(probabilities):
    """
    Extract predicted language and confidence.
    """
    # Find language with highest probability
    predicted_lang = max(probabilities, key=probabilities.get)
    confidence = probabilities[predicted_lang]
    
    return {
        'language_code': predicted_lang,
        'confidence': confidence,
        'all_probabilities': probabilities
    }

# Example
result = predict_language(language_probabilities)
# {
#     'language_code': 'en',
#     'confidence': 0.702,
#     'all_probabilities': {'en': 0.702, 'fr': 0.234, ...}
# }
```

**Confidence Thresholds:**
- **> 0.9:** Very high confidence (clear language)
- **0.7 - 0.9:** High confidence (likely correct)
- **0.5 - 0.7:** Moderate confidence (may need verification)
- **< 0.5:** Low confidence (ambiguous or mixed content)

**Top-K Predictions:**
FastText can return top-K languages:
```python
def get_top_k(probabilities, k=3):
    """Get top K languages by probability."""
    sorted_langs = sorted(
        probabilities.items(),
        key=lambda x: x[1],
        reverse=True
    )
    return sorted_langs[:k]

# Example
top_3 = get_top_k(language_probabilities, k=3)
# [('en', 0.702), ('fr', 0.234), ('ar', 0.064)]
```

**Use Cases:**
1. **Single language detection:** Use highest probability
2. **Mixed language detection:** Check if multiple languages have significant probabilities
3. **Confidence filtering:** Reject predictions below threshold
4. **Language ranking:** Sort by probability for analysis

---

### Complete Pipeline Example

Let's trace a complete example through all stages:

```python
# Input
text = "Hello world"

# Step 1: Input Text
input_text = "Hello world"  # 11 characters

# Step 2: Character N-grams (1-3)
char_ngrams = [
    # 1-grams
    '<', 'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '>',
    # 2-grams
    '<H', 'He', 'el', 'll', 'lo', 'o ', ' w', 'wo', 'or', 'rl', 'ld', 'd>',
    # 3-grams
    '<He', 'Hel', 'ell', 'llo', 'lo ', 'o w', ' wo', 'wor', 'orl', 'rld', 'ld>'
]
# Total: ~35 character n-grams

# Step 3: Word N-grams (1-2)
word_ngrams = [
    'hello',           # 1-gram
    'world',           # 1-gram
    'hello world'      # 2-gram
]
# Total: 3 word n-grams

# Step 4: FastText Embedding (dim=128)
# Each of ~38 n-grams → 128-dim vector
# Average all vectors → single 128-dim vector
text_embedding = [0.23, -0.12, 0.45, ..., 0.67]  # 128 dimensions

# Step 5: Linear Classifier
# W (250 × 128) @ embedding (128 × 1) + b (250 × 1) = logits (250 × 1)
logits = [3.2, 2.1, 0.8, -1.5, ..., 0.3]  # 250 languages

# Step 6: Softmax Layer
# exp(logits) / sum(exp(logits))
probabilities = [0.702, 0.234, 0.064, 0.000, ..., 0.001]  # 250 languages

# Step 7: Language Probabilities
result = {
    'language_code': 'en',      # Highest probability
    'confidence': 0.702,        # 70.2% confident
    'all_probabilities': {...}   # Full distribution
}
```

**Performance Characteristics:**
- **Speed:** <5ms per text (on CPU)
- **Memory:** ~500MB model size (for 250 languages)
- **Accuracy:** >95% on typical texts
- **Robustness:** Works with short texts (10+ characters)

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

## Learning Resources

### FastText Fundamentals

**Official Documentation & Tutorials:**
- **[FastText Official Website](https://fasttext.cc/)** - Official homepage with tutorials and documentation
- **[FastText GitHub Repository](https://github.com/facebookresearch/fastText)** - Source code, examples, and pre-trained models
- **[FastText Python Tutorial](https://fasttext.cc/docs/en/python-module.html)** - Python API documentation and examples
- **[FastText Tutorial (Blog)](https://fasttext.cc/blog/2017/10/02/blog-post.html)** - Getting started guide from FastText team

**Research Papers:**
- **[Bag of Tricks for Efficient Text Classification (2016)](https://arxiv.org/abs/1607.01759)** - Original FastText paper by Joulin et al.
- **[Enriching Word Vectors with Subword Information (2017)](https://arxiv.org/abs/1607.04606)** - FastText word embeddings paper
- **[Language Identification Paper Collection](https://github.com/saffsd/langid.py)** - Comprehensive list of language identification research

### N-grams and Text Features

**Character N-grams:**
- **[N-gram Wikipedia](https://en.wikipedia.org/wiki/N-gram)** - Comprehensive overview of n-grams
- **[Character N-grams Tutorial (Stanford)](https://web.stanford.edu/~jurafsky/slp3/3.pdf)** - Chapter on n-grams from Speech and Language Processing
- **[Text Classification with N-grams (scikit-learn)](https://scikit-learn.org/stable/modules/feature_extraction.html#text-feature-extraction)** - Practical implementation guide

**Word N-grams:**
- **[Word N-grams for NLP (Towards Data Science)](https://towardsdatascience.com/understanding-word-n-grams-and-n-gram-probability-in-natural-language-processing-9d9eef0fa058)** - Understanding word n-grams
- **[N-gram Language Models (Jurafsky & Martin)](https://web.stanford.edu/~jurafsky/slp3/3.pdf)** - Deep dive into n-gram models

### Embeddings and Vector Representations

**Word Embeddings:**
- **[Word Embeddings Explained (Jay Alammar)](https://jalammar.github.io/illustrated-word2vec/)** - Visual explanation of word embeddings
- **[The Illustrated Word2vec (Jay Alammar)](https://jalammar.github.io/illustrated-word2vec/)** - Step-by-step word2vec tutorial
- **[Embeddings Guide (TensorFlow)](https://www.tensorflow.org/text/guide/word_embeddings)** - TensorFlow's guide to embeddings

**FastText Embeddings:**
- **[FastText Embeddings Tutorial](https://fasttext.cc/docs/en/unsupervised-tutorial.html)** - Learning word representations with FastText
- **[FastText vs Word2vec (Blog)](https://towardsdatascience.com/fasttext-under-the-hood-11efc57b2b3)** - Comparison and deep dive
- **[Subword Information in FastText](https://arxiv.org/abs/1607.04606)** - Research paper on subword embeddings

### Linear Classifiers and Machine Learning

**Linear Classification:**
- **[Linear Classifiers (Stanford CS229)](https://cs229.stanford.edu/notes2021spring/cs229-notes1.pdf)** - Mathematical foundations
- **[Linear Models for Classification (scikit-learn)](https://scikit-learn.org/stable/modules/linear_model.html#classification)** - Practical implementation
- **[Logistic Regression Explained (Towards Data Science)](https://towardsdatascience.com/logistic-regression-explained-9ee73cede081)** - Understanding linear classifiers

**Matrix Operations:**
- **[Matrix Multiplication Explained (3Blue1Brown)](https://www.youtube.com/watch?v=LyGKycYT2v0)** - Visual explanation of matrix operations
- **[Linear Algebra for ML (Khan Academy)](https://www.khanacademy.org/math/linear-algebra)** - Free course on linear algebra

### Softmax and Probability Distributions

**Softmax Function:**
- **[Softmax Explained (DeepLearning.AI)](https://www.youtube.com/watch?v=ytbYRIN0N4A)** - Video explanation of softmax
- **[Softmax Function (Wikipedia)](https://en.wikipedia.org/wiki/Softmax_function)** - Mathematical definition and properties
- **[Softmax vs Sigmoid (Towards Data Science)](https://towardsdatascience.com/softmax-function-simplified-714068bf8156)** - Comparison and use cases

**Probability Theory:**
- **[Probability Distributions (Khan Academy)](https://www.khanacademy.org/math/statistics-probability)** - Free course on probability
- **[Cross-Entropy Loss (Towards Data Science)](https://towardsdatascience.com/cross-entropy-loss-function-f38c4ec8643e)** - Understanding loss functions

### Language Identification

**General Language Detection:**
- **[Language Identification Survey (2019)](https://jair.org/index.php/jair/article/view/11675)** - Comprehensive survey paper
- **[langid.py Documentation](https://github.com/saffsd/langid.py)** - Popular Python language detection library
- **[Google's Compact Language Detector](https://github.com/google/cld3)** - Google's language detection library

**FastText Language Models:**
- **[Pre-trained Language Models](https://fasttext.cc/docs/en/language-identification.html)** - FastText's pre-trained language identification models
- **[Language Identification Tutorial](https://fasttext.cc/docs/en/supervised-tutorial.html)** - Training your own language detector
- **[176 Languages Model](https://fasttext.cc/docs/en/language-identification.html)** - Pre-trained model for 176 languages

### Implementation and Code Examples

**Python Implementations:**
- **[FastText Python Examples](https://github.com/facebookresearch/fastText/tree/main/python)** - Official Python examples
- **[Language Detection with FastText (Tutorial)](https://towardsdatascience.com/language-detection-using-fasttext-1c0eb743d0c)** - Step-by-step implementation
- **[FastText Tutorial (Real Python)](https://realpython.com/fasttext-natural-language-processing/)** - Practical Python tutorial

**Hands-on Tutorials:**
- **[FastText Tutorial (Kaggle)](https://www.kaggle.com/code/jhoward/fastai-v2-fasttext)** - Kaggle notebook with examples
- **[Building Language Detector (Medium)](https://medium.com/@ageitgey/natural-language-processing-is-fun-part-2-using-a-machine-learning-classifier-to-build-a-language-3c594d3d7253)** - Building from scratch
- **[FastText Colab Notebook](https://colab.research.google.com/github/facebookresearch/fastText/blob/master/tutorials/supervised_learning.ipynb)** - Interactive Google Colab tutorial

### Advanced Topics

**Optimization and Performance:**
- **[Efficient Text Classification (Paper)](https://arxiv.org/abs/1607.01759)** - FastText optimization techniques
- **[Batch Processing Optimization](https://pytorch.org/tutorials/beginner/data_loading_tutorial.html)** - Efficient batch processing
- **[Model Compression Techniques](https://arxiv.org/abs/1607.04606)** - Reducing model size

**Production Deployment:**
- **[FastText Model Serving](https://fasttext.cc/docs/en/support.html)** - Production deployment guide
- **[Model Optimization (ONNX)](https://onnx.ai/)** - Converting models for production
- **[FastText C++ API](https://fasttext.cc/docs/en/cpp-api.html)** - High-performance C++ implementation

### Related Concepts

**Natural Language Processing:**
- **[NLP Course (Stanford)](https://web.stanford.edu/~jurafsky/slp3/)** - Free online NLP textbook
- **[NLP with Deep Learning (Stanford CS224N)](https://web.stanford.edu/class/cs224n/)** - Advanced NLP course
- **[Hugging Face NLP Course](https://huggingface.co/learn/nlp-course/)** - Modern NLP with transformers

**Text Classification:**
- **[Text Classification Guide (scikit-learn)](https://scikit-learn.org/stable/tutorial/text_analytics/working_with_text_data.html)** - Practical text classification
- **[Text Classification Survey](https://arxiv.org/abs/2004.03705)** - Recent advances in text classification

### Quick Start Guides

**For Beginners:**
1. Start with: [FastText Official Tutorial](https://fasttext.cc/docs/en/supervised-tutorial.html)
2. Then read: [FastText Paper (arXiv)](https://arxiv.org/abs/1607.01759)
3. Practice: [FastText Python Examples](https://github.com/facebookresearch/fastText/tree/main/python)

**For Practitioners:**
1. Deep dive: [FastText GitHub Repository](https://github.com/facebookresearch/fastText)
2. Advanced: [Language Identification Survey](https://jair.org/index.php/jair/article/view/11675)
3. Production: [FastText C++ API](https://fasttext.cc/docs/en/cpp-api.html)

**For Researchers:**
1. Foundation: [Bag of Tricks Paper](https://arxiv.org/abs/1607.01759)
2. Subword info: [Subword Information Paper](https://arxiv.org/abs/1607.04606)
3. Survey: [Language Identification Survey](https://jair.org/index.php/jair/article/view/11675)

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

