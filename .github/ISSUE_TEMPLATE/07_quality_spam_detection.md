---
name: '[M5][quality] One-class spam/quality scoring + site-level roll-ups'
about: 'Implement unsupervised spam and quality detection using one-class machine learning models'
title: '[M5][quality] One-class spam/quality scoring + site-level roll-ups'
labels: 'kind/feature, area/quality, priority/P1, status/backlog'
assignees: ''
---

# Subtask 07: Quality & Spam Detection (M5)

## Issue Title
`[M5][quality] One-class spam detection + site-level quality scoring`

## Summary
Implement universal unsupervised spam and quality detection using one-class machine learning models. Detect low-quality content and spam pages across any languages, applying site-level reputation scoring.

## Implementation Language
**Primary: Python** (ML model training, anomaly detection)
**Serving: C++** (fast scoring, integration)

## Technical Requirements
- One-Class SVM and Isolation Forest models
- Site-level aggregation and reputation
- Feature engineering for spam detection
- Real-time scoring in indexing/serving
- Safe-list for known good domains

## Tasks
- [ ] Extract spam features (text/HTML ratio, keyword density, etc.)
- [ ] Train One-Class SVM on quality content baseline
- [ ] Implement Isolation Forest for anomaly detection
- [ ] Build site-level reputation aggregation
- [ ] Create safe-list for authoritative domains
- [ ] Implement real-time scoring pipeline
- [ ] Add confidence thresholds and fallback behavior
- [ ] Python model training and evaluation scripts
- [ ] Integrate with indexing and serving pipelines

## Acceptance Criteria
- Spam detection precision ≥85% on test spam pages
- False positive rate <2% on quality content
- Site-level filtering removes ≥70% spam domains
- Processing adds minimal latency to pipelines
- Safe-list prevents demotion of known good sites

## Dependencies
- scikit-learn for One-Class SVM and Isolation Forest
- Pandas/NumPy for feature processing
- MongoDB for site reputation storage
- Python for model training pipelines

## API Interface
```python
# Python training and analysis
class SpamDetector:
    def train_models(self, quality_documents, spam_documents=None):
        """Train unsupervised spam detection models"""
        pass

    def score_document(self, document_features) -> float:
        """Return spam score [0,1] - higher = more spammy"""
        pass

    def get_site_reputation(self, domain: str) -> float:
        """Get aggregated site-level reputation"""
        pass
```

```cpp
// C++ serving integration
class QualityScorer {
    double score_document(const DocumentFeatures& doc);
    double get_site_reputation(const std::string& domain);
    bool should_filter(const DocumentFeatures& doc);
};
```

## Files to Create/Modify
- `src/python/spam_detector/`
- `include/quality/QualityScorer.h`
- `src/quality/QualityScorer.cpp`
- `tests/spam_detection_test.py`

## Notes
- Python for complex ML model training and analysis
- C++ for fast scoring in indexing/serving pipelines
- Unsupervised approach (no manual spam labeling needed)
- Site-level aggregation prevents spam farm exploitation
