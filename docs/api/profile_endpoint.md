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

**Ownership Enforcement:** All profiles require a valid `ownerToken` for protected operations. Profiles created before the token system cannot be modified without a token (no backward-compatibility bypass).

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

Unauthenticated personal-profile responses omit fields hidden by header privacy
settings. Private profiles return 403 unless the request includes the owner's
Bearer token or `x-profile-token`. Successful ID responses use
`Cache-Control: private, no-store`.

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

Canonical slugs contain 1–100 Unicode characters: Persian or English letters,
Persian or English digits, and dots, for example `/هاتف.رستمخانی`. Leading/trailing
dots, consecutive dots, and dot/hyphen or dot/underscore adjacency are rejected.
Creation normalizes legacy hyphen/underscore separators to dots; name generation
also collapses whitespace to a single dot. Errors include English and Persian text.
Percent-encoded path segments are decoded once; malformed encoding and invalid
slugs return 400. Reserved slugs return 404. Both public route aliases redirect
hyphen/underscore requests with HTTP 301 when the dotted profile exists. Existing
stored legacy slugs remain readable when no dotted counterpart exists; no profiles
are automatically renamed.

Requests with `Accept: application/json` receive the existing response envelope
with extended personal fields in `data`. Other requests receive a Persian RTL
page with a responsive cover/avatar header and leveled skills. Public HTML,
JSON, and JSON-LD omit email, phone, location, and availability when hidden by
their corresponding privacy flags. Ownership credentials are never included.

Public profile responses use `Cache-Control: no-cache, must-revalidate` and
`Vary: Accept`. Private profiles return 403 and missing profiles return 404.
The header uses the preferred display name when supplied, falls back to the
stored name, and uses legacy skills only when leveled skills are absent.
Missing or failed images reveal placeholders.

See [profile header verification](../../tests/profile/README.md) for focused,
browser, and HTTP integration tests.

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

**Authentication:** Required (ownership verified via `ownerToken`)

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

**Error Responses:**

_Forbidden:_

```json
{
  "success": false,
  "message": "Forbidden: You don't have permission to restore this profile",
  "error": "FORBIDDEN"
}
```

_Not Found:_

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

**Success Response (Reserved):**

```json
{
  "success": true,
  "available": false,
  "message": "This slug is reserved and cannot be used"
}
```

**Notes:**

- Reserved system slugs (api, admin, search, etc.) are reported as unavailable
- Soft-deleted profiles do not block slug reuse

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

**Authentication:** Required (ownership verified via `ownerToken`)

**Request Headers:**

```
Authorization: Bearer <owner_token>
```

\*\*Success Response:

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
- `301 Moved Permanently` - SEO redirect to current slug (old slug in `previousSlugs`)
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

- Canonical slugs use Persian/English letters and digits with dots (1–100 Unicode characters).
- Reserved system slugs (api, admin, search, etc.) are blocked from all write operations
- Sensitive fields (email, phone, address) are encrypted at rest using AES-256
- Profile views are tracked for analytics (IP addresses not stored in analytics)
- Old slugs automatically redirect to new slugs (301 redirect via indexed `previousSlugs` lookup)
- Soft-deleted profiles are excluded from all read operations and slug availability checks
- Rate limits apply per IP address (including public profile and slug routes)
- Owner tokens are generated using cryptographically secure random numbers (`std::random_device`)
- Ownership is enforced on all protected operations; missing tokens result in denial (no backward-compat bypass)
- TOCTOU protection: duplicate slug insertion is caught via MongoDB E11000 error handling

## Personal editor and private drafts

Personal creation now starts as a private draft. Send `type: "PERSON"`, a free
`slug`, and `isPublic: false` to `POST /api/profiles`. `name` may be empty or a
single Persian character. An omitted personal `isPublic` defaults to false;
creating an already-public personal profile returns 400. Publication is a
subsequent authenticated update. Business creation retains its existing default.

A successful creation returns the existing JSON envelope with `data`, `canEdit`,
and a one-time `ownerToken` containing a cryptographically random 64-character
hex key. New keys are stored only as SHA-256 hashes. Existing plaintext owner
keys still authenticate. Neither raw keys nor hashes appear in profile data.

`POST /api/profiles/:id/session` accepts `{ "key": "…" }` with a matching
`Origin` header. It sets the same per-profile cookie that creation sets:
`HttpOnly`, `SameSite=Strict`, `Max-Age=2592000`, path `/api/profiles/:id`.
Configure `BASE_URL` with `https://` when served through HTTPS to enable `Secure`.
`DELETE` on that session URL expires the cookie and also requires a matching
origin. Bearer and `X-Profile-Token` authentication remain available. Cookie
mutations require an origin matching the request host and configured scheme.
Editor HTML and authentication responses use `Cache-Control: private, no-store`.

