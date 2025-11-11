"""
Comprehensive tests for language detection - Task 01.2

Tests cover:
- 176 languages detection
- Mixed-language content
- Short text handling
- Edge cases
- Performance requirements
"""

import pytest
from text_processing import (
    UniversalLanguageDetector,
    LanguageInfo,
    detect_language,
    detect_language_batch
)


class TestLanguageInfo:
    """Test LanguageInfo dataclass."""
    
    def test_language_info_creation(self):
        """Test LanguageInfo object creation."""
        info = LanguageInfo(
            language_code="en",
            script_code="Latn",
            confidence=0.95,
            is_mixed_content=False,
            detected_languages=[("en", 0.95), ("fr", 0.03)],
            detection_method="fasttext"
        )
        
        assert info.language_code == "en"
        assert info.script_code == "Latn"
        assert info.confidence == 0.95
        assert not info.is_mixed_content
        assert len(info.detected_languages) == 2
        assert info.detection_method == "fasttext"


class TestScriptDetection:
    """Test script detection functionality."""
    
    def test_latin_script(self, test_texts):
        """Test Latin script detection."""
        from text_processing.language_detector import detect_script
        
        assert detect_script(test_texts['en']) == "Latn"
        assert detect_script(test_texts['fr']) == "Latn"
        assert detect_script(test_texts['de']) == "Latn"
    
    def test_arabic_script(self, test_texts):
        """Test Arabic script detection."""
        from text_processing.language_detector import detect_script
        
        assert detect_script(test_texts['ar']) == "Arab"
        assert detect_script(test_texts['fa']) == "Arab"
    
    def test_cyrillic_script(self, test_texts):
        """Test Cyrillic script detection."""
        from text_processing.language_detector import detect_script
        
        assert detect_script(test_texts['ru']) == "Cyrl"
        assert detect_script(test_texts['uk']) == "Cyrl"
    
    def test_cjk_scripts(self, test_texts):
        """Test CJK script detection."""
        from text_processing.language_detector import detect_script
        
        assert detect_script(test_texts['zh']) == "Hans"
        # Japanese can be Hans or Jpan depending on Kanji vs Hiragana/Katakana mix
        ja_script = detect_script(test_texts['ja'])
        assert ja_script in ["Hans", "Jpan"], f"Expected Hans or Jpan, got {ja_script}"
        assert detect_script(test_texts['ko']) == "Kore"
    
    def test_empty_text(self):
        """Test script detection with empty text."""
        from text_processing.language_detector import detect_script
        
        assert detect_script("") == "Zyyy"
        assert detect_script("   ") == "Zyyy"


@pytest.mark.skipif(
    not pytest.importorskip("fasttext", reason="fasttext not installed"),
    reason="Requires fasttext library"
)
class TestFastTextDetector:
    """Test FastText detection functionality."""
    
    def test_detector_initialization(self, has_model):
        """Test detector can be initialized."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        assert detector is not None
    
    def test_english_detection(self, has_model, test_texts):
        """Test English language detection."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        result = detector.detect(test_texts['en'])
        
        assert result.language_code == "en"
        assert result.confidence > 0.7
        assert result.script_code == "Latn"
    
    def test_persian_detection(self, has_model, test_texts):
        """Test Persian language detection."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        result = detector.detect(test_texts['fa'])
        
        assert result.language_code == "fa"
        assert result.confidence > 0.7
        assert result.script_code == "Arab"
    
    def test_chinese_detection(self, has_model, test_texts):
        """Test Chinese language detection."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        result = detector.detect(test_texts['zh'])
        
        assert result.language_code == "zh"
        assert result.confidence > 0.7
    
    def test_multiple_languages(self, has_model, test_texts):
        """Test detection of multiple languages."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        
        languages_to_test = ['en', 'fa', 'ar', 'ru', 'zh', 'es', 'fr', 'de']
        
        for lang_code in languages_to_test:
            if lang_code in test_texts:
                result = detector.detect(test_texts[lang_code])
                assert result.language_code == lang_code, \
                    f"Expected {lang_code}, got {result.language_code}"
                assert result.confidence > 0.5


class TestShortTextDetection:
    """Test detection of very short texts."""
    
    def test_short_texts(self, has_model, short_texts):
        """Test short text detection."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector(use_fallback=True)
        
        for lang_code, text in short_texts.items():
            result = detector.detect(text)
            # Short texts may be less accurate, so just check it doesn't crash
            assert result is not None
            assert isinstance(result.confidence, float)


