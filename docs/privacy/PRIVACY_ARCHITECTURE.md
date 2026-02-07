# Privacy Architecture - Three-Tier System

## Overview

Our search engine implements a revolutionary **three-tier privacy architecture** that separates user analytics from legal compliance data, providing **unprecedented transparency** while maintaining **complete legal compliance**.

## The Challenge

Local law requires retaining IP addresses for potential legal investigations, **BUT** users deserve privacy and we don't need IPs for analytics.

## Our Solution: Complete Data Separation

```
┌─────────────────────────────────────────┐
│   TIER 1: Analytics Database            │
│   (What Users See)                      │
│   ❌ NO IP addresses stored             │
│   ✅ City-level geo data only           │
│   ✅ Browser/device (generic)           │
│   Purpose: User insights & dashboard    │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│   TIER 2: Compliance Database           │
│   (Encrypted & Isolated)                │
│   ✅ IP encrypted with AES-256          │
│   ✅ Auto-delete after 12 months        │
│   ✅ API key access required            │
│   Purpose: Legal compliance             │
└─────────────────────────────────────────┘
              ↓ Air Gap (Physical)
┌─────────────────────────────────────────┐
│   TIER 3: Legal Vault                   │
│   (Court Orders Only)                   │
│   ✅ Original IP (unencrypted)          │
│   ✅ Requires CEO + Legal Officer       │
│   ✅ Full audit trail                   │
│   Purpose: Valid court requests only    │
└─────────────────────────────────────────┘
```

## Tier 1: Privacy-First Analytics

### What Gets Stored (NO IP!)

```json
{
  "viewId": "1639392000000-1234",
  "profileId": "507f1f77bcf86cd799439011",
  "timestamp": "2024-06-15T14:23:11Z",
  "country": "Iran",
  "province": "Tehran",
  "city": "Tehran",
  "browser": "Chrome",
  "os": "Android",
  "deviceType": "Mobile"
}
```

### What Users See

Privacy dashboard shows:
- **45 views from Tehran this month**
- **78% from mobile devices**
- **Top referrer: Google Search**
- **Peak viewing hours: 2-4 PM**

**Zero personal identification data visible.**

### Collection: `profile_view_analytics`

**Indexes:**
- `profileId` + `timestamp` (for dashboard queries)
- `timestamp` (for aggregations)

**Retention:** 2 years (user configurable in future)

## Tier 2: Encrypted Compliance Data

### What Gets Stored (ALL ENCRYPTED)

```json
{
  "logId": "1639392000000-1234-compliance",
  "userId": "anonymous",
  "timestamp": "2024-06-15T14:23:11Z",
  "ipAddress_encrypted": "AES256_ENCRYPTED_DATA...",
  "userAgent_encrypted": "AES256_ENCRYPTED_DATA...",
  "referrer_encrypted": "AES256_ENCRYPTED_DATA...",
  "viewId": "1639392000000-1234",
  "retentionExpiry": "2025-06-15T14:23:11Z",
  "isUnderInvestigation": false
}
```

### Security Measures

- **Encryption:** AES-256-GCM with authenticated encryption
- **Key Management:** Environment variable (rotate regularly)
- **Access Control:** Protected API key required
- **Audit Trail:** All access logged
- **Auto-Deletion:** 12 months retention (configurable)

### Collection: `legal_compliance_logs`

**Indexes:**
- `retentionExpiry` (for auto-deletion)
- `timestamp` (for audit queries)
- `viewId` (link to Tier 1 analytics)

**Retention:** 12 months (auto-deleted unless under investigation)

### Auto-Deletion Job

```bash
# Run daily via cron
curl -X POST http://localhost:3000/api/internal/compliance/cleanup \
  -H "x-api-key: YOUR_INTERNAL_API_KEY"
```

**Response:**
```json
{
  "success": true,
  "message": "Compliance logs cleanup completed",
  "data": {
    "expiredLogsFound": 1247,
    "logsDeleted": 1247,
    "timestamp": 1639392000000
  }
}
```

