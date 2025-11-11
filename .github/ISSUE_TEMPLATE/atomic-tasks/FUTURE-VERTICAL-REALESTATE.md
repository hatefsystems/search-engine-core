# 🏘️ Future Task: Real Estate Vertical Implementation

## 📋 Overview
**Purpose:** Add Real Estate vertical search capability to the universal search engine  
**Status:** 🟡 Planning (not started)  
**Priority:** P2 (After universal search MVP)  
**Estimated Duration:** 3-4 weeks  
**Prerequisites:** M0-M3 completed (Universal foundation)

---

## 🎯 Business Context

### Use Case
Enable specialized real estate search with queries like:
```
"اجاره خانه 70 متری تهرانپارس زیر 10 میلیون تومان و سقف پیش پرداخت 200 میلیون تومان تک واحدی"
```

### Success Criteria
- [ ] Extract structured real estate data (price, size, location, features)
- [ ] Detect real estate vertical with ≥90% accuracy
- [ ] Match user requirements (budget, size, location) precisely
- [ ] Rank results by relevance considering RE-specific features
- [ ] Support Persian real estate terminology

### Integration Points
- Partner real estate website (TBD)
- Or public data crawling (ethical, robots.txt compliant)
- Schema.org RealEstateListing support

---

## 📦 Required Tasks

### Phase 1: Foundation (1 week)

#### ✅ Task 1: Schema.org RealEstate Parser
**File:** `src/python/extractors/realestate_schema_parser.py`  
**Duration:** 2 days  
**Depends on:** Task 04.1 (Schema.org Parser)

**What to build:**
```python
class RealEstateSchemaParser:
    """Parse Schema.org RealEstateListing"""
    
    def parse(self, html: str) -> Dict:
        """
        Extract structured data from real estate listing
        
        Schema.org types supported:
        - RealEstateListing
        - Apartment
        - House
        - SingleFamilyResidence
        """
        return {
            '@type': 'RealEstateListing',
            'property': {
                '@type': 'Apartment',
                'numberOfRooms': 2,
                'floorSize': {
                    '@type': 'QuantitativeValue',
                    'value': 70,
                    'unitCode': 'MTK'  # متر مربع
                },
                'address': {
                    '@type': 'PostalAddress',
                    'addressLocality': 'تهرانپارس',
                    'addressRegion': 'تهران'
                }
            },
            'price': 10000000,  # تومان/ماه
            'priceCurrency': 'IRR',
            'rentalPrice': 10000000,
            'securityDeposit': 200000000,  # پیش پرداخت
            'availableFrom': '2024-01-01',
            'features': ['parking', 'elevator', 'standalone']
        }

# Example usage
parser = RealEstateSchemaParser()
data = parser.parse(html_content)
```

**Checklist:**
- [ ] JSON-LD parser for RealEstateListing
- [ ] Microdata parser support
- [ ] RDFa parser support
- [ ] Field mapping to internal schema
- [ ] Unit tests (50+ test cases)
- [ ] Multi-language support (Persian/English)

**Files created:**
- `src/python/extractors/realestate_schema_parser.py` (300 lines)
- `tests/test_realestate_schema.py` (60 test cases)
- `docs/realestate-schema-guide.md`

---

#### ✅ Task 2: Custom Field Extraction
**File:** `src/python/extractors/realestate_field_extractor.py`  
**Duration:** 3 days  
**Depends on:** Task 04.2 (Price Extraction)

