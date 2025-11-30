# 🚀 Profile Database Models

**Duration:** 3 days
**Dependencies:** MongoDB instance setup
**Acceptance Criteria:**
- ✅ Profile base model with common fields
- ✅ Person profile model extending base
- ✅ Business profile model extending base
- ✅ Profile validation rules implemented
- ✅ Database indexes created for performance
- ✅ Unit tests for all models (85%+ coverage)
- ✅ Privacy-first architecture with minimal data collection
- ✅ IP/Geo separation system implemented
- ✅ Three-tier data storage (Analytics, Compliance, Legal Vault)

## 💎 Why This Feature Exists

### Problem It Solves
Users need a secure, privacy-respecting way to manage their online professional identity while platforms must comply with local legal requirements for data retention. The challenge is balancing user privacy with legal obligations.

### Unique Value for Hatef
Unlike platforms that collect everything for ads, Hatef implements **minimal data collection** with **transparent legal compliance**. We only store what's necessary, encrypt sensitive data, and give users complete visibility and control.

### Success Metric
- Zero privacy violations or data breaches
- 100% legal compliance with local data retention laws
- User trust score >80%
- All sensitive data encrypted
- Auto-deletion working correctly (12-month retention verified)

### Best Practice Applied
**Lesson #5: Privacy & Security** - Failed platforms have had catastrophic data breaches that were hidden for months, destroying user trust. We build privacy-first from day one, with complete transparency and minimal data collection.

## 🎯 Task Description

Create the core database models for the profile system with **privacy-first architecture**. This task establishes the foundation for all profile types (personal and business) with proper MongoDB schema design, validation, and a revolutionary **IP/Geo separation system** that balances user privacy with legal compliance for local markets.

## 🔒 Privacy-First Data Architecture

### Minimal Data Collection Principle
**We only collect what is absolutely necessary.** Every field must answer: "Why do we need this?"

```cpp
// ✅ REQUIRED (Minimal)
struct ProfileBase {
    std::string id;
    std::string slug;
    ProfileType type;
    bool isPublic = false;  // DEFAULT: Private
    Date createdAt;
};

// ✅ OPTIONAL (User Controlled)
struct ProfileOptional {
    std::string name;
    std::string bio;
    std::string avatarUrl;
};

// ✅ ENCRYPTED (Sensitive Data)
struct ProfileSensitive {
    std::string email_encrypted;  // AES-256
    std::string phone_encrypted;  // AES-256
};

// ❌ NEVER STORED
// - IP addresses in profile database
// - Browsing history
// - Third-party tracking data
// - Location beyond city-level
// - Device fingerprints
```

### Data Retention Policy
- **Profile Data**: Until account deletion
- **Technical Logs**: 12 months (auto-delete)
- **Deleted Accounts**: 0 days (immediate secure wipe)
- **Legal Hold Exception**: Only with valid court order

## 🗺️ Revolutionary IP/Geo Separation System

### The Challenge
Local law requires storing IP addresses for potential legal investigations, BUT users deserve privacy and we don't need IPs for analytics.

### Our Solution: Three-Tier Architecture

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
│   ✅ 2-factor access required           │
│   Purpose: Legal compliance             │
└─────────────────────────────────────────┘
              ↓ Air Gap (Physical)
