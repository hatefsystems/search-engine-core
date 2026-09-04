# Search Endpoint API Specification

## Overview

The `/search` endpoint provides full-text search functionality across indexed
content.

## Endpoint

```
GET /search
```

## Request Parameters

| Parameter       | Type                     | Required | Default | Description                                        | Validation/Notes             |
| --------------- | ------------------------ | -------- | ------- | -------------------------------------------------- | ---------------------------- |
| `q`             | string                   | ✅       | —       | URL-encoded search query                           | Must be non-empty            |
| `page`          | integer (1-based)        | ❌       | 1       | Page number for pagination                         | Min: 1, Max: 1000            |
| `limit`         | integer                  | ❌       | 10      | Number of results per page                         | Min: 1, Max: 100             |
| `domain_filter` | comma-separated string[] | ❌       | —       | Optional whitelist of domains to filter results by | e.g., "example.com,test.org" |

## Query Normalization & Preprocessing

All search queries undergo symmetric text normalization before index lookup:

1. **Arabic-to-Persian Letter Unification:**
   - Arabic Yeh (`ي`, U+064A) and Alef Maksura (`ى`, U+0649) → Persian Yeh (`ی`, U+06CC)
   - Arabic Kaf (`ك`, U+0643) → Persian Kaf (`ک`, U+06A9)
   - Teh Marbuta (`ة`, U+0629) → Heh (`ه`, U+0647)
   - Hamza forms (`أ`, `إ`, `آ`) → Bare Alef (`ا`, U+0627), `ؤ` → `و`, `ئ` → `ی`
2. **Numeral Normalization:**
   - Persian/Eastern Arabic digits (`۰-۹`, U+06F0–U+06F9) → ASCII digits (`0-9`)
   - Arabic-Indic digits (`٠-٩`, U+0660–U+0669) → ASCII digits (`0-9`)
3. **Diacritic & Tatweel Stripping:**
   - Harakat (Fatha, Damma, Kasra, Tanwin, Shadda, Sukun) are stripped completely
   - Tatweel/Kashida (`ـ`, U+0640) used for text stretching is removed
4. **Whitespace & Control Characters:**
   - Zero-Width Non-Joiner (ZWNJ, `\u200c`) and ZWJ (`\u200d`) are normalized to standard delimiter space
   - Zero-width space, BOM, and soft-hyphens are stripped
   - Multiple whitespace sequences are collapsed to a single space, and leading/trailing spaces are trimmed

> **Symmetric Matching:** Because documents are normalized with the exact same rules at index time (both during live crawl and batch sync), a query for `كتاب` or `کتاب` returns the exact same matching document set.

## Response Format

### Success Response (200 OK)

```json
{
  "meta": {
    "total": 123,
    "page": 1,
    "pageSize": 10
  },
  "results": [
    {
      "url": "https://example.com/page",
      "title": "Example Page Title",
      "snippet": "This is a snippet of the content with <em>highlighted</em> search terms...",
      "score": 0.987,
      "timestamp": "2024-01-15T10:30:00Z"
    }
  ]
}
```

### Response Schema

See [search_response.schema.json](./search_response.schema.json) for the
complete JSON Schema definition.

### Response Fields

#### Meta Object

- `total` (integer): Total number of results matching the query
- `page` (integer): Current page number (1-based)
- `pageSize` (integer): Number of results returned in this page

#### Results Array

Each result object contains:

- `url` (string): Full URL of the matched document
- `title` (string): Title of the document
- `snippet` (string): Text snippet with search terms highlighted using `<em>`
  tags
- `score` (number): Relevance score between 0.0 and 1.0
- `timestamp` (string): ISO 8601 formatted timestamp of when the document was
  last indexed

## Error Responses

### 400 Bad Request

Returned when request parameters are invalid.

```json
{
  "error": {
    "code": "INVALID_REQUEST",
    "message": "Invalid request parameters",
    "details": {
      "q": "Query parameter is required",
      "page": "Page must be between 1 and 1000"
    }
  }
}
```

### 500 Internal Server Error

Returned when an unexpected server error occurs.

```json
{
  "error": {
    "code": "INTERNAL_ERROR",
    "message": "An unexpected error occurred"
  }
}
```

## Examples

### Basic Search

```
GET /search?q=javascript%20tutorial
```

### Paginated Search

```
GET /search?q=python&page=2&limit=20
```

### Domain-Filtered Search

```
GET /search?q=machine%20learning&domain_filter=arxiv.org,nature.com
```

## Implementation Notes

1. Search queries are case-insensitive
2. Results are ordered by relevance score (descending)
3. Empty result sets return `200 OK` with empty `results` array
4. Query strings should be properly URL-encoded
5. Domain filtering is applied after search, may affect pagination
