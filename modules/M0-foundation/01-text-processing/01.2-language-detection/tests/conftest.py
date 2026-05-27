"""
Pytest configuration and fixtures for language detection tests.
"""

import pytest
from pathlib import Path


@pytest.fixture
def test_texts():
    """Sample texts in various languages for testing."""
    return {
        # Latin scripts
        'en': "The quick brown fox jumps over the lazy dog",
        'es': "El rápido zorro marrón salta sobre el perro perezoso",
        'fr': "Le rapide renard brun saute par-dessus le chien paresseux",
        'de': "Der schnelle braune Fuchs springt über den faulen Hund",
        'it': "La volpe marrone veloce salta sopra il cane pigro",
        'pt': "A rápida raposa marrom salta sobre o cão preguiçoso",
        
        # Arabic script
        'ar': "الثعلب البني السريع يقفز فوق الكلب الكسول",
        'fa': "روباه قهوه‌ای سریع از روی سگ تنبل می‌پرد",
        'ur': "تیز بھوری لومڑی سست کتے کے اوپر چھلانگ لگاتی ہے",
        
        # Cyrillic script
        'ru': "Быстрая коричневая лиса прыгает через ленивую собаку",
        'uk': "Швидка коричнева лисиця стрибає через ледачого собаку",
        'bg': "Бързата кафява лисица скача над мързеливото куче",
        
        # CJK
        'zh': "这是一个中文示例文本用于语言检测测试和验证",  # Longer Chinese text for better detection
        'ja': "素早い茶色のキツネが怠け者の犬を飛び越える",
        'ko': "빠른 갈색 여우가 게으른 개를 뛰어넘습니다",
        
        # Other scripts
        'hi': "तेज़ भूरी लोमड़ी आलसी कुत्ते के ऊपर कूदती है",
        'he': "השועל החום המהיר קופץ מעל הכלב העצלן",
        'el': "Η γρήγορη καφέ αλεπού πηδάει πάνω από το τεμπέλικο σκυλί",
        'th': "สุนัขจิ้งจอกสีน้ำตาลที่รวดเร็วกระโดดข้ามสุนัขที่ขี้เกียจ",
        'vi': "Con cáo nâu nhanh nhẹn nhảy qua con chó lười biếng",
    }


@pytest.fixture
def short_texts():
    """Very short texts for testing fallback detection."""
    return {
        'en': "Hello",
        'fa': "سلام",
        'ar': "مرحبا",
        'zh': "你好",
        'ja': "こんにちは",
        'ko': "안녕",
        'ru': "Привет",
        'es': "Hola",
        'fr': "Bonjour",
        'de': "Hallo",
    }


@pytest.fixture
def mixed_texts():
    """Mixed-language texts for testing."""
    return [
        "Hello سلام مرحبا",  # English + Persian + Arabic
        "This is English with 中文",  # English + Chinese
        "Bonjour, 안녕하세요",  # French + Korean
        "مرحبا Hello Привет",  # Arabic + English + Russian
    ]


@pytest.fixture
def edge_cases():
    """Edge case texts for robustness testing."""
    return {
        'empty': "",
        'whitespace': "   \n\t  ",
        'numbers': "123456789",
        'symbols': "!@#$%^&*()",
        'mixed_symbols': "abc123!@#",
        'single_char': "a",
        'emoji': "😀🎉🚀",
        'mixed_emoji': "Hello 😀 World",
    }


@pytest.fixture
def models_dir():
    """Get models directory path."""
    return Path(__file__).parent.parent / "models"


@pytest.fixture
def has_model(models_dir):
    """Check if any model is available."""
    if not models_dir.exists():
        return False
    
    model_files = list(models_dir.glob("*.bin")) + list(models_dir.glob("*.ftz"))
    return len(model_files) > 0

