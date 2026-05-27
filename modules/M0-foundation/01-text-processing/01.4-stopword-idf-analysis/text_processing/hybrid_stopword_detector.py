"""
Hybrid Stopword Detector - Two-Layer Architecture

Layer 1: IDF-based detection (universal, 100+ languages)
Layer 2: Stanza POS tagging (enhanced, 60+ languages)

This hybrid approach ensures universal coverage with enhanced accuracy
where grammar models are available.
"""

import json
from dataclasses import dataclass, asdict
from typing import List, Dict, Optional, Set
import redis
from pathlib import Path

from .idf_analyzer import IDFAnalyzer, IDFScore
from .stopword_detector import StopwordDetector, StopwordInfo
from .stanza_pos_tagger import StanzaPOSTagger, get_global_tagger, STANZA_AVAILABLE
from shared.logger import setup_logger

logger = setup_logger(__name__)


@dataclass
class HybridStopwordInfo:
    """Extended stopword info with grammar verification"""
    term: str
    is_stopword: bool
    confidence: float
    language: str
    idf: float = 0.0
    document_frequency: int = 0
    pos_tag: Optional[str] = None
    grammar_verified: bool = False
    detection_method: str = "idf"  # 'idf', 'hybrid', 'bootstrap'
    
    def __repr__(self) -> str:
        grammar_info = f", POS: {self.pos_tag}" if self.pos_tag else ""
        return (
            f"HybridStopwordInfo(term='{self.term}', is_stopword={self.is_stopword}, "
            f"confidence={self.confidence:.4f}, language='{self.language}'{grammar_info}, "
            f"method={self.detection_method})"
        )
    
    def to_dict(self) -> Dict:
        """Convert to dictionary for Redis storage"""
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: Dict) -> 'HybridStopwordInfo':
        """Create from dictionary"""
        return cls(**data)


