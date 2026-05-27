"""
Cyrillic Script Processor - Task 01.3

Handles Cyrillic script text processing:
- Variant unification (ё → е, configurable)
- Case folding
- Language-specific character preservation
"""

from typing import Set

from shared.logger import setup_logger
from typing import TYPE_CHECKING

# Import ProcessedText - avoid circular import
if TYPE_CHECKING:
    from .__init__ import ProcessedText
else:
    from . import ProcessedText

logger = setup_logger(__name__)

# Cyrillic ё (U+0451) and е (U+0435)
CYRILLIC_YO = '\u0451'  # ё
CYRILLIC_E = '\u0435'   # е

# Languages that should preserve ё
PRESERVE_YO_LANGUAGES: Set[str] = {
    'ru',  # Russian (though normalization is common)
    # Add other languages if needed
}

# Ukrainian/Belarusian specific characters that should be preserved
UKRAINIAN_SPECIFIC = {
    '\u0456',  # і (Ukrainian/Belarusian i)
    '\u0457',  # ї (Ukrainian yi)
    '\u0491',  # ґ (Ukrainian ghe)
}

BELARUSIAN_SPECIFIC = {
    '\u0456',  # і (also Belarusian)
}


def unify_cyrillic_variants(
    text: str,
    normalize_yo: bool = True,
    language_code: str = ""
) -> str:
    """
    Unify Cyrillic character variants.
    
    Main operation: ё → е normalization (configurable).
    Preserves language-specific characters for Ukrainian/Belarusian.
    
    Args:
        text: Input text
        normalize_yo: If True, normalize ё to е
        language_code: ISO 639-1 language code (ru, uk, be, etc.)
        
    Returns:
        Text with variants unified
    """
    if not text:
        return text
    
    result = []
    normalized_count = 0
    
    # Check if we should preserve ё for this language
    should_preserve_yo = language_code in PRESERVE_YO_LANGUAGES and not normalize_yo
    
    for char in text:
        # Normalize ё to е (unless preserving)
        if char == CYRILLIC_YO and not should_preserve_yo:
            result.append(CYRILLIC_E)
            normalized_count += 1
        else:
            result.append(char)
    
    if normalized_count > 0:
        logger.debug(f"Normalized {normalized_count} ё → е")
    
    return ''.join(result)


def process_cyrillic(
    text: str,
    language_code: str,
    normalize_yo: bool = True
) -> ProcessedText:
    """
    Process Cyrillic script text.
    
    Args:
        text: Input text
        language_code: ISO 639-1 language code (ru, uk, be, bg, etc.)
        normalize_yo: If True, normalize ё to е
        
    Returns:
        ProcessedText with processed text and metadata
    """
    if not text:
        return ProcessedText(
            text="",
            original="",
            script_code="Cyrl",
            language_code=language_code,
            applied_rules=[],
            confidence=0.0
        )
    
    original_text = text
    applied_rules = []
    
    # Step 1: Unify variants (ё → е)
    if normalize_yo:
        text = unify_cyrillic_variants(text, normalize_yo=True, language_code=language_code)
        applied_rules.append("unify_variants")
    else:
        applied_rules.append("preserve_yo")
    
    # Step 2: Case folding (Unicode case folding handles Cyrillic properly)
    # We don't need special handling here - standard case folding works
    
    logger.debug(
        f"Processed Cyrillic text",
        language=language_code,
        original_length=len(original_text),
        processed_length=len(text),
        rules_applied=applied_rules
    )
    
    return ProcessedText(
        text=text,
        original=original_text,
        script_code="Cyrl",
        language_code=language_code,
        applied_rules=applied_rules,
        confidence=0.0  # Will be set by caller
    )
