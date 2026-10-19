# Pulse API Specification

## Overview

Pulse provides lightweight public analytics for search activity. The first
version is intentionally small: search requests enqueue best-effort analytics
events, a background worker stores them in MongoDB, and public APIs expose
relative scores instead of exact raw counts.

Pulse data is stored in a separate MongoDB database. The default database name
is `hatef_pulse`.

## Public Routes

- `GET /نبض` - primary Persian RTL Pulse page
- `GET /pulse` - optional English fallback page

Both public pages can use the same range selector as the API, for example
`/نبض?range=week`.

## Captured Search Activity

Pulse records search activity from:

- `GET /api/search`
- first-page `GET /search` requests

Pagination and follow-up result loading should not inflate Pulse activity. The
search response path only enqueues an event; MongoDB writes happen in the
background.

Captured events include:

- event timestamp
- raw query
- normalized query
- normalized query hash
- result count for internal aggregation
- latency in milliseconds
- status: `ok`, `empty`, or `error`
- language estimate
- source
- basic device information derived from the user agent
- optional anonymous user/request hashes when `PULSE_HASH_SALT` is configured
- nullable future metadata fields for `country`, `province`, and `city`

Location metadata is not used by the public UI.

## Query Normalization

Pulse query normalization is handled by `PulseQueryNormalizer` and currently
performs:

- Arabic/Persian character normalization
- Persian and Arabic digit normalization
- zero-width and half-space cleanup
- punctuation cleanup
- ASCII lowercasing
- whitespace cleanup
- public-safety filtering for sensitive-looking queries

Sensitive-looking queries such as emails, URLs, phone-like numbers, very long
tokens, and extremely short or long queries are excluded from public aggregate
responses.

## Range Parameter

All public API endpoints accept an optional `range` query parameter. Invalid
values fall back to `today`.

| Range   | Meaning                                                  | Query Trend Points |
| ------- | -------------------------------------------------------- | ------------------ |
| `today` | Current UTC day for summary/list endpoints               | 24 hourly points   |
| `week`  | Rolling last 7 days                                      | 7 points           |
| `month` | Rolling last 30 days                                     | 30 points          |
| `year`  | Rolling last 365 days                                    | 12 points          |
| `all`   | All aggregate data for summary/top/zero-result endpoints | 12 compact points  |

For `all`, rising queries use a lightweight recent comparison so the endpoint
stays fast on a resource-constrained server.

## API Routes

All public API responses avoid exact raw search counts. Scores are relative
values from `0` to `100` where appropriate.

### `GET /api/pulse/summary`

Returns summary scores for the selected range. Defaults to `today`.

```json
{
  "success": true,
  "data": {
    "range": "today",
    "enoughData": true,
    "activityScore": 72,
    "successScore": 91,
    "zeroResultOpportunityScore": 12,
    "speedScore": 96
  }
}
```

### `GET /api/pulse/top-queries`

Returns top public-safe query scores for the selected range.

Query parameters:

- `range`: `today`, `week`, `month`, `year`, or `all`
- `limit`: optional item count from `1` to `50`; defaults to `10`

Example response:

```json
{
  "success": true,
  "data": {
    "window": "week",
    "range": "week",
    "enoughData": true,
    "items": [
      {
        "query": "example",
        "score": 100,
        "rank": 1,
        "languageEstimate": "en",
        "enoughData": true
      }
    ]
  }
}
```

### `GET /api/pulse/rising`

Returns public-safe queries with stronger activity in the selected range than
the previous comparable range.

Query parameters:

- `range`: `today`, `week`, `month`, `year`, or `all`
- `limit`: optional item count from `1` to `50`; defaults to `10`

Items may include `trendScore` when a rising score is available.

### `GET /api/pulse/zero-results`

Returns public-safe no-result opportunities for the selected range.

Query parameters:

- `range`: `today`, `week`, `month`, `year`, or `all`
- `limit`: optional item count from `1` to `50`; defaults to `10`

### `GET /api/pulse/query?q=...`

Returns a selected query trend line using relative point scores.

Query parameters:

- `q`: required raw query
- `range`: `today`, `week`, `month`, `year`, or `all`

Example response:

```json
{
  "success": true,
  "data": {
    "query": "example",
    "range": "month",
    "enoughData": true,
    "points": [
      {
        "timestamp": "2026-05-01T00:00:00Z",
        "score": 42
      }
    ]
  }
}
```

## Storage

Pulse uses the `hatef_pulse` MongoDB database by default. Raw events are
short-lived, and aggregate collections are used for public API responses.

Collections:

- `search_events_raw` - short-retention raw events
- `search_query_buckets` - hourly per-query aggregates used by summary, top,
  rising, and query trend APIs
- `search_daily_stats` - daily rollups for future operational summaries
- `search_trends` - reserved for future precomputed trend intelligence
- `zero_result_queries` - daily zero-result opportunities

## Indexes

| Collection             | Index                                                                   | Purpose                                                  |
| ---------------------- | ----------------------------------------------------------------------- | -------------------------------------------------------- |
| `search_events_raw`    | `timestamp` TTL                                                         | Removes raw events after the configured retention window |
| `search_query_buckets` | `granularity`, `bucket_start`, `normalized_query_hash`, `source` unique | Prevents duplicate hourly aggregate buckets              |
| `search_query_buckets` | `granularity`, `bucket_start`, `search_count`                           | Supports public list and summary lookups                 |
| `search_daily_stats`   | `date`, `source` unique                                                 | Maintains one daily rollup per source                    |
| `search_trends`        | `window`, `updated_at`, `trend_score`                                   | Future trend-score lookup                                |
| `zero_result_queries`  | `date`, `normalized_query_hash`, `source` unique                        | Prevents duplicate daily zero-result buckets             |
| `zero_result_queries`  | `date`, `search_count`                                                  | Supports public zero-result lookups                      |

## Failure and Performance Rules

- Search responses never wait for Pulse MongoDB writes.
- Pulse failures are logged and do not fail `/api/search` or `/search`.
- The request path enqueues best-effort events only.
- If the queue is full or locked, events are dropped instead of slowing search.
- Dropped events are counted and logged periodically.
- Batch writes happen in the background.
- When `PULSE_ANALYTICS_ENABLED=false`, Pulse APIs return graceful empty data
  without opening the analytics database.
- Public APIs return scores from `0` to `100` where appropriate.
- Public APIs do not expose exact raw search counts.
- Location fields are nullable metadata only and are not used by the UI.

## UI Behavior

The public page is designed to stay lightweight:

- Persian RTL primary page at `/نبض`
- optional English fallback at `/pulse`
- no location, city, province, map, or IP-derived UI
- mobile-first layout
- minimal JavaScript
- no external charting dependency
- range buttons for `today`, `week`, `month`, `year`, and `all`
- tooltips for relative list scores and trend points
- graceful empty state when there is not enough data

## Environment Variables

| Variable                       | Default                                                                                 | Notes                                                    |
| ------------------------------ | --------------------------------------------------------------------------------------- | -------------------------------------------------------- |
| `PULSE_ANALYTICS_ENABLED`      | `true` in local Docker Compose, `false` in production Compose                           | Enables event capture and public API storage reads       |
| `PULSE_MONGODB_URI`            | local Compose uses MongoDB with `authSource=admin`; code can fall back to `MONGODB_URI` | Set explicitly in production                             |
| `PULSE_MONGODB_DATABASE`       | `hatef_pulse`                                                                           | Keep Pulse analytics separate from search storage        |
| `PULSE_RAW_RETENTION_DAYS`     | `14`                                                                                    | TTL window for `search_events_raw`                       |
| `PULSE_FLUSH_INTERVAL_SECONDS` | `5`                                                                                     | Background queue flush interval                          |
| `PULSE_MAX_QUEUE_SIZE`         | `1000`                                                                                  | Events are dropped when this queue is full               |
| `PULSE_HASH_SALT`              | empty                                                                                   | Enables anonymous user/request hashes when set           |
| `PULSE_PUBLIC_MIN_EVENTS`      | `3`                                                                                     | Minimum internal event threshold for enough-data signals |

## Local Verification

```bash
curl "http://localhost:3000/api/pulse/summary?range=week"
curl "http://localhost:3000/api/pulse/top-queries?range=month&limit=10"
curl "http://localhost:3000/api/pulse/query?q=example&range=month"
```
