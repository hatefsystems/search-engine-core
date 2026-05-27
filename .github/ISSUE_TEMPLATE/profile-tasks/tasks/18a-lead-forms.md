# 🚀 Lead Generation - Contact Forms & Business Inquiry System

**Duration:** 2 days
**Dependencies:** Profile database models, Profile routing CRUD
**Acceptance Criteria:**
- ✅ Contact forms on business profile pages
- ✅ Form validation and spam prevention
- ✅ Lead categorization (quote, partnership, support)
- ✅ Lead privacy controls and consent
- ✅ Form customization options
- ✅ Mobile-responsive form design
- ✅ Lead source tracking

## 🎯 Task Description

Implement contact forms on business profiles that allow potential customers to easily reach out, while providing businesses with categorized leads and proper privacy controls to build trust and facilitate business connections.

## 📝 Contact Form System Architecture

### Form Configuration
```cpp
enum class LeadCategory {
    GENERAL_INQUIRY,        // General questions
    QUOTE_REQUEST,          // Price/service quotes
    PARTNERSHIP,            // Business partnerships
    SUPPORT,                // Technical support
    JOB_INQUIRY,            // Job opportunities
    COLLABORATION,          // Project collaboration
    MEDIA_INQUIRY,          // Press/media requests
    CUSTOM                  // Custom categories
};

enum class FormFieldType {
    TEXT,
    TEXTAREA,
    EMAIL,
    PHONE,
    SELECT,
    CHECKBOX,
    RADIO
};

struct ContactFormField {
    std::string fieldId;
    std::string label;
    FormFieldType type;
    bool isRequired = false;
    std::string placeholder;
    std::vector<std::string> options;    // For select/radio/checkbox
    std::string validationPattern;       // Regex validation
    int maxLength = 1000;
};

struct ContactForm {
    std::string formId;
    std::string profileId;
    std::string title;                   // "Get in Touch", "Request Quote"
    std::string description;            // Form description text
    std::string submitButtonText;       // "Send Message", "Request Quote"

    // Form fields
    std::vector<ContactFormField> fields;

    // Lead categories
    std::vector<LeadCategory> availableCategories;

    // Settings
    bool requireConsent = true;         // GDPR consent checkbox
    bool showCategories = true;         // Show category selection
    bool collectSourceInfo = true;      // Track where lead came from
    int maxSubmissionsPerHour = 5;      // Rate limiting

    // Status
    bool isActive = true;
    Date createdAt;
    Date lastModified;
};
```

### Default Form Templates
```cpp
class FormTemplateService {
public:
    static ContactForm createGeneralInquiryForm(const std::string& profileId);
    static ContactForm createQuoteRequestForm(const std::string& profileId);
    static ContactForm createJobApplicationForm(const std::string& profileId);

private:
    static std::vector<ContactFormField> getDefaultFields(LeadCategory category);
};

ContactForm FormTemplateService::createGeneralInquiryForm(const std::string& profileId) {
    return ContactForm{
        .formId = generateUUID(),
        .profileId = profileId,
        .title = "Get in Touch",
        .description = "Have a question or want to work together? We'd love to hear from you.",
        .submitButtonText = "Send Message",
        .fields = {
            {
                .fieldId = "name",
                .label = "Full Name",
                .type = FormFieldType::TEXT,
                .isRequired = true,
                .placeholder = "Your full name"
            },
            {
                .fieldId = "email",
                .label = "Email Address",
                .type = FormFieldType::EMAIL,
                .isRequired = true,
                .placeholder = "your.email@example.com"
            },
            {
                .fieldId = "phone",
                .label = "Phone Number (Optional)",
                .type = FormFieldType::PHONE,
                .isRequired = false,
                .placeholder = "+1 (555) 123-4567"
            },
            {
                .fieldId = "category",
                .label = "How can we help?",
                .type = FormFieldType::SELECT,
                .isRequired = true,
                .options = {"General Inquiry", "Quote Request", "Partnership", "Support"}
            },
            {
                .fieldId = "message",
                .label = "Message",
                .type = FormFieldType::TEXTAREA,
                .isRequired = true,
                .placeholder = "Tell us more about your needs...",
                .maxLength = 2000
            }
        },
        .availableCategories = {
            LeadCategory::GENERAL_INQUIRY,
            LeadCategory::QUOTE_REQUEST,
            LeadCategory::PARTNERSHIP,
            LeadCategory::SUPPORT
        }
    };
}
```

## 📋 Lead Submission System

