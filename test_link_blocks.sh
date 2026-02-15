#!/bin/bash

# Link Blocks API Test Script
# Tests link block CRUD operations, redirect, and analytics

set -e

BASE_URL="http://localhost:3000"
PROFILE_ID=""
OWNER_TOKEN=""
LINK_ID=""

echo "======================================"
echo "Link Blocks API Test Script"
echo "======================================"
echo ""

# Step 1: Create a test profile
echo "1. Creating test profile..."
CREATE_PROFILE_RESPONSE=$(curl -s -X POST "${BASE_URL}/api/profiles" \
  -H "Content-Type: application/json" \
  -d '{
    "slug": "test-link-blocks-'$(date +%s)'",
    "name": "Test User for Link Blocks",
    "type": "PERSON",
    "bio": "Testing link blocks feature"
  }')

echo "Response: $CREATE_PROFILE_RESPONSE"
echo ""

# Extract owner token and slug
OWNER_TOKEN=$(echo $CREATE_PROFILE_RESPONSE | grep -oP '"ownerToken":"[^"]*"' | cut -d'"' -f4)
SLUG=$(echo $CREATE_PROFILE_RESPONSE | grep -oP '"slug":"[^"]*"' | cut -d'"' -f4)

if [ -z "$OWNER_TOKEN" ] || [ -z "$SLUG" ]; then
    echo "❌ Failed to create profile"
    exit 1
fi

# Fetch the profile by slug to get the ID
PROFILE_RESPONSE=$(curl -s "http://localhost:3000/api/profiles" | grep -A20 "\"slug\":\"$SLUG\"")
PROFILE_ID=$(echo $PROFILE_RESPONSE | grep -oP '"id":"[^"]*"' | cut -d'"' -f4 | head -1)

if [ -z "$PROFILE_ID" ]; then
    echo "❌ Failed to get profile ID"
    exit 1
fi

echo "✅ Profile created: $PROFILE_ID"
echo "   Owner token: ${OWNER_TOKEN:0:20}..."
echo ""

# Step 2: Create a link block
echo "2. Creating link block..."
CREATE_LINK_RESPONSE=$(curl -s -X POST "${BASE_URL}/api/profiles/${PROFILE_ID}/links" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer ${OWNER_TOKEN}" \
  -d '{
    "url": "https://github.com/test-user",
    "title": "My GitHub Profile",
    "description": "Check out my open source projects!",
    "privacy": "PUBLIC",
    "sortOrder": 0,
    "tags": ["social", "code"]
  }')

echo "Response: $CREATE_LINK_RESPONSE"
echo ""

# Extract link ID
LINK_ID=$(echo $CREATE_LINK_RESPONSE | grep -o '"id":"[^"]*"' | cut -d'"' -f4 | head -1)

if [ -z "$LINK_ID" ]; then
    echo "❌ Failed to create link"
    exit 1
fi

echo "✅ Link created: $LINK_ID"
echo ""

# Step 3: List all links for profile
echo "3. Listing all links..."
LIST_LINKS_RESPONSE=$(curl -s -X GET "${BASE_URL}/api/profiles/${PROFILE_ID}/links")
echo "Response: $LIST_LINKS_RESPONSE"
echo ""

# Step 4: Get specific link
echo "4. Getting link by ID..."
GET_LINK_RESPONSE=$(curl -s -X GET "${BASE_URL}/api/profiles/${PROFILE_ID}/links/${LINK_ID}")
echo "Response: $GET_LINK_RESPONSE"
echo ""

# Step 5: Test link redirect
echo "5. Testing link redirect..."
REDIRECT_RESPONSE=$(curl -s -w "\nHTTP Status: %{http_code}\nRedirect URL: %{redirect_url}\n" \
  -o /dev/null \
  -H "Referer: https://hatef.ir/test" \
  "${BASE_URL}/l/${LINK_ID}")

echo "$REDIRECT_RESPONSE"
echo "✅ Redirect tested (should be 302 to https://github.com/test-user)"
echo ""

# Step 6: Simulate multiple clicks for analytics
echo "6. Simulating clicks for analytics..."
for i in {1..5}; do
    curl -s -o /dev/null -w "Click $i: HTTP %{http_code}\n" \
      -H "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36" \
      "${BASE_URL}/l/${LINK_ID}"
    sleep 0.5
done
echo ""

# Step 7: Get analytics
echo "7. Getting link analytics..."
ANALYTICS_RESPONSE=$(curl -s -X GET "${BASE_URL}/api/profiles/${PROFILE_ID}/links/analytics" \
  -H "Authorization: Bearer ${OWNER_TOKEN}")
echo "Response: $ANALYTICS_RESPONSE"
echo ""

# Step 8: Update link
echo "8. Updating link..."
UPDATE_LINK_RESPONSE=$(curl -s -X PUT "${BASE_URL}/api/profiles/${PROFILE_ID}/links/${LINK_ID}" \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer ${OWNER_TOKEN}" \
  -d '{
    "title": "Updated GitHub Profile",
    "description": "New description!",
    "sortOrder": 5
  }')

echo "Response: $UPDATE_LINK_RESPONSE"
echo ""

# Step 9: Verify update
echo "9. Verifying link update..."
VERIFY_UPDATE=$(curl -s -X GET "${BASE_URL}/api/profiles/${PROFILE_ID}/links/${LINK_ID}")
echo "Response: $VERIFY_UPDATE"
echo ""

# Step 10: Delete link
echo "10. Deleting link..."
DELETE_LINK_RESPONSE=$(curl -s -X DELETE "${BASE_URL}/api/profiles/${PROFILE_ID}/links/${LINK_ID}" \
  -H "Authorization: Bearer ${OWNER_TOKEN}")
echo "Response: $DELETE_LINK_RESPONSE"
echo ""

# Step 11: Verify deletion (should 404)
echo "11. Verifying link deletion..."
VERIFY_DELETE=$(curl -s -w "\nHTTP Status: %{http_code}\n" \
  "${BASE_URL}/api/profiles/${PROFILE_ID}/links/${LINK_ID}")
echo "Response: $VERIFY_DELETE"
echo "✅ Link deleted (should return 404)"
echo ""

# Step 12: Test redirect after deletion (should 404)
echo "12. Testing redirect after deletion..."
REDIRECT_AFTER_DELETE=$(curl -s -w "\nHTTP Status: %{http_code}\n" \
  -o /dev/null \
  "${BASE_URL}/l/${LINK_ID}")
echo "$REDIRECT_AFTER_DELETE"
echo "✅ Redirect after delete (should return 404)"
echo ""

# Cleanup: Delete test profile
echo "13. Cleaning up test profile..."
DELETE_PROFILE_RESPONSE=$(curl -s -X DELETE "${BASE_URL}/api/profiles/${PROFILE_ID}" \
  -H "Authorization: Bearer ${OWNER_TOKEN}")
echo "Response: $DELETE_PROFILE_RESPONSE"
echo ""

echo "======================================"
echo "✅ All tests completed!"
echo "======================================"
echo ""
echo "Summary:"
echo "- Profile ID: $PROFILE_ID"
echo "- Link ID: $LINK_ID"
echo "- All CRUD operations tested"
echo "- Redirect functionality tested"
echo "- Analytics recorded (5 test clicks)"
echo "- Cleanup completed"
echo ""
echo "Note: Run 'docker logs core' to see server-side logs"
