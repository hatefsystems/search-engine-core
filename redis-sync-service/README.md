# Redis Sync Service

Python microservice for syncing MongoDB `indexed_pages` collection to Redis for ultra-fast search.

## Features

- ✅ **Full Sync** - Sync all indexed pages from MongoDB to Redis
- ✅ **Incremental Sync** - Sync only recently modified pages  
- ✅ **Automatic Scheduling** - Continuous sync at configurable intervals
- ✅ **Batch Processing** - Efficient batch processing with progress tracking
- ✅ **Status Monitoring** - Track sync status and document counts

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `MONGODB_URI` | `mongodb://admin:password123@mongodb:27017` | MongoDB connection string |
| `MONGODB_DB` | `search-engine` | MongoDB database name |
| `REDIS_HOST` | `redis` | Redis host |
| `REDIS_PORT` | `6379` | Redis port |
| `REDIS_DB` | `0` | Redis database number |
| `SEARCH_INDEX_NAME` | `search_index` | RediSearch index name |
| `KEY_PREFIX` | `doc:` | Redis key prefix for documents |
| `BATCH_SIZE` | `100` | Batch size for processing |
| `MAX_CONTENT_SIZE` | `50000` | Maximum content size per document (bytes) |
| `SYNC_MODE` | `full` | Sync mode: `full` or `incremental` |
| `SYNC_INTERVAL_SECONDS` | `3600` | Sync interval in seconds (default: 1 hour) |
| `INCREMENTAL_WINDOW_HOURS` | `24` | Time window for incremental sync (hours) |
| `REDIS_AUTO_CLEAR_ORPHANS` | `false` | When true, automatically drops index and re-syncs if Redis document count exceeds MongoDB |

## Key Generation Architecture

To ensure consistency across process restarts and prevent duplicate/orphan documents:
- Document keys are generated using a deterministic SHA-256 hash: `doc:<sha256(url)[:16]>`.
- This matches the C++ engine implementation in `src/storage/RedisSearchStorage.cpp::urlToKey()`.
- Avoids Python's randomized built-in `hash()` which causes duplicate keys across process restarts.

## Usage

### Run with Docker Compose

Already configured in both `docker-compose.yml` (development) and `docker/docker-compose.prod.yml` (production). The service will:
1. Start automatically with the stack
2. Perform initial full sync if Redis is empty
3. Continue with scheduled syncs based on `SYNC_MODE`

### Production Deployment

The service is automatically built and published to GitHub Container Registry as part of the CI/CD pipeline:
- **Image**: `ghcr.io/hatefsystems/search-engine-core/redis-sync:latest`
- **Build Workflow**: `.github/workflows/build-redis-sync.yml`
- **Orchestration**: Included in `docker-build-orchestrator.yml`

### Manual Sync

```bash
# Full sync
docker exec redis-sync python sync.py

# Clean re-index (drop old index + documents, and rebuild from MongoDB)
docker exec redis-sync python sync.py --clear

# Check logs
docker logs -f redis-sync
```

## Performance

For 11,247 documents:
- **Full sync**: ~45-60 seconds
- **Incremental sync (24h)**: ~5-10 seconds
- **Memory usage**: ~168MB in Redis

## Search Speed Comparison

- **MongoDB**: 1,500ms (1.5 seconds)
- **Redis**: 5-10ms  
- **Speedup**: **100-300x faster!** 🚀

