"""
Integration Tests - Task 01.3

Tests integration with Tasks 01.1 and 01.2.
"""

import sys
from pathlib import Path
import importlib

# STEP 1: Import Task 01.2 FIRST (before fixing paths for Task 01.3)
# Task 01.2 is installed in editable mode, so we need to explicitly import from its directory
task_01_2_path = Path(__file__).parent.parent.parent / "01.2-language-detection"
HAS_TASK_01_2 = False
Task01_2_LanguageInfo = None
Task01_2_detect_language = None

if task_01_2_path.exists():
    try:
        # Temporarily add Task 01.2's directory to sys.path FIRST
        task_01_2_str = str(task_01_2_path)
        if task_01_2_str not in sys.path:
            sys.path.insert(0, task_01_2_str)
        
        # Clear any cached text_processing module
        if 'text_processing' in sys.modules:
            del sys.modules['text_processing']
        importlib.invalidate_caches()
        
        # Now import Task 01.2's text_processing
        import text_processing as task_01_2_text_processing
        
        # Verify it's actually Task 01.2 (has LanguageInfo and detect_language)
        if hasattr(task_01_2_text_processing, 'LanguageInfo') and hasattr(task_01_2_text_processing, 'detect_language'):
            # Store Task 01.2's exports before we switch to Task 01.3
            Task01_2_LanguageInfo = task_01_2_text_processing.LanguageInfo
            Task01_2_detect_language = task_01_2_text_processing.detect_language
            HAS_TASK_01_2 = True
        
        # Keep Task 01.2 in sys.path (but move it after Task 01.3) 
        # so Task 01.2's internal imports work when detect_language is called
        # We'll ensure Task 01.3 is first when we import text_processing
            
    except (ImportError, AttributeError) as e:
        HAS_TASK_01_2 = False
        Task01_2_LanguageInfo = None
        Task01_2_detect_language = None
        # Make sure to remove Task 01.2 path even on error
        if task_01_2_str in sys.path:
            sys.path.remove(task_01_2_str)

# STEP 2: Now fix import path to prioritize Task 01.3
module_dir = Path(__file__).parent.parent
module_dir_str = str(module_dir)

# Ensure Task 01.3's module directory is FIRST (before Task 01.2)
# This way, when we import text_processing, we get Task 01.3's version
# But Task 01.2's internal imports will still work because its path is in sys.path
if module_dir_str in sys.path:
    sys.path.remove(module_dir_str)
sys.path.insert(0, module_dir_str)

# Ensure Task 01.2's path is still in sys.path (but after Task 01.3)
# so Task 01.2's internal imports work when detect_language is called
if task_01_2_path.exists():
    task_01_2_str = str(task_01_2_path)
    if task_01_2_str not in sys.path:
        sys.path.append(task_01_2_str)  # Append to end, not insert at beginning

# Clear import cache to force re-import of text_processing
importlib.invalidate_caches()

# Remove Task 01.2's text_processing from cache if it exists
if 'text_processing' in sys.modules:
    del sys.modules['text_processing']

import pytest

# STEP 3: Now import Task 01.3's text_processing (script processing)
from text_processing import ScriptHandler, process_by_script

# STEP 4: Set up LanguageInfo and detect_language for use in tests
if HAS_TASK_01_2:
    # Use Task 01.2's LanguageInfo
    LanguageInfo = Task01_2_LanguageInfo
    
    # Create a wrapper for detect_language that temporarily prioritizes Task 01.2's path
    # This ensures Task 01.2's internal imports work correctly
    def detect_language_wrapper(text):
        """Wrapper that ensures Task 01.2's imports work when calling detect_language."""
        task_01_2_str = str(task_01_2_path)
        module_dir_str = str(Path(__file__).parent.parent)
        
        # Save current state
        task_01_2_was_in_path = task_01_2_str in sys.path
        task_01_2_index = sys.path.index(task_01_2_str) if task_01_2_was_in_path else -1
        module_dir_was_in_path = module_dir_str in sys.path
        module_dir_index = sys.path.index(module_dir_str) if module_dir_was_in_path else -1
        
        # Clear cached text_processing modules to force reload from correct location
        modules_to_remove = [k for k in sys.modules.keys() if k.startswith('text_processing')]
        for mod_name in modules_to_remove:
            del sys.modules[mod_name]
        
        # Ensure Task 01.2's path is FIRST
        if not task_01_2_was_in_path:
            sys.path.insert(0, task_01_2_str)
        elif task_01_2_index > 0:
            sys.path.remove(task_01_2_str)
            sys.path.insert(0, task_01_2_str)
        
        # Ensure Task 01.3's path is SECOND (not first)
        if module_dir_was_in_path and module_dir_index == 0:
            sys.path.remove(module_dir_str)
            sys.path.insert(1, module_dir_str)
        elif not module_dir_was_in_path:
            sys.path.insert(1, module_dir_str)
        
        try:
            # Re-import Task 01.2's text_processing with correct path context
            importlib.invalidate_caches()
            import text_processing as task_01_2_reloaded
            
            # Verify we got Task 01.2's version
            if not hasattr(task_01_2_reloaded, 'detect_language'):
                raise ImportError("Failed to import Task 01.2's detect_language")
            
            detect_language_func = task_01_2_reloaded.detect_language
            result = detect_language_func(text)
            return result
        finally:
            # Clear modules again
            modules_to_remove = [k for k in sys.modules.keys() if k.startswith('text_processing')]
            for mod_name in modules_to_remove:
                del sys.modules[mod_name]
            importlib.invalidate_caches()
            
            # Restore original path order: Task 01.3 first, Task 01.2 after
            if task_01_2_str in sys.path:
                sys.path.remove(task_01_2_str)
            if module_dir_str in sys.path:
                sys.path.remove(module_dir_str)
            
            # Restore Task 01.3's path priority (for subsequent imports in other tests)
            sys.path.insert(0, module_dir_str)
            if task_01_2_was_in_path:
                sys.path.append(task_01_2_str)
    
    detect_language = detect_language_wrapper
else:
    # Fallback: Create a minimal LanguageInfo for tests that don't need Task 01.2
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
    
    detect_language = None


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
        # Task 01.2 may return 'zh' or 'zh-cn', both are valid
        assert result.language_code.startswith("zh")
    
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
        # Task 01.2 may detect various Cyrillic languages (ru, bg, uk, etc.)
        # The important thing is that script processing works correctly
        assert result.language_code in ("ru", "bg", "uk", "sr", "mk") or result.language_code.startswith("ru-") or result.language_code.startswith("bg-")
    
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
