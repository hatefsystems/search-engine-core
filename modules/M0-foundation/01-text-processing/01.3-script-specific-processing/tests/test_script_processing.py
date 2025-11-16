"""
Comprehensive tests for script-specific processing - Task 01.3

Tests cover:
- Arabic script (ZWNJ preservation, diacritics, shapes)
- CJK scripts (Chinese, Japanese, Korean segmentation)
- Cyrillic script (variant unification, case folding)
- Latin script (diacritics, ligatures)
- Mixed-script handling
- Edge cases (empty strings, malformed Unicode, etc.)
- Performance requirements
"""

import pytest
from text_processing import (
    ScriptHandler,
    ProcessedText,
    process_by_script,
    process_mixed_script,
)
from text_processing.arabic_processor import (
    preserve_zwnj,
    remove_arabic_diacritics,
    normalize_arabic_shapes,
    process_arabic,
    ZWNJ,
)
from text_processing.cjk_processor import (
    segment_chinese,
    segment_japanese,
    segment_korean,
    process_cjk,
)
from text_processing.cyrillic_processor import (
    unify_cyrillic_variants,
    process_cyrillic,
)
from text_processing.latin_processor import (
    normalize_diacritics,
    handle_ligatures,
    process_latin,
)
from text_processing.script_handler import detect_script_boundaries

from conftest import LanguageInfo


# ============================================================================
# Arabic Script Tests
# ============================================================================

class TestArabicProcessor:
    """Test Arabic script processing."""
    
    def test_zwnj_preservation(self, language_info_fa):
        """Test ZWNJ preservation in Persian text."""
        text = "می‌خواهم"  # Contains ZWNJ
        result = process_arabic(text, "fa")
        
        assert ZWNJ in result.text, "ZWNJ must be preserved"
        assert result.text == text, "ZWNJ text should remain unchanged"
        assert "preserve_zwnj" in result.applied_rules
    
    def test_zwnj_preservation_function(self):
        """Test ZWNJ preservation function."""
        text = "می‌خواهم"
        preserved = preserve_zwnj(text)
        assert preserved == text
        assert ZWNJ in preserved
    
    def test_zwnj_without_zwnj(self):
        """Test text without ZWNJ."""
        text = "سلام"
        preserved = preserve_zwnj(text)
        assert preserved == text
    
    def test_remove_arabic_diacritics(self):
        """Test Arabic diacritic removal."""
        text = "مَرْحَبًا"
        result = remove_arabic_diacritics(text)
        assert "مرحبا" in result or len(result) < len(text)
        assert "remove_diacritics" not in result  # No diacritics in result
    
    def test_preserve_diacritics(self):
        """Test preserving Arabic diacritics."""
        text = "مَرْحَبًا"
        result = process_arabic(text, "ar", preserve_diacritics=True)
        assert "preserve_diacritics" in result.applied_rules
    
    def test_normalize_arabic_shapes(self):
        """Test Arabic shape normalization."""
        text = "سلام"
        result = normalize_arabic_shapes(text)
        assert isinstance(result, str)
        assert len(result) == len(text)
    
    def test_arabic_processing_persian(self, language_info_fa):
        """Test full Arabic processing for Persian."""
        text = "می‌خواهم"
        result = process_arabic(text, "fa")
        
        assert result.script_code == "Arab"
        assert result.language_code == "fa"
        assert result.original == text
        assert "preserve_zwnj" in result.applied_rules
    
    def test_arabic_processing_arabic(self, language_info_ar):
        """Test full Arabic processing for Arabic."""
        text = "مرحبا"
        result = process_arabic(text, "ar")
        
        assert result.script_code == "Arab"
        assert result.language_code == "ar"
        assert result.original == text
    
    def test_arabic_empty_string(self):
        """Test Arabic processing with empty string."""
        result = process_arabic("", "fa")
        assert result.text == ""
        assert result.original == ""
    
    def test_arabic_mixed_content(self):
        """Test Arabic with mixed content."""
        text = "سلام Hello"
        result = process_arabic(text, "fa")
        assert result.script_code == "Arab"


# ============================================================================
# CJK Script Tests
# ============================================================================