**What to build:**
```python
class RealEstateFieldExtractor:
    """Extract RE-specific fields from text"""
    
    def extract(self, doc: Dict) -> Dict:
        """
        Extract real estate fields from document
        
        Returns structured data even without Schema.org
        """
        return {
            # Basic info
            'property_type': self.extract_property_type(doc),  # آپارتمان، ویلا، مغازه
            'transaction_type': self.extract_transaction(doc),  # اجاره، خرید، رهن
            
            # Size
            'area_sqm': self.extract_area(doc),  # 70 متر
            'rooms': self.extract_rooms(doc),  # 2 خواب
            
            # Location
            'city': self.extract_city(doc),  # تهران
            'neighborhood': self.extract_neighborhood(doc),  # تهرانپارس
            'district': self.extract_district(doc),  # منطقه 4
            
            # Pricing (in Toman)
            'rental_price_monthly': self.extract_rental(doc),  # 10,000,000
            'security_deposit': self.extract_deposit(doc),  # 200,000,000
            'sale_price': self.extract_sale_price(doc),  # قیمت خرید
            
            # Features
            'floor_number': self.extract_floor(doc),  # طبقه 3
            'building_age': self.extract_age(doc),  # 5 ساله
            'has_parking': self.has_parking(doc),  # bool
            'has_elevator': self.has_elevator(doc),  # bool
            'has_storage': self.has_storage(doc),  # انباری
            'is_standalone': self.is_standalone(doc),  # تک واحدی
            'is_renovated': self.is_renovated(doc),  # بازسازی شده
            
            # Additional
            'year_built': self.extract_year_built(doc),
            'direction': self.extract_direction(doc),  # شمالی، جنوبی
            'view': self.extract_view(doc),  # ویو دارد
        }
    
    def extract_area(self, doc: Dict) -> Optional[int]:
        """Extract area in square meters"""
        text = doc.get('text', '')
        
        # Patterns: "70 متر", "70متری", "70 متر مربع", "70m2"
        patterns = [
            r'(\d+)\s*متر(?:\s*مربع)?',
            r'(\d+)\s*متری',
            r'(\d+)\s*m2',
            r'(\d+)\s*sqm'
        ]
        
        for pattern in patterns:
            match = re.search(pattern, text)
            if match:
                return int(match.group(1))
        
        return None
    
    def extract_rental(self, doc: Dict) -> Optional[int]:
        """Extract monthly rental price in Toman"""
        text = doc.get('text', '')
        
        # "10 میلیون تومان", "10میلیون", "10M"
        # Near "اجاره" or "ماه"
        match = re.search(
            r'(?:اجاره|ماهانه).*?(\d+)\s*(?:میلیون|M)',
            text,
            re.IGNORECASE
        )
        
        if match:
            millions = int(match.group(1))
            return millions * 1_000_000
        
        return None
    
    def extract_deposit(self, doc: Dict) -> Optional[int]:
        """Extract security deposit (پیش پرداخت) in Toman"""
        text = doc.get('text', '')
        
        # "200 میلیون پیش", "ودیعه 200 میلیون"
        patterns = [
            r'(?:پیش|ودیعه|رهن).*?(\d+)\s*(?:میلیون|M)',
            r'(\d+)\s*(?:میلیون|M).*?(?:پیش|ودیعه)'
        ]
        
        for pattern in patterns:
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                millions = int(match.group(1))
                return millions * 1_000_000
        
        return None
    
    def extract_neighborhood(self, doc: Dict) -> Optional[str]:
        """Extract neighborhood/district from text"""
        text = doc.get('text', '')
        
        # Known neighborhoods in Tehran (expand as needed)
        neighborhoods = [
            'تهرانپارس', 'ونک', 'نیاوران', 'فرمانیه', 'شهرک غرب',
            'سعادت آباد', 'جردن', 'الهیه', 'زعفرانیه', 'پاسداران',
            # ... add more
        ]
        
        for neighborhood in neighborhoods:
            if neighborhood in text:
                return neighborhood
        
        return None
    
    def is_standalone(self, doc: Dict) -> bool:
        """Check if property is standalone (تک واحدی)"""
        text = doc.get('text', '')
        keywords = ['تک واحد', 'تک‌واحد', 'تکواحد', 'standalone']
        return any(kw in text for kw in keywords)
```

