#!/bin/bash

# Website Profile API Test Script
# This script tests all endpoints of the Website Profile API

BASE_URL="http://localhost:3000"
API_BASE="/api/v2"

echo "=========================================="
echo "Website Profile API Test Script"
echo "=========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test data
TEST_URL="testwebsite.ir"

echo -e "${YELLOW}Test 1: Save Website Profile${NC}"
echo "POST ${BASE_URL}${API_BASE}/website-profile"
curl -s -X POST "${BASE_URL}${API_BASE}/website-profile" \
  -H "Content-Type: application/json" \
  -d '{
    "business_name": "فروشگاه تست",
    "website_url": "'"${TEST_URL}"'",
    "owner_name": "مالک تست",
    "grant_date": {
      "persian": "1404/06/05",
      "gregorian": "2025-08-27"
    },
    "expiry_date": {
      "persian": "1406/06/05",
      "gregorian": "2027-08-27"
    },
    "address": "آدرس تستی - تهران",
    "phone": "02112345678",
    "email": "test@example.com",
    "location": {
      "latitude": 35.6892,
      "longitude": 51.3890
    },
    "business_experience": "5 years",
    "business_hours": "9-18",
    "business_services": [
      {
        "row_number": "1",
        "service_title": "خدمات تستی",
        "permit_issuer": "ناشر مجوز",
        "permit_number": "123456",
        "validity_start_date": "2025-01-01",
        "validity_end_date": "2026-01-01",
        "status": "تایید شده"
      }
    ],
    "extraction_timestamp": "2025-10-08T12:00:00.000Z",
    "domain_info": {
      "page_number": 1,
      "row_index": 1,
      "row_number": "1",
      "province": "تهران",
      "city": "تهران",
      "domain_url": "https://example.com"
    }
  }' | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 2: Check if Profile Exists${NC}"
echo "GET ${BASE_URL}${API_BASE}/website-profile/check/${TEST_URL}"
curl -s "${BASE_URL}${API_BASE}/website-profile/check/${TEST_URL}" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 3: Get Website Profile by URL${NC}"
echo "GET ${BASE_URL}${API_BASE}/website-profile/${TEST_URL}"
curl -s "${BASE_URL}${API_BASE}/website-profile/${TEST_URL}" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 4: Get All Website Profiles${NC}"
echo "GET ${BASE_URL}${API_BASE}/website-profiles?limit=5"
curl -s "${BASE_URL}${API_BASE}/website-profiles?limit=5" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 5: Update Website Profile${NC}"
echo "PUT ${BASE_URL}${API_BASE}/website-profile/${TEST_URL}"
curl -s -X PUT "${BASE_URL}${API_BASE}/website-profile/${TEST_URL}" \
  -H "Content-Type: application/json" \
  -d '{
    "business_name": "فروشگاه تست (به‌روزرسانی شده)",
    "website_url": "'"${TEST_URL}"'",
    "owner_name": "مالک جدید",
    "grant_date": {
      "persian": "1404/06/05",
      "gregorian": "2025-08-27"
    },
    "expiry_date": {
      "persian": "1406/06/05",
      "gregorian": "2027-08-27"
    },
    "address": "آدرس جدید - تهران",
    "phone": "02198765432",
    "email": "updated@example.com",
    "location": {
      "latitude": 35.6892,
      "longitude": 51.3890
    },
    "business_experience": "7 years",
    "business_hours": "8-20",
    "business_services": [],
    "extraction_timestamp": "2025-10-08T14:00:00.000Z",
    "domain_info": {
      "page_number": 2,
      "row_index": 2,
      "row_number": "2",
      "province": "تهران",
      "city": "تهران",
      "domain_url": "https://example.com"
    }
  }' | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 6: Verify Update${NC}"
echo "GET ${BASE_URL}${API_BASE}/website-profile/${TEST_URL}"
curl -s "${BASE_URL}${API_BASE}/website-profile/${TEST_URL}" | jq '.data.business_name, .data.email'

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 7: Delete Website Profile${NC}"
echo "DELETE ${BASE_URL}${API_BASE}/website-profile/${TEST_URL}"
curl -s -X DELETE "${BASE_URL}${API_BASE}/website-profile/${TEST_URL}" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 8: Verify Deletion${NC}"
echo "GET ${BASE_URL}${API_BASE}/website-profile/${TEST_URL}"
curl -s "${BASE_URL}${API_BASE}/website-profile/${TEST_URL}" | jq .

echo ""
echo "=========================================="
echo -e "${GREEN}All tests completed!${NC}"
echo "=========================================="

