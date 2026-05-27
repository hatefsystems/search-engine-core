"""
Universal Language Detection - Task 01.2

FastText-based language detection supporting 176+ languages.
Scalable to 250+ languages through custom model training.

Performance: 5000+ detections/second
Accuracy: ≥95% on diverse corpus
"""

import unicodedata
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Tuple, Optional

from shared.logger import setup_logger

logger = setup_logger(__name__)


@dataclass
class LanguageInfo:
    """
    Language detection result with metadata.
    
    Attributes:
        language_code: ISO 639-1 code (e.g., "fa", "en", "zh")
        script_code: ISO 15924 code (e.g., "Arab", "Latn", "Hans")
        confidence: Detection confidence score (0.0-1.0)
        is_mixed_content: True if multiple languages detected
        detected_languages: All detected languages with scores
        detection_method: Method used ("fasttext" or "ngram")
    """
    language_code: str
    script_code: str
    confidence: float
    is_mixed_content: bool = False
    detected_languages: List[Tuple[str, float]] = field(default_factory=list)
    detection_method: str = "fasttext"


def detect_script(text: str) -> str:
    """
    Detect primary script of text using Unicode properties.
    
    Args:
        text: Input text
        
    Returns:
        ISO 15924 script code (e.g., 'Arab', 'Latn', 'Hans')
    """
    if not text:
        return "Zyyy"  # Common
    
    # Count characters per script
    script_counts = {}
    for char in text:
        if char.isspace() or not char.isalnum():
            continue
        try:
            script = unicodedata.name(char).split()[0]
            script_counts[script] = script_counts.get(script, 0) + 1
        except ValueError:
            continue
    
    if not script_counts:
        return "Zyyy"
    
    # Get most common script
    primary_script = max(script_counts, key=script_counts.get)
    
    # Map to ISO 15924 codes
    script_map = {
        'ARABIC': 'Arab',
        'LATIN': 'Latn',
        'CYRILLIC': 'Cyrl',
        'CJK': 'Hans',
        'HIRAGANA': 'Jpan',
        'KATAKANA': 'Jpan',
        'HANGUL': 'Kore',
        'DEVANAGARI': 'Deva',
        'HEBREW': 'Hebr',
        'GREEK': 'Grek',
        'THAI': 'Thai',
        'ARMENIAN': 'Armn',
        'GEORGIAN': 'Geor',
        'BENGALI': 'Beng',
        'GUJARATI': 'Gujr',
        'TAMIL': 'Taml',
        'TELUGU': 'Telu',
        'KANNADA': 'Knda',
        'MALAYALAM': 'Mlym',
        'SINHALA': 'Sinh',
        'MYANMAR': 'Mymr',
        'KHMER': 'Khmr',
        'LAO': 'Laoo',
        'TIBETAN': 'Tibt',
        'ETHIOPIC': 'Ethi',
    }
    
    return script_map.get(primary_script, 'Zyyy')


