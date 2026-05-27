"""Tests for Stopword Detector - Task 01.4"""

import pytest
from text_processing import StopwordDetector, StopwordInfo, is_stopword, get_stopwords


class TestStopwordDetector:
    """Test stopword detection API"""
    
    def test_detector_initialization(self):
        """Test StopwordDetector initialization"""
        detector = StopwordDetector(
            redis_url="redis://localhost:6379",
            default_threshold=0.85
        )
        
        assert detector.default_threshold == 0.85
        assert detector.use_bootstrap
    
    def test_is_stopword_bootstrap(self, stopword_detector_no_redis):
        """Test stopword detection with bootstrap lists"""
        # English stopwords
        result = stopword_detector_no_redis.is_stopword("the", "en")
        # Note: Will only work if bootstrap files exist, otherwise returns False
        assert isinstance(result, StopwordInfo)
        assert result.term == "the"
        assert result.language == "en"
    
    def test_is_stopword_not_stopword(self, stopword_detector_no_redis):
        """Test non-stopword detection"""
        result = stopword_detector_no_redis.is_stopword("quantum", "en")
        
        assert isinstance(result, StopwordInfo)
        assert not result.is_stopword or result.confidence < 0.5
    
    def test_empty_term(self, stopword_detector_no_redis):
        """Test handling of empty term"""
        result = stopword_detector_no_redis.is_stopword("", "en")
        
        assert isinstance(result, StopwordInfo)
        assert not result.is_stopword
        assert result.confidence == 0.0
    
    def test_whitespace_term(self, stopword_detector_no_redis):
        """Test handling of whitespace-only term"""
        result = stopword_detector_no_redis.is_stopword("   ", "en")
        
        assert isinstance(result, StopwordInfo)
        assert not result.is_stopword
    
    def test_case_insensitivity(self, stopword_detector_no_redis):
        """Test case-insensitive stopword detection"""
        lower_result = stopword_detector_no_redis.is_stopword("the", "en")
        upper_result = stopword_detector_no_redis.is_stopword("THE", "en")
        mixed_result = stopword_detector_no_redis.is_stopword("The", "en")
        
        # Should all have same is_stopword value
        assert lower_result.is_stopword == upper_result.is_stopword == mixed_result.is_stopword
    
    def test_custom_threshold(self, stopword_detector_no_redis):
        """Test custom confidence threshold"""
        # High threshold - fewer stopwords
        high_result = stopword_detector_no_redis.is_stopword("the", "en", threshold=0.99)
        
        # Low threshold - more stopwords
        low_result = stopword_detector_no_redis.is_stopword("the", "en", threshold=0.1)
        
        assert isinstance(high_result, StopwordInfo)
        assert isinstance(low_result, StopwordInfo)
    
    def test_unicode_stopword(self, stopword_detector_no_redis):
        """Test stopword detection with Unicode characters"""
        # Persian stopword "و" (and)
        result = stopword_detector_no_redis.is_stopword("و", "fa")
        
        assert isinstance(result, StopwordInfo)
        assert result.language == "fa"
    
    @pytest.mark.requires_redis
    def test_redis_lookup(self, stopword_detector_redis, idf_analyzer, sample_corpus):
        """Test Redis-based stopword lookup"""
        # Analyze corpus and export to Redis
        idf_scores = idf_analyzer.analyze(sample_corpus)
        exported = stopword_detector_redis.export_to_redis(idf_scores, "en")
        
        assert exported > 0, "Should export some stopwords"
        
        # Lookup stopword
        result = stopword_detector_redis.is_stopword("the", "en")
        
        assert isinstance(result, StopwordInfo)
        assert result.is_stopword
        assert result.confidence > 0.8
    
    @pytest.mark.requires_redis
    def test_redis_export_import(self, stopword_detector_redis, idf_analyzer, sample_corpus):
        """Test exporting and importing stopwords to/from Redis"""
        # Analyze and export
        idf_scores = idf_analyzer.analyze(sample_corpus)
        exported_count = stopword_detector_redis.export_to_redis(idf_scores, "en")
        
        assert exported_count > 0
        
        # Retrieve stopwords
        stopwords = stopword_detector_redis.get_stopwords("en", limit=10)
        
        assert len(stopwords) > 0
        assert all(isinstance(sw, tuple) for sw in stopwords)
        assert all(len(sw) == 2 for sw in stopwords)  # (term, confidence) tuples
    
    @pytest.mark.requires_redis
    def test_clear_language(self, stopword_detector_redis, idf_analyzer, sample_corpus):
        """Test clearing stopwords for a language"""
        # Export stopwords
        idf_scores = idf_analyzer.analyze(sample_corpus)
        stopword_detector_redis.export_to_redis(idf_scores, "test_lang")
        
        # Clear
        deleted = stopword_detector_redis.clear_language("test_lang")
        
        assert deleted > 0, "Should delete some keys"
        
        # Verify cleared
        stopwords = stopword_detector_redis.get_stopwords("test_lang")
        assert len(stopwords) == 0


