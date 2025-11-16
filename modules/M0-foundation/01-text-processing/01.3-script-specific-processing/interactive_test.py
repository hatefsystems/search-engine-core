#!/usr/bin/env python3
"""
Interactive Testing Tool - Task 01.3

Interactive CLI for testing script-specific processing.
"""

import sys
from pathlib import Path

# Add Task 01.2 to path
task_01_2_path = Path(__file__).parent.parent / "01.2-language-detection"
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


def print_result(result):
    """Print processing result."""
    print("\n" + "=" * 60)
    print("Processing Result")
    print("=" * 60)
    print(f"Original:     {result.original}")
    print(f"Processed:     {result.text}")
    print(f"Script:        {result.script_code}")
    print(f"Language:      {result.language_code}")
    print(f"Confidence:    {result.confidence:.2f}")
    print(f"Rules Applied: {', '.join(result.applied_rules)}")
    if result.word_boundaries:
        print(f"Word Boundaries: {result.word_boundaries}")
    print("=" * 60 + "\n")


def interactive_mode():
    """Interactive mode for testing."""
    handler = ScriptHandler()
    
    print("\n" + "=" * 60)
    print("Script-Specific Processing - Interactive Test")
    print("=" * 60)
    print("\nEnter text to process (or 'quit' to exit)")
    print("Examples:")
    print("  - Persian: می‌خواهم")
    print("  - Chinese: 你好世界")
    print("  - Russian: Привет")
    print("  - English: Hello World")
    print("  - Mixed:   Hello سلام")
    print("=" * 60 + "\n")
    
    while True:
        try:
            text = input("Enter text: ").strip()
            
            if text.lower() in ('quit', 'exit', 'q'):
                print("Goodbye!")
                break
            
            if not text:
                continue
            
            # Try to detect language if Task 01.2 available
            if HAS_TASK_01_2:
                try:
                    language_info = detect_language(text)
                    print(f"\nDetected: {language_info.language_code} ({language_info.script_code})")
                except Exception as e:
                    print(f"\nLanguage detection failed: {e}")
                    print("Using default language info...")
                    language_info = LanguageInfo(
                        language_code="en",
                        script_code="Latn",
                        confidence=0.5
                    )
            else:
                # Manual language selection
                print("\nSelect language:")
                print("1. Persian (fa)")
                print("2. Arabic (ar)")
                print("3. Chinese (zh)")
                print("4. Japanese (ja)")
                print("5. Korean (ko)")
                print("6. Russian (ru)")
                print("7. English (en)")
                print("8. French (fr)")
                
                choice = input("Choice (1-8, default 7): ").strip() or "7"
                
                lang_map = {
                    "1": ("fa", "Arab"),
                    "2": ("ar", "Arab"),
                    "3": ("zh", "Hans"),
                    "4": ("ja", "Jpan"),
                    "5": ("ko", "Kore"),
                    "6": ("ru", "Cyrl"),
                    "7": ("en", "Latn"),
                    "8": ("fr", "Latn"),
                }
                
                lang_code, script_code = lang_map.get(choice, ("en", "Latn"))
                language_info = LanguageInfo(
                    language_code=lang_code,
                    script_code=script_code,
                    confidence=0.95
                )
            
            # Process text
            try:
                result = handler.process_by_script(text, language_info)
                print_result(result)
            except Exception as e:
                print(f"\nError processing text: {e}")
                import traceback
                traceback.print_exc()
                print()
        
        except KeyboardInterrupt:
            print("\n\nGoodbye!")
            break
        except EOFError:
            print("\n\nGoodbye!")
            break


def batch_mode(texts):
    """Batch mode for processing multiple texts."""
    handler = ScriptHandler()
    
    print("\n" + "=" * 60)
    print("Batch Processing")
    print("=" * 60 + "\n")
    
    for i, text in enumerate(texts, 1):
        print(f"Text {i}: {text}")
        
        # Detect language if available
        if HAS_TASK_01_2:
            try:
                language_info = detect_language(text)
            except Exception:
                language_info = LanguageInfo("en", "Latn", 0.5)
        else:
            language_info = LanguageInfo("en", "Latn", 0.5)
        
        # Process
        try:
            result = handler.process_by_script(text, language_info)
            print(f"  → {result.text} ({result.script_code})")
        except Exception as e:
            print(f"  → Error: {e}")
        print()


def main():
    """Main entry point."""
    if len(sys.argv) > 1:
        # Batch mode
        texts = sys.argv[1:]
        batch_mode(texts)
    else:
        # Interactive mode
        interactive_mode()


if __name__ == "__main__":
    main()