## Tier 3: Legal Vault

See [LEGAL_VAULT_PROTOCOL.md](./LEGAL_VAULT_PROTOCOL.md) for complete details.

**Summary:**
- Physical air-gapped server
- Dual authorization (CEO + Legal Officer)
- Valid court order required
- Complete audit trail
- Secure data destruction after case closure

## Profile PII Encryption

### Sensitive Fields Encrypted at Rest

**PersonProfile:**
- `email` (encrypted)
- `phone` (encrypted)

**BusinessProfile:**
- `businessEmail` (encrypted)
- `businessPhone` (encrypted)
- `address` (encrypted)

### How It Works

```cpp
// Storing profile (automatic encryption)
PersonProfile profile;
profile.email = "user@example.com";  // Plaintext in memory
profile.phone = "+989123456789";

storage->store(profile);  // Automatically encrypts before saving to DB

// Reading profile (automatic decryption)
auto result = storage->findById(profileId);
if (result.success) {
    // result.value.email is decrypted automatically
    std::string email = result.value.email.value();  // "user@example.com"
}
```

### Public Profile Views

When users view a public profile:
- Email/phone are **never** returned to the client
- Only public fields (name, bio, links) are visible
- PII remains encrypted in database
- Only profile owner sees their own PII (future: with authentication)

## Data Flow: Profile View

```mermaid
sequenceDiagram
    participant User
    participant Server
    participant T1 as Tier 1 Analytics
    participant T2 as Tier 2 Compliance
    participant Memory as Secure Memory

    User->>Server: GET /profiles/:slug
    Server->>Server: Extract IP, User-Agent, Referrer
    Server->>Server: GeoIP Lookup (city-level)
    Server->>Server: Parse User-Agent (generic)
    
    Server->>T1: Record Analytics (NO IP!)
    Note over T1: country, province, city<br/>browser, os, deviceType
    
    Server->>Server: Encrypt IP, UA, Referrer (AES-256)
    Server->>T2: Record Compliance Log
    Note over T2: IP encrypted<br/>12-month retention
    
    Server->>Memory: Secure Memory Wipe
    Note over Memory: Overwrite IP/UA<br/>strings with zeros
    
    Server->>User: Return Profile (NO PII)
```

## Privacy Dashboard API

### Endpoint

```
GET /api/profiles/:id/privacy-dashboard
```

### Response

```json
{
  "success": true,
  "data": {
    "profileId": "507f1f77bcf86cd799439011",
    "totalViews": 145,
    "recentActivity": [
      {
        "when": "2024-06-15T14:23:11Z",
        "action": "profile_view",
        "location": "Tehran, Tehran, Iran",
        "device": "Chrome on Android (Mobile)"
      }
    ],
    "dataRetention": {
      "profileData": "Until account deletion",
      "analyticsData": "2 years (730 days)",
      "complianceLogs": "12 months (365 days) - auto-deleted",
      "deletedData": "Immediate (0 days)"
    },
    "userControls": {
      "canExportAllData": true,
      "canDeleteAccount": true,
      "canControlRetention": false
    },
    "legalRequestsCount": 0,
    "privacyLevel": "Maximum",
    "encryptionEnabled": true,
    "ipAddressStored": "Encrypted only (12 months)"
  }
}
```

## Environment Variables

### Required

```bash
# Encryption key for PII and compliance data (MUST be 32 bytes for AES-256)
COMPLIANCE_ENCRYPTION_KEY="0123456789abcdef0123456789abcdef"

# Internal API key for compliance cleanup endpoint
INTERNAL_API_KEY="change-this-secret-key-in-production"
```

### Optional

```bash
# Separate MongoDB instance for Tier 2 (strict isolation)
MONGODB_COMPLIANCE_URI="mongodb://admin:password@compliance-db:27017"
```

