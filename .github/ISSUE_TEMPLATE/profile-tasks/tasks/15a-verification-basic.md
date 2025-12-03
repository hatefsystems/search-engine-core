# 🚀 Profile Verification - Basic Verification Workflow

**Duration:** 2 days
**Dependencies:** Profile database models
**Acceptance Criteria:**
- ✅ Basic verification workflow (document upload, review)
- ✅ Verification status tracking
- ✅ Document storage and management
- ✅ Basic fraud detection
- ✅ Verification queue management
- ✅ Email notifications for verification status
- ✅ Admin verification dashboard

## 🎯 Task Description

Implement the basic verification workflow that allows users to submit documents for profile verification and administrators to review and approve them. This establishes the foundation for trust and credibility in the profile system.

## 📋 Verification Workflow Overview

### User Journey
1. **Initiate Verification**: User clicks "Verify Profile" button
2. **Document Upload**: User uploads identity documents
3. **Information Submission**: User provides additional verification info
4. **Submission**: System validates and submits for review
5. **Review Process**: Admin reviews submission
6. **Approval/Denial**: Admin approves or denies with feedback
7. **Notification**: User receives email notification
8. **Badge Display**: Verified profiles show verification badge

### Verification Types
```cpp
enum class VerificationType {
    INDIVIDUAL_BASIC,    // National ID, photo
    INDIVIDUAL_ADVANCED, // + Address, education verification
    BUSINESS_BASIC,      // Business registration, tax ID
    BUSINESS_ADVANCED    // + Legal documents, address verification
};

enum class VerificationStatus {
    UNVERIFIED,          // Not submitted
    PENDING,             // Submitted, awaiting review
    UNDER_REVIEW,        // Admin is reviewing
    APPROVED,            // Verified successfully
    DENIED,              // Verification denied
    EXPIRED,             // Verification expired (needs renewal)
    REVOKED              // Verification revoked due to violation
};
```

## 📄 Document Management System

### Supported Document Types
```cpp
struct VerificationDocument {
    std::string documentId;
    std::string profileId;
    DocumentType type;           // ID, PHOTO, BUSINESS_LICENSE, etc.
    std::string filename;
    std::string originalFilename;
    std::string mimeType;
    long fileSize;              // bytes
    std::string storagePath;    // Encrypted storage location
    std::string checksum;       // SHA-256 for integrity
    Date uploadedAt;
    bool isEncrypted = true;    // All documents encrypted at rest
};

enum class DocumentType {
    // Individual documents
    NATIONAL_ID_FRONT,
    NATIONAL_ID_BACK,
    PASSPORT,
    DRIVING_LICENSE,
    SELFIE_WITH_ID,

    // Business documents
    BUSINESS_LICENSE,
    TAX_CERTIFICATE,
    COMPANY_REGISTRATION,
    ADDRESS_PROOF,

    // Education/Professional
    DEGREE_CERTIFICATE,
    EMPLOYMENT_LETTER,
    PROFESSIONAL_CERTIFICATE
};
```

### Document Upload System
```cpp
class DocumentUploader {
public:
    static UploadResult uploadDocument(const std::string& profileId,
                                     const DocumentUploadRequest& request);

    static bool validateDocument(const DocumentUploadRequest& request);
    static std::string generateSecureFilename(const std::string& originalName);
    static std::string calculateChecksum(const std::vector<uint8_t>& data);

private:
    static const long MAX_FILE_SIZE = 10 * 1024 * 1024;  // 10MB
    static const std::vector<std::string> ALLOWED_MIME_TYPES;
};
```

## 🔍 Verification Submission System

### Verification Request Structure
```cpp
struct VerificationRequest {
    std::string requestId;
    std::string profileId;
    VerificationType type;
    VerificationStatus status = VerificationStatus::PENDING;

    // User provided information
    std::string fullName;        // As appears on documents
    std::string nationalId;      // National ID number
    Date dateOfBirth;
    std::string phoneNumber;     // For verification SMS if needed

    // Business information (if applicable)
    std::string companyName;
    std::string businessRegistrationNumber;
    std::string taxId;

    // Documents
    std::vector<VerificationDocument> documents;

    // Metadata
    Date submittedAt;
    Date reviewedAt;            // When admin reviews
    std::string reviewedBy;     // Admin user ID
    std::string reviewNotes;    // Admin feedback
    std::string denialReason;   // If denied

    // Processing
    int processingPriority = 1; // 1=Normal, 2=High, 3=Urgent
    Date expectedCompletion;    // SLA deadline
};
```

