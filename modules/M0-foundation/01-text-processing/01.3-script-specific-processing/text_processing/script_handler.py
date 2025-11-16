"""
Script Handler - Task 01.3

Main orchestrator for script-specific processing.
Routes text to appropriate processor based on script code.
Handles mixed-script text and bidirectional text.
"""

import re
import sys
from pathlib import Path
from typing import List, Tuple, Optional, TYPE_CHECKING

from shared.logger import setup_logger

from .arabic_processor import process_arabic
from .cjk_processor import process_cjk
from .cyrillic_processor import process_cyrillic
from .latin_processor import process_latin

# Import ProcessedText - avoid circular import by importing from __init__ after it's defined
if TYPE_CHECKING:
    from .__init__ import ProcessedText
else:
    # Runtime import - ProcessedText is defined in __init__.py before this import
    from . import ProcessedText

logger = setup_logger(__name__)

# Try to import LanguageInfo from Task 01.2
try:
    task_01_2_path = Path(__file__).parent.parent.parent.parent / "01.2-language-detection"
    if task_01_2_path.exists():
        sys.path.insert(0, str(task_01_2_path))
        from text_processing import LanguageInfo
    else:
        # Fallback: define minimal LanguageInfo if Task 01.2 not available
        from dataclasses import dataclass
        from typing import List, Tuple
        
        @dataclass
        class LanguageInfo:
            language_code: str
            script_code: str
            confidence: float
            is_mixed_content: bool = False
            detected_languages: List[Tuple[str, float]] = None
            
            def __post_init__(self):
                if self.detected_languages is None:
                    self.detected_languages = []
except ImportError:
    # Fallback: define minimal LanguageInfo
    from dataclasses import dataclass
    from typing import List, Tuple
    
    @dataclass
    class LanguageInfo:
        language_code: str
        script_code: str
        confidence: float
        is_mixed_content: bool = False
        detected_languages: List[Tuple[str, float]] = None
        
        def __post_init__(self):
            if self.detected_languages is None:
                self.detected_languages = []


# Script detection regex patterns
SCRIPT_PATTERNS = {
    'Arab': re.compile(r'[\u0600-\u06FF\u0750-\u077F\u08A0-\u08FF\uFB50-\uFDFF\uFE70-\uFEFF]'),
    'Latn': re.compile(r'[a-zA-Z]'),
    'Cyrl': re.compile(r'[\u0400-\u04FF]'),
    'Hans': re.compile(r'[\u4E00-\u9FFF]'),
    'Hant': re.compile(r'[\u4E00-\u9FFF]'),
    'Jpan': re.compile(r'[\u3040-\u309F\u30A0-\u30FF\u4E00-\u9FFF]'),  # Hiragana, Katakana, Kanji
    'Kore': re.compile(r'[\uAC00-\uD7AF\u1100-\u11FF\u3130-\u318F]'),  # Hangul
}


def detect_script_boundaries(text: str) -> List[Tuple[int, int, str]]:
    """
    Detect script boundaries in mixed-script text.
    
    Args:
        text: Input text
        
    Returns:
        List of (start, end, script_code) tuples
    """
    if not text:
        return []
    
    boundaries = []
    current_script = None
    start_pos = 0
    
    i = 0
    while i < len(text):
        char = text[i]
        
        # Skip whitespace and punctuation
        if char.isspace() or not char.isalnum():
            i += 1
            continue
        
        # Detect script for this character
        detected_script = None
        for script_code, pattern in SCRIPT_PATTERNS.items():
            if pattern.match(char):
                detected_script = script_code
                break
        
        # If no script detected, use "Zyyy" (Common)
        if detected_script is None:
            detected_script = "Zyyy"
        
        # If script changed, save previous segment
        if current_script is not None and detected_script != current_script:
            if i > start_pos:
                boundaries.append((start_pos, i, current_script))
            start_pos = i
            current_script = detected_script
        elif current_script is None:
            current_script = detected_script
            start_pos = i
        
        i += 1
    
    # Add final segment
    if current_script is not None and start_pos < len(text):
        boundaries.append((start_pos, len(text), current_script))
    
    return boundaries


def handle_bidirectional_text(text: str) -> str:
    """
    Handle bidirectional text (RTL + LTR mixing).
    
    Uses Unicode BiDi algorithm implicitly through proper text handling.
    For now, we preserve the logical order.
    
    Args:
        text: Input text
        
    Returns:
        Text with bidirectional handling applied
    """
    # Unicode bidirectional algorithm is handled by the rendering system
    # We preserve logical order here
    # For more sophisticated handling, consider using python-bidi library
    return text


