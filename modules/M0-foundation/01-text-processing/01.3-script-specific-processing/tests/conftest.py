"""Pytest configuration and fixtures for script-specific processing tests."""

import sys
from pathlib import Path

# Ensure we import from the correct text_processing module (this one, not 01.2)
# Remove any conflicting paths that might point to 01.2-language-detection
conflicting_paths = [p for p in sys.path if '01.2-language-detection' in str(p)]
for path in conflicting_paths:
    sys.path.remove(path)

# Add the parent directory to sys.path so imports work correctly
module_dir = Path(__file__).parent.parent
if str(module_dir) not in sys.path:
    sys.path.insert(0, str(module_dir))

# Clear import cache to ensure fresh imports
import importlib
importlib.invalidate_caches()

import pytest
from dataclasses import dataclass
from typing import List, Tuple


def pytest_configure(config):
    """Configure pytest to use the correct module path."""
    # This runs before test collection, ensuring correct imports
    import sys
    from pathlib import Path
    
    # Remove conflicting paths
    conflicting_paths = [p for p in sys.path if '01.2-language-detection' in str(p)]
    for path in conflicting_paths:
        if path in sys.path:
            sys.path.remove(path)
    
    # Ensure our module directory is first
    module_dir = Path(__file__).parent.parent
    module_dir_str = str(module_dir)
    if module_dir_str in sys.path:
        sys.path.remove(module_dir_str)
    sys.path.insert(0, module_dir_str)
    
    # Clear import cache
    importlib.invalidate_caches()


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


@pytest.fixture
def language_info_ur():
    """Urdu language info."""
    return LanguageInfo(
        language_code="ur",
        script_code="Arab",
        confidence=0.97
    )


@pytest.fixture
def language_info_de():
    """German language info."""
    return LanguageInfo(
        language_code="de",
        script_code="Latn",
        confidence=0.98
    )


@pytest.fixture
def language_info_es():
    """Spanish language info."""
    return LanguageInfo(
        language_code="es",
        script_code="Latn",
        confidence=0.98
    )


@pytest.fixture
def language_info_pl():
    """Polish language info."""
    return LanguageInfo(
        language_code="pl",
        script_code="Latn",
        confidence=0.97
    )
