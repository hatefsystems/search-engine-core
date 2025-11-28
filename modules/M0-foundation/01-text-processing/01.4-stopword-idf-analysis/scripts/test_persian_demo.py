#!/usr/bin/env python3
"""
Persian Stopword Detection Demo

Tests hybrid stopword detection for Persian (Farsi) language.
Demonstrates both IDF-only and Stanza-enhanced detection.

Usage:
    python scripts/test_persian_demo.py
"""

import sys
from pathlib import Path

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from text_processing import (
    HybridStopwordDetector,
    STANZA_AVAILABLE
)


def print_header(title: str):
    """Print section header"""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70 + "\n")


def print_result(result, description=""):
    """Print detection result"""
    if description:
        print(f"📝 {description}")
    
    print(f"کلمه: '{result.term}' (زبان: {result.language})")
    print(f"  ├─ Stopword: {result.is_stopword}")
    print(f"  ├─ اطمینان (Confidence): {result.confidence:.4f}")
    print(f"  ├─ IDF: {result.idf:.4f}")
    print(f"  ├─ فراوانی در اسناد: {result.document_frequency}")
    
    if result.grammar_verified:
        print(f"  ├─ نوع کلمه (POS): {result.pos_tag} ✅")
        print(f"  ├─ تأیید گرامری: بله")
    else:
        print(f"  ├─ نوع کلمه (POS): ---")
        print(f"  ├─ تأیید گرامری: خیر (از IDF استفاده شد)")
    
    print(f"  └─ روش تشخیص: {result.detection_method}")
    print()


def test_idf_only():
    """Test IDF-only detection"""
    print_header("لایه 1: تشخیص با IDF (بدون گرامر)")
    
    print("ایجاد detector بدون Stanza...")
    detector = HybridStopwordDetector(
        redis_url="redis://localhost:6379",
        enable_stanza=False
    )
    
    print("تست stopwordهای فارسی:\n")
    
    # Common Persian stopwords
    persian_stopwords = [
        ("و", "حرف ربط 'و'"),
        ("در", "حرف اضافه 'در'"),
        ("از", "حرف اضافه 'از'"),
        ("به", "حرف اضافه 'به'"),
        ("با", "حرف اضافه 'با'"),
        ("این", "اشاره 'این'"),
        ("که", "حرف ربط 'که'"),
        ("را", "نشانه مفعول"),
    ]
    
    for term, desc in persian_stopwords:
        print(f"🔍 {desc}")
        result = detector.is_stopword(term, "fa")
        print_result(result)


def test_hybrid():
    """Test hybrid detection with Stanza"""
    if not STANZA_AVAILABLE:
        print("⚠️  Stanza موجود نیست. این بخش نیاز به نصب Stanza دارد.")
        print("   نصب: pip install stanza")
        print("   دانلود مدل: python scripts/download_stanza_models.py fa")
        return
    
    print_header("لایه 1 + لایه 2: تشخیص ترکیبی (IDF + گرامر)")
    
    print("ایجاد detector با پشتیبانی Stanza...")
    detector = HybridStopwordDetector(
        redis_url="redis://localhost:6379",
        enable_stanza=True,
        confidence_boost=1.2,
        confidence_penalty=0.7
    )
    
    # Check if Persian model is available
    if not detector.supports_grammar_verification("fa"):
        print("\n⚠️  مدل فارسی Stanza دانلود نشده است!")
        print("   دانلود کنید با:")
        print("   python scripts/download_stanza_models.py fa")
        print("\n   در حال استفاده از IDF-only برای فارسی...")
        print()
    else:
        print("✅ مدل فارسی Stanza یافت شد!\n")
    
    print("⏳ تست stopwordهای فارسی با تأیید گرامری:\n")
    
    # Test cases with descriptions
    test_cases = [
        ("و", "حرف ربط 'و' (باید تأیید شود)"),
        ("در", "حرف اضافه 'در' (باید تأیید شود)"),
        ("از", "حرف اضافه 'از' (باید تأیید شود)"),
        ("به", "حرف اضافه 'به' (باید تأیید شود)"),
        ("با", "حرف اضافه 'با' (باید تأیید شود)"),
        ("این", "ضمیر اشاره 'این' (باید تأیید شود)"),
        ("که", "حرف ربط 'که' (باید تأیید شود)"),
        ("را", "نشانه مفعول 'را' (باید تأیید شود)"),
        ("کتاب", "اسم 'کتاب' (نباید stopword باشد)"),
        ("خوب", "صفت 'خوب' (نباید stopword باشد)"),
    ]
    
    for term, description in test_cases:
        result = detector.is_stopword(term, "fa")
        print_result(result, description)


