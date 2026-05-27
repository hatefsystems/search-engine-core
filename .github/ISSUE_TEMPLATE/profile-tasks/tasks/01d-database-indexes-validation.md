# 🚀 Profile Database Models - Indexes & Advanced Validation

**Status:** ✅ **Implemented** (2026-02-09)
**Duration:** 1 day
**Dependencies:** 01c-privacy-architecture.md (privacy system)
**Acceptance Criteria:**
- ✅ Database indexes created for performance (slug, type, createdAt, etc.)
- ✅ Advanced validation rules implemented
- ✅ Audit logging system
- ✅ Performance testing with realistic data
- ✅ Query optimization verified
- ✅ Index maintenance and monitoring
- ✅ Database schema documentation

## 📦 Implementation Summary

| Component | Location | Notes |
|-----------|----------|--------|
| Named indexes (profiles) | `src/storage/ProfileStorage.cpp` | slug_unique, type_index, created_at_index, public_filter, type_public_recent, person_skills (partial), business_location_industry (partial) |
| Named indexes (analytics) | `src/storage/ProfileViewAnalyticsStorage.cpp` | profile_views_timeline, location_analytics, device_analytics on collection `profile_view_analytics` |
| Named indexes (compliance) | `src/storage/ComplianceStorage.cpp` | user_compliance_history, auto_deletion_index, analytics_link on collection `legal_compliance_logs` |
| ProfileValidator | `include/search_engine/storage/ProfileValidator.h`, `src/storage/ProfileValidator.cpp` | ValidationResult, validate/validatePersonFields/validateBusinessFields, email/phone/URL/slug validation; slug supports Persian + English |
| Audit system | `include/search_engine/storage/ProfileAuditLog.h`, `src/storage/AuditStorage.cpp`, `src/storage/AuditLogger.cpp` | Collection `profile_audit_logs`; CREATE/UPDATE/DELETE/VIEW logged from ProfileController |
| Index monitoring | `include/search_engine/storage/IndexMonitor.h`, `src/storage/IndexMonitor.cpp` | getIndexStats, getAllProfileIndexStats, detectUnusedIndexes |
| Performance tests | `tests/storage/test_profile_performance.cpp`, `tests/storage/test_profile_validator.cpp` | Slug lookup & analytics benchmarks; validator unit tests (78 assertions) |
| Schema documentation | `docs/architecture/profile-database-schema.md` | Collections, fields, all index names, query patterns, maintenance |

**Collection names in code:** `profiles`, `profile_view_analytics`, `legal_compliance_logs`, `profile_audit_logs` (task spec used logical names `profile_views` / `compliance_logs`; implementation keeps existing names).

## 🎯 Task Description

Complete the database foundation with performance optimizations and advanced validation. This task adds the final layer of robustness to ensure the profile system can handle production-scale traffic while maintaining data integrity and performance.

## 📊 Database Indexes Strategy

### Core Performance Indexes

#### Profile Collection Indexes
```javascript
// MongoDB index definitions

// Primary lookup index (most important)
db.profiles.createIndex({ "slug": 1 }, { unique: true, name: "slug_unique" })

// Profile type queries
db.profiles.createIndex({ "type": 1 }, { name: "type_index" })

// Creation date for sorting/filtering
db.profiles.createIndex({ "createdAt": 1 }, { name: "created_at_index" })

// Public profiles only (frequent filter)
db.profiles.createIndex({ "isPublic": 1 }, { name: "public_filter" })

// Compound indexes for complex queries
db.profiles.createIndex({
    "type": 1,
    "isPublic": 1,
    "createdAt": -1
}, { name: "type_public_recent" })

// Person profile specific indexes
db.profiles.createIndex({
    "type": "PERSON",
    "skills": 1
}, { name: "person_skills" })

// Business profile specific indexes
db.profiles.createIndex({
    "type": "BUSINESS",
    "industry": 1,
    "city": 1
}, { name: "business_location_industry" })
```

### Analytics Database Indexes
```javascript
// Profile view analytics
db.profile_views.createIndex({
    "profileId": 1,
    "timestamp": -1
}, { name: "profile_views_timeline" })

db.profile_views.createIndex({
    "city": 1,
    "timestamp": -1
}, { name: "location_analytics" })

db.profile_views.createIndex({
    "deviceType": 1,
    "timestamp": -1
}, { name: "device_analytics" })
```

### Compliance Database Indexes
```javascript
// Legal compliance (encrypted data)
db.compliance_logs.createIndex({
    "userId": 1,
    "timestamp": -1
}, { name: "user_compliance_history" })

db.compliance_logs.createIndex({
    "retentionExpiry": 1
}, { name: "auto_deletion_index" })

db.compliance_logs.createIndex({
    "viewId": 1
}, { name: "analytics_link" })
```

## ✅ Advanced Validation Rules