**Regex Patterns Reference:**
```python
PATTERNS = {
    'area': [
        r'(\d+)\s*متر(?:\s*مربع)?',
        r'(\d+)\s*متری',
        r'(\d+)\s*m2'
    ],
    'rooms': [
        r'(\d+)\s*(?:خواب|اتاق)',
        r'(\d+)\s*bedroom'
    ],
    'price_toman': [
        r'(\d+)\s*میلیون\s*تومان',
        r'(\d+)\s*میلیون',
        r'(\d+)M'
    ],
    'floor': [
        r'طبقه\s*(\d+)',
        r'floor\s*(\d+)'
    ],
    'age': [
        r'(\d+)\s*ساله',
        r'(\d+)\s*سال\s*ساخت',
        r'(\d+)\s*years?\s*old'
    ]
}
```

**Checklist:**
- [ ] Area extraction (متر مربع)
- [ ] Room count extraction
- [ ] Price extraction (rental, deposit, sale)
- [ ] Location extraction (city, neighborhood, district)
- [ ] Floor and building age
- [ ] Boolean features (parking, elevator, etc.)
- [ ] Standalone detection (تک واحدی)
- [ ] Unit tests (100+ cases)
- [ ] Persian number handling (۱۲۳ → 123)

**Files created:**
- `src/python/extractors/realestate_field_extractor.py` (500 lines)
- `tests/test_realestate_extraction.py` (100+ cases)
- `data/patterns/realestate_patterns.json`
- `docs/realestate-field-extraction.md`

---

### Phase 2: Vertical Detection (3 days)

#### ✅ Task 3: Real Estate Vertical Detector
**File:** `src/python/vertical_detector/realestate_detector.py`  
**Duration:** 3 days  
**Depends on:** Task 06.3 (Vertical Detectors)

**What to build:**
```python
class RealEstateDetector(BaseVerticalDetector):
    """Detect real estate content"""
    
    def detect(self, doc: Dict) -> VerticalResult:
        """
        Detect if document is a real estate listing
        
        Returns confidence score and signals
        """
        score = 0.0
        signals = []
        
        # Schema.org signals (highest confidence)
        if doc.get('schema_type') in ['RealEstateListing', 'Apartment', 'House']:
            score += 0.5
            signals.append('schema.org')
        
        # Price patterns (rental or sale)
        if self._has_price_pattern(doc):
            score += 0.25
            signals.append('price_pattern')
        
        # Size/area mentions
        if self._has_area_pattern(doc):
            score += 0.15
            signals.append('area_sqm')
        
        # Location signals
        if self._has_location_pattern(doc):
            score += 0.1
            signals.append('location')
        
        # RE keywords
        if self._has_realestate_keywords(doc):
            score += 0.1
            signals.append('keywords')
        
        # URL patterns
        if self._has_realestate_url(doc):
            score += 0.05
            signals.append('url_pattern')
        
        if score >= 0.6:
            return VerticalResult(
                vertical='RealEstate',
                confidence=min(score, 1.0),
                signals=signals
            )
        
        return VerticalResult(vertical='General', confidence=0.0, signals=[])
    
    def _has_price_pattern(self, doc: Dict) -> bool:
        """Check for price patterns"""
        text = doc.get('text', '')
        patterns = [
            r'\d+\s*میلیون\s*تومان',
            r'اجاره.*\d+\s*میلیون',
            r'رهن.*\d+\s*میلیون',
            r'قیمت.*\d+\s*میلیون'
        ]
        return any(re.search(p, text) for p in patterns)
    
    def _has_area_pattern(self, doc: Dict) -> bool:
        """Check for area/size mentions"""
        text = doc.get('text', '')
        return bool(re.search(r'\d+\s*متر', text))
    
    def _has_location_pattern(self, doc: Dict) -> bool:
        """Check for location mentions"""
        text = doc.get('text', '')
        cities = ['تهران', 'مشهد', 'اصفهان', 'شیراز', 'کرج', 'تبریز']
        return any(city in text for city in cities)
    
    def _has_realestate_keywords(self, doc: Dict) -> bool:
        """Check for RE-specific keywords"""
        text = doc.get('text', '').lower()
        keywords = [
            'اجاره', 'خرید', 'فروش', 'ملک', 'آپارتمان', 'خانه', 'ویلا',
            'مغازه', 'زمین', 'واحد', 'تک واحدی', 'پیش پرداخت', 'ودیعه'
        ]
        count = sum(1 for kw in keywords if kw in text)
        return count >= 2  # At least 2 keywords
    
    def _has_realestate_url(self, doc: Dict) -> bool:
        """Check URL patterns"""
        url = doc.get('url', '').lower()
        patterns = ['/property/', '/listing/', '/rent/', '/sale/', '/real-estate/']
        return any(p in url for p in patterns)
```

