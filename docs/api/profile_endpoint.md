# Profile API Documentation

## Overview

The Profile API provides endpoints for managing user and business profiles with support for authentication, rate limiting, and soft delete functionality.

**Base URLs:**

- API: `/api/profiles`
- Public: `/profiles/:slug` and `/:slug`

**Collection:** `profiles` (MongoDB database: `search-engine`)

**Features:**

- Token-based authentication for profile ownership
- Rate limiting (60 requests per 60 seconds by default)
- Soft delete with recovery
- SEO-friendly URLs with Persian and English support

---

## Authentication

Protected endpoints (create, update, delete, restore, changeSlug) support optional token-based authentication:

**Headers:**

```
Authorization: Bearer <owner_token>
```

or

```
x-profile-token: <owner_token>
```

**Backward Compatibility:** Profiles without `ownerToken` can be modified without authentication.

---

## Rate Limiting

All API endpoints are rate-limited to prevent abuse.

**Default Limits:**

- 60 requests per 60 seconds per IP address

**Configuration:**

```bash
PROFILE_API_RATE_LIMIT_REQUESTS=60
PROFILE_API_RATE_LIMIT_WINDOW_SECONDS=60
```

**Rate Limit Response:**

```json
{
  "success": false,
  "message": "Rate limit exceeded. Please try again later.",
  "error": "RATE_LIMIT_EXCEEDED",
  "retryAfter": 30
}
```

**Headers:**

```
HTTP/1.1 429 Too Many Requests
Retry-After: 30
```

---

## Endpoints

### 1. Create Profile

**Endpoint:** `POST /api/profiles`

**Description:** Create a new profile (Person or Business type). Returns an `ownerToken` for authentication.

**Request Headers:**

```
Content-Type: application/json
```

**Request Body (Person Profile):**

```json
{
  "slug": "john-doe",
  "name": "John Doe",
  "type": "PERSON",
  "bio": "Software Engineer",
  "isPublic": true,
  "title": "Senior Developer",
  "company": "Tech Corp",
  "skills": ["JavaScript", "Python", "C++"],
  "experienceLevel": "Senior",
  "education": "Computer Science",
  "school": "MIT",
  "linkedinUrl": "https://linkedin.com/in/johndoe",
  "githubUrl": "https://github.com/johndoe",
  "portfolioUrl": "https://johndoe.com",
  "email": "john@example.com",
  "phone": "+1234567890"
}
```

**Request Body (Business Profile):**

```json
{
  "slug": "tech-corp",
  "name": "Tech Corp",
  "type": "BUSINESS",
  "bio": "Innovative Technology Company",
  "isPublic": true,
  "companyName": "Tech Corporation Ltd.",
  "industry": "Technology",
  "companySize": "51-200",
  "foundedYear": 2010,
  "address": "123 Tech Street",
  "city": "San Francisco",
  "country": "USA",
  "website": "https://techcorp.com",
  "description": "We build innovative software",
  "services": ["Web Development", "Mobile Apps"],
  "businessEmail": "info@techcorp.com",
  "businessPhone": "+1234567890"
}
```

**Success Response:**

```json
{
  "success": true,
  "message": "Profile stored successfully",
  "ownerToken": "1a2b3c4d5e6f7g8h9i0j...",
  "data": {
    "id": "507f1f77bcf86cd799439011",
    "slug": "john-doe",
    "name": "John Doe",
    "type": "PERSON",
    "isPublic": true,
    "bio": "Software Engineer",
    "createdAt": "2026-02-10T12:00:00Z"
  }
}
```

**Note:** Save the `ownerToken` - it's only returned once on creation and is required for updates/deletes.

**Error Responses:**

_Validation Error:_

```json
{
  "success": false,
  "message": "Validation failed",
  "errors": {
    "slug": "Slug is required",
    "name": "Name is required"
  }
}
```

_Duplicate Slug:_

```json
{
  "success": false,
  "message": "Slug 'john-doe' is already taken."
}
```

---

### 2. Get Profile by ID

**Endpoint:** `GET /api/profiles/:id`

**Description:** Retrieve a profile by its ID.

**Success Response:**

```json
{
  "success": true,
  "message": "Profile found",
  "data": {
    "id": "507f1f77bcf86cd799439011",
    "slug": "john-doe",
    "name": "John Doe",
    "type": "PERSON",
    "isPublic": true,
    "bio": "Software Engineer",
    "createdAt": "2026-02-10T12:00:00Z",
    "updatedAt": "2026-02-10T14:30:00Z"
  }
}
```

**Error Response:**

