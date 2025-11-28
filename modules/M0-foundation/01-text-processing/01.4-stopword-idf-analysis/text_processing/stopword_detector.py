"""
Stopword Detector with Redis Storage

Main API for stopword detection with Redis caching for <1ms lookups.
Supports 100+ languages automatically through IDF analysis.
"""

import json
from dataclasses import dataclass, asdict
from typing import List, Dict, Optional, Tuple, Set
import redis
from pathlib import Path

from .idf_analyzer import IDFAnalyzer, IDFScore
from shared.logger import setup_logger

logger = setup_logger(__name__)


@dataclass
class StopwordInfo:
    """Stopword detection result"""
    term: str
    is_stopword: bool
    confidence: float
    language: str
    idf: float = 0.0
    document_frequency: int = 0
    
    def __repr__(self) -> str:
        return (
            f"StopwordInfo(term='{self.term}', is_stopword={self.is_stopword}, "
            f"confidence={self.confidence:.4f}, language='{self.language}')"
        )


class StopwordDetector:
    """
    Universal Stopword Detector with Redis Backend
    
    Features:
    - Automatic stopword detection via IDF analysis
    - Multi-language support (100+ languages)
    - Fast Redis lookup (<1ms)
    - Confidence scoring
    - Bootstrap fallback with NLTK stopword lists
    
    Usage:
        detector = StopwordDetector(redis_url="redis://localhost:6379")
        
        # Check if term is stopword
        result = detector.is_stopword("the", "en")
        print(result.is_stopword)  # True
        print(result.confidence)   # 0.98
        
        # Get all stopwords for language
        stopwords = detector.get_stopwords("en", limit=100)
    """
    
    def __init__(
        self,
        redis_url: str = "redis://localhost:6379",
        redis_db: int = 0,
        default_threshold: float = 0.8,
        use_bootstrap: bool = True,
        bootstrap_path: Optional[Path] = None
    ):
        """
        Initialize Stopword Detector
        
        Args:
            redis_url: Redis connection URL
            redis_db: Redis database number
            default_threshold: Default confidence threshold for stopword detection
            use_bootstrap: Use NLTK bootstrap lists as fallback
            bootstrap_path: Path to bootstrap stopword lists directory
        """
        self.default_threshold = default_threshold
        self.use_bootstrap = use_bootstrap
        self.bootstrap_cache: Dict[str, Set[str]] = {}
        
        # Initialize Redis connection
        try:
            self.redis_client = redis.from_url(
                redis_url,
                db=redis_db,
                decode_responses=True,
                socket_connect_timeout=5
            )
            # Test connection
            self.redis_client.ping()
            logger.info(
                "Redis connection established",
                redis_url=redis_url,
                db=redis_db
            )
        except Exception as e:
            logger.error(f"Failed to connect to Redis: {e}")
            logger.warning("Falling back to bootstrap-only mode")
            self.redis_client = None
        
        # Bootstrap stopword lists path
        if bootstrap_path is None:
            self.bootstrap_path = Path(__file__).parent.parent / "data" / "stopwords" / "bootstrap"
        else:
            self.bootstrap_path = Path(bootstrap_path)
        
        logger.info(
            "Stopword Detector initialized",
            default_threshold=default_threshold,
            use_bootstrap=use_bootstrap,
            bootstrap_path=str(self.bootstrap_path)
        )
    
    def is_stopword(
        self,
        term: str,
        language: str,
        threshold: Optional[float] = None
    ) -> StopwordInfo:
        """
        Check if term is a stopword
        
        Args:
            term: Term to check
            language: Language code (ISO 639-1, e.g., 'en', 'fa', 'zh')
            threshold: Confidence threshold (uses default if None)
            
        Returns:
            StopwordInfo object with detection result
        """
        if threshold is None:
            threshold = self.default_threshold
        
        term_lower = term.lower().strip()
        
        if not term_lower:
            return StopwordInfo(
                term=term,
                is_stopword=False,
                confidence=0.0,
                language=language
            )
        
        # Try Redis lookup first
        if self.redis_client:
            try:
                result = self._redis_lookup(term_lower, language)
                if result:
                    is_stop = result["confidence"] >= threshold
                    return StopwordInfo(
                        term=term,
                        is_stopword=is_stop,
                        confidence=result["confidence"],
                        language=language,
                        idf=result.get("idf", 0.0),
                        document_frequency=result.get("df", 0)
                    )
            except Exception as e:
                logger.warning(f"Redis lookup failed: {e}, falling back to bootstrap")
        
        # Fallback to bootstrap stopword lists
        if self.use_bootstrap:
            is_stop = self._bootstrap_lookup(term_lower, language)
            confidence = 0.9 if is_stop else 0.0  # Bootstrap lists have high confidence
            return StopwordInfo(
                term=term,
                is_stopword=is_stop,
                confidence=confidence,
                language=language
            )
        
        # No data available
        return StopwordInfo(
            term=term,
            is_stopword=False,
            confidence=0.0,
            language=language
        )
    
    def _redis_lookup(self, term: str, language: str) -> Optional[Dict]:
        """
        Lookup term in Redis
        
        Redis key format: stopword:{lang}:{term}
        Redis value: JSON with {confidence, df, idf}
        """
        key = f"stopword:{language}:{term}"
        data = self.redis_client.hgetall(key)
        
        if not data:
            return None
        
        return {
            "confidence": float(data.get("confidence", 0.0)),
            "df": int(data.get("df", 0)),
            "idf": float(data.get("idf", 0.0))
        }
    
    def _bootstrap_lookup(self, term: str, language: str) -> bool:
        """
        Lookup term in bootstrap stopword lists
        
        Bootstrap lists are NLTK stopword lists stored as text files
        """
        # Load bootstrap list for language (cached)
        if language not in self.bootstrap_cache:
            self._load_bootstrap_list(language)
        
        return term in self.bootstrap_cache.get(language, set())
    
    def _load_bootstrap_list(self, language: str) -> None:
        """Load bootstrap stopword list for language"""
        bootstrap_file = self.bootstrap_path / f"{language}.txt"
        
        if not bootstrap_file.exists():
            logger.debug(f"No bootstrap list for language: {language}")
            self.bootstrap_cache[language] = set()
            return
        
        try:
            with open(bootstrap_file, 'r', encoding='utf-8') as f:
                stopwords = {line.strip().lower() for line in f if line.strip()}
            
            self.bootstrap_cache[language] = stopwords
            logger.info(
                f"Loaded bootstrap stopwords for {language}",
                count=len(stopwords)
            )
        except Exception as e:
            logger.error(f"Failed to load bootstrap list for {language}: {e}")
            self.bootstrap_cache[language] = set()
    
    def get_stopwords(
        self,
        language: str,
        min_confidence: float = 0.0,
        limit: Optional[int] = None
    ) -> List[Tuple[str, float]]:
        """
        Get all stopwords for a language
        
        Args:
            language: Language code
            min_confidence: Minimum confidence threshold
            limit: Maximum number of stopwords to return
            
        Returns:
            List of (term, confidence) tuples sorted by confidence (descending)
        """
        stopwords = []
        
        # Get from Redis
        if self.redis_client:
            try:
                # Scan Redis for stopword keys
                pattern = f"stopword:{language}:*"
                cursor = 0
                
                while True:
                    cursor, keys = self.redis_client.scan(
                        cursor,
                        match=pattern,
                        count=1000
                    )
                    
                    for key in keys:
                        data = self.redis_client.hgetall(key)
                        confidence = float(data.get("confidence", 0.0))
                        
                        if confidence >= min_confidence:
                            # Extract term from key: stopword:en:the -> the
                            term = key.split(":", 2)[2]
                            stopwords.append((term, confidence))
                    
                    if cursor == 0:
                        break
                
                logger.debug(
                    f"Retrieved {len(stopwords)} stopwords from Redis",
                    language=language,
                    min_confidence=min_confidence
                )
            except Exception as e:
                logger.warning(f"Failed to retrieve stopwords from Redis: {e}")
        
        # Add bootstrap stopwords if no Redis data
        if not stopwords and self.use_bootstrap:
            if language not in self.bootstrap_cache:
                self._load_bootstrap_list(language)
            
            bootstrap_stopwords = self.bootstrap_cache.get(language, set())
            stopwords = [(term, 0.9) for term in bootstrap_stopwords]
            logger.debug(
                f"Using bootstrap stopwords",
                language=language,
                count=len(stopwords)
            )
        
        # Sort by confidence (descending)
        stopwords.sort(key=lambda x: x[1], reverse=True)
        
        if limit:
            stopwords = stopwords[:limit]
        
        return stopwords
    
    def export_to_redis(
        self,
        idf_scores: Dict[str, IDFScore],
        language: str,
        batch_size: int = 1000
    ) -> int:
        """
        Export IDF analysis results to Redis
        
        Args:
            idf_scores: Dictionary of term -> IDFScore from IDFAnalyzer
            language: Language code
            batch_size: Number of keys to write per pipeline
            
        Returns:
            Number of stopwords exported
        """
        if not self.redis_client:
            logger.error("Redis not available, cannot export")
            return 0
        
        logger.info(
            "Starting Redis export",
            language=language,
            total_terms=len(idf_scores)
        )
        
        exported_count = 0
        pipeline = self.redis_client.pipeline()
        pipeline_count = 0
        
        for term, score in idf_scores.items():
            if not score.is_stopword_candidate:
                continue
            
            # Redis key: stopword:{lang}:{term}
            key = f"stopword:{language}:{term}"
            
            # Store as hash: {confidence, df, idf}
            pipeline.hset(key, mapping={
                "confidence": f"{score.confidence:.6f}",
                "df": str(score.document_frequency),
                "idf": f"{score.idf:.6f}"
            })
            
            exported_count += 1
            pipeline_count += 1
            
            # Execute pipeline in batches
            if pipeline_count >= batch_size:
                pipeline.execute()
                pipeline = self.redis_client.pipeline()
                pipeline_count = 0
                
                if exported_count % 10000 == 0:
                    logger.debug(f"Exported {exported_count} stopwords to Redis")
        
        # Execute remaining commands
        if pipeline_count > 0:
            pipeline.execute()
        
        logger.info(
            "Redis export complete",
            language=language,
            exported_count=exported_count
        )
        
        return exported_count
    
    def clear_language(self, language: str) -> int:
        """
        Clear all stopwords for a language from Redis
        
        Args:
            language: Language code
            
        Returns:
            Number of keys deleted
        """
        if not self.redis_client:
            logger.error("Redis not available")
            return 0
        
        pattern = f"stopword:{language}:*"
        deleted = 0
        cursor = 0
        
        while True:
            cursor, keys = self.redis_client.scan(cursor, match=pattern, count=1000)
            
            if keys:
                deleted += self.redis_client.delete(*keys)
            
            if cursor == 0:
                break
        
        logger.info(f"Cleared {deleted} stopwords for language: {language}")
        return deleted
    
    def get_statistics(self, language: Optional[str] = None) -> Dict:
        """
        Get stopword statistics
        
        Args:
            language: Language code (None for all languages)
            
        Returns:
            Dictionary with statistics
        """
        if not self.redis_client:
            return {"error": "Redis not available"}
        
        if language:
            pattern = f"stopword:{language}:*"
        else:
            pattern = "stopword:*"
        
        # Count keys
        count = 0
        cursor = 0
        
        while True:
            cursor, keys = self.redis_client.scan(cursor, match=pattern, count=1000)
            count += len(keys)
            
            if cursor == 0:
                break
        
        return {
            "language": language or "all",
            "stopword_count": count,
            "redis_connected": True
        }


