#!/bin/bash
# Get scheduler status

set -e

echo "=================================="
echo "Crawler Scheduler Status"
echo "=================================="
echo ""

# Docker containers
echo "Docker Containers:"
docker-compose ps
echo ""

# Worker logs (last 20 lines)
echo "=================================="
echo "Recent Worker Logs:"
echo "=================================="
docker logs --tail 20 crawler-scheduler-worker 2>&1 || echo "Worker not running"
echo ""

# Pending files count
echo "=================================="
echo "File Status:"
echo "=================================="
PENDING=$(find ./data/pending -name "*.json" 2>/dev/null | wc -l)
PROCESSED=$(find ./data/processed -name "*.json" 2>/dev/null | wc -l)
FAILED=$(find ./data/failed -name "*.json" 2>/dev/null | wc -l)

echo "Pending: $PENDING files"
echo "Processed: $PROCESSED files"
echo "Failed: $FAILED files"
echo ""

echo "=================================="
echo "Access Flower Dashboard:"
echo "http://localhost:5555"
echo "=================================="

