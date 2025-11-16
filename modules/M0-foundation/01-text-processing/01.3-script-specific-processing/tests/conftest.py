"""Pytest configuration and fixtures for script-specific processing tests."""

import pytest
from dataclasses import dataclass
from typing import List, Tuple


@dataclass
class LanguageInfo:
    """Minimal LanguageInfo for testing."""
    language_code: str
    script_code: str
    confidence: float
    is_mixed_content: bool = False
    detected_languages: List[Tuple[str, float]] = None
    
    def __post_init__(self):
        if self.detected_languages is None:
            self.detected_languages = []


@pytest.fixture
def language_info_fa():
    """Persian language info."""
    return LanguageInfo(
        language_code="fa",
        script_code="Arab",
        confidence=0.98
    )


@pytest.fixture
def language_info_ar():
    """Arabic language info."""
    return LanguageInfo(
        language_code="ar",
        script_code="Arab",
        confidence=0.95
    )


@pytest.fixture
def language_info_zh():
    """Chinese language info."""
    return LanguageInfo(
        language_code="zh",
        script_code="Hans",
        confidence=0.99
    )


@pytest.fixture
def language_info_ja():
    """Japanese language info."""
    return LanguageInfo(
        language_code="ja",
        script_code="Jpan",
        confidence=0.97
    )


@pytest.fixture
def language_info_ko():
    """Korean language info."""
    return LanguageInfo(
        language_code="ko",
        script_code="Kore",
        confidence=0.96
    )


@pytest.fixture
def language_info_ru():
    """Russian language info."""
    return LanguageInfo(
        language_code="ru",
        script_code="Cyrl",
        confidence=0.98
    )


@pytest.fixture
def language_info_en():
    """English language info."""
    return LanguageInfo(
        language_code="en",
        script_code="Latn",
        confidence=0.99
    )


@pytest.fixture
def language_info_fr():
    """French language info."""
    return LanguageInfo(
        language_code="fr",
        script_code="Latn",
        confidence=0.97
    )
