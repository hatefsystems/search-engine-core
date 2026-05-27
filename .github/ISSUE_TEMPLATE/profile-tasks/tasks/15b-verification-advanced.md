# 🚀 Profile Verification - Advanced Verification & Claims

**Duration:** 3 days
**Dependencies:** 15a-verification-basic.md (basic verification workflow)
**Acceptance Criteria:**
- ✅ Advanced verification with third-party checks
- ✅ Profile claims and dispute resolution system
- ✅ Verification tiers (Bronze, Silver, Gold)
- ✅ Automated verification for trusted sources
- ✅ Verification expiration and renewal
- ✅ Public verification badges and certificates
- ✅ Trust scoring system

## 🎯 Task Description

Implement advanced verification features that go beyond basic document review to include automated verification, trust scoring, and a comprehensive claims system for handling disputes and maintaining verification integrity.

## 🏆 Verification Tiers System

### Tier Structure
```cpp
enum class VerificationTier {
    NONE,           // Unverified
    BRONZE,         // Basic verification (ID check)
    SILVER,         // Enhanced verification (Address, employment)
    GOLD,           // Premium verification (Background check, references)
    PLATINUM        // Enterprise verification (Legal, financial checks)
};

struct VerificationLevel {
    VerificationTier tier;
    Date achievedAt;
    Date expiresAt;             // Verification expires after 1 year
    std::string certificateId;  // Unique certificate identifier
    std::vector<std::string> verifiedClaims; // What was verified

    // Trust score components
    int documentScore;          // 0-100 based on document quality
    int consistencyScore;       // 0-100 based on data consistency
    int socialScore;           // 0-100 based on social verification
    int overallTrustScore;     // 0-100 composite score
};
```

### Tier Requirements
```cpp
struct TierRequirements {
    VerificationTier tier;
    std::vector<DocumentType> requiredDocuments;
    std::vector<std::string> requiredChecks;
    int minimumTrustScore;
    bool requiresBackgroundCheck;
    bool requiresReferences;
    double renewalFee;          // Annual renewal cost
};

const std::map<VerificationTier, TierRequirements> TIER_REQUIREMENTS = {
    {VerificationTier::BRONZE, {
        .requiredDocuments = {NATIONAL_ID_FRONT, NATIONAL_ID_BACK, SELFIE_WITH_ID},
        .requiredChecks = {"ID_VALIDITY", "PHOTO_MATCH"},
        .minimumTrustScore = 70,
        .requiresBackgroundCheck = false,
        .requiresReferences = false,
        .renewalFee = 0.0
    }},
    {VerificationTier::SILVER, {
        .requiredDocuments = {/* Bronze docs + */ ADDRESS_PROOF, EMPLOYMENT_LETTER},
        .requiredChecks = {/* Bronze + */ "ADDRESS_VERIFICATION", "EMPLOYMENT_VERIFICATION"},
        .minimumTrustScore = 80,
        .requiresBackgroundCheck = false,
        .requiresReferences = true,
        .renewalFee = 29.99
    }},
    {VerificationTier::GOLD, {
        .requiredDocuments = {/* Silver docs + */ DEGREE_CERTIFICATE, PROFESSIONAL_CERTIFICATE},
        .requiredChecks = {/* Silver + */ "BACKGROUND_CHECK", "REFERENCE_CHECK"},
        .minimumTrustScore = 90,
        .requiresBackgroundCheck = true,
        .requiresReferences = true,
        .renewalFee = 99.99
    }}
};
```

## 🔍 Automated Verification System

### Third-Party Integration
```cpp
class AutomatedVerificationService {
public:
    static VerificationResult performAutomatedChecks(const VerificationRequest& request);

    // Individual checks
    static bool verifyNationalId(const std::string& nationalId, const std::string& fullName);
    static bool verifyAddress(const std::string& address, const VerificationDocuments& docs);
    static bool verifyEmployment(const std::string& company, const std::string& position);
    static bool performBackgroundCheck(const std::string& nationalId);

private:
    static NationalIdVerificationService nationalIdService_;
    static AddressVerificationService addressService_;
    static EmploymentVerificationService employmentService_;
    static BackgroundCheckService backgroundService_;
};
```

