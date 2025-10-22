# Integration Guide

How to integrate the Crawler Scheduler with your main Search Engine Core project.

## Integration Methods

### Method 1: Standalone (Testing)

Keep the scheduler as a separate service with its own `docker-compose.yml`:

```bash
cd crawler-scheduler
docker-compose up -d
```

**Pros**: Easy to test and develop independently  
**Cons**: Need to manage two docker-compose files

---

### Method 2: Integrated (Production - Recommended)

Add scheduler services to your main `docker-compose.yml` in project root.

#### Step 1: Add to Main docker-compose.yml

Add these services to `/root/search-engine-core/docker-compose.yml`:

```yaml
services:
  # ... existing services (core, mongodb_test, redis, etc.) ...

  # Crawler Scheduler Worker + Beat
  crawler-scheduler:
    build: ./crawler-scheduler
    container_name: crawler-scheduler-worker
    command: celery -A app.celery_app worker --beat --loglevel=info
    volumes:
      - ./crawler-scheduler/data:/app/data
      - ./crawler-scheduler/app:/app/app  # Hot reload for development
    environment:
      # Celery Configuration
      - CELERY_BROKER_URL=redis://redis:6379/1
      - CELERY_RESULT_BACKEND=redis://redis:6379/1
      
      # MongoDB Configuration
      - MONGODB_URI=mongodb://admin:password123@mongodb_test:27017
      - MONGODB_DB=search-engine
      
      # API Configuration
      - API_BASE_URL=http://core:3000
      
      # Warm-up Configuration
      - WARMUP_ENABLED=true
      - WARMUP_SCHEDULE=50,100,200,400,800
      - WARMUP_START_HOUR=10
      - WARMUP_END_HOUR=12
      
      # Jitter Configuration
      - JITTER_MIN_SECONDS=30
      - JITTER_MAX_SECONDS=60
      
      # Task Configuration
      - TASK_INTERVAL_SECONDS=60
      - MAX_RETRIES=3
      - RETRY_DELAY_SECONDS=300
      
      # Logging
      - LOG_LEVEL=info
    networks:
      - search-engine-network
    depends_on:
      - redis
      - mongodb_test
      - core
    restart: unless-stopped

  # Flower Web UI for Monitoring
  crawler-flower:
    build: ./crawler-scheduler
    container_name: crawler-scheduler-flower
    command: celery -A app.celery_app flower --port=5555 --url_prefix=flower
    ports:
      - "5555:5555"
    environment:
      - CELERY_BROKER_URL=redis://redis:6379/1
      - CELERY_RESULT_BACKEND=redis://redis:6379/1
      - FLOWER_BASIC_AUTH=admin:admin123
    networks:
      - search-engine-network
    depends_on:
      - redis
      - crawler-scheduler
    restart: unless-stopped
```

#### Step 2: Update Redis Configuration

Make sure Redis is using database 1 for scheduler (to avoid conflicts):

```yaml
services:
  redis:
    # ... existing config ...
    # No changes needed - Redis supports multiple databases
```

#### Step 3: Start Everything Together

```bash
cd /root/search-engine-core
docker-compose up --build -d
```

Now all services start together:
- Core API (C++)
- MongoDB
- Redis
- Browserless
- **Crawler Scheduler** ← NEW
- **Flower Dashboard** ← NEW

---

## Network Configuration

Both methods require the `search-engine-network` Docker network.

### If Network Doesn't Exist

```bash
docker network create search-engine-network
```

### Verify Network

```bash
docker network inspect search-engine-network
```

---

## Environment Variables

### Option A: Add to Main `.env` File

Add scheduler config to your main `.env` file:

```bash
# Crawler Scheduler Configuration
WARMUP_ENABLED=true
WARMUP_SCHEDULE=50,100,200,400,800
WARMUP_START_HOUR=10
WARMUP_END_HOUR=12
JITTER_MIN_SECONDS=30
JITTER_MAX_SECONDS=60
TASK_INTERVAL_SECONDS=60
```

### Option B: Use Separate `.env` File

Keep `crawler-scheduler/.env` separate (for standalone mode).

---

## Testing Integration

### 1. Verify Services Are Running

```bash
docker ps | grep crawler
```

You should see:
- `crawler-scheduler-worker`
- `crawler-scheduler-flower`

### 2. Check Network Connectivity

```bash
# Test if scheduler can reach core API
docker exec crawler-scheduler-worker curl -I http://core:3000

# Test if scheduler can reach MongoDB
docker exec crawler-scheduler-worker python -c "
from pymongo import MongoClient
client = MongoClient('mongodb://admin:password123@mongodb_test:27017')
print('✓ MongoDB connection successful')
"
```

### 3. Access Flower Dashboard

Open: http://localhost:5555

- Username: `admin`
- Password: `admin123`

### 4. Add Test File

```bash
cp crawler-scheduler/data/pending/example_domain.json \
   crawler-scheduler/data/pending/test_$(date +%s).json
```

Watch processing in Flower dashboard or logs:

```bash
docker logs -f crawler-scheduler-worker
```

---

## Data Persistence

### File Storage

