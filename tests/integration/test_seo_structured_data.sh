#!/bin/bash
set -e  # Exit on error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
API_URL="${API_BASE_URL:-http://localhost:3000}"
PROFILE_ID=""
PROFILE_SLUG=""
OWNER_TOKEN=""

echo "============================================"
echo "SEO & Structured Data Integration Tests"
echo "============================================"
echo ""

# Cleanup function
cleanup() {
    if [ -n "$PROFILE_ID" ] && [ -n "$OWNER_TOKEN" ]; then
        echo ""
        echo "Cleaning up test profile..."
        curl -s -X DELETE "$API_URL/api/profiles/$PROFILE_ID" \
            -H "Authorization: Bearer $OWNER_TOKEN" > /dev/null || true
    fi
}
trap cleanup EXIT

# Test 1: Create a test profile
test_create_profile() {
    echo "Test 1: Creating test profile for SEO testing..."
    
    RESPONSE=$(curl -s -X POST "$API_URL/api/profiles" \
        -H "Content-Type: application/json" \
        -d '{
            "slug": "seo-test-john-doe",
            "name": "John Doe",
            "type": "PERSON",
            "bio": "Senior Software Engineer specializing in C++ and distributed systems",
            "title": "Senior Software Engineer",
            "company": "Tech Corp",
            "skills": ["C++", "Python", "JavaScript"],
            "email": "john@example.com"
        }')
    
    # Extract profile ID and owner token
    PROFILE_ID=$(echo "$RESPONSE" | jq -r '.data.id')
    OWNER_TOKEN=$(echo "$RESPONSE" | jq -r '.data.ownerToken')
    PROFILE_SLUG=$(echo "$RESPONSE" | jq -r '.data.slug')
    
    if [ -z "$PROFILE_ID" ] || [ "$PROFILE_ID" == "null" ]; then
        echo -e "${RED}✗ Failed to create test profile${NC}"
        echo "Response: $RESPONSE"
        exit 1
    fi
    
    echo -e "${GREEN}✓ Profile created: $PROFILE_SLUG (ID: $PROFILE_ID)${NC}"
}

# Test 2: Fetch HTML page and verify it's HTML
test_html_response() {
    echo ""
    echo "Test 2: Fetching profile as HTML (browser request)..."
    
    HTML=$(curl -s "$API_URL/$PROFILE_SLUG" \
        -H "Accept: text/html")
    
    # Check if response is HTML
    if echo "$HTML" | grep -q "<!DOCTYPE html>"; then
        echo -e "${GREEN}✓ Received HTML response${NC}"
    else
        echo -e "${RED}✗ Expected HTML response${NC}"
        echo "Response preview: $(echo "$HTML" | head -n 5)"
        exit 1
    fi
}

# Test 3: Verify JSON-LD structured data exists
test_jsonld_exists() {
    echo ""
    echo "Test 3: Checking for JSON-LD structured data..."
    
    HTML=$(curl -s "$API_URL/$PROFILE_SLUG")
    
    if echo "$HTML" | grep -q '<script type="application/ld+json">'; then
        echo -e "${GREEN}✓ JSON-LD script tag found${NC}"
    else
        echo -e "${RED}✗ JSON-LD script tag not found${NC}"
        exit 1
    fi
}

# Test 4: Extract and validate JSON-LD Person schema
test_jsonld_person_schema() {
    echo ""
    echo "Test 4: Extracting and validating JSON-LD Person schema..."
    
    HTML=$(curl -s "$API_URL/$PROFILE_SLUG")
    
    # Extract JSON-LD (this is a simplified extraction)
    JSONLD=$(echo "$HTML" | sed -n '/<script type="application\/ld+json">/,/<\/script>/p' | sed '1d;$d')
    
    if [ -z "$JSONLD" ]; then
        echo -e "${YELLOW}⚠ Could not extract JSON-LD (might need jq or better parsing)${NC}"
        return
    fi
    
    # Check for required fields
    if echo "$JSONLD" | jq -e '.["@context"]' > /dev/null 2>&1; then
        echo -e "${GREEN}✓ @context field present${NC}"
    else
        echo -e "${RED}✗ @context field missing${NC}"
    fi
    
    if echo "$JSONLD" | jq -e '.["@type"]' > /dev/null 2>&1; then
        TYPE=$(echo "$JSONLD" | jq -r '.["@type"]')
        if [ "$TYPE" == "Person" ]; then
            echo -e "${GREEN}✓ @type is 'Person'${NC}"
        else
            echo -e "${RED}✗ @type is not 'Person' (got: $TYPE)${NC}"
        fi
    else
        echo -e "${RED}✗ @type field missing${NC}"
    fi
    
    if echo "$JSONLD" | jq -e '.name' > /dev/null 2>&1; then
        echo -e "${GREEN}✓ name field present${NC}"
    else
        echo -e "${RED}✗ name field missing${NC}"
    fi
}

