# Pulse API Specification

## Overview

Pulse provides lightweight public analytics for search activity. The first
version exposes relative scores only and stores analytics in a separate MongoDB
database.

## Public Routes

- `GET /نبض` - Persian RTL Pulse page
- `GET /pulse` - optional English fallback page

## API Routes

All public API responses avoid exact raw search counts. List and summary
endpoints accept an optional `range` query parameter:

- `today`
- `week`
- `month`
- `year`
- `all`

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

Returns top public-safe queries for the selected range.

### `GET /api/pulse/rising`

Returns public-safe queries with stronger activity in the selected range than
the previous comparable range.

### `GET /api/pulse/zero-results`

Returns public-safe no-result opportunities for the selected range.

### `GET /api/pulse/query?q=...`

Returns a trend line for a selected query using relative point scores and the
selected range.

## Storage

Pulse uses the `hatef_pulse` MongoDB database by default.

Collections:

- `search_events_raw` with TTL on `timestamp`
- `search_query_buckets`
- `search_daily_stats`
- `search_trends`
- `zero_result_queries`

Raw events are short-lived. Aggregated collections are used for public API
responses.

## Safety Rules

- Search responses never wait for Pulse writes.
- Pulse failures are logged and do not affect `/api/search`.
- When `PULSE_ANALYTICS_ENABLED=false`, Pulse APIs return graceful empty data
  without opening the analytics database.
- Public APIs return scores from 0 to 100 where appropriate.
- Public APIs filter sensitive-looking queries such as emails, URLs, phone-like
  numbers, and long token-like strings.
- Location fields are nullable metadata only and are not used by the UI.

## Environment Variables

- `PULSE_ANALYTICS_ENABLED=false`
- `PULSE_MONGODB_URI`
- `PULSE_MONGODB_DATABASE=hatef_pulse`
- `PULSE_RAW_RETENTION_DAYS=14`
- `PULSE_FLUSH_INTERVAL_SECONDS=5`
- `PULSE_MAX_QUEUE_SIZE=1000`
- `PULSE_HASH_SALT`
- `PULSE_PUBLIC_MIN_EVENTS=3`