Owner reads at `GET /api/profiles/:id` include `canEdit: true` and `data.version`.
Private reads require ownership. The personal `PUT` endpoint accepts only the
supplied fields from this list:

- `name`, `title`, `company`, `bio`, `location`, `availabilityStatus`
- `skillsWithLevel` (array of `{name, level}`), `isPublic`
- `avatarUrl` and `coverImageUrl` only as empty strings to remove images
- required integer `version` from the last acknowledged owner response

The editor has no English-name field. Existing English names, privacy settings,
contact details, and credentials are preserved by partial updates. Strings and
skills arrays may be cleared. Repeating an unchanged value succeeds. Limits
count Unicode code points: name/title/company/location 200, bio 500, skill name
80, and at most 50 skills. Publishing requires a nonempty Persian name.

The database atomically compares `version` and increments it on success.
A stale version returns 409 and requires explicit reconciliation. Autosave has
a separate authenticated limit of 120 updates/minute per profile owner; 429
includes `Retry-After`. Saved changes after publication update the public page
immediately. `POST /:id/avatar`, `POST /:id/cover`, and `POST /:id/skills` also
require `version`; image responses return the new version. The legacy skill
DELETE uses the numeric version in `If-Match`.

The UI is available at `/profiles/new?slug=…` and `/profiles/:slug/edit`.
Opening a free slug returns an invitation with HTTP 404 and `noindex`; a GET
never creates a record. Creation begins with the explicit start button.
Unacknowledged form changes are kept locally per tab, reconciled on conflicts,
and cleared on owner logout. Keep the displayed access key: this milestone
has no account signup or lost-key recovery.

## Images attached to experiences and projects

The owner can attach up to 10 static JPEG, PNG or WebP images (5 MiB each)
to an individual work experience or project. Both use the same versioned contract:

- `POST /api/profiles/:id/experiences/:itemId/media`
- `POST /api/profiles/:id/projects/:itemId/media`

Body: `{ "version": 7, "image": "data:image/png;base64,...", "alt": "Deployment diagram" }`.
The response returns the updated owner profile and its incremented version.
Images are decoded, normalized to WebP, scaled to at most 1600 px and stripped of
metadata. Animated images and excessive dimensions are rejected.

Each item's `media` array contains `{id, alt}` objects. To edit captions, reorder
or remove images, submit this array through the existing
`PUT /api/profiles/:id/content/:section/:itemId` endpoint with the current version.
Only images already belonging to that exact item may appear in the array.
Copying an item does not copy its uploaded images. Captions are limited to 300
Unicode characters. Removing a reference or deleting the item removes its stored file.

`GET /api/profiles/:id/media/:mediaId` checks access on every request and uses
`Cache-Control: private, no-store`. Owners can view their private images; visitors
can only retrieve images whose profile, section and owning item are public.
Hidden or removed images return 404, including when their URL is known.

The basic editor also accepts `tagline`, an optional 120-character value statement
shown below the professional title. It is separate from the full introduction.

The public page and owner preview share the same expanded presentation. Related
images, captions, project outcomes, readable date ranges and all populated fields
remain visible. The section index includes all displayed sections; on smaller
screens it becomes a keyboard-accessible menu.

### Content validation and independent saves

Content mutation errors retain HTTP 400 and `error.code: "BAD_REQUEST"`.
They also identify the requested `section` and `itemId`; `field` is included when
the validator identifies a specific field. For example, clearing the name of a
public certification returns:

```json
{
  "error": {
    "code": "BAD_REQUEST",
    "message": "نام گواهی‌نامه را برای نمایش عمومی وارد کنید.",
    "section": "certifications",
    "itemId": "certificate-regression",
    "field": "name"
  }
}
```

A rejected mutation does not change the stored item or advance its version.
The editor retains the rejected draft locally, marks the affected item, and
continues saving independent operations. Successful operations are acknowledged
individually. Invalid drafts are retried after editing or an explicit retry.
The last valid public item remains visible; invalid items are not automatically
hidden. HTTP 409 still requires reconciliation.

Avatar/cover upload and removal share the profile's serialized version sequence,
but do not require every content draft to save first. Upload responses remain
partial objects containing the media URL, version, size and MIME type; clients
must merge these fields rather than replace the complete profile.

Avatar and cover files are persisted in the application container's
`/app/uploads` volume. See [upload migration](../operations/profile-uploads.md)
before replacing a legacy container that has files in its writable layer.
