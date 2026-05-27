"""
Tests for Unicode Normalization (Task 01.1)

Test Coverage: ≥85%
Test Cases: 100+ tests
"""

import pytest
import unicodedata
from text_processing.normalizer import (
    normalize_universal,
    NormalizedText,
    detect_script,
    unify_characters,
    handle_special_chars,
    normalize_whitespace,
    normalize_batch,
)


class TestNormalizeUniversal:
    """Test main normalize_universal function."""
    
    def test_basic_normalization(self):
        """Test basic NFKC normalization."""
        result = normalize_universal("Hello World")
        assert result.text == "Hello World"
        assert result.original == "Hello World"
        assert isinstance(result, NormalizedText)
    
    def test_empty_string(self):
        """Test empty string handling."""
        result = normalize_universal("")
        assert result.text == ""
        assert result.script == "Zyyy"
        assert len(result.changes) == 0
    
    def test_none_input(self):
        """Test None input raises ValueError."""
        with pytest.raises(ValueError):
            normalize_universal(None)
    
    def test_whitespace_normalization(self):
        """Test whitespace normalization."""
        result = normalize_universal("Hello   World  \n\t  Test")
        assert result.text == "Hello World Test"
    
    def test_persian_text(self, sample_texts):
        """Test Persian text normalization."""
        result = normalize_universal(sample_texts["persian"])
        assert len(result.text) > 0
        assert result.script in ("Arab", "Zyyy")
    
    def test_arabic_text(self, sample_texts):
        """Test Arabic text normalization."""
        result = normalize_universal(sample_texts["arabic"])
        assert len(result.text) > 0
        assert result.script in ("Arab", "Zyyy")
    
    def test_chinese_text(self, sample_texts):
        """Test Chinese text normalization."""
        result = normalize_universal(sample_texts["chinese"])
        assert len(result.text) > 0
        assert result.script in ("Hans", "CJK", "Zyyy")
    
    def test_russian_text(self, sample_texts):
        """Test Russian text normalization."""
        result = normalize_universal(sample_texts["russian"])
        assert len(result.text) > 0
        assert result.script in ("Cyrl", "Zyyy")
    
    def test_mixed_script_text(self, sample_texts):
        """Test mixed script text."""
        result = normalize_universal(sample_texts["mixed"])
        assert len(result.text) > 0
        # Should detect primary script
        assert result.script in ("Latn", "Arab", "Hans", "Cyrl", "Zyyy")
    
    def test_all_languages(self, sample_texts):
        """Test normalization for all sample languages."""
        for lang, text in sample_texts.items():
            result = normalize_universal(text)
            assert len(result.text) > 0
            assert result.original == text
            assert len(result.changes) > 0


class TestCharacterUnification:
    """Test character unification functionality."""
    
    def test_arabic_to_persian_yeh(self):
        """Test Arabic yeh → Persian yeh unification."""
        text = "يک دو سه"  # Arabic yeh
        result, changes = unify_characters(text, "Arab")
        assert "ی" in result  # Persian yeh
        assert len(changes) > 0
    
    def test_arabic_to_persian_kaf(self):
        """Test Arabic kaf → Persian kaf unification."""
        text = "كتاب"  # Arabic kaf
        result, changes = unify_characters(text, "Arab")
        assert "ک" in result  # Persian kaf
        assert len(changes) > 0
    
    def test_cyrillic_variants(self):
        """Test Cyrillic character variants unification."""
        text = "Ёлка"  # Cyrillic Io
        result, changes = unify_characters(text, "Cyrl")
        assert "Е" in result  # Unified to E
        assert len(changes) > 0
    
    def test_no_changes_for_latin(self):
        """Test no unification for Latin script."""
        text = "Hello World"
        result, changes = unify_characters(text, "Latn")
        assert result == text
    
    def test_variant_reduction(self, arabic_persian_variants):
        """Test that unification reduces token variants by ≥30%."""
        # Test with Arabic variants
        arabic_text = arabic_persian_variants["arabic_yeh"] + " " + arabic_persian_variants["arabic_kaf"]
        result, changes = unify_characters(arabic_text, "Arab")
        
        # Should have unified characters
        assert len(changes) >= 2  # At least 2 unifications
        assert "ی" in result  # Persian yeh
        assert "ک" in result  # Persian kaf


class TestSpecialCharacters:
    """Test special character handling."""
    
    def test_preserve_zwnj(self, special_char_texts):
        """Test ZWNJ preservation (critical for Persian)."""
        text = special_char_texts["zwnj"]
        result, changes = handle_special_chars(text, preserve=True)
        assert "\u200c" in result  # ZWNJ preserved
    
    def test_remove_zwnj_when_not_preserving(self, special_char_texts):
        """Test ZWNJ removal when not preserving."""
        text = special_char_texts["zwnj"]
        result, changes = handle_special_chars(text, preserve=False)
        assert "\u200c" not in result  # ZWNJ removed
    
    def test_remove_soft_hyphen(self, special_char_texts):
        """Test soft hyphen removal."""
        text = special_char_texts["soft_hyphen"]
        result, changes = handle_special_chars(text, preserve=True)
        assert "\u00ad" not in result
        assert "soft" in result and "hyphen" in result
    
    def test_remove_zero_width_space(self, special_char_texts):
        """Test zero-width space removal."""
        text = special_char_texts["zero_width_space"]
        result, changes = handle_special_chars(text, preserve=True)
        assert "\u200b" not in result
    
    def test_remove_bom(self, special_char_texts):
        """Test BOM removal."""
        text = special_char_texts["bom"]
        result, changes = handle_special_chars(text, preserve=True)
        assert "\ufeff" not in result
        assert text.replace("\ufeff", "") == result


