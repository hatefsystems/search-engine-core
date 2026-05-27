# 📝 Custom Text Testing Guide

Complete guide for testing the normalizer with your own custom text.

---

## 🎯 Quick Start

### Method 1: Interactive Command-Line Tool (Easiest)

```bash
cd /root/search-engine-core/ml-pipeline

# Test any text directly
python3 interactive_test.py "Your text here"

# Test multiple texts at once
python3 interactive_test.py "Hello" "سلام" "你好"

# Verbose mode (shows detailed transformations)
python3 interactive_test.py --verbose "Your text"

# Show examples
python3 interactive_test.py --examples
```

### Method 2: Interactive Mode

```bash
python3 interactive_test.py
```

Then type your text when prompted:
```
📝 Enter text: Hello World
📝 Enter text: سلام دنیا
📝 Enter text: quit
```

### Method 3: Pytest Parametrized Tests

Edit `tests/test_custom_text.py` and add your text to the parameters:

```python
@pytest.mark.parametrize("text,expected_script,description", [
    ("Your custom text", "Latn", "My test description"),
    ("متن دلخواه شما", "Arab", "Persian custom text"),
    # Add more...
])
```

Then run:
```bash
pytest tests/test_custom_text.py::test_custom_text_parametrized -v
```

### Method 4: Python Script

```python
from tests.test_custom_text import normalize_text

# Test your text
result = normalize_text("Your text here")
```

---

## 📚 Detailed Usage

### 1. Command-Line Tool (interactive_test.py)

#### Basic Usage
```bash
# Test single text
python3 interactive_test.py "Hello World"

# Test multiple texts
python3 interactive_test.py "Text 1" "Text 2" "Text 3"

# Persian text
python3 interactive_test.py "سلام دنیا چطور هستید؟"

# Chinese text
python3 interactive_test.py "你好世界，今天天气很好"

# Mixed languages
python3 interactive_test.py "Hello سلام 你好 Привет"
```

#### Verbose Mode (Shows All Transformations)
```bash
python3 interactive_test.py --verbose "Your text"
```

Output:
```
======================================================================
📝 INPUT TEXT:
   Your text

📊 NORMALIZED TEXT:
   Your text

🔍 METADATA:
   Script:          Latn
   Original Length: 9 characters
   Final Length:    9 characters
   Changes Applied: 2

🔧 TRANSFORMATION DETAILS:
   1. Applied NFKC normalization
   2. Normalized whitespace
======================================================================
```

#### Batch Testing from File
```bash
# Create a text file with your texts (one per line)
echo "Hello World" > my_texts.txt
echo "سلام دنیا" >> my_texts.txt
echo "你好世界" >> my_texts.txt

# Test all texts from file
python3 interactive_test.py --file my_texts.txt
```

#### Interactive Mode
```bash
python3 interactive_test.py
```

Available commands:
- Type any text to normalize it
- `verbose` - Toggle detailed output
- `examples` - Show sample texts
- `quit` or `exit` - Quit the program

---

### 2. Pytest Tests

#### Run Single Text Test
```bash
# Edit the text in test_single_text() first
pytest tests/test_custom_text.py::test_single_text -v -s
```

#### Run Your Custom Texts Test
```bash
# Edit custom_texts list in test_your_custom_text() first
pytest tests/test_custom_text.py::test_your_custom_text -v -s
```

#### Run Parametrized Tests
```bash
# All parametrized tests
pytest tests/test_custom_text.py::test_custom_text_parametrized -v

# Specific language
pytest tests/test_custom_text.py::test_custom_text_parametrized[Persian] -v
```

#### Run Class-Based Tests
```bash
# All class tests
pytest tests/test_custom_text.py::TestCustomInputs -v

# Specific test
pytest tests/test_custom_text.py::TestCustomInputs::test_persian_sentences -v
```

---

### 3. Python Script Usage

#### Quick Test
```python
from text_processing import normalize_universal

# Your custom text
text = "Your text here"
result = normalize_universal(text)

print(f"Input:      {result.original}")
print(f"Output:     {result.text}")
print(f"Script:     {result.script}")
print(f"Changes:    {len(result.changes)}")
```

#### Using Utility Function
```python
from tests.test_custom_text import normalize_text

# Automatically shows detailed output
result = normalize_text("Your text here")

# Without details
result = normalize_text("Your text here", show_details=False)
```

#### Batch Processing
```python
from text_processing.normalizer import normalize_batch

texts = [
    "Your first text",
    "Your second text",
    "Your third text"
]

results = normalize_batch(texts)

for result in results:
    print(f"{result.script}: {result.text}")
```

---

## 🎨 Examples

### English Text
```bash
python3 interactive_test.py "The quick brown fox jumps over the lazy dog."
```

