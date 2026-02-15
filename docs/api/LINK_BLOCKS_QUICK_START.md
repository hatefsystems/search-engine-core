# Link Blocks - Quick Start Guide

**Status:** ✅ Live and tested
**Base URL:** `http://localhost:3000`

---

## Quick Start (5 minutes)

### 1. Create a Profile

```bash
curl -X POST http://localhost:3000/api/profiles \
  -H "Content-Type: application/json" \
  -d '{
    "slug": "your-username",
    "name": "Your Name",
    "type": "PERSON"
  }'
```

**Save the `ownerToken` from response!**

### 2. Get Your Profile ID

```bash
curl http://localhost:3000/api/profiles | jq '.data[] | select(.slug=="your-username") | .id'
```

### 3. Create Your First Link

```bash
curl -X POST http://localhost:3000/api/profiles/<PROFILE_ID>/links \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <YOUR_TOKEN>" \
  -d '{
    "url": "https://github.com/yourusername",
    "title": "My GitHub Profile",
    "description": "Check out my open source projects!",
    "privacy": "PUBLIC"
  }'
```

**Save the `id` from response!**

### 4. Test Your Link

```bash
# Redirect to your link
curl -L http://localhost:3000/l/<LINK_ID>

# This will redirect to your GitHub profile and record analytics
```

### 5. View Analytics

```bash
curl http://localhost:3000/api/profiles/<PROFILE_ID>/links/analytics \
  -H "Authorization: Bearer <YOUR_TOKEN>" | jq .
```

---

## Common Operations

### Create Multiple Links

```bash
# GitHub
curl -X POST http://localhost:3000/api/profiles/<PROFILE_ID>/links \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <TOKEN>" \
  -d '{"url":"https://github.com/user","title":"GitHub","sortOrder":1}'

# Twitter  
curl -X POST http://localhost:3000/api/profiles/<PROFILE_ID>/links \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <TOKEN>" \
  -d '{"url":"https://twitter.com/user","title":"Twitter","sortOrder":2}'

# LinkedIn
curl -X POST http://localhost:3000/api/profiles/<PROFILE_ID>/links \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <TOKEN>" \
  -d '{"url":"https://linkedin.com/in/user","title":"LinkedIn","sortOrder":3}'
```

### Update a Link

```bash
curl -X PUT http://localhost:3000/api/profiles/<PROFILE_ID>/links/<LINK_ID> \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <TOKEN>" \
  -d '{"title":"Updated Title","description":"New description"}'
```

### Change Link Privacy

```bash
# Make link hidden (redirect works, no analytics)
curl -X PUT http://localhost:3000/api/profiles/<PROFILE_ID>/links/<LINK_ID> \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <TOKEN>" \
  -d '{"privacy":"HIDDEN"}'

# Disable link (404 on redirect)
curl -X PUT http://localhost:3000/api/profiles/<PROFILE_ID>/links/<LINK_ID> \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <TOKEN>" \
  -d '{"privacy":"DISABLED"}'
```

### Delete a Link

```bash
curl -X DELETE http://localhost:3000/api/profiles/<PROFILE_ID>/links/<LINK_ID> \
  -H "Authorization: Bearer <TOKEN>"
```

---

## Privacy Levels Explained

| Privacy | On Profile | Redirect | Analytics |
|---------|-----------|----------|-----------|
| **PUBLIC** | ✅ Visible | ✅ Works | ✅ Recorded |
| **HIDDEN** | ❌ Hidden | ✅ Works | ❌ Not recorded |
| **DISABLED** | ❌ Hidden | ❌ Returns 404 | ❌ Not recorded |

---

## Analytics Data Collected

### ✅ What We Collect (Privacy-Safe)

- Click timestamp
- City-level location (e.g., "Tehran, Iran")
- Browser family (e.g., "Chrome" - no version)
- OS family (e.g., "Android" - no version)
- Device type (Mobile, Tablet, or Desktop)
- Referrer domain (sanitized)

### ❌ What We DON'T Collect

- IP addresses
- Precise user-agent strings
- User identifiers or cookies
- Precise geolocation (lat/lon)
- Browser/OS versions
- Any personally identifiable information

---

## Testing Your Links

### Quick Test Flow

```bash
# 1. Create a test link
LINK_RESPONSE=$(curl -s -X POST http://localhost:3000/api/profiles/<PROFILE_ID>/links \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer <TOKEN>" \
  -d '{"url":"https://example.com","title":"Test Link"}')

LINK_ID=$(echo "$LINK_RESPONSE" | jq -r '.data.id')

# 2. Click it 5 times
for i in {1..5}; do
  curl -s -o /dev/null http://localhost:3000/l/$LINK_ID
  sleep 0.5
done

# 3. Check analytics
curl -s http://localhost:3000/api/profiles/<PROFILE_ID>/links/analytics \
  -H "Authorization: Bearer <TOKEN>" | jq '{clicks: .data.totalClicks}'
```

---

## Maintenance

### Clean Up Old Analytics

```bash
# Run this periodically (e.g., daily cron job)
curl -X POST http://localhost:3000/api/internal/analytics/cleanup \
  -H "x-api-key: <YOUR_INTERNAL_API_KEY>"
```

**Default:** Deletes analytics older than 90 days

**Configure:** Set `LINK_ANALYTICS_RETENTION_DAYS` environment variable

---

## Troubleshooting

### Link Not Found (404)

- Check link ID is correct (24-character hex)
- Verify link belongs to the profile
- Check if link is DISABLED or deleted

### Forbidden (403)

- Verify owner token is correct
- Check Authorization header format: `Bearer <token>`
- Ensure you're using the profile's owner token

### Analytics Not Recording

- Check link privacy is PUBLIC (not HIDDEN or DISABLED)
- Verify server logs: `docker logs core | grep "Link click recorded"`
- Ensure MongoDB is running and connected

### Rate Limit Exceeded (429)

- Wait for the retry-after period (default: 60 seconds)
- Reduce request frequency
- Configure higher limits via environment variables

---

## Next Steps

1. **Add More Links:** Create links for all your social profiles
2. **Customize Display:** Use sortOrder to organize links
3. **Monitor Analytics:** Track which links get the most clicks
4. **Adjust Privacy:** Use HIDDEN for links you want trackable but not public
5. **Clean Up:** Regularly run the cleanup endpoint to maintain GDPR compliance

---

## Support

- **API Docs:** [link_blocks_endpoint.md](link_blocks_endpoint.md)
- **Feature Details:** [../features/LINK_BLOCKS.md](../features/LINK_BLOCKS.md)
- **Database Schema:** [../architecture/profile-database-schema.md](../architecture/profile-database-schema.md)
- **Server Logs:** `docker logs core`
