# 🚀 Profile Database Models - Privacy Architecture

**Duration:** 2 days
**Dependencies:** 01b-database-personal-business.md (profile models)
**Acceptance Criteria:**
- ✅ Three-tier database architecture (Analytics, Compliance, Legal Vault)
- ✅ IP/Geo separation system implemented
- ✅ Encryption utilities for sensitive data (AES-256)
- ✅ Auto-deletion system for compliance logs
- ✅ Access control for compliance database
- ✅ Privacy controls and data retention policies
- ✅ Privacy dashboard foundation
- ✅ Legal compliance with data retention laws

## 🎯 Task Description

Implement the comprehensive privacy-first architecture that balances user privacy with legal compliance. This revolutionary three-tier system separates analytics data from compliance data while providing users complete transparency and control over their data.

## 🗺️ Three-Tier Database Architecture

### The Challenge
Local law requires storing IP addresses for potential legal investigations, BUT users deserve privacy and we don't need IPs for analytics. We need to balance both requirements.

### Our Solution: Complete Data Separation

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

## 🔐 Encryption System

### Sensitive Data Fields
```cpp
struct EncryptedFields {
    std::string email_encrypted;      // Person: email, Business: businessEmail
    std::string phone_encrypted;      // Person: phone, Business: businessPhone
    std::string address_encrypted;    // Business: address (optional)
};
```

### Encryption Implementation
```cpp
class DataEncryption {
public:
    static std::string encrypt(const std::string& plaintext, const std::string& key);
    static std::string decrypt(const std::string& ciphertext, const std::string& key);

private:
    static const std::string COMPLIANCE_KEY;  // Rotated daily
    static const std::string LEGAL_KEY;       // Air-gapped storage
};

// Usage
std::string encryptedEmail = DataEncryption::encrypt(userEmail, COMPLIANCE_KEY);
std::string decryptedEmail = DataEncryption::decrypt(encryptedEmail, COMPLIANCE_KEY);
```

## 📊 Tier 1: Analytics Database

### What Gets Stored (Privacy-First)
```cpp
struct ProfileViewAnalytics {
    std::string viewId;
    std::string profileId;
    Date timestamp;

    // Geographic (city-level, NO IP)
    std::string country = "CountryName";
    std::string province = "ProvinceName";
    std::string city = "CityName";

    // Device (generic)
    std::string browser = "Chrome";      // Not version
    std::string os = "Android";          // Not version
    std::string deviceType = "Mobile";   // Mobile/Desktop/Tablet
};
```

### User Dashboard Shows
```
Your Profile Analytics:
- 45 views from Tehran this month
- 78% from mobile devices
- Top referrer: Google Search
- Peak viewing hours: 2-4 PM
```

## 🛡️ Tier 2: Compliance Database

### Encrypted Legal Data
```cpp
struct LegalComplianceLog {
    std::string logId;
    std::string userId;
    Date timestamp;

    // Encrypted with AES-256 (separate key from analytics)
    std::string ipAddress_encrypted;
    std::string userAgent_encrypted;
    std::string referrer_encrypted;

    // Link to analytics (without exposing sensitive data)
    std::string viewId;

    // Auto-management
    Date retentionExpiry = timestamp + 12_months;
    bool isUnderInvestigation = false;
};
```

### Access Controls
- **Database Level**: Separate MongoDB instance
- **Application Level**: 2-factor authentication required
- **Network Level**: VPN + IP whitelist
- **Audit Level**: All access logged with user ID

## ⚖️ Tier 3: Legal Vault (Air-Gapped)

### Emergency Legal Data
```cpp
struct EmergencyLegalData {
    std::string caseNumber;
    std::string courtOrder;
    Date orderDate;

    // Original data (unencrypted - only accessible with court order)
    std::string ipAddress_original;
    std::string userAgent_original;
    std::string referrer_original;

    // Access control
    std::string authorizedBy;      // CEO + Legal Officer
    Date accessedAt;
    std::string accessReason;
};
```

### Access Protocol
1. Valid court order received
2. CEO + Legal Officer signature required
3. Data extracted to secure environment
4. Full audit trail recorded
5. Data destroyed after legal requirement fulfilled

## 🔄 Data Processing Flow

### Profile View Processing
```cpp
void handleProfileView(const HttpRequest& req) {
    std::string ip = req.getRemoteIP();
    std::string userAgent = req.getUserAgent();

    // Step 1: Extract geo data immediately (city-level only)
    GeoData geo = GeoIPService::lookup(ip);
    // geo = {country: "Iran", province: "Tehran", city: "Tehran"}

    // Step 2: Store in Analytics DB (NO IP!)
    ProfileViewAnalytics analytics{
        .viewId = generateUUID(),
        .profileId = req.getProfileId(),
        .timestamp = now(),
        .country = geo.country,
        .province = geo.province,
        .city = geo.city,
        .browser = parseBrowser(userAgent),
        .os = parseOS(userAgent),
        .deviceType = parseDeviceType(userAgent)
    };
    analyticsDB.save(analytics);

    // Step 3: Store in Compliance DB (Encrypted IP)
    LegalComplianceLog legalLog{
        .logId = generateUUID(),
        .userId = req.getUserId(),
        .timestamp = now(),
        .ipAddress_encrypted = DataEncryption::encrypt(ip, COMPLIANCE_KEY),
        .userAgent_encrypted = DataEncryption::encrypt(userAgent, COMPLIANCE_KEY),
        .viewId = analytics.viewId,
        .retentionExpiry = now() + 12_months
    };
    complianceDB.save(legalLog);

    // Step 4: Secure memory wipe
    secureMemoryWipe(&ip);
    secureMemoryWipe(&userAgent);
}
```