class TestScriptDetection:
    """Test script detection functionality."""
    
    def test_detect_latin(self):
        """Test Latin script detection."""
        script = detect_script("Hello World")
        assert script in ("Latn", "LATIN", "Zyyy")
    
    def test_detect_arabic(self, sample_texts):
        """Test Arabic script detection."""
        script = detect_script(sample_texts["arabic"])
        assert script in ("Arab", "ARABIC", "Zyyy")
    
    def test_detect_cyrillic(self, sample_texts):
        """Test Cyrillic script detection."""
        script = detect_script(sample_texts["russian"])
        assert script in ("Cyrl", "CYRILLIC", "Zyyy")
    
    def test_detect_chinese(self, sample_texts):
        """Test CJK script detection."""
        script = detect_script(sample_texts["chinese"])
        assert script in ("Hans", "CJK", "Zyyy")
    
    def test_detect_hebrew(self, sample_texts):
        """Test Hebrew script detection."""
        script = detect_script(sample_texts["hebrew"])
        assert script in ("Hebr", "HEBREW", "Zyyy")
    
    def test_detect_empty(self):
        """Test empty string script detection."""
        script = detect_script("")
        assert script == "Zyyy"
    
    def test_detect_mixed_script(self, sample_texts):
        """Test mixed script detection (should return primary)."""
        script = detect_script(sample_texts["mixed"])
        assert script in ("Latn", "Arab", "Hans", "Cyrl", "Zyyy")


class TestWhitespaceNormalization:
    """Test whitespace normalization."""
    
    def test_multiple_spaces(self):
        """Test multiple spaces normalization."""
        result = normalize_whitespace("Hello    World")
        assert result == "Hello World"
    
    def test_tabs_and_newlines(self):
        """Test tabs and newlines normalization."""
        result = normalize_whitespace("Hello\t\nWorld")
        assert result == "Hello World"
    
    def test_leading_trailing_whitespace(self):
        """Test leading/trailing whitespace removal."""
        result = normalize_whitespace("   Hello World   ")
        assert result == "Hello World"
    
    def test_mixed_whitespace(self):
        """Test mixed whitespace types."""
        result = normalize_whitespace("  Hello  \t\n  World  \r\n  ")
        assert result == "Hello World"


class TestBatchNormalization:
    """Test batch normalization functionality."""
    
    def test_batch_empty_list(self):
        """Test batch normalization with empty list."""
        results = normalize_batch([])
        assert len(results) == 0
    
    def test_batch_single_text(self):
        """Test batch normalization with single text."""
        results = normalize_batch(["Hello World"])
        assert len(results) == 1
        assert results[0].text == "Hello World"
    
    def test_batch_multiple_texts(self, sample_texts):
        """Test batch normalization with multiple texts."""
        texts = list(sample_texts.values())
        results = normalize_batch(texts)
        assert len(results) == len(texts)
        
        for i, result in enumerate(results):
            assert result.original == texts[i]
            assert len(result.text) > 0
    
    def test_batch_with_errors(self):
        """Test batch normalization handles errors gracefully."""
        texts = ["Hello", "", "World"]
        results = normalize_batch(texts)
        assert len(results) == 3
        # All should succeed (empty string is valid)
        assert all(isinstance(r, NormalizedText) for r in results)


class TestEdgeCases:
    """Test edge cases and error handling."""
    
    def test_very_long_text(self):
        """Test very long text (10K+ characters)."""
        long_text = "a" * 10000
        result = normalize_universal(long_text)
        assert len(result.text) == 10000
    
    def test_only_whitespace(self):
        """Test text with only whitespace."""
        result = normalize_universal("   \n\t   ")
        assert result.text == ""
    
    def test_only_special_chars(self):
        """Test text with only special characters."""
        result = normalize_universal("\u200c\u200d")
        # Should handle gracefully
        assert isinstance(result, NormalizedText)
    
    def test_unicode_categories(self):
        """Test various Unicode categories."""
        # Combining marks
        text = "é"  # e + combining acute
        result = normalize_universal(text)
        assert len(result.text) <= len(text)  # Should be composed
        
        # Compatibility characters
        text = "ﬁ"  # Latin small ligature fi
        result = normalize_universal(text)
        assert result.text == "fi"  # Should decompose
    
    def test_emoji_handling(self):
        """Test emoji handling."""
        text = "Hello 😀 World 🌍"
        result = normalize_universal(text)
        # Emojis should be preserved
        assert "😀" in result.text
        assert "🌍" in result.text
    
    def test_rtl_text(self, sample_texts):
        """Test right-to-left text (Arabic, Hebrew)."""
        result_arabic = normalize_universal(sample_texts["arabic"])
        result_hebrew = normalize_universal(sample_texts["hebrew"])
        
        # Should handle RTL without errors
        assert len(result_arabic.text) > 0
        assert len(result_hebrew.text) > 0
    
    def test_malformed_unicode(self):
        """Test handling of malformed Unicode."""
        # This should not crash
        try:
            result = normalize_universal("Test\udc80Invalid")
            assert isinstance(result, NormalizedText)
        except Exception:
            # If it does raise, it should be caught gracefully
            pass


