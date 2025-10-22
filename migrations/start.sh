#!/bin/bash

# Use MONGODB_URI environment variable if available, otherwise build from components
if [ -n "$MONGODB_URI" ]; then
    MONGO_URI="$MONGODB_URI"
    echo "Using MONGODB_URI from environment: $MONGO_URI"
else
    # MongoDB connection parameters (can be set via environment variables)
    MONGO_HOST=${MONGO_HOST:-"localhost"}
    MONGO_PORT=${MONGO_PORT:-"27017"}
    MONGO_DB=${MONGO_DB:-"search-engine"}
    MONGO_USER=${MONGO_USER:-"admin"}
    MONGO_PASS=${MONGO_PASS:-"password123"}

    # Build MongoDB connection string
    if [ -n "$MONGO_USER" ] && [ -n "$MONGO_PASS" ]; then
        MONGO_URI="mongodb://${MONGO_USER}:${MONGO_PASS}@${MONGO_HOST}:${MONGO_PORT}/${MONGO_DB}"
    else
        MONGO_URI="mongodb://${MONGO_HOST}:${MONGO_PORT}/${MONGO_DB}"
    fi
    echo "Built MongoDB URI from components: $MONGO_URI"
fi

echo "Starting search engine core..."

# MongoDB connection test with retry logic
echo "Testing MongoDB connection..."
(
    # Retry logic with exponential backoff
    MAX_RETRIES=5
    RETRY_DELAY=2
    RETRY_COUNT=0
    
    while [ $RETRY_COUNT -lt $MAX_RETRIES ]; do
        # Extract host and port from MongoDB URI for network connectivity test
        MONGO_HOST=$(echo "$MONGO_URI" | sed -E 's|mongodb://[^@]*@?([^:/]+).*|\1|')
        MONGO_PORT=$(echo "$MONGO_URI" | sed -E 's|mongodb://[^@]*@?[^:]+:([0-9]+).*|\1|')
        
        # Test network connectivity to MongoDB
        if timeout 5 bash -c "</dev/tcp/$MONGO_HOST/$MONGO_PORT" 2>/dev/null; then
            echo "✅ MongoDB connection test successful (attempt $((RETRY_COUNT + 1)))"
            break
        else
            RETRY_COUNT=$((RETRY_COUNT + 1))
            if [ $RETRY_COUNT -lt $MAX_RETRIES ]; then
                echo "⚠️  MongoDB connection test failed (attempt $RETRY_COUNT/$MAX_RETRIES), retrying in ${RETRY_DELAY}s..."
                sleep $RETRY_DELAY
                RETRY_DELAY=$((RETRY_DELAY * 2))  # Exponential backoff
            else
                echo "⚠️  MongoDB connection test failed after $MAX_RETRIES attempts - service will connect lazily"
            fi
        fi
    done
) &

# Redis connection test with retry logic
echo "Testing Redis connection..."
# Use SEARCH_REDIS_URI if available, otherwise default to tcp://localhost:6379
if [ -n "$SEARCH_REDIS_URI" ]; then
    REDIS_URI="$SEARCH_REDIS_URI"
    echo "Using SEARCH_REDIS_URI from environment: $REDIS_URI"
else
    REDIS_URI="tcp://localhost:6379"
    echo "Using default Redis URI: $REDIS_URI"
fi

# Extract host and port from REDIS_URI (format: tcp://host:port)
REDIS_HOST=$(echo "$REDIS_URI" | sed -E 's|tcp://([^:]+):([0-9]+).*|\1|')
REDIS_PORT=$(echo "$REDIS_URI" | sed -E 's|tcp://([^:]+):([0-9]+).*|\2|')

(
    # Retry logic with exponential backoff
    MAX_RETRIES=5
    RETRY_DELAY=2
    RETRY_COUNT=0
    
    while [ $RETRY_COUNT -lt $MAX_RETRIES ]; do
        # Test network connectivity to Redis
        if timeout 5 bash -c "</dev/tcp/$REDIS_HOST/$REDIS_PORT" 2>/dev/null; then
            echo "✅ Redis connection test successful (attempt $((RETRY_COUNT + 1)))"
            break
        else
            RETRY_COUNT=$((RETRY_COUNT + 1))
            if [ $RETRY_COUNT -lt $MAX_RETRIES ]; then
                echo "⚠️  Redis connection test failed (attempt $RETRY_COUNT/$MAX_RETRIES), retrying in ${RETRY_DELAY}s..."
                sleep $RETRY_DELAY
                RETRY_DELAY=$((RETRY_DELAY * 2))  # Exponential backoff
            else
                echo "⚠️  Redis connection test failed after $MAX_RETRIES attempts - service will connect lazily"
            fi
        fi
    done
) &


# Start the server application
echo "Starting server application..."
./server &

# Keep the container running
echo "Search engine core is running. Press Ctrl+C to stop."
wait