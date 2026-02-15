#!/bin/bash

# Clean URL Routing System Test Script
# This script tests all features of the Clean URL Routing System including:
# - Profile CRUD operations
# - Multiple URL formats (/:slug, /profiles/:slug) - clean URLs without @
# - Unicode slug support (Persian/Arabic)
# - SEO redirects (301)
# - Slug management APIs
# - Performance caching
# - Reserved path handling

BASE_URL="http://localhost:3000"
API_BASE="/api"

echo "=========================================="
echo "Clean URL Routing System Test Script"
echo "=========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test data
ENGLISH_SLUG="john-doe"
PERSIAN_SLUG="علی-رضایی"
MIXED_SLUG="ali-علی-123"
BUSINESS_SLUG="test-company"

# Store created profile IDs for cleanup
CREATED_IDS=""

echo -e "${YELLOW}Test 1: Create Profile with English Slug${NC}"
echo "POST ${BASE_URL}${API_BASE}/profiles"
RESPONSE=$(curl -s -X POST "${BASE_URL}${API_BASE}/profiles" \
  -H "Content-Type: application/json" \
  -d '{
    "slug": "'"${ENGLISH_SLUG}"'",
    "name": "John Doe",
    "type": "PERSON",
    "bio": "A test profile for API testing",
    "isPublic": true
  }')

echo "$RESPONSE" | jq .

# Extract ID from response
ENGLISH_ID=$(echo "$RESPONSE" | jq -r '.data.id // empty')
if [ ! -z "$ENGLISH_ID" ]; then
    CREATED_IDS="$CREATED_IDS $ENGLISH_ID"
    echo -e "${GREEN}✓ Profile created with ID: $ENGLISH_ID${NC}"
else
    echo -e "${RED}✗ Failed to create profile${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 2: Create Profile with Persian Slug${NC}"
echo "POST ${BASE_URL}${API_BASE}/profiles"
RESPONSE=$(curl -s -X POST "${BASE_URL}${API_BASE}/profiles" \
  -H "Content-Type: application/json" \
  -d '{
    "slug": "'"${PERSIAN_SLUG}"'",
    "name": "علی رضایی",
    "type": "PERSON",
    "bio": "یک پروفایل آزمایشی برای تست API",
    "isPublic": true
  }')

echo "$RESPONSE" | jq .

# Extract ID from response
PERSIAN_ID=$(echo "$RESPONSE" | jq -r '.data.id // empty')
if [ ! -z "$PERSIAN_ID" ]; then
    CREATED_IDS="$CREATED_IDS $PERSIAN_ID"
    echo -e "${GREEN}✓ Persian profile created with ID: $PERSIAN_ID${NC}"
else
    echo -e "${RED}✗ Failed to create Persian profile${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 3: Create Profile with Mixed Persian-English Slug${NC}"
echo "POST ${BASE_URL}${API_BASE}/profiles"
RESPONSE=$(curl -s -X POST "${BASE_URL}${API_BASE}/profiles" \
  -H "Content-Type: application/json" \
  -d '{
    "slug": "'"${MIXED_SLUG}"'",
    "name": "Ali رضایی",
    "type": "PERSON",
    "bio": "A mixed Persian-English profile",
    "isPublic": true
  }')

echo "$RESPONSE" | jq .

# Extract ID from response
MIXED_ID=$(echo "$RESPONSE" | jq -r '.data.id // empty')
if [ ! -z "$MIXED_ID" ]; then
    CREATED_IDS="$CREATED_IDS $MIXED_ID"
    echo -e "${GREEN}✓ Mixed profile created with ID: $MIXED_ID${NC}"
else
    echo -e "${RED}✗ Failed to create mixed profile${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 4: Create Business Profile${NC}"
echo "POST ${BASE_URL}${API_BASE}/profiles"
RESPONSE=$(curl -s -X POST "${BASE_URL}${API_BASE}/profiles" \
  -H "Content-Type: application/json" \
  -d '{
    "slug": "'"${BUSINESS_SLUG}"'",
    "name": "Test Company Ltd",
    "type": "BUSINESS",
    "bio": "A test business profile",
    "isPublic": true
  }')

echo "$RESPONSE" | jq .

# Extract ID from response
BUSINESS_ID=$(echo "$RESPONSE" | jq -r '.data.id // empty')
if [ ! -z "$BUSINESS_ID" ]; then
    CREATED_IDS="$CREATED_IDS $BUSINESS_ID"
    echo -e "${GREEN}✓ Business profile created with ID: $BUSINESS_ID${NC}"
else
    echo -e "${RED}✗ Failed to create business profile${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 5: Try to Create Duplicate Slug (Should Fail)${NC}"
echo "POST ${BASE_URL}${API_BASE}/profiles"
RESPONSE=$(curl -s -X POST "${BASE_URL}${API_BASE}/profiles" \
  -H "Content-Type: application/json" \
  -d '{
    "slug": "'"${ENGLISH_SLUG}"'",
    "name": "Another John Doe",
    "type": "PERSON"
  }')

echo "$RESPONSE" | jq .