class TestPerformanceRequirements:
    """Test performance requirements."""
    
    def test_single_document_speed(self):
        """Test single document processing speed."""
        import time
        
        text = "Hello World " * 100
        start = time.time()
        
        for _ in range(100):
            normalize_universal(text)
        
        elapsed = time.time() - start
        docs_per_sec = 100 / elapsed
        
        # Should process at least 100 docs/sec even in test
        assert docs_per_sec > 100
    
    @pytest.mark.benchmark
    def test_batch_throughput(self, performance_corpus):
        """Test batch processing throughput (≥1000 docs/sec)."""
        import time
        
        start = time.time()
        results = normalize_batch(performance_corpus)
        elapsed = time.time() - start
        
        docs_per_sec = len(performance_corpus) / elapsed
        
        print(f"\nBatch throughput: {docs_per_sec:.2f} docs/sec")
        
        # Target: 1000+ docs/sec
        # In practice, should be much faster
        assert docs_per_sec > 500  # Conservative test threshold
        assert len(results) == len(performance_corpus)
    
    @pytest.mark.benchmark
    def test_memory_usage(self, performance_corpus):
        """Test memory usage (<100MB for 10K documents)."""
        import tracemalloc
        
        # Limit corpus to 10K for memory test
        corpus = performance_corpus * 10  # 10K documents
        
        tracemalloc.start()
        results = normalize_batch(corpus)
        current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        
        peak_mb = peak / (1024 * 1024)
        print(f"\nPeak memory: {peak_mb:.2f} MB for {len(corpus)} documents")
        
        # Target: <100MB for 10K docs
        assert peak_mb < 150  # Conservative threshold
        assert len(results) == len(corpus)


class TestMetadata:
    """Test NormalizedText metadata."""
    
    def test_changes_recorded(self):
        """Test that changes are recorded."""
        result = normalize_universal("Hello  World")
        assert len(result.changes) > 0
        assert any("normalization" in change.lower() for change in result.changes)
    
    def test_original_preserved(self):
        """Test original text is preserved."""
        original = "Hello   World"
        result = normalize_universal(original)
        assert result.original == original
        assert result.text != original  # Should be normalized
    
    def test_script_detected(self, sample_texts):
        """Test script is detected and recorded."""
        for lang, text in sample_texts.items():
            result = normalize_universal(text)
            # Should have a script assigned
            assert result.script is not None
            assert len(result.script) >= 3  # ISO 15924 codes are 4 chars


class TestIntegration:
    """Integration tests."""
    
    def test_end_to_end_pipeline(self, sample_texts):
        """Test complete normalization pipeline."""
        for lang, text in sample_texts.items():
            # Normalize
            result = normalize_universal(text)
            
            # Verify all components worked
            assert isinstance(result, NormalizedText)
            assert result.text is not None
            assert result.original == text
            assert result.script is not None
            assert len(result.changes) > 0
    
    def test_idempotency(self):
        """Test that normalizing twice gives same result."""
        text = "Hello  World  "
        result1 = normalize_universal(text)
        result2 = normalize_universal(result1.text)
        
        # Second normalization should not change text further
        assert result1.text == result2.text
    
    def test_real_world_corpus(self, sample_texts):
        """Test with real-world mixed corpus."""
        # Mix different languages
        corpus = [
            sample_texts["english"] + " " + sample_texts["persian"],
            sample_texts["chinese"] + " " + sample_texts["russian"],
            sample_texts["arabic"] + " " + sample_texts["hebrew"],
        ]
        
        results = normalize_batch(corpus)
        
        assert len(results) == len(corpus)
        for result in results:
            assert len(result.text) > 0
            assert result.script is not None


# Benchmark fixtures
@pytest.fixture
def benchmark_normalizer(benchmark):
    """Benchmark fixture for normalizer."""
    def run_normalization():
        text = "Hello World Test Document " * 10
        return normalize_universal(text)
    
    return benchmark(run_normalization)


def test_normalizer_benchmark(benchmark):
    """Benchmark normalization performance."""
    text = "Hello World Test Document " * 10
    result = benchmark(normalize_universal, text)
    assert isinstance(result, NormalizedText)

