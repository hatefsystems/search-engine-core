# Docker Health Check Best Practices

## Overview

Docker health checks are essential for monitoring container health and ensuring reliable service orchestration. This document outlines best practices for implementing effective health checks in production environments.

## 🎯 Health Check Fundamentals

### What is a Health Check?

A health check is a command that Docker runs periodically to determine if a container is working properly. It helps:

- Detect when services become unresponsive
- Enable automatic recovery in orchestration systems
- Provide visibility into service status
- Prevent routing traffic to unhealthy containers

### Health Check States

- **Starting**: Container is starting up (during `start_period`)
- **Healthy**: Container is working correctly
- **Unhealthy**: Container is not working correctly

## 🏗️ Implementation Best Practices

### 1. Choose the Right Health Check Method

#### For Node.js Services (Recommended)

```yaml
# Option 1: Using wget (Recommended for Alpine images)
healthcheck:
  test: ["CMD-SHELL", "wget --no-verbose --tries=1 --spider http://localhost:3002/health || exit 1"]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 10s

# Option 2: Using Node.js built-in modules (Alternative)
healthcheck:
  test: ["CMD", "node", "-e", "const http = require('http'); const req = http.request('http://localhost:3002/health', (res) => { process.exit(res.statusCode === 200 ? 0 : 1); }); req.on('error', () => process.exit(1)); req.end();"]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 10s
```

**wget Advantages (Recommended):**

- ✅ Available in Alpine Linux images (included by default)
- ✅ Fast and lightweight execution
- ✅ `--spider` mode only checks existence without downloading content
- ✅ `--no-verbose` reduces output noise
- ✅ Simple and reliable command
- ✅ Handles network errors gracefully
- ✅ No additional dependencies required

**Node.js Advantages (Alternative):**

- ✅ No external dependencies (uses built-in Node.js modules)
- ✅ Fast execution
- ✅ Reliable in all Node.js environments
- ✅ Works in minimal Alpine images
- ✅ Handles network errors gracefully

#### Alternative Methods

**Using wget (Recommended for Alpine images):**

```yaml
healthcheck:
  test:
    [
      "CMD-SHELL",
      "wget --no-verbose --tries=1 --spider http://localhost:3002/health || exit 1",
    ]
```

**Using curl (if available in container):**

```yaml
healthcheck:
  test: ["CMD-SHELL", "curl -f http://localhost:3002/health || exit 1"]
```

**Using dedicated health check script:**

```yaml
healthcheck:
  test: ["CMD", "node", "health-check.js"]
```

#### Tool Availability in Common Images

| Image Type         | curl                | wget                | Node.js      |
| ------------------ | ------------------- | ------------------- | ------------ |
| **Alpine Linux**   | ❌ Not included     | ✅ Included         | ✅ Available |
| **Ubuntu/Debian**  | ✅ Usually included | ✅ Usually included | ✅ Available |
| **Node.js Alpine** | ❌ Not included     | ✅ Included         | ✅ Available |
| **Node.js Slim**   | ❌ Not included     | ✅ Usually included | ✅ Available |

### 2. Health Check Timing Configuration

#### Recommended Settings

```yaml
healthcheck:
  interval: 30s # Check every 30 seconds
  timeout: 10s # Health check must complete within 10 seconds
  retries: 3 # Mark unhealthy after 3 consecutive failures
  start_period: 10s # Allow 10 seconds for initial startup
```

#### Timing Guidelines

- **`interval`**: 30s for most services, 60s for heavy services
- **`timeout`**: Should be less than `interval`, typically 10-15s
- **`retries`**: 3-5 failures before marking unhealthy
- **`start_period`**: Allow sufficient time for service initialization

### 3. Health Endpoint Design

#### Best Practice Health Endpoint

```javascript
app.get("/health", (req, res) => {
  res.json({
    status: "healthy",
    service: "js-minifier-enhanced",
    timestamp: new Date().toISOString(),
    version: process.env.APP_VERSION || "1.0.0",
    uptime: process.uptime(),
    memory: process.memoryUsage(),
    supported_methods: [
      "JSON payload",
      "Raw text",
      "File upload",
      "URL fetch",
      "Streaming",
    ],
  });
});
```

#### Health Check Response Requirements

- ✅ **Fast**: Should respond in <1 second
- ✅ **Lightweight**: Minimal processing and data
- ✅ **Informative**: Include service status and key metrics
- ✅ **Consistent**: Always return same structure
- ✅ **HTTP 200**: Only return 200 for healthy state

### 4. Service-Specific Health Checks

#### Database Services (MongoDB)

```yaml
healthcheck:
  test: ["CMD", "mongosh", "--eval", "db.adminCommand('ping')"]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 40s
```

#### Cache Services (Redis)