### Lead Data Structure
```cpp
struct LeadSubmission {
    std::string leadId;
    std::string profileId;              // Business profile receiving lead
    std::string submitterId;            // User submitting (null if anonymous)
    LeadCategory category;

    // Contact information
    std::string name;
    std::string email;
    std::string phone;

    // Form data
    std::map<std::string, std::string> formData;  // Field ID -> Value

    // Metadata
    std::string ipAddress;
    std::string userAgent;
    std::string referrer;               // How they found the profile
    std::string sourceUrl;              // Profile URL they came from

    // Processing
    LeadStatus status = LeadStatus::NEW;
    Date submittedAt;
    Date firstViewedAt;                 // When business first saw it
    Date respondedAt;                   // When business responded

    // Privacy & consent
    bool hasConsent = false;            // GDPR consent given
    std::string consentText;            // What they consented to
    Date consentGivenAt;

    // Spam detection
    double spamScore = 0.0;
    bool isMarkedAsSpam = false;
    std::string spamReason;
};

enum class LeadStatus {
    NEW,                    // Just submitted
    VIEWED,                 // Business has seen it
    RESPONDED,              // Business replied
    CLOSED_WON,             // Converted to customer
    CLOSED_LOST,            // Not interested
    SPAM,                   // Marked as spam
    ARCHIVED               // Old lead
};
```

### Form Processing Service
```cpp
class LeadFormService {
public:
    static LeadSubmission processFormSubmission(
        const std::string& profileId,
        const FormSubmissionRequest& request);

    static ValidationResult validateSubmission(
        const ContactForm& form,
        const FormSubmissionRequest& request);

    static SpamCheckResult checkForSpam(const FormSubmissionRequest& request);

private:
    static std::string extractFieldValue(const FormSubmissionRequest& request,
                                       const std::string& fieldId);

    static LeadCategory determineCategory(const FormSubmissionRequest& request);

    static void sendLeadNotification(const LeadSubmission& lead);

    static void logLeadAnalytics(const LeadSubmission& lead);
};
```

## 🛡️ Form Validation & Spam Prevention

### Validation Engine
```cpp
class FormValidationService {
public:
    static ValidationResult validateField(
        const ContactFormField& field,
        const std::string& value);

    static ValidationResult validateFormSubmission(
        const ContactForm& form,
        const FormSubmissionRequest& request);

private:
    static bool validateEmail(const std::string& email);
    static bool validatePhone(const std::string& phone);
    static bool validateRequiredFields(const ContactForm& form,
                                     const FormSubmissionRequest& request);
    static bool validateFieldLength(const ContactFormField& field,
                                  const std::string& value);
    static bool validatePattern(const std::string& pattern,
                              const std::string& value);
};

struct ValidationResult {
    bool isValid = true;
    std::vector<ValidationError> errors;
    std::vector<std::string> warnings;
};

struct ValidationError {
    std::string fieldId;
    std::string errorCode;      // "required", "invalid_format", "too_long"
    std::string errorMessage;
    std::string suggestedFix;
};
```

### Spam Detection System
```cpp
class SpamDetectionService {
public:
    static SpamCheckResult analyzeSubmission(
        const FormSubmissionRequest& request,
        const std::string& profileId);

private:
    static double calculateSpamScore(const FormSubmissionRequest& request);
    static bool containsSpamPatterns(const std::string& text);
    static bool hasExcessiveLinks(const std::string& text);
    static bool isSuspiciousTiming(const std::string& ipAddress);
    static bool exceedsRateLimit(const std::string& ipAddress,
                               const std::string& profileId);
    static double checkHistoricalSpamRate(const std::string& ipAddress);
};

struct SpamCheckResult {
    double spamScore;              // 0.0 - 1.0 (higher = more spammy)
    bool isSpam = false;
    std::string riskLevel;         // "low", "medium", "high", "critical"
    std::vector<std::string> flags; // Reasons for spam score
    std::string recommendedAction;  // "allow", "flag", "block", "require_captcha"
};
```

## 🔒 Privacy & Consent Management

