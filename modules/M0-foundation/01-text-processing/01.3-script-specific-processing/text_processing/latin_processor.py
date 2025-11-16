"""
Latin Script Processor - Task 01.3

Handles Latin script text processing:
- Diacritic normalization (é → e, configurable)
- Ligature handling (æ, œ semantic preservation)
- Case folding
"""

import unicodedata
from typing import List, Set

from shared.logger import setup_logger
from typing import TYPE_CHECKING

# Import ProcessedText - avoid circular import
if TYPE_CHECKING:
    from .__init__ import ProcessedText
else:
    from . import ProcessedText

logger = setup_logger(__name__)

# Languages that should preserve diacritics
PRESERVE_DIACRITICS_LANGUAGES: Set[str] = {
    'fr',  # French (é, è, ê, etc.)
    'es',  # Spanish (ñ, á, é, etc.)
    'pt',  # Portuguese (ã, ç, etc.)
    'de',  # German (ä, ö, ü, ß)
    'it',  # Italian (à, è, ì, ò, ù)
    'pl',  # Polish (ą, ć, ę, ł, ń, ó, ś, ź, ż)
    'cs',  # Czech (á, č, ď, é, ě, í, ň, ó, ř, š, ť, ú, ů, ý, ž)
    'sk',  # Slovak (similar to Czech)
    'hu',  # Hungarian (á, é, í, ó, ö, ő, ú, ü, ű)
    'ro',  # Romanian (ă, â, î, ș, ț)
    'tr',  # Turkish (ç, ğ, ı, ö, ş, ü)
    'vi',  # Vietnamese (extensive diacritics)
    'is',  # Icelandic (á, é, í, ó, ú, ý, þ, æ, ö)
    'da',  # Danish (æ, ø, å)
    'no',  # Norwegian (æ, ø, å)
    'sv',  # Swedish (ä, ö, å)
    'fi',  # Finnish (ä, ö)
    'et',  # Estonian (ä, ö, õ, ü)
    'lv',  # Latvian (ā, č, ē, ģ, ī, ķ, ļ, ņ, ō, ŗ, š, ū, ž)
    'lt',  # Lithuanian (ą, č, ę, ė, į, š, ų, ū, ž)
}

# Semantic ligatures that should be preserved
SEMANTIC_LIGATURES = {
    'æ': 'ae',  # Latin ligature (e.g., "encyclopædia")
    'œ': 'oe',  # Latin ligature (e.g., "cœur" in French)
    'Æ': 'AE',
    'Œ': 'OE',
}

# Non-semantic ligatures (can be normalized)
NON_SEMANTIC_LIGATURES = {
    'ﬁ': 'fi',
    'ﬂ': 'fl',
    'ﬀ': 'ff',
    'ﬃ': 'ffi',
    'ﬄ': 'ffl',
    'ﬅ': 'st',
}


def normalize_diacritics(
    text: str,
    preserve_for_languages: List[str] = None
) -> str:
    """
    Normalize diacritics in Latin text (é → e, ñ → n, etc.).
    
    Args:
        text: Input text
        preserve_for_languages: List of language codes to preserve diacritics for
        
    Returns:
        Text with diacritics normalized (or preserved based on language)
    """
    if not text:
        return text
    
    preserve_set = set(preserve_for_languages) if preserve_for_languages else set()
    
    # Check if we should preserve diacritics
    should_preserve = bool(preserve_set)
    
    if should_preserve:
        # Preserve diacritics for specified languages
        return text
    
    # Normalize diacritics using Unicode NFD + remove combining marks
    normalized = []
    removed_count = 0
    
    for char in text:
        # Decompose character (é → e + combining acute)
        decomposed = unicodedata.normalize('NFD', char)
        
        # Keep base character, remove combining marks
        base_chars = []
        for c in decomposed:
            category = unicodedata.category(c)
            if category != 'Mn':  # Mn = Nonspacing Mark (diacritics)
                base_chars.append(c)
        
        if len(base_chars) < len(decomposed):
            removed_count += 1
        
        normalized.extend(base_chars)
    
    if removed_count > 0:
        logger.debug(f"Normalized {removed_count} diacritics")
    
    return ''.join(normalized)


def handle_ligatures(text: str, preserve_semantic: bool = True) -> str:
    """
    Handle ligatures in Latin text.
    
    Args:
        text: Input text
        preserve_semantic: If True, preserve semantic ligatures (æ, œ)
        
    Returns:
        Text with ligatures handled
    """
    if not text:
        return text
    
    result = []
    normalized_count = 0
    
    for char in text:
        if char in SEMANTIC_LIGATURES:
            if preserve_semantic:
                # Preserve semantic ligatures
                result.append(char)
            else:
                # Normalize semantic ligatures
                result.append(SEMANTIC_LIGATURES[char])
                normalized_count += 1
        elif char in NON_SEMANTIC_LIGATURES:
            # Always normalize non-semantic ligatures
            result.append(NON_SEMANTIC_LIGATURES[char])
            normalized_count += 1
        else:
            result.append(char)
    
    if normalized_count > 0:
        logger.debug(f"Normalized {normalized_count} ligatures")
    
    return ''.join(result)


def process_latin(
    text: str,
    language_code: str,
    normalize_diacritics_flag: bool = False,
    preserve_semantic_ligatures: bool = True
) -> ProcessedText:
    """
    Process Latin script text.
    
    Args:
        text: Input text
        language_code: ISO 639-1 language code (en, fr, es, etc.)
        normalize_diacritics_flag: If True, normalize diacritics (é → e)
        preserve_semantic_ligatures: If True, preserve æ, œ ligatures
        
    Returns:
        ProcessedText with processed text and metadata
    """
    if not text:
        return ProcessedText(
            text="",
            original="",
            script_code="Latn",
            language_code=language_code,
            applied_rules=[],
            confidence=0.0
        )
    
    original_text = text
    applied_rules = []
    
    # Step 1: Handle ligatures
    text = handle_ligatures(text, preserve_semantic=preserve_semantic_ligatures)
    applied_rules.append("handle_ligatures")
    
    # Step 2: Normalize diacritics (if requested and language doesn't require preservation)
    should_preserve = language_code in PRESERVE_DIACRITICS_LANGUAGES
    
    if normalize_diacritics_flag and not should_preserve:
        text = normalize_diacritics(text)
        applied_rules.append("normalize_diacritics")
    else:
        applied_rules.append("preserve_diacritics")
    
    # Step 3: Case folding (Unicode handles Latin properly)
    # No special handling needed
    
    logger.debug(
        f"Processed Latin text",
        language=language_code,
        original_length=len(original_text),
        processed_length=len(text),
        rules_applied=applied_rules
    )
    
    return ProcessedText(
        text=text,
        original=original_text,
        script_code="Latn",
        language_code=language_code,
        applied_rules=applied_rules,
        confidence=0.0  # Will be set by caller
    )
