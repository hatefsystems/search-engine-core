"""
Tests for Hybrid Stopword Detector (Layer 1 + Layer 2)

Tests cover:
- Hybrid detection with both IDF and Stanza
- Confidence boosting/penalty based on grammar
- Fallback to IDF-only when Stanza unavailable
- Redis export with POS metadata
"""

import pytest
from unittest.mock import Mock, MagicMock, patch
from text_processing.hybrid_stopword_detector import (
    HybridStopwordDetector,
    HybridStopwordInfo
)
from text_processing.stanza_pos_tagger import STANZA_AVAILABLE


class TestHybridStopwordDetectorBasics:
    """Test basic hybrid detector functionality"""
    
    def test_initialization(self):
        """Test hybrid detector initialization"""
        detector = HybridStopwordDetector(
            redis_url="redis://localhost:6379",
            enable_stanza=False  # Disable for speed
        )
        
        assert detector.idf_detector is not None
        assert detector.enable_stanza is False
    
    def test_initialization_with_stanza(self):
        """Test initialization with Stanza enabled"""
        if not STANZA_AVAILABLE:
            pytest.skip("Stanza not available")
        
        detector = HybridStopwordDetector(
            redis_url="redis://localhost:6379",
            enable_stanza=True
        )
        
        assert detector.enable_stanza is True
        assert detector.stanza_tagger is not None
    
    def test_grammar_support_check(self):
        """Test checking grammar verification support"""
        detector = HybridStopwordDetector(enable_stanza=False)
        
        # Should return False when Stanza disabled
        assert detector.supports_grammar_verification('en') is False
        assert detector.supports_grammar_verification('fa') is False
    
    @pytest.mark.skipif(not STANZA_AVAILABLE, reason="Stanza not available")
    def test_grammar_support_with_stanza(self):
        """Test grammar support with Stanza enabled"""
        detector = HybridStopwordDetector(enable_stanza=True)
        
        # Supported languages
        assert detector.supports_grammar_verification('en') is True
        assert detector.supports_grammar_verification('fa') is True
        assert detector.supports_grammar_verification('ar') is True
        
        # Unsupported language
        assert detector.supports_grammar_verification('tlh') is False


class TestHybridDetectionIDFOnly:
    """Test hybrid detector in IDF-only mode"""
    
    def test_idf_only_detection(self):
        """Test detection falls back to IDF-only when Stanza disabled"""
        detector = HybridStopwordDetector(
            redis_url="redis://localhost:6379",
            enable_stanza=False
        )
        
        # Mock the IDF detector
        mock_result = Mock()
        mock_result.term = "test"
        mock_result.is_stopword = True
        mock_result.confidence = 0.85
        mock_result.language = "en"
        mock_result.idf = 1.5
        mock_result.document_frequency = 1000
        
        detector.idf_detector.is_stopword = Mock(return_value=mock_result)
        
        # Should use IDF result directly
        result = detector.is_stopword("test", "en")
        
        assert isinstance(result, HybridStopwordInfo)
        assert result.is_stopword is True
        assert result.confidence == 0.85
        assert result.grammar_verified is False
        assert result.pos_tag is None
        assert result.detection_method == "idf"
    
    def test_idf_only_batch_check(self):
        """Test batch checking in IDF-only mode"""
        detector = HybridStopwordDetector(enable_stanza=False)
        
        terms = ["the", "test", "and"]
        results = detector.batch_check(terms, "en")
        
        assert isinstance(results, dict)
        assert len(results) == len(terms)
        
        for term in terms:
            assert term in results
            assert isinstance(results[term], HybridStopwordInfo)
            assert results[term].grammar_verified is False


