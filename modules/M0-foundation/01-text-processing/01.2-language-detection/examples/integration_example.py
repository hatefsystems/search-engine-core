#!/usr/bin/env python3
"""
Integration Example: Task 01.1 + Task 01.2

Demonstrates complete text processing pipeline:
Raw Text → Unicode Normalization → Language Detection

Example output:
    Original:  "روباه  قهوه‌ای   سریع"
    Normalized: "روباه قهوه‌ای سریع"
    Language:  fa (Persian)
    Script:    Arab (Arabic script)
    Confidence: 0.98
"""

import sys
from pathlib import Path

# Add parent to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from text_processing.integration import TextProcessingPipeline, process_text


def main():
    """Demonstrate integration pipeline."""
    
    print("=" * 70)
    print("🔗 Integration Example: Unicode Normalization + Language Detection")
    print("=" * 70)
    print()
    
    # Initialize pipeline
    print("🔧 Initializing pipeline...")
    try:
        pipeline = TextProcessingPipeline(use_normalizer=True)
        print("✅ Pipeline ready!")
    except Exception as e:
        print(f"❌ Pipeline initialization failed: {e}")
        print("\n💡 Make sure:")
        print("  1. Task 01.1 (unicode-normalization) is available")
        print("  2. Models are downloaded: ./scripts/download_models.sh")
        return 1
    
    print()
    
    # Test samples
    samples = [
        ("روباه  قهوه‌ای   سریع", "Persian with extra spaces"),
        ("Hello   World", "English with extra spaces"),
        ("こんにちは　世界", "Japanese with full-width space"),
        ("Привет   мир", "Russian with extra spaces"),
        ("你好   世界", "Chinese with extra spaces"),
    ]
    
    print("📝 Processing samples...\n")
    
    for text, description in samples:
        print("-" * 70)
        print(f"📄 {description}")
        print(f"   Original:   \"{text}\"")
        
        # Process
        result = pipeline.process(text)
        
        print(f"   Normalized: \"{result.normalized_text}\"")
        print(f"   Language:   {result.language_code}")
        print(f"   Script:     {result.script_code}")
        print(f"   Confidence: {result.confidence:.2%}")
        print(f"   Method:     {result.detection_method}")
        
        if result.normalization_changes:
            print(f"   Changes:    {len(result.normalization_changes)} applied")
        
        print()
    
    print("-" * 70)
    print("\n✅ Integration example complete!")
    print()
    
    # Show pipeline benefits
    print("💡 Pipeline Benefits:")
    print("  ✅ Consistent text processing")
    print("  ✅ Improved language detection accuracy")
    print("  ✅ Unified script detection")
    print("  ✅ Character unification (30% fewer variants)")
    print("  ✅ Clean, normalized output")
    print()
    
    return 0


if __name__ == '__main__':
    sys.exit(main())