### GDPR Compliance
```cpp
struct PrivacyConsent {
    std::string consentId;
    std::string leadId;
    std::string consentType;           // "contact", "marketing", "data_processing"

    // Consent details
    std::string consentText;           // What they agreed to
    bool wasGiven = false;
    Date consentGivenAt;
    std::string ipAddress;             // Where consent was given
    std::string userAgent;

    // Legal compliance
    std::string legalBasis;            // "legitimate_interest", "consent"
    Date consentExpiresAt;             // When consent expires
    bool canWithdraw = true;           // Can they change their mind?
};

class PrivacyConsentService {
public:
    static PrivacyConsent recordConsent(
        const std::string& leadId,
        const std::string& consentType,
        const ConsentRequest& request);

    static bool hasValidConsent(const std::string& leadId,
                              const std::string& consentType);

    static bool withdrawConsent(const std::string& leadId,
                              const std::string& consentType);

    static std::vector<PrivacyConsent> getConsentsForLead(const std::string& leadId);

private:
    static std::string generateConsentText(const std::string& consentType);
    static Date calculateConsentExpiry(const std::string& consentType);
};
```

### Privacy Controls
```cpp
struct LeadPrivacySettings {
    // What data to collect
    bool collectName = true;
    bool collectEmail = true;
    bool collectPhone = false;         // Optional by default
    bool collectCompany = false;       // Optional by default

    // Consent requirements
    bool requireConsent = true;
    bool allowMarketingConsent = false; // Separate from contact consent

    // Data retention
    int leadRetentionDays = 365;       // How long to keep leads
    bool autoDeleteOldLeads = true;

    // Sharing controls
    bool allowLeadExport = true;
    bool allowThirdPartyIntegration = false;
    std::vector<std::string> allowedIntegrations; // CRM systems, etc.
};
```

## 📊 Lead Source Tracking

### Source Attribution
```cpp
struct LeadSource {
    std::string sourceType;        // "direct", "search", "social", "referral", "email"
    std::string sourceMedium;      // "organic", "paid", "referral", "email"
    std::string sourceCampaign;    // Campaign identifier
    std::string sourceContent;     // Content identifier

    // Technical details
    std::string referrerUrl;
    std::string landingPage;
    std::string searchQuery;       // If came from search
    std::string socialPlatform;    // "linkedin", "twitter", etc.

    // Attribution
    double attributionWeight;      // How much credit this source gets
    Date firstTouch;               // First interaction
    Date lastTouch;                // Final interaction before conversion
};

class LeadSourceTracker {
public:
    static LeadSource identifyLeadSource(const FormSubmissionRequest& request);
    static void trackSourceJourney(const std::string& leadId,
                                 const LeadSource& source);
    static std::map<std::string, int> getSourceBreakdown(
        const std::string& profileId, DateRange range);

private:
    static std::string extractReferrerType(const std::string& referrer);
    static std::string extractSearchQuery(const std::string& referrer);
    static std::string extractSocialPlatform(const std::string& referrer);
};
```

## 🎨 Form UI Components

### Responsive Contact Form HTML
```html
<div class="contact-form-container">
    <div class="form-header">
        <h3>Get in Touch</h3>
        <p class="form-description">
            Have a question or want to work together? We'd love to hear from you.
        </p>
    </div>

    <form class="contact-form" data-profile-id="profile-123">
        <!-- Name Field -->
        <div class="form-group">
            <label for="name">Full Name *</label>
            <input type="text" id="name" name="name" required
                   placeholder="Your full name" maxlength="100">
            <div class="field-error" data-field="name"></div>
        </div>

        <!-- Email Field -->
        <div class="form-group">
            <label for="email">Email Address *</label>
            <input type="email" id="email" name="email" required
                   placeholder="your.email@example.com">
            <div class="field-error" data-field="email"></div>
        </div>

        <!-- Phone Field (Optional) -->
        <div class="form-group">
            <label for="phone">Phone Number</label>
            <input type="tel" id="phone" name="phone"
                   placeholder="+1 (555) 123-4567">
            <div class="field-hint">Optional - for faster response</div>
        </div>

        <!-- Category Selection -->
        <div class="form-group">
            <label for="category">How can we help? *</label>
            <select id="category" name="category" required>
                <option value="">Select a category...</option>
                <option value="general">General Inquiry</option>
                <option value="quote">Request Quote</option>
                <option value="partnership">Business Partnership</option>
                <option value="support">Technical Support</option>
            </select>
        </div>

        <!-- Message Field -->
        <div class="form-group">
            <label for="message">Message *</label>
            <textarea id="message" name="message" required
                      placeholder="Tell us more about your needs..."
                      maxlength="2000" rows="5"></textarea>
            <div class="character-count">
                <span class="current-count">0</span>/2000 characters
            </div>
        </div>

        <!-- Consent Checkbox -->
        <div class="form-group consent-group">
            <label class="checkbox-label">
                <input type="checkbox" name="consent" required>
                <span class="checkmark"></span>
                I agree to be contacted about my inquiry and understand my data will be processed according to the privacy policy.
            </label>
        </div>

        <!-- Submit Button -->
        <div class="form-actions">
            <button type="submit" class="submit-btn" disabled>
                <span class="btn-text">Send Message</span>
                <span class="btn-spinner hidden"></span>
            </button>
        </div>

        <!-- Success/Error Messages -->
        <div class="form-messages">
            <div class="success-message hidden">
                ✅ Thank you! We'll get back to you within 24 hours.
            </div>
            <div class="error-message hidden">
                ❌ Something went wrong. Please try again.
            </div>
        </div>
    </form>
</div>
```