@pytest.mark.skipif(not STANZA_AVAILABLE, reason="Stanza not available")
class TestHybridDetectionWithGrammar:
    """Test hybrid detection with grammar verification"""
    
    @pytest.mark.slow
    def test_grammar_verified_stopword(self):
        """Test stopword verified by grammar (confidence boost)"""
        detector = HybridStopwordDetector(
            redis_url="redis://localhost:6379",
            enable_stanza=True,
            confidence_boost=1.2,
            auto_download=True
        )
        
        # Mock IDF result for 'the' (stopword)
        mock_idf_result = Mock()
        mock_idf_result.term = "the"
        mock_idf_result.is_stopword = True
        mock_idf_result.confidence = 0.85  # Base confidence
        mock_idf_result.language = "en"
        mock_idf_result.idf = 1.2
        mock_idf_result.document_frequency = 9500
        
        detector.idf_detector.is_stopword = Mock(return_value=mock_idf_result)
        
        # Should get grammar verification and confidence boost
        result = detector.is_stopword("the", "en")
        
        assert isinstance(result, HybridStopwordInfo)
        assert result.grammar_verified is True
        assert result.pos_tag == 'DET'
        assert result.confidence > 0.85  # Boosted
        assert result.detection_method == "hybrid"
    
    @pytest.mark.slow
    def test_grammar_rejected_false_positive(self):
        """Test false positive rejected by grammar (confidence penalty)"""
        detector = HybridStopwordDetector(
            redis_url="redis://localhost:6379",
            enable_stanza=True,
            confidence_penalty=0.7
        )
        
        # Mock IDF result for 'API' (false positive)
        mock_idf_result = Mock()
        mock_idf_result.term = "API"
        mock_idf_result.is_stopword = True  # Falsely detected by IDF
        mock_idf_result.confidence = 0.85
        mock_idf_result.language = "en"
        mock_idf_result.idf = 1.8
        mock_idf_result.document_frequency = 7500
        
        detector.idf_detector.is_stopword = Mock(return_value=mock_idf_result)
        
        # Should reduce confidence due to grammar
        result = detector.is_stopword("API", "en")
        
        assert isinstance(result, HybridStopwordInfo)
        assert result.grammar_verified is True
        assert result.pos_tag in ['NOUN', 'PROPN']  # Not a stopword POS
        assert result.confidence < 0.85  # Penalized
        # Might or might not be considered stopword after penalty
    
    @pytest.mark.slow
    def test_multilingual_grammar_verification(self):
        """Test grammar verification across languages"""
        detector = HybridStopwordDetector(
            redis_url="redis://localhost:6379",
            enable_stanza=True
        )
        
        # Test Persian stopword
        mock_fa_result = Mock()
        mock_fa_result.term = "و"  # 'and' in Persian
        mock_fa_result.is_stopword = True
        mock_fa_result.confidence = 0.90
        mock_fa_result.language = "fa"
        mock_fa_result.idf = 1.1
        mock_fa_result.document_frequency = 8500
        
        detector.idf_detector.is_stopword = Mock(return_value=mock_fa_result)
        
        result = detector.is_stopword("و", "fa")
        
        assert result.grammar_verified is True
        assert result.pos_tag == 'CCONJ'
        assert result.is_stopword is True
    
    @pytest.mark.slow
    def test_fallback_for_unsupported_language(self):
        """Test fallback to IDF-only for unsupported language"""
        detector = HybridStopwordDetector(
            redis_url="redis://localhost:6379",
            enable_stanza=True
        )
        
        # Mock IDF result for unsupported language
        mock_result = Mock()
        mock_result.term = "test"
        mock_result.is_stopword = True
        mock_result.confidence = 0.88
        mock_result.language = "tlh"  # Klingon (not supported)
        mock_result.idf = 1.4
        mock_result.document_frequency = 8000
        
        detector.idf_detector.is_stopword = Mock(return_value=mock_result)
        
        # Should fall back to IDF-only
        result = detector.is_stopword("test", "tlh")
        
        assert result.grammar_verified is False
        assert result.pos_tag is None
        assert result.confidence == 0.88  # No adjustment
        assert result.detection_method == "idf"