# Convenience functions

def is_stopword(
    term: str,
    language: str,
    threshold: float = 0.8,
    redis_url: str = "redis://localhost:6379"
) -> bool:
    """
    Convenience function to check if term is stopword
    
    Args:
        term: Term to check
        language: Language code
        threshold: Confidence threshold
        redis_url: Redis connection URL
        
    Returns:
        True if stopword, False otherwise
    """
    detector = StopwordDetector(redis_url=redis_url, default_threshold=threshold)
    result = detector.is_stopword(term, language)
    return result.is_stopword


def get_stopwords(
    language: str,
    limit: int = 100,
    redis_url: str = "redis://localhost:6379"
) -> List[str]:
    """
    Convenience function to get stopwords for language
    
    Args:
        language: Language code
        limit: Maximum number of stopwords
        redis_url: Redis connection URL
        
    Returns:
        List of stopword strings
    """
    detector = StopwordDetector(redis_url=redis_url)
    results = detector.get_stopwords(language, limit=limit)
    return [term for term, confidence in results]


def is_stopword_batch(
    terms: List[str],
    language: str,
    threshold: float = 0.8,
    redis_url: str = "redis://localhost:6379"
) -> List[bool]:
    """
    Batch stopword checking for multiple terms
    
    Args:
        terms: List of terms to check
        language: Language code
        threshold: Confidence threshold
        redis_url: Redis connection URL
        
    Returns:
        List of boolean results (same order as input)
    """
    detector = StopwordDetector(redis_url=redis_url, default_threshold=threshold)
    return [
        detector.is_stopword(term, language).is_stopword
        for term in terms
    ]

