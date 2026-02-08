# Profile Database Schema Documentation

**Version:** 1.0
**Last Updated:** 2026-02-09
**Database:** MongoDB (search-engine)

## Overview

This document describes the MongoDB database schema for the profile system, including all collections, fields, indexes, and their purposes. The profile system implements a privacy-first architecture with multiple storage tiers for different data sensitivity levels.

## Collections

### 1. profiles

**Purpose:** Core profile data for both person and business profiles.

**Collection Name:** `profiles`

#### Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `_id` | ObjectId | Yes | MongoDB auto-generated unique identifier |
| `slug` | String | Yes | URL-friendly identifier (supports Persian + English + numbers + hyphens) |
| `name` | String | Yes | Display name (full Unicode support) |
| `type` | String | Yes | Profile type: "PERSON" or "BUSINESS" |
| `bio` | String | No | Short description (max 500 characters) |
| `isPublic` | Boolean | Yes | Public visibility flag (default: true) |
| `previousSlugs` | Array[String] | No | History of previous slugs for SEO redirects |
| `slugChangedAt` | Date | No | Timestamp of last slug change |
| `createdAt` | Date | Yes | Profile creation timestamp |

**Extended Fields (PersonProfile):**
- `title`: Job title
- `company`: Current company
- `skills`: Array of skill strings
- `experienceLevel`: "Entry", "Mid", "Senior", "Executive"
- `education`: Degree/field of study
- `school`: University/school name
- `linkedinUrl`: LinkedIn profile URL
- `githubUrl`: GitHub profile URL
- `portfolioUrl`: Personal website/portfolio URL
- `email`: Contact email (will be encrypted in future)
- `phone`: Phone number (international format)

**Extended Fields (BusinessProfile):**
- `companyName`: Official company name (required for business profiles)
- `industry`: Industry category (validated against allowed list)
- `companySize`: "1-10", "11-50", "51-200", "201-1000", "1000+"
- `foundedYear`: Year company was founded
- `address`: Business address
- `city`: City
- `country`: Country
- `website`: Company website URL
- `description`: Company description
- `services`: Array of services offered
- `businessEmail`: Business email
- `businessPhone`: Business phone number

#### Indexes

| Index Name | Keys | Type | Purpose |
|------------|------|------|---------|
| `slug_unique` | `{ "slug": 1 }` | Unique | Fast slug lookups, prevent duplicates |
| `type_index` | `{ "type": 1 }` | Regular | Filter profiles by type |
| `created_at_index` | `{ "createdAt": -1 }` | Regular | Sort profiles by creation date (recent first) |
| `public_filter` | `{ "isPublic": 1 }` | Regular | Filter for public profiles |
| `type_public_recent` | `{ "type": 1, "isPublic": 1, "createdAt": -1 }` | Compound | List public profiles by type, sorted by recency |
| `person_skills` | `{ "skills": 1 }` | Partial (type=PERSON) | Search by skills for person profiles |
| `business_location_industry` | `{ "industry": 1, "city": 1 }` | Partial (type=BUSINESS) | Search businesses by industry and location |

**Query Patterns:**
- Get profile by slug: `db.profiles.find({ slug: "john-doe" })`
- List public person profiles: `db.profiles.find({ type: "PERSON", isPublic: true }).sort({ createdAt: -1 })`
- Search person by skills: `db.profiles.find({ type: "PERSON", skills: "C++" })`
- Search business by location: `db.profiles.find({ type: "BUSINESS", industry: "Technology", city: "Tehran" })`

---

### 2. profile_view_analytics

**Purpose:** Tier 1 Analytics - Privacy-first profile view tracking (NO IP addresses).

**Collection Name:** `profile_view_analytics`

#### Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `viewId` | String | Yes | Unique view identifier (links to Tier 2 compliance if needed) |
| `profileId` | String | Yes | Profile being viewed |
| `timestamp` | Date | Yes | View timestamp |
| `country` | String | Yes | Country (city-level only, default "Unknown") |
| `province` | String | Yes | Province/state (default "Unknown") |
| `city` | String | Yes | City (default "Unknown") |
| `browser` | String | Yes | Browser name only (e.g., "Chrome", no version) |
| `os` | String | Yes | OS name only (e.g., "Linux", no version) |
| `deviceType` | String | Yes | "Mobile", "Tablet", or "Desktop" |

#### Indexes

