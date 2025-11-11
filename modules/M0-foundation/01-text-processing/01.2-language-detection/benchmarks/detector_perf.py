"""
Performance Benchmarks for Language Detection - Task 01.2

Performance targets:
- Throughput: 5000+ detections/second
- Latency: <5ms per detection
- Accuracy: ≥95% on test corpus
- Memory: <200MB with model loaded

Usage:
    python benchmarks/detector_perf.py
"""

import time
import statistics
import sys
from pathlib import Path
from typing import List, Dict

# Add parent to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from text_processing import UniversalLanguageDetector, detect_language_batch
from shared.logger import setup_logger

logger = setup_logger(__name__)


class LanguageDetectionBenchmark:
    """Benchmark suite for language detection."""
    
    def __init__(self):
        """Initialize benchmark."""
        self.detector = None
        self.test_texts = self._get_test_texts()
    
    def _get_test_texts(self) -> Dict[str, str]:
        """Get test texts in various languages."""
        return {
            'en': "The quick brown fox jumps over the lazy dog in the beautiful garden",
            'fa': "روباه قهوه‌ای سریع از روی سگ تنبل در باغ زیبا می‌پرد",
            'ar': "الثعلب البني السريع يقفز فوق الكلب الكسول في الحديقة الجميلة",
            'ru': "Быстрая коричневая лиса прыгает через ленивую собаку в красивом саду",
            'zh': "敏捷的棕色狐狸跳过懒狗在美丽的花园里",
            'ja': "素早い茶色のキツネが美しい庭で怠け者の犬を飛び越える",
            'ko': "빠른 갈색 여우가 아름다운 정원에서 게으른 개를 뛰어넘습니다",
            'es': "El rápido zorro marrón salta sobre el perro perezoso en el hermoso jardín",
            'fr': "Le rapide renard brun saute par-dessus le chien paresseux dans le beau jardin",
            'de': "Der schnelle braune Fuchs springt über den faulen Hund im schönen Garten",
            'hi': "तेज़ भूरी लोमड़ी सुंदर बगीचे में आलसी कुत्ते के ऊपर कूदती है",
            'he': "השועל החום המהיר קופץ מעל הכלב העצלן בגן היפה",
        }
    
    def setup(self):
        """Setup benchmark - load model."""
        print("🔧 Setting up detector...")
        start = time.time()
        
        try:
            self.detector = UniversalLanguageDetector()
            elapsed = time.time() - start
            
            print(f"   ✅ Detector loaded in {elapsed:.3f}s")
            return True
        except Exception as e:
            print(f"   ❌ Failed to load detector: {e}")
            return False
    
    def benchmark_single_detection(self, num_iterations: int = 1000) -> Dict:
        """
        Benchmark single text detection.
        
        Args:
            num_iterations: Number of iterations per language
            
        Returns:
            Benchmark results
        """
        print(f"\n📊 Benchmarking single detection ({num_iterations} iterations)...")
        
        all_latencies = []
        results_by_lang = {}
        
        for lang_code, text in self.test_texts.items():
            latencies = []
            
            # Warmup
            for _ in range(10):
                self.detector.detect(text)
            
            # Measure
            for _ in range(num_iterations):
                start = time.perf_counter()
                result = self.detector.detect(text)
                latency = (time.perf_counter() - start) * 1000  # ms
                latencies.append(latency)
                
                # Verify correctness
                if result.language_code != lang_code:
                    print(f"   ⚠️  {lang_code}: detected as {result.language_code}")
            
            all_latencies.extend(latencies)
            results_by_lang[lang_code] = {
                'mean': statistics.mean(latencies),
                'median': statistics.median(latencies),
                'p95': statistics.quantiles(latencies, n=20)[18],  # 95th percentile
                'p99': statistics.quantiles(latencies, n=100)[98],  # 99th percentile
                'min': min(latencies),
                'max': max(latencies),
            }
        
        overall = {
            'mean': statistics.mean(all_latencies),
            'median': statistics.median(all_latencies),
            'p95': statistics.quantiles(all_latencies, n=20)[18],
            'p99': statistics.quantiles(all_latencies, n=100)[98],
            'min': min(all_latencies),
            'max': max(all_latencies),
            'by_language': results_by_lang
        }
        
        return overall
    
    def benchmark_batch_detection(self, batch_size: int = 100, num_batches: int = 50) -> Dict:
        """
        Benchmark batch detection.
        
        Args:
            batch_size: Number of texts per batch
            num_batches: Number of batches to process
            
        Returns:
            Benchmark results
        """
        print(f"\n📊 Benchmarking batch detection ({batch_size} texts x {num_batches} batches)...")
        
        # Prepare batch
        texts = list(self.test_texts.values()) * (batch_size // len(self.test_texts) + 1)
        texts = texts[:batch_size]
        
        latencies = []
        throughputs = []
        
        # Warmup
        for _ in range(3):
            detect_language_batch(texts)
        
        # Measure
        for _ in range(num_batches):
            start = time.perf_counter()
            results = detect_language_batch(texts)
            elapsed = time.perf_counter() - start
            
            latency = elapsed * 1000  # ms
            throughput = len(results) / elapsed  # detections/sec
            
            latencies.append(latency)
            throughputs.append(throughput)
        
        return {
            'batch_size': batch_size,
            'latency_mean_ms': statistics.mean(latencies),
            'latency_p95_ms': statistics.quantiles(latencies, n=20)[18],
            'throughput_mean': statistics.mean(throughputs),
            'throughput_min': min(throughputs),
            'throughput_max': max(throughputs),
        }
    
    def benchmark_accuracy(self) -> Dict:
        """
        Benchmark detection accuracy on test texts.
        
        Returns:
            Accuracy metrics
        """
        print(f"\n📊 Benchmarking accuracy on {len(self.test_texts)} languages...")
        
        correct = 0
        total = len(self.test_texts)
        confidences = []
        
        for expected_lang, text in self.test_texts.items():
            result = self.detector.detect(text)
            
            if result.language_code == expected_lang:
                correct += 1
            else:
                print(f"   ❌ {expected_lang}: detected as {result.language_code} (conf: {result.confidence:.3f})")
            
            confidences.append(result.confidence)
        
        accuracy = correct / total
        
        return {
            'accuracy': accuracy,
            'correct': correct,
            'total': total,
            'mean_confidence': statistics.mean(confidences),
            'min_confidence': min(confidences),
        }
    
    def print_results(self, single_results: Dict, batch_results: Dict, accuracy_results: Dict):
        """Print benchmark results."""
        print("\n" + "=" * 60)
        print("🎯 LANGUAGE DETECTION BENCHMARK RESULTS")
        print("=" * 60)
        
        # Single detection
        print("\n📍 Single Detection Latency:")
        print(f"   Mean:    {single_results['mean']:.3f} ms")
        print(f"   Median:  {single_results['median']:.3f} ms")
        print(f"   P95:     {single_results['p95']:.3f} ms")
        print(f"   P99:     {single_results['p99']:.3f} ms")
        print(f"   Min:     {single_results['min']:.3f} ms")
        print(f"   Max:     {single_results['max']:.3f} ms")
        
        target_met = "✅" if single_results['mean'] < 5.0 else "❌"
        print(f"\n   Target: <5ms {target_met} (Mean: {single_results['mean']:.3f}ms)")
        
        # Batch detection
        print("\n📦 Batch Detection:")
        print(f"   Batch size:           {batch_results['batch_size']}")
        print(f"   Latency (mean):       {batch_results['latency_mean_ms']:.2f} ms")
        print(f"   Throughput (mean):    {batch_results['throughput_mean']:.0f} detections/sec")
        print(f"   Throughput (min):     {batch_results['throughput_min']:.0f} detections/sec")
        print(f"   Throughput (max):     {batch_results['throughput_max']:.0f} detections/sec")
        
        throughput_target_met = "✅" if batch_results['throughput_mean'] >= 5000 else "❌"
        print(f"\n   Target: 5000+/sec {throughput_target_met} (Mean: {batch_results['throughput_mean']:.0f}/sec)")
        
        # Accuracy
        print("\n🎯 Accuracy:")
        print(f"   Correct:      {accuracy_results['correct']}/{accuracy_results['total']}")
        print(f"   Accuracy:     {accuracy_results['accuracy']:.1%}")
        print(f"   Confidence:   {accuracy_results['mean_confidence']:.3f} (mean)")
        
        accuracy_target_met = "✅" if accuracy_results['accuracy'] >= 0.95 else "❌"
        print(f"\n   Target: ≥95% {accuracy_target_met} (Actual: {accuracy_results['accuracy']:.1%})")
        
        print("\n" + "=" * 60)
        
        # Overall status
        all_targets_met = (
            single_results['mean'] < 5.0 and
            batch_results['throughput_mean'] >= 5000 and
            accuracy_results['accuracy'] >= 0.95
        )
        
        if all_targets_met:
            print("🎉 ALL PERFORMANCE TARGETS MET!")
        else:
            print("⚠️  Some performance targets not met")
        
        print("=" * 60 + "\n")
    
    def run_all(self):
        """Run all benchmarks."""
        if not self.setup():
            return False
        
        single_results = self.benchmark_single_detection(num_iterations=1000)
        batch_results = self.benchmark_batch_detection(batch_size=100, num_batches=50)
        accuracy_results = self.benchmark_accuracy()
        
        self.print_results(single_results, batch_results, accuracy_results)
        
        return True


def main():
    """Main benchmark entry point."""
    print("=" * 60)
    print("🚀 Language Detection Performance Benchmark")
    print("=" * 60)
    print("\nTask 01.2: Universal Language Detection")
    print("Testing FastText-based detection on 12 languages")
    print("")
    
    benchmark = LanguageDetectionBenchmark()
    success = benchmark.run_all()
    
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())