### Profile Validation Engine
```cpp
class ProfileValidator {
public:
    static ValidationResult validate(const Profile& profile);
    static ValidationResult validatePersonFields(const PersonProfile& profile);
    static ValidationResult validateBusinessFields(const BusinessProfile& profile);

private:
    static bool isValidSlug(const std::string& slug);
    static bool isValidEmail(const std::string& email);
    static bool isValidPhone(const std::string& phone);
    static bool isValidUrl(const std::string& url);
};

struct ValidationResult {
    bool isValid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};
```

### Validation Rules

#### Slug Validation
```cpp
bool ProfileValidator::isValidSlug(const std::string& slug) {
    if (slug.empty() || slug.length() > 50) return false;

    // Only lowercase letters, numbers, hyphens
    std::regex pattern("^[a-z0-9]+(?:-[a-z0-9]+)*$");
    return std::regex_match(slug, pattern);
}

// Test cases
TEST(SlugValidationTest, ValidSlugs) {
    EXPECT_TRUE(isValidSlug("john-doe"));
    EXPECT_TRUE(isValidSlug("test123"));
    EXPECT_TRUE(isValidSlug("user-name"));
}

TEST(SlugValidationTest, InvalidSlugs) {
    EXPECT_FALSE(isValidSlug("John Doe"));      // uppercase
    EXPECT_FALSE(isValidSlug("john@doe"));      // special chars
    EXPECT_FALSE(isValidSlug("john--doe"));     // double hyphen
    EXPECT_FALSE(isValidSlug(""));              // empty
}
```

#### Email Validation (Pre-encryption)
```cpp
bool ProfileValidator::isValidEmail(const std::string& email) {
    if (email.empty() || email.length() > 254) return false;

    std::regex pattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    return std::regex_match(email, pattern);
}
```

#### Phone Validation (International Format)
```cpp
bool ProfileValidator::isValidPhone(const std::string& phone) {
    if (phone.empty()) return true; // Optional field

    // Support international formats: +98xxxxxxxxx, +1xxxxxxxxxx, etc.
    std::regex pattern(R"(^\+\d{1,4}\d{6,14}$)");
    return std::regex_match(phone, pattern);
}
```

#### Business-Specific Validation
```cpp
ValidationResult ProfileValidator::validateBusinessFields(const BusinessProfile& profile) {
    ValidationResult result = {true, {}, {}};

    // Required business fields
    if (profile.companyName.empty()) {
        result.errors.push_back("Company name is required");
        result.isValid = false;
    }

    // Industry validation
    std::vector<std::string> validIndustries = {
        "Technology", "Healthcare", "Finance", "Education",
        "Manufacturing", "Retail", "Food", "Construction"
    };

    if (std::find(validIndustries.begin(), validIndustries.end(),
                  profile.industry) == validIndustries.end()) {
        result.errors.push_back("Invalid industry category");
        result.isValid = false;
    }

    // Founded year validation
    if (profile.foundedYear < 1800 || profile.foundedYear > currentYear() + 1) {
        result.errors.push_back("Invalid founded year");
        result.isValid = false;
    }

    return result;
}
```

## 📋 Audit Logging System

### Comprehensive Audit Trail
```cpp
struct AuditLog {
    std::string id;
    Date timestamp;
    std::string action;          // CREATE, UPDATE, DELETE, VIEW
    std::string resourceType;    // "profile", "analytics", "compliance"
    std::string resourceId;      // Profile ID, etc.
    std::string userId;          // Who performed action
    std::string ipAddress;       // For compliance
    std::string userAgent;       // Device info

    // Change details
    std::string oldValue;        // JSON of old state
    std::string newValue;        // JSON of new state
    std::string reason;          // Why the change was made

    // Metadata
    std::string sessionId;
    std::string apiVersion;
    bool isAutomated = false;    // Was this done by system?
};

class AuditLogger {
public:
    static void logProfileCreate(const Profile& profile, const std::string& userId);
    static void logProfileUpdate(const Profile& oldProfile, const Profile& newProfile, const std::string& userId);
    static void logProfileDelete(const std::string& profileId, const std::string& userId);
    static void logProfileView(const std::string& profileId, const std::string& viewerId);

private:
    static void writeToAuditLog(const AuditLog& log);
};
```

### Audit Log Usage
```cpp
// When profile is created
void ProfileService::createProfile(const Profile& profile, const std::string& userId) {
    // Validate first
    auto validation = ProfileValidator::validate(profile);
    if (!validation.isValid) {
        throw ValidationException(validation.errors);
    }

    // Create profile
    std::string profileId = profileRepository.save(profile);

    // Audit log
    AuditLogger::logProfileCreate(profile, userId);

    return profileId;
}
```

## ⚡ Performance Testing & Optimization