## ⏰ Auto-Deletion System

### Daily Cleanup Job
```cpp
void autoDeleteExpiredLogs() {
    auto expiredLogs = complianceDB.query(
        "retentionExpiry < NOW() AND isUnderInvestigation = false"
    );

    for (auto& log : expiredLogs) {
        // Secure deletion (3-pass overwrite)
        secureDelete(log);
        auditLog("Deleted expired compliance log: " + log.logId);
    }

    LOG_INFO("Auto-deleted " + std::to_string(expiredLogs.size()) + " expired logs");
}
```

### Data Retention Policy
- **Profile Data**: Until account deletion
- **Analytics Data**: 2 years (user configurable)
- **Compliance Logs**: 12 months (auto-delete)
- **Deleted Accounts**: 0 days (immediate secure wipe)
- **Legal Hold**: Only with valid court order

## 👤 User Privacy Dashboard

### What Users Can See
```cpp
struct UserPrivacyDashboard {
    // Activity Log (from analytics - no IPs!)
    struct ActivityLog {
        Date when;
        std::string action;      // "profile_view", "login", "update"
        std::string location;    // "Tehran, Iran" (city level only)
        std::string device;      // "Chrome on Android"
    };
    std::vector<ActivityLog> last30Days;

    // Data Retention Settings
    struct DataRetention {
        int profileData = -1;        // Until deletion
        int analyticsData = 730;     // 2 years default
        int complianceLogs = 365;    // 12 months
        int deletedData = 0;         // Immediate
    };

    // User Controls
    bool canExportAllData = true;      // One-click export
    bool canDeleteAccount = true;      // One-click deletion
    bool canControlRetention = true;   // Change retention periods
    int legalRequestsCount = 0;        // Number of court orders
};
```

### Dashboard UI Example
```
🔒 Your Privacy & Security

✅ Profile data: Stored until account deletion
✅ Analytics data: 2 years (customizable)
✅ Compliance logs: Auto-deleted after 12 months
✅ Sensitive data: Encrypted with AES-256

📊 Recent Activity:
• Profile viewed from Tehran, Iran - Chrome on Android - Yesterday
• Login from Tehran, Iran - Safari on iPhone - 3 days ago

⚙️ Privacy Controls:
[Export All My Data] [Delete Account] [Change Retention Settings]

📞 Legal Requests: 0
```

## 📋 Implementation Plan

### Day 1: Encryption + Tier 1 Analytics
- Implement AES-256 encryption utilities
- Create analytics database schema
- Add encryption to sensitive profile fields
- Implement geo data extraction (city-level only)
- Create profile view analytics tracking

### Day 2: Tier 2 Compliance + Auto-Deletion
- Create compliance database schema
- Implement encrypted IP storage
- Add 2FA access controls
- Implement auto-deletion system
- Create privacy dashboard foundation
- Test end-to-end data flow

## 🧪 Testing Strategy

### Security Tests
```cpp
TEST(EncryptionTest, AES256RoundTrip) {
    std::string original = "user@example.com";
    std::string encrypted = DataEncryption::encrypt(original, TEST_KEY);
    std::string decrypted = DataEncryption::decrypt(encrypted, TEST_KEY);

    EXPECT_NE(encrypted, original);
    EXPECT_EQ(decrypted, original);
}

TEST(PrivacyTest, NoIPInAnalytics) {
    auto analytics = createProfileViewAnalytics();
    EXPECT_TRUE(analytics.ipAddress.empty());
    EXPECT_FALSE(analytics.city.empty());  // But city data exists
}

TEST(ComplianceTest, AutoDeletionWorks) {
    // Create old log
    auto oldLog = createLogFromDate(now() - 13_months);

    // Run auto-deletion
    runAutoDelete();

    // Verify deleted
    EXPECT_FALSE(complianceDB.exists(oldLog.id));
}
```

### Integration Tests
- Test complete data flow from request to storage
- Verify encryption/decryption works
- Test geo data accuracy
- Validate access controls
- Test auto-deletion timing
- Verify memory wiping

## 🎉 Success Criteria

### Privacy & Security ✨
- ✅ **Three-tier architecture implemented**
- ✅ **AES-256 encryption working**
- ✅ **No IP addresses in analytics database**
- ✅ **Geo data accurate** (city-level, 95%+ accuracy)
- ✅ **Memory wiped** after IP processing
- ✅ **Auto-deletion working** (12-month retention)
- ✅ **Access controls implemented**

### Legal Compliance
- ✅ **Data retention policies implemented**
- ✅ **Court-ready encrypted data available**
- ✅ **Audit trails complete**
- ✅ **Air-gapped legal vault foundation**

### User Trust
- ✅ **Privacy dashboard functional**
- ✅ **User controls working**
- ✅ **Data export possible**
- ✅ **Account deletion works**
- ✅ **Zero privacy violations**

## 🔗 Dependencies & Integration

### Depends On
- **01b-database-personal-business.md** - Profile models with fields to encrypt

### Enables
- **01d-database-indexes-validation.md** - Performance indexes for encrypted data
- **All privacy-dependent features** - Secure foundation for user data
- **Legal compliance** - Ready for production deployment

This privacy architecture provides **unprecedented transparency** while maintaining **complete legal compliance** - a first in the industry for local market platforms.
