# Link Blocks API Documentation

**Version:** 1.0
**Base URL:** `http://localhost:3000`
**Authentication:** Bearer token (owner token from profile)

---

## Overview

The Link Blocks API allows profile owners to manage clickable links on their profiles (social media, websites, portfolios, etc.) and track click analytics with privacy-first principles.

### Features

- **Link Management:** Create, read, update, and delete link blocks
- **Privacy-First Analytics:** Track clicks without storing IP addresses
- **Secure Redirects:** Fast, secure redirects to destination URLs
- **Privacy Controls:** Per-link privacy settings (PUBLIC, HIDDEN, DISABLED)
- **Rate Limiting:** Protects against abuse

---

## Authentication

All link management endpoints require the profile owner token:

```bash
Authorization: Bearer <ownerToken>
```

The owner token is returned when creating a profile and should be stored securely.

---

## Endpoints

### 1. Create Link Block

**POST** `/api/profiles/:id/links`

Create a new link block for a profile.

#### Request

**Headers:**
- `Content-Type: application/json`
- `Authorization: Bearer <ownerToken>`

**Path Parameters:**
- `id` (string, required): Profile ID

**Body:**
```json
{
  "url": "https://github.com/username",
  "title": "My GitHub",
  "description": "Check out my open source projects",
  "iconUrl": "https://github.com/favicon.ico",
  "isActive": true,
  "privacy": "PUBLIC",
  "tags": ["social", "code"],
  "sortOrder": 0
}
```

**Fields:**
- `url` (string, required): Destination URL (http/https, max 2048 chars)
- `title` (string, required): Link title (max 200 chars)
- `description` (string, optional): Description (max 500 chars)
- `iconUrl` (string, optional): Icon/favicon URL (validated)
- `isActive` (boolean, optional): Active state (default: true)
- `privacy` (string, optional): Privacy level - "PUBLIC", "HIDDEN", or "DISABLED" (default: "PUBLIC")
- `tags` (array, optional): Tags for grouping
- `sortOrder` (number, optional): Display order, lower = higher (default: 0)

#### Response

**200 OK:**
```json
{
  "success": true,
  "message": "Link created successfully",
  "data": {
    "id": "507f1f77bcf86cd799439011",
    "profileId": "507f191e810c19729de860ea",
    "url": "https://github.com/username",
    "title": "My GitHub",
    "description": "Check out my open source projects",
    "iconUrl": "https://github.com/favicon.ico",
    "isActive": true,
    "privacy": "PUBLIC",
    "tags": ["social", "code"],
    "sortOrder": 0,
    "createdAt": "2026-02-15T10:30:00Z"
  }
}
```

**400 Bad Request:**
```json
{
  "success": false,
  "message": "Invalid link data",
  "error": "BAD_REQUEST"
}
```

**403 Forbidden:**
```json
{
  "success": false,
  "message": "Not authorized to create links for this profile",
  "error": "FORBIDDEN"
}
```

---

### 2. List Links

**GET** `/api/profiles/:id/links`

Get all links for a profile (ordered by sortOrder).

#### Request

**Path Parameters:**
- `id` (string, required): Profile ID

#### Response

**200 OK:**
```json
{
  "success": true,
  "message": "Links retrieved successfully",
  "data": [
    {
      "id": "507f1f77bcf86cd799439011",
      "profileId": "507f191e810c19729de860ea",
      "url": "https://github.com/username",
      "title": "My GitHub",
      "isActive": true,
      "privacy": "PUBLIC",
      "sortOrder": 0,
      "createdAt": "2026-02-15T10:30:00Z"
    }
  ]
}
```

---

### 3. Get Link by ID

**GET** `/api/profiles/:id/links/:linkId`

Get a specific link by ID.

#### Request

**Path Parameters:**
- `id` (string, required): Profile ID
- `linkId` (string, required): Link ID

#### Response

**200 OK:**
```json
{
  "success": true,
  "message": "Link retrieved successfully",
  "data": {
    "id": "507f1f77bcf86cd799439011",
    "profileId": "507f191e810c19729de860ea",
    "url": "https://github.com/username",
    "title": "My GitHub",
    "description": "Check out my open source projects",
    "isActive": true,
    "privacy": "PUBLIC",
    "sortOrder": 0,
    "createdAt": "2026-02-15T10:30:00Z"
  }
}
```

**404 Not Found:**
```json
{
  "success": false,
  "message": "Link not found",
  "error": "NOT_FOUND"
}
```

---

### 4. Update Link

**PUT** `/api/profiles/:id/links/:linkId`

Update an existing link.

#### Request

**Headers:**
- `Content-Type: application/json`
- `Authorization: Bearer <ownerToken>`

**Path Parameters:**
- `id` (string, required): Profile ID
- `linkId` (string, required): Link ID

**Body:** (all fields optional, only include fields to update)
```json
{
  "url": "https://github.com/newusername",
  "title": "Updated GitHub",
  "isActive": false,
  "sortOrder": 5
}
```

#### Response

**200 OK:**
```json
{
  "success": true,
  "message": "Link updated successfully",
  "data": {
    "id": "507f1f77bcf86cd799439011",
    "profileId": "507f191e810c19729de860ea",
    "url": "https://github.com/newusername",
    "title": "Updated GitHub",
    "isActive": false,
    "sortOrder": 5,
    "createdAt": "2026-02-15T10:30:00Z",
    "updatedAt": "2026-02-15T11:00:00Z"
  }
}
```