def test_comparison():
    """Compare IDF-only vs Hybrid"""
    if not STANZA_AVAILABLE:
        print("⚠️  برای مقایسه، Stanza باید نصب باشد.")
        return
    
    print_header("مقایسه: IDF-only در مقابل Hybrid")
    
    # Create both detectors
    idf_detector = HybridStopwordDetector(enable_stanza=False)
    hybrid_detector = HybridStopwordDetector(enable_stanza=True)
    
    # Check if model available
    if not hybrid_detector.supports_grammar_verification("fa"):
        print("⚠️  مدل فارسی موجود نیست. مقایسه انجام نمی‌شود.")
        return
    
    # Test terms
    test_terms = [
        ("و", "حرف ربط (stopword واقعی)"),
        ("در", "حرف اضافه (stopword واقعی)"),
        ("کتاب", "اسم (نه stopword)"),
    ]
    
    for term, description in test_terms:
        print(f"\n📊 {description}: '{term}'")
        print("-" * 50)
        
        # IDF-only result
        idf_result = idf_detector.is_stopword(term, "fa")
        print(f"IDF-only:  confidence={idf_result.confidence:.4f}, stopword={idf_result.is_stopword}")
        
        # Hybrid result
        hybrid_result = hybrid_detector.is_stopword(term, "fa")
        print(f"Hybrid:    confidence={hybrid_result.confidence:.4f}, stopword={hybrid_result.is_stopword}")
        print(f"           POS={hybrid_result.pos_tag}, grammar_verified={hybrid_result.grammar_verified}")
        
        # Analysis
        diff = hybrid_result.confidence - idf_result.confidence
        if diff > 0:
            print(f"           ✅ اطمینان افزایش یافت (+{diff:.2f}) - گرامر تأیید کرد")
        elif diff < 0:
            print(f"           ⚠️  اطمینان کاهش یافت ({diff:.2f}) - گرامر رد کرد")
        else:
            print(f"           ➡️  بدون تغییر (گرامر موجود نبود)")


def test_persian_corpus():
    """Test with actual Persian corpus"""
    print_header("تست با جملات فارسی واقعی")
    
    detector = HybridStopwordDetector(
        redis_url="redis://localhost:6379",
        enable_stanza=STANZA_AVAILABLE
    )
    
    # Sample Persian sentences
    persian_sentences = [
        "این یک متن فارسی است",
        "من به مدرسه می‌روم",
        "او در خانه است",
        "کتاب را بر روی میز گذاشت",
    ]
    
    print("جملات نمونه فارسی:\n")
    
    for sentence in persian_sentences:
        print(f"📝 جمله: {sentence}")
        
        # Tokenize (simple split for demo)
        words = sentence.split()
        
        stopwords_found = []
        content_words = []
        
        for word in words:
            result = detector.is_stopword(word, "fa")
            if result.is_stopword:
                stopwords_found.append(word)
            else:
                content_words.append(word)
        
        print(f"   Stopwords: {', '.join(stopwords_found) if stopwords_found else '---'}")
        print(f"   کلمات محتوایی: {', '.join(content_words) if content_words else '---'}")
        print()


def main():
    """Main function"""
    print("\n" + "#" * 70)
    print("#  تست تشخیص Stopword فارسی")
    print("#  Persian Stopword Detection Demo")
    print("#" * 70)
    
    # Check Stanza availability
    print_header("بررسی وضعیت Stanza")
    
    if STANZA_AVAILABLE:
        print("✅ Stanza نصب شده است")
        print("   تشخیص ترکیبی (IDF + گرامر) امکان‌پذیر است")
    else:
        print("⚠️  Stanza نصب نشده است")
        print("   فقط از IDF استفاده می‌شود")
        print("\n   نصب Stanza:")
        print("   pip install stanza")
        print("\n   دانلود مدل فارسی:")
        print("   python scripts/download_stanza_models.py fa")
    
    # Test 1: IDF-only (always works)
    test_idf_only()
    
    # Test 2: Hybrid with Stanza
    if STANZA_AVAILABLE:
        test_hybrid()
        test_comparison()
    
    # Test 3: Corpus test
    test_persian_corpus()
    
    print("\n" + "=" * 70)
    print("  تست کامل شد! ✅")
    print("  Demo completed! ✅")
    print("=" * 70 + "\n")
    
    if not STANZA_AVAILABLE:
        print("💡 نکته: برای استفاده از لایه گرامری (دقت بالاتر):")
        print("   1. نصب Stanza:")
        print("      pip install stanza")
        print("\n   2. دانلود مدل فارسی:")
        print("      python scripts/download_stanza_models.py fa")
        print("\n   3. اجرای مجدد این اسکریپت")


if __name__ == "__main__":
    main()