### Automated Verification Flow
```cpp
VerificationResult AutomatedVerificationService::performAutomatedChecks(
    const VerificationRequest& request) {

    VerificationResult result;
    result.requestId = request.requestId;

    // National ID verification
    if (!verifyNationalId(request.nationalId, request.fullName)) {
        result.flags.push_back("NATIONAL_ID_MISMATCH");
        result.automatedScore -= 30;
    }

    // Address verification
    if (!verifyAddress(request.address, request.documents)) {
        result.flags.push_back("ADDRESS_UNVERIFIED");
        result.automatedScore -= 20;
    }

    // Employment verification (for Silver/Gold)
    if (request.type >= VerificationType::INDIVIDUAL_ADVANCED) {
        if (!verifyEmployment(request.company, request.title)) {
            result.flags.push_back("EMPLOYMENT_UNVERIFIED");
            result.automatedScore -= 25;
        }
    }

    // Background check (for Gold)
    if (request.type == VerificationType::INDIVIDUAL_PREMIUM) {
        auto backgroundResult = performBackgroundCheck(request.nationalId);
        if (!backgroundResult.clean) {
            result.flags.push_back("BACKGROUND_CHECK_FAILED");
            result.recommendation = "MANUAL_REVIEW";
            result.automatedScore -= 50;
        }
    }

    // Determine recommendation
    result.recommendation = calculateRecommendation(result.automatedScore, result.flags);

    return result;
}
```

## ⚖️ Profile Claims & Dispute Resolution

### Claims System
```cpp
enum class ClaimType {
    INCORRECT_INFORMATION,      // Wrong details on profile
    IMPERSONATION,              // Someone pretending to be someone else
    FAKE_DOCUMENTS,             // Submitted fraudulent documents
    EXPIRED_VERIFICATION,       // Verification expired
    VIOLATION_OF_TERMS,         // Profile violates platform rules
    LEGAL_ISSUES               // Court orders, legal disputes
};

struct ProfileClaim {
    std::string claimId;
    std::string profileId;
    std::string claimantId;         // Who filed the claim
    ClaimType type;
    std::string description;
    std::vector<ClaimEvidence> evidence;

    // Processing
    ClaimStatus status = ClaimStatus::PENDING;
    Date filedAt;
    Date resolvedAt;
    std::string resolvedBy;        // Admin who resolved
    std::string resolution;
    ClaimResolutionType resolutionType;

    // Verification impact
    bool verificationRevoked = false;
    VerificationTier previousTier;
    std::string revocationReason;
};

enum class ClaimResolutionType {
    DISMISSED,                  // Claim unfounded
    WARNING_ISSUED,             // Warning given, no action
    VERIFICATION_REVOKED,       // Verification removed
    PROFILE_SUSPENDED,          // Profile temporarily suspended
    PROFILE_BANNED,             // Profile permanently banned
    LEGAL_ACTION                // Escalated to legal
};
```

### Claim Submission & Processing
```cpp
class ClaimsService {
public:
    static ProfileClaim submitClaim(const ClaimSubmission& submission);
    static void processClaim(const std::string& claimId, const ClaimResolution& resolution);
    static std::vector<ProfileClaim> getClaimsForProfile(const std::string& profileId);

private:
    static bool validateClaimSubmission(const ClaimSubmission& submission);
    static void notifyClaimant(const ProfileClaim& claim);
    static void applyClaimResolution(const ProfileClaim& claim);
    static void updateProfileVerificationStatus(const std::string& profileId,
                                              const ClaimResolution& resolution);
};
```

## 🏅 Public Verification Badges & Certificates

### Badge System
```cpp
struct VerificationBadge {
    VerificationTier tier;
    std::string badgeId;
    std::string certificateUrl;     // Public certificate link
    Date issuedAt;
    Date expiresAt;
    bool isActive = true;

    // Visual properties
    std::string badgeImageUrl;
    std::string badgeColor;
    std::string badgeText;         // "Verified Bronze"
};

class BadgeService {
public:
    static VerificationBadge generateBadge(const std::string& profileId,
                                         VerificationTier tier);

    static std::string getBadgeHtml(const VerificationBadge& badge);
    static std::string getCertificateUrl(const std::string& certificateId);

private:
    static std::string generateCertificateId();
    static std::string createCertificatePage(const VerificationBadge& badge);
};
```