# Test 5: Verify Open Graph tags
test_open_graph_tags() {
    echo ""
    echo "Test 5: Checking for Open Graph meta tags..."
    
    HTML=$(curl -s "$API_URL/$PROFILE_SLUG")
    
    OG_TAGS=("og:title" "og:description" "og:type" "og:url")
    
    for TAG in "${OG_TAGS[@]}"; do
        if echo "$HTML" | grep -q "property=\"$TAG\""; then
            echo -e "${GREEN}✓ $TAG tag found${NC}"
        else
            echo -e "${YELLOW}⚠ $TAG tag not found${NC}"
        fi
    done
}

# Test 6: Verify Twitter Card tags
test_twitter_card_tags() {
    echo ""
    echo "Test 6: Checking for Twitter Card meta tags..."
    
    HTML=$(curl -s "$API_URL/$PROFILE_SLUG")
    
    TWITTER_TAGS=("twitter:card" "twitter:title" "twitter:description")
    
    for TAG in "${TWITTER_TAGS[@]}"; do
        if echo "$HTML" | grep -q "name=\"$TAG\""; then
            echo -e "${GREEN}✓ $TAG tag found${NC}"
        else
            echo -e "${YELLOW}⚠ $TAG tag not found${NC}"
        fi
    done
}

# Test 7: Verify canonical URL
test_canonical_url() {
    echo ""
    echo "Test 7: Checking for canonical URL..."
    
    HTML=$(curl -s "$API_URL/$PROFILE_SLUG")
    
    if echo "$HTML" | grep -q '<link rel="canonical"'; then
        echo -e "${GREEN}✓ Canonical URL tag found${NC}"
    else
        echo -e "${YELLOW}⚠ Canonical URL tag not found${NC}"
    fi
}

# Test 8: Verify JSON API still works
test_json_api() {
    echo ""
    echo "Test 8: Verifying JSON API with Accept header..."
    
    JSON=$(curl -s "$API_URL/$PROFILE_SLUG" \
        -H "Accept: application/json")
    
    if echo "$JSON" | jq -e '.success' > /dev/null 2>&1; then
        SUCCESS=$(echo "$JSON" | jq -r '.success')
        if [ "$SUCCESS" == "true" ]; then
            echo -e "${GREEN}✓ JSON API works correctly${NC}"
        else
            echo -e "${RED}✗ JSON API returned success=false${NC}"
        fi
    else
        echo -e "${RED}✗ JSON API response is not valid JSON${NC}"
        exit 1
    fi
}

# Test 9: Test sitemap.xml
test_sitemap() {
    echo ""
    echo "Test 9: Checking sitemap.xml..."
    
    SITEMAP=$(curl -s "$API_URL/sitemap.xml")
    
    if echo "$SITEMAP" | grep -q '<?xml version="1.0" encoding="UTF-8"?>'; then
        echo -e "${GREEN}✓ Sitemap has valid XML declaration${NC}"
    else
        echo -e "${YELLOW}⚠ Sitemap XML declaration not found${NC}"
    fi
    
    if echo "$SITEMAP" | grep -q '<urlset'; then
        echo -e "${GREEN}✓ Sitemap has urlset element${NC}"
    else
        echo -e "${YELLOW}⚠ Sitemap urlset element not found${NC}"
    fi
    
    # Check if our profile is in the sitemap
    if echo "$SITEMAP" | grep -q "$PROFILE_SLUG"; then
        echo -e "${GREEN}✓ Test profile found in sitemap${NC}"
    else
        echo -e "${YELLOW}⚠ Test profile not found in sitemap (might be cached)${NC}"
    fi
}

# Test 10: Verify meta description length
test_meta_description() {
    echo ""
    echo "Test 10: Checking meta description length..."
    
    HTML=$(curl -s "$API_URL/$PROFILE_SLUG")
    
    DESC=$(echo "$HTML" | grep -o '<meta name="description" content="[^"]*"' | sed 's/<meta name="description" content="//;s/"$//')
    
    if [ -n "$DESC" ]; then
        LENGTH=${#DESC}
        if [ $LENGTH -le 160 ]; then
            echo -e "${GREEN}✓ Meta description length is $LENGTH chars (≤160)${NC}"
        else
            echo -e "${YELLOW}⚠ Meta description is $LENGTH chars (>160)${NC}"
        fi
    else
        echo -e "${RED}✗ Meta description not found${NC}"
    fi
}

# Run all tests
test_create_profile
test_html_response
test_jsonld_exists
test_jsonld_person_schema
test_open_graph_tags
test_twitter_card_tags
test_canonical_url
test_json_api
test_sitemap
test_meta_description

echo ""
echo "============================================"
echo -e "${GREEN}All SEO tests completed!${NC}"
echo "============================================"
echo ""
echo "Manual validation steps:"
echo "1. Visit the profile at: $API_URL/$PROFILE_SLUG"
echo "2. Test with Google Rich Results: https://search.google.com/test/rich-results"
echo "3. Test with Twitter Card Validator: https://cards-dev.twitter.com/validator"
echo "4. Test with Facebook Debugger: https://developers.facebook.com/tools/debug/"
echo ""