Files are stored in `crawler-scheduler/data/`:

```
crawler-scheduler/data/
├── pending/      ← Place JSON files here
├── processed/    ← Successfully processed files
└── failed/       ← Failed files
```

### Database Storage

Processing history stored in MongoDB:

- **Database**: `search-engine`
- **Collection**: `crawler_scheduler_tracking`

### View Processing History

```bash
docker exec mongodb_test mongosh --username admin --password password123 --eval "
use('search-engine');
db.crawler_scheduler_tracking.find().limit(5).pretty();
"
```

---

## Customizing Configuration

### Change Warm-up Schedule

Edit schedule in `docker-compose.yml`:

```yaml
environment:
  # Day 1: 10, Day 2: 25, Day 3: 50, Day 4: 100, Day 5+: 200
  - WARMUP_SCHEDULE=10,25,50,100,200
```

### Change Time Window

```yaml
environment:
  - WARMUP_START_HOUR=8   # Start at 8 AM
  - WARMUP_END_HOUR=18    # End at 6 PM
```

### Change Check Interval

```yaml
environment:
  - TASK_INTERVAL_SECONDS=30  # Check every 30 seconds
```

### Disable Warm-up (Process All Files ASAP)

```yaml
environment:
  - WARMUP_ENABLED=false  # No rate limiting
```

---

## Monitoring and Alerts

### Built-in Monitoring (Flower)

Flower provides:
- Real-time task monitoring
- Worker health checks
- Task success/failure rates
- Task execution history

Access at: http://localhost:5555

### Custom Monitoring (Prometheus + Grafana)

Flower can export Prometheus metrics:

```yaml
services:
  crawler-flower:
    command: celery -A app.celery_app flower --port=5555 --prometheus-address=0.0.0.0:9090
    ports:
      - "5555:5555"
      - "9090:9090"  # Prometheus metrics
```

### Log Aggregation

Send logs to ELK stack or similar:

```yaml
services:
  crawler-scheduler:
    logging:
      driver: "json-file"
      options:
        max-size: "10m"
        max-file: "3"
```

---

## Scaling

### Multiple Workers

To process more files in parallel:

```yaml
services:
  crawler-scheduler:
    command: celery -A app.celery_app worker --concurrency=4 --loglevel=info
    # Processes 4 files simultaneously

  # Separate Beat scheduler (recommended for production)
  crawler-beat:
    build: ./crawler-scheduler
    command: celery -A app.celery_app beat --loglevel=info
    # Only schedules tasks, doesn't process them
```

### Multiple Worker Containers

```yaml
services:
  crawler-scheduler-1:
    # ... worker config ...
  
  crawler-scheduler-2:
    # ... worker config ...
  
  crawler-beat:
    # ... beat scheduler only ...
```

---

## Production Checklist

Before deploying to production:

- [ ] Change Flower password (`FLOWER_BASIC_AUTH`)
- [ ] Enable TLS/SSL for Flower
- [ ] Set up firewall rules (restrict port 5555)
- [ ] Configure log rotation
- [ ] Set up monitoring/alerting
- [ ] Configure backup for MongoDB tracking collection
- [ ] Test failover scenarios
- [ ] Document runbook for common issues
- [ ] Set resource limits (CPU/memory)
- [ ] Enable auto-restart policies

---

## Troubleshooting

### Services Won't Start

```bash
# Check logs
docker logs crawler-scheduler-worker
docker logs crawler-scheduler-flower

# Common issues:
# 1. Redis not running
# 2. MongoDB not accessible
# 3. Network not found
# 4. Port conflict (5555)
```

### Files Not Being Processed

```bash
# Check rate limiter status
docker exec crawler-scheduler-worker python -c "
from app.rate_limiter import get_rate_limiter
limiter = get_rate_limiter()
import json
print(json.dumps(limiter.get_status_info(), indent=2, default=str))
"
```

### API Calls Failing

```bash
# Test API from scheduler container
docker exec crawler-scheduler-worker curl -X POST \
  http://core:3000/api/v2/website-profile \
  -H "Content-Type: application/json" \
  -d '{"test": "data"}'
```

### Reset Everything

```bash
# Stop and remove containers
docker-compose down

# Clear tracking database
docker exec mongodb_test mongosh --username admin --password password123 --eval "
use('search-engine');
db.crawler_scheduler_tracking.deleteMany({});
"

# Clear data directories
rm -rf crawler-scheduler/data/processed/*
rm -rf crawler-scheduler/data/failed/*

# Restart
docker-compose up --build -d
```

---

## Support

For issues specific to:

- **Scheduler Logic**: Check `crawler-scheduler/app/` code
- **Celery Issues**: Check Celery docs or Flower dashboard
- **API Integration**: Check core C++ service logs
- **Database Issues**: Check MongoDB logs

---

## Next Steps

After integration:

1. **Add your 200 domain files** to `data/pending/`
2. **Monitor in Flower** at http://localhost:5555
3. **Adjust warm-up schedule** based on actual load
4. **Set up alerts** for failed tasks
5. **Configure backup** for tracking database

Happy scheduling! 🚀