### Public Certificate Example
```html
<!-- /certificates/ABC123-DEF456 -->
<div class="verification-certificate">
    <div class="certificate-header">
        <img src="/images/hatef-logo.png" alt="Hatef.ir" class="logo">
        <h1>Profile Verification Certificate</h1>
    </div>

    <div class="certificate-body">
        <div class="verification-details">
            <div class="detail-row">
                <span class="label">Verified Individual:</span>
                <span class="value">John Doe</span>
            </div>

            <div class="detail-row">
                <span class="label">Verification Tier:</span>
                <span class="value">Gold</span>
            </div>

            <div class="detail-row">
                <span class="label">Certificate ID:</span>
                <span class="value">ABC123-DEF456</span>
            </div>

            <div class="detail-row">
                <span class="label">Issued Date:</span>
                <span class="value">January 15, 2024</span>
            </div>

            <div class="detail-row">
                <span class="label">Expiration Date:</span>
                <span class="value">January 15, 2025</span>
            </div>

            <div class="detail-row">
                <span class="label">Verified Information:</span>
                <ul class="verified-info">
                    <li>Identity Documents</li>
                    <li>Address Verification</li>
                    <li>Employment Verification</li>
                    <li>Background Check</li>
                    <li>Professional References</li>
                </ul>
            </div>
        </div>

        <div class="certificate-qr">
            <img src="/qr/certificates/ABC123-DEF456" alt="Certificate QR Code">
            <p>Scan to verify authenticity</p>
        </div>
    </div>

    <div class="certificate-footer">
        <p>This certificate confirms that the above individual has been verified by Hatef.ir through our comprehensive verification process.</p>
        <div class="signature">
            <img src="/images/digital-signature.png" alt="Digital Signature">
            <p>Hatef.ir Verification Authority</p>
        </div>
    </div>
</div>
```

## ⏰ Verification Lifecycle Management

### Expiration & Renewal System
```cpp
class VerificationLifecycleService {
public:
    static void checkExpiredVerifications();
    static void sendRenewalReminders();
    static RenewalResult processRenewal(const std::string& profileId,
                                      VerificationTier newTier);

private:
    static std::vector<std::string> findExpiredVerifications();
    static void revokeExpiredVerification(const std::string& profileId);
    static void sendExpirationWarning(const std::string& profileId, int daysRemaining);
};

struct RenewalResult {
    bool success;
    std::string newCertificateId;
    Date newExpirationDate;
    double renewalFee;
    std::string errorMessage;
};
```

### Automated Lifecycle Management
```cpp
void VerificationLifecycleService::checkExpiredVerifications() {
    auto expiredProfiles = verificationRepository.findExpiredVerifications();

    for (const auto& profileId : expiredProfiles) {
        // Revoke verification
        revokeExpiredVerification(profileId);

        // Send notification
        sendExpirationNotification(profileId);

        // Update search index
        searchIndex.updateVerificationStatus(profileId, false);

        auditLog("Verification expired for profile: " + profileId);
    }
}
```

## 🎯 Trust Scoring System

### Trust Score Calculation
```cpp
class TrustScoreCalculator {
public:
    static int calculateOverallTrustScore(const std::string& profileId);

private:
    static int calculateDocumentTrust(const VerificationDocuments& docs);
    static int calculateConsistencyTrust(const Profile& profile);
    static int calculateSocialTrust(const std::string& profileId);
    static int calculateHistoryTrust(const std::string& profileId);
};

int TrustScoreCalculator::calculateOverallTrustScore(const std::string& profileId) {
    auto profile = profileRepository.findById(profileId);
    auto verification = verificationRepository.findByProfileId(profileId);

    int documentScore = calculateDocumentTrust(verification.documents);
    int consistencyScore = calculateConsistencyTrust(*profile);
    int socialScore = calculateSocialTrust(profileId);
    int historyScore = calculateHistoryTrust(profileId);

    // Weighted calculation
    int overallScore = (documentScore * 0.4) +
                      (consistencyScore * 0.3) +
                      (socialScore * 0.2) +
                      (historyScore * 0.1);

    return std::min(100, std::max(0, overallScore));
}
```

