#!/bin/bash
# Test script to verify API endpoint before running scheduler

set -e

API_URL="${API_BASE_URL:-http://localhost:3000}/api/v2/website-profile"

echo "=================================="
echo "API Endpoint Test"
echo "=================================="
echo "Testing: $API_URL"
echo ""

# Sample test data
TEST_DATA='{
  "business_name": "Test Store",
  "website_url": "www.test.com",
  "owner_name": "Test Owner",
  "email": "test@example.com",
  "phone": "1234567890"
}'

echo "Sending test request..."
echo ""

RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" \
  -X POST "$API_URL" \
  -H "Content-Type: application/json" \
  -d "$TEST_DATA")

HTTP_STATUS=$(echo "$RESPONSE" | grep "HTTP_STATUS:" | cut -d':' -f2)
BODY=$(echo "$RESPONSE" | sed '/HTTP_STATUS:/d')

echo "Response Status: $HTTP_STATUS"
echo "Response Body:"
echo "$BODY" | jq '.' 2>/dev/null || echo "$BODY"

if [ "$HTTP_STATUS" = "200" ]; then
  echo ""
  echo "✓ API is working correctly!"
  exit 0
else
  echo ""
  echo "✗ API returned error status: $HTTP_STATUS"
  exit 1
fi