### Benchmark Tests
```cpp
TEST(PerformanceTest, ProfileLookupBySlug) {
    // Create test profiles
    createTestProfiles(1000);

    // Benchmark slug lookup
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        auto profile = profileRepository.findBySlug("test-profile-" + std::to_string(i));
        ASSERT_TRUE(profile.has_value());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should be under 100ms for 100 lookups
    EXPECT_LT(duration.count(), 100);
}

TEST(PerformanceTest, AnalyticsQueries) {
    // Create test analytics data
    createTestAnalyticsData(10000);

    // Test analytics queries performance
    auto start = std::chrono::high_resolution_clock::now();

    auto cityStats = analyticsRepository.getViewsByCity("Tehran", 30); // Last 30 days
    auto deviceStats = analyticsRepository.getViewsByDevice("Mobile", 7); // Last 7 days

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Analytics queries should be under 200ms
    EXPECT_LT(duration.count(), 200);
}
```

### Index Performance Monitoring
```cpp
struct IndexPerformance {
    std::string indexName;
    long long sizeBytes;
    long long usageCount;
    double avgQueryTimeMs;
    Date lastUsed;
};

class IndexMonitor {
public:
    static std::vector<IndexPerformance> getIndexStats();
    static void suggestIndexOptimizations();
    static void detectUnusedIndexes();
};
```

## 📋 Implementation Plan

### Day 1: Indexes + Basic Validation
- Create all database indexes
- Implement basic validation rules (slug, email, phone)
- Add validation to profile creation/update
- Test index performance

### Day 1 Continued: Advanced Validation + Audit
- Implement business-specific validation
- Create audit logging system
- Add audit logs to all CRUD operations
- Performance testing and optimization

## 🧪 Testing Strategy

### Validation Tests
```cpp
TEST(ValidationTest, ComprehensiveProfileValidation) {
    PersonProfile profile;
    profile.slug = "john-doe";
    profile.name = "John Doe";
    profile.email = "john@example.com";
    profile.phone = "+989123456789";
    profile.skills = {"C++", "MongoDB"};

    auto result = ProfileValidator::validate(profile);
    EXPECT_TRUE(result.isValid);
    EXPECT_TRUE(result.errors.empty());
}

TEST(ValidationTest, InvalidDataRejected) {
    PersonProfile profile;
    profile.slug = "John Doe";  // Invalid slug
    profile.email = "invalid-email";  // Invalid email

    auto result = ProfileValidator::validate(profile);
    EXPECT_FALSE(result.isValid);
    EXPECT_GT(result.errors.size(), 0);
}
```

### Index Tests
```cpp
TEST(IndexTest, SlugLookupPerformance) {
    // Setup: Create profiles with indexes
    createProfilesWithIndexes(1000);

    // Test: Slug lookups should be fast
    measureQueryPerformance("slug_lookup", []() {
        return profileRepo.findBySlug("profile-500");
    });
}
```

### Audit Tests
```cpp
TEST(AuditTest, AllActionsLogged) {
    // Create profile
    auto profileId = profileService.createProfile(validProfile, "user123");

    // Check audit log
    auto logs = auditRepository.getLogsForResource(profileId);
    EXPECT_EQ(logs.size(), 1);
    EXPECT_EQ(logs[0].action, "CREATE");
    EXPECT_EQ(logs[0].userId, "user123");
}
```

## 🎉 Success Criteria

### Performance
- ✅ **Indexes improve query performance 10x+**
- ✅ **Profile slug lookup < 10ms average**
- ✅ **Analytics queries < 50ms average**
- ✅ **Concurrent users supported: 1000+**
- ✅ **Database size optimized**

### Data Integrity
- ✅ **All validation rules implemented**
- ✅ **Invalid data rejected at API level**
- ✅ **Business rules enforced**
- ✅ **Data consistency maintained**

### Monitoring & Maintenance
- ✅ **Audit logging working**
- ✅ **Index usage monitored**
- ✅ **Performance benchmarks met**
- ✅ **Schema documentation complete**

### Production Ready
- ✅ **Error handling robust**
- ✅ **Edge cases covered**
- ✅ **Scalability tested**
- ✅ **Maintenance procedures documented**

## 🔗 Dependencies & Integration

### Depends On
- **01c-privacy-architecture.md** - Privacy system to audit and validate

### Enables
- **All profile features** - Robust foundation for high-traffic usage
- **Search integration** - Fast indexed queries
- **Analytics** - Efficient data aggregation
- **Production deployment** - Performance and reliability verified

This completes the database foundation with **enterprise-grade performance** and **bulletproof data integrity**.

---

## 📝 Implementation Notes (as built)

- **Slug validation:** Kept existing Persian + English + numbers + hyphens (same as 01a/01b); no switch to ASCII-only.
- **Audit userId:** Placeholder `"anonymous"` until auth is added; document where to plug in (see AuditLogger calls in ProfileController).
- **Run tests:** Set `COMPLIANCE_ENCRYPTION_KEY` (32 chars) for profile storage tests; validator tests need no env and run with `./tests/storage/test_profile_validator`.