┌─────────────────────────────────────────┐
│   TIER 3: Legal Vault                   │
│   (Court Orders Only)                   │
│   ✅ Original IP (unencrypted)          │
│   ✅ Requires 2 signatures              │
│   ✅ Full audit trail                   │
│   Purpose: Valid court requests only    │
└─────────────────────────────────────────┘
```

### Processing Flow

```cpp
// When user visits a profile:
void handleProfileView(const HttpRequest& req) {
    std::string ip = req.getRemoteIP();
    std::string userAgent = req.getUserAgent();
    
    // Step 1: Extract geo data immediately (city-level only)
    GeoData geo = GeoIPService::lookup(ip);
    // geo = {country: "CountryName", province: "ProvinceName", city: "CityName"}
    
    // Step 2: Store for Analytics (NO IP!)
    ProfileViewAnalytics analytics{
        .viewId = generateUUID(),
        .profileId = req.getProfileId(),
        .timestamp = now(),
        .country = geo.country,
        .province = geo.province,
        .city = geo.city,  // ❌ Not neighborhood!
        .browser = parseBrowser(userAgent),  // "Chrome" not "Chrome 120.0.1"
        .deviceType = parseDeviceType(userAgent)  // "Mobile" not specific model
    };
    analyticsDB.save(analytics);
    
    // Step 3: Store for Legal Compliance (Encrypted)
    LegalComplianceLog legalLog{
        .logId = generateUUID(),
        .userId = req.getUserId(),
        .timestamp = now(),
        .ipAddress_encrypted = AES256_Encrypt(ip, LEGAL_KEY),
        .userAgent_encrypted = AES256_Encrypt(userAgent, LEGAL_KEY),
        .viewId = analytics.viewId,  // Link to analytics
        .retentionExpiry = now() + 12_months
    };
    complianceDB.save(legalLog);
    
    // Step 4: Clear from memory (Security)
    ip.clear();
    userAgent.clear();
    secureMemoryWipe(&ip);
    secureMemoryWipe(&userAgent);
    
    // IP is now ONLY in encrypted compliance DB
}
```

### Database Schemas

```cpp
// ═══════════════════════════════════════
// TIER 1: Analytics (User-Facing)
// ═══════════════════════════════════════
struct ProfileViewAnalytics {
    std::string viewId;
    std::string profileId;
    Date timestamp;
    
    // Geographic (city-level, NO IP)
    std::string country = "CountryName";
    std::string province = "ProvinceName";
    std::string city = "CityName";
    // ❌ No: neighborhood, street, coordinates
    
    // Device (generic)
    std::string browser = "Chrome";  // Not version
    std::string os = "Android";      // Not version
    std::string deviceType = "Mobile";  // Mobile/Desktop/Tablet
    
    // User Dashboard shows:
    // "45 people from CityName viewed your profile"
    // "78% viewed from mobile"
};

// ═══════════════════════════════════════
// TIER 2: Legal Compliance (Encrypted)
// ═══════════════════════════════════════
struct LegalComplianceLog {
    std::string logId;
    std::string userId;
    Date timestamp;
    
    // Encrypted with AES-256
    std::string ipAddress_encrypted;
    std::string userAgent_encrypted;
    
    // Link to analytics (without exposing IP)
    std::string viewId;
    
    // Auto-management
    Date retentionExpiry = timestamp + 12_months;
    bool isUnderInvestigation = false;
    
    // This table is in separate database
    // Encrypted at rest
    // Access requires 2-factor authentication
};

// ═══════════════════════════════════════
// TIER 3: Emergency Legal (Air-Gapped)
// ═══════════════════════════════════════
struct EmergencyLegalData {
    std::string caseNumber;
    std::string courtOrder;
    Date orderDate;
    
    // Original data (unencrypted)
    std::string ipAddress_original;
    std::string userAgent_original;
    
    // Access control
    std::string authorizedBy;  // CEO + Legal Officer
    Date accessedAt;
    std::string accessReason;
    
    // This is in physically separate server
    // Requires 2 signatures to access
};
```

## 🔐 Security Measures

### Encryption Standards
- **AES-256** for all sensitive data
- **Daily salt rotation** for hashing
- **TLS 1.3** for all connections
- **Encrypted backups** with separate keys

### Auto-Deletion System
```cpp
// Runs daily at 2 AM
void autoDeleteExpiredLogs() {
    auto expiredLogs = complianceDB.query(
        "retentionExpiry < NOW() AND isUnderInvestigation = false"
    );
    
    for (auto& log : expiredLogs) {
        // Secure deletion (3-pass overwrite)
        secureDelete(log);
        auditLog("Deleted expired log: " + log.logId);
    }
    
    LOG_INFO("Auto-deleted " + std::to_string(expiredLogs.size()) + " expired logs");
}
```

### Access Control
- **Analytics DB**: Normal application access
- **Compliance DB**: Admin + 2FA required
- **Legal Vault**: CEO + Legal Officer signatures required

## 📋 Daily Breakdown

### Day 1: Base Profile Model + Privacy Architecture
- Design base profile schema with minimal fields
- Implement username/slug validation
- Create profile types enum (PERSON, BUSINESS)
- Add privacy controls (private by default)
- Set up three-tier database structure
- Implement encryption utilities

### Day 2: Person Profile + GeoIP System
- Extend base model for individual profiles
- Add personal info fields (name, bio, skills)
- Implement GeoIP lookup and extraction
- Create analytics data model (no IP)
- Add compliance data model (encrypted IP)
- Test IP/Geo separation flow

### Day 3: Business Profile + Security
- Extend base model for company/brand profiles
- Add business info fields (company name, category, location)
- Implement auto-deletion system
- Create database indexes for performance
- Set up audit logging
- Test encryption/decryption
- Validate 12-month retention

## 👤 User Privacy Dashboard

Users can see exactly what data we have:

```cpp
struct UserPrivacyDashboard {
    // What user sees
    struct ActivityLog {
        Date when;
        std::string action;  // "profile_view", "login"
        std::string location;  // "CityName" (city only, NO IP)
        std::string device;  // "Chrome on Android"
    };
    std::vector<ActivityLog> last30Days;
    