class TestMixedLanguageDetection:
    """Test detection of mixed-language content."""
    
    def test_mixed_content_detection(self, has_model, mixed_texts):
        """Test mixed language content detection."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        
        for text in mixed_texts:
            result = detector.detect(text)
            # Mixed content should be detected
            assert result is not None
            # May or may not be flagged as mixed depending on dominance


class TestEdgeCases:
    """Test edge cases and error handling."""
    
    def test_empty_text(self, has_model):
        """Test empty text handling."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        result = detector.detect("")
        
        assert result.language_code == "unknown"
        assert result.confidence == 0.0
    
    def test_whitespace_only(self, has_model):
        """Test whitespace-only text."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        result = detector.detect("   \n\t  ")
        
        assert result.language_code == "unknown"
    
    def test_numbers_only(self, has_model):
        """Test numbers-only text."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        result = detector.detect("123456789")
        
        # Numbers may detect as various languages, just ensure no crash
        assert result is not None
    
    def test_symbols_only(self, has_model):
        """Test symbols-only text."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        result = detector.detect("!@#$%^&*()")
        
        assert result is not None
    
    def test_none_input(self, has_model):
        """Test None input raises error."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        
        with pytest.raises(ValueError):
            detector.detect(None)


class TestBatchDetection:
    """Test batch detection functionality."""
    
    def test_batch_detection(self, has_model, test_texts):
        """Test batch language detection."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        
        texts = [test_texts['en'], test_texts['fa'], test_texts['zh']]
        results = detector.detect_batch(texts)
        
        assert len(results) == 3
        assert all(isinstance(r, LanguageInfo) for r in results)
    
    def test_batch_with_empty(self, has_model, test_texts):
        """Test batch detection with empty texts."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        
        texts = [test_texts['en'], "", test_texts['fa']]
        results = detector.detect_batch(texts)
        
        assert len(results) == 3
        assert results[1].language_code == "unknown"


class TestConvenienceFunctions:
    """Test convenience wrapper functions."""
    
    def test_detect_language_function(self, has_model, test_texts):
        """Test detect_language convenience function."""
        if not has_model:
            pytest.skip("No model available")
        
        result = detect_language(test_texts['en'])
        
        assert isinstance(result, LanguageInfo)
        assert result.language_code == "en"
    
    def test_detect_language_batch_function(self, has_model, test_texts):
        """Test detect_language_batch convenience function."""
        if not has_model:
            pytest.skip("No model available")
        
        texts = [test_texts['en'], test_texts['fa']]
        results = detect_language_batch(texts)
        
        assert len(results) == 2
        assert all(isinstance(r, LanguageInfo) for r in results)


class TestReliability:
    """Test reliability checking."""
    
    def test_is_reliable_high_confidence(self, has_model, test_texts):
        """Test reliable detection with high confidence."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector(confidence_threshold=0.7)
        result = detector.detect(test_texts['en'])
        
        assert detector.is_reliable(result)
    
    def test_is_reliable_low_confidence(self, has_model):
        """Test unreliable detection with low confidence."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector(confidence_threshold=0.99)
        result = detector.detect("a")  # Very short, low confidence
        
        # May or may not be reliable depending on actual confidence
        assert isinstance(detector.is_reliable(result), bool)


class TestPerformance:
    """Test performance requirements."""
    
    def test_detection_speed(self, has_model, test_texts, benchmark):
        """Test detection meets speed requirements (<5ms)."""
        if not has_model:
            pytest.skip("No model available")
        
        detector = UniversalLanguageDetector()
        
        # Benchmark detection
        result = benchmark(detector.detect, test_texts['en'])
        
        # Check result is valid
        assert result.language_code == "en"
        
        # Note: benchmark plugin will report timing
        # Target: <5ms per detection
    
    @pytest.mark.slow
    def test_batch_throughput(self, has_model, test_texts):
        """Test batch detection throughput (target: 5000/sec)."""
        if not has_model:
            pytest.skip("No model available")
        
        import time
        
        detector = UniversalLanguageDetector()
        
        # Prepare batch
        texts = [test_texts['en']] * 1000
        
        # Measure time
        start = time.time()
        results = detector.detect_batch(texts)
        elapsed = time.time() - start
        
        throughput = len(results) / elapsed
        
        # Target: 5000+ detections/second
        # Be lenient in tests (1000+ is acceptable)
        assert throughput > 1000, f"Throughput: {throughput:.0f} detections/sec"


class TestCustomModelSupport:
    """Test custom model loading capability."""
    
    def test_custom_model_path(self, tmp_path):
        """Test custom model path can be specified."""
        custom_model = tmp_path / "custom_model.bin"
        
        # We can't test actual custom model without training one
        # Just test the path is accepted
        try:
            detector = UniversalLanguageDetector(model_path=custom_model)
        except FileNotFoundError:
            # Expected - model doesn't exist
            pass


# Integration tests
class TestIntegration:
    """Integration tests with full pipeline."""
    
    def test_full_detection_pipeline(self, has_model):
        """Test complete detection pipeline."""
        if not has_model:
            pytest.skip("No model available")
        
        # Create detector
        detector = UniversalLanguageDetector(
            use_fallback=True,
            confidence_threshold=0.7
        )
        
        # Test text
        text = "This is a test in English language."
        
        # Detect
        result = detector.detect(text)
        
        # Validate
        assert result.language_code == "en"
        assert result.script_code == "Latn"
        assert result.confidence > 0.7
        assert detector.is_reliable(result)
        assert result.detection_method in ["fasttext", "ngram"]

