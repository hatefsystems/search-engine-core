---
name: '[M6][ranking] Feature fusion + MMR diversification + parameter optimization'
about: 'Implement final ranking formula combining multiple signals with diversification and auto-tuning'
title: '[M6][ranking] Feature fusion + MMR diversification + parameter optimization'
labels: 'kind/feature, area/ranking, priority/P1, status/backlog'
assignees: ''
---

# Subtask 08: Ranking Fusion & Diversification (M6)

## Issue Title
`[M6][ranking] Feature fusion + MMR diversification + parameter optimization`

## Summary
Implement universal final ranking formula combining multiple signals (BM25, embeddings, authority, quality) with diversification to prevent domain repetition. Auto-tune weights using proxy metrics that work across any languages.

## Implementation Language
**Primary: Python** (model tuning, experimentation)
**Core: C++** (fast scoring, serving)

## Technical Requirements
- Multi-feature scoring formula implementation
- MMR (Maximal Marginal Relevance) diversification
- Automatic parameter tuning via grid search
- Feature normalization and combination
- Real-time scoring with low latency

## Tasks
- [ ] Implement FinalScore formula with configurable weights
- [ ] Build feature gathering pipeline (BM25, EmbSim, HostRank, etc.)
- [ ] Add MMR diversification algorithm
- [ ] Create parameter tuning framework (grid/line search)
- [ ] Implement feature normalization and scaling
- [ ] Build Python experimentation framework
- [ ] C++ high-performance scoring implementation
- [ ] Add feature caching and precomputation
- [ ] Integrate with query pipeline

## Acceptance Criteria
- Proxy NDCG@10 improves ≥10% vs BM25-only baseline
- Diversification ensures ≤3 results per domain in Top-10
- Parameter tuning converges within reasonable time
- End-to-end scoring adds <5ms latency
- Feature weights correlate with expected importance

## Dependencies
- NumPy/SciPy for optimization
- Pandas for experimentation tracking
- Redis for feature caching
- C++ for performance-critical scoring

## API Interface
```python
# Python experimentation and tuning
class RankerTuner:
    def tune_parameters(self, training_queries, proxy_metrics):
        """Auto-tune ranking weights using proxy metrics"""
        return {'bm25_weight': 0.55, 'emb_weight': 0.15, ...}

    def evaluate_ranking(self, ranked_results, ground_truth):
        """Compute NDCG, diversity metrics"""
        pass
```

```cpp
// C++ high-performance ranking
class Ranker {
    std::vector<ScoredDocument> rank_and_diversify(
        const std::vector<Candidate>& candidates,
        const std::string& query,
        const RankingWeights& weights);

    struct RankingWeights {
        double bm25 = 0.55;
        double embedding_sim = 0.15;
        double host_rank = 0.10;
        // ... other weights
    };
};
```

## Files to Create/Modify
- `src/python/ranker_tuner/`
- `include/ranking/Ranker.h`
- `src/ranking/Ranker.cpp`
- `src/ranking/Diversifier.cpp`
- `tests/ranking_test.cpp`

## Notes
- Python for experimentation, analysis, and parameter tuning
- C++ for production scoring with strict latency requirements
- MMR diversification prevents domain monopolization
- Support A/B testing of different ranking formulas