## 📋 Implementation Plan

### Day 1: Form System + Validation
- Implement contact form data structures and templates
- Create form validation engine
- Add spam detection and prevention
- Build basic form submission processing

### Day 2: Privacy + Source Tracking
- Implement privacy consent management
- Add lead source tracking and attribution
- Create form UI components
- Test end-to-end form submission flow

## 🧪 Testing Strategy

### Form Validation Tests
```cpp
TEST(FormValidationTest, RequiredFields) {
    auto form = createGeneralInquiryForm("profile-123");

    // Missing required fields
    FormSubmissionRequest invalidRequest{
        .formData = {{"name", ""}, {"email", "invalid-email"}}
    };

    auto result = FormValidationService::validateFormSubmission(form, invalidRequest);
    EXPECT_FALSE(result.isValid);
    EXPECT_TRUE(hasError(result.errors, "name", "required"));
    EXPECT_TRUE(hasError(result.errors, "email", "invalid_format"));
}

TEST(FormValidationTest, EmailValidation) {
    EXPECT_TRUE(FormValidationService::validateEmail("user@example.com"));
    EXPECT_TRUE(FormValidationService::validateEmail("test.email+tag@domain.co.uk"));
    EXPECT_FALSE(FormValidationService::validateEmail("invalid-email"));
    EXPECT_FALSE(FormValidationService::validateEmail("@domain.com"));
}
```

### Spam Detection Tests
```cpp
TEST(SpamDetectionTest, SpamPatternRecognition) {
    // Legitimate message
    FormSubmissionRequest cleanRequest{
        .formData = {{"message", "Hello, I'm interested in your services."}}
    };
    auto cleanResult = SpamDetectionService::analyzeSubmission(cleanRequest, "profile-123");
    EXPECT_LT(cleanResult.spamScore, 0.3);

    // Spam message
    FormSubmissionRequest spamRequest{
        .formData = {{"message", "BUY NOW!!! CHEAP WATCHES!!! http://spam-site.ru"}}
    };
    auto spamResult = SpamDetectionService::analyzeSubmission(spamRequest, "profile-123");
    EXPECT_GT(spamResult.spamScore, 0.8);
    EXPECT_TRUE(spamResult.isSpam);
}
```

### Lead Processing Tests
```cpp
TEST(LeadProcessingTest, SuccessfulSubmission) {
    auto form = createGeneralInquiryForm("profile-123");

    FormSubmissionRequest request{
        .formData = {
            {"name", "John Doe"},
            {"email", "john@example.com"},
            {"category", "quote"},
            {"message", "I'd like a quote for your services."}
        },
        .consent = true
    };

    auto lead = LeadFormService::processFormSubmission("profile-123", request);
    EXPECT_FALSE(lead.leadId.empty());
    EXPECT_EQ(lead.status, LeadStatus::NEW);
    EXPECT_EQ(lead.category, LeadCategory::QUOTE_REQUEST);
    EXPECT_TRUE(lead.hasConsent);
}
```

## 🎉 Success Criteria

### Form Functionality
- ✅ **Contact forms on business profiles**
- ✅ **Form validation and error handling**
- ✅ **Lead categorization system**
- ✅ **Mobile-responsive design**

### Privacy & Compliance
- ✅ **GDPR consent management**
- ✅ **Privacy controls for data collection**
- ✅ **Lead retention policies**
- ✅ **Data export capabilities**

### Spam Prevention
- ✅ **Automated spam detection**
- ✅ **Rate limiting and abuse prevention**
- ✅ **Manual spam reporting**
- ✅ **Captcha integration points**

### Analytics & Tracking
- ✅ **Lead source attribution**
- ✅ **Conversion tracking setup**
- ✅ **Form performance analytics**
- ✅ **A/B testing framework foundation**

This creates a **professional lead generation system** that **respects privacy** while **effectively connecting businesses with potential customers**.
