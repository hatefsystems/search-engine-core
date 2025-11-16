"""
Integration Tests - Task 01.3

Tests integration with Tasks 01.1 and 01.2.
"""

import pytest
import sys
from pathlib import Path

# Try to import Task 01.2
task_01_2_path = Path(__file__).parent.parent.parent / "01.2-language-detection"
if task_01_2_path.exists():
    sys.path.insert(0, str(task_01_2_path))
    try:
        from text_processing import LanguageInfo, detect_language
        HAS_TASK_01_2 = True
    except ImportError:
        HAS_TASK_01_2 = False
        from dataclasses import dataclass
        from typing import List, Tuple
        
        @dataclass
        class LanguageInfo:
            language_code: str
            script_code: str
            confidence: float
            is_mixed_content: bool = False
            detected_languages: List[Tuple[str, float]] = None
            
            def __post_init__(self):
                if self.detected_languages is None:
                    self.detected_languages = []
else:
    HAS_TASK_01_2 = False
    from dataclasses import dataclass
    from typing import List, Tuple
    
    @dataclass
    class LanguageInfo:
        language_code: str
        script_code: str
        confidence: float
        is_mixed_content: bool = False
        detected_languages: List[Tuple[str, float]] = None
        
        def __post_init__(self):
            if self.detected_languages is None:
                self.detected_languages = []

from text_processing import ScriptHandler, process_by_script


@pytest.mark.integration
class TestTask01_2Integration:
    """Test integration with Task 01.2 (Language Detection)."""
    
    @pytest.mark.skipif(not HAS_TASK_01_2, reason="Task 01.2 not available")
    def test_detect_and_process_arabic(self):
        """Test language detection + script processing for Arabic."""
        text = "می‌خواهم"
        
        # Detect language (Task 01.2)
        language_info = detect_language(text)
        
        # Process by script (Task 01.3)
        handler = ScriptHandler()
        result = handler.process_by_script(text, language_info)
        
        assert result.script_code == "Arab"
        assert result.language_code == "fa"
        assert result.confidence == language_info.confidence
    
    @pytest.mark.skipif(not HAS_TASK_01_2, reason="Task 01.2 not available")
    def test_detect_and_process_chinese(self):
        """Test language detection + script processing for Chinese."""
        text = "你好世界"
        
        # Detect language (Task 01.2)
        language_info = detect_language(text)
        
        # Process by script (Task 01.3)
        handler = ScriptHandler()
        result = handler.process_by_script(text, language_info)
        
        assert result.script_code in ("Hans", "Hant")
        assert result.language_code == "zh"
    
    @pytest.mark.skipif(not HAS_TASK_01_2, reason="Task 01.2 not available")
    def test_detect_and_process_cyrillic(self):
        """Test language detection + script processing for Cyrillic."""
        text = "Привет мир"
        
        # Detect language (Task 01.2)
        language_info = detect_language(text)
        
        # Process by script (Task 01.3)
        handler = ScriptHandler()
        result = handler.process_by_script(text, language_info)
        
        assert result.script_code == "Cyrl"
        assert result.language_code == "ru"
    
    @pytest.mark.skipif(not HAS_TASK_01_2, reason="Task 01.2 not available")
    def test_detect_and_process_latin(self):
        """Test language detection + script processing for Latin."""
        text = "Hello World"
        
        # Detect language (Task 01.2)
        language_info = detect_language(text)
        
        # Process by script (Task 01.3)
        handler = ScriptHandler()
        result = handler.process_by_script(text, language_info)
        
        assert result.script_code == "Latn"
        assert result.language_code == "en"
    
    @pytest.mark.skipif(not HAS_TASK_01_2, reason="Task 01.2 not available")
    def test_detect_and_process_mixed(self):
        """Test language detection + script processing for mixed text."""
        text = "Hello سلام"
        
        # Detect language (Task 01.2)
        language_info = detect_language(text)
        
        # Process mixed script (Task 01.3)
        handler = ScriptHandler()
        result = handler.process_mixed_script(text, language_info)
        
        assert result.original == text
        assert len(result.applied_rules) > 0


@pytest.mark.integration
class TestFullPipeline:
    """Test full processing pipeline."""
    
    def test_arabic_pipeline(self):
        """Test full Arabic processing pipeline."""
        handler = ScriptHandler()
        text = "می‌خواهم"
        language_info = LanguageInfo("fa", "Arab", 0.98)
        
        result = handler.process_by_script(text, language_info)
        
        # Verify ZWNJ preserved
        assert '\u200C' in result.text
        assert result.script_code == "Arab"
        assert "preserve_zwnj" in result.applied_rules
    
    def test_cjk_pipeline(self):
        """Test full CJK processing pipeline."""
        handler = ScriptHandler()
        text = "你好世界"
        language_info = LanguageInfo("zh", "Hans", 0.99)
        
        try:
            result = handler.process_by_script(text, language_info)
            assert result.script_code == "Hans"
            assert result.language_code == "zh"
        except ImportError:
            pytest.skip("jieba not available")
    
    def test_cyrillic_pipeline(self):
        """Test full Cyrillic processing pipeline."""
        handler = ScriptHandler()
        text = "ёлка"
        language_info = LanguageInfo("ru", "Cyrl", 0.98)
        
        result = handler.process_by_script(text, language_info, normalize_yo=True)
        assert result.script_code == "Cyrl"
        assert "unify_variants" in result.applied_rules
    
    def test_latin_pipeline(self):
        """Test full Latin processing pipeline."""
        handler = ScriptHandler()
        text = "Hello World"
        language_info = LanguageInfo("en", "Latn", 0.99)
        
        result = handler.process_by_script(text, language_info)
        assert result.script_code == "Latn"
        assert result.language_code == "en"


@pytest.mark.integration
class TestConvenienceFunctions:
    """Test convenience functions work correctly."""
    
    def test_process_by_script_function(self):
        """Test process_by_script convenience function."""
        text = "Hello"
        language_info = LanguageInfo("en", "Latn", 0.99)
        
        result = process_by_script(text, language_info)
        assert result.text == text
        assert result.script_code == "Latn"
    
    def test_process_mixed_script_function(self):
        """Test process_mixed_script convenience function."""
        from text_processing import process_mixed_script
        
        text = "Hello سلام"
        language_info = LanguageInfo("fa", "Arab", 0.95)
        
        result = process_mixed_script(text, language_info)
        assert result.original == text
