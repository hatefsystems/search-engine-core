"""
Script-Specific Processing - Task 01.3

Processes text based on detected script codes from Task 01.2.
Handles Arabic (ZWNJ preservation), CJK (word segmentation), 
Cyrillic (variant unification), Latin (diacritic handling), 
and mixed-script scenarios.

Main exports:
- ScriptHandler: Main orchestrator class
- ProcessedText: Processing result dataclass
- process_by_script: Convenience function
"""

from dataclasses import dataclass, field
from typing import List

# Define ProcessedText first to avoid circular imports
@dataclass
class ProcessedText:
    """
    Script-processed text result with metadata.
    
    Attributes:
        text: Script-processed text
        original: Original text before processing
        script_code: ISO 15924 script code (e.g., "Arab", "Latn", "Hans")
        language_code: ISO 639-1 language code (e.g., "fa", "en", "zh")
        applied_rules: List of processing rules applied
        word_boundaries: Character positions of word boundaries (for CJK)
        confidence: Language detection confidence (from LanguageInfo)
    """
    text: str
    original: str
    script_code: str
    language_code: str
    applied_rules: List[str] = field(default_factory=list)
    word_boundaries: List[int] = field(default_factory=list)
    confidence: float = 0.0


# Import handlers after ProcessedText is defined
from .script_handler import (
    ScriptHandler,
    process_by_script,
    process_mixed_script,
)

__all__ = [
    'ScriptHandler',
    'ProcessedText',
    'process_by_script',
    'process_mixed_script',
]

__version__ = '0.1.0'
