# Search Result Count Mismatch (Reported Count vs Rendered Results)

## Issue Summary

**Symptom:**

When executing a search query (such as `/search?q=%D8%A2%D8%B1%D8%A7%D9%85`), the header summary claims a certain number of results were found (e.g., "6 results found"), but fewer unique items are rendered on the page (e.g., only 3 items).

**Impact:**

- Confusing user experience with pagination and result counters not matching visible cards.
- Potential pagination breakdown where pages appear to have results that cannot be browsed.
- Accumulation of duplicate/orphan documents in RediSearch index.

---

## Root Cause Analysis

### 1. Non-Deterministic Key Generation in Python (`sync.py`)

In `redis-sync-service/sync.py`, document keys were previously generated using Python's built-in `hash()` function:

```python
# ❌ BEFORE: Non-deterministic hash across process runs
def _generate_doc_key(self, url: str) -> str:
    url_hash = str(hash(url))
    return f"{self.key_prefix}{url_hash}"
```

In Python 3, `hash()` is randomized with a process-specific salt (`PYTHONHASHSEED`). Every time the container or script restarted, the same URL generated an entirely different key. Over time, multiple sync runs created duplicate keys for identical URLs, ballooning the Redis index size (e.g. 307,854 keys for 153,927 unique MongoDB documents).

### 2. Mismatch Between Raw Redis Count and Deduplicated Results

In `src/storage/RedisSearchStorage.cpp`, search retrieval executes multi-tier searches and deduplicates documents using an in-memory `seenUrls` set:

```cpp
// De-duplicating visible results
if (seenUrls.find(result.url) == seenUrls.end()) {
    response.results.push_back(result);
    seenUrls.insert(result.url);
}
```

However, `response.totalResults` was previously set directly to the raw integer returned by RediSearch:

```cpp
// ❌ BEFORE: Raw RediSearch document count including duplicate keys
response.totalResults = totalResultsFromRedis;
```

Because 3 unique URLs had 6 keys in Redis (2 copies of each), RediSearch returned `totalResults = 6`, while the de-duplication loop filtered the display list down to 3 unique documents.

### 3. Out-of-Sync Index with Persian/Arabic Normalization

When symmetric character normalization (`آ` -> `ا`) was introduced in the query pipeline, queries for `آرام` normalized to `ارام`. However, Redis still held un-normalized keys and titles because the sync service had halted full syncs due to an orphan key warning (`Redis has more documents than MongoDB`).

---

## Solution & Implementation

### 1. Deterministic SHA-256 Key Hashing

Both Python and C++ were updated to use a deterministic 16-character hex digest of SHA-256 for URL keys.

#### In `redis-sync-service/sync.py`:

```python
# ✅ AFTER: Deterministic SHA-256 hash matching C++
def _generate_doc_key(self, url: str) -> str:
    url_hash = hashlib.sha256(url.encode('utf-8')).hexdigest()[:16]
    return f"{self.key_prefix}{url_hash}"
```

#### In `src/storage/RedisSearchStorage.cpp`:

```cpp
// ✅ AFTER: Deterministic SHA-256 hash matching Python sync
std::string urlToKey(const std::string& url) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(url.data()), url.size(), hash);
    std::ostringstream oss;
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return oss.str();
}
```

### 2. Accurate `totalResults` Deduplication Logic

In `src/storage/RedisSearchStorage.cpp`:

```cpp
// ✅ AFTER: Accurately reflect unique deduplicated results count
if (totalResultsFromRedis <= 1000) {
    response.totalResults = uniqueResultsFetched;
} else {
    response.totalResults = std::max(static_cast<int64_t>(totalResultsFromRedis), static_cast<int64_t>(uniqueResultsFetched));
}
```

In `src/controllers/SearchController.cpp`:

```cpp
// ✅ AFTER: Safeguard on first page when result set is smaller than page limit
if (page == 1 && searchResults.size() < static_cast<size_t>(limit) && totalResults > static_cast<int64_t>(searchResults.size())) {
    totalResults = searchResults.size();
}
```

### 3. Automated Index Purge and Re-sync Flag

Added support for clean re-syncing via command-line argument or environment variable:

```bash
docker exec redis-sync python sync.py --clear
```

Environment variable `REDIS_AUTO_CLEAR_ORPHANS=true` can also be configured in `docker-compose.yml`.

---

## Verification & Testing

1. **Purged Duplicate Keys and Rebuilt Index:**

   ```bash
   docker exec -e BATCH_SIZE=1000 redis-sync python sync.py --clear
   ```

   Result: MongoDB count: `153,927`, Redis count: `153,927`, `In Sync: True`.

2. **Verified Search Results:**

   ```bash
   curl -s "http://127.0.0.1:3000/search?q=%D8%A2%D8%B1%D8%A7%D9%85" | grep -E "search-info|result-item"
   ```

   Result: Found **169** normalized results with matching displayed items and consistent pagination across pages 1 and 2.

3. **Verified Edge Case (Queries with < 10 results):**
   ```bash
   curl -s "http://127.0.0.1:3000/search?q=%D8%AF%D9%85%D9%86%D9%88%D8%B4%20%D9%85%DB%8C%D9%88%D9%87%20%DA%AF%D9%84"
   ```
   Result: Exactly 1 result reported and exactly 1 result item displayed.