### Submission Workflow
```cpp
class VerificationService {
public:
    static VerificationRequest submitVerificationRequest(
        const std::string& profileId,
        const VerificationSubmission& submission);

    static bool validateSubmission(const VerificationSubmission& submission);
    static void notifyUserOfSubmission(const std::string& profileId);

private:
    static std::string generateRequestId();
    static Date calculateExpectedCompletion(VerificationType type, int priority);
    static bool checkDuplicateSubmission(const std::string& profileId);
};
```

## 👨‍💼 Admin Review Dashboard

### Review Queue Management
```cpp
struct VerificationQueueItem {
    std::string requestId;
    std::string profileId;
    std::string profileName;
    VerificationType type;
    Date submittedAt;
    int priority;
    Date expectedCompletion;
    bool isOverdue;              // Past expected completion date

    // Quick preview info
    std::string submittedBy;     // User name
    int documentCount;
    std::string primaryDocumentType;
};

class VerificationAdminService {
public:
    static std::vector<VerificationQueueItem> getPendingQueue(int limit = 50);
    static VerificationRequest getRequestDetails(const std::string& requestId);
    static bool approveVerification(const std::string& requestId,
                                  const std::string& adminId,
                                  const std::string& notes = "");
    static bool denyVerification(const std::string& requestId,
                               const std::string& adminId,
                               const std::string& reason,
                               const std::string& notes = "");

private:
    static void updateProfileVerificationStatus(const std::string& profileId,
                                              VerificationStatus status);
    static void sendVerificationNotification(const std::string& profileId,
                                           VerificationStatus status,
                                           const std::string& message);
};
```

### Admin Dashboard UI Structure
```html
<div class="verification-admin-dashboard">
    <div class="dashboard-header">
        <h1>Profile Verification Queue</h1>
        <div class="queue-stats">
            <div class="stat-item">
                <span class="stat-number">47</span>
                <span class="stat-label">Pending</span>
            </div>
            <div class="stat-item">
                <span class="stat-number">12</span>
                <span class="stat-label">Overdue</span>
            </div>
            <div class="stat-item">
                <span class="stat-number">3.2h</span>
                <span class="stat-label">Avg. Review Time</span>
            </div>
        </div>
    </div>

    <div class="queue-filters">
        <select id="type-filter">
            <option value="all">All Types</option>
            <option value="INDIVIDUAL_BASIC">Individual Basic</option>
            <option value="BUSINESS_BASIC">Business Basic</option>
        </select>

        <select id="priority-filter">
            <option value="all">All Priorities</option>
            <option value="3">Urgent</option>
            <option value="2">High</option>
            <option value="1">Normal</option>
        </select>

        <input type="text" placeholder="Search by name..." id="name-search">
    </div>

    <div class="verification-queue">
        <div class="queue-item" data-request-id="req-123">
            <div class="item-header">
                <div class="profile-info">
                    <img src="/avatars/user123.jpg" class="profile-avatar">
                    <div class="profile-details">
                        <div class="profile-name">John Doe</div>
                        <div class="verification-type">Individual Basic</div>
                    </div>
                </div>

                <div class="item-meta">
                    <div class="submitted-date">2 hours ago</div>
                    <div class="priority-badge priority-normal">Normal</div>
                </div>
            </div>

            <div class="item-documents">
                <div class="document-preview">
                    <img src="/docs/preview/national-id-front.jpg" alt="National ID Front">
                    <span class="document-label">National ID (Front)</span>
                </div>

                <div class="document-preview">
                    <img src="/docs/preview/selfie.jpg" alt="Selfie">
                    <span class="document-label">Selfie with ID</span>
                </div>
            </div>

            <div class="item-actions">
                <button class="btn-approve" onclick="approveRequest('req-123')">
                    Approve
                </button>
                <button class="btn-deny" onclick="denyRequest('req-123')">
                    Deny
                </button>
                <button class="btn-view-details" onclick="viewDetails('req-123')">
                    View Details
                </button>
            </div>
        </div>

        <!-- More queue items... -->
    </div>
</div>
```

## 📧 Notification System