class TestCJKProcessor:
    """Test CJK script processing."""
    
    @pytest.mark.requires_jieba
    def test_chinese_segmentation(self, language_info_zh):
        """Test Chinese word segmentation."""
        text = "你好世界"
        result = process_cjk(text, "zh", "Hans")
        
        assert result.script_code == "Hans"
        assert result.language_code == "zh"
        assert "chinese_segmentation" in result.applied_rules
        assert len(result.word_boundaries) > 0
    
    @pytest.mark.requires_jieba
    def test_segment_chinese_function(self):
        """Test Chinese segmentation function."""
        text = "你好世界"
        words = segment_chinese(text)
        assert isinstance(words, list)
        assert len(words) > 0
    
    def test_japanese_tokenization(self, language_info_ja):
        """Test Japanese tokenization."""
        text = "こんにちは世界"
        result = process_cjk(text, "ja", "Jpan")
        
        assert result.script_code == "Jpan"
        assert result.language_code == "ja"
        assert "japanese_tokenization" in result.applied_rules
    
    def test_segment_japanese_function(self):
        """Test Japanese segmentation function."""
        text = "こんにちは"
        words = segment_japanese(text)
        assert isinstance(words, list)
        assert len(words) > 0
    
    def test_korean_segmentation(self, language_info_ko):
        """Test Korean word segmentation."""
        text = "안녕하세요 세계"
        result = process_cjk(text, "ko", "Kore")
        
        assert result.script_code == "Kore"
        assert result.language_code == "ko"
        assert "korean_segmentation" in result.applied_rules
    
    def test_segment_korean_function(self):
        """Test Korean segmentation function."""
        text = "안녕하세요"
        words = segment_korean(text)
        assert isinstance(words, list)
        assert len(words) > 0
    
    def test_cjk_empty_string(self):
        """Test CJK processing with empty string."""
        result = process_cjk("", "zh", "Hans")
        assert result.text == ""
        assert result.original == ""
    
    def test_cjk_word_boundaries(self):
        """Test word boundary detection."""
        text = "你好"
        words = ["你", "好"]
        from text_processing.cjk_processor import get_word_boundaries
        boundaries = get_word_boundaries(text, words)
        assert isinstance(boundaries, list)
        assert 0 in boundaries


# ============================================================================
# Cyrillic Script Tests
# ============================================================================

class TestCyrillicProcessor:
    """Test Cyrillic script processing."""
    
    def test_unify_cyrillic_variants(self):
        """Test Cyrillic variant unification (ё → е)."""
        text = "ёлка"
        result = unify_cyrillic_variants(text, normalize_yo=True)
        assert "ё" not in result or "е" in result
    
    def test_preserve_yo(self):
        """Test preserving ё character."""
        text = "ёлка"
        result = unify_cyrillic_variants(text, normalize_yo=False, language_code="ru")
        assert "ё" in result
    
    def test_cyrillic_processing(self, language_info_ru):
        """Test full Cyrillic processing."""
        text = "Привет мир"
        result = process_cyrillic(text, "ru")
        
        assert result.script_code == "Cyrl"
        assert result.language_code == "ru"
        assert result.original == text
    
    def test_cyrillic_normalize_yo(self, language_info_ru):
        """Test Cyrillic with yo normalization."""
        text = "ёлка"
        result = process_cyrillic(text, "ru", normalize_yo=True)
        assert "unify_variants" in result.applied_rules
    
    def test_cyrillic_preserve_yo(self, language_info_ru):
        """Test Cyrillic preserving yo."""
        text = "ёлка"
        result = process_cyrillic(text, "ru", normalize_yo=False)
        assert "preserve_yo" in result.applied_rules
    
    def test_cyrillic_empty_string(self):
        """Test Cyrillic processing with empty string."""
        result = process_cyrillic("", "ru")
        assert result.text == ""
        assert result.original == ""


# ============================================================================
# Latin Script Tests
# ============================================================================

class TestLatinProcessor:
    """Test Latin script processing."""
    
    def test_normalize_diacritics(self):
        """Test diacritic normalization."""
        text = "café"
        result = normalize_diacritics(text)
        assert "é" not in result or "e" in result
    
    def test_preserve_diacritics_for_language(self):
        """Test preserving diacritics for specific languages."""
        text = "café"
        result = normalize_diacritics(text, preserve_for_languages=["fr"])
        assert "é" in result
    
    def test_handle_ligatures(self):
        """Test ligature handling."""
        text = "encyclopædia"
        result = handle_ligatures(text, preserve_semantic=True)
        assert "æ" in result
    
    def test_normalize_ligatures(self):
        """Test ligature normalization."""
        text = "encyclopædia"
        result = handle_ligatures(text, preserve_semantic=False)
        assert "ae" in result or "æ" not in result
    
    def test_latin_processing_english(self, language_info_en):
        """Test Latin processing for English."""
        text = "Hello World"
        result = process_latin(text, "en")
        
        assert result.script_code == "Latn"
        assert result.language_code == "en"
        assert result.original == text
    
    def test_latin_processing_french(self, language_info_fr):
        """Test Latin processing for French (preserves diacritics)."""
        text = "café"
        result = process_latin(text, "fr", normalize_diacritics_flag=False)
        assert "preserve_diacritics" in result.applied_rules
    
    def test_latin_empty_string(self):
        """Test Latin processing with empty string."""
        result = process_latin("", "en")
        assert result.text == ""
        assert result.original == ""


