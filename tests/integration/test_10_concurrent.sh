#!/bin/bash

# Get call count from first parameter, default to 10
CALL_COUNT=${1:-10}

echo "Starting $CALL_COUNT concurrent API requests to test server stability..."
echo "Timestamp: $(date)"

# Function to make a single request
make_request() {
    local request_id=$1
    echo "Request $request_id starting at $(date)"
    
    response=$(curl -s -w "HTTP_CODE:%{http_code},TIME:%{time_total}" \
        -X POST http://localhost:3000/api/crawl/add-site \
        -H "Content-Type: application/json" \
        -d '{"url": "https://www.example'$request_id'.com", "maxPages": 1, "maxDepth": 1}' \
        --max-time 30)
    
    echo "Request $request_id completed at $(date)"
    echo "Request $request_id response: $response"
    echo "---"
}

# Start concurrent requests
for i in $(seq 1 $CALL_COUNT); do
    make_request $i &
done

# Wait for all background jobs to complete
echo "Waiting for all requests to complete..."
wait

echo "All requests completed at $(date)"
echo "Test finished."
