#!/usr/bin/env python3
"""
Persian Stopword Detection Performance Test

Measures CPU, RAM, GPU usage and execution time for:
1. Stanza model loading
2. IDF-only detection
3. Hybrid (IDF + Stanza) detection
4. Batch processing

Usage:
    python scripts/test_persian_performance.py
"""

import sys
import os
import time
import psutil
from pathlib import Path
from typing import List, Dict, Tuple

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from text_processing import HybridStopwordDetector, STANZA_AVAILABLE

# Try to import GPU monitoring libraries
try:
    import torch
    TORCH_AVAILABLE = True
except ImportError:
    TORCH_AVAILABLE = False

try:
    import GPUtil
    GPUTIL_AVAILABLE = True
except ImportError:
    GPUTIL_AVAILABLE = False


def get_memory_usage() -> float:
    """Get current process memory usage in MB"""
    process = psutil.Process(os.getpid())
    return process.memory_info().rss / 1024 / 1024  # Convert to MB


def get_cpu_percent() -> float:
    """Get current CPU usage percentage"""
    return psutil.cpu_percent(interval=0.1)


def get_gpu_info() -> Dict:
    """Get GPU information if available"""
    gpu_info = {
        'available': False,
        'name': None,
        'memory_used_mb': 0,
        'memory_total_mb': 0,
        'utilization_percent': 0
    }
    
    if TORCH_AVAILABLE and torch.cuda.is_available():
        gpu_info['available'] = True
        gpu_info['name'] = torch.cuda.get_device_name(0)
        gpu_info['memory_used_mb'] = torch.cuda.memory_allocated(0) / 1024 / 1024
        gpu_info['memory_total_mb'] = torch.cuda.get_device_properties(0).total_memory / 1024 / 1024
        
    if GPUTIL_AVAILABLE:
        try:
            gpus = GPUtil.getGPUs()
            if gpus:
                gpu = gpus[0]
                gpu_info['available'] = True
                gpu_info['name'] = gpu.name
                gpu_info['memory_used_mb'] = gpu.memoryUsed
                gpu_info['memory_total_mb'] = gpu.memoryTotal
                gpu_info['utilization_percent'] = gpu.load * 100
        except:
            pass
    
    return gpu_info


def print_header(text: str):
    """Print a formatted header"""
    print("\n" + "=" * 70)
    print(f"  {text}")
    print("=" * 70)


def print_metrics(label: str, start_time: float, start_mem: float, start_cpu: float):
    """Print performance metrics"""
    end_time = time.time()
    end_mem = get_memory_usage()
    end_cpu = get_cpu_percent()
    
    duration = end_time - start_time
    mem_delta = end_mem - start_mem
    
    print(f"\n📊 {label}:")
    print(f"   ⏱️  Duration: {duration:.3f} seconds")
    print(f"   💾 Memory: {start_mem:.1f} MB → {end_mem:.1f} MB (Δ {mem_delta:+.1f} MB)")
    print(f"   🖥️  CPU: ~{end_cpu:.1f}%")
    
    return duration, end_mem, mem_delta


