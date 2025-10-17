# Production JavaScript Minification Setup

This document provides comprehensive guidance for deploying the JavaScript minification service in production environments using the pre-built Docker images from GitHub Container Registry.

## 🚀 Quick Production Deployment

### 1. Environment Configuration

Create a `.env` file with the following configuration:

```bash
# MongoDB Configuration
MONGO_INITDB_ROOT_USERNAME=admin
MONGO_INITDB_ROOT_PASSWORD=your_secure_password_here
MONGODB_URI=mongodb://admin:your_secure_password_here@mongodb:27017

# JavaScript Minification Configuration
MINIFY_JS=true
MINIFY_JS_LEVEL=advanced
JS_CACHE_ENABLED=true
JS_CACHE_TYPE=redis
JS_CACHE_TTL=3600
JS_CACHE_REDIS_DB=1

# Optional Configuration
PORT=3000
SEARCH_REDIS_URI=tcp://redis:6379
SEARCH_REDIS_POOL_SIZE=8
SEARCH_INDEX_NAME=search_index
```

### 2. Deploy with Docker Compose

```bash
# Login to GitHub Container Registry (if using private images)
docker login ghcr.io -u your_username -p your_token

# Pull latest images and start services
docker compose -f docker/docker-compose.prod.yml pull
docker compose -f docker/docker-compose.prod.yml up -d

# Check service status
docker compose -f docker/docker-compose.prod.yml ps

# View logs
docker compose -f docker/docker-compose.prod.yml logs -f js-minifier
```

## 🏗️ Production Architecture

### Service Overview

The production setup includes the following services:

- **search-engine-core**: Main C++ search engine application
- **js-minifier**: JavaScript minification microservice (Node.js + Terser)
- **mongodb**: Document database with persistent storage
- **redis**: Cache and search index with persistent storage
- **browserless**: Headless Chrome for SPA rendering

### JS Minifier Service Details

**Image**: `ghcr.io/hatefsystems/search-engine-core/js-minifier:latest`

**Configuration**:

- **Port**: 3002 (internal Docker network)
- **Health Check**: `/health` endpoint (using `wget` command)
- **Max File Size**: 50MB
- **Concurrent Requests**: 50
- **Cache TTL**: 3600 seconds (1 hour)

**Environment Variables**:

```bash
NODE_ENV=production
PORT=3002
MAX_FILE_SIZE=52428800
MAX_CONCURRENT_REQUESTS=50
CACHE_ENABLED=true
CACHE_TTL=3600
```

## 📊 Performance Optimization

### Caching Strategy

The production setup uses Redis for high-performance caching:

- **Cache Type**: Redis (shared across services)
- **Cache TTL**: 3600 seconds (configurable)
- **Cache Database**: Redis DB 1 (separate from search index)
- **Performance**: 99.6% faster cached responses

### Minification Levels

| Level          | Description              | Use Case                     |
| -------------- | ------------------------ | ---------------------------- |
| **none**       | No minification          | Development/debugging        |
| **basic**      | Basic compression        | Simple optimization          |
| **advanced**   | Full Terser optimization | **Production (recommended)** |
| **aggressive** | Maximum compression      | High-traffic sites           |

## 🔧 Monitoring & Health Checks

### Service Health Monitoring

```bash
# Check JS minifier health
curl http://localhost:3002/health

# Expected response:
{
  "status": "healthy",
  "service": "js-minifier-enhanced",
  "timestamp": "2025-08-23T08:52:56.750Z",
  "supported_methods": ["JSON payload", "Raw text", "File upload", "URL fetch", "Streaming"]
}
```

#### Docker Health Check Configuration

The JS minifier service includes a robust health check that uses `wget` (since `curl` is not available in Alpine images):

```yaml
healthcheck:
  test:
    [
      "CMD-SHELL",
      "wget --no-verbose --tries=1 --spider http://localhost:3002/health || exit 1",
    ]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 10s
```

**Benefits of this approach:**

- ✅ Available in Alpine Linux images (included by default)
- ✅ Fast execution (<1 second)
- ✅ `--spider` mode only checks existence without downloading content
- ✅ `--no-verbose` reduces output noise
- ✅ Simple and reliable command
- ✅ Handles network errors gracefully

