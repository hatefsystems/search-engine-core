"""
Tests for Stanza POS Tagger Integration (Layer 2)

Tests cover:
- POS tagging for multiple languages
- Model loading and caching
- Stopword POS tag detection
- Error handling and graceful degradation
"""

import pytest
from text_processing.stanza_pos_tagger import (
    StanzaPOSTagger,
    get_global_tagger,
    STANZA_AVAILABLE
)


# Skip all tests if Stanza not available
pytestmark = pytest.mark.skipif(
    not STANZA_AVAILABLE,
    reason="Stanza not installed. Install with: pip install stanza"
)


class TestStanzaPOSTaggerBasics:
    """Test basic StanzaPOSTagger functionality"""
    
    def test_initialization(self):
        """Test tagger initialization"""
        tagger = StanzaPOSTagger()
        assert tagger.is_available()
        assert tagger.max_models_in_memory == 3
    
    def test_language_support_check(self):
        """Test language support detection"""
        tagger = StanzaPOSTagger()
        
        # Supported languages
        assert tagger.is_supported('en')
        assert tagger.is_supported('fa')
        assert tagger.is_supported('ar')
        assert tagger.is_supported('de')
        assert tagger.is_supported('es')
        
        # Unsupported language
        assert not tagger.is_supported('tlh')  # Klingon
        assert not tagger.is_supported('xxx')  # Invalid
    
    def test_stopword_pos_detection(self):
        """Test stopword POS tag identification"""
        tagger = StanzaPOSTagger()
        
        # Stopword POS tags
        assert tagger.is_stopword_pos('ADP')
        assert tagger.is_stopword_pos('AUX')
        assert tagger.is_stopword_pos('CCONJ')
        assert tagger.is_stopword_pos('DET')
        assert tagger.is_stopword_pos('PART')
        assert tagger.is_stopword_pos('PRON')
        assert tagger.is_stopword_pos('SCONJ')
        
        # Non-stopword POS tags
        assert not tagger.is_stopword_pos('NOUN')
        assert not tagger.is_stopword_pos('VERB')
        assert not tagger.is_stopword_pos('ADJ')
        assert not tagger.is_stopword_pos('ADV')
    
    def test_get_supported_languages(self):
        """Test getting list of supported languages"""
        tagger = StanzaPOSTagger()
        languages = tagger.get_supported_languages()
        
        assert isinstance(languages, list)
        assert len(languages) >= 60  # Should have 60+ languages
        assert 'en' in languages
        assert 'fa' in languages
        assert sorted(languages) == languages  # Should be sorted


class TestStanzaPOSTagging:
    """Test POS tagging functionality"""
    
    @pytest.mark.slow
    def test_pos_tagging_english(self):
        """Test POS tagging for English"""
        tagger = StanzaPOSTagger(auto_download=True)
        
        # Test stopwords
        assert tagger.get_pos_tag('the', 'en') == 'DET'
        assert tagger.get_pos_tag('and', 'en') == 'CCONJ'
        assert tagger.get_pos_tag('in', 'en') == 'ADP'
        
        # Test content words
        pos = tagger.get_pos_tag('quantum', 'en')
        assert pos in ['NOUN', 'ADJ']  # Could be either depending on context
    
    @pytest.mark.slow
    def test_pos_tagging_persian(self):
        """Test POS tagging for Persian (Farsi)"""
        tagger = StanzaPOSTagger(auto_download=True)
        
        # Persian stopwords
        assert tagger.get_pos_tag('و', 'fa') == 'CCONJ'  # and
        assert tagger.get_pos_tag('در', 'fa') == 'ADP'   # in
        assert tagger.get_pos_tag('از', 'fa') == 'ADP'   # from
    
    @pytest.mark.slow
    def test_pos_tagging_arabic(self):
        """Test POS tagging for Arabic"""
        tagger = StanzaPOSTagger(auto_download=True)
        
        # Arabic stopwords
        pos = tagger.get_pos_tag('في', 'ar')  # in
        assert pos in ['ADP', 'PART']  # Could be either
    
    def test_pos_tagging_unsupported_language(self):
        """Test POS tagging for unsupported language"""
        tagger = StanzaPOSTagger()
        
        # Should return None for unsupported language
        result = tagger.get_pos_tag('test', 'tlh')
        assert result is None
    
    @pytest.mark.slow
    def test_batch_tagging(self):
        """Test batch POS tagging"""
        tagger = StanzaPOSTagger(auto_download=True)
        
        terms = ['the', 'quick', 'brown', 'fox']
        results = tagger.batch_tag(terms, 'en')
        
        assert isinstance(results, dict)
        assert len(results) == len(terms)
        assert results['the'] == 'DET'
        assert results['fox'] == 'NOUN'


class TestStanzaModelManagement:
    """Test model loading and caching"""
    
    def test_lazy_loading(self):
        """Test lazy model loading"""
        tagger = StanzaPOSTagger()
        
        # No models should be loaded initially
        assert len(tagger.get_loaded_languages()) == 0
    
    @pytest.mark.slow
    def test_model_eviction(self):
        """Test LRU model eviction"""
        tagger = StanzaPOSTagger(max_models_in_memory=2, auto_download=True)
        
        # Load first model
        tagger.get_pos_tag('the', 'en')
        assert 'en' in tagger.get_loaded_languages()
        
        # Load second model
        tagger.get_pos_tag('the', 'de')
        assert 'de' in tagger.get_loaded_languages()
        assert len(tagger.get_loaded_languages()) == 2
        
        # Load third model (should evict first)
        tagger.get_pos_tag('the', 'es')
        assert 'es' in tagger.get_loaded_languages()
        assert len(tagger.get_loaded_languages()) == 2
        assert 'en' not in tagger.get_loaded_languages()  # Evicted
    
    def test_cache_clearing(self):
        """Test cache clearing"""
        tagger = StanzaPOSTagger()
        
        # Clear cache should not raise error
        tagger.clear_cache()
    
    def test_unload_all_models(self):
        """Test unloading all models"""
        tagger = StanzaPOSTagger()
        tagger.unload_all_models()
        
        assert len(tagger.get_loaded_languages()) == 0


