"""Tests for IDF calculation - Task 01.4"""

import pytest
import math
from text_processing import IDFAnalyzer, IDFScore


class TestIDFCalculation:
    """Test IDF calculation algorithm"""
    
    def test_idf_high_frequency_term(self, idf_analyzer, sample_corpus):
        """Test IDF for high frequency terms (should be low)"""
        idf_scores = idf_analyzer.analyze(sample_corpus)
        
        # "the" appears in most documents
        the_score = idf_scores.get("the")
        assert the_score is not None
        assert the_score.idf < 1.0, "High frequency term should have low IDF"
        assert the_score.is_stopword_candidate, "'the' should be stopword candidate"
    
    def test_idf_rare_term(self, idf_analyzer, sample_corpus):
        """Test IDF for rare terms (should be high)"""
        idf_scores = idf_analyzer.analyze(sample_corpus)
        
        # "programming" appears in only 2 documents
        programming_score = idf_scores.get("programming")
        assert programming_score is not None
        assert programming_score.idf > 1.5, "Rare term should have high IDF"
        assert not programming_score.is_stopword_candidate
    
    def test_idf_zero_frequency(self, idf_analyzer):
        """Test IDF for terms not in corpus"""
        corpus = ["hello world", "test document"]
        idf_scores = idf_analyzer.analyze(corpus)
        
        # Term not in corpus
        nonexistent = idf_scores.get("nonexistent")
        assert nonexistent is None, "Term not in corpus should not have score"
    
    def test_idf_formula_correctness(self, idf_analyzer):
        """Test IDF formula: IDF = log(N / df)"""
        corpus = [
            "the cat",
            "the dog",
            "the bird"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus)
        the_score = idf_scores.get("the")
        
        # "the" appears in all 3 documents
        N = 3
        df = 3
        
        if idf_analyzer.smoothing:
            expected_idf = math.log((N + 1) / (df + 1)) + 1
        else:
            expected_idf = math.log(N / df)
        
        assert abs(the_score.idf - expected_idf) < 0.01, \
            f"IDF calculation incorrect: got {the_score.idf}, expected {expected_idf}"
    
    def test_idf_different_frequencies(self, idf_analyzer):
        """Test IDF increases as document frequency decreases"""
        corpus = [
            "the the the",  # "the" in doc 1
            "the the cat",  # "the" in doc 2
            "the dog fox",  # "the" in doc 3
            "cat dog bird", # "cat" in doc 4
            "only unique"   # "only" in doc 5
        ]
        
        idf_scores = idf_analyzer.analyze(corpus)
        
        # Get IDF scores
        the_idf = idf_scores.get("the").idf     # appears in 3 docs
        cat_idf = idf_scores.get("cat").idf     # appears in 2 docs
        only_idf = idf_scores.get("only").idf   # appears in 1 doc
        
        # IDF should increase as frequency decreases
        assert the_idf < cat_idf < only_idf, \
            "IDF should increase as document frequency decreases"
    
    def test_empty_corpus(self, idf_analyzer):
        """Test handling of empty corpus"""
        idf_scores = idf_analyzer.analyze([])
        assert len(idf_scores) == 0, "Empty corpus should return no scores"
    
    def test_single_document_corpus(self, idf_analyzer):
        """Test IDF with single document"""
        corpus = ["hello world this is a test"]
        idf_scores = idf_analyzer.analyze(corpus)
        
        # With N=1, df=1 for all terms, IDF should be 0 (or 1 with smoothing)
        for term, score in idf_scores.items():
            assert score.document_frequency == 1
            assert score.total_documents == 1
    
    def test_confidence_scoring(self, idf_analyzer, sample_corpus):
        """Test confidence score calculation"""
        idf_scores = idf_analyzer.analyze(sample_corpus)
        
        # High frequency terms should have high confidence as stopwords
        the_score = idf_scores.get("the")
        assert the_score.confidence > 0.8, "High frequency stopword should have high confidence"
        
        # Low frequency terms should have low confidence
        programming_score = idf_scores.get("programming")
        assert programming_score.confidence < 0.3, "Rare term should have low stopword confidence"
    
    def test_min_df_filter(self):
        """Test minimum document frequency filter"""
        analyzer = IDFAnalyzer(min_df=2)  # Ignore terms in <2 documents
        
        corpus = [
            "common common word",
            "common word here",
            "rare term only"
        ]
        
        idf_scores = analyzer.analyze(corpus)
        
        # "common" appears in 2 docs - should be included
        assert "common" in idf_scores
        
        # "rare" appears in 1 doc - should be filtered out
        assert "rare" not in idf_scores
    
    def test_max_df_filter(self):
        """Test maximum document frequency ratio filter"""
        analyzer = IDFAnalyzer(max_df_ratio=0.8)  # Ignore terms in >80% docs
        
        corpus = [
            "the word one",
            "the word two",
            "the word three",
            "the word four",
            "unique term five"
        ]
        
        idf_scores = analyzer.analyze(corpus)
        
        # "the" appears in 100% docs (>80%) - should have IDF=0
        the_score = idf_scores.get("the")
        assert the_score.idf == 0.0, "Terms above max_df_ratio should have IDF=0"
    
    def test_stopword_candidate_detection(self, idf_analyzer, sample_corpus):
        """Test stopword candidate detection based on threshold"""
        idf_scores = idf_analyzer.analyze(sample_corpus)
        
        # Get stopword candidates
        candidates = idf_analyzer.get_stopword_candidates()
        
        # Should find common English stopwords
        candidate_terms = [term for term, score in candidates]
        
        # Common stopwords should be in candidates
        assert "the" in candidate_terms
        assert "and" in candidate_terms
        
        # Rare terms should not be candidates
        assert "programming" not in candidate_terms
        assert "tensorflow" not in candidate_terms
    
    def test_batch_processing(self, idf_analyzer):
        """Test batch processing for large corpus"""
        # Generate large corpus
        def corpus_generator():
            for i in range(100):
                yield f"document {i} with common terms like the and a"
        
        idf_scores = idf_analyzer.analyze_batch(
            corpus_generator(),
            batch_size=10
        )
        
        assert idf_analyzer.total_documents == 100
        assert len(idf_scores) > 0
        
        # "the" should be stopword candidate
        the_score = idf_scores.get("the")
        assert the_score is not None
        assert the_score.is_stopword_candidate
    
    def test_export_statistics(self, idf_analyzer, sample_corpus):
        """Test export of analysis statistics"""
        idf_analyzer.analyze(sample_corpus)
        stats = idf_analyzer.export_statistics()
        
        assert stats["total_documents"] == len(sample_corpus)
        assert stats["vocabulary_size"] > 0
        assert stats["analyzed_terms"] > 0
        assert stats["stopword_candidates"] > 0
        assert 0 <= stats["stopword_ratio"] <= 1
    
    def test_unicode_support(self, idf_analyzer):
        """Test IDF calculation with Unicode text"""
        corpus = [
            "این یک متن فارسی است",
            "این متن فارسی است",
            "متن فارسی",
            "English mixed with فارسی"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus)
        
        # Persian word "است" (is) appears in 2 docs
        ast_score = idf_scores.get("است")
        assert ast_score is not None
        assert ast_score.document_frequency == 2
    
    def test_case_insensitivity(self, idf_analyzer):
        """Test that IDF calculation is case-insensitive"""
        corpus = [
            "The quick brown fox",
            "THE LAZY DOG",
            "the cat"
        ]
        
        idf_scores = idf_analyzer.analyze(corpus)
        
        # "the", "The", "THE" should all count as same term
        the_score = idf_scores.get("the")
        assert the_score.document_frequency == 3


class TestIDFScore:
    """Test IDFScore dataclass"""
    
    def test_idf_score_creation(self):
        """Test IDFScore object creation"""
        score = IDFScore(
            term="test",
            idf=2.5,
            document_frequency=10,
            total_documents=100,
            is_stopword_candidate=False,
            confidence=0.2
        )
        
        assert score.term == "test"
        assert score.idf == 2.5
        assert score.document_frequency == 10
        assert score.total_documents == 100
        assert not score.is_stopword_candidate
        assert score.confidence == 0.2
    
    def test_idf_score_repr(self):
        """Test IDFScore string representation"""
        score = IDFScore(
            term="example",
            idf=1.5,
            document_frequency=50,
            total_documents=100
        )
        
        repr_str = repr(score)
        assert "example" in repr_str
        assert "1.5" in repr_str
        assert "50" in repr_str

