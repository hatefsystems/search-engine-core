"""
Language Detection - Task 01.2

Universal language detection supporting 176+ languages with FastText.
Scalable to 250+ languages through custom training.

Main exports:
- UniversalLanguageDetector: Main detector class
- LanguageInfo: Detection result dataclass
- detect_language: Convenience function
"""

from .language_detector import (
    UniversalLanguageDetector,
    LanguageInfo,
    detect_language,
    detect_language_batch
)

__all__ = [
    'UniversalLanguageDetector',
    'LanguageInfo',
    'detect_language',
    'detect_language_batch',
]

__version__ = '0.1.0'