class TestStanzaGlobalTagger:
    """Test global singleton tagger"""
    
    def test_get_global_tagger(self):
        """Test getting global tagger instance"""
        tagger1 = get_global_tagger()
        tagger2 = get_global_tagger()
        
        # Should return same instance
        assert tagger1 is tagger2
    
    def test_global_tagger_caching(self):
        """Test that global tagger caches results"""
        tagger = get_global_tagger()
        
        # Clear cache first
        tagger.clear_cache()
        
        # First call should cache result
        result1 = tagger.get_pos_tag('test', 'en') if tagger.is_supported('en') else None
        
        # Second call should use cache (very fast)
        result2 = tagger.get_pos_tag('test', 'en') if tagger.is_supported('en') else None
        
        assert result1 == result2


class TestStanzaErrorHandling:
    """Test error handling and edge cases"""
    
    def test_empty_term(self):
        """Test POS tagging empty term"""
        tagger = StanzaPOSTagger()
        
        result = tagger.get_pos_tag('', 'en')
        assert result is None
    
    def test_whitespace_term(self):
        """Test POS tagging whitespace"""
        tagger = StanzaPOSTagger()
        
        result = tagger.get_pos_tag('   ', 'en')
        assert result is None or result == 'X'  # Could be unknown
    
    def test_invalid_language_code(self):
        """Test invalid language code"""
        tagger = StanzaPOSTagger()
        
        result = tagger.get_pos_tag('test', 'invalid')
        assert result is None
    
    def test_special_characters(self):
        """Test POS tagging special characters"""
        tagger = StanzaPOSTagger()
        
        # Should handle gracefully
        result = tagger.get_pos_tag('!!!', 'en')
        assert result is None or result == 'PUNCT'


class TestStanzaIntegrationWithIDF:
    """Test integration scenarios with IDF analysis"""
    
    @pytest.mark.slow
    def test_stopword_verification_english(self):
        """Test stopword verification for English"""
        tagger = StanzaPOSTagger(auto_download=True)
        
        # True stopwords (should be verified)
        stopwords = ['the', 'a', 'an', 'and', 'or', 'in', 'on', 'at', 'to', 'for']
        for term in stopwords:
            pos_tag = tagger.get_pos_tag(term, 'en')
            if pos_tag:
                # Most should be stopword POS tags
                is_stopword = tagger.is_stopword_pos(pos_tag)
                assert is_stopword, f"'{term}' should be verified as stopword (POS: {pos_tag})"
    
    @pytest.mark.slow
    def test_false_positive_filtering(self):
        """Test filtering false positives (high IDF terms)"""
        tagger = StanzaPOSTagger(auto_download=True)
        
        # Domain-specific terms that might have low IDF but aren't stopwords
        false_positives = ['API', 'HTTP', 'JSON', 'REST']
        for term in false_positives:
            pos_tag = tagger.get_pos_tag(term, 'en')
            if pos_tag:
                is_stopword = tagger.is_stopword_pos(pos_tag)
                # Should NOT be stopword POS (likely NOUN or PROPN)
                assert not is_stopword, f"'{term}' should NOT be verified as stopword (POS: {pos_tag})"
    
    @pytest.mark.slow
    def test_multilingual_verification(self):
        """Test stopword verification across multiple languages"""
        tagger = StanzaPOSTagger(auto_download=True)
        
        # Test cases: (term, language, expected_is_stopword)
        test_cases = [
            ('the', 'en', True),
            ('و', 'fa', True),  # Persian 'and'
            ('في', 'ar', True),  # Arabic 'in'
            ('der', 'de', True),  # German 'the'
            ('el', 'es', True),  # Spanish 'the'
        ]
        
        for term, lang, expected in test_cases:
            if tagger.is_supported(lang):
                pos_tag = tagger.get_pos_tag(term, lang)
                if pos_tag:
                    is_stopword = tagger.is_stopword_pos(pos_tag)
                    assert is_stopword == expected, \
                        f"'{term}' ({lang}) POS: {pos_tag}, expected stopword: {expected}"


@pytest.mark.performance
class TestStanzaPerformance:
    """Test performance characteristics"""
    
    @pytest.mark.slow
    def test_caching_speedup(self, benchmark):
        """Test that caching provides speedup"""
        tagger = StanzaPOSTagger(auto_download=True)
        
        # Warm up
        tagger.get_pos_tag('test', 'en')
        
        # Benchmark cached call
        result = benchmark(tagger.get_pos_tag, 'test', 'en')
        
        # Cached call should be very fast (<5ms)
        assert benchmark.stats['mean'] < 0.005  # Less than 5ms
    
    @pytest.mark.slow
    def test_batch_efficiency(self):
        """Test batch processing efficiency"""
        tagger = StanzaPOSTagger(auto_download=True)
        
        terms = ['the', 'quick', 'brown', 'fox', 'jumps', 'over', 'lazy', 'dog']
        
        import time
        start = time.time()
        results = tagger.batch_tag(terms, 'en')
        elapsed = time.time() - start
        
        # Should process all terms quickly
        assert len(results) == len(terms)
        assert elapsed < 1.0  # Should take less than 1 second


def test_stanza_availability_flag():
    """Test STANZA_AVAILABLE flag"""
    # Should be True if tests are running (pytestmark skips if not available)
    assert STANZA_AVAILABLE is True