### Log Monitoring

```bash
# View JS minifier logs
docker compose -f docker/docker-compose.prod.yml logs -f js-minifier

# View all service logs
docker compose -f docker/docker-compose.prod.yml logs -f
```

### Performance Metrics

Monitor these key metrics:

- **Response Time**: Target <5ms for cached requests
- **Cache Hit Rate**: Target >90%
- **Error Rate**: Target <1%
- **Memory Usage**: Monitor for leaks
- **CPU Usage**: Monitor during peak loads

## 🔒 Security Considerations

### Network Security

- **Internal Communication**: Services communicate via Docker network
- **No External Exposure**: JS minifier port not exposed externally
- **Health Checks**: Internal health monitoring only

### Input Validation

- **File Size Limits**: 50MB maximum file size
- **Content Validation**: JavaScript files only
- **Rate Limiting**: 50 concurrent requests maximum

### Container Security

- **Non-root User**: Service runs as non-root user
- **Resource Limits**: Memory and CPU limits configured
- **Regular Updates**: Pull latest images for security patches

## 🚨 Troubleshooting

### Common Issues

**1. Service Not Starting**

```bash
# Check if image exists
docker images | grep js-minifier

# Pull latest image
docker pull ghcr.io/hatefsystems/search-engine-core/js-minifier:latest

# Check logs for errors
docker compose -f docker/docker-compose.prod.yml logs js-minifier
```

**2. Health Check Failing**

```bash
# Check if service is responding
curl -f http://localhost:3002/health

# Test health check command manually
docker exec js-minifier wget --no-verbose --tries=1 --spider http://localhost:3002/health

# Check container status
docker compose -f docker/docker-compose.prod.yml ps js-minifier

# Restart service
docker compose -f docker/docker-compose.prod.yml restart js-minifier
```

**3. Cache Issues**

```bash
# Check Redis connectivity
docker compose -f docker/docker-compose.prod.yml exec redis redis-cli ping

# Check Redis database
docker compose -f docker/docker-compose.prod.yml exec redis redis-cli -n 1 keys "*"
```

### Debug Mode

Enable debug logging by setting environment variables:

```bash
# Add to .env file
JS_MINIFIER_DEBUG=true
TERSER_DEBUG_DIR=/tmp/terser-logs
```

## 📈 Scaling Considerations

### Horizontal Scaling

For high-traffic deployments:

```bash
# Scale JS minifier instances
docker compose -f docker/docker-compose.prod.yml up -d --scale js-minifier=3

# Scale browserless instances
docker compose -f docker/docker-compose.prod.yml up -d --scale browserless=3
```

### Load Balancing

When scaling multiple JS minifier instances:

- **Round-robin**: Default Docker load balancing
- **Health-based**: Unhealthy instances automatically removed
- **Session affinity**: Not required for stateless minification

### Resource Allocation

Recommended resource limits:

```yaml
# Add to docker-compose.prod.yml
js-minifier:
  deploy:
    resources:
      limits:
        memory: 1G
        cpus: "0.5"
      reservations:
        memory: 512M
        cpus: "0.25"
```

## 🔄 Updates & Maintenance

### Regular Updates

```bash
# Pull latest images
docker compose -f docker/docker-compose.prod.yml pull

# Update services with zero downtime
docker compose -f docker/docker-compose.prod.yml up -d --no-deps js-minifier
```

### Backup Strategy

- **Configuration**: Backup `.env` files
- **Data**: MongoDB and Redis data volumes
- **Logs**: Rotated log files (10MB max, 3 files)

### Monitoring Alerts

Set up alerts for:

- Service unavailability
- Response times > 5 seconds
- Memory usage > 80%
- Error rates > 5%
- Cache hit rate < 80%

## 📚 Additional Resources

- **[JS Minification Guide](./guides/README_JS_MINIFICATION.md)** - Detailed minification features
- **[Caching Best Practices](./guides/JS_CACHING_BEST_PRACTICES.md)** - Production caching strategies
- **[Performance Optimizations](./PERFORMANCE_OPTIMIZATIONS_SUMMARY.md)** - Complete performance guide

---

**Last Updated**: January 2024  
**Version**: 1.0  
**Maintainer**: Search Engine Core Team