**Checklist:**
- [ ] Schema.org detection
- [ ] Price pattern detection
- [ ] Area/size detection
- [ ] Location detection
- [ ] Keyword matching
- [ ] URL pattern matching
- [ ] Confidence calibration
- [ ] Unit tests (80+ cases)
- [ ] Integration with vertical registry

**Files created:**
- `src/python/vertical_detector/realestate_detector.py` (300 lines)
- `tests/test_realestate_detector.py` (80+ cases)
- `docs/realestate-detector-guide.md`

---

### Phase 3: Custom Ranking Features (1 week)

#### ✅ Task 4: Real Estate Ranking Features
**File:** `src/cpp/ranking/RealEstateFeatures.cpp`  
**Duration:** 5 days  
**Depends on:** Task 08.1 (Feature Fusion)

**What to build:**
```cpp
// include/ranking/RealEstateFeatures.h
class RealEstateFeatures {
public:
    // Price matching
    static double PriceMatch(const Query& query, const Document& doc);
    
    // Location proximity
    static double LocationProximity(const Query& query, const Document& doc);
    
    // Size matching
    static double SizeMatch(const Query& query, const Document& doc);
    
    // Feature matching
    static double FeatureMatch(const Query& query, const Document& doc);
};

// src/cpp/ranking/RealEstateFeatures.cpp
double RealEstateFeatures::PriceMatch(const Query& query, const Document& doc) {
    // Extract budget from query: "زیر 10 میلیون"
    auto budget = query.extractBudget();
    if (!budget.has_value()) return 0.5;  // Neutral
    
    // Get rental price from document
    auto price = doc.getRentalPrice();
    if (!price.has_value()) return 0.5;
    
    double budget_val = budget.value();
    double price_val = price.value();
    
    // Perfect match: within budget
    if (price_val <= budget_val) return 1.0;
    
    // Close: within 20% over budget
    if (price_val <= budget_val * 1.2) return 0.7;
    
    // Far over budget
    double ratio = budget_val / price_val;
    return std::max(0.0, ratio);
}

double RealEstateFeatures::LocationProximity(const Query& query, const Document& doc) {
    auto query_location = query.extractLocation();  // "تهرانپارس"
    if (!query_location.has_value()) return 0.5;
    
    auto doc_neighborhood = doc.getNeighborhood();
    if (!doc_neighborhood.has_value()) return 0.5;
    
    std::string q_loc = query_location.value();
    std::string d_loc = doc_neighborhood.value();
    
    // Exact neighborhood match
    if (q_loc == d_loc) return 1.0;
    
    // Same district/zone
    if (sameDistrict(q_loc, d_loc)) return 0.7;
    
    // Same city
    if (sameCity(q_loc, d_loc)) return 0.3;
    
    return 0.0;
}

double RealEstateFeatures::SizeMatch(const Query& query, const Document& doc) {
    auto query_size = query.extractSize();  // "70 متری"
    if (!query_size.has_value()) return 0.5;
    
    auto doc_size = doc.getAreaSqm();
    if (!doc_size.has_value()) return 0.5;
    
    int q_size = query_size.value();
    int d_size = doc_size.value();
    
    int diff = std::abs(q_size - d_size);
    
    // Very close (±5 sqm)
    if (diff <= 5) return 1.0;
    
    // Acceptable (±15 sqm)
    if (diff <= 15) return 0.7;
    
    // Okay (±30 sqm)
    if (diff <= 30) return 0.4;
    
    // Far
    return std::max(0.0, 1.0 - (diff / 100.0));
}

double RealEstateFeatures::FeatureMatch(const Query& query, const Document& doc) {
    auto required_features = query.extractFeatures();  // ["تک واحدی", "پارکینگ"]
    if (required_features.empty()) return 0.5;
    
    int matched = 0;
    int total = required_features.size();
    
    for (const auto& feature : required_features) {
        if (doc.hasFeature(feature)) {
            matched++;
        }
    }
    
    return static_cast<double>(matched) / total;
}
```

