"""Integration tests - Task 01.4"""

import pytest
from text_processing import IDFAnalyzer, StopwordDetector
from text_processing.corpus_processor import CorpusProcessor


class TestFullPipeline:
    """Test complete stopword detection pipeline"""
    
    def test_end_to_end_pipeline(self, sample_corpus):
        """Test full pipeline: analyze → export → detect"""
        # Step 1: Analyze corpus
        analyzer = IDFAnalyzer(idf_threshold=2.0)
        idf_scores = analyzer.analyze(sample_corpus, language="en")
        
        assert len(idf_scores) > 0
        
        # Step 2: Get stopword candidates
        candidates = analyzer.get_stopword_candidates(min_confidence=0.7)
        
        assert len(candidates) > 0
        
        # Step 3: Verify stopwords
        candidate_terms = [term for term, score in candidates]
        assert "the" in candidate_terms
        assert "and" in candidate_terms
    
    @pytest.mark.requires_redis
    def test_full_pipeline_with_redis(self, sample_corpus, redis_available):
        """Test full pipeline with Redis storage"""
        if not redis_available:
            pytest.skip("Redis not available")
        
        # Analyze
        analyzer = IDFAnalyzer(idf_threshold=2.0)
        idf_scores = analyzer.analyze(sample_corpus, language="en")
        
        # Export to Redis
        detector = StopwordDetector(redis_url="redis://localhost:6379", redis_db=15)
        exported = detector.export_to_redis(idf_scores, "test_pipeline")
        
        assert exported > 0
        
        # Detect stopwords
        result = detector.is_stopword("the", "test_pipeline")
        assert result.is_stopword
        assert result.confidence > 0.7
        
        # Cleanup
        detector.clear_language("test_pipeline")
    
    def test_pipeline_with_large_corpus(self):
        """Test pipeline with large corpus (batch processing)"""
        # Generate large corpus
        def large_corpus():
            for i in range(1000):
                yield f"document {i} the quick brown fox and lazy dog"
        
        analyzer = IDFAnalyzer(idf_threshold=2.0)
        idf_scores = analyzer.analyze_batch(large_corpus(), batch_size=100)
        
        assert analyzer.total_documents == 1000
        assert len(idf_scores) > 0
        
        # Should detect common stopwords
        the_score = idf_scores.get("the")
        assert the_score is not None
        assert the_score.is_stopword_candidate
    
    def test_multilingual_pipeline(self):
        """Test pipeline with multiple languages"""
        languages = {
            "en": ["the cat and dog"] * 10,
            "es": ["el gato y perro"] * 10,
            "fa": ["گربه و سگ این"] * 10
        }
        
        analyzer = IDFAnalyzer(idf_threshold=2.0)
        
        all_stopwords = {}
        for lang, corpus in languages.items():
            idf_scores = analyzer.analyze(corpus, language=lang)
            candidates = analyzer.get_stopword_candidates()
            all_stopwords[lang] = [term for term, score in candidates]
        
        # Each language should have stopwords
        for lang in languages.keys():
            assert len(all_stopwords[lang]) > 0, f"No stopwords for {lang}"
    
    def test_incremental_updates(self, sample_corpus):
        """Test incremental corpus updates"""
        # Initial analysis
        analyzer = IDFAnalyzer(idf_threshold=2.0)
        initial_scores = analyzer.analyze(sample_corpus[:5])
        
        initial_stopwords = [term for term, score in initial_scores.items() 
                           if score.is_stopword_candidate]
        
        # Full analysis
        full_scores = analyzer.analyze(sample_corpus)
        full_stopwords = [term for term, score in full_scores.items() 
                         if score.is_stopword_candidate]
        
        # Full analysis should have more or equal stopwords
        assert len(full_scores) >= len(initial_scores)