class TestHybridStopwordRetrieval:
    """Test retrieving stopwords with hybrid info"""
    
    def test_get_stopwords_basic(self):
        """Test getting stopwords with hybrid info"""
        detector = HybridStopwordDetector(enable_stanza=False)
        
        # Mock IDF detector's get_stopwords
        mock_stopwords = [
            Mock(term="the", is_stopword=True, confidence=0.95, language="en", idf=0.8, document_frequency=9500),
            Mock(term="and", is_stopword=True, confidence=0.92, language="en", idf=1.0, document_frequency=9000),
            Mock(term="in", is_stopword=True, confidence=0.90, language="en", idf=1.2, document_frequency=8500),
        ]
        
        detector.idf_detector.get_stopwords = Mock(return_value=mock_stopwords)
        
        results = detector.get_stopwords("en", limit=10)
        
        assert isinstance(results, list)
        assert len(results) == 3
        assert all(isinstance(r, HybridStopwordInfo) for r in results)
    
    def test_get_stopwords_with_confidence_filter(self):
        """Test filtering stopwords by confidence"""
        detector = HybridStopwordDetector(enable_stanza=False)
        
        results = detector.get_stopwords("en", limit=100, min_confidence=0.90)
        
        # All results should have confidence >= 0.90
        assert all(r.confidence >= 0.90 for r in results)
    
    @pytest.mark.skipif(not STANZA_AVAILABLE, reason="Stanza not available")
    def test_get_stopwords_grammar_verified_only(self):
        """Test getting only grammar-verified stopwords"""
        detector = HybridStopwordDetector(
            redis_url="redis://localhost:6379",
            enable_stanza=True
        )
        
        results = detector.get_stopwords(
            "en",
            limit=100,
            grammar_verified_only=True
        )
        
        # All results should be grammar-verified
        assert all(r.grammar_verified for r in results)


class TestHybridRedisExport:
    """Test Redis export with POS metadata"""
    
    def test_export_to_redis_basic(self):
        """Test exporting stopwords with POS metadata to Redis"""
        detector = HybridStopwordDetector(enable_stanza=False)
        
        # Mock Redis client
        mock_redis = Mock()
        mock_pipeline = Mock()
        mock_redis.pipeline.return_value = mock_pipeline
        detector.idf_detector.redis_client = mock_redis
        
        # Create test stopwords
        stopwords = [
            HybridStopwordInfo(
                term="the",
                is_stopword=True,
                confidence=0.98,
                language="en",
                idf=0.8,
                document_frequency=9500,
                pos_tag="DET",
                grammar_verified=True,
                detection_method="hybrid"
            ),
            HybridStopwordInfo(
                term="and",
                is_stopword=True,
                confidence=0.95,
                language="en",
                idf=1.0,
                document_frequency=9000,
                pos_tag="CCONJ",
                grammar_verified=True,
                detection_method="hybrid"
            ),
        ]
        
        # Export
        count = detector.export_to_redis(stopwords, "en")
        
        assert count == 2
        assert mock_pipeline.hset.call_count == 2
        assert mock_pipeline.execute.call_count == 1
    
    def test_export_without_redis(self):
        """Test export fails gracefully when Redis unavailable"""
        detector = HybridStopwordDetector(enable_stanza=False)
        detector.idf_detector.redis_client = None  # Simulate Redis unavailable
        
        stopwords = [
            HybridStopwordInfo(
                term="test",
                is_stopword=True,
                confidence=0.85,
                language="en"
            )
        ]
        
        # Should return 0 without crashing
        count = detector.export_to_redis(stopwords, "en")
        assert count == 0