```yaml
healthcheck:
  test: ["CMD", "redis-cli", "ping"]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 10s
```

#### Web Services (Nginx)

```yaml
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost/health"]
  interval: 30s
  timeout: 10s
  retries: 3
  start_period: 10s
```

## 🔧 Production Implementation

### Complete Example: JS Minifier Service

```yaml
js-minifier:
  image: ghcr.io/hatefsystems/search-engine-core/js-minifier:latest
  container_name: js-minifier
  restart: unless-stopped
  environment:
    - NODE_ENV=production
    - PORT=3002
    - MAX_FILE_SIZE=52428800
    - MAX_CONCURRENT_REQUESTS=50
    - CACHE_ENABLED=true
    - CACHE_TTL=3600
  ports:
    - "3002:3002"
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
  logging:
    driver: json-file
    options:
      max-size: "10m"
      max-file: "3"
  networks:
    - search-network
```

### Dockerfile Integration

```dockerfile
# Health check in Dockerfile (using wget since curl is not available in Alpine)
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD wget --no-verbose --tries=1 --spider http://localhost:3002/health || exit 1
```

## 📊 Monitoring and Troubleshooting

### Check Health Status

```bash
# View container health status
docker ps

# Check health check logs
docker inspect --format='{{json .State.Health}}' container_name

# View detailed health check history
docker inspect container_name | jq '.[0].State.Health'
```

### Common Issues and Solutions

#### Issue: Health Check Always Fails

**Symptoms:** Container shows as unhealthy despite service working
**Solutions:**

1. Test health check command manually: `docker exec container_name wget --no-verbose --tries=1 --spider http://localhost:port/health`
2. Verify health endpoint is accessible: `curl http://localhost:port/health`
3. Check if required tools are available in container: `docker exec container_name which wget`
4. Increase `start_period` if service needs more time to initialize

#### Issue: Health Check Times Out

**Symptoms:** Health check takes too long to complete
**Solutions:**

1. Increase `timeout` value
2. Optimize health endpoint performance
3. Use lighter health check method
4. Check for resource constraints

#### Issue: Intermittent Health Check Failures

**Symptoms:** Container alternates between healthy and unhealthy
**Solutions:**

1. Increase `retries` value
2. Check for network connectivity issues
3. Monitor service resource usage
4. Review health endpoint reliability

## 🚀 Advanced Patterns

### Liveness vs Readiness Probes

```yaml
# Liveness probe (is service alive?)
healthcheck:
  test:
    [
      "CMD-SHELL",
      "wget --no-verbose --tries=1 --spider http://localhost:3002/health || exit 1",
    ]
  interval: 30s
# Readiness probe (is service ready to serve traffic?)
# Can be implemented with custom logic or separate endpoint
```

### Graceful Degradation

```javascript
app.get("/health", (req, res) => {
  const checks = {
    database: checkDatabaseConnection(),
    cache: checkCacheConnection(),
    external_api: checkExternalAPI(),
  };

  const isHealthy = Object.values(checks).every(
    (check) => check.status === "ok",
  );
  const statusCode = isHealthy ? 200 : 503;

  res.status(statusCode).json({
    status: isHealthy ? "healthy" : "degraded",
    checks,
    timestamp: new Date().toISOString(),
  });
});
```

### Health Check with Authentication

```yaml
healthcheck:
  test:
    [
      "CMD",
      "node",
      "-e",
      "const http = require('http'); const options = { hostname: 'localhost', port: 3002, path: '/health', headers: { 'Authorization': 'Bearer health-check-token' } }; const req = http.request(options, (res) => { process.exit(res.statusCode === 200 ? 0 : 1); }); req.on('error', () => process.exit(1)); req.end();",
    ]
```

## 📋 Checklist for Production Health Checks

- [ ] Health check command is fast and reliable
- [ ] Health endpoint responds quickly (<1 second)
- [ ] Health check doesn't require external dependencies
- [ ] Appropriate timing configuration (interval, timeout, retries)
- [ ] Sufficient start period for service initialization
- [ ] Health endpoint returns meaningful status information
- [ ] Health check handles network errors gracefully
- [ ] Monitoring and alerting configured for health status
- [ ] Documentation updated with health check details
- [ ] Tested in staging environment before production

## 🔗 Related Documentation

- [Docker Health Check Documentation](https://docs.docker.com/engine/reference/builder/#healthcheck)
- [Docker Compose Health Check](https://docs.docker.com/compose/compose-file/compose-file-v3/#healthcheck)
- [Kubernetes Health Checks](https://kubernetes.io/docs/tasks/configure-pod-container/configure-liveness-readiness-startup-probes/)

---

**Last Updated:** 2025-08-23  
**Version:** 1.0.0  
**Author:** Search Engine Core Team