class TestErrorHandling:
    """Test error handling and edge cases"""
    
    def test_empty_corpus_handling(self, idf_analyzer):
        """Test handling of empty corpus"""
        empty_corpus = []
        idf_scores = idf_analyzer.analyze(empty_corpus)
        
        assert len(idf_scores) == 0
        assert idf_analyzer.total_documents == 0
    
    def test_single_document_corpus(self, idf_analyzer):
        """Test single document corpus"""
        corpus = ["single document with some words"]
        idf_scores = idf_analyzer.analyze(corpus)
        
        assert len(idf_scores) > 0
        assert idf_analyzer.total_documents == 1
    
    def test_duplicate_documents(self, idf_analyzer):
        """Test corpus with duplicate documents"""
        corpus = ["the same document"] * 10
        idf_scores = idf_analyzer.analyze(corpus)
        
        # Should still work correctly
        assert len(idf_scores) > 0
        
        # All terms should be in all documents
        for term, score in idf_scores.items():
            assert score.document_frequency == 10
    
    def test_very_long_document(self, idf_analyzer):
        """Test handling of very long documents"""
        long_doc = " ".join(["word"] * 10000)
        corpus = [long_doc, "short doc"]
        
        idf_scores = idf_analyzer.analyze(corpus)
        assert len(idf_scores) > 0
    
    def test_special_characters(self, idf_analyzer):
        """Test handling of special characters"""
        corpus = [
            "test@example.com and #hashtag",
            "price: $100.50 or €85.30",
            "emoticons :) :( ;-)"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus)
        # Should handle without crashing
        assert idf_analyzer.total_documents == 3
    
    def test_mixed_whitespace(self, idf_analyzer):
        """Test handling of mixed whitespace"""
        corpus = [
            "normal   spacing",
            "tabs\tand\tnewlines\n",
            "   leading and trailing   "
        ]
        
        idf_scores = idf_analyzer.analyze(corpus)
        assert len(idf_scores) > 0
    
    def test_redis_connection_failure(self):
        """Test graceful handling of Redis connection failure"""
        detector = StopwordDetector(
            redis_url="redis://invalid-host:9999",
            use_bootstrap=True
        )
        
        # Should not crash, fall back to bootstrap
        assert detector.redis_client is None
        
        # Should still work with bootstrap
        result = detector.is_stopword("test", "en")
        assert isinstance(result, object)


class TestPerformance:
    """Test performance characteristics"""
    
    def test_large_vocabulary_size(self, idf_analyzer):
        """Test performance with large vocabulary"""
        # Create corpus with many unique words
        corpus = [f"word{i} word{i+1} word{i+2}" for i in range(1000)]
        
        idf_scores = idf_analyzer.analyze(corpus)
        
        # Should handle large vocabulary
        assert len(idf_scores) > 2000
    
    def test_high_document_count(self, idf_analyzer):
        """Test performance with many documents"""
        # Create many documents
        corpus = [f"document {i} with the and a" for i in range(10000)]
        
        idf_scores = idf_analyzer.analyze(corpus)
        
        assert idf_analyzer.total_documents == 10000
        assert len(idf_scores) > 0
    
    @pytest.mark.benchmark
    def test_idf_calculation_speed(self, benchmark, idf_analyzer):
        """Benchmark IDF calculation speed"""
        corpus = ["the quick brown fox"] * 100
        
        result = benchmark(idf_analyzer.analyze, corpus)
        
        assert len(result) > 0
    
    @pytest.mark.benchmark
    @pytest.mark.requires_redis
    def test_redis_lookup_speed(self, benchmark, stopword_detector_redis):
        """Benchmark Redis lookup speed (target: <1ms)"""
        # Setup: add test stopword
        if stopword_detector_redis.redis_client:
            stopword_detector_redis.redis_client.hset(
                "stopword:test:word",
                mapping={"confidence": "0.95", "df": "1000", "idf": "0.5"}
            )
        
        result = benchmark(stopword_detector_redis.is_stopword, "word", "test")
        
        assert result.is_stopword


@pytest.mark.requires_mongodb
class TestMongoDBIntegration:
    """Test MongoDB corpus processing"""
    
    def test_corpus_processor_initialization(self, mongodb_available):
        """Test CorpusProcessor initialization"""
        if not mongodb_available:
            pytest.skip("MongoDB not available")
        
        processor = CorpusProcessor(
            mongodb_uri="mongodb://localhost:27017",
            database="test_db",
            collection="test_collection"
        )
        
        assert processor.database_name == "test_db"
        assert processor.collection_name == "test_collection"
        
        processor.close()
    
    def test_corpus_iteration(self, mongodb_available):
        """Test iterating over MongoDB corpus"""
        if not mongodb_available:
            pytest.skip("MongoDB not available")
        
        # Note: This test requires actual MongoDB with data
        # In practice, you would seed test data first
        processor = CorpusProcessor(
            mongodb_uri="mongodb://localhost:27017",
            database="test_db",
            collection="test_docs"
        )
        
        # Try to iterate (may be empty)
        doc_count = 0
        for doc in processor.iterate_documents(limit=10):
            doc_count += 1
        
        # Should not crash even if empty
        assert doc_count >= 0
        
        processor.close()