| Index Name | Keys | Type | Purpose |
|------------|------|------|---------|
| `profile_views_timeline` | `{ "profileId": 1, "timestamp": -1 }` | Compound | Dashboard queries (recent views for profile) |
| `location_analytics` | `{ "city": 1, "timestamp": -1 }` | Compound | Geographic analytics |
| `device_analytics` | `{ "deviceType": 1, "timestamp": -1 }` | Compound | Device analytics |
| (unnamed) | `{ "timestamp": -1 }` | Regular | Cleanup/aggregation queries |

**Query Patterns:**
- Get recent views for profile: `db.profile_view_analytics.find({ profileId: "..." }).sort({ timestamp: -1 }).limit(30)`
- Views by city: `db.profile_view_analytics.find({ city: "Tehran" }).sort({ timestamp: -1 })`
- Views by device: `db.profile_view_analytics.aggregate([{ $match: { deviceType: "Mobile" }}, { $group: { _id: "$profileId", count: { $sum: 1 }}}])`

---

### 3. legal_compliance_logs

**Purpose:** Tier 2 Compliance - Legal requirement tracking with encrypted sensitive data.

**Collection Name:** `legal_compliance_logs`

#### Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `logId` | String | Yes | Unique log identifier |
| `userId` | String | Yes | Profile owner or viewer ID |
| `timestamp` | Date | Yes | Log timestamp |
| `ipAddress_encrypted` | String | Yes | AES-256-GCM encrypted IP address |
| `userAgent_encrypted` | String | Yes | AES-256-GCM encrypted user agent |
| `referrer_encrypted` | String | Yes | AES-256-GCM encrypted referrer URL |
| `viewId` | String | Yes | Link to Tier 1 analytics view |
| `retentionExpiry` | Date | Yes | Auto-deletion date (timestamp + 12 months) |
| `isUnderInvestigation` | Boolean | Yes | If true, don't auto-delete |

#### Indexes

| Index Name | Keys | Type | Purpose |
|------------|------|------|---------|
| `user_compliance_history` | `{ "userId": 1, "timestamp": -1 }` | Compound | User compliance history queries |
| `auto_deletion_index` | `{ "retentionExpiry": 1 }` | Regular | Find logs ready for auto-deletion |
| `analytics_link` | `{ "viewId": 1 }` | Regular | Link to Tier 1 analytics |
| (unnamed) | `{ "timestamp": -1 }` | Regular | Audit queries |

**Security:**
- All sensitive fields encrypted with AES-256-GCM
- Encryption key loaded from `COMPLIANCE_ENCRYPTION_KEY` environment variable
- Access restricted to internal APIs with API key
- Auto-deletion after 12 months (unless under investigation)

**Query Patterns:**
- Get compliance logs for user: `db.legal_compliance_logs.find({ userId: "..." }).sort({ timestamp: -1 })`
- Find expired logs: `db.legal_compliance_logs.find({ retentionExpiry: { $lt: new Date() }, isUnderInvestigation: false })`

---

### 4. profile_audit_logs

**Purpose:** Audit trail for profile CRUD and view operations.

**Collection Name:** `profile_audit_logs`

#### Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `_id` | String | Yes | Unique audit log ID |
| `timestamp` | Date | Yes | Action timestamp |
| `action` | String | Yes | "CREATE", "UPDATE", "DELETE", or "VIEW" |
| `resourceType` | String | Yes | Resource type (always "profile") |
| `resourceId` | String | Yes | Profile ID affected |
| `userId` | String | Yes | User who performed action (or "anonymous") |
| `ipAddress` | String | Yes | IP address of requester |
| `userAgent` | String | Yes | User agent string |
| `oldValue` | String | No | JSON of old state (for UPDATE) |
| `newValue` | String | No | JSON of new state (for CREATE/UPDATE) |
| `reason` | String | No | Reason for action |
| `sessionId` | String | No | Session ID if available |
| `apiVersion` | String | Yes | API version (e.g., "v1") |
| `isAutomated` | Boolean | Yes | Was action done by system? |

#### Indexes

| Index Name | Keys | Type | Purpose |
|------------|------|------|---------|
| `audit_resource_timeline` | `{ "resourceId": 1, "timestamp": -1 }` | Compound | Audit trail for specific profile |
| `audit_user_timeline` | `{ "userId": 1, "timestamp": -1 }` | Compound | Actions by specific user |
| `audit_action` | `{ "action": 1 }` | Regular | Filter by action type |

**Query Patterns:**
- Get audit logs for profile: `db.profile_audit_logs.find({ resourceId: "..." }).sort({ timestamp: -1 })`
- Get audit logs for user: `db.profile_audit_logs.find({ userId: "..." }).sort({ timestamp: -1 })`
- Get all deletions: `db.profile_audit_logs.find({ action: "DELETE" })`