class UniversalLanguageDetector:
    """
    Universal language detector with FastText backend.
    
    Features:
    - 176 languages supported out-of-box
    - Scalable to 250+ languages via custom training
    - Confidence scoring
    - Mixed-language detection
    - Script detection (ISO 15924)
    - Fallback for short texts
    
    Performance:
    - Throughput: 5000+ detections/sec
    - Latency: <5ms per text
    - Accuracy: ≥95%
    
    Example:
        >>> detector = UniversalLanguageDetector()
        >>> result = detector.detect("این یک متن فارسی است")
        >>> print(result.language_code, result.confidence)
        fa 0.98
    """
    
    def __init__(
        self,
        model_path: Optional[Path] = None,
        use_fallback: bool = True,
        confidence_threshold: float = 0.7,
        top_k: int = 3
    ):
        """
        Initialize language detector.
        
        Args:
            model_path: Path to FastText model (default: models/lid.176.ftz)
            use_fallback: Use n-gram fallback for short texts
            confidence_threshold: Minimum confidence for reliable detection
            top_k: Number of top predictions to consider
        """
        self.confidence_threshold = confidence_threshold
        self.top_k = top_k
        self.use_fallback = use_fallback
        
        # Lazy load FastText detector
        self._fasttext_detector = None
        self._model_path = model_path
        
        # Lazy load n-gram detector
        self._ngram_detector = None
        
        logger.info(
            "Language detector initialized",
            model_path=str(model_path) if model_path else "default",
            use_fallback=use_fallback,
            confidence_threshold=confidence_threshold
        )
    
    @property
    def fasttext_detector(self):
        """Lazy load FastText detector."""
        if self._fasttext_detector is None:
            from .fasttext_detector import FastTextDetector
            self._fasttext_detector = FastTextDetector(self._model_path)
        return self._fasttext_detector
    
    @property
    def ngram_detector(self):
        """Lazy load n-gram detector."""
        if self.use_fallback and self._ngram_detector is None:
            from .ngram_detector import NgramDetector
            self._ngram_detector = NgramDetector()
        return self._ngram_detector
    
    def detect(self, text: str) -> LanguageInfo:
        """
        Detect language of text with confidence scoring.
        
        Args:
            text: Input text (any language)
            
        Returns:
            LanguageInfo with language code, script, and confidence
            
        Raises:
            ValueError: If text is None or empty
        """
        if text is None:
            raise ValueError("Input text cannot be None")
        
        if not text.strip():
            return LanguageInfo(
                language_code="unknown",
                script_code="Zyyy",
                confidence=0.0,
                is_mixed_content=False,
                detected_languages=[],
                detection_method="none"
            )
        
        # Detect script first
        script = detect_script(text)
        
        # Choose detection method based on text length
        text_len = len(text.strip())
        
        try:
            if text_len < 20 and self.use_fallback:
                # Use n-gram for very short text
                predictions = self.ngram_detector.detect(text, k=self.top_k)
                method = "ngram"
            else:
                # Use FastText (primary method)
                predictions = self.fasttext_detector.detect(text, k=self.top_k)
                method = "fasttext"
            
            if not predictions:
                return LanguageInfo(
                    language_code="unknown",
                    script_code=script,
                    confidence=0.0,
                    is_mixed_content=False,
                    detected_languages=[],
                    detection_method=method
                )
            
            # Primary language is top prediction
            primary_lang, primary_conf = predictions[0]
            
            # Check for mixed content
            is_mixed = (
                len(predictions) > 1 and
                predictions[1][1] > 0.2  # Second language has >20% confidence
            )
            
            logger.debug(
                "Language detected",
                text_preview=text[:50],
                language=primary_lang,
                confidence=primary_conf,
                script=script,
                is_mixed=is_mixed,
                method=method
            )
            
            return LanguageInfo(
                language_code=primary_lang,
                script_code=script,
                confidence=primary_conf,
                is_mixed_content=is_mixed,
                detected_languages=predictions,
                detection_method=method
            )
            
        except Exception as e:
            logger.error(
                "Language detection failed",
                error=str(e),
                text_preview=text[:100]
            )
            # Return unknown on error (zero crashes requirement)
            return LanguageInfo(
                language_code="unknown",
                script_code=script,
                confidence=0.0,
                is_mixed_content=False,
                detected_languages=[],
                detection_method="error"
            )
    
    def detect_batch(self, texts: List[str]) -> List[LanguageInfo]:
        """
        Batch detect languages for multiple texts.
        
        Args:
            texts: List of input texts
            
        Returns:
            List of LanguageInfo objects
        """
        results = []
        for text in texts:
            try:
                result = self.detect(text)
                results.append(result)
            except Exception as e:
                logger.error("Batch detection error", error=str(e))
                results.append(LanguageInfo(
                    language_code="unknown",
                    script_code="Zyyy",
                    confidence=0.0,
                    is_mixed_content=False,
                    detected_languages=[],
                    detection_method="error"
                ))
        
        return results
    
    def is_reliable(self, language_info: LanguageInfo) -> bool:
        """
        Check if detection is reliable based on confidence threshold.
        
        Args:
            language_info: Detection result
            
        Returns:
            True if confidence >= threshold
        """
        return language_info.confidence >= self.confidence_threshold


# Convenience functions
def detect_language(text: str, **kwargs) -> LanguageInfo:
    """
    Convenience function for one-off language detection.
    
    Args:
        text: Input text
        **kwargs: Arguments passed to UniversalLanguageDetector
        
    Returns:
        LanguageInfo with detection results
    """
    detector = UniversalLanguageDetector(**kwargs)
    return detector.detect(text)


def detect_language_batch(texts: List[str], **kwargs) -> List[LanguageInfo]:
    """
    Convenience function for batch language detection.
    
    Args:
        texts: List of input texts
        **kwargs: Arguments passed to UniversalLanguageDetector
        
    Returns:
        List of LanguageInfo objects
    """
    detector = UniversalLanguageDetector(**kwargs)
    return detector.detect_batch(texts)