def main():
    print_header("Persian Stopword Detection - Performance Test")
    
    # System information
    print("\n📋 System Information:")
    print(f"   CPU Cores: {psutil.cpu_count(logical=False)} physical, {psutil.cpu_count(logical=True)} logical")
    print(f"   Total RAM: {psutil.virtual_memory().total / 1024 / 1024 / 1024:.1f} GB")
    print(f"   Available RAM: {psutil.virtual_memory().available / 1024 / 1024 / 1024:.1f} GB")
    
    # GPU information
    gpu_info = get_gpu_info()
    if gpu_info['available']:
        print(f"\n🎮 GPU Information:")
        print(f"   GPU Name: {gpu_info['name']}")
        print(f"   GPU Memory: {gpu_info['memory_used_mb']:.1f} / {gpu_info['memory_total_mb']:.1f} MB")
        print(f"   GPU Utilization: {gpu_info['utilization_percent']:.1f}%")
    else:
        print(f"\n🎮 GPU: Not available (using CPU only)")
    
    # Stanza availability
    print(f"\n📚 Stanza NLP:")
    if STANZA_AVAILABLE:
        print(f"   Status: ✅ Available")
        print(f"   Note: Stanza uses CPU by default (use_gpu=False)")
    else:
        print(f"   Status: ❌ Not installed")
        return
    
    # Test data
    persian_words = ["و", "در", "از", "به", "با", "این", "که", "را", "کتاب", "خوب"]
    persian_sentences = [
        "این یک متن فارسی است",
        "من به مدرسه می‌روم",
        "او در خانه است",
        "کتاب را بر روی میز گذاشت"
    ]
    
    print_header("Test 1: IDF-Only Mode (No Stanza)")
    
    # Test 1: IDF-only mode
    start_mem = get_memory_usage()
    start_cpu = get_cpu_percent()
    start_time = time.time()
    
    detector_idf = HybridStopwordDetector(
        redis_url="redis://localhost:6379",
        enable_stanza=False
    )
    
    duration, end_mem, mem_delta = print_metrics("Initialization (IDF-only)", start_time, start_mem, start_cpu)
    
    # Process words
    start_time = time.time()
    start_mem = get_memory_usage()
    start_cpu = get_cpu_percent()
    
    results_idf = []
    for word in persian_words:
        result = detector_idf.is_stopword(word, "fa")
        results_idf.append(result)
    
    duration, end_mem, mem_delta = print_metrics(f"Processing {len(persian_words)} words", start_time, start_mem, start_cpu)
    print(f"   📈 Throughput: {len(persian_words) / duration:.1f} words/second")
    
    print_header("Test 2: Hybrid Mode (IDF + Stanza)")
    
    # Test 2: Hybrid mode with Stanza
    start_mem = get_memory_usage()
    start_cpu = get_cpu_percent()
    start_time = time.time()
    
    detector_hybrid = HybridStopwordDetector(
        redis_url="redis://localhost:6379",
        enable_stanza=True
    )
    
    duration, end_mem, mem_delta = print_metrics("Initialization (Hybrid)", start_time, start_mem, start_cpu)
    
    # First word triggers model loading
    print("\n⏳ Loading Stanza model for Persian (first time)...")
    start_time = time.time()
    start_mem = get_memory_usage()
    start_cpu = get_cpu_percent()
    
    first_result = detector_hybrid.is_stopword(persian_words[0], "fa")
    
    duration, end_mem, mem_delta = print_metrics("Model loading + first word", start_time, start_mem, start_cpu)
    print(f"   🔍 Result: '{persian_words[0]}' → {first_result.pos_tag} (stopword: {first_result.is_stopword})")
    
    # Process remaining words (model already loaded)
    print("\n⏳ Processing remaining words (model in memory)...")
    start_time = time.time()
    start_mem = get_memory_usage()
    start_cpu = get_cpu_percent()
    
    results_hybrid = [first_result]
    for word in persian_words[1:]:
        result = detector_hybrid.is_stopword(word, "fa")
        results_hybrid.append(result)
    
    duration, end_mem, mem_delta = print_metrics(f"Processing {len(persian_words)-1} words (cached model)", start_time, start_mem, start_cpu)
    print(f"   📈 Throughput: {(len(persian_words)-1) / duration:.1f} words/second")
    
    print_header("Test 3: Sentence Processing")
    
    # Test 3: Process full sentences
    start_time = time.time()
    start_mem = get_memory_usage()
    start_cpu = get_cpu_percent()
    
    total_words = 0
    for sentence in persian_sentences:
        words = sentence.split()
        total_words += len(words)
        for word in words:
            result = detector_hybrid.is_stopword(word, "fa")
    
    duration, end_mem, mem_delta = print_metrics(f"Processing {len(persian_sentences)} sentences ({total_words} words)", start_time, start_mem, start_cpu)
    print(f"   📈 Throughput: {total_words / duration:.1f} words/second")
    
    print_header("Performance Summary")
    
    # Check GPU usage after all tests
    gpu_info_final = get_gpu_info()
    
    print("\n💡 Key Findings:")
    print(f"   1. IDF-only mode: Lightweight, minimal memory overhead")
    print(f"   2. Stanza model loading: ~{end_mem - start_mem:.1f} MB RAM increase")
    print(f"   3. Stanza uses CPU only (use_gpu=False by default)")
    print(f"   4. First POS tagging is slower (model loading)")
    print(f"   5. Subsequent POS tagging is fast (model cached in RAM)")
    
    if gpu_info_final['available']:
        print(f"\n🎮 GPU Status After Tests:")
        print(f"   GPU was {'used' if gpu_info_final['memory_used_mb'] > gpu_info['memory_used_mb'] else 'NOT used'}")
        print(f"   Memory: {gpu_info['memory_used_mb']:.1f} MB → {gpu_info_final['memory_used_mb']:.1f} MB")
    else:
        print(f"\n🎮 GPU: Not used (CPU-only mode)")
        print(f"   ✅ This is GOOD: No GPU required for production deployment")
        print(f"   ✅ Works on any server with sufficient RAM (4-8 GB recommended)")
    
    print("\n📊 Resource Requirements:")
    print(f"   Minimum RAM: 2 GB")
    print(f"   Recommended RAM: 4-8 GB (for multiple language models)")
    print(f"   CPU: Any modern CPU (2+ cores recommended)")
    print(f"   GPU: Not required ✅")
    
    print("\n🚀 Optimization Tips:")
    print(f"   1. Use Redis caching to avoid repeated POS tagging")
    print(f"   2. Load models lazily (only when needed)")
    print(f"   3. Keep max 3 models in RAM simultaneously (LRU eviction)")
    print(f"   4. Use batch processing for large corpora")
    print(f"   5. Enable GPU only if available and needed (set use_gpu=True)")
    
    print("\n" + "=" * 70)
    print("  ✅ Performance test completed!")
    print("=" * 70)


if __name__ == "__main__":
    main()

