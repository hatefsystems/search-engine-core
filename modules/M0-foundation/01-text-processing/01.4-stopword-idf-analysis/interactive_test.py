#!/usr/bin/env python3
"""
Interactive Testing Tool for Stopword IDF Analysis - Task 01.4

Provides an interactive CLI for testing stopword detection with real-time IDF analysis.
"""

import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from text_processing import IDFAnalyzer, StopwordDetector, is_stopword
from shared.logger import setup_logger

logger = setup_logger(__name__, level="INFO")


def print_banner():
    """Print welcome banner"""
    print("=" * 70)
    print(" " * 15 + "STOPWORD IDF ANALYSIS - INTERACTIVE TEST")
    print(" " * 20 + "Task 01.4 - Universal Stopword Detection")
    print("=" * 70)
    print()


def test_idf_analysis():
    """Interactive IDF analysis test"""
    print("\n📊 IDF ANALYSIS TEST")
    print("-" * 70)
    print("Enter documents line by line (empty line to finish):")
    print()
    
    corpus = []
    while True:
        doc = input(f"Document {len(corpus) + 1}: ").strip()
        if not doc:
            break
        corpus.append(doc)
    
    if not corpus:
        print("❌ No documents provided")
        return
    
    print(f"\n✅ Analyzing {len(corpus)} documents...")
    
    # Analyze corpus
    analyzer = IDFAnalyzer(idf_threshold=2.0)
    idf_scores = analyzer.analyze(corpus)
    
    # Display statistics
    stats = analyzer.export_statistics()
    print(f"\n📈 CORPUS STATISTICS:")
    print(f"   Total Documents: {stats['total_documents']}")
    print(f"   Vocabulary Size: {stats['vocabulary_size']}")
    print(f"   Analyzed Terms: {stats['analyzed_terms']}")
    print(f"   Stopword Candidates: {stats['stopword_candidates']}")
    print(f"   Stopword Ratio: {stats['stopword_ratio']:.2%}")
    
    # Get stopword candidates
    candidates = analyzer.get_stopword_candidates(min_confidence=0.7, limit=20)
    
    print(f"\n🎯 TOP STOPWORD CANDIDATES (confidence ≥ 0.7):")
    print(f"{'Rank':<6} {'Term':<20} {'IDF':<8} {'DF':<8} {'Confidence':<12}")
    print("-" * 70)
    
    for idx, (term, score) in enumerate(candidates, 1):
        print(f"{idx:<6} {term:<20} {score.idf:<8.4f} {score.document_frequency:<8} {score.confidence:<12.4f}")
    
    if not candidates:
        print("   (No stopword candidates found)")
    
    # Show content words (high IDF)
    content_words = [
        (term, score)
        for term, score in idf_scores.items()
        if not score.is_stopword_candidate
    ]
    content_words.sort(key=lambda x: x[1].idf, reverse=True)
    content_words = content_words[:10]
    
    print(f"\n📝 TOP CONTENT WORDS (high IDF):")
    print(f"{'Rank':<6} {'Term':<20} {'IDF':<8} {'DF':<8}")
    print("-" * 70)
    
    for idx, (term, score) in enumerate(content_words, 1):
        print(f"{idx:<6} {term:<20} {score.idf:<8.4f} {score.document_frequency:<8}")


def test_stopword_detection():
    """Interactive stopword detection test"""
    print("\n🔍 STOPWORD DETECTION TEST")
    print("-" * 70)
    
    # Get Redis URL
    redis_url = input("Redis URL (default: redis://localhost:6379): ").strip()
    if not redis_url:
        redis_url = "redis://localhost:6379"
    
    # Initialize detector
    try:
        detector = StopwordDetector(redis_url=redis_url, use_bootstrap=True)
        print("✅ Stopword detector initialized")
        
        if detector.redis_client:
            print("✅ Redis connection established")
        else:
            print("⚠️  Redis not available, using bootstrap only")
    except Exception as e:
        print(f"❌ Error initializing detector: {e}")
        return
    
    print("\nEnter terms to check (type 'quit' to exit):")
    print()
    
    while True:
        term = input("Term: ").strip()
        
        if term.lower() == 'quit':
            break
        
        if not term:
            continue
        
        # Get language
        language = input("Language code (e.g., en, fa, es) [default: en]: ").strip()
        if not language:
            language = "en"
        
        # Check stopword
        result = detector.is_stopword(term, language)
        
        # Display result
        print(f"\n   Term: {result.term}")
        print(f"   Language: {result.language}")
        print(f"   Is Stopword: {'✅ YES' if result.is_stopword else '❌ NO'}")
        print(f"   Confidence: {result.confidence:.4f}")
        if result.idf > 0:
            print(f"   IDF: {result.idf:.4f}")
            print(f"   Document Frequency: {result.document_frequency}")
        print()


