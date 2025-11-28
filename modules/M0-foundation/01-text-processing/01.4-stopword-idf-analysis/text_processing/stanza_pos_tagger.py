"""
Stanza POS Tagger - Grammar-aware stopword verification (Layer 2)

Uses Stanford Stanza NLP library for Part-of-Speech tagging to verify
stopword candidates from IDF analysis. Supports 60+ languages with
lazy model loading and efficient caching.
"""

import os
from functools import lru_cache
from typing import Dict, Optional, Set, List
from pathlib import Path

from shared.logger import setup_logger

logger = setup_logger(__name__)

# Import stanza with graceful degradation
try:
    import stanza
    STANZA_AVAILABLE = True
    logger.info("Stanza NLP library loaded successfully")
except ImportError:
    STANZA_AVAILABLE = False
    logger.warning(
        "Stanza not available. Install with: pip install stanza",
        fallback="IDF-only mode"
    )


class StanzaPOSTagger:
    """
    POS Tagging wrapper with model management and caching
    
    Features:
    - Lazy model loading (memory efficient)
    - LRU cache for POS tags (10K terms)
    - Automatic model download on first use
    - Graceful degradation if Stanza unavailable
    - Support for 60+ languages
    
    Usage:
        tagger = StanzaPOSTagger()
        
        # Check if language supported
        if tagger.is_supported('fa'):
            pos_tag = tagger.get_pos_tag('و', 'fa')
            print(pos_tag)  # 'CCONJ'
            
            is_stopword = tagger.is_stopword_pos(pos_tag)
            print(is_stopword)  # True
    """
    
    # Universal POS tags that indicate stopwords (function words)
    STOPWORD_POS_TAGS: Set[str] = {
        'ADP',    # Adposition (preposition, postposition)
        'AUX',    # Auxiliary verb
        'CCONJ',  # Coordinating conjunction
        'DET',    # Determiner
        'PART',   # Particle
        'PRON',   # Pronoun
        'SCONJ',  # Subordinating conjunction
    }
    
    # Stanza supported languages (60+ languages)
    # Full list: https://stanfordnlp.github.io/stanza/models.html
    SUPPORTED_LANGUAGES: Set[str] = {
        # Priority languages (high-quality models)
        'en', 'fa', 'ar', 'de', 'es', 'fr', 'it', 'pt', 'ru', 'tr',
        'zh', 'ja', 'ko', 'hi', 'id', 'vi', 'nl', 'pl', 'uk', 'cs',
        
        # Extended support
        'af', 'eu', 'bg', 'ca', 'hr', 'da', 'et', 'fi', 'gl', 'he',
        'hu', 'is', 'lv', 'lt', 'no', 'ro', 'sk', 'sl', 'sv', 'th',
        'ur', 'el', 'be', 'ta', 'te', 'mr', 'bn', 'gu', 'kn', 'ml',
        'pa', 'or', 'as', 'sa', 'my', 'km', 'lo', 'si', 'ne', 'hy',
        'ka', 'mn', 'bo', 'cy', 'ga', 'gd', 'eu', 'fo', 'fy', 'lb'
    }
    
    def __init__(
        self,
        max_models_in_memory: int = 3,
        model_dir: Optional[Path] = None,
        auto_download: bool = True,
        use_gpu: bool = False
    ):
        """
        Initialize Stanza POS Tagger
        
        Args:
            max_models_in_memory: Maximum models to keep in RAM (default: 3)
            model_dir: Directory to store Stanza models
            auto_download: Automatically download models on first use
            use_gpu: Use GPU for inference (default: False for production)
        """
        self.max_models_in_memory = max_models_in_memory
        self.auto_download = auto_download
        self.use_gpu = use_gpu
        
        # Model cache (lazy-loaded)
        self._models: Dict[str, any] = {}
        self._model_load_order: List[str] = []
        
        # Set Stanza resources directory
        if model_dir:
            os.environ['STANZA_RESOURCES_DIR'] = str(model_dir)
            logger.info(f"Stanza model directory: {model_dir}")
        
        # Check availability
        if not STANZA_AVAILABLE:
            logger.warning("Stanza not available. POS tagging disabled.")
    
    def is_available(self) -> bool:
        """Check if Stanza is available"""
        return STANZA_AVAILABLE
    
    def is_supported(self, language: str) -> bool:
        """
        Check if Stanza supports this language
        
        Args:
            language: ISO 639-1 language code (e.g., 'en', 'fa', 'ar')
            
        Returns:
            True if language is supported
        """
        if not STANZA_AVAILABLE:
            return False
        return language in self.SUPPORTED_LANGUAGES
    
    def download_model(self, language: str, force: bool = False) -> bool:
        """
        Download Stanza model for language
        
        Args:
            language: ISO 639-1 language code
            force: Force re-download even if model exists
            
        Returns:
            True if download successful
        """
        if not STANZA_AVAILABLE:
            logger.error("Stanza not available. Cannot download models.")
            return False
        
        if not self.is_supported(language):
            logger.error(f"Language '{language}' not supported by Stanza")
            return False
        
        try:
            logger.info(f"Downloading Stanza model for '{language}'...")
            
            # Get model directory (use default if not set)
            model_dir = os.environ.get('STANZA_RESOURCES_DIR')
            
            # Download with or without custom model_dir
            if model_dir:
                stanza.download(
                    language,
                    processors='tokenize,pos',
                    verbose=True,
                    model_dir=model_dir
                )
            else:
                # Use Stanza's default model directory
                stanza.download(
                    language,
                    processors='tokenize,pos',
                    verbose=True
                )
            
            logger.info(f"✅ Model downloaded successfully: {language}")
            return True
        except Exception as e:
            logger.error(f"Failed to download model for '{language}': {e}")
            return False
    
    def is_model_downloaded(self, language: str) -> bool:
        """
        Check if model is already downloaded
        
        Args:
            language: ISO 639-1 language code
            
        Returns:
            True if model is downloaded
        """
        if not STANZA_AVAILABLE:
            return False
        
        try:
            # Try to load model metadata
            model_dir = os.environ.get(
                'STANZA_RESOURCES_DIR',
                str(Path.home() / 'stanza_resources')
            )
            model_path = Path(model_dir) / language
            return model_path.exists() and len(list(model_path.glob('*.pt'))) > 0
        except Exception:
            return False
    
    def _load_model(self, language: str):
        """
        Lazy load Stanza model with LRU eviction
        
        Args:
            language: ISO 639-1 language code
            
        Returns:
            Loaded Stanza pipeline
        """
        if not STANZA_AVAILABLE:
            raise RuntimeError("Stanza not available")
        
        # Check if model already loaded
        if language in self._models:
            return self._models[language]
        
        # LRU eviction: Remove oldest model if too many loaded
        if len(self._models) >= self.max_models_in_memory:
            oldest_lang = self._model_load_order.pop(0)
            del self._models[oldest_lang]
            logger.info(f"Evicted model from memory: {oldest_lang}")
        
        # Auto-download if needed
        if self.auto_download and not self.is_model_downloaded(language):
            logger.info(f"Model not found for '{language}'. Downloading...")
            self.download_model(language)
        
        # Load model
        try:
            logger.info(f"Loading Stanza model for '{language}'...")
            pipeline = stanza.Pipeline(
                language,
                processors='tokenize,pos',
                verbose=False,
                use_gpu=self.use_gpu,
                tokenize_no_ssplit=True  # Don't split sentences for single terms
            )
            
            self._models[language] = pipeline
            self._model_load_order.append(language)
            
            logger.info(f"✅ Model loaded: {language}")
            return pipeline
            
        except Exception as e:
            logger.error(f"Failed to load model for '{language}': {e}")
            raise
    
    @lru_cache(maxsize=10000)
    def get_pos_tag(self, term: str, language: str) -> Optional[str]:
        """
        Get POS tag for single term (cached)
        
        Args:
            term: Word to tag
            language: ISO 639-1 language code
            
        Returns:
            Universal POS tag (UPOS) or None if unavailable
            
        Example:
            >>> tagger = StanzaPOSTagger()
            >>> tagger.get_pos_tag('the', 'en')
            'DET'
            >>> tagger.get_pos_tag('و', 'fa')
            'CCONJ'
        """
        if not STANZA_AVAILABLE:
            return None
        
        if not self.is_supported(language):
            logger.debug(f"Language '{language}' not supported by Stanza")
            return None
        
        try:
            nlp = self._load_model(language)
            doc = nlp(term)
            
            if doc.sentences and doc.sentences[0].words:
                pos_tag = doc.sentences[0].words[0].upos
                return pos_tag
            
            return None
            
        except Exception as e:
            logger.error(
                f"POS tagging failed for '{term}' ({language}): {e}"
            )
            return None
    
    def is_stopword_pos(self, pos_tag: str) -> bool:
        """
        Check if POS tag indicates stopword (function word)
        
        Args:
            pos_tag: Universal POS tag (UPOS)
            
        Returns:
            True if POS tag indicates stopword
            
        Example:
            >>> tagger = StanzaPOSTagger()
            >>> tagger.is_stopword_pos('DET')
            True
            >>> tagger.is_stopword_pos('NOUN')
            False
        """
        return pos_tag in self.STOPWORD_POS_TAGS
    
    def batch_tag(self, terms: List[str], language: str) -> Dict[str, Optional[str]]:
        """
        Batch POS tagging (more efficient than individual calls)
        
        Args:
            terms: List of terms to tag
            language: ISO 639-1 language code
            
        Returns:
            Dictionary mapping term -> POS tag
            
        Note:
            Results are automatically cached via @lru_cache on get_pos_tag
        """
        results = {}
        for term in terms:
            results[term] = self.get_pos_tag(term, language)
        return results
    
    def clear_cache(self):
        """Clear LRU cache for POS tags"""
        self.get_pos_tag.cache_clear()
        logger.info("Cleared POS tag cache")
    
    def unload_all_models(self):
        """Unload all models from memory"""
        self._models.clear()
        self._model_load_order.clear()
        logger.info("Unloaded all Stanza models from memory")
    
    def get_supported_languages(self) -> List[str]:
        """Get list of all supported languages"""
        return sorted(list(self.SUPPORTED_LANGUAGES))
    
    def get_loaded_languages(self) -> List[str]:
        """Get list of currently loaded languages"""
        return list(self._models.keys())


# Singleton instance for reuse
_global_tagger: Optional[StanzaPOSTagger] = None


def get_global_tagger() -> StanzaPOSTagger:
    """
    Get global singleton POS tagger instance
    
    Returns:
        Shared StanzaPOSTagger instance
    """
    global _global_tagger
    if _global_tagger is None:
        _global_tagger = StanzaPOSTagger()
    return _global_tagger