**FinalScore Formula for Real Estate:**
```cpp
// src/cpp/ranking/RealEstateRanker.cpp
double RealEstateRanker::calculateScore(const Query& query, const Document& doc) {
    return 
        0.30 * BM25(query, doc) +              // Lexical relevance (reduced)
        0.15 * EmbeddingSimilarity(query, doc) +
        0.10 * HostRank(doc) +
        0.12 * PriceMatch(query, doc) +        // 🆕 Price importance
        0.10 * LocationProximity(query, doc) + // 🆕 Location critical
        0.08 * SizeMatch(query, doc) +         // 🆕 Size matching
        0.07 * FeatureMatch(query, doc) +      // 🆕 Feature matching
        0.05 * Freshness(doc) +                // Recent listings
        - 0.08 * Spamness(doc) +
        0.03 * IntentAlign(query, doc);
}
```

**Checklist:**
- [ ] PriceMatch implementation
- [ ] LocationProximity implementation
- [ ] SizeMatch implementation
- [ ] FeatureMatch implementation
- [ ] Query entity extraction (budget, location, size, features)
- [ ] Document field access
- [ ] Weight tuning
- [ ] Unit tests (100+ cases)
- [ ] Integration with ranking pipeline

**Files created:**
- `include/ranking/RealEstateFeatures.h` (100 lines)
- `src/cpp/ranking/RealEstateFeatures.cpp` (400 lines)
- `src/cpp/ranking/RealEstateRanker.cpp` (200 lines)
- `tests/test_realestate_ranking.cpp` (100+ cases)
- `docs/realestate-ranking-guide.md`

---

### Phase 4: Query Processing (3 days)

#### ✅ Task 5: Real Estate Query Parser
**File:** `src/python/query/realestate_query_parser.py`  
**Duration:** 3 days  
**Depends on:** Task 09.6 (Query Rewriting)

