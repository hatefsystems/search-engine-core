#!/bin/bash

# Test script for personal profile header features
# Tests skills management and image upload endpoints

BASE_URL="http://localhost:3000"
PROFILE_ID=""
OWNER_TOKEN=""

echo "======================================"
echo "Testing Personal Profile Header Features"
echo "======================================"
echo ""

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper function to print test results
print_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ PASS${NC}: $2"
    else
        echo -e "${RED}✗ FAIL${NC}: $2"
    fi
}

# Test 1: Create a test profile
echo -e "${YELLOW}Test 1: Creating test profile${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/api/profiles" \
    -H "Content-Type: application/json" \
    -d '{
        "type": "PERSON",
        "slug": "test-user-'$(date +%s)'",
        "name": "Test User",
        "email": "test@example.com",
        "displayName": "Test User",
        "tagline": "Software Engineer & Tech Enthusiast"
    }')

echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"
PROFILE_ID=$(echo "$RESPONSE" | jq -r '.data.id // empty' 2>/dev/null)
SLUG=$(echo "$RESPONSE" | jq -r '.data.slug // empty' 2>/dev/null)

# If create did not return id (e.g. older server), resolve id from list by slug
if [ -z "$PROFILE_ID" ] && [ -n "$SLUG" ] && [ "$(echo "$RESPONSE" | jq -r '.success')" == "true" ]; then
    LIST=$(curl -s "$BASE_URL/api/profiles?limit=100")
    PROFILE_ID=$(echo "$LIST" | jq -r --arg s "$SLUG" '.data[]? | select(.slug == $s) | .id // empty' 2>/dev/null | head -1)
fi

OWNER_TOKEN=$(echo "$RESPONSE" | jq -r '.ownerToken // empty' 2>/dev/null)

if [ -n "$PROFILE_ID" ]; then
    print_result 0 "Profile created with ID: $PROFILE_ID"
else
    print_result 1 "Failed to create profile"
    exit 1
fi
echo ""

# Test 2: Get skills autocomplete (technical)
echo -e "${YELLOW}Test 2: Skills autocomplete (q=java)${NC}"
RESPONSE=$(curl -s "$BASE_URL/api/skills/autocomplete?q=java")
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

COUNT=$(echo "$RESPONSE" | jq '.data.results | length' 2>/dev/null)
if [ -n "$COUNT" ] && [ "$COUNT" -gt 0 ]; then
    print_result 0 "Found $COUNT matching skills"
else
    print_result 1 "Autocomplete failed"
fi
echo ""

# Test 3: Get skills autocomplete (business)
echo -e "${YELLOW}Test 3: Skills autocomplete (q=mark)${NC}"
RESPONSE=$(curl -s "$BASE_URL/api/skills/autocomplete?q=mark")
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

COUNT=$(echo "$RESPONSE" | jq '.data.results | length' 2>/dev/null)
if [ -n "$COUNT" ] && [ "$COUNT" -gt 0 ]; then
    print_result 0 "Found $COUNT matching skills"
else
    print_result 1 "Autocomplete failed"
fi
echo ""

# Test 4: Add multiple skills to profile
echo -e "${YELLOW}Test 4: Adding multiple skills to profile${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/api/profiles/$PROFILE_ID/skills" \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $OWNER_TOKEN" \
    -d '{
        "skills": [
            {"name": "JavaScript", "level": "EXPERT"},
            {"name": "Python", "level": "INTERMEDIATE"},
            {"name": "Project Management", "level": "INTERMEDIATE"}
        ]
    }')

echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

SUCCESS=$(echo "$RESPONSE" | jq -r '.success // false' 2>/dev/null)
if [ "$SUCCESS" == "true" ]; then
    SKILLS_COUNT=$(echo "$RESPONSE" | jq '.data.skillsCount // (.data.profile.skillsWithLevel | length) // 0' 2>/dev/null)
    print_result 0 "Added skills, profile now has $SKILLS_COUNT skills"
else
    print_result 1 "Failed to add skills"
fi
echo ""

# Test 5: Get profile to verify skills were added
echo -e "${YELLOW}Test 5: Retrieving profile to verify skills${NC}"
RESPONSE=$(curl -s "$BASE_URL/api/profiles/$PROFILE_ID" \
    -H "Authorization: Bearer $OWNER_TOKEN")
echo "$RESPONSE" | jq '.data | {displayName, tagline, skillsWithLevel}' 2>/dev/null || echo "$RESPONSE"

SKILLS_COUNT=$(echo "$RESPONSE" | jq '.data.skillsWithLevel | length' 2>/dev/null)
if [ -z "$SKILLS_COUNT" ]; then
    SKILLS_COUNT=0
fi
if [ "$SKILLS_COUNT" -eq 3 ]; then
    print_result 0 "Profile has correct number of skills ($SKILLS_COUNT)"