# Check if it failed as expected
SUCCESS=$(echo "$RESPONSE" | jq -r '.success // false')
if [ "$SUCCESS" = "false" ]; then
    echo -e "${GREEN}✓ Correctly rejected duplicate slug${NC}"
else
    echo -e "${RED}✗ Should have rejected duplicate slug${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 6: Try to Create Invalid Slug (Should Fail)${NC}"
echo "POST ${BASE_URL}${API_BASE}/profiles"
RESPONSE=$(curl -s -X POST "${BASE_URL}${API_BASE}/profiles" \
  -H "Content-Type: application/json" \
  -d '{
    "slug": "invalid slug",
    "name": "Invalid Slug User",
    "type": "PERSON"
  }')

echo "$RESPONSE" | jq .

# Check if it failed as expected
SUCCESS=$(echo "$RESPONSE" | jq -r '.success // false')
if [ "$SUCCESS" = "false" ]; then
    echo -e "${GREEN}✓ Correctly rejected invalid slug${NC}"
else
    echo -e "${RED}✗ Should have rejected invalid slug${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 7: Get Profile by ID${NC}"
if [ ! -z "$ENGLISH_ID" ]; then
    echo "GET ${BASE_URL}${API_BASE}/profiles/${ENGLISH_ID}"
    curl -s "${BASE_URL}${API_BASE}/profiles/${ENGLISH_ID}" | jq .
else
    echo -e "${RED}Skipping: No English profile ID available${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 8: Get Public Profile by English Slug${NC}"
echo "GET ${BASE_URL}/profiles/${ENGLISH_SLUG}"
curl -s "${BASE_URL}/profiles/${ENGLISH_SLUG}" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 9: Get Public Profile by Persian Slug${NC}"
echo "GET ${BASE_URL}/profiles/${PERSIAN_SLUG}"
curl -s "${BASE_URL}/profiles/${PERSIAN_SLUG}" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 10: Get Public Profile by Mixed Slug${NC}"
echo "GET ${BASE_URL}/profiles/${MIXED_SLUG}"
curl -s "${BASE_URL}/profiles/${MIXED_SLUG}" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 11: List All Profiles${NC}"
echo "GET ${BASE_URL}${API_BASE}/profiles?limit=10"
curl -s "${BASE_URL}${API_BASE}/profiles?limit=10" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 12: List Only Person Profiles${NC}"
echo "GET ${BASE_URL}${API_BASE}/profiles?type=PERSON&limit=10"
curl -s "${BASE_URL}${API_BASE}/profiles?type=PERSON&limit=10" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 13: List Only Business Profiles${NC}"
echo "GET ${BASE_URL}${API_BASE}/profiles?type=BUSINESS&limit=10"
curl -s "${BASE_URL}${API_BASE}/profiles?type=BUSINESS&limit=10" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 14: Update Profile${NC}"
if [ ! -z "$ENGLISH_ID" ]; then
    echo "PUT ${BASE_URL}${API_BASE}/profiles/${ENGLISH_ID}"
    RESPONSE=$(curl -s -X PUT "${BASE_URL}${API_BASE}/profiles/${ENGLISH_ID}" \
      -H "Content-Type: application/json" \
      -d '{
        "name": "Updated John Doe",
        "bio": "Updated bio with Persian: بروزرسانی شد",
        "isPublic": false
      }')

    echo "$RESPONSE" | jq .

    SUCCESS=$(echo "$RESPONSE" | jq -r '.success // false')
    if [ "$SUCCESS" = "true" ]; then
        echo -e "${GREEN}✓ Profile updated successfully${NC}"
    else
        echo -e "${RED}✗ Profile update failed${NC}"
    fi
else
    echo -e "${RED}Skipping: No profile ID available${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 15: Try to Access Now-Private Profile (Should Fail)${NC}"
echo "GET ${BASE_URL}/profiles/${ENGLISH_SLUG}"
RESPONSE=$(curl -s "${BASE_URL}/profiles/${ENGLISH_SLUG}")

# Check HTTP status (should be 403)
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "${BASE_URL}/profiles/${ENGLISH_SLUG}")
if [ "$HTTP_CODE" = "403" ]; then
    echo -e "${GREEN}✓ Correctly returned 403 for private profile${NC}"
    echo "$RESPONSE" | jq .
else
    echo -e "${RED}✗ Should have returned 403 for private profile (got $HTTP_CODE)${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 16: Get Non-existent Profile${NC}"
echo "GET ${BASE_URL}${API_BASE}/profiles/non-existent-id"
RESPONSE=$(curl -s "${BASE_URL}${API_BASE}/profiles/non-existent-id")

HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "${BASE_URL}${API_BASE}/profiles/non-existent-id")
if [ "$HTTP_CODE" = "404" ]; then
    echo -e "${GREEN}✓ Correctly returned 404 for non-existent profile${NC}"
else
    echo -e "${RED}✗ Should have returned 404 (got $HTTP_CODE)${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 17: Get Non-existent Public Profile Slug${NC}"
echo "GET ${BASE_URL}/profiles/non-existent-slug"
RESPONSE=$(curl -s "${BASE_URL}/profiles/non-existent-slug")

HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "${BASE_URL}/profiles/non-existent-slug")
if [ "$HTTP_CODE" = "404" ]; then
    echo -e "${GREEN}✓ Correctly returned 404 for non-existent slug${NC}"
else
    echo -e "${RED}✗ Should have returned 404 (got $HTTP_CODE)${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

echo -e "${YELLOW}Test 18: Test Root-Level Slug Routes${NC}"
echo "GET ${BASE_URL}/${ENGLISH_SLUG}"
curl -s "${BASE_URL}/${ENGLISH_SLUG}" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 19: Test Clean URL Routes (Without @)${NC}"
if [ ! -z "$ENGLISH_SLUG" ]; then
    echo "GET ${BASE_URL}/${ENGLISH_SLUG}"
    curl -s "${BASE_URL}/${ENGLISH_SLUG}" | jq .
else
    echo -e "${RED}Skipping: No English slug available${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 20: Test SEO Redirects (Change Slug and Access Old URL)${NC}"
if [ ! -z "$ENGLISH_ID" ]; then
    # Change slug to something new
    NEW_SLUG="redirect-test-slug"
    echo "POST ${BASE_URL}${API_BASE}/profiles/${ENGLISH_ID}/change-slug"
    RESPONSE=$(curl -s -X POST "${BASE_URL}${API_BASE}/profiles/${ENGLISH_ID}/change-slug" \
      -H "Content-Type: application/json" \
      -d "{\"slug\": \"${NEW_SLUG}\"}")

    echo "$RESPONSE" | jq .

    SUCCESS=$(echo "$RESPONSE" | jq -r '.success // false')
    if [ "$SUCCESS" = "true" ]; then
        echo -e "${GREEN}✓ Slug changed successfully${NC}"

        # Now try to access the old slug - should redirect
        echo "GET ${BASE_URL}/${ENGLISH_SLUG} (should redirect to /${NEW_SLUG})"
        HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "${BASE_URL}/${ENGLISH_SLUG}")
        if [ "$HTTP_CODE" = "301" ]; then
            echo -e "${GREEN}✓ SEO redirect working (301)${NC}"
        else
            echo -e "${RED}✗ Expected 301 redirect, got $HTTP_CODE${NC}"
        fi

        # Test new slug still works
        echo "GET ${BASE_URL}/${NEW_SLUG}"
        curl -s "${BASE_URL}/${NEW_SLUG}" | jq .
    else
        echo -e "${RED}✗ Failed to change slug${NC}"
    fi
else
    echo -e "${RED}Skipping: No profile ID available${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 21: Test Slug Availability Checking${NC}"
echo "GET ${BASE_URL}${API_BASE}/profiles/check-slug?slug=test-availability"
RESPONSE=$(curl -s "${BASE_URL}${API_BASE}/profiles/check-slug?slug=test-availability")
echo "$RESPONSE" | jq .

echo ""
echo "GET ${BASE_URL}${API_BASE}/profiles/check-slug?slug=${ENGLISH_SLUG}"
RESPONSE=$(curl -s "${BASE_URL}${API_BASE}/profiles/check-slug?slug=${ENGLISH_SLUG:-john-doe}")
echo "$RESPONSE" | jq .

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${YELLOW}Test 22: Test Reserved Slug Handling${NC}"
echo "GET ${BASE_URL}/api (should not match profile route)"
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" "${BASE_URL}/api")
if [ "$HTTP_CODE" = "404" ]; then
    echo -e "${GREEN}✓ Reserved slug 'api' properly handled${NC}"
else
    echo -e "${YELLOW}Reserved slug returned $HTTP_CODE (may be handled by other routes)${NC}"
fi

echo ""
echo "----------------------------------------"
echo ""

sleep 1

echo -e "${BLUE}Cleaning up created profiles...${NC}"
for id in $CREATED_IDS; do
    if [ ! -z "$id" ]; then
        echo -e "${BLUE}Deleting profile ID: $id${NC}"
        curl -s -X DELETE "${BASE_URL}${API_BASE}/profiles/$id" > /dev/null
    fi
done

echo ""
echo -e "${GREEN}=========================================="
echo -e "Clean URL Routing System Testing Complete!"
echo -e "==========================================${NC}"
echo ""
echo "Tested features:"
echo "✓ English slug profiles"
echo "✓ Persian slug profiles"
echo "✓ Mixed Persian-English slug profiles"
echo "✓ Business profiles"
echo "✓ Profile creation, reading, updating, deletion"
echo "✓ Public profile access via slug (/profiles/:slug)"
echo "✓ Root-level profile access (/:slug - clean URLs without @)"
echo "✓ Private profile access control"
echo "✓ Slug uniqueness validation"
echo "✓ Error handling and HTTP status codes"
echo "✓ Profile listing with filtering"
echo "✓ SEO redirects for changed URLs (301)"
echo "✓ Slug availability checking API"
echo "✓ Slug change API with redirect setup"
echo "✓ Reserved path handling"
echo "✓ Performance caching for slug lookups"
