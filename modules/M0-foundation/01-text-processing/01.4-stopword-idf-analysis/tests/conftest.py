"""Pytest configuration and fixtures for stopword IDF analysis tests"""

import pytest
from typing import List, Dict
import redis
from pymongo import MongoClient
from text_processing import IDFAnalyzer, StopwordDetector


@pytest.fixture
def sample_corpus() -> List[str]:
    """Sample English corpus for testing"""
    return [
        "the quick brown fox jumps over the lazy dog",
        "the dog was very lazy and slept all day",
        "a quick brown fox is faster than the lazy dog",
        "the cat and the dog are best friends",
        "the quick fox jumps high over the fence",
        "programming in python is fun and exciting",
        "machine learning with python and tensorflow",
        "deep learning neural networks are powerful",
        "artificial intelligence is transforming technology",
        "data science requires statistics and programming"
    ]


@pytest.fixture
def multilingual_corpus() -> Dict[str, List[str]]:
    """Multilingual corpus for testing"""
    return {
        "en": [
            "the quick brown fox",
            "the lazy dog sleeps",
            "a cat and a dog"
        ],
        "fa": [
            "این یک متن فارسی است",
            "این متن برای تست است",
            "زبان فارسی زیبا است"
        ],
        "es": [
            "el rápido zorro marrón",
            "el perro perezoso duerme",
            "un gato y un perro"
        ]
    }


@pytest.fixture
def idf_analyzer() -> IDFAnalyzer:
    """IDF Analyzer instance"""
    return IDFAnalyzer(idf_threshold=2.0, min_df=1)


@pytest.fixture
def stopword_detector_no_redis() -> StopwordDetector:
    """Stopword detector without Redis (bootstrap only)"""
    return StopwordDetector(
        redis_url="redis://localhost:9999",  # Invalid URL
        use_bootstrap=True
    )


@pytest.fixture(scope="session")
def redis_available() -> bool:
    """Check if Redis is available for testing"""
    try:
        client = redis.Redis(host='localhost', port=6379, db=15, socket_connect_timeout=1)
        client.ping()
        client.close()
        return True
    except:
        return False


@pytest.fixture
def stopword_detector_redis(redis_available):
    """Stopword detector with Redis (if available)"""
    if not redis_available:
        pytest.skip("Redis not available for testing")
    
    detector = StopwordDetector(
        redis_url="redis://localhost:6379",
        redis_db=15  # Use test database
    )
    
    yield detector
    
    # Cleanup: clear test data
    if detector.redis_client:
        detector.redis_client.flushdb()


@pytest.fixture(scope="session")
def mongodb_available() -> bool:
    """Check if MongoDB is available for testing"""
    try:
        client = MongoClient('mongodb://localhost:27017', serverSelectionTimeoutMS=1000)
        client.server_info()
        client.close()
        return True
    except:
        return False


@pytest.fixture
def sample_idf_scores(idf_analyzer, sample_corpus):
    """Pre-calculated IDF scores for sample corpus"""
    return idf_analyzer.analyze(sample_corpus)


@pytest.fixture
def high_idf_terms() -> List[str]:
    """Terms with high IDF (rare, content words)"""
    return ["quantum", "cryptocurrency", "blockchain", "tensorflow", "neuroscience"]


@pytest.fixture
def low_idf_terms() -> List[str]:
    """Terms with low IDF (common, stopwords)"""
    return ["the", "a", "an", "and", "or", "but", "in", "on", "at", "to"]


@pytest.fixture
def bootstrap_stopwords() -> Dict[str, List[str]]:
    """Known bootstrap stopwords for testing"""
    return {
        "en": ["the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "is"],
        "fa": ["و", "در", "به", "از", "که", "این", "را", "با", "آن", "است"],
        "es": ["el", "la", "de", "en", "y", "a", "los", "del", "se", "las"]
    }