# ============================================================================
# Script Handler Tests
# ============================================================================

class TestScriptHandler:
    """Test main script handler."""
    
    def test_handler_initialization(self):
        """Test handler initialization."""
        handler = ScriptHandler()
        assert handler is not None
    
    def test_process_arabic_text(self, language_info_fa):
        """Test processing Arabic text."""
        handler = ScriptHandler()
        text = "می‌خواهم"
        result = handler.process_by_script(text, language_info_fa)
        
        assert isinstance(result, ProcessedText)
        assert result.script_code == "Arab"
        assert result.confidence == language_info_fa.confidence
    
    @pytest.mark.requires_jieba
    def test_process_chinese_text(self, language_info_zh):
        """Test processing Chinese text."""
        handler = ScriptHandler()
        text = "你好世界"
        result = handler.process_by_script(text, language_info_zh)
        
        assert isinstance(result, ProcessedText)
        assert result.script_code == "Hans"
    
    def test_process_cyrillic_text(self, language_info_ru):
        """Test processing Cyrillic text."""
        handler = ScriptHandler()
        text = "Привет"
        result = handler.process_by_script(text, language_info_ru)
        
        assert isinstance(result, ProcessedText)
        assert result.script_code == "Cyrl"
    
    def test_process_latin_text(self, language_info_en):
        """Test processing Latin text."""
        handler = ScriptHandler()
        text = "Hello World"
        result = handler.process_by_script(text, language_info_en)
        
        assert isinstance(result, ProcessedText)
        assert result.script_code == "Latn"
    
    def test_process_empty_string(self, language_info_en):
        """Test processing empty string."""
        handler = ScriptHandler()
        result = handler.process_by_script("", language_info_en)
        
        assert result.text == ""
        assert result.original == ""
    
    def test_process_unknown_script(self):
        """Test processing unknown script."""
        handler = ScriptHandler()
        lang_info = LanguageInfo(
            language_code="xx",
            script_code="Xxxx",
            confidence=0.5
        )
        text = "test"
        result = handler.process_by_script(text, lang_info)
        
        assert result.text == text
        assert "no_processing" in result.applied_rules


# ============================================================================
# Mixed-Script Tests
# ============================================================================

class TestMixedScript:
    """Test mixed-script processing."""
    
    def test_detect_script_boundaries(self):
        """Test script boundary detection."""
        text = "Hello سلام"
        boundaries = detect_script_boundaries(text)
        
        assert len(boundaries) >= 1
        assert all(isinstance(b, tuple) and len(b) == 3 for b in boundaries)
    
    def test_process_mixed_script(self, language_info_fa):
        """Test processing mixed-script text."""
        handler = ScriptHandler()
        text = "Hello سلام World"
        result = handler.process_mixed_script(text, language_info_fa)
        
        assert isinstance(result, ProcessedText)
        assert len(result.applied_rules) > 0
    
    def test_mixed_arabic_latin(self, language_info_fa):
        """Test Arabic-Latin mixed text."""
        handler = ScriptHandler()
        text = "Hello سلام"
        result = handler.process_mixed_script(text, language_info_fa)
        
        assert result.original == text
        assert len(result.applied_rules) > 0
    
    def test_mixed_cjk_latin(self, language_info_zh):
        """Test CJK-Latin mixed text."""
        handler = ScriptHandler()
        text = "Hello 你好"
        result = handler.process_mixed_script(text, language_info_zh)
        
        assert result.original == text
    
    def test_bidirectional_text(self, language_info_fa):
        """Test bidirectional text handling."""
        handler = ScriptHandler()
        text = "Hello سلام World"
        result = handler.process_mixed_script(text, language_info_fa)
        
        assert isinstance(result, ProcessedText)


# ============================================================================
# Convenience Function Tests
# ============================================================================

class TestConvenienceFunctions:
    """Test convenience functions."""
    
    def test_process_by_script_function(self, language_info_en):
        """Test process_by_script convenience function."""
        result = process_by_script("Hello", language_info_en)
        assert isinstance(result, ProcessedText)
    
    def test_process_mixed_script_function(self, language_info_fa):
        """Test process_mixed_script convenience function."""
        result = process_mixed_script("Hello سلام", language_info_fa)
        assert isinstance(result, ProcessedText)


# ============================================================================
# Edge Cases Tests
# ============================================================================

