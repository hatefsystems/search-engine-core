#!/usr/bin/env python3
"""
Demo Script for Hybrid Stopword Detection

Demonstrates the two-layer architecture:
- Layer 1: IDF-based detection (universal)
- Layer 2: Stanza POS tagging (grammar verification)

Usage:
    python scripts/test_hybrid_demo.py
"""

import sys
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from text_processing import (
    HybridStopwordDetector,
    StanzaPOSTagger,
    STANZA_AVAILABLE
)


def print_header(title: str):
    """Print section header"""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70 + "\n")


def print_result(result):
    """Print stopword detection result"""
    print(f"Term: '{result.term}' ({result.language})")
    print(f"  ├─ Is stopword: {result.is_stopword}")
    print(f"  ├─ Confidence: {result.confidence:.4f}")
    print(f"  ├─ IDF: {result.idf:.4f}")
    print(f"  ├─ Document frequency: {result.document_frequency}")
    
    if result.grammar_verified:
        print(f"  ├─ POS tag: {result.pos_tag} ✅")
        print(f"  ├─ Grammar verified: Yes")
    else:
        print(f"  ├─ POS tag: None")
        print(f"  ├─ Grammar verified: No (fallback to IDF)")
    
    print(f"  └─ Detection method: {result.detection_method}")
    print()


def demo_stanza_availability():
    """Check if Stanza is available"""
    print_header("Stanza Availability Check")
    
    if STANZA_AVAILABLE:
        print("✅ Stanza is AVAILABLE")
        print("   Grammar verification (Layer 2) can be used\n")
        
        tagger = StanzaPOSTagger()
        languages = tagger.get_supported_languages()
        print(f"   Supported languages: {len(languages)}")
        print(f"   Sample languages: {', '.join(languages[:10])}...")
        
        return True
    else:
        print("⚠️  Stanza is NOT AVAILABLE")
        print("   Only Layer 1 (IDF) will be used")
        print("\n   Install with: pip install stanza")
        return False


def demo_idf_only_mode():
    """Demo IDF-only detection (Layer 1)"""
    print_header("Layer 1: IDF-Only Detection (Universal)")
    
    print("Initializing detector in IDF-only mode...")
    detector = HybridStopwordDetector(
        redis_url="redis://localhost:6379",
        enable_stanza=False
    )
    
    print("Testing English stopwords:\n")
    
    # Test common stopwords
    test_terms = [
        ("the", "en"),
        ("and", "en"),
        ("API", "en"),  # Might be false positive
        ("quantum", "en")
    ]
    
    for term, lang in test_terms:
        result = detector.is_stopword(term, lang)
        print_result(result)


def demo_hybrid_mode():
    """Demo hybrid detection (Layer 1 + Layer 2)"""
    if not STANZA_AVAILABLE:
        print("⚠️  Skipping hybrid mode demo (Stanza not available)")
        return
    
    print_header("Layer 1 + Layer 2: Hybrid Detection (Enhanced)")
    
    print("Initializing detector with Stanza support...")
    detector = HybridStopwordDetector(
        redis_url="redis://localhost:6379",
        enable_stanza=True,
        confidence_boost=1.2,
        confidence_penalty=0.7
    )
    
    print("\n⏳ Testing English stopwords (with grammar verification):\n")
    
    # Test terms with expected behavior
    test_cases = [
        ("the", "en", "True stopword - will be grammar-verified"),
        ("and", "en", "True stopword - coordinating conjunction"),
        ("API", "en", "False positive - should be rejected by grammar"),
        ("HTTP", "en", "False positive - should be rejected by grammar"),
        ("quantum", "en", "Content word - not a stopword"),
    ]
    
    for term, lang, description in test_cases:
        print(f"📝 {description}")
        result = detector.is_stopword(term, lang)
        print_result(result)


