---
name: '[M7][metrics] Proxy metrics + evaluation framework + interleaving experiments'
about: 'Build comprehensive evaluation system with proxy metrics, dashboards, and interleaving for safe experimentation'
title: '[M7][metrics] Proxy metrics + evaluation framework + interleaving experiments'
labels: 'kind/feature, area/eval, priority/P1, status/backlog'
assignees: ''
---

# Subtask 10: Metrics, Dashboards & Evaluation Framework (M7)

## Issue Title
`[M7][metrics] Proxy metrics + evaluation framework + interleaving experiments`

## Summary
Build comprehensive universal evaluation system with proxy metrics, dashboards, and interleaving framework for safe online experimentation across any languages. Enable data-driven ranking improvements.

## Implementation Language
**Primary: Python** (analysis, experimentation, visualization)
**Collection: C++** (metrics logging in serving)

## Technical Requirements
- Proxy metrics implementation (NDCG, diversity, vertical presence)
- Interleaving framework for safe A/B testing
- Real-time dashboards with Grafana
- Automated daily reports
- Statistical significance testing

## Tasks
- [ ] Implement proxy metrics (NDCG@10, navigational success, diversity)
- [ ] Build interleaving framework (Team-Draft algorithm)
- [ ] Create evaluation dataset and ground truth
- [ ] Develop Python analysis and reporting scripts
- [ ] Set up Grafana dashboards with real-time metrics
- [ ] Implement automated daily report generation
- [ ] Add statistical testing for experiment results
- [ ] Build metrics collection in C++ serving layer
- [ ] Create experiment management and tracking

## Acceptance Criteria
- Proxy metrics correlate ≥0.6 with expected ranking quality
- Interleaving framework ready for production experiments
- Dashboards show real-time system health and performance
- Daily reports generated automatically
- Statistical testing prevents false positive results

## Dependencies
- Pandas/NumPy for metrics computation
- Scipy for statistical testing
- Grafana/Prometheus for dashboards
- Matplotlib/Seaborn for visualization
- C++ logging framework for metrics collection

## API Interface
```python
# Python evaluation framework
class EvaluationFramework:
    def compute_proxy_metrics(self, ranked_results, ground_truth):
        """Compute NDCG, diversity, vertical presence"""
        return {
            'ndcg@10': 0.85,
            'diversity_score': 0.72,
            'navigational_success': 0.94
        }

    def run_interleaving_experiment(self, ranker_a, ranker_b,
                                  queries, sample_size=1000):
        """Run Team-Draft interleaving experiment"""
        return ExperimentResult(winner=ranker_a, confidence=0.95)

class InterleavingEngine:
    def create_interleaved_list(self, list_a, list_b):
        """Create interleaved ranking for comparison"""
        pass

    def analyze_clicks(self, interleaved_results, clicks):
        """Analyze which ranker performs better"""
        pass
```

```cpp
// C++ metrics collection
class MetricsCollector {
    void log_query_metrics(const QueryMetrics& metrics);
    void log_ranking_metrics(const RankingMetrics& metrics);

    struct QueryMetrics {
        std::string query;
        std::string language;
        size_t num_results;
        double latency_ms;
        // ... other metrics
    };
};
```

## Files to Create/Modify
- `src/python/evaluation/`
- `src/python/interleaving/`
- `include/metrics/MetricsCollector.h`
- `src/metrics/MetricsCollector.cpp`
- `src/python/dashboard/`
- `tests/evaluation_test.py`

## Notes
- Python for complex analysis and experimentation
- C++ for efficient metrics collection in serving
- Interleaving enables safe production experiments
- Comprehensive logging for debugging and improvement