class TestHybridStatistics:
    """Test statistics and introspection"""
    
    def test_get_statistics_idf_only(self):
        """Test getting statistics in IDF-only mode"""
        detector = HybridStopwordDetector(enable_stanza=False)
        
        stats = detector.get_statistics("en")
        
        assert stats['language'] == 'en'
        assert stats['stanza_available'] is False
        assert stats['grammar_support'] is False
        assert stats['detection_method'] == 'idf'
    
    @pytest.mark.skipif(not STANZA_AVAILABLE, reason="Stanza not available")
    def test_get_statistics_with_stanza(self):
        """Test getting statistics with Stanza enabled"""
        detector = HybridStopwordDetector(enable_stanza=True)
        
        stats = detector.get_statistics("en")
        
        assert stats['language'] == 'en'
        assert stats['stanza_available'] is True
        assert stats['grammar_support'] is True
        assert stats['detection_method'] == 'hybrid'
        assert 'stanza_languages_supported' in stats
        assert stats['stanza_languages_supported'] >= 60


class TestHybridStopwordInfo:
    """Test HybridStopwordInfo dataclass"""
    
    def test_creation(self):
        """Test creating HybridStopwordInfo"""
        info = HybridStopwordInfo(
            term="the",
            is_stopword=True,
            confidence=0.98,
            language="en",
            idf=0.8,
            document_frequency=9500,
            pos_tag="DET",
            grammar_verified=True,
            detection_method="hybrid"
        )
        
        assert info.term == "the"
        assert info.is_stopword is True
        assert info.confidence == 0.98
        assert info.pos_tag == "DET"
        assert info.grammar_verified is True
    
    def test_to_dict(self):
        """Test converting to dictionary"""
        info = HybridStopwordInfo(
            term="the",
            is_stopword=True,
            confidence=0.98,
            language="en"
        )
        
        data = info.to_dict()
        
        assert isinstance(data, dict)
        assert data['term'] == "the"
        assert data['confidence'] == 0.98
    
    def test_from_dict(self):
        """Test creating from dictionary"""
        data = {
            'term': "the",
            'is_stopword': True,
            'confidence': 0.98,
            'language': "en",
            'idf': 0.8,
            'document_frequency': 9500,
            'pos_tag': "DET",
            'grammar_verified': True,
            'detection_method': "hybrid"
        }
        
        info = HybridStopwordInfo.from_dict(data)
        
        assert info.term == "the"
        assert info.confidence == 0.98
        assert info.pos_tag == "DET"
    
    def test_repr(self):
        """Test string representation"""
        info = HybridStopwordInfo(
            term="the",
            is_stopword=True,
            confidence=0.98,
            language="en",
            pos_tag="DET"
        )
        
        repr_str = repr(info)
        
        assert "the" in repr_str
        assert "0.98" in repr_str
        assert "DET" in repr_str


@pytest.mark.integration
class TestHybridIntegrationScenarios:
    """Integration tests for real-world scenarios"""
    
    @pytest.mark.skipif(not STANZA_AVAILABLE, reason="Stanza not available")
    @pytest.mark.slow
    def test_complete_hybrid_pipeline(self):
        """Test complete pipeline: IDF → Stanza → Redis"""
        detector = HybridStopwordDetector(
            redis_url="redis://localhost:6379",
            enable_stanza=True
        )
        
        # Test a few terms
        terms = ["the", "and", "quantum", "API"]
        
        for term in terms:
            result = detector.is_stopword(term, "en")
            
            # Should have valid result
            assert isinstance(result, HybridStopwordInfo)
            assert result.term == term
            
            # Check consistency
            if result.grammar_verified:
                assert result.pos_tag is not None
                assert result.detection_method == "hybrid"
    
    @pytest.mark.slow
    def test_multilanguage_hybrid_detection(self):
        """Test hybrid detection across multiple languages"""
        if not STANZA_AVAILABLE:
            pytest.skip("Stanza not available")
        
        detector = HybridStopwordDetector(enable_stanza=True)
        
        # Test cases: (term, language)
        test_cases = [
            ("the", "en"),
            ("و", "fa"),
            ("der", "de"),
            ("el", "es"),
        ]
        
        for term, lang in test_cases:
            result = detector.is_stopword(term, lang)
            
            # Should detect as stopword
            assert result.is_stopword is True
            
            # Should have grammar verification if language supported
            if detector.supports_grammar_verification(lang):
                assert result.grammar_verified is True
                assert result.pos_tag is not None