---

### 5. Delete Link

**DELETE** `/api/profiles/:id/links/:linkId`

Delete a link block.

#### Request

**Headers:**
- `Authorization: Bearer <ownerToken>`

**Path Parameters:**
- `id` (string, required): Profile ID
- `linkId` (string, required): Link ID

#### Response

**200 OK:**
```json
{
  "success": true,
  "message": "Link deleted successfully"
}
```

**404 Not Found:**
```json
{
  "success": false,
  "message": "Link not found",
  "error": "NOT_FOUND"
}
```

---

### 6. Redirect to Link

**GET** `/l/:linkId`

Redirect to the link's destination URL and record analytics.

#### Request

**Path Parameters:**
- `linkId` (string, required): Link ID

**Optional Headers:**
- `Referer`: Original page (tracked in analytics)
- `User-Agent`: Browser/device info (parsed for analytics)

#### Response

**302 Found:**
```
Location: https://destination-url.com
```

**404 Not Found:**
```
Link not found
```

**429 Too Many Requests:**
```json
{
  "success": false,
  "message": "Too many redirect requests. Please try again later.",
  "error": "RATE_LIMIT_EXCEEDED",
  "retryAfter": 30
}
```

#### Notes

- Redirects are **fast** (< 50ms target)
- **Secure:** Only redirects to stored URL (no open redirect vulnerability)
- **Privacy:** IP addresses not stored in analytics
- **Rate Limited:** 120 requests per minute per IP (configurable)

---

### 7. Get Link Analytics

**GET** `/api/profiles/:id/links/analytics`

Get click analytics for all links in a profile.

#### Request

**Headers:**
- `Authorization: Bearer <ownerToken>`

**Path Parameters:**
- `id` (string, required): Profile ID

#### Response

**200 OK:**
```json
{
  "success": true,
  "message": "Analytics retrieved successfully",
  "data": {
    "totalClicks": 1234,
    "recentClicks": [
      {
        "linkId": "507f1f77bcf86cd799439011",
        "timestamp": 1708000000000,
        "country": "Iran",
        "city": "Tehran",
        "browser": "Chrome",
        "os": "Android",
        "deviceType": "Mobile",
        "referrer": "https://example.com"
      }
    ]
  }
}
```

**403 Forbidden:**
```json
{
  "success": false,
  "message": "Not authorized to view analytics for this profile",
  "error": "FORBIDDEN"
}
```

#### Notes

- **Privacy-First:** No IP addresses, only city-level location
- **Generic Device Info:** Browser/OS family only, no versions
- **Recent Clicks:** Last 100 clicks returned
- **Performance:** Target < 200ms response time

---

## Privacy Levels

| Level | Visibility | Redirect Works | Analytics Recorded |
|-------|-----------|----------------|-------------------|
| PUBLIC | Visible on profile | ✅ Yes | ✅ Yes |
| HIDDEN | Hidden from profile | ✅ Yes | ❌ No |
| DISABLED | Hidden from profile | ❌ No (404) | ❌ No |

---

## Rate Limits

| Endpoint | Limit | Window |
|----------|-------|--------|
| Link Management APIs | 60 requests | 60 seconds |
| Link Redirects | 120 requests | 60 seconds |

Limits are per IP address and configurable via environment variables:
- `PROFILE_API_RATE_LIMIT_REQUESTS`
- `PROFILE_API_RATE_LIMIT_WINDOW_SECONDS`
- `LINK_REDIRECT_RATE_LIMIT_REQUESTS`
- `LINK_REDIRECT_RATE_LIMIT_WINDOW_SECONDS`

---

## Data Retention

Link click analytics follow GDPR-friendly retention policies:

- **Default Retention:** 90 days (configurable)
- **Environment Variable:** `LINK_ANALYTICS_RETENTION_DAYS`
- **Privacy:** IP addresses never stored in analytics tier
- **Compliance:** Automated cleanup runs periodically

---

## Examples

### Create a GitHub Link

```bash
curl -X POST http://localhost:3000/api/profiles/507f191e810c19729de860ea/links \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer your_owner_token_here" \
  -d '{
    "url": "https://github.com/username",
    "title": "My GitHub",
    "description": "Check out my code!",
    "privacy": "PUBLIC",
    "sortOrder": 0
  }'
```

### List All Links

```bash
curl http://localhost:3000/api/profiles/507f191e810c19729de860ea/links
```

### Click a Link (Redirect)

```bash
curl -L http://localhost:3000/l/507f1f77bcf86cd799439011
```

### Get Analytics

```bash
curl http://localhost:3000/api/profiles/507f191e810c19729de860ea/links/analytics \
  -H "Authorization: Bearer your_owner_token_here"
```

---

## Error Codes

| Code | Description |
|------|-------------|
| BAD_REQUEST | Invalid request data (validation failed) |
| FORBIDDEN | Not authorized (invalid or missing owner token) |
| NOT_FOUND | Resource not found |
| RATE_LIMIT_EXCEEDED | Too many requests |
| STORAGE_ERROR | Database error |

---

## Security

- **URL Validation:** All URLs validated (http/https only, max 2048 chars)
- **Open Redirect Prevention:** Redirects only to stored URLs
- **Authentication:** Owner token required for all write operations
- **Rate Limiting:** Protects against abuse and spam
- **Input Validation:** Title/description length limits enforced
- **Privacy:** IP addresses not stored in analytics