### Email Templates
```cpp
class VerificationNotificationService {
public:
    static void sendSubmissionConfirmation(const std::string& profileId,
                                         const VerificationRequest& request);

    static void sendApprovalNotification(const std::string& profileId,
                                       const VerificationRequest& request);

    static void sendDenialNotification(const std::string& profileId,
                                     const VerificationRequest& request,
                                     const std::string& reason);

    static void sendAdminReviewNotification(const std::string& adminId,
                                          const VerificationRequest& request);

private:
    static std::string generateSubmissionEmail(const VerificationRequest& request);
    static std::string generateApprovalEmail(const VerificationRequest& request);
    static std::string generateDenialEmail(const VerificationRequest& request,
                                         const std::string& reason);
};
```

## 🛡️ Basic Fraud Detection

### Automated Checks
```cpp
class FraudDetectionService {
public:
    static FraudCheckResult checkForFraud(const VerificationRequest& request);

private:
    static bool checkDocumentIntegrity(const std::vector<VerificationDocument>& docs);
    static bool checkDuplicateDocuments(const VerificationRequest& request);
    static bool checkSuspiciousPatterns(const VerificationRequest& request);
    static double calculateFraudScore(const VerificationRequest& request);
};

struct FraudCheckResult {
    bool isSuspicious;
    double fraudScore;          // 0.0 - 1.0
    std::vector<std::string> flags;  // Reasons for suspicion
    std::string recommendedAction;   // "APPROVE", "MANUAL_REVIEW", "DENY"
};
```

## 📋 Implementation Plan

### Day 1: Document Upload + Submission
- Implement document upload system with encryption
- Create verification request submission workflow
- Add basic validation and fraud detection
- Set up notification system for submissions

### Day 2: Admin Review + Approval
- Build admin review dashboard
- Implement approval/denial workflow
- Add verification status tracking
- Create email notifications for decisions
- Test end-to-end verification flow

## 🧪 Testing Strategy

### Submission Tests
```cpp
TEST(VerificationTest, DocumentUploadValidation) {
    // Test valid document upload
    auto result = DocumentUploader::uploadDocument("profile-123", validDocumentRequest);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.documentId.empty());

    // Test invalid document (too large)
    auto invalidResult = DocumentUploader::uploadDocument("profile-123", largeDocumentRequest);
    EXPECT_FALSE(invalidResult.success);
    EXPECT_EQ(invalidResult.error, "File too large");
}

TEST(VerificationTest, SubmissionWorkflow) {
    // Create verification submission
    auto request = VerificationService::submitVerificationRequest("profile-123", validSubmission);
    EXPECT_FALSE(request.requestId.empty());
    EXPECT_EQ(request.status, VerificationStatus::PENDING);

    // Verify profile status updated
    auto profile = profileRepository.findById("profile-123");
    EXPECT_TRUE(profile->isVerificationPending);
}
```

### Admin Review Tests
```cpp
TEST(AdminTest, ApprovalWorkflow) {
    // Submit verification request
    auto request = createTestVerificationRequest();

    // Admin approves
    bool approved = VerificationAdminService::approveVerification(
        request.requestId, "admin-123", "Looks good!");

    EXPECT_TRUE(approved);

    // Verify profile updated
    auto profile = profileRepository.findById(request.profileId);
    EXPECT_TRUE(profile->isVerified);
    EXPECT_EQ(profile->verificationStatus, VerificationStatus::APPROVED);
}

TEST(AdminTest, DenialWorkflow) {
    // Submit and deny verification
    auto request = createTestVerificationRequest();
    bool denied = VerificationAdminService::denyVerification(
        request.requestId, "admin-123", "Documents unclear", "Please resubmit with clearer images");

    EXPECT_TRUE(denied);

    // Verify denial recorded
    auto updatedRequest = verificationRepository.findById(request.requestId);
    EXPECT_EQ(updatedRequest->status, VerificationStatus::DENIED);
    EXPECT_EQ(updatedRequest->denialReason, "Documents unclear");
}
```

## 🎉 Success Criteria

### Submission & Upload
- ✅ **Document upload with encryption**
- ✅ **Verification request submission**
- ✅ **Input validation and sanitization**
- ✅ **Basic fraud detection**
- ✅ **User notifications**

### Admin Review
- ✅ **Review queue dashboard**
- ✅ **Approval/denial workflow**
- ✅ **Status tracking and updates**
- ✅ **Admin notifications**

### Security & Compliance
- ✅ **Document encryption at rest**
- ✅ **Secure file handling**
- ✅ **Audit trail for all actions**
- ✅ **Data retention compliance**

### User Experience
- ✅ **Clear submission process**
- ✅ **Progress tracking**
- ✅ **Timely notifications**
- ✅ **Helpful error messages**

This establishes the **foundation for trust** in the profile system with a **secure, user-friendly verification process**.
