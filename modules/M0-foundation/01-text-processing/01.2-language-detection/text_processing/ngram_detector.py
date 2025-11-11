"""
N-gram Language Detection - Task 01.2

Fallback detector for short texts (<20 characters) where FastText may be unreliable.
Uses character n-gram profiles for language identification.

This is a lightweight fallback, not meant to replace FastText for normal texts.
"""

import re
from collections import Counter
from typing import List, Tuple, Dict

try:
    from langdetect import detect_langs, LangDetectException
    LANGDETECT_AVAILABLE = True
except ImportError:
    LANGDETECT_AVAILABLE = False

from shared.logger import setup_logger

logger = setup_logger(__name__)


class NgramDetector:
    """
    N-gram based language detection for short texts.
    
    Uses langdetect library as fallback for texts where FastText
    may be unreliable (very short text, single words, etc.)
    
    Features:
    - Fast detection for short texts
    - 55+ languages supported
    - Probabilistic approach
    
    Example:
        >>> detector = NgramDetector()
        >>> predictions = detector.detect("Hello", k=3)
        >>> print(predictions)
        [('en', 0.87), ('fr', 0.08), ('de', 0.05)]
    """
    
    def __init__(self):
        """Initialize n-gram detector."""
        if not LANGDETECT_AVAILABLE:
            logger.warning(
                "langdetect library not available. "
                "N-gram fallback disabled. "
                "Install with: pip install langdetect"
            )
        
        self.available = LANGDETECT_AVAILABLE
        logger.info("N-gram detector initialized", available=self.available)
    
    def detect(self, text: str, k: int = 3) -> List[Tuple[str, float]]:
        """
        Detect language using n-gram analysis.
        
        Args:
            text: Input text (preferably short text)
            k: Number of top predictions to return
            
        Returns:
            List of (language_code, probability) tuples
        """
        if not self.available:
            logger.warning("N-gram detector not available, returning empty")
            return []
        
        if not text or not text.strip():
            return []
        
        try:
            # langdetect returns list of Language objects
            results = detect_langs(text)
            
            # Convert to our format and limit to k results
            predictions = [
                (lang.lang, lang.prob)
                for lang in results[:k]
            ]
            
            logger.debug(
                "N-gram detection",
                text_preview=text[:50],
                top_prediction=predictions[0] if predictions else None
            )
            
            return predictions
            
        except LangDetectException as e:
            logger.warning(
                "N-gram detection failed",
                error=str(e),
                text_preview=text[:50]
            )
            return []
        except Exception as e:
            logger.error(
                "Unexpected n-gram detection error",
                error=str(e),
                text_preview=text[:50]
            )
            return []
    
    def is_available(self) -> bool:
        """Check if n-gram detector is available."""
        return self.available


class SimpleNgramDetector:
    """
    Simple n-gram detector without external dependencies.
    
    This is a minimal implementation for when langdetect is not available.
    Only supports basic language detection for common scripts.
    """
    
    # Character range definitions for common scripts
    SCRIPT_RANGES = {
        'Latin': (0x0041, 0x024F),
        'Arabic': (0x0600, 0x06FF),
        'Cyrillic': (0x0400, 0x04FF),
        'CJK': (0x4E00, 0x9FFF),
        'Hiragana': (0x3040, 0x309F),
        'Katakana': (0x30A0, 0x30FF),
        'Hangul': (0xAC00, 0xD7AF),
        'Devanagari': (0x0900, 0x097F),
        'Hebrew': (0x0590, 0x05FF),
        'Greek': (0x0370, 0x03FF),
        'Thai': (0x0E00, 0x0E7F),
    }
    
    # Script to common language mapping
    SCRIPT_TO_LANGS = {
        'Latin': [('en', 0.3), ('es', 0.15), ('fr', 0.15), ('de', 0.1)],
        'Arabic': [('ar', 0.6), ('fa', 0.3), ('ur', 0.1)],
        'Cyrillic': [('ru', 0.6), ('uk', 0.2), ('bg', 0.1)],
        'CJK': [('zh', 0.6), ('ja', 0.2), ('ko', 0.2)],
        'Hiragana': [('ja', 1.0)],
        'Katakana': [('ja', 1.0)],
        'Hangul': [('ko', 1.0)],
        'Devanagari': [('hi', 0.7), ('ne', 0.2), ('mr', 0.1)],
        'Hebrew': [('he', 1.0)],
        'Greek': [('el', 1.0)],
        'Thai': [('th', 1.0)],
    }
    
    def detect(self, text: str, k: int = 3) -> List[Tuple[str, float]]:
        """
        Simple script-based language detection.
        
        Args:
            text: Input text
            k: Number of predictions to return
            
        Returns:
            List of (language_code, probability) tuples
        """
        if not text or not text.strip():
            return []
        
        # Count characters per script
        script_counts = Counter()
        total_chars = 0
        
        for char in text:
            if char.isspace():
                continue
            
            char_code = ord(char)
            for script_name, (start, end) in self.SCRIPT_RANGES.items():
                if start <= char_code <= end:
                    script_counts[script_name] += 1
                    total_chars += 1
                    break
        
        if not script_counts:
            return [('unknown', 0.0)]
        
        # Get dominant script
        dominant_script = script_counts.most_common(1)[0][0]
        
        # Return common languages for that script
        predictions = self.SCRIPT_TO_LANGS.get(
            dominant_script,
            [('unknown', 0.0)]
        )
        
        return predictions[:k]

