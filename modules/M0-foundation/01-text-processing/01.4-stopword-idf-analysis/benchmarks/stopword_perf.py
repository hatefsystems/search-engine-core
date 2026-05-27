"""
Performance Benchmarks for Stopword IDF Analysis - Task 01.4

Measures performance across different scenarios:
- IDF calculation throughput
- Redis lookup latency
- Batch processing performance
- Large corpus handling
"""

import time
import sys
from pathlib import Path
from typing import List, Dict
import statistics

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from text_processing import IDFAnalyzer, StopwordDetector
from shared.logger import setup_logger

logger = setup_logger(__name__, level="INFO")


class StopwordBenchmark:
    """Performance benchmark suite for stopword detection"""
    
    def __init__(self):
        self.results: Dict[str, Dict] = {}
    
    def benchmark_idf_calculation(self, corpus_sizes: List[int] = [100, 1000, 10000]):
        """Benchmark IDF calculation with different corpus sizes"""
        print("\n📊 BENCHMARK: IDF Calculation")
        print("=" * 70)
        
        for size in corpus_sizes:
            # Generate test corpus
            corpus = [
                f"document {i} the quick brown fox and lazy dog jumps"
                for i in range(size)
            ]
            
            analyzer = IDFAnalyzer(idf_threshold=2.0)
            
            # Benchmark
            start_time = time.time()
            idf_scores = analyzer.analyze(corpus)
            duration = time.time() - start_time
            
            docs_per_sec = size / duration
            
            self.results[f"idf_calculation_{size}"] = {
                "corpus_size": size,
                "duration_seconds": duration,
                "docs_per_second": docs_per_sec,
                "unique_terms": len(idf_scores)
            }
            
            print(f"\nCorpus Size: {size:,} documents")
            print(f"  Duration: {duration:.4f} seconds")
            print(f"  Throughput: {docs_per_sec:,.0f} docs/sec")
            print(f"  Unique Terms: {len(idf_scores):,}")
    
    def benchmark_stopword_lookup(self, num_lookups: int = 100000):
        """Benchmark stopword lookup performance"""
        print("\n🔍 BENCHMARK: Stopword Lookup")
        print("=" * 70)
        
        detector = StopwordDetector(use_bootstrap=True)
        
        # Test terms
        test_terms = [
            "the", "and", "or", "but", "in", "on", "at", "to",
            "quantum", "cryptocurrency", "blockchain", "neural"
        ] * (num_lookups // 12)
        
        # Warm-up
        for term in test_terms[:100]:
            detector.is_stopword(term, "en")
        
        # Benchmark
        latencies = []
        
        for term in test_terms[:num_lookups]:
            start_time = time.perf_counter()
            detector.is_stopword(term, "en")
            latency = (time.perf_counter() - start_time) * 1000  # Convert to ms
            latencies.append(latency)
        
        # Calculate statistics
        mean_latency = statistics.mean(latencies)
        median_latency = statistics.median(latencies)
        p95_latency = statistics.quantiles(latencies, n=20)[18]  # 95th percentile
        p99_latency = statistics.quantiles(latencies, n=100)[98]  # 99th percentile
        
        lookups_per_sec = num_lookups / (sum(latencies) / 1000)
        
        self.results["stopword_lookup"] = {
            "num_lookups": num_lookups,
            "mean_latency_ms": mean_latency,
            "median_latency_ms": median_latency,
            "p95_latency_ms": p95_latency,
            "p99_latency_ms": p99_latency,
            "lookups_per_second": lookups_per_sec
        }
        
        print(f"\nLookups Performed: {num_lookups:,}")
        print(f"  Mean Latency: {mean_latency:.6f} ms")
        print(f"  Median Latency: {median_latency:.6f} ms")
        print(f"  P95 Latency: {p95_latency:.6f} ms")
        print(f"  P99 Latency: {p99_latency:.6f} ms")
        print(f"  Throughput: {lookups_per_sec:,.0f} lookups/sec")
        
        # Check target
        if mean_latency < 1.0:
            print(f"  ✅ Target MET: <1ms mean latency")
        else:
            print(f"  ⚠️  Target MISSED: {mean_latency:.4f}ms (target: <1ms)")
    
    def benchmark_batch_processing(self):
        """Benchmark batch processing performance"""
        print("\n📦 BENCHMARK: Batch Processing")
        print("=" * 70)
        
        # Generate large corpus
        def corpus_generator():
            for i in range(100000):
                yield f"document {i} with common terms the and a"
        
        analyzer = IDFAnalyzer(idf_threshold=2.0)
        
        batch_sizes = [100, 1000, 10000]
        
        for batch_size in batch_sizes:
            # Reset generator
            def corpus_gen():
                for i in range(100000):
                    yield f"document {i} with common terms the and a"
            
            start_time = time.time()
            idf_scores = analyzer.analyze_batch(corpus_gen(), batch_size=batch_size)
            duration = time.time() - start_time
            
            docs_per_sec = analyzer.total_documents / duration
            
            self.results[f"batch_processing_{batch_size}"] = {
                "batch_size": batch_size,
                "total_documents": analyzer.total_documents,
                "duration_seconds": duration,
                "docs_per_second": docs_per_sec
            }
            
            print(f"\nBatch Size: {batch_size:,}")
            print(f"  Total Documents: {analyzer.total_documents:,}")
            print(f"  Duration: {duration:.2f} seconds")
            print(f"  Throughput: {docs_per_sec:,.0f} docs/sec")
    
    def benchmark_memory_usage(self):
        """Benchmark memory usage"""
        print("\n💾 BENCHMARK: Memory Usage")
        print("=" * 70)
        
        try:
            import psutil
            import os
            
            process = psutil.Process(os.getpid())
            
            # Baseline memory
            baseline_memory = process.memory_info().rss / 1024 / 1024  # MB
            
            # Create large corpus
            corpus = [f"document {i} the quick brown fox" for i in range(50000)]
            
            memory_after_corpus = process.memory_info().rss / 1024 / 1024
            
            # Analyze
            analyzer = IDFAnalyzer(idf_threshold=2.0)
            idf_scores = analyzer.analyze(corpus)
            
            memory_after_analysis = process.memory_info().rss / 1024 / 1024
            
            corpus_memory = memory_after_corpus - baseline_memory
            analysis_memory = memory_after_analysis - memory_after_corpus
            total_memory = memory_after_analysis - baseline_memory
            
            self.results["memory_usage"] = {
                "baseline_mb": baseline_memory,
                "corpus_memory_mb": corpus_memory,
                "analysis_memory_mb": analysis_memory,
                "total_memory_mb": total_memory
            }
            
            print(f"\nBaseline Memory: {baseline_memory:.2f} MB")
            print(f"Corpus Memory: {corpus_memory:.2f} MB")
            print(f"Analysis Memory: {analysis_memory:.2f} MB")
            print(f"Total Memory: {total_memory:.2f} MB")
            
        except ImportError:
            print("\n⚠️  psutil not available, skipping memory benchmark")
            print("   Install with: pip install psutil")
    
    def print_summary(self):
        """Print benchmark summary"""
        print("\n" + "=" * 70)
        print("BENCHMARK SUMMARY")
        print("=" * 70)
        
        # IDF Calculation Summary
        print("\n📊 IDF Calculation:")
        for key in sorted(self.results.keys()):
            if key.startswith("idf_calculation_"):
                result = self.results[key]
                print(f"  {result['corpus_size']:>7,} docs: {result['docs_per_second']:>10,.0f} docs/sec")
        
        # Stopword Lookup Summary
        if "stopword_lookup" in self.results:
            result = self.results["stopword_lookup"]
            print(f"\n🔍 Stopword Lookup:")
            print(f"  Mean Latency: {result['mean_latency_ms']:.6f} ms")
            print(f"  P95 Latency: {result['p95_latency_ms']:.6f} ms")
            print(f"  Throughput: {result['lookups_per_second']:,.0f} lookups/sec")
        
        # Batch Processing Summary
        print(f"\n📦 Batch Processing:")
        for key in sorted(self.results.keys()):
            if key.startswith("batch_processing_"):
                result = self.results[key]
                print(f"  Batch {result['batch_size']:>6,}: {result['docs_per_second']:>10,.0f} docs/sec")
        
        # Memory Usage Summary
        if "memory_usage" in self.results:
            result = self.results["memory_usage"]
            print(f"\n💾 Memory Usage:")
            print(f"  Total: {result['total_memory_mb']:.2f} MB")
        
        # Targets
        print("\n🎯 TARGET COMPARISON:")
        if "stopword_lookup" in self.results:
            lookup_result = self.results["stopword_lookup"]
            latency_status = "✅ MET" if lookup_result['mean_latency_ms'] < 1.0 else "❌ MISSED"
            throughput_status = "✅ MET" if lookup_result['lookups_per_second'] > 1000000 else "⚠️  CLOSE"
            
            print(f"  Lookup Latency <1ms: {latency_status}")
            print(f"  Throughput >1M/sec: {throughput_status}")


def main():
    """Run all benchmarks"""
    print("=" * 70)
    print(" " * 15 + "STOPWORD IDF ANALYSIS - PERFORMANCE BENCHMARKS")
    print(" " * 25 + "Task 01.4")
    print("=" * 70)
    
    benchmark = StopwordBenchmark()
    
    try:
        # Run benchmarks
        benchmark.benchmark_idf_calculation([100, 1000, 10000])
        benchmark.benchmark_stopword_lookup(num_lookups=100000)
        benchmark.benchmark_batch_processing()
        benchmark.benchmark_memory_usage()
        
        # Print summary
        benchmark.print_summary()
        
        print("\n✅ All benchmarks complete!")
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Benchmarks interrupted")
    except Exception as e:
        print(f"\n❌ Error during benchmarks: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    main()