## 📋 Implementation Plan

### Day 1: Verification Tiers + Automated Checks
- Implement tier system with requirements
- Add automated verification services
- Create trust scoring foundation
- Test tier upgrade/downgrade logic

### Day 2: Claims System + Dispute Resolution
- Build claims submission and processing
- Implement dispute resolution workflow
- Add claim evidence handling
- Create admin claims dashboard

### Day 3: Badges, Certificates + Lifecycle Management
- Implement verification badges and certificates
- Add expiration and renewal system
- Create public certificate pages
- Test end-to-end advanced verification flow

## 🧪 Testing Strategy

### Tier System Tests
```cpp
TEST(VerificationTierTest, TierRequirements) {
    // Test Bronze requirements
    auto bronzeReqs = TIER_REQUIREMENTS[VerificationTier::BRONZE];
    EXPECT_TRUE(bronzeReqs.requiredDocuments.size() >= 3);
    EXPECT_FALSE(bronzeReqs.requiresBackgroundCheck);

    // Test Gold requirements
    auto goldReqs = TIER_REQUIREMENTS[VerificationTier::GOLD];
    EXPECT_TRUE(goldReqs.requiresBackgroundCheck);
    EXPECT_TRUE(goldReqs.requiresReferences);
    EXPECT_GT(goldReqs.renewalFee, 0);
}

TEST(TrustScoreTest, ScoreCalculation) {
    // Create verified profile
    auto profileId = createVerifiedProfile();

    // Calculate trust score
    int score = TrustScoreCalculator::calculateOverallTrustScore(profileId);

    // Should be high for verified profile
    EXPECT_GE(score, 80);
    EXPECT_LE(score, 100);
}
```

### Claims System Tests
```cpp
TEST(ClaimsTest, ClaimSubmissionAndResolution) {
    // Submit impersonation claim
    auto claim = ClaimsService::submitClaim({
        .profileId = "target-profile",
        .claimantId = "claimant-user",
        .type = ClaimType::IMPERSONATION,
        .description = "This person is impersonating me"
    });

    EXPECT_FALSE(claim.claimId.empty());
    EXPECT_EQ(claim.status, ClaimStatus::PENDING);

    // Admin resolves claim
    ClaimsService::processClaim(claim.claimId, {
        .resolutionType = ClaimResolutionType::VERIFICATION_REVOKED,
        .resolution = "Confirmed impersonation"
    });

    // Verify profile verification revoked
    auto profile = profileRepository.findById("target-profile");
    EXPECT_FALSE(profile->isVerified);
}
```

### Certificate Tests
```cpp
TEST(CertificateTest, BadgeGeneration) {
    auto badge = BadgeService::generateBadge("profile-123", VerificationTier::GOLD);

    EXPECT_EQ(badge.tier, VerificationTier::GOLD);
    EXPECT_FALSE(badge.certificateId.empty());
    EXPECT_TRUE(badge.expiresAt > badge.issuedAt);

    // Test badge HTML generation
    auto html = BadgeService::getBadgeHtml(badge);
    EXPECT_TRUE(html.find("Gold") != std::string::npos);
    EXPECT_TRUE(html.find(badge.certificateId) != std::string::npos);
}
```

## 🎉 Success Criteria

### Advanced Verification
- ✅ **Tier system with clear requirements**
- ✅ **Automated third-party verification**
- ✅ **Trust scoring system implemented**
- ✅ **Verification expiration and renewal**

### Claims & Disputes
- ✅ **Claims submission and processing**
- ✅ **Dispute resolution workflow**
- ✅ **Verification revocation system**
- ✅ **Audit trail for all claims**

### Public Trust
- ✅ **Verification badges and certificates**
- ✅ **Public certificate pages**
- ✅ **QR code verification**
- ✅ **Trust indicators throughout platform**

### Lifecycle Management
- ✅ **Automated expiration handling**
- ✅ **Renewal reminder system**
- ✅ **Fee-based renewals**
- ✅ **Lifecycle event notifications**

This creates a **comprehensive verification ecosystem** that builds **maximum trust** while providing **flexible tiers** for different user needs.
