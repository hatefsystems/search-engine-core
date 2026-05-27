"""
Text processing module - M0 Foundation tasks.

This module contains universal text processing components:
- Unicode normalization (Task 01.1)
- Language detection (Task 01.2)
- Script-specific processing (Task 01.3)
- Stopword analysis (Task 01.4)
- Batch jobs (Task 01.5)
"""

from .normalizer import (
    normalize_universal,
    NormalizedText,
    unify_characters,
    handle_special_chars,
)

__all__ = [
    "normalize_universal",
    "NormalizedText",
    "unify_characters",
    "handle_special_chars",
]

__version__ = "0.1.0"

