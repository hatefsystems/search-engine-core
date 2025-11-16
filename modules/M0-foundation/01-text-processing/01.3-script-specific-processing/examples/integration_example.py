"""
Integration Examples - Task 01.3

Demonstrates integration with Task 01.2 (Language Detection)
and usage of script-specific processors.
"""

import sys
from pathlib import Path

# Add Task 01.2 to path for LanguageInfo import
task_01_2_path = Path(__file__).parent.parent.parent / "01.2-language-detection"
if task_01_2_path.exists():
    sys.path.insert(0, str(task_01_2_path))
    try:
        from text_processing import LanguageInfo, detect_language
        HAS_TASK_01_2 = True
    except ImportError:
        HAS_TASK_01_2 = False
        # Fallback LanguageInfo
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

# Import Task 01.3
from text_processing import (
    ScriptHandler,
    process_by_script,
    process_mixed_script,
)


def example_arabic_processing():
    """Example: Arabic script processing with ZWNJ preservation."""
    print("=" * 60)
    print("Example 1: Arabic Script Processing")
    print("=" * 60)
    
    handler = ScriptHandler()
    
    # Persian text with ZWNJ
    text = "می‌خواهم"
    language_info = LanguageInfo(
        language_code="fa",
        script_code="Arab",
        confidence=0.98
    )
    
    result = handler.process_by_script(text, language_info)
    
    print(f"Original:  {result.original}")
    print(f"Processed: {result.text}")
    print(f"Script:    {result.script_code}")
    print(f"Language:  {result.language_code}")
    print(f"Rules:     {', '.join(result.applied_rules)}")
    print(f"ZWNJ preserved: {'\u200C' in result.text}")
    print()


def example_chinese_processing():
    """Example: Chinese word segmentation."""
    print("=" * 60)
    print("Example 2: Chinese Word Segmentation")
    print("=" * 60)
    
    handler = ScriptHandler()
    
    text = "你好世界"
    language_info = LanguageInfo(
        language_code="zh",
        script_code="Hans",
        confidence=0.99
    )
    
    try:
        result = handler.process_by_script(text, language_info)
        
        print(f"Original:  {result.original}")
        print(f"Processed: {result.text}")
        print(f"Script:    {result.script_code}")
        print(f"Language:  {result.language_code}")
        print(f"Rules:     {', '.join(result.applied_rules)}")
        print(f"Word boundaries: {result.word_boundaries}")
    except ImportError as e:
        print(f"Error: {e}")
        print("Install jieba-fast: pip install jieba-fast")
    print()


def example_cyrillic_processing():
    """Example: Cyrillic variant unification."""
    print("=" * 60)
    print("Example 3: Cyrillic Variant Unification")
    print("=" * 60)
    
    handler = ScriptHandler()
    
    text = "ёлка"
    language_info = LanguageInfo(
        language_code="ru",
        script_code="Cyrl",
        confidence=0.98
    )
    
    # Normalize ё → е
    result = handler.process_by_script(text, language_info, normalize_yo=True)
    
    print(f"Original:  {result.original}")
    print(f"Processed: {result.text}")
    print(f"Script:    {result.script_code}")
    print(f"Rules:     {', '.join(result.applied_rules)}")
    print(f"ё normalized: {'ё' not in result.text}")
    print()


def example_latin_processing():
    """Example: Latin script processing."""
    print("=" * 60)
    print("Example 4: Latin Script Processing")
    print("=" * 60)
    
    handler = ScriptHandler()
    
    # English text
    text = "Hello World"
    language_info = LanguageInfo(
        language_code="en",
        script_code="Latn",
        confidence=0.99
    )
    
    result = handler.process_by_script(text, language_info)
    
    print(f"Original:  {result.original}")
    print(f"Processed: {result.text}")
    print(f"Script:    {result.script_code}")
    print(f"Rules:     {', '.join(result.applied_rules)}")
    print()
    
    # French text (preserves diacritics)
    text_fr = "café"
    language_info_fr = LanguageInfo(
        language_code="fr",
        script_code="Latn",
        confidence=0.97
    )
    
    result_fr = handler.process_by_script(text_fr, language_info_fr)
    
    print(f"French text: {result_fr.original}")
    print(f"Processed:   {result_fr.text}")
    print(f"Diacritics preserved: {'é' in result_fr.text}")
    print()


def example_mixed_script():
    """Example: Mixed-script text processing."""
    print("=" * 60)
    print("Example 5: Mixed-Script Processing")
    print("=" * 60)
    
    handler = ScriptHandler()
    
    # Arabic + Latin mixed
    text = "Hello سلام World"
    language_info = LanguageInfo(
        language_code="fa",
        script_code="Arab",
        confidence=0.95,
        is_mixed_content=True
    )
    
    result = handler.process_mixed_script(text, language_info)
    
    print(f"Original:  {result.original}")
    print(f"Processed: {result.text}")
    print(f"Scripts detected: Multiple")
    print(f"Rules:     {', '.join(result.applied_rules)}")
    print()


def example_integration_with_task_01_2():
    """Example: Integration with Task 01.2 language detection."""
    print("=" * 60)
    print("Example 6: Integration with Task 01.2")
    print("=" * 60)
    
    if not HAS_TASK_01_2:
        print("Task 01.2 not available - using mock LanguageInfo")
        print()
        return
    
    # Detect language first
    texts = [
        "Hello World",
        "می‌خواهم",
        "你好世界",
        "Привет мир"
    ]
    
    handler = ScriptHandler()
    
    for text in texts:
        print(f"\nText: {text}")
        
        # Detect language (Task 01.2)
        language_info = detect_language(text)
        
        print(f"Detected: {language_info.language_code} ({language_info.script_code})")
        print(f"Confidence: {language_info.confidence:.2f}")
        
        # Process by script (Task 01.3)
        result = handler.process_by_script(text, language_info)
        
        print(f"Processed: {result.text}")
        print(f"Rules: {', '.join(result.applied_rules)}")
    print()


def example_performance_benchmark():
    """Example: Performance benchmarking."""
    print("=" * 60)
    print("Example 7: Performance Benchmark")
    print("=" * 60)
    
    import time
    
    handler = ScriptHandler()
    
    # Test texts
    texts = {
        "Arabic": ("می‌خواهم", LanguageInfo("fa", "Arab", 0.98)),
        "Latin": ("Hello World", LanguageInfo("en", "Latn", 0.99)),
        "Cyrillic": ("Привет", LanguageInfo("ru", "Cyrl", 0.98)),
    }
    
    iterations = 1000
    
    for name, (text, lang_info) in texts.items():
        start = time.time()
        for _ in range(iterations):
            handler.process_by_script(text, lang_info)
        elapsed = time.time() - start
        
        throughput = iterations / elapsed
        avg_latency = (elapsed / iterations) * 1000  # ms
        
        print(f"{name}:")
        print(f"  Throughput: {throughput:.0f} docs/sec")
        print(f"  Avg Latency: {avg_latency:.2f} ms")
    print()


def main():
    """Run all examples."""
    print("\n" + "=" * 60)
    print("Script-Specific Processing - Integration Examples")
    print("=" * 60 + "\n")
    
    example_arabic_processing()
    example_chinese_processing()
    example_cyrillic_processing()
    example_latin_processing()
    example_mixed_script()
    example_integration_with_task_01_2()
    example_performance_benchmark()
    
    print("=" * 60)
    print("Examples completed!")
    print("=" * 60)


if __name__ == "__main__":
    main()