class HybridStopwordDetector:
    """
    Hybrid Stopword Detector with Two-Layer Architecture
    
    Architecture:
        Layer 1 (Universal): IDF-based detection
            - Works for ALL languages (100+)
            - Fast, corpus-based
            - ~90% accuracy
            
        Layer 2 (Enhanced): Stanza POS tagging
            - Grammar-aware verification
            - 60+ languages with models
            - ~95% accuracy
            - Automatic fallback to Layer 1 if unavailable
    
    Features:
    - Universal coverage (any language works)
    - Enhanced accuracy for supported languages
    - Graceful degradation (Stanza unavailable → IDF-only)
    - Fast Redis lookup (<1ms)
    - Confidence boosting with grammar verification
    - POS metadata for debugging
    
    Usage:
        detector = HybridStopwordDetector(
            redis_url="redis://localhost:6379"
        )
        
        # Check stopword with grammar verification (if available)
        result = detector.is_stopword("the", "en")
        print(result.is_stopword)        # True
        print(result.confidence)         # 0.98
        print(result.pos_tag)           # 'DET'
        print(result.grammar_verified)  # True
        
        # Get full info
        info = detector.get_stopword_info("API", "en")
        print(info.confidence)          # 0.45 (lowered by grammar)
        print(info.pos_tag)            # 'NOUN' (not a stopword POS)
        
        # Check if language has grammar support
        print(detector.supports_grammar_verification("fa"))  # True
        print(detector.supports_grammar_verification("tlh"))  # False (Klingon)
    """
    
    def __init__(
        self,
        redis_url: str = "redis://localhost:6379",
        redis_db: int = 0,
        default_threshold: float = 0.8,
        use_bootstrap: bool = True,
        bootstrap_path: Optional[Path] = None,
        enable_stanza: bool = True,
        confidence_boost: float = 1.2,
        confidence_penalty: float = 0.7,
        stanza_tagger: Optional[StanzaPOSTagger] = None
    ):
        """
        Initialize Hybrid Stopword Detector
        
        Args:
            redis_url: Redis connection URL
            redis_db: Redis database number
            default_threshold: Default confidence threshold
            use_bootstrap: Use NLTK bootstrap lists as fallback
            bootstrap_path: Path to bootstrap stopword lists
            enable_stanza: Enable Layer 2 (Stanza POS tagging)
            confidence_boost: Multiplier for grammar-verified stopwords (default: 1.2 = 20% boost)
            confidence_penalty: Multiplier for grammar-rejected terms (default: 0.7 = 30% reduction)
            stanza_tagger: Custom StanzaPOSTagger instance (optional)
        """
        # Layer 1: IDF-based detector (always active)
        self.idf_detector = StopwordDetector(
            redis_url=redis_url,
            redis_db=redis_db,
            default_threshold=default_threshold,
            use_bootstrap=use_bootstrap,
            bootstrap_path=bootstrap_path
        )
        
        # Layer 2: Stanza POS tagger (optional)
        self.enable_stanza = enable_stanza and STANZA_AVAILABLE
        self.confidence_boost = confidence_boost
        self.confidence_penalty = confidence_penalty
        
        if self.enable_stanza:
            self.stanza_tagger = stanza_tagger or get_global_tagger()
            logger.info(
                "Hybrid detector initialized with Stanza support",
                supported_languages=len(self.stanza_tagger.get_supported_languages())
            )
        else:
            self.stanza_tagger = None
            if enable_stanza and not STANZA_AVAILABLE:
                logger.warning(
                    "Stanza requested but not available. Using IDF-only mode.",
                    fallback="Layer 1 (IDF) only"
                )
            else:
                logger.info("Hybrid detector in IDF-only mode (Stanza disabled)")
    
    def supports_grammar_verification(self, language: str) -> bool:
        """
        Check if grammar verification (Stanza) is available for language
        
        Args:
            language: ISO 639-1 language code
            
        Returns:
            True if Stanza model available for this language
        """
        if not self.enable_stanza or not self.stanza_tagger:
            return False
        return self.stanza_tagger.is_supported(language)
    
    def _apply_grammar_refinement(
        self,
        base_info: StopwordInfo,
        language: str
    ) -> HybridStopwordInfo:
        """
        Apply Layer 2 (Stanza) grammar refinement to Layer 1 (IDF) result
        
        Args:
            base_info: Result from Layer 1 (IDF)
            language: ISO 639-1 language code
            
        Returns:
            Enhanced result with grammar verification
        """
        term = base_info.term
        
        # If Stanza not available or language not supported, return IDF result
        if not self.supports_grammar_verification(language):
            return HybridStopwordInfo(
                term=term,
                is_stopword=base_info.is_stopword,
                confidence=base_info.confidence,
                language=language,
                idf=base_info.idf,
                document_frequency=base_info.document_frequency,
                pos_tag=None,
                grammar_verified=False,
                detection_method="idf"
            )
        
        # Get POS tag from Stanza
        try:
            pos_tag = self.stanza_tagger.get_pos_tag(term, language)
            
            if pos_tag is None:
                # POS tagging failed, return IDF result
                logger.debug(
                    f"POS tagging failed for '{term}' ({language})",
                    fallback="IDF-only"
                )
                return HybridStopwordInfo(
                    term=term,
                    is_stopword=base_info.is_stopword,
                    confidence=base_info.confidence,
                    language=language,
                    idf=base_info.idf,
                    document_frequency=base_info.document_frequency,
                    pos_tag=None,
                    grammar_verified=False,
                    detection_method="idf"
                )
            
            # Apply grammar-based confidence adjustment
            is_stopword_pos = self.stanza_tagger.is_stopword_pos(pos_tag)
            
            if is_stopword_pos:
                # Grammar confirms stopword → boost confidence
                adjusted_confidence = min(0.99, base_info.confidence * self.confidence_boost)
                is_stopword = True
                logger.debug(
                    f"Grammar verified stopword: '{term}' ({language})",
                    pos=pos_tag,
                    confidence_boost=f"{base_info.confidence:.2f} → {adjusted_confidence:.2f}"
                )
            else:
                # Grammar rejects stopword → reduce confidence
                adjusted_confidence = base_info.confidence * self.confidence_penalty
                is_stopword = adjusted_confidence >= self.idf_detector.default_threshold
                logger.debug(
                    f"Grammar rejected stopword: '{term}' ({language})",
                    pos=pos_tag,
                    confidence_penalty=f"{base_info.confidence:.2f} → {adjusted_confidence:.2f}"
                )
            
            return HybridStopwordInfo(
                term=term,
                is_stopword=is_stopword,
                confidence=adjusted_confidence,
                language=language,
                idf=base_info.idf,
                document_frequency=base_info.document_frequency,
                pos_tag=pos_tag,
                grammar_verified=True,
                detection_method="hybrid"
            )
            
        except Exception as e:
            logger.error(
                f"Grammar verification failed for '{term}' ({language}): {e}",
                fallback="IDF-only"
            )
            return HybridStopwordInfo(
                term=term,
                is_stopword=base_info.is_stopword,
                confidence=base_info.confidence,
                language=language,
                idf=base_info.idf,
                document_frequency=base_info.document_frequency,
                pos_tag=None,
                grammar_verified=False,
                detection_method="idf"
            )
    
    def is_stopword(
        self,
        term: str,
        language: str,
        threshold: Optional[float] = None
    ) -> HybridStopwordInfo:
        """
        Check if term is stopword with hybrid detection
        
        Args:
            term: Word to check
            language: ISO 639-1 language code
            threshold: Custom confidence threshold (overrides default)
            
        Returns:
            HybridStopwordInfo with full metadata
        """
        # Layer 1: Get IDF-based result
        base_result = self.idf_detector.is_stopword(
            term,
            language,
            threshold=threshold
        )
        
        # Layer 2: Apply grammar refinement (if available)
        hybrid_result = self._apply_grammar_refinement(base_result, language)
        
        return hybrid_result
    
    def get_stopword_info(
        self,
        term: str,
        language: str
    ) -> HybridStopwordInfo:
        """
        Get detailed stopword information with grammar metadata
        
        Args:
            term: Word to check
            language: ISO 639-1 language code
            
        Returns:
            HybridStopwordInfo with all available metadata
        """
        return self.is_stopword(term, language)
    
    def get_stopwords(
        self,
        language: str,
        limit: int = 1000,
        grammar_verified_only: bool = False,
        min_confidence: Optional[float] = None
    ) -> List[HybridStopwordInfo]:
        """
        Get stopwords for language with full metadata
        
        Args:
            language: ISO 639-1 language code
            limit: Maximum number of stopwords
            grammar_verified_only: Only return Stanza-verified stopwords
            min_confidence: Minimum confidence threshold
            
        Returns:
            List of HybridStopwordInfo objects
        """
        # Get stopwords from Layer 1 (IDF)
        idf_stopwords = self.idf_detector.get_stopwords(
            language,
            limit=limit,
            min_confidence=min_confidence
        )
        
        # Convert to hybrid results
        hybrid_stopwords = []
        for sw_info in idf_stopwords:
            hybrid_info = self._apply_grammar_refinement(sw_info, language)
            
            # Filter by grammar verification if requested
            if grammar_verified_only and not hybrid_info.grammar_verified:
                continue
            
            hybrid_stopwords.append(hybrid_info)
        
        # Sort by confidence (descending)
        hybrid_stopwords.sort(key=lambda x: x.confidence, reverse=True)
        
        return hybrid_stopwords[:limit]
    
    def batch_check(
        self,
        terms: List[str],
        language: str,
        threshold: Optional[float] = None
    ) -> Dict[str, HybridStopwordInfo]:
        """
        Batch check multiple terms (efficient)
        
        Args:
            terms: List of terms to check
            language: ISO 639-1 language code
            threshold: Custom confidence threshold
            
        Returns:
            Dictionary mapping term -> HybridStopwordInfo
        """
        results = {}
        for term in terms:
            results[term] = self.is_stopword(term, language, threshold)
        return results
    
    def export_to_redis(
        self,
        stopwords: List[HybridStopwordInfo],
        language: str,
        batch_size: int = 1000
    ) -> int:
        """
        Export stopwords to Redis with POS metadata
        
        Args:
            stopwords: List of stopwords to export
            language: ISO 639-1 language code
            batch_size: Batch size for pipeline operations
            
        Returns:
            Number of stopwords exported
        """
        if not self.idf_detector.redis_client:
            logger.error("Redis not available. Cannot export.")
            return 0
        
        exported_count = 0
        
        try:
            for i in range(0, len(stopwords), batch_size):
                batch = stopwords[i:i + batch_size]
                pipeline = self.idf_detector.redis_client.pipeline()
                
                for sw_info in batch:
                    key = f"stopword:{language}:{sw_info.term}"
                    
                    # Store with POS metadata
                    data = {
                        'confidence': sw_info.confidence,
                        'df': sw_info.document_frequency,
                        'idf': sw_info.idf,
                        'pos': sw_info.pos_tag or '',
                        'grammar_verified': str(sw_info.grammar_verified),
                        'method': sw_info.detection_method
                    }
                    
                    pipeline.hset(key, mapping=data)
                    exported_count += 1
                
                pipeline.execute()
                logger.debug(f"Exported batch {i//batch_size + 1}: {len(batch)} stopwords")
            
            logger.info(
                f"Exported {exported_count} stopwords to Redis",
                language=language,
                grammar_verified=sum(1 for sw in stopwords if sw.grammar_verified)
            )
            return exported_count
            
        except Exception as e:
            logger.error(f"Failed to export stopwords to Redis: {e}")
            return exported_count
    
    def get_statistics(self, language: str) -> Dict:
        """
        Get statistics for stopword detection
        
        Args:
            language: ISO 639-1 language code
            
        Returns:
            Statistics dictionary
        """
        stats = {
            'language': language,
            'stanza_available': self.enable_stanza,
            'grammar_support': self.supports_grammar_verification(language),
            'detection_method': 'hybrid' if self.supports_grammar_verification(language) else 'idf',
        }
        
        if self.stanza_tagger:
            stats['stanza_languages_supported'] = len(self.stanza_tagger.get_supported_languages())
            stats['stanza_models_loaded'] = len(self.stanza_tagger.get_loaded_languages())
        
        return stats

