#!/usr/bin/env python3
"""
Interactive Language Detection Testing Tool - Task 01.2

Test language detection interactively with your own texts.
Provides detailed detection results including confidence, script, and alternatives.

Usage:
    python interactive_test.py
"""

import sys
from pathlib import Path
from typing import List

# Add current directory to path
sys.path.insert(0, str(Path(__file__).parent))

try:
    from text_processing import UniversalLanguageDetector, LanguageInfo
    from shared.logger import setup_logger
except ImportError as e:
    print(f"❌ Import error: {e}")
    print("\n💡 Make sure to install dependencies:")
    print("   pip install -r requirements.txt")
    sys.exit(1)

logger = setup_logger(__name__)


def print_header():
    """Print welcome header."""
    print("\n" + "=" * 70)
    print("🌍 Interactive Language Detection Tool")
    print("=" * 70)
    print("Task 01.2: Universal Language Detection (176+ languages)")
    print("\nSupports: English, Persian, Arabic, Chinese, Japanese, Korean,")
    print("          Russian, Spanish, French, German, and 166 more...")
    print("\nScalable to 250+ languages with custom training!")
    print("=" * 70 + "\n")


def print_detection_result(result: LanguageInfo, text: str):
    """
    Print detection result in a beautiful format.
    
    Args:
        result: Detection result
        text: Original text
    """
    print("\n" + "-" * 70)
    print("📝 Text:")
    print(f"   {text}")
    print("\n🔍 Detection Results:")
    print(f"   Primary Language:  {result.language_code}")
    print(f"   Script (ISO 15924): {result.script_code}")
    print(f"   Confidence:        {result.confidence:.2%}")
    print(f"   Mixed Content:     {'Yes' if result.is_mixed_content else 'No'}")
    print(f"   Detection Method:  {result.detection_method}")
    
    if len(result.detected_languages) > 1:
        print("\n🌐 Alternative Languages:")
        for i, (lang, conf) in enumerate(result.detected_languages[1:4], 1):
            print(f"   {i}. {lang}: {conf:.2%}")
    
    # Confidence indicator
    if result.confidence >= 0.9:
        indicator = "✅ Very Confident"
    elif result.confidence >= 0.7:
        indicator = "👍 Confident"
    elif result.confidence >= 0.5:
        indicator = "⚠️  Moderate"
    else:
        indicator = "❓ Low Confidence"
    
    print(f"\n{indicator}")
    print("-" * 70)


def test_predefined_samples(detector: UniversalLanguageDetector):
    """Test with predefined samples in various languages."""
    print("\n" + "=" * 70)
    print("📚 Testing with predefined samples...")
    print("=" * 70)
    
    samples = {
        "English": "The quick brown fox jumps over the lazy dog",
        "Persian": "روباه قهوه‌ای سریع از روی سگ تنبل می‌پرد",
        "Arabic": "الثعلب البني السريع يقفز فوق الكلب الكسول",
        "Chinese": "敏捷的棕色狐狸跳过懒狗",
        "Japanese": "素早い茶色のキツネが怠け者の犬を飛び越える",
        "Korean": "빠른 갈색 여우가 게으른 개를 뛰어넘습니다",
        "Russian": "Быстрая коричневая лиса прыгает через ленивую собаку",
        "Spanish": "El rápido zorro marrón salta sobre el perro perezoso",
        "French": "Le rapide renard brun saute par-dessus le chien paresseux",
        "German": "Der schnelle braune Fuchs springt über den faulen Hund",
    }
    
    for language, text in samples.items():
        print(f"\n{language}:")
        print(f"  {text}")
        result = detector.detect(text)
        print(f"  → Detected: {result.language_code} (confidence: {result.confidence:.2%})")
    
    print("\n" + "=" * 70)


def interactive_mode(detector: UniversalLanguageDetector):
    """Run interactive detection mode."""
    print("\n💬 Interactive Mode")
    print("Type your text to detect its language (or 'quit' to exit)\n")
    
    while True:
        try:
            # Get input
            text = input("🔤 Enter text: ").strip()
            
            # Check for exit
            if text.lower() in ['quit', 'exit', 'q']:
                print("\n👋 Goodbye!")
                break
            
            # Check for empty
            if not text:
                print("⚠️  Please enter some text\n")
                continue
            
            # Check for special commands
            if text.lower() == 'help':
                print("\n📖 Commands:")
                print("  - Type any text to detect language")
                print("  - 'samples' - Test predefined samples")
                print("  - 'stats' - Show detector statistics")
                print("  - 'quit' - Exit\n")
                continue
            
            if text.lower() == 'samples':
                test_predefined_samples(detector)
                continue
            
            if text.lower() == 'stats':
                print("\n📊 Detector Statistics:")
                try:
                    num_langs = detector.fasttext_detector.get_num_languages()
                    langs = detector.fasttext_detector.get_supported_languages()
                    print(f"  Languages supported: {num_langs}")
                    print(f"  Sample languages: {', '.join(langs[:10])}...")
                except Exception as e:
                    print(f"  Unable to get stats: {e}")
                print()
                continue
            
            # Detect language
            try:
                result = detector.detect(text)
                print_detection_result(result, text)
            except Exception as e:
                print(f"\n❌ Detection error: {e}\n")
        
        except KeyboardInterrupt:
            print("\n\n👋 Interrupted. Goodbye!")
            break
        except EOFError:
            print("\n\n👋 EOF. Goodbye!")
            break


def main():
    """Main interactive testing function."""
    print_header()
    
    # Initialize detector
    print("🔧 Initializing language detector...")
    
    try:
        detector = UniversalLanguageDetector()
        print("✅ Detector ready!\n")
    except FileNotFoundError as e:
        print(f"\n❌ Model not found: {e}")
        print("\n💡 Download models first:")
        print("   ./scripts/download_models.sh")
        print("\nOr specify custom model path in code.")
        return 1
    except Exception as e:
        print(f"\n❌ Failed to initialize detector: {e}")
        return 1
    
    # Show usage
    print("💡 Quick tips:")
    print("  - Type 'samples' to test predefined samples")
    print("  - Type 'stats' to see detector info")
    print("  - Type 'help' for more commands")
    print("  - Type 'quit' to exit")
    
    # Run interactive mode
    interactive_mode(detector)
    
    return 0


if __name__ == '__main__':
    sys.exit(main())