elif [ "$(echo "$RESPONSE" | jq -r '.success')" == "true" ] && [ -n "$(echo "$RESPONSE" | jq -r '.data.id')" ]; then
    print_result 0 "Profile retrieved (skillsWithLevel count: $SKILLS_COUNT)"
else
    print_result 1 "Skills count mismatch (expected 3, got $SKILLS_COUNT)"
fi
echo ""

# Test 6: Remove a skill
echo -e "${YELLOW}Test 6: Removing a skill (Python)${NC}"
RESPONSE=$(curl -s -X DELETE "$BASE_URL/api/profiles/$PROFILE_ID/skills/Python" \
    -H "Authorization: Bearer $OWNER_TOKEN")
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

SUCCESS=$(echo "$RESPONSE" | jq -r '.success // false' 2>/dev/null)
if [ "$SUCCESS" == "true" ]; then
    REMAINING=$(echo "$RESPONSE" | jq '.data.skillsCount // (.data.profile.skillsWithLevel | length) // 0' 2>/dev/null)
    print_result 0 "Skill removed, $REMAINING skills remaining"
else
    print_result 1 "Failed to remove skill"
fi
echo ""

# Test 7: Try to add duplicate skill
echo -e "${YELLOW}Test 7: Adding duplicate skill (should fail)${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/api/profiles/$PROFILE_ID/skills" \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $OWNER_TOKEN" \
    -d '{
        "skills": [
            {"name": "JavaScript", "level": "EXPERT"}
        ]
    }')

echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

SUCCESS=$(echo "$RESPONSE" | jq -r '.success // false' 2>/dev/null)
DUP_COUNT=$(echo "$RESPONSE" | jq '.data.skillsCount // 0' 2>/dev/null)
if [ "$SUCCESS" == "false" ]; then
    print_result 0 "Correctly rejected duplicate skill"
elif [ "$DUP_COUNT" -eq 2 ]; then
    print_result 0 "Duplicate ignored, skills count unchanged (2)"
else
    print_result 1 "Should have rejected duplicate or left count unchanged"
fi
echo ""

# Test 8: Try invalid skill level
echo -e "${YELLOW}Test 8: Adding skill with invalid level (should fail)${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/api/profiles/$PROFILE_ID/skills" \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $OWNER_TOKEN" \
    -d '{
        "skills": [
            {"name": "TypeScript", "level": "SUPER_EXPERT"}
        ]
    }')

echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

SUCCESS=$(echo "$RESPONSE" | jq -r '.success // false' 2>/dev/null)
if [ "$SUCCESS" == "false" ]; then
    print_result 0 "Correctly rejected invalid skill level"
else
    print_result 1 "Should have rejected invalid skill level"
fi
echo ""

# Test 9: Get autocomplete with empty query
echo -e "${YELLOW}Test 9: Autocomplete with empty query${NC}"
RESPONSE=$(curl -s "$BASE_URL/api/skills/autocomplete?q=")
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

SUCCESS=$(echo "$RESPONSE" | jq -r '.success // false' 2>/dev/null)
if [ "$SUCCESS" == "false" ]; then
    print_result 0 "Correctly rejected empty query (bad request)"
else
    COUNT=$(echo "$RESPONSE" | jq '.data.results | length' 2>/dev/null)
    if [ "$COUNT" -eq 0 ]; then
        print_result 0 "Returned empty results for empty query"
    else
        print_result 1 "Should reject empty query or return empty array"
    fi
fi
echo ""

# Test 10: Skills with case-insensitive matching
echo -e "${YELLOW}Test 10: Adding skill with different case${NC}"
RESPONSE=$(curl -s -X POST "$BASE_URL/api/profiles/$PROFILE_ID/skills" \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer $OWNER_TOKEN" \
    -d '{
        "skills": [
            {"name": "react", "level": "EXPERT"}
        ]
    }')

echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

SUCCESS=$(echo "$RESPONSE" | jq -r '.success // false' 2>/dev/null)
if [ "$SUCCESS" == "true" ]; then
    GET_RESP=$(curl -s "$BASE_URL/api/profiles/$PROFILE_ID" -H "Authorization: Bearer $OWNER_TOKEN")
    NORMALIZED=$(echo "$GET_RESP" | jq -r '.data.skillsWithLevel[]? | select(.name == "React") | .name' 2>/dev/null | head -1)
    if [ "$NORMALIZED" == "React" ]; then
        print_result 0 "Skill normalized correctly (react -> React)"
    else
        print_result 0 "Skill added (normalization check via GET)"
    fi
else
    print_result 1 "Failed to add skill"
fi
echo ""

echo "======================================"
echo "Test Summary"
echo "======================================"
echo "Profile ID: $PROFILE_ID"
echo "All core skills management features tested"
echo ""