---

## Data Flow

```
┌─────────────────┐
│  User Request   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Validation     │ ◄─── ProfileValidator
│  (Email, Phone, │
│   URL, Business)│
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   profiles      │ ◄─── ProfileStorage
│   (Core Data)   │
└────────┬────────┘
         │
         ├──────────────────────────┐
         │                          │
         ▼                          ▼
┌──────────────────┐     ┌──────────────────────┐
│ profile_view_    │     │  profile_audit_logs  │
│ analytics        │     │  (CRUD/View Audit)   │
│ (Privacy-First)  │     └──────────────────────┘
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│ legal_compliance │
│ _logs            │
│ (Encrypted IP)   │
└──────────────────┘
```

## Validation Rules

### Slug Validation
- **Pattern:** Persian letters (U+0600–U+06FF) + English letters + numbers + hyphens
- **Length:** 1-50 characters
- **Warnings:** Double hyphens (`--`) trigger a warning

### Email Validation
- **Pattern:** RFC 5322 simplified: `[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}`
- **Length:** Max 254 characters

### Phone Validation
- **Pattern:** International format: `^\+\d{1,4}\d{6,14}$`
- **Example:** +989123456789, +14155551234

### URL Validation
- **Pattern:** `^https?://[a-zA-Z0-9\-._~:/?#\[\]@!$&'()*+,;=%]+$`
- **Length:** Max 2048 characters

### Business-Specific Validation
- **companyName:** Required for business profiles, max 200 characters
- **industry:** Must be from allowed list (Technology, Healthcare, Finance, Education, Manufacturing, Retail, Food, Construction, Real Estate, Transportation, Hospitality, Media, Entertainment, Consulting, Legal, Marketing, Agriculture, Energy, Telecommunications, Other)
- **foundedYear:** Must be between 1800 and (current year + 1)
- **companySize:** Must be one of: "1-10", "11-50", "51-200", "201-1000", "1000+"

## Performance Targets

| Operation | Target | Actual Index |
|-----------|--------|--------------|
| Slug lookup (100 queries) | < 100ms | slug_unique |
| Profile ID lookup | < 50ms | _id (default) |
| Recent analytics views | < 200ms | profile_views_timeline |
| Analytics count | < 100ms | profile_views_timeline |
| Audit trail query | < 100ms | audit_resource_timeline |

## Maintenance

### Index Management

**Check index usage:**
```javascript
db.profiles.aggregate([{ $indexStats: {} }])
```

**List all indexes:**
```javascript
db.profiles.getIndexes()
db.profile_view_analytics.getIndexes()
db.legal_compliance_logs.getIndexes()
db.profile_audit_logs.getIndexes()
```

**Monitor index sizes:**
```javascript
db.profiles.stats().indexSizes
```

### Data Retention

| Collection | Retention Policy |
|------------|------------------|
| profiles | Indefinite (user-controlled deletion) |
| profile_view_analytics | Indefinite (for profile owner dashboard) |
| legal_compliance_logs | 12 months auto-deletion (unless under investigation) |
| profile_audit_logs | Indefinite (for compliance and debugging) |

### Backup Strategy

- **Frequency:** Daily full backup + hourly incremental
- **Retention:** 30 days
- **Encryption:** All backups encrypted at rest
- **Testing:** Monthly restore tests

## Privacy Architecture

The profile system implements a tiered privacy architecture:

1. **Tier 1 (profile_view_analytics):** Privacy-first analytics with NO IP addresses, only city-level geo data
2. **Tier 2 (legal_compliance_logs):** Encrypted compliance data for legal requirements
3. **Tier 3 (profile_audit_logs):** Audit trail for operations and debugging

See [docs/privacy/](../privacy/) for detailed privacy architecture documentation.

## Migration Notes

### Version History

- **v1.0 (2026-02-09):** Initial schema with named indexes, validation, audit logging, and performance tests

### Future Enhancements

- Add TTL index for analytics cleanup (optional)
- Implement email/phone encryption (as designed in privacy architecture)
- Add full-text search index on profile name and bio
- Consider sharding strategy for high-scale deployments (> 10M profiles)

## Related Documentation

- [Privacy Architecture](../privacy/PRIVACY_ARCHITECTURE.md)
- [Legal Vault Protocol](../privacy/LEGAL_VAULT_PROTOCOL.md)
- [MongoDB C++ Driver Guide](../development/MONGODB_CPP_GUIDE.md)
- [Content Storage Layer](content-storage-layer.md)