```json
{
  "success": false,
  "message": "Profile not found"
}
```

---

### 3. Get Public Profile by Slug

**Endpoints:**

- `GET /profiles/:slug`
- `GET /:slug` (root-level route)

**Description:** Retrieve a public profile by its slug. Tracks view analytics and SEO redirects for old slugs.

**Example:** `GET /profiles/john-doe`

**Success Response:**

```json
{
  "success": true,
  "message": "Profile found",
  "data": {
    "id": "507f1f77bcf86cd799439011",
    "slug": "john-doe",
    "name": "John Doe",
    "type": "PERSON",
    "isPublic": true,
    "bio": "Software Engineer",
    "createdAt": "2026-02-10T12:00:00Z",
    "updatedAt": "2026-02-10T14:30:00Z"
  }
}
```

**SEO Redirect:** If slug has changed, returns 301 redirect to new slug.

---

### 4. Update Profile

**Endpoint:** `PUT /api/profiles/:id`

**Description:** Update an existing profile (partial update supported).

**Authentication:** Required if profile has `ownerToken`

**Request Headers:**

```
Content-Type: application/json
Authorization: Bearer <owner_token>
```

**Request Body (Partial Update):**

```json
{
  "name": "John Doe Updated",
  "bio": "Senior Software Engineer"
}
```

**Success Response:**

```json
{
  "success": true,
  "message": "Profile updated successfully",
  "data": {
    "id": "507f1f77bcf86cd799439011",
    "slug": "john-doe",
    "name": "John Doe Updated",
    "type": "PERSON",
    "bio": "Senior Software Engineer",
    "createdAt": "2026-02-10T12:00:00Z",
    "updatedAt": "2026-02-10T15:00:00Z"
  }
}
```

**Error Responses:**

_Forbidden (No/Invalid Token):_

```json
{
  "success": false,
  "message": "Forbidden: You don't have permission to update this profile",
  "error": "FORBIDDEN"
}
```

_Validation Error:_

```json
{
  "success": false,
  "message": "Validation failed",
  "errors": {
    "slug": "Invalid slug format"
  }
}
```

---

### 5. Delete Profile (Soft Delete)

**Endpoint:** `DELETE /api/profiles/:id`

**Description:** Soft delete a profile (can be restored later).

**Authentication:** Required if profile has `ownerToken`

**Request Headers:**

```
Authorization: Bearer <owner_token>
```

**Success Response:**

```
HTTP/1.1 204 No Content
```

**Error Responses:**

_Forbidden:_

```json
{
  "success": false,
  "message": "Forbidden: You don't have permission to delete this profile",
  "error": "FORBIDDEN"
}
```

_Not Found:_

```json
{
  "success": false,
  "message": "Profile not found"
}
```

---

### 6. Restore Profile

**Endpoint:** `POST /api/profiles/:id/restore`

**Description:** Restore a soft-deleted profile.

**Authentication:** Required if profile has `ownerToken`

**Request Headers:**

```
Authorization: Bearer <owner_token>
```

**Success Response:**

```json
{
  "success": true,
  "message": "Profile restored successfully"
}
```

**Error Response:**

```json
{
  "success": false,
  "message": "No profile found with given ID or profile was not deleted"
}
```

---

### 7. List Profiles

**Endpoint:** `GET /api/profiles`

**Description:** List profiles with pagination and filtering.

**Query Parameters:**

- `limit` - Number of profiles per page (1-100, default: 50)
- `skip` - Number of profiles to skip (default: 0)
- `type` - Filter by type: `PERSON` or `BUSINESS`

**Example:** `GET /api/profiles?limit=10&skip=0&type=PERSON`

**Success Response:**

```json
{
  "success": true,
  "message": "Found 10 profiles",
  "data": [
    {
      "id": "507f1f77bcf86cd799439011",
      "slug": "john-doe",
      "name": "John Doe",
      "type": "PERSON",
      "isPublic": true,
      "createdAt": "2026-02-10T12:00:00Z"
    }
  ]
}
```

---

### 8. Check Slug Availability

**Endpoint:** `GET /api/profiles/check-slug`

**Description:** Check if a slug is available for use.

**Query Parameters:**

- `slug` - The slug to check (required)

**Example:** `GET /api/profiles/check-slug?slug=john-doe`

**Success Response (Available):**

```json
{
  "success": true,
  "available": true,
  "message": "Slug is available"
}
```

**Success Response (Taken):**

```json
{
  "success": true,
  "available": false,
  "message": "Slug is already taken"
}
```

---

### 9. Change Slug

**Endpoint:** `POST /api/profiles/:id/change-slug`