**What to build:**
```python
class RealEstateQueryParser:
    """Parse real estate queries and extract entities"""
    
    def parse(self, query: str) -> RealEstateQuery:
        """
        Parse query like:
        "اجاره خانه 70 متری تهرانپارس زیر 10 میلیون تومان پیش 200 میلیون تک واحدی"
        
        Returns structured query with entities
        """
        return RealEstateQuery(
            transaction_type=self.extract_transaction(query),  # 'rent'
            property_type=self.extract_property_type(query),   # 'apartment'
            size=self.extract_size(query),                     # 70
            location=self.extract_location(query),             # 'تهرانپارس'
            max_rental=self.extract_budget(query),             # 10000000
            max_deposit=self.extract_deposit_limit(query),     # 200000000
            features=self.extract_features(query),             # ['standalone']
            rooms=self.extract_rooms(query),                   # None
            min_size=self.extract_size_range(query)[0],        # None
            max_size=self.extract_size_range(query)[1],        # None
        )
    
    def extract_transaction(self, query: str) -> Optional[str]:
        """Extract transaction type: rent, sale, mortgage"""
        if 'اجاره' in query or 'rent' in query.lower():
            return 'rent'
        if 'خرید' in query or 'فروش' in query or 'sale' in query.lower():
            return 'sale'
        if 'رهن' in query:
            return 'mortgage'
        return None
    
    def extract_property_type(self, query: str) -> Optional[str]:
        """Extract property type"""
        types = {
            'apartment': ['آپارتمان', 'خانه', 'واحد', 'apartment'],
            'house': ['ویلا', 'house', 'villa'],
            'commercial': ['مغازه', 'shop', 'store', 'commercial'],
            'land': ['زمین', 'land']
        }
        
        for ptype, keywords in types.items():
            if any(kw in query for kw in keywords):
                return ptype
        
        return None
    
    def extract_size(self, query: str) -> Optional[int]:
        """Extract size in square meters"""
        match = re.search(r'(\d+)\s*متر', query)
        if match:
            return int(match.group(1))
        return None
    
    def extract_location(self, query: str) -> Optional[str]:
        """Extract location/neighborhood"""
        # List of known neighborhoods
        neighborhoods = self._load_neighborhoods()
        
        for neighborhood in neighborhoods:
            if neighborhood in query:
                return neighborhood
        
        return None
    
    def extract_budget(self, query: str) -> Optional[int]:
        """Extract budget limit (in Toman)"""
        # "زیر 10 میلیون", "تا 10 میلیون", "حداکثر 10 میلیون"
        patterns = [
            r'(?:زیر|تا|حداکثر|max)\s*(\d+)\s*(?:میلیون|M)',
            r'(\d+)\s*(?:میلیون|M).*?(?:زیر|تا|حداکثر)'
        ]
        
        for pattern in patterns:
            match = re.search(pattern, query, re.IGNORECASE)
            if match:
                millions = int(match.group(1))
                return millions * 1_000_000
        
        return None
    
    def extract_deposit_limit(self, query: str) -> Optional[int]:
        """Extract deposit/down payment limit"""
        # "پیش 200 میلیون", "ودیعه 200"
        patterns = [
            r'(?:پیش|ودیعه|deposit).*?(\d+)\s*(?:میلیون|M)',
        ]
        
        for pattern in patterns:
            match = re.search(pattern, query, re.IGNORECASE)
            if match:
                millions = int(match.group(1))
                return millions * 1_000_000
        
        return None
    
    def extract_features(self, query: str) -> List[str]:
        """Extract required features"""
        features = []
        
        feature_keywords = {
            'standalone': ['تک واحد', 'تک‌واحد'],
            'parking': ['پارکینگ', 'parking'],
            'elevator': ['آسانسور', 'elevator'],
            'storage': ['انباری', 'storage'],
            'renovated': ['بازسازی', 'نوساز', 'renovated'],
            'furnished': ['مبله', 'furnished'],
        }
        
        for feature, keywords in feature_keywords.items():
            if any(kw in query for kw in keywords):
                features.append(feature)
        
        return features
```

**Checklist:**
- [ ] Transaction type extraction (rent/sale)
- [ ] Property type extraction
- [ ] Size extraction
- [ ] Location extraction
- [ ] Budget extraction
- [ ] Deposit limit extraction
- [ ] Features extraction
- [ ] Room count extraction
- [ ] Unit tests (100+ cases)
- [ ] Integration with query pipeline

**Files created:**
- `src/python/query/realestate_query_parser.py` (400 lines)
- `tests/test_realestate_query_parser.py` (100+ cases)
- `data/geo/neighborhoods.json` (neighborhood list)
- `docs/realestate-query-parsing.md`

---

### Phase 5: Integration & Testing (3 days)

#### ✅ Task 6: End-to-End Integration
**Duration:** 3 days  
**Depends on:** All previous tasks

**What to integrate:**
1. Schema parser → Field extraction
2. Vertical detection → Routing
3. Query parsing → Feature extraction
4. Ranking features → Final scoring
5. API response formatting

**Integration Points:**
```cpp
// src/controllers/SearchController.cpp

// Add to search endpoint
if (verticalDetector->detect(query).vertical == "RealEstate") {
    // Use RealEstate-specific pipeline
    auto reQueryParser = new RealEstateQueryParser();
    auto parsedQuery = reQueryParser->parse(query);
    
    // Enhanced retrieval
    auto results = retriever->searchRealEstate(parsedQuery);
    
    // RE-specific ranking
    auto reRanker = new RealEstateRanker();
    auto rankedResults = reRanker->rank(results, parsedQuery);
    
    return formatRealEstateResponse(rankedResults);
}
```

