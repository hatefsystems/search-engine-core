"""
CJK Script Processor - Task 01.3

Handles Chinese, Japanese, and Korean text processing:
- Chinese: jieba word segmentation
- Japanese: Tokenization (MeCab optional)
- Korean: Hangul syllable handling and word boundaries
"""

import re
from typing import List, Optional

from shared.logger import setup_logger
from typing import TYPE_CHECKING

# Import ProcessedText - avoid circular import
if TYPE_CHECKING:
    from .__init__ import ProcessedText
else:
    from . import ProcessedText

logger = setup_logger(__name__)

# Lazy load jieba (heavy dependency)
_jieba = None
_mecab = None


def _get_jieba():
    """Lazy load jieba for Chinese segmentation."""
    global _jieba
    if _jieba is None:
        try:
            import jieba_fast as jieba
            _jieba = jieba
            logger.info("Loaded jieba-fast for Chinese segmentation")
        except ImportError:
            try:
                import jieba
                _jieba = jieba
                logger.warning("Using jieba (slow) instead of jieba-fast")
            except ImportError:
                logger.error("jieba not available - Chinese segmentation disabled")
                raise ImportError("jieba or jieba-fast required for Chinese processing")
    return _jieba


def _get_mecab():
    """Lazy load MeCab for Japanese tokenization (optional)."""
    global _mecab
    if _mecab is None:
        try:
            import MeCab
            _mecab = MeCab.Tagger("-Owakati")
            logger.info("Loaded MeCab for Japanese tokenization")
        except ImportError:
            logger.debug("MeCab not available - using regex-based Japanese tokenization")
            _mecab = False  # Mark as unavailable
    return _mecab


def segment_chinese(text: str) -> List[str]:
    """
    Segment Chinese text into words using jieba.
    
    Args:
        text: Chinese text
        
    Returns:
        List of segmented words
    """
    jieba = _get_jieba()
    words = jieba.cut(text, cut_all=False)
    return list(words)


def segment_japanese(text: str) -> List[str]:
    """
    Segment Japanese text into tokens.
    
    Uses MeCab if available, otherwise falls back to regex-based segmentation.
    
    Args:
        text: Japanese text
        
    Returns:
        List of tokens
    """
    mecab = _get_mecab()
    
    if mecab and mecab is not False:
        # Use MeCab for accurate tokenization
        try:
            tokens = mecab.parse(text).strip().split()
            return tokens
        except Exception as e:
            logger.warning(f"MeCab tokenization failed: {e}, falling back to regex")
    
    # Fallback: regex-based segmentation
    # Split by Hiragana, Katakana, Kanji boundaries
    # This is a simple heuristic and not as accurate as MeCab
    tokens = []
    current_token = []
    
    for char in text:
        # Check script type
        if '\u3040' <= char <= '\u309F':  # Hiragana
            if current_token and not any('\u3040' <= c <= '\u309F' for c in current_token):
                tokens.append(''.join(current_token))
                current_token = [char]
            else:
                current_token.append(char)
        elif '\u30A0' <= char <= '\u30FF':  # Katakana
            if current_token and not any('\u30A0' <= c <= '\u30FF' for c in current_token):
                tokens.append(''.join(current_token))
                current_token = [char]
            else:
                current_token.append(char)
        elif '\u4E00' <= char <= '\u9FAF':  # CJK Unified Ideographs (Kanji)
            if current_token and not any('\u4E00' <= c <= '\u9FAF' for c in current_token):
                tokens.append(''.join(current_token))
                current_token = [char]
            else:
                current_token.append(char)
        else:
            # Punctuation, spaces, etc.
            if current_token:
                tokens.append(''.join(current_token))
                current_token = []
            if not char.isspace():
                tokens.append(char)
    
    if current_token:
        tokens.append(''.join(current_token))
    
    return [t for t in tokens if t.strip()]


def segment_korean(text: str) -> List[str]:
    """
    Segment Korean text into words.
    
    Korean uses spaces for word boundaries, but we also handle
    Hangul syllables and compound words.
    
    Args:
        text: Korean text
        
    Returns:
        List of words/syllables
    """
    # Korean typically uses spaces for word boundaries
    # Split by spaces first
    words = text.split()
    
    # Further segment compound words if needed
    # For now, we keep space-separated words
    # More sophisticated segmentation could use KoNLPy or similar
    
    return words


def get_word_boundaries(text: str, words: List[str]) -> List[int]:
    """
    Get character positions of word boundaries from segmented words.
    
    Args:
        text: Original text
        words: List of segmented words
        
    Returns:
        List of character positions marking word boundaries
    """
    boundaries = [0]
    current_pos = 0
    
    for word in words:
        # Find word in text starting from current position
        pos = text.find(word, current_pos)
        if pos != -1:
            boundaries.append(pos + len(word))
            current_pos = pos + len(word)
        else:
            # Word not found, advance by word length
            current_pos += len(word)
            boundaries.append(current_pos)
    
    # Remove duplicates and sort
    boundaries = sorted(set(boundaries))
    
    return boundaries


def process_cjk(
    text: str,
    language_code: str,
    script_code: str
) -> ProcessedText:
    """
    Process CJK script text (Chinese, Japanese, Korean).
    
    Args:
        text: Input text
        language_code: ISO 639-1 language code (zh, ja, ko)
        script_code: ISO 15924 script code (Hans, Hant, Jpan, Kore)
        
    Returns:
        ProcessedText with segmented text and word boundaries
    """
    if not text:
        return ProcessedText(
            text="",
            original="",
            script_code=script_code,
            language_code=language_code,
            applied_rules=[],
            word_boundaries=[],
            confidence=0.0
        )
    
    original_text = text
    applied_rules = []
    words = []
    word_boundaries = []
    
    # Process based on language
    if language_code == "zh" or script_code in ("Hans", "Hant"):
        # Chinese segmentation
        words = segment_chinese(text)
        applied_rules.append("chinese_segmentation")
        word_boundaries = get_word_boundaries(text, words)
        
    elif language_code == "ja" or script_code == "Jpan":
        # Japanese tokenization
        words = segment_japanese(text)
        applied_rules.append("japanese_tokenization")
        word_boundaries = get_word_boundaries(text, words)
        
    elif language_code == "ko" or script_code == "Kore":
        # Korean word segmentation
        words = segment_korean(text)
        applied_rules.append("korean_segmentation")
        word_boundaries = get_word_boundaries(text, words)
    
    else:
        # Unknown CJK language, try Chinese as default
        logger.warning(f"Unknown CJK language {language_code}, using Chinese segmentation")
        words = segment_chinese(text)
        applied_rules.append("chinese_segmentation_fallback")
        word_boundaries = get_word_boundaries(text, words)
    
    # Join words with spaces for processed text
    processed_text = ' '.join(words)
    
    logger.debug(
        f"Processed CJK text",
        language=language_code,
        script=script_code,
        original_length=len(original_text),
        word_count=len(words),
        rules_applied=applied_rules
    )
    
    return ProcessedText(
        text=processed_text,
        original=original_text,
        script_code=script_code,
        language_code=language_code,
        applied_rules=applied_rules,
        word_boundaries=word_boundaries,
        confidence=0.0  # Will be set by caller
    )