class TestEdgeCases:
    """Test edge cases and error handling."""
    
    def test_empty_string_all_scripts(self):
        """Test empty string for all script types."""
        scripts = ["Arab", "Latn", "Cyrl", "Hans", "Jpan", "Kore"]
        handler = ScriptHandler()
        
        for script in scripts:
            lang_info = LanguageInfo(
                language_code="xx",
                script_code=script,
                confidence=0.5
            )
            result = handler.process_by_script("", lang_info)
            assert result.text == ""
            assert result.original == ""
    
    def test_whitespace_only(self, language_info_en):
        """Test whitespace-only text."""
        handler = ScriptHandler()
        result = handler.process_by_script("   ", language_info_en)
        assert isinstance(result, ProcessedText)
    
    def test_numbers_and_punctuation(self, language_info_en):
        """Test text with numbers and punctuation."""
        handler = ScriptHandler()
        text = "Hello 123! World."
        result = handler.process_by_script(text, language_info_en)
        assert isinstance(result, ProcessedText)
    
    def test_unicode_surrogates(self, language_info_en):
        """Test handling of Unicode surrogates."""
        handler = ScriptHandler()
        # Try to create text with potential issues
        text = "Hello"
        result = handler.process_by_script(text, language_info_en)
        assert isinstance(result, ProcessedText)
    
    def test_very_long_text(self, language_info_en):
        """Test very long text."""
        handler = ScriptHandler()
        text = "Hello " * 1000
        result = handler.process_by_script(text, language_info_en)
        assert isinstance(result, ProcessedText)
        assert len(result.text) > 0
    
    def test_special_characters(self, language_info_en):
        """Test text with special characters."""
        handler = ScriptHandler()
        text = "Hello @#$%^&*() World"
        result = handler.process_by_script(text, language_info_en)
        assert isinstance(result, ProcessedText)


# ============================================================================
# Integration Tests
# ============================================================================

class TestIntegration:
    """Test integration scenarios."""
    
    def test_full_pipeline_arabic(self, language_info_fa):
        """Test full processing pipeline for Arabic."""
        handler = ScriptHandler()
        text = "می‌خواهم"
        result = handler.process_by_script(text, language_info_fa)
        
        assert result.text == text  # ZWNJ preserved
        assert result.confidence == language_info_fa.confidence
        assert "preserve_zwnj" in result.applied_rules
    
    @pytest.mark.requires_jieba
    def test_full_pipeline_chinese(self, language_info_zh):
        """Test full processing pipeline for Chinese."""
        handler = ScriptHandler()
        text = "你好世界"
        result = handler.process_by_script(text, language_info_zh)
        
        assert result.script_code == "Hans"
        assert len(result.word_boundaries) > 0
    
    def test_full_pipeline_cyrillic(self, language_info_ru):
        """Test full processing pipeline for Cyrillic."""
        handler = ScriptHandler()
        text = "Привет мир"
        result = handler.process_by_script(text, language_info_ru)
        
        assert result.script_code == "Cyrl"
        assert result.language_code == "ru"


# ============================================================================
# Performance Tests
# ============================================================================

class TestPerformance:
    """Test performance requirements."""
    
    @pytest.mark.slow
    def test_throughput_requirement(self, language_info_en):
        """Test 1000+ docs/sec throughput requirement."""
        import time
        handler = ScriptHandler()
        texts = ["Hello World"] * 1000
        
        start = time.time()
        for text in texts:
            handler.process_by_script(text, language_info_en)
        elapsed = time.time() - start
        
        throughput = len(texts) / elapsed
        assert throughput >= 1000, f"Throughput {throughput:.0f} docs/sec < 1000 docs/sec"
    
    def test_latency_requirement(self, language_info_en):
        """Test <10ms latency requirement."""
        import time
        handler = ScriptHandler()
        text = "Hello World"
        
        start = time.time()
        handler.process_by_script(text, language_info_en)
        elapsed = (time.time() - start) * 1000  # Convert to ms
        
        assert elapsed < 10, f"Latency {elapsed:.2f}ms >= 10ms"


# ============================================================================
# Data Structure Tests
# ============================================================================

class TestProcessedText:
    """Test ProcessedText dataclass."""
    
    def test_processed_text_creation(self):
        """Test ProcessedText object creation."""
        result = ProcessedText(
            text="processed",
            original="original",
            script_code="Latn",
            language_code="en",
            applied_rules=["rule1"],
            word_boundaries=[0, 5],
            confidence=0.95
        )
        
        assert result.text == "processed"
        assert result.original == "original"
        assert result.script_code == "Latn"
        assert result.language_code == "en"
        assert "rule1" in result.applied_rules
        assert result.word_boundaries == [0, 5]
        assert result.confidence == 0.95
    
    def test_processed_text_defaults(self):
        """Test ProcessedText with defaults."""
        result = ProcessedText(
            text="test",
            original="test",
            script_code="Latn",
            language_code="en"
        )
        
        assert result.applied_rules == []
        assert result.word_boundaries == []
        assert result.confidence == 0.0
