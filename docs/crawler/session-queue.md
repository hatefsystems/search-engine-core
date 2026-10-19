# Crawl session admission

`CrawlerManager` limits running sessions and holds excess requests in a bounded,
in-memory queue. A queued session has no crawler, connection, or worker thread.

## Configuration

The limits are read once when the manager is constructed. Both Compose files
forward these variables to the search engine:

| Variable                  | Default | Allowed range | Meaning                         |
| ------------------------- | ------- | ------------- | ------------------------------- |
| `MAX_CONCURRENT_SESSIONS` | 5       | 1–1024        | Reserved running/starting slots |
| `MAX_PENDING_SESSIONS`    | 100     | 0–100000      | Maximum waiting requests        |

Invalid, negative, partially numeric, and overflowing values use the defaults.
Setting the pending limit to zero disables waiting when every slot is occupied.
These limits apply to each search-engine process, not across replicas.

## API compatibility

`POST /api/crawl/add-site` still returns HTTP 200 for an accepted request, with the
existing `success`, `message`, and `data.sessionId` fields. `data.status` is the
actual session state, including `queued`. Consumers should poll
`GET /api/crawl/status?sessionId=...` rather than assume an accepted job is running.

When running and waiting capacity are both full, the API returns HTTP 429, the
existing `error.code: TOO_MANY_REQUESTS` shape, and `Retry-After: 30`. No session is
created for the rejected request.

Statuses are `queued`, `starting`, `crawling`, `stopping`, `completed`, `failed`,
`cancelled`, `timed_out`, and `not_found`. Completed/cancelled/failed records are
retained for up to 60 seconds; at most 100 terminal records are retained.

`stopPreviousSessions` includes queued sessions. Internal callers can cancel one
request with `stopCrawl(id)`. A running cancellation is cooperative: its slot stays
reserved until its crawler has stopped and joined. No public cancellation or queue
listing endpoint is added before session ownership/authentication is available.
In particular, this change does not expose a list of other users' queued URLs.

Public requests use FIFO at NORMAL priority. Trusted C++ callers may supply LOW or
HIGH; equal-priority jobs remain FIFO. This is admission priority, not preemption
of a running crawl. A continuous flow of HIGH jobs can delay lower priorities.

## Lifecycle and durability

Admission, slot reservation, cancellation, and dispatch share one manager lock.
Crawler construction, shutdown, callbacks, and joins happen outside that lock.
The manager observes crawler liveness, so waiting URL retries do not prematurely
complete a session. A slot is released only after the crawler stops and its
completion callback finishes. Callbacks execute once, only for normally completed
sessions, and may query the manager or enqueue a new request.

URL-level retries/backoff remain in `URLFrontier`. Whole-session retries are
intentionally deferred: there is no same-ID session/thread replacement and no
repeated completion email caused by such retries.

Pending jobs are not durable. A process restart loses queued jobs and in-memory
status/results; do not treat HTTP acceptance as a durable delivery guarantee.
Durable admission, per-user quotas/authorization, and cross-replica scheduling are
follow-up work for issues #14 and #13, not promises of this implementation.

## Validation

For a standalone build (Catch2 is fetched at a pinned commit if not installed):

```bash
cmake -S tests/crawler/session_scheduler -B build-sessions
cmake --build build-sessions --parallel 2
ctest --test-dir build-sessions --output-on-failure
```

`session_scheduler_tests` links the actual manager and queue to controlled runner
implementations. It requires no database, HTTP service, or Browserless. It covers
concurrent admission, overflow, cancellation, priority, failure recovery, callback
reentrancy, timeout, shutdown, and retaining slots while a crawler is stopping.
It runs in the Docker build's required short-test stage:

```bash
cmake --build build --target session_scheduler_tests
ctest --test-dir build --output-on-failure -R '^session_scheduler_tests$'
```