## Security Best Practices

### Key Management

1. **Generate strong keys:**
   ```bash
   # Generate 32-byte random key
   openssl rand -hex 32
   ```

2. **Rotate keys regularly:**
   - Compliance encryption key: Every 90 days
   - Internal API key: Every 30 days

3. **Store keys securely:**
   - Use Docker secrets in production
   - Never commit keys to git
   - Use environment variable injection

### Access Control

1. **Tier 1 (Analytics):**
   - Public API for profile owners (future: with auth)
   - No sensitive data exposed

2. **Tier 2 (Compliance):**
   - Internal API only
   - API key required
   - All access audited

3. **Tier 3 (Legal Vault):**
   - Physical access only
   - Dual authorization required
   - Court order mandatory

### Monitoring

1. **Track compliance log growth:**
   ```bash
   # Monitor Tier 2 storage
   curl http://localhost:3000/api/internal/compliance/stats \
     -H "x-api-key: YOUR_KEY"
   ```

2. **Verify auto-deletion:**
   ```bash
   # Check cleanup job logs
   docker logs core | grep "Compliance cleanup"
   ```

3. **Audit access patterns:**
   - Review compliance database access logs daily
   - Alert on unusual access patterns
   - Monitor failed API key attempts

## Testing

### Unit Tests

See `tests/storage/test_privacy_architecture.cpp` (to be implemented)

### Integration Tests

```bash
# Test profile view tracking
curl -X GET http://localhost:3000/profiles/test-profile

# Verify Tier 1 analytics (no IP)
curl http://localhost:3000/api/profiles/:id/privacy-dashboard

# Test compliance cleanup
curl -X POST http://localhost:3000/api/internal/compliance/cleanup \
  -H "x-api-key: YOUR_KEY"
```

### Security Tests

```bash
# Verify encryption
# 1. Store profile with email
# 2. Check MongoDB directly - email should be encrypted
docker exec mongodb_test mongosh --eval "db.profiles.findOne()"

# Verify no IP in analytics
docker exec mongodb_test mongosh --eval "db.profile_view_analytics.findOne()"
# Should NOT contain "ipAddress" field

# Verify encrypted IP in compliance
docker exec mongodb_test mongosh --eval "db.legal_compliance_logs.findOne()"
# "ipAddress_encrypted" should be AES-256 ciphertext
```

## Compliance

### GDPR Compliance

- ✅ **Right to access:** Privacy dashboard provides transparency
- ✅ **Right to erasure:** Account deletion removes all data
- ✅ **Right to data portability:** Export functionality (future)
- ✅ **Privacy by design:** Three-tier architecture
- ✅ **Data minimization:** Only collect what's needed
- ✅ **Storage limitation:** Auto-deletion after retention period

### Local Law Compliance

- ✅ **IP retention:** 12 months in encrypted Tier 2
- ✅ **Legal requests:** Tier 3 vault for court orders
- ✅ **Audit trail:** Complete logging of all access
- ✅ **Data security:** AES-256 encryption at rest

## Future Enhancements

### Phase 2 (Task 01d)
- Performance indexes for encrypted data
- Query optimization for privacy dashboard
- Bulk export functionality

### Phase 3
- User authentication and authorization
- Self-service data export (GDPR)
- Configurable retention periods
- Advanced analytics (still privacy-first)

### Phase 4
- Real GeoIP database integration (MaxMind GeoLite2)
- Enhanced user-agent parsing
- Anomaly detection in compliance logs
- Automated key rotation

## Support

For privacy-related questions:
- Technical: See code in `src/storage/` and `include/search_engine/storage/`
- Legal: Review [LEGAL_VAULT_PROTOCOL.md](./LEGAL_VAULT_PROTOCOL.md)
- Security: Contact security team

---

**Document Version:** 1.0  
**Last Updated:** Task 01c Implementation  
**Next Review:** After Phase 2 implementation
