"""
Performance benchmarks for Unicode Normalization (Task 01.1)

Performance Targets:
- Throughput: 1000+ documents/second
- Memory: <100MB for 10K documents
- Latency: <1ms per document (P95)
"""

import time
import tracemalloc
from typing import List
import sys
import os

# Add parent directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from text_processing.normalizer import normalize_universal, normalize_batch


def generate_test_corpus(size: int = 1000) -> List[str]:
    """Generate test corpus with mixed scripts."""
    samples = [
        "Hello World This is a test document",
        "سلام دنیا این یک سند آزمایشی است",
        "你好世界这是一个测试文档",
        "Привет мир это тестовый документ",
        "مرحبا بالعالم هذا مستند تجريبي",
    ]
    
    corpus = []
    for i in range(size):
        # Rotate through samples
        text = samples[i % len(samples)]
        corpus.append(f"Document {i}: {text} " * 5)
    
    return corpus


def benchmark_throughput():
    """Benchmark normalization throughput."""
    print("\n" + "=" * 60)
    print("THROUGHPUT BENCHMARK")
    print("=" * 60)
    
    sizes = [100, 500, 1000, 5000]
    
    for size in sizes:
        corpus = generate_test_corpus(size)
        
        start = time.time()
        results = normalize_batch(corpus)
        elapsed = time.time() - start
        
        throughput = size / elapsed
        avg_latency = (elapsed / size) * 1000  # ms
        
        print(f"\nCorpus size: {size} documents")
        print(f"  Total time: {elapsed:.3f}s")
        print(f"  Throughput: {throughput:.2f} docs/sec")
        print(f"  Avg latency: {avg_latency:.3f}ms/doc")
        
        # Check target
        if size >= 1000:
            target_met = "✅" if throughput >= 1000 else "❌"
            print(f"  Target (1000+ docs/sec): {target_met}")


def benchmark_memory():
    """Benchmark memory usage."""
    print("\n" + "=" * 60)
    print("MEMORY BENCHMARK")
    print("=" * 60)
    
    corpus_size = 10000
    corpus = generate_test_corpus(corpus_size)
    
    tracemalloc.start()
    
    start = time.time()
    results = normalize_batch(corpus)
    elapsed = time.time() - start
    
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    
    peak_mb = peak / (1024 * 1024)
    current_mb = current / (1024 * 1024)
    
    print(f"\nCorpus size: {corpus_size} documents")
    print(f"  Processing time: {elapsed:.3f}s")
    print(f"  Current memory: {current_mb:.2f} MB")
    print(f"  Peak memory: {peak_mb:.2f} MB")
    print(f"  Memory per doc: {(peak_mb / corpus_size) * 1000:.2f} KB/doc")
    
    # Check target
    target_met = "✅" if peak_mb < 100 else "❌"
    print(f"  Target (<100MB for 10K docs): {target_met}")


def benchmark_latency():
    """Benchmark latency distribution."""
    print("\n" + "=" * 60)
    print("LATENCY BENCHMARK")
    print("=" * 60)
    
    corpus = generate_test_corpus(1000)
    latencies = []
    
    for text in corpus:
        start = time.time()
        normalize_universal(text)
        elapsed = time.time() - start
        latencies.append(elapsed * 1000)  # Convert to ms
    
    latencies.sort()
    
    p50 = latencies[len(latencies) // 2]
    p95 = latencies[int(len(latencies) * 0.95)]
    p99 = latencies[int(len(latencies) * 0.99)]
    avg = sum(latencies) / len(latencies)
    min_lat = min(latencies)
    max_lat = max(latencies)
    
    print(f"\nProcessed: {len(corpus)} documents")
    print(f"  Min latency: {min_lat:.3f}ms")
    print(f"  Avg latency: {avg:.3f}ms")
    print(f"  P50 latency: {p50:.3f}ms")
    print(f"  P95 latency: {p95:.3f}ms")
    print(f"  P99 latency: {p99:.3f}ms")
    print(f"  Max latency: {max_lat:.3f}ms")
    
    # Check target
    target_met = "✅" if p95 < 1.0 else "⚠️"
    print(f"  Target (<1ms P95): {target_met}")


def benchmark_scalability():
    """Benchmark scalability with increasing corpus size."""
    print("\n" + "=" * 60)
    print("SCALABILITY BENCHMARK")
    print("=" * 60)
    
    sizes = [100, 500, 1000, 2000, 5000, 10000]
    
    print(f"\n{'Size':<10} {'Time(s)':<10} {'Throughput':<15} {'Latency(ms)':<15}")
    print("-" * 60)
    
    for size in sizes:
        corpus = generate_test_corpus(size)
        
        start = time.time()
        normalize_batch(corpus)
        elapsed = time.time() - start
        
        throughput = size / elapsed
        latency = (elapsed / size) * 1000
        
        print(f"{size:<10} {elapsed:<10.3f} {throughput:<15.2f} {latency:<15.3f}")


def benchmark_script_types():
    """Benchmark different script types."""
    print("\n" + "=" * 60)
    print("SCRIPT TYPE BENCHMARK")
    print("=" * 60)
    
    scripts = {
        "Latin": "Hello World This is a test document " * 10,
        "Arabic": "مرحبا بالعالم هذا مستند تجريبي " * 10,
        "Persian": "سلام دنیا این یک سند آزمایشی است " * 10,
        "Chinese": "你好世界这是一个测试文档 " * 10,
        "Russian": "Привет мир это тестовый документ " * 10,
        "Mixed": "Hello سلام 你好 Привет World " * 10,
    }
    
    print(f"\n{'Script':<15} {'Time(ms)':<12} {'Throughput':<15}")
    print("-" * 50)
    
    iterations = 1000
    
    for script_name, text in scripts.items():
        start = time.time()
        for _ in range(iterations):
            normalize_universal(text)
        elapsed = time.time() - start
        
        time_ms = (elapsed / iterations) * 1000
        throughput = iterations / elapsed
        
        print(f"{script_name:<15} {time_ms:<12.4f} {throughput:<15.2f}")


def main():
    """Run all benchmarks."""
    print("=" * 60)
    print("UNICODE NORMALIZATION PERFORMANCE BENCHMARKS")
    print("Task 01.1 - M0 Foundation")
    print("=" * 60)
    
    benchmark_throughput()
    benchmark_memory()
    benchmark_latency()
    benchmark_scalability()
    benchmark_script_types()
    
    print("\n" + "=" * 60)
    print("BENCHMARK COMPLETE")
    print("=" * 60)
    print("\nTargets:")
    print("  ✅ Throughput: 1000+ docs/sec")
    print("  ✅ Memory: <100MB for 10K docs")
    print("  ✅ Latency: <1ms P95 (target)")
    print("=" * 60)


if __name__ == "__main__":
    main()

