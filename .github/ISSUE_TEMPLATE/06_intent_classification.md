---
name: '[M4][classification] Weakly-supervised intent classifier + vertical detectors'
about: 'Build machine learning classifiers for query intent detection and content vertical classification'
title: '[M4][classification] Weakly-supervised intent classifier + vertical detectors'
labels: 'kind/feature, area/classification, priority/P1, status/backlog'
assignees: ''
---

# Subtask 06: Intent Detection & Vertical Classification (M4)

## Issue Title
`[M4][classification] Weakly-supervised intent classifier + vertical detectors`

## Summary
Build universal machine learning classifiers for query intent detection (informational/transactional/navigational) and content vertical classification (Book/Product/Article) across any languages using weak supervision from structured signals.

## Implementation Language
**Primary: Python** (ML training, classification)
**Serving: C++** (fast prediction, feature extraction)

## Technical Requirements
- Weak supervision from structured page signals
- Gradient Boosting Decision Trees (GBDT) for intent
- Vertical detectors for content categorization
- Feature engineering from queries and documents
- Real-time classification in serving pipeline

## Tasks
- [ ] Generate weak labels from structured content (Product/Offer pages)
- [ ] Create navigational seed patterns (brand/domain matching)
- [ ] Train GBDT classifier for intent (Info/Trans/Nav)
- [ ] Build vertical classifiers (Book/Product/Article/Download)
- [ ] Implement feature extraction pipeline
- [ ] Create Python training and evaluation scripts
- [ ] Build C++ prediction service with model serialization
- [ ] Integrate with query processing pipeline
- [ ] Add confidence scoring and fallback handling

## Acceptance Criteria
- Intent classification accuracy ≥75% on labeled test set
- Vertical detection precision ≥80% for structured content
- Processing adds <2ms latency to query pipeline
- Handles mixed-language queries appropriately
- Model updates work without service interruption

## Dependencies
- scikit-learn/XGBoost for classification
- Pandas for feature engineering
- ONNX for model serialization (optional)
- C++ ML libraries or custom prediction code

## API Interface
```python
# Python training and evaluation
class IntentClassifier:
    def train(self, queries, weak_labels):
        """Train intent classifier from weak supervision"""
        pass

    def predict_intent(self, query: str) -> Dict[str, float]:
        """Predict intent probabilities"""
        return {'informational': 0.6, 'transactional': 0.3, 'navigational': 0.1}
```

```cpp
// C++ prediction service
class IntentPredictor {
    IntentPrediction predict_intent(const std::string& query);
    VerticalType predict_vertical(const DocumentFeatures& doc);

    struct IntentPrediction {
        double informational_prob;
        double transactional_prob;
        double navigational_prob;
    };
};
```

## Files to Create/Modify
- `src/python/intent_classifier/`
- `include/classification/IntentPredictor.h`
- `src/classification/IntentPredictor.cpp`
- `src/python/vertical_detector/`
- `tests/classification_test.py`

## Notes
- Python for model training and complex feature engineering
- C++ for fast prediction in serving layer
- Use weak supervision to avoid manual labeling
- Support model versioning and A/B testing