**Description:** Change a profile's slug (maintains SEO redirect from old slug).

**Authentication:** Required if profile has `ownerToken`

**Request Headers:**

```
Content-Type: application/json
Authorization: Bearer <owner_token>
```

**Request Body:**

```json
{
  "slug": "new-slug"
}
```

**Success Response:**

```json
{
  "success": true,
  "message": "Slug updated successfully. Old slug will redirect to new slug."
}
```

**Error Responses:**

_Forbidden:_

```json
{
  "success": false,
  "message": "Forbidden: You don't have permission to change this profile's slug",
  "error": "FORBIDDEN"
}
```

_Slug Taken:_

```json
{
  "success": false,
  "message": "Slug 'new-slug' is already taken by another profile."
}
```

---

### 10. Privacy Dashboard

**Endpoint:** `GET /api/profiles/:id/privacy-dashboard`

**Description:** Get privacy and analytics dashboard for profile owners.

**Success Response:**

```json
{
  "success": true,
  "data": {
    "profileId": "507f1f77bcf86cd799439011",
    "totalViews": 1234,
    "recentActivity": {
      "last30Days": 456,
      "last7Days": 123
    },
    "dataRetention": {
      "analytics": "30 days",
      "compliance": "90 days"
    },
    "legalRequests": 0
  }
}
```

---

## Example cURL Requests

### Create Profile

```bash
curl -X POST http://localhost:3000/api/profiles \
  -H "Content-Type: application/json" \
  -d '{
    "slug": "john-doe",
    "name": "John Doe",
    "type": "PERSON",
    "bio": "Software Engineer"
  }'
```

### Update Profile

```bash
curl -X PUT http://localhost:3000/api/profiles/507f1f77bcf86cd799439011 \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer your_token_here" \
  -d '{
    "bio": "Senior Software Engineer"
  }'
```

### Delete Profile

```bash
curl -X DELETE http://localhost:3000/api/profiles/507f1f77bcf86cd799439011 \
  -H "Authorization: Bearer your_token_here"
```

### Restore Profile

```bash
curl -X POST http://localhost:3000/api/profiles/507f1f77bcf86cd799439011/restore \
  -H "Authorization: Bearer your_token_here"
```

---

## HTTP Status Codes

- `200 OK` - Request successful
- `201 Created` - Profile created successfully
- `204 No Content` - Profile deleted successfully
- `400 Bad Request` - Invalid request data
- `403 Forbidden` - Authentication required or failed
- `404 Not Found` - Profile not found
- `429 Too Many Requests` - Rate limit exceeded
- `500 Internal Server Error` - Server error

---

## Data Models

### Profile Types

- `PERSON` - Individual person profile
- `BUSINESS` - Business/company profile

### Common Fields

- `id` - Unique profile identifier (MongoDB ObjectId)
- `slug` - URL-friendly identifier (supports Persian and English)
- `name` - Display name
- `type` - Profile type (PERSON or BUSINESS)
- `bio` - Optional short description
- `isPublic` - Public visibility (default: true)
- `createdAt` - Creation timestamp (ISO 8601)
- `updatedAt` - Last update timestamp (ISO 8601)
- `deletedAt` - Soft delete timestamp (ISO 8601, if deleted)

### Person Profile Specific Fields

- `title` - Job title
- `company` - Current company
- `skills` - Array of skills
- `experienceLevel` - Entry/Mid/Senior/Executive
- `education` - Degree/field of study
- `school` - University/school name
- `linkedinUrl` - LinkedIn profile URL
- `githubUrl` - GitHub profile URL
- `portfolioUrl` - Personal website URL
- `email` - Contact email (encrypted at rest)
- `phone` - Phone number (encrypted at rest)

### Business Profile Specific Fields

- `companyName` - Official company name
- `industry` - Industry category
- `companySize` - 1-10, 11-50, 51-200, 201-1000, 1000+
- `foundedYear` - Year founded
- `address` - Business address (encrypted at rest)
- `city` - City
- `country` - Country
- `website` - Company website
- `description` - Company description
- `services` - Array of services offered
- `businessEmail` - Business email (encrypted at rest)
- `businessPhone` - Business phone (encrypted at rest)

---

## Notes

- Slugs support Persian and English characters, numbers, and hyphens
- Sensitive fields (email, phone, address) are encrypted at rest using AES-256
- Profile views are tracked for analytics (IP addresses not stored in analytics)
- Old slugs automatically redirect to new slugs (301 redirect)
- Soft-deleted profiles are excluded from all read operations
- Rate limits apply per IP address