def test_multilingual():
    """Test multilingual stopword detection"""
    print("\n🌍 MULTILINGUAL STOPWORD TEST")
    print("-" * 70)
    
    samples = [
        ("en", "the", "English"),
        ("en", "quantum", "English (not stopword)"),
        ("fa", "و", "Persian (and)"),
        ("fa", "کوانتوم", "Persian (quantum)"),
        ("es", "el", "Spanish (the)"),
        ("es", "cuántico", "Spanish (quantum)"),
        ("de", "der", "German (the)"),
        ("fr", "le", "French (the)"),
        ("ru", "это", "Russian (this)"),
        ("ar", "في", "Arabic (in)"),
    ]
    
    detector = StopwordDetector(use_bootstrap=True)
    
    print(f"\n{'Language':<10} {'Term':<15} {'Stopword':<10} {'Confidence':<12} {'Description':<30}")
    print("-" * 80)
    
    for lang, term, description in samples:
        result = detector.is_stopword(term, lang)
        is_stop = "✅ YES" if result.is_stopword else "❌ NO"
        print(f"{lang:<10} {term:<15} {is_stop:<10} {result.confidence:<12.4f} {description:<30}")


def test_performance_benchmark():
    """Simple performance benchmark"""
    print("\n⚡ PERFORMANCE BENCHMARK")
    print("-" * 70)
    
    import time
    
    # Generate test corpus
    print("Generating test corpus (10,000 documents)...")
    test_corpus = [
        f"document {i} the quick brown fox and lazy dog jumps over"
        for i in range(10000)
    ]
    
    # Benchmark IDF analysis
    print("\n1️⃣ Benchmarking IDF Analysis...")
    analyzer = IDFAnalyzer(idf_threshold=2.0)
    
    start_time = time.time()
    idf_scores = analyzer.analyze(test_corpus)
    duration = time.time() - start_time
    
    docs_per_sec = len(test_corpus) / duration
    
    print(f"   Documents Processed: {len(test_corpus):,}")
    print(f"   Duration: {duration:.2f} seconds")
    print(f"   Throughput: {docs_per_sec:,.0f} docs/sec")
    print(f"   Unique Terms: {len(idf_scores):,}")
    
    # Benchmark Redis lookup (if available)
    print("\n2️⃣ Benchmarking Stopword Lookup...")
    
    detector = StopwordDetector(use_bootstrap=True)
    
    test_terms = ["the", "and", "quantum", "cryptocurrency", "fox", "dog"] * 1000
    
    start_time = time.time()
    for term in test_terms:
        detector.is_stopword(term, "en")
    duration = time.time() - start_time
    
    lookups_per_sec = len(test_terms) / duration
    avg_latency_ms = (duration / len(test_terms)) * 1000
    
    print(f"   Lookups Performed: {len(test_terms):,}")
    print(f"   Duration: {duration:.2f} seconds")
    print(f"   Throughput: {lookups_per_sec:,.0f} lookups/sec")
    print(f"   Average Latency: {avg_latency_ms:.4f} ms")
    
    if avg_latency_ms < 1.0:
        print("   ✅ Target met: <1ms latency")
    else:
        print(f"   ⚠️  Target not met: {avg_latency_ms:.2f}ms (target: <1ms)")


def main_menu():
    """Display main menu and handle user selection"""
    while True:
        print("\n" + "=" * 70)
        print("MAIN MENU")
        print("=" * 70)
        print("1. IDF Analysis Test (analyze custom corpus)")
        print("2. Stopword Detection Test (check individual terms)")
        print("3. Multilingual Test (test multiple languages)")
        print("4. Performance Benchmark")
        print("5. Exit")
        print()
        
        choice = input("Select option (1-5): ").strip()
        
        if choice == "1":
            test_idf_analysis()
        elif choice == "2":
            test_stopword_detection()
        elif choice == "3":
            test_multilingual()
        elif choice == "4":
            test_performance_benchmark()
        elif choice == "5":
            print("\n👋 Goodbye!")
            break
        else:
            print("❌ Invalid option. Please select 1-5.")


def main():
    """Main entry point"""
    print_banner()
    
    print("Welcome to the Stopword IDF Analysis Interactive Test Tool!")
    print()
    print("This tool allows you to:")
    print("  • Analyze custom document corpora for IDF scores")
    print("  • Test stopword detection with Redis/bootstrap")
    print("  • Test multilingual stopword support")
    print("  • Benchmark performance")
    print()
    
    try:
        main_menu()
    except KeyboardInterrupt:
        print("\n\n👋 Interrupted. Goodbye!")
    except Exception as e:
        print(f"\n❌ Error: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    main()

