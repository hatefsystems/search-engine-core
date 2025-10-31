---
name: '[M2][graph] Link graph construction + HostRank computation'
about: 'Build comprehensive link analysis system to compute host-level PageRank for authority scoring'
title: '[M2][graph] Link graph construction + HostRank computation'
labels: 'kind/feature, area/graph, priority/P1, status/backlog'
assignees: ''
---

# Subtask 03: Link Graph Analysis & HostRank (M2)

## Issue Title
`[M2][graph] Link graph construction + HostRank computation`

## Summary
Build comprehensive multilingual link analysis system to compute host-level PageRank (HostRank) for authority scoring. Process crawled link data from any language websites to construct host-to-host graphs and compute universal prestige scores for ranking.

## Implementation Language
**Primary: Python** (graph processing, algorithms)
**Storage: C++** (integration with MongoDB/Redis)

## Technical Requirements
- Host-level graph construction (eTLD+1 domains)
- PageRank computation with damping factor 0.85
- Incremental updates for new crawl data
- Efficient storage and retrieval of HostRank scores
- Batch processing for large-scale graphs

## Tasks
- [ ] Extract outlinks/inlinks from crawled documents
- [ ] Implement domain normalization (eTLD+1 extraction)
- [ ] Build host-to-host adjacency matrix/graph
- [ ] Implement PageRank algorithm (power iteration)
- [ ] Normalize scores to [0,1] range
- [ ] Create incremental update system
- [ ] Store HostRank in feature store (Redis/MongoDB)
- [ ] Python batch processing pipeline
- [ ] Validation against known authoritative domains

## Acceptance Criteria
- HostRank correlates ≥0.7 with known authority sites
- Incremental updates complete within 30 minutes for 100M edges
- HostRank distribution follows power-law (as expected)
- Memory efficient for billion-edge graphs
- Daily batch processing completes within SLA

## Dependencies
- NetworkX or igraph for graph processing
- NumPy/SciPy for PageRank computation
- MongoDB/Redis for feature storage
- Python multiprocessing for batch processing

## API Interface
```cpp
// C++ feature retrieval
class HostRankStore {
    double get_host_rank(const std::string& domain);
    void update_host_ranks(const std::vector<HostRank>& updates);
};
```

```python
# Python batch processing
class HostRankProcessor:
    def process_crawl_data(self, crawl_documents):
        # Extract links, build graph, compute PageRank
        pass

    def incremental_update(self, new_links):
        # Update existing graph with new data
        pass
```

## Files to Create/Modify
- `src/python/link_graph/`
- `include/features/HostRankStore.h`
- `src/features/HostRankStore.cpp`
- `src/python/hostrank_processor/`
- `tests/hostrank_test.py`

## Notes
- Python for complex graph algorithms and batch processing
- C++ for fast feature retrieval in serving layer
- Support incremental updates to avoid full recomputation
- Scale to handle web-scale link graphs
