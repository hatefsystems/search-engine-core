"""Pytest configuration and fixtures for ml-pipeline tests."""

import pytest


@pytest.fixture
def sample_texts():
    """Sample texts in multiple languages for testing."""
    return {
        "persian": "سلام دنیا",
        "arabic": "مرحبا بالعالم",
        "chinese": "你好世界",
        "japanese": "こんにちは世界",
        "korean": "안녕하세요 세계",
        "russian": "Привет мир",
        "hindi": "नमस्ते दुनिया",
        "hebrew": "שלום עולם",
        "thai": "สวัสดีชาวโลก",
        "greek": "Γειά σου κόσμε",
        "english": "Hello World",
        "mixed": "Hello سلام 你好 Привет",
    }


@pytest.fixture
def malformed_texts():
    """Malformed or edge case texts for testing."""
    return [
        "",  # Empty string
        " ",  # Whitespace only
        "   \n\t   ",  # Multiple whitespace types
        "a" * 10000,  # Very long text
        "\u200c\u200d",  # Only special characters
        "Hello\u00adWorld",  # Soft hyphen
        "Test\ufeffBOM",  # BOM character
        None,  # None value (should raise ValueError)
    ]


@pytest.fixture
def arabic_persian_variants():
    """Arabic and Persian character variants for unification testing."""
    return {
        "arabic_yeh": "يک دو سه",  # Arabic yeh
        "persian_yeh": "یک دو سه",  # Persian yeh
        "arabic_kaf": "كتاب",  # Arabic kaf
        "persian_kaf": "کتاب",  # Persian kaf
    }


@pytest.fixture
def special_char_texts():
    """Texts with special Unicode characters."""
    return {
        "zwnj": "می‌خواهم",  # Persian with ZWNJ
        "zwj": "क्‍ष",  # Devanagari with ZWJ
        "soft_hyphen": "soft\u00adhyphen",
        "zero_width_space": "zero\u200bwidth",
        "bom": "\ufeffBOM at start",
    }


@pytest.fixture
def performance_corpus():
    """Large corpus for performance testing."""
    texts = []
    sample_text = "This is a sample text for performance testing. " * 10
    
    # Generate 1000 documents
    for i in range(1000):
        texts.append(f"Document {i}: {sample_text}")
    
    return texts