    struct DataRetention {
        int profileData = -1;  // Until account deletion
        int technicalLogs = 365;  // 12 months
        int deletedAccountData = 0;  // Immediate deletion
    };
    
    // User actions
    bool canExportAllData = true;  // One-click export
    bool canDeleteAccount = true;  // One-click deletion
    bool canSeeLegalRequests = true;  // Transparency
    
    int legalRequestsCount = 0;  // How many court orders?
};
```

**Dashboard Shows:**
```
Your Privacy & Security:
✅ Strong password
✅ Two-factor authentication active
⚠️ Last login: Yesterday from CityName
🔒 Encrypted data: Email, phone number
📊 Access log: 5 accesses in last 30 days

[Download All My Data]
[Delete My Account Forever]
[View Legal Requests: 0]
```

## 🧪 Testing Strategy

### Unit Tests
```cpp
// Test profile creation with minimal data
TEST(ProfileModelTest, CreateMinimalProfile) {
    Profile profile{
        .id = generateId(),
        .slug = "test-profile",
        .type = ProfileType::PERSON,
        .isPublic = false  // Default: private
    };
    EXPECT_TRUE(profile.isValid());
    EXPECT_FALSE(profile.isPublic);  // Privacy first!
}

// Test IP/Geo separation
TEST(PrivacyTest, IPNotStoredInAnalytics) {
    auto analytics = createProfileViewAnalytics();
    EXPECT_TRUE(analytics.city == "CityName");
    EXPECT_TRUE(analytics.ipAddress.empty());  // Must be empty!
}

// Test encryption
TEST(SecurityTest, SensitiveDataEncrypted) {
    Profile profile;
    profile.setEmail("user@example.com");
    EXPECT_NE(profile.email_encrypted, "user@example.com");
    EXPECT_TRUE(isEncrypted(profile.email_encrypted));
}

// Test auto-deletion
TEST(ComplianceTest, OldLogsAutoDeleted) {
    auto oldLog = createLogFromDate(now() - 13_months);
    runAutoDelete();
    EXPECT_FALSE(complianceDB.exists(oldLog.id));
}
```

### Integration Tests
- Test MongoDB document insertion/extraction
- Validate schema constraints
- Test index performance
- Verify data consistency across profile types
- **Test IP extraction and geo data storage**
- **Test encryption/decryption with AES-256**
- **Test auto-deletion after 12 months**
- **Test access control for compliance DB**

### Security Tests
- **Penetration testing** before production
- **SQL injection attempts** on all fields
- **XSS attack attempts** on text fields
- **Encryption strength validation**
- **Memory leak detection** after IP processing
- **Audit log integrity** verification

## 🎉 Success Criteria

### Functionality
- All models compile without errors
- MongoDB documents can be created and retrieved
- Validation rules prevent invalid data
- Indexes improve query performance by 10x+
- Unit test coverage >85%

### Privacy & Security ✨ NEW
- ✅ **No IP addresses** stored in analytics database
- ✅ **All sensitive data encrypted** with AES-256
- ✅ **Auto-deletion working** (12-month retention verified)
- ✅ **Geo data accurate** (city-level, 95%+ accuracy)
- ✅ **Memory wiped** after IP processing (verified with memory profiler)
- ✅ **Access control working** (compliance DB requires 2FA)
- ✅ **Audit logs complete** (every access logged)

### Compliance
- ✅ **Legal compliance** with local data retention laws
- ✅ **GDPR-ready** (data export, right to be forgotten)
- ✅ **Transparent** (users can see all their data)
- ✅ **Court-ready** (encrypted IP available with valid order)

### User Trust
- ✅ User privacy dashboard functional
- ✅ One-click data export works
- ✅ One-click account deletion works
- ✅ Zero privacy violations
- ✅ Zero data breaches
- ✅ User trust score target: >80%