class ScriptHandler:
    """
    Main handler for script-specific text processing.
    """
    
    def __init__(self):
        """Initialize script handler."""
        logger.info("Initialized ScriptHandler")
    
    def process_by_script(
        self,
        text: str,
        language_info: LanguageInfo,
        **kwargs
    ) -> ProcessedText:
        """
        Process text based on script code from LanguageInfo.
        
        Args:
            text: Input text
            language_info: LanguageInfo from Task 01.2
            **kwargs: Additional processor-specific options
            
        Returns:
            ProcessedText with processed text and metadata
        """
        if not text:
            return ProcessedText(
                text="",
                original="",
                script_code=language_info.script_code,
                language_code=language_info.language_code,
                applied_rules=[],
                confidence=language_info.confidence
            )
        
        script_code = language_info.script_code
        language_code = language_info.language_code
        
        # Route to appropriate processor
        if script_code == "Arab":
            result = process_arabic(
                text,
                language_code,
                preserve_diacritics=kwargs.get('preserve_diacritics', False),
                normalize_shapes=kwargs.get('normalize_shapes', True)
            )
        elif script_code in ("Hans", "Hant", "Jpan", "Kore"):
            result = process_cjk(text, language_code, script_code)
        elif script_code == "Cyrl":
            result = process_cyrillic(
                text,
                language_code,
                normalize_yo=kwargs.get('normalize_yo', True)
            )
        elif script_code == "Latn":
            result = process_latin(
                text,
                language_code,
                normalize_diacritics_flag=kwargs.get('normalize_diacritics', False),
                preserve_semantic_ligatures=kwargs.get('preserve_semantic_ligatures', True)
            )
        else:
            # Unknown script - return as-is
            logger.warning(f"Unknown script code: {script_code}, returning text as-is")
            result = ProcessedText(
                text=text,
                original=text,
                script_code=script_code,
                language_code=language_code,
                applied_rules=["no_processing"],
                confidence=language_info.confidence
            )
        
        # Set confidence from language_info
        result.confidence = language_info.confidence
        
        return result
    
    def process_mixed_script(
        self,
        text: str,
        language_info: LanguageInfo,
        **kwargs
    ) -> ProcessedText:
        """
        Process mixed-script text by detecting boundaries and processing each segment.
        
        Args:
            text: Input text (may contain multiple scripts)
            language_info: LanguageInfo from Task 01.2
            **kwargs: Additional processor-specific options
            
        Returns:
            ProcessedText with processed text and metadata
        """
        if not text:
            return ProcessedText(
                text="",
                original="",
                script_code=language_info.script_code,
                language_code=language_info.language_code,
                applied_rules=[],
                confidence=language_info.confidence
            )
        
        # Detect script boundaries
        boundaries = detect_script_boundaries(text)
        
        if len(boundaries) <= 1:
            # Single script, use regular processing
            return self.process_by_script(text, language_info, **kwargs)
        
        # Process each segment
        processed_segments = []
        all_applied_rules = set()
        all_word_boundaries = []
        offset = 0
        
        for start, end, script_code in boundaries:
            segment = text[start:end]
            
            # Create LanguageInfo for this segment
            segment_lang_info = LanguageInfo(
                language_code=language_info.language_code,  # Use primary language
                script_code=script_code,
                confidence=language_info.confidence
            )
            
            # Process segment
            segment_result = self.process_by_script(segment, segment_lang_info, **kwargs)
            
            # Adjust word boundaries for segment position
            adjusted_boundaries = [b + start for b in segment_result.word_boundaries]
            all_word_boundaries.extend(adjusted_boundaries)
            
            processed_segments.append(segment_result.text)
            all_applied_rules.update(segment_result.applied_rules)
        
        # Combine processed segments
        processed_text = ''.join(processed_segments)
        
        # Handle bidirectional text
        processed_text = handle_bidirectional_text(processed_text)
        
        logger.debug(
            f"Processed mixed-script text",
            segments=len(boundaries),
            scripts=[s for _, _, s in boundaries],
            rules_applied=list(all_applied_rules)
        )
        
        return ProcessedText(
            text=processed_text,
            original=text,
            script_code=language_info.script_code,  # Primary script
            language_code=language_info.language_code,
            applied_rules=list(all_applied_rules),
            word_boundaries=sorted(set(all_word_boundaries)),
            confidence=language_info.confidence
        )


# Convenience functions
def process_by_script(
    text: str,
    language_info: LanguageInfo,
    **kwargs
) -> ProcessedText:
    """
    Convenience function to process text by script.
    
    Args:
        text: Input text
        language_info: LanguageInfo from Task 01.2
        **kwargs: Additional processor-specific options
        
    Returns:
        ProcessedText with processed text and metadata
    """
    handler = ScriptHandler()
    return handler.process_by_script(text, language_info, **kwargs)


def process_mixed_script(
    text: str,
    language_info: LanguageInfo,
    **kwargs
) -> ProcessedText:
    """
    Convenience function to process mixed-script text.
    
    Args:
        text: Input text (may contain multiple scripts)
        language_info: LanguageInfo from Task 01.2
        **kwargs: Additional processor-specific options
        
    Returns:
        ProcessedText with processed text and metadata
    """
    handler = ScriptHandler()
    return handler.process_mixed_script(text, language_info, **kwargs)
