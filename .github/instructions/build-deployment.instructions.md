---
description: Build, CMake, and Docker deployment
---

# Build and Deployment

## Local build

```bash
cd /root/search-engine-core && mkdir -p build && cd build
cmake .. && make -j4
```

## Docker

```bash
docker cp /root/search-engine-core/build/server core:/app/server
docker restart core
# or: docker compose up --build
```

## CMake

When adding new storage classes:
1. Add to `src/storage/CMakeLists.txt`
2. Create static library target
3. Link to main server executable
4. Include in install targets

## Production Deployment

1. Set appropriate environment variables
2. Enable HTTPS
3. Configure proper MongoDB authentication
4. Set up monitoring and alerting
5. Implement backup strategies
6. Use production Docker images from GHCR