**Checklist:**
- [ ] Vertical detection routing
- [ ] Query parser integration
- [ ] Field extractor integration
- [ ] Ranking feature integration
- [ ] Response formatting
- [ ] End-to-end tests (50+ scenarios)
- [ ] Performance benchmarking
- [ ] Documentation

**Test Scenarios:**
```python
test_cases = [
    {
        'query': 'اجاره آپارتمان 70 متری تهرانپارس',
        'expected': {
            'vertical': 'RealEstate',
            'top_result': {
                'area_sqm': 70,
                'location': 'تهرانپارس',
                'score': '>10'
            }
        }
    },
    {
        'query': 'اجاره خانه زیر 10 میلیون تومان پیش 200 میلیون',
        'expected': {
            'all_results_within_budget': True,
            'all_deposits_under': 200000000
        }
    },
    # Add 50+ more test cases
]
```

**Files created:**
- Integration tests in existing test files
- `tests/integration/test_realestate_e2e.py` (50+ scenarios)
- `docs/realestate-integration-guide.md`

---

## 🗂️ File Structure Summary

```
search-engine-core/
├── src/
│   ├── python/
│   │   ├── extractors/
│   │   │   ├── realestate_schema_parser.py        # NEW (300 lines)
│   │   │   └── realestate_field_extractor.py      # NEW (500 lines)
│   │   ├── vertical_detector/
│   │   │   └── realestate_detector.py             # NEW (300 lines)
│   │   └── query/
│   │       └── realestate_query_parser.py         # NEW (400 lines)
│   └── cpp/
│       └── ranking/
│           ├── RealEstateFeatures.h               # NEW (100 lines)
│           ├── RealEstateFeatures.cpp             # NEW (400 lines)
│           └── RealEstateRanker.cpp               # NEW (200 lines)
├── tests/
│   ├── test_realestate_schema.py                  # NEW (60 cases)
│   ├── test_realestate_extraction.py              # NEW (100 cases)
│   ├── test_realestate_detector.py                # NEW (80 cases)
│   ├── test_realestate_query_parser.py            # NEW (100 cases)
│   ├── test_realestate_ranking.cpp                # NEW (100 cases)
│   └── integration/
│       └── test_realestate_e2e.py                 # NEW (50+ scenarios)
├── data/
│   ├── patterns/
│   │   └── realestate_patterns.json               # NEW
│   └── geo/
│       └── neighborhoods.json                     # NEW (Tehran neighborhoods)
└── docs/
    ├── realestate-schema-guide.md                 # NEW
    ├── realestate-field-extraction.md             # NEW
    ├── realestate-detector-guide.md               # NEW
    ├── realestate-ranking-guide.md                # NEW
    ├── realestate-query-parsing.md                # NEW
    └── realestate-integration-guide.md            # NEW
```

**Total New Code:** ~2,500 lines  
**Total Test Code:** ~500 test cases  
**Documentation:** 6 guides

---

## 📊 Effort Estimation

| Phase | Duration | Complexity | Priority |
|-------|----------|------------|----------|
| Schema Parser | 2 days | Low | P1 |
| Field Extraction | 3 days | Medium | P1 |
| Vertical Detection | 3 days | Low | P1 |
| Ranking Features | 5 days | High | P0 |
| Query Parsing | 3 days | Medium | P1 |
| Integration | 3 days | Medium | P0 |

**Total:** 19 days (≈ 4 weeks)

---

## 🚀 Getting Started

### When to Start
- [ ] After M0-M3 completed (Universal foundation ready)
- [ ] When business decision made (partner confirmed or POC approved)
- [ ] When team has bandwidth (2 developers for 1 month)

### Quick Start Commands
```bash
# 1. Create branch
git checkout -b feature/realestate-vertical

# 2. Create directory structure
mkdir -p src/python/extractors
mkdir -p src/python/vertical_detector
mkdir -p src/python/query
mkdir -p src/cpp/ranking
mkdir -p data/patterns
mkdir -p data/geo

# 3. Start with Phase 1, Task 1
# Copy code from this document
# Implement RealEstateSchemaParser

# 4. Test as you go
pytest tests/test_realestate_schema.py -v

# 5. Iterate through tasks
# Follow checklist above
```

