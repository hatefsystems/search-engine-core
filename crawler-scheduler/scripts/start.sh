#!/bin/bash
# Quick start script for crawler scheduler

set -e

echo "=================================="
echo "Crawler Scheduler - Quick Start"
echo "=================================="
echo ""

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
  echo "✗ Docker is not running. Please start Docker first."
  exit 1
fi

echo "✓ Docker is running"

# Check if network exists
if ! docker network inspect search-engine-network > /dev/null 2>&1; then
  echo "Creating Docker network: search-engine-network"
  docker network create search-engine-network
fi

echo "✓ Docker network exists"

# Build the image
echo ""
echo "Building Docker image..."
docker build -t crawler-scheduler:latest .

echo "✓ Image built successfully"

# Start services
echo ""
echo "Starting services..."
docker-compose up -d

echo "✓ Services started"

# Wait for services to be healthy
echo ""
echo "Waiting for services to start (10 seconds)..."
sleep 10

# Check service status
echo ""
echo "=================================="
echo "Service Status"
echo "=================================="

docker-compose ps

echo ""
echo "=================================="
echo "Access Points"
echo "=================================="
echo "• Flower Dashboard: http://localhost:5555"
echo "  Username: admin"
echo "  Password: admin123"
echo ""
echo "• Worker Logs: docker logs -f crawler-scheduler-worker"
echo "• Flower Logs: docker logs -f crawler-scheduler-flower"
echo ""
echo "=================================="
echo "Next Steps"
echo "=================================="
echo "1. Add JSON files to: ./data/pending/"
echo "2. Open Flower dashboard to monitor"
echo "3. Files will be processed automatically"
echo ""
echo "✓ Setup complete!"

