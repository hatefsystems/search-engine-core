# Crawler Scheduler Service

Production-ready Celery + Flower scheduler for automated crawler task management with progressive warm-up rate limiting.

## Features

✅ **Progressive Warm-up Schedule**: Start with 50 requests/day, gradually scale to 800  
✅ **Time Window Control**: Process only between 10:00-12:00 (configurable)  
✅ **Jitter/Randomization**: Adds ±30-60 seconds delay to avoid exact timing  
✅ **Duplicate Prevention**: MongoDB tracking ensures each file processed once  
✅ **Automatic File Management**: Moves files to processed/failed folders  
✅ **Beautiful Web UI**: Flower dashboard for monitoring (http://localhost:5555)  
✅ **Production Ready**: Docker containerized, Redis-backed, MongoDB tracking  

## Architecture

```
┌─────────────────┐      ┌──────────────┐      ┌──────────────┐
│  Pending Files  │─────>│ Celery Worker│─────>│  Core API    │
│  (JSON files)   │      │  + Beat      │      │ /api/v2/...  │
└─────────────────┘      └──────────────┘      └──────────────┘
                               │                       │
                               ▼                       ▼
                         ┌──────────┐           ┌──────────┐
                         │  Redis   │           │ MongoDB  │
                         │ (Queue)  │           │(Tracking)│
                         └──────────┘           └──────────┘
                               │
                               ▼
                         ┌──────────┐
                         │  Flower  │ (Web UI)
                         │  :5555   │
                         └──────────┘
```

## Quick Start

### 1. Build and Start Services

```bash
cd crawler-scheduler

# Build the Docker image
docker build -t crawler-scheduler:latest .

# Start services (standalone mode)
docker-compose up -d

# Or integrate with main docker-compose.yml (recommended)
```

### 2. Add JSON Files to Process

Place your JSON files in `data/pending/` directory:

```bash
# Example: Copy your domain files
cp /path/to/your/domains/*.json ./data/pending/
```

### 3. Access Flower Dashboard

Open your browser: **http://localhost:5555**

- Username: `admin`
- Password: `admin123` (change in production!)

### 4. Monitor Processing

In Flower dashboard you'll see:

- **Tasks**: Real-time task execution status
- **Workers**: Worker health and performance
- **Monitor**: Live task stream
- **Scheduler**: View scheduled tasks and next run times

## Configuration

### Environment Variables

Edit `docker-compose.yml` or create `.env` file:

```bash
# Timezone Configuration
SCHEDULER_TIMEZONE=America/New_York  # Optional: Override timezone (auto-detects if not set)
# Or use TZ environment variable:
# TZ=America/New_York

# Warm-up Configuration
WARMUP_ENABLED=true
WARMUP_SCHEDULE=50,100,200,400,800  # Day 1: 50, Day 2: 100, etc.
WARMUP_START_HOUR=10  # Start at 10:00 AM (in configured timezone)
WARMUP_END_HOUR=12    # End at 12:00 PM (in configured timezone)

# Jitter Configuration
JITTER_MIN_SECONDS=30  # Minimum random delay
JITTER_MAX_SECONDS=60  # Maximum random delay

# Task Configuration
TASK_INTERVAL_SECONDS=60  # Check every 60 seconds
MAX_RETRIES=3
RETRY_DELAY_SECONDS=300

# API Configuration
API_BASE_URL=http://core:3000
```

### Timezone Configuration

The scheduler automatically detects your system timezone. You can override it using:

**Option 1: SCHEDULER_TIMEZONE environment variable**
```bash
SCHEDULER_TIMEZONE=Europe/London
```

**Option 2: TZ system environment variable**
```bash
TZ=Asia/Tokyo
```

**Timezone Detection Priority:**
1. `SCHEDULER_TIMEZONE` environment variable (highest priority)
2. `TZ` environment variable
3. System timezone from `/etc/timezone`
4. System timezone from `/etc/localtime` symlink
5. Default to `UTC` if detection fails

**Important:** All time-based settings (`WARMUP_START_HOUR`, `WARMUP_END_HOUR`) use the configured timezone. For example, if you set `SCHEDULER_TIMEZONE=America/New_York` and `WARMUP_START_HOUR=10`, the scheduler will start processing at 10:00 AM New York time.

### Warm-up Schedule Explained

The scheduler implements progressive rate limiting to safely ramp up crawler activity:

| Day | Limit | Duration | Description |
|-----|-------|----------|-------------|
| 1   | 50    | 2 hours  | Initial warm-up (1 request every 2.4 minutes) |
| 2   | 100   | 2 hours  | Moderate load (1 request every 1.2 minutes) |
| 3   | 200   | 2 hours  | Increased load (1 request every 36 seconds) |
| 4   | 400   | 2 hours  | High load (1 request every 18 seconds) |
| 5+  | 800   | 2 hours  | Maximum throughput (1 request every 9 seconds) |

**Note**: Days are calculated from first processed file, not calendar days.

### Jitter Explained

Random delays (30-60 seconds) are added before each API call to:

- Avoid hitting API at exact minute boundaries
- Distribute load more naturally
- Prevent thundering herd problems
- Make crawling pattern look more organic

## File Processing Flow

```
1. File placed in data/pending/
   ├─> JSON parsed and validated
   ├─> Check if already processed (MongoDB)
   ├─> Check rate limiter (can we process now?)
   │
2. Rate Limiter Checks
   ├─> In time window? (10:00-12:00)
   ├─> Under daily limit? (50/100/200/400/800)
   │
3. Processing
   ├─> Mark as "processing" in MongoDB
   ├─> Apply jitter (random delay)
   ├─> Call API: POST /api/v2/website-profile
   │
4. Result
   ├─> Success: Move to data/processed/
   │   └─> Mark as "processed" in MongoDB
   │
   └─> Failure: Move to data/failed/
       └─> Mark as "failed" in MongoDB
```

## JSON File Format

Place files in `data/pending/` with this format:

```json
{
  "business_name": "فروشگاه اینترنتی 6لیک",
  "website_url": "www.irangan.com",
  "owner_name": "وحید توکلی زاده",
  "grant_date": {
    "persian": "1404/06/05",
    "gregorian": "2025-08-27"
  },
  "expiry_date": {
    "persian": "1406/06/05",
    "gregorian": "2027-08-27"
  },
  "address": "استان : خراسان رضوی...",
  "phone": "05138538777",
  "email": "hatef.rostamkhani@gmail.com",
  "location": {
    "latitude": 36.29208870822794,
    "longitude": 59.59234356880189
  },
  "business_experience": "",
  "business_hours": "10-20",
  "business_services": [...],
  "extraction_timestamp": "2025-09-05T19:32:20.028672",
  "domain_info": {...}
}
```

## MongoDB Collections

The scheduler creates a collection: `crawler_scheduler_tracking`

### Document Schema

```javascript
{
  _id: ObjectId("..."),
  filename: "domain_123.json",  // Unique index
  status: "processed",  // processing | processed | failed
  file_data: { ... },  // Original JSON content
  started_at: ISODate("..."),
  processed_at: ISODate("..."),
  attempts: 1,
  api_response: { ... },  // Response from API
  error_message: null
}
```

## Flower Web UI Features

### Dashboard View
- Total tasks processed
- Success/failure rates
- Active workers
- Task timeline graphs

### Tasks View
- Click any task to see:
  - Arguments and result
  - Execution time
  - Traceback (if failed)
  - Worker that executed it

### Workers View
- Worker status (active/offline)
- CPU/Memory usage
- Processed task count
- Current task

### Monitor View
- Real-time task stream
- Live success/failure updates
- Task distribution across workers

### Scheduler View (Beat)
- All scheduled tasks
- Next run time
- Schedule type (interval/cron)
- Last run result

## Manual Operations via Flower

You can manually trigger tasks from Flower UI:

1. **Get Status**: `app.tasks.get_scheduler_status`
2. **Process Single File**: `app.tasks.process_single_file` with file path
3. **Reset Schedule**: `app.tasks.reset_warmup_schedule` (clears history)

## Integration with Main Project

### Option 1: Standalone (Current Setup)

Use separate `docker-compose.yml` in this directory.

### Option 2: Integrated (Recommended)

Add to main `docker-compose.yml`:

```yaml
services:
  # Add these services
  crawler-scheduler:
    build: ./crawler-scheduler
    container_name: crawler-scheduler-worker
    command: celery -A app.celery_app worker --beat --loglevel=info
    volumes:
      - ./crawler-scheduler/data:/app/data
    environment:
      - CELERY_BROKER_URL=redis://redis:6379/1
      - MONGODB_URI=mongodb://admin:password123@mongodb_test:27017
      - API_BASE_URL=http://core:3000
      # ... other config
    networks:
      - search-engine-network
    depends_on:
      - redis
      - mongodb_test
      - core

  crawler-flower:
    build: ./crawler-scheduler
    container_name: crawler-scheduler-flower
    command: celery -A app.celery_app flower --port=5555
    ports:
      - "5555:5555"
    environment:
      - CELERY_BROKER_URL=redis://redis:6379/1
    networks:
      - search-engine-network
    depends_on:
      - crawler-scheduler
```

## Monitoring and Debugging

### View Logs

```bash
# Worker logs
docker logs -f crawler-scheduler-worker

# Flower logs
docker logs -f crawler-scheduler-flower
```

### Check Stats in MongoDB

```bash
docker exec mongodb_test mongosh --username admin --password password123 --eval "
use('search-engine');
db.crawler_scheduler_tracking.aggregate([
  { \$group: { 
    _id: '\$status', 
    count: { \$sum: 1 } 
  }}
]).pretty()
"
```

### Common Issues

#### No Files Being Processed

1. Check rate limiter: "Outside time window" or "Daily limit reached"
2. Check Flower dashboard for failed tasks
3. Verify files exist in `data/pending/`
4. Check MongoDB connection

#### API Calls Failing

1. Check core service is running: `docker ps | grep core`
2. Verify API endpoint: `curl http://localhost:3000/api/v2/website-profile`
3. Check network connectivity between containers
4. View error details in Flower task result

#### Files Not Moving

1. Check file permissions on `data/` directories
2. Verify volume mounts in docker-compose
3. Check worker logs for errors

## Production Recommendations

### Security

- [ ] Change Flower password in `FLOWER_BASIC_AUTH`
- [ ] Use environment secrets management (not `.env` files)
- [ ] Enable TLS for Flower dashboard
- [ ] Restrict Flower port (5555) with firewall

### Scaling

- [ ] Increase worker count: `--concurrency=4`
- [ ] Separate Beat scheduler from worker
- [ ] Use Redis Sentinel for HA
- [ ] Monitor with Prometheus/Grafana

### Monitoring

- [ ] Set up Flower alerts
- [ ] Export metrics to Prometheus
- [ ] Configure error notifications (Sentry, email)
- [ ] Monitor disk space in `data/` directories

## API Response Handling

After the scheduler calls your API, your C++ core should:

1. Process the website profile data
2. Store in database
3. Trigger crawler (if needed)
4. **Send email to domain manager** (your internal logic)

The scheduler doesn't handle email - it just calls the API and tracks results.

## Development

### Local Testing

```bash
# Install dependencies
pip install -r requirements.txt

# Run worker locally (requires Redis and MongoDB)
export CELERY_BROKER_URL=redis://localhost:6379/1
export MONGODB_URI=mongodb://localhost:27017
celery -A app.celery_app worker --beat --loglevel=debug

# Run Flower locally
celery -A app.celery_app flower
```

### Adding Custom Tasks

Edit `app/tasks.py`:

```python
@app.task(base=BaseTask)
def my_custom_task():
    # Your logic here
    return {'status': 'success'}
```

Trigger from Flower or programmatically.

## License

Part of Search Engine Core project.

## Support

For issues or questions, check:
- Flower dashboard: http://localhost:5555
- Worker logs: `docker logs crawler-scheduler-worker`
- MongoDB tracking collection for processing history