### Development Workflow
```bash
# For each task:
1. Create feature branch
2. Implement according to specs above
3. Write unit tests
4. Run tests: pytest tests/test_*.py
5. Update documentation
6. Code review
7. Merge to main
8. Move to next task
```

---

## 🎯 Success Metrics

### Functional Metrics
- [ ] Vertical detection accuracy ≥90% on test set
- [ ] Field extraction precision ≥85% (price, size, location)
- [ ] Query entity extraction accuracy ≥90%
- [ ] Ranking relevance: NDCG@10 ≥0.75 on evaluation set

### Performance Metrics
- [ ] Vertical detection latency <1ms per document
- [ ] Query parsing latency <10ms
- [ ] Ranking feature computation <5ms per doc
- [ ] End-to-end latency P95 <300ms

### Business Metrics
- [ ] User satisfaction: positive feedback from partner
- [ ] Query success rate: ≥85% find what they're looking for
- [ ] CTR@1 ≥30% (first result click rate)
- [ ] Zero-result rate <5%

---

## 📝 Notes

### Important Considerations
- **Persian Language:** All patterns must handle Persian text correctly
- **Price Formats:** Support both "میلیون" and numeric formats
- **Location Data:** Maintain neighborhood/district mappings
- **Feature Flags:** Use flags to enable/disable RE vertical
- **Backward Compatibility:** Don't break existing verticals

### Future Enhancements
- [ ] Map integration (show on map)
- [ ] Price history tracking
- [ ] Comparable listings
- [ ] Alert system (notify when match found)
- [ ] Advanced filters (proximity to metro, schools, etc.)

### Monitoring
```yaml
Metrics to Track:
  - realestate_queries_per_day
  - realestate_vertical_detection_rate
  - realestate_field_extraction_success
  - realestate_ranking_latency_p95
  - realestate_zero_results_rate
  - realestate_ctr_at_1_3_10
```

---

## 🔗 References

### Related Tasks
- M0: Foundation (text processing, language detection)
- M1: Retrieval (BM25, indexing)
- M2: Structured Data (Schema.org parser, price extraction)
- M4: Vertical Detection (Book, Product, Article patterns)
- M6: Ranking (feature fusion, MMR)
- M7: Metrics (evaluation framework)

### External Resources
- [Schema.org RealEstateListing](https://schema.org/RealEstateListing)
- [Persian Text Processing](https://github.com/persiannlp)
- [Real Estate Search Best Practices](https://www.elastic.co/blog/real-estate-search)

---

## ✅ Completion Checklist

### Before Starting
- [ ] Universal search engine MVP completed (M0-M3)
- [ ] Business decision made (partner or POC)
- [ ] Requirements gathered from partner
- [ ] Team assigned (2 developers)
- [ ] Timeline approved (~4 weeks)

### During Development
- [ ] All 6 tasks completed
- [ ] All unit tests passing (500+ cases)
- [ ] Integration tests passing (50+ scenarios)
- [ ] Performance benchmarks met
- [ ] Documentation complete
- [ ] Code reviewed and approved

### Before Launch
- [ ] End-to-end testing on staging
- [ ] Partner review and approval
- [ ] Load testing completed
- [ ] Monitoring dashboards ready
- [ ] Rollback plan documented
- [ ] On-call runbook created

### Post-Launch
- [ ] Monitor metrics for 1 week
- [ ] Collect user feedback
- [ ] Fix any critical bugs
- [ ] Plan next iteration improvements

---

**Last Updated:** 2024-01-11  
**Status:** 🟡 Planning Phase  
**Next Review:** When M0-M3 completed

---

**Remember:** This is a flexible plan. Adjust based on:
- Actual partner requirements
- Team capacity
- Technical discoveries during development
- User feedback from POC

**Good luck! 🚀🏘️**

