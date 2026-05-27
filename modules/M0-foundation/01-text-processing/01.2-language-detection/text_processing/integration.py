"""
Integration Pipeline - Task 01.1 + Task 01.2

Combines Unicode normalization with language detection for complete text processing.

Pipeline: Raw Text → Normalize (01.1) → Detect Language (01.2) → Processed Result

Example:
    >>> pipeline = TextProcessingPipeline()
    >>> result = pipeline.process("روباه قهوه‌ای   سریع")
    >>> print(result.normalized_text, result.language, result.script)
    روباه قهوه‌ای سریع fa Arab
"""

import sys
from pathlib import Path
from dataclasses import dataclass
from typing import Optional

# Add Task 01.1 to path
task_01_1_path = Path(__file__).parent.parent.parent / "01.1-unicode-normalization"
if task_01_1_path.exists():
    sys.path.insert(0, str(task_01_1_path))

try:
    from text_processing import normalize_universal, NormalizedText
    NORMALIZER_AVAILABLE = True
except ImportError:
    NORMALIZER_AVAILABLE = False

from .language_detector import UniversalLanguageDetector, LanguageInfo
from shared.logger import setup_logger

logger = setup_logger(__name__)


@dataclass
class ProcessedText:
    """
    Complete text processing result.
    
    Combines normalization and language detection results.
    
    Attributes:
        original_text: Original input text
        normalized_text: Normalized text (Task 01.1)
        language_code: Detected language (ISO 639-1)
        script_code: Detected script (ISO 15924)
        confidence: Language detection confidence
        is_mixed_content: Multiple languages detected
        normalization_changes: List of normalization changes applied
        detected_languages: All detected languages with scores
    """
    original_text: str
    normalized_text: str
    language_code: str
    script_code: str
    confidence: float
    is_mixed_content: bool = False
    normalization_changes: list = None
    detected_languages: list = None
    detection_method: str = "fasttext"


class TextProcessingPipeline:
    """
    Complete text processing pipeline: Normalize + Detect Language.
    
    Features:
    - Unicode normalization (Task 01.1)
    - Language detection (Task 01.2)
    - Script detection (ISO 15924)
    - Error handling and fallback
    
    Example:
        >>> pipeline = TextProcessingPipeline()
        >>> result = pipeline.process("Hello   World")
        >>> print(result.normalized_text, result.language_code)
        Hello World en
    """
    
    def __init__(
        self,
        use_normalizer: bool = True,
        model_path: Optional[Path] = None,
        confidence_threshold: float = 0.7
    ):
        """
        Initialize processing pipeline.
        
        Args:
            use_normalizer: Use Unicode normalizer from Task 01.1
            model_path: Path to FastText model (optional)
            confidence_threshold: Minimum confidence for language detection
        """
        self.use_normalizer = use_normalizer and NORMALIZER_AVAILABLE
        
        if self.use_normalizer and not NORMALIZER_AVAILABLE:
            logger.warning(
                "Normalizer not available. "
                "Install Task 01.1 or set use_normalizer=False"
            )
        
        # Initialize language detector
        self.detector = UniversalLanguageDetector(
            model_path=model_path,
            confidence_threshold=confidence_threshold
        )
        
        logger.info(
            "Pipeline initialized",
            use_normalizer=self.use_normalizer,
            confidence_threshold=confidence_threshold
        )
    
    def process(self, text: str) -> ProcessedText:
        """
        Process text through complete pipeline.
        
        Args:
            text: Raw input text
            
        Returns:
            ProcessedText with normalization and language detection results
        """
        if text is None:
            raise ValueError("Input text cannot be None")
        
        original_text = text
        normalization_changes = []
        
        # Step 1: Normalize text (Task 01.1)
        if self.use_normalizer:
            try:
                normalized = normalize_universal(text)
                text = normalized.text
                normalization_changes = normalized.changes
                
                logger.debug(
                    "Text normalized",
                    original_length=len(original_text),
                    normalized_length=len(text),
                    num_changes=len(normalization_changes)
                )
            except Exception as e:
                logger.error("Normalization failed", error=str(e))
                # Continue with original text
                text = original_text
        
        # Step 2: Detect language (Task 01.2)
        try:
            lang_info = self.detector.detect(text)
            
            logger.debug(
                "Language detected",
                language=lang_info.language_code,
                confidence=lang_info.confidence,
                script=lang_info.script_code
            )
        except Exception as e:
            logger.error("Language detection failed", error=str(e))
            # Return with unknown language
            return ProcessedText(
                original_text=original_text,
                normalized_text=text,
                language_code="unknown",
                script_code="Zyyy",
                confidence=0.0,
                is_mixed_content=False,
                normalization_changes=normalization_changes,
                detected_languages=[],
                detection_method="error"
            )
        
        # Step 3: Combine results
        return ProcessedText(
            original_text=original_text,
            normalized_text=text,
            language_code=lang_info.language_code,
            script_code=lang_info.script_code,
            confidence=lang_info.confidence,
            is_mixed_content=lang_info.is_mixed_content,
            normalization_changes=normalization_changes,
            detected_languages=lang_info.detected_languages,
            detection_method=lang_info.detection_method
        )
    
    def process_batch(self, texts: list) -> list:
        """
        Process multiple texts through pipeline.
        
        Args:
            texts: List of raw input texts
            
        Returns:
            List of ProcessedText results
        """
        results = []
        for text in texts:
            try:
                result = self.process(text)
                results.append(result)
            except Exception as e:
                logger.error("Batch processing error", error=str(e))
                results.append(ProcessedText(
                    original_text=text,
                    normalized_text=text,
                    language_code="unknown",
                    script_code="Zyyy",
                    confidence=0.0,
                    normalization_changes=[],
                    detected_languages=[]
                ))
        
        return results


# Convenience function
def process_text(text: str, **kwargs) -> ProcessedText:
    """
    Convenience function for one-off text processing.
    
    Args:
        text: Input text
        **kwargs: Arguments passed to TextProcessingPipeline
        
    Returns:
        ProcessedText with complete processing results
    """
    pipeline = TextProcessingPipeline(**kwargs)
    return pipeline.process(text)