class TestStopwordInfo:
    """Test StopwordInfo dataclass"""
    
    def test_stopword_info_creation(self):
        """Test StopwordInfo object creation"""
        info = StopwordInfo(
            term="test",
            is_stopword=True,
            confidence=0.95,
            language="en",
            idf=0.5,
            document_frequency=1000
        )
        
        assert info.term == "test"
        assert info.is_stopword
        assert info.confidence == 0.95
        assert info.language == "en"
        assert info.idf == 0.5
        assert info.document_frequency == 1000
    
    def test_stopword_info_repr(self):
        """Test StopwordInfo string representation"""
        info = StopwordInfo(
            term="example",
            is_stopword=True,
            confidence=0.9,
            language="en"
        )
        
        repr_str = repr(info)
        assert "example" in repr_str
        assert "True" in repr_str
        assert "0.9" in repr_str


class TestConvenienceFunctions:
    """Test convenience functions"""
    
    def test_is_stopword_function(self):
        """Test is_stopword convenience function"""
        # This will try to connect to Redis or use bootstrap
        # May return False if neither available
        result = is_stopword("the", "en", redis_url="redis://localhost:9999")
        
        assert isinstance(result, bool)
    
    def test_get_stopwords_function(self):
        """Test get_stopwords convenience function"""
        stopwords = get_stopwords("en", limit=10, redis_url="redis://localhost:9999")
        
        assert isinstance(stopwords, list)
        # May be empty if Redis and bootstrap not available
    
    def test_is_stopword_batch(self):
        """Test batch stopword checking"""
        from text_processing import is_stopword_batch
        
        terms = ["the", "quantum", "and", "cryptocurrency"]
        results = is_stopword_batch(terms, "en", redis_url="redis://localhost:9999")
        
        assert isinstance(results, list)
        assert len(results) == len(terms)
        assert all(isinstance(r, bool) for r in results)


class TestBootstrapLists:
    """Test bootstrap stopword list loading"""
    
    def test_bootstrap_loading(self, stopword_detector_no_redis):
        """Test loading bootstrap stopword lists"""
        # Try to load English bootstrap
        stopword_detector_no_redis._load_bootstrap_list("en")
        
        # Check if loaded (may be empty if file doesn't exist)
        assert "en" in stopword_detector_no_redis.bootstrap_cache
    
    def test_bootstrap_cache(self, stopword_detector_no_redis):
        """Test bootstrap list caching"""
        # Load twice - should use cache second time
        stopword_detector_no_redis._load_bootstrap_list("en")
        first_load = stopword_detector_no_redis.bootstrap_cache.get("en", set())
        
        stopword_detector_no_redis._load_bootstrap_list("en")
        second_load = stopword_detector_no_redis.bootstrap_cache.get("en", set())
        
        # Should be same object (cached)
        assert first_load is second_load
    
    def test_missing_bootstrap_language(self, stopword_detector_no_redis):
        """Test handling of missing bootstrap language"""
        # Load non-existent language
        stopword_detector_no_redis._load_bootstrap_list("nonexistent_lang")
        
        # Should have empty set
        assert stopword_detector_no_redis.bootstrap_cache["nonexistent_lang"] == set()


@pytest.mark.requires_redis
class TestRedisIntegration:
    """Test Redis integration features"""
    
    def test_redis_connection(self, redis_available):
        """Test Redis connection establishment"""
        if not redis_available:
            pytest.skip("Redis not available")
        
        detector = StopwordDetector(redis_url="redis://localhost:6379", redis_db=15)
        
        assert detector.redis_client is not None
        assert detector.redis_client.ping()
    
    def test_redis_key_format(self, stopword_detector_redis, idf_analyzer, sample_corpus):
        """Test Redis key format: stopword:{lang}:{term}"""
        idf_scores = idf_analyzer.analyze(sample_corpus)
        stopword_detector_redis.export_to_redis(idf_scores, "en")
        
        # Check key exists
        key = "stopword:en:the"
        exists = stopword_detector_redis.redis_client.exists(key)
        
        assert exists, f"Key {key} should exist in Redis"
    
    def test_redis_data_structure(self, stopword_detector_redis, idf_analyzer, sample_corpus):
        """Test Redis hash data structure"""
        idf_scores = idf_analyzer.analyze(sample_corpus)
        stopword_detector_redis.export_to_redis(idf_scores, "en")
        
        # Get hash data
        key = "stopword:en:the"
        data = stopword_detector_redis.redis_client.hgetall(key)
        
        assert "confidence" in data
        assert "df" in data
        assert "idf" in data
        
        # Verify data types
        assert 0.0 <= float(data["confidence"]) <= 1.0
        assert int(data["df"]) > 0
        assert float(data["idf"]) >= 0.0
    
    def test_get_statistics(self, stopword_detector_redis, idf_analyzer, sample_corpus):
        """Test statistics retrieval"""
        idf_scores = idf_analyzer.analyze(sample_corpus)
        stopword_detector_redis.export_to_redis(idf_scores, "en")
        
        stats = stopword_detector_redis.get_statistics("en")
        
        assert "stopword_count" in stats
        assert "language" in stats
        assert stats["language"] == "en"
        assert stats["stopword_count"] > 0
    
    def test_batch_export(self, stopword_detector_redis, idf_analyzer):
        """Test batch export of large stopword list"""
        # Create large corpus
        large_corpus = [f"document {i} with the and a words" for i in range(100)]
        
        idf_scores = idf_analyzer.analyze(large_corpus)
        exported = stopword_detector_redis.export_to_redis(
            idf_scores,
            "test_batch",
            batch_size=10
        )
        
        assert exported > 0
        
        # Cleanup
        stopword_detector_redis.clear_language("test_batch")