### Persian Text
```bash
python3 interactive_test.py "سلام دنیا! چطور هستید؟"
```

### Arabic Text
```bash
python3 interactive_test.py "مرحبا بالعالم! كيف حالك؟"
```

### Chinese Text
```bash
python3 interactive_test.py "你好世界！今天天气很好。"
```

### Japanese Text
```bash
python3 interactive_test.py "こんにちは世界！お元気ですか？"
```

### Mixed Script
```bash
python3 interactive_test.py "Hello سلام 你好 Привет שלום"
```

### Text with Special Characters
```bash
python3 interactive_test.py --verbose "Hello  World   with    spaces"
python3 interactive_test.py --verbose "Text with soft­hyphen"
python3 interactive_test.py --verbose "Persian with ZWNJ: می‌خواهم"
```

---

## 📊 Adding Your Own Tests

### Option 1: Edit test_custom_text.py

Open `tests/test_custom_text.py` and modify:

```python
def test_your_custom_text():
    # ⭐ ADD YOUR CUSTOM TEXTS HERE ⭐
    custom_texts = [
        "Your custom text here",
        "Add as many as you want",
        "Each will be tested individually",
    ]
```

### Option 2: Add Parametrized Tests

```python
@pytest.mark.parametrize("text,expected_script,description", [
    # Add your tests here
    ("My test text", "Latn", "Test description"),
    ("Another test", None, "No script check"),
])
def test_custom_text_parametrized(text, expected_script, description):
    # Test code runs automatically
```

### Option 3: Create Your Own Test File

```python
# my_custom_tests.py
from text_processing import normalize_universal

def test_my_specific_case():
    text = "Your specific test case"
    result = normalize_universal(text)
    
    assert result.text == "Expected output"
    assert result.script == "Latn"
```

Run with:
```bash
pytest my_custom_tests.py -v
```

---

## 🚀 Advanced Usage

### Testing with Different Options

```python
from text_processing import normalize_universal

# Preserve special characters (ZWNJ, ZWJ)
result = normalize_universal(text, preserve_special=True)

# Disable character unification
result = normalize_universal(text, unify_chars=False)

# Both options
result = normalize_universal(
    text,
    preserve_special=True,
    unify_chars=False
)
```

### Batch Testing with Options

```python
from text_processing.normalizer import normalize_batch

texts = ["text1", "text2", "text3"]

# Pass options to all texts
results = normalize_batch(texts, preserve_special=False)
```

### Performance Testing Your Text

```python
import time
from text_processing import normalize_universal

text = "Your text to benchmark"
iterations = 1000

start = time.time()
for _ in range(iterations):
    normalize_universal(text)
elapsed = time.time() - start

throughput = iterations / elapsed
print(f"Throughput: {throughput:.2f} docs/sec")
```

---

## 🎯 Common Use Cases

### 1. Testing Different Scripts
```bash
python3 interactive_test.py \
  "English" \
  "فارسی" \
  "中文" \
  "Русский" \
  "עברית" \
  "ไทย"
```

### 2. Testing Whitespace Normalization
```bash
python3 interactive_test.py --verbose "Text  with    extra     spaces"
```

### 3. Testing Character Unification
```bash
# Arabic yeh → Persian yeh
python3 interactive_test.py --verbose "يك دو سه"
```

### 4. Testing Special Characters
```bash
python3 interactive_test.py --verbose "Text­with­soft­hyphens"
```

### 5. Testing Mixed Scripts
```bash
python3 interactive_test.py "English + فارسی + 中文 = Mixed"
```

---

## 📝 Tips

1. **Use verbose mode** to see exactly what transformations are applied
2. **Test edge cases** like empty strings, very long text, special characters
3. **Compare before/after** to understand the normalization effects
4. **Use parametrized tests** for regression testing
5. **Batch test** similar texts together for efficiency

---

## 🐛 Troubleshooting

### Issue: Text not normalized as expected
```bash
# Use verbose mode to see what's happening
python3 interactive_test.py --verbose "Your problematic text"
```

### Issue: Script detection wrong
```python
from text_processing.normalizer import detect_script

script = detect_script("Your text")
print(f"Detected script: {script}")
```

### Issue: Need to preserve special characters
```python
result = normalize_universal(text, preserve_special=True)
```

---

## 📚 Resources

- **Main Documentation**: `README.md`
- **API Reference**: `text_processing/normalizer.py` (docstrings)
- **Task Details**: `.github/ISSUE_TEMPLATE/atomic-tasks/M0-foundation/01-text-processing/01.1-unicode-normalization.md`
- **Completion Report**: `docs/TASK_01.1_COMPLETION.md`

---

**Happy Testing! 🎉**

