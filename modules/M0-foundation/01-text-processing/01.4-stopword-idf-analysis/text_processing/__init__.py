"""
Stopword IDF Analysis - Task 01.4

Hybrid stopword detection using a two-layer approach:
1. Layer 1 (Universal): IDF-based detection for ALL languages (100+)
2. Layer 2 (Enhanced): Stanza POS tagging for grammatical refinement (60+ languages)

Main exports:
- HybridStopwordDetector: Two-layer detection with grammar verification
- StopwordDetector: IDF-only detection (Layer 1)
- IDFAnalyzer: Core IDF calculation engine
- StanzaPOSTagger: Grammar-aware POS tagging (Layer 2)
"""

from .idf_analyzer import IDFAnalyzer, IDFScore
from .stopword_detector import (
    StopwordDetector,
    StopwordInfo,
    is_stopword,
    get_stopwords,
    is_stopword_batch
)
from .stanza_pos_tagger import StanzaPOSTagger, get_global_tagger, STANZA_AVAILABLE
from .hybrid_stopword_detector import HybridStopwordDetector, HybridStopwordInfo

__all__ = [
    # Hybrid Detection (Recommended)
    'HybridStopwordDetector',
    'HybridStopwordInfo',
    
    # Layer 1: IDF-based detection
    'IDFAnalyzer',
    'IDFScore',
    'StopwordDetector',
    'StopwordInfo',
    
    # Layer 2: Grammar verification
    'StanzaPOSTagger',
    'get_global_tagger',
    'STANZA_AVAILABLE',
    
    # Convenience functions
    'is_stopword',
    'get_stopwords',
    'is_stopword_batch',
]

__version__ = '0.2.0'  # Updated for hybrid architecture