def demo_multilingual():
    """Demo multilingual stopword detection"""
    if not STANZA_AVAILABLE:
        print("⚠️  Skipping multilingual demo (Stanza not available)")
        return
    
    print_header("Multilingual Stopword Detection")
    
    detector = HybridStopwordDetector(
        redis_url="redis://localhost:6379",
        enable_stanza=True
    )
    
    # Test cases for multiple languages
    test_cases = [
        ("the", "en", "English"),
        ("و", "fa", "Persian (and)"),
        ("در", "fa", "Persian (in)"),
        ("في", "ar", "Arabic (in)"),
        ("der", "de", "German (the)"),
        ("el", "es", "Spanish (the)"),
    ]
    
    for term, lang, description in test_cases:
        print(f"🌍 {description}")
        
        # Check if grammar support available
        has_grammar = detector.supports_grammar_verification(lang)
        print(f"   Grammar support: {'✅ Available' if has_grammar else '❌ Not available'}")
        
        result = detector.is_stopword(term, lang)
        print_result(result)


def demo_comparison():
    """Demo comparison between IDF-only and Hybrid"""
    if not STANZA_AVAILABLE:
        print("⚠️  Skipping comparison demo (Stanza not available)")
        return
    
    print_header("Comparison: IDF-Only vs Hybrid")
    
    # Create both detectors
    idf_detector = HybridStopwordDetector(enable_stanza=False)
    hybrid_detector = HybridStopwordDetector(enable_stanza=True)
    
    # Test terms
    test_terms = [
        ("API", "en", "False positive case"),
        ("the", "en", "True stopword case"),
        ("quantum", "en", "Content word case"),
    ]
    
    for term, lang, description in test_terms:
        print(f"\n📊 {description}: '{term}'")
        print("-" * 50)
        
        # IDF-only result
        idf_result = idf_detector.is_stopword(term, lang)
        print(f"IDF-only:  confidence={idf_result.confidence:.4f}, stopword={idf_result.is_stopword}")
        
        # Hybrid result
        hybrid_result = hybrid_detector.is_stopword(term, lang)
        print(f"Hybrid:    confidence={hybrid_result.confidence:.4f}, stopword={hybrid_result.is_stopword}")
        print(f"           POS={hybrid_result.pos_tag}, grammar_verified={hybrid_result.grammar_verified}")
        
        # Analysis
        diff = hybrid_result.confidence - idf_result.confidence
        if diff > 0:
            print(f"           ✅ Confidence BOOSTED by {diff:.2f} (grammar confirmed)")
        elif diff < 0:
            print(f"           ⚠️  Confidence REDUCED by {abs(diff):.2f} (grammar rejected)")
        else:
            print(f"           ➡️  No change (grammar not available)")


def demo_statistics():
    """Demo statistics API"""
    print_header("Statistics & Introspection")
    
    detector = HybridStopwordDetector(
        redis_url="redis://localhost:6379",
        enable_stanza=STANZA_AVAILABLE
    )
    
    # Get statistics for English
    stats = detector.get_statistics("en")
    
    print("Statistics for English (en):")
    for key, value in stats.items():
        print(f"  {key}: {value}")


def main():
    """Run all demos"""
    print("\n" + "#" * 70)
    print("#  Hybrid Stopword Detection Demo")
    print("#  Layer 1 (IDF) + Layer 2 (Stanza POS Tagging)")
    print("#" * 70)
    
    # Check Stanza availability
    has_stanza = demo_stanza_availability()
    
    # Demo 1: IDF-only mode (always works)
    demo_idf_only_mode()
    
    if has_stanza:
        # Demo 2: Hybrid mode (IDF + Stanza)
        demo_hybrid_mode()
        
        # Demo 3: Multilingual detection
        demo_multilingual()
        
        # Demo 4: Comparison
        demo_comparison()
    
    # Demo 5: Statistics
    demo_statistics()
    
    print("\n" + "=" * 70)
    print("  Demo completed! ✅")
    print("=" * 70 + "\n")
    
    if not has_stanza:
        print("💡 Tip: Install Stanza to enable Layer 2 (grammar verification):")
        print("   pip install stanza")
        print("   python -c \"import stanza; stanza.download('en')\"")


if __name__ == "__main__":
    main()

