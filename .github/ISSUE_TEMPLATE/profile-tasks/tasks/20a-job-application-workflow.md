# 🚀 Job Application System - Application Workflow

**Duration:** 2 days
**Dependencies:** Profile database models, Profile routing CRUD
**Acceptance Criteria:**
- ✅ Job posting creation and management
- ✅ Job application form with resume upload
- ✅ Application submission and validation
- ✅ Application status tracking
- ✅ Automated application confirmation
- ✅ Application deadline management
- ✅ Job application templates

## 🎯 Task Description

Implement the job application workflow that allows companies to post jobs on their profiles and candidates to apply with their resumes, creating a streamlined hiring process that integrates with the profile system.

## 💼 Job Posting System

### Job Posting Data Model
```cpp
enum class JobType {
    FULL_TIME,
    PART_TIME,
    CONTRACT,
    FREELANCE,
    INTERNSHIP,
    VOLUNTEER
};

enum class ExperienceLevel {
    ENTRY_LEVEL,       // 0-2 years
    MID_LEVEL,         // 2-5 years
    SENIOR_LEVEL,      // 5-10 years
    EXECUTIVE_LEVEL,   // 10+ years
    NOT_SPECIFIED
};

enum class JobStatus {
    DRAFT,             // Being created
    PUBLISHED,         // Active and visible
    PAUSED,            // Temporarily paused
    CLOSED,            // No longer accepting applications
    FILLED,            // Position filled
    CANCELLED          // Job cancelled
};

struct JobPosting {
    std::string jobId;
    std::string profileId;           // Company profile posting the job
    std::string title;
    std::string companyName;
    std::string location;            // City, Country or "Remote"

    // Job details
    JobType jobType;
    ExperienceLevel experienceLevel;
    std::string salaryRange;         // "50,000 - 70,000 USD" or "Competitive"
    std::string department;
    std::string industry;

    // Content
    std::string description;         // Job description (HTML)
    std::vector<std::string> requirements; // Required skills/experience
    std::vector<std::string> responsibilities; // Job responsibilities
    std::vector<std::string> benefits; // Benefits offered

    // Application settings
    Date applicationDeadline;
    int maxApplications = 0;         // 0 = unlimited
    bool requireResume = true;
    bool requireCoverLetter = false;
    std::vector<std::string> customQuestions; // Additional questions

    // Metadata
    JobStatus status = JobStatus::DRAFT;
    Date createdAt;
    Date publishedAt;
    Date updatedAt;
    std::string createdBy;           // User ID who created the job

    // Analytics
    int viewCount = 0;
    int applicationCount = 0;
    int shortlistedCount = 0;
    int hiredCount = 0;

    // Application form configuration
    JobApplicationForm applicationForm;
};

struct JobApplicationForm {
    std::vector<FormField> standardFields;    // Name, email, phone, resume
    std::vector<FormField> customFields;      // Company-specific questions
    bool allowMultipleApplications = false;   // One application per user
    bool requireProfileCompletion = false;    // Must have complete profile
    std::string welcomeMessage;               // Message shown before form
    std::string confirmationMessage;          // Message after submission
};
```

### Job Posting Service
```cpp
class JobPostingService {
public:
    static JobPosting createJobPosting(const JobPostingRequest& request);
    static void updateJobPosting(const std::string& jobId,
                               const JobPostingUpdate& update);
    static void publishJobPosting(const std::string& jobId);
    static void closeJobPosting(const std::string& jobId,
                              const std::string& reason = "");

    static std::vector<JobPosting> getCompanyJobPostings(
        const std::string& profileId,
        const JobFilter& filter = {});

    static std::vector<JobPosting> searchJobPostings(
        const JobSearchQuery& query);

    static JobPostingAnalytics getJobAnalytics(const std::string& jobId);

private:
    static void validateJobPosting(const JobPosting& job);
    static void indexJobForSearch(const JobPosting& job);
    static void notifyFollowersOfNewJob(const JobPosting& job);
};
```

## 📋 Job Application System

### Application Data Model
```cpp
enum class ApplicationStatus {
    SUBMITTED,         // Just submitted
    UNDER_REVIEW,      // Being reviewed
    SHORTLISTED,       // Moved to shortlist
    INTERVIEWING,      // In interview process
    OFFER_MADE,        // Job offer extended
    OFFER_ACCEPTED,    // Candidate accepted offer
    OFFER_DECLINED,    // Candidate declined offer
    HIRED,             // Successfully hired
    REJECTED,          // Application rejected
    WITHDRAWN          // Candidate withdrew application
};

struct JobApplication {
    std::string applicationId;
    std::string jobId;
    std::string applicantId;         // User ID of applicant
    ApplicationStatus status = ApplicationStatus::SUBMITTED;

    // Personal information
    std::string fullName;
    std::string email;
    std::string phone;
    std::string location;
    std::string linkedinUrl;
    std::string portfolioUrl;

    // Application content
    std::string resumeUrl;           // Uploaded resume file
    std::string coverLetter;         // Cover letter text
    std::map<std::string, std::string> customAnswers; // Answers to custom questions

    // Profile integration
    bool useProfileData = true;      // Auto-fill from profile
    std::string profileSnapshot;     // Snapshot of profile at application time

    // Workflow
    Date submittedAt;
    Date lastUpdated;
    std::string reviewedBy;          // Current reviewer
    Date reviewStartedAt;
    std::vector<ApplicationNote> notes; // Internal notes

    // Communication
    std::vector<ApplicationMessage> messages; // Messages between applicant and company
    std::vector<Interview> interviews; // Scheduled interviews

    // Analytics
    Date firstViewedAt;             // When company first viewed
    int viewCount = 0;              // How many times viewed
    double matchScore = 0.0;        // How well candidate matches job (0-100)
};

struct ApplicationNote {
    std::string noteId;
    std::string authorId;
    std::string authorName;
    std::string content;
    bool isPrivate = true;          // Only visible to company team
    Date createdAt;
};

struct ApplicationMessage {
    std::string messageId;
    std::string senderId;
    std::string senderName;
    std::string content;
    Date sentAt;
    bool isFromApplicant;           // Applicant -> Company or Company -> Applicant
};
```

### Application Submission Service
```cpp
class JobApplicationService {
public:
    static JobApplication submitApplication(const ApplicationSubmission& submission);

    static bool canUserApply(const std::string& userId, const std::string& jobId);

    static std::vector<JobApplication> getJobApplications(
        const std::string& jobId,
        const ApplicationFilter& filter = {});

    static std::vector<JobApplication> getUserApplications(
        const std::string& userId,
        const ApplicationFilter& filter = {});

    static void updateApplicationStatus(const std::string& applicationId,
                                      ApplicationStatus newStatus,
                                      const std::string& userId,
                                      const std::string& reason = "");

    static void addApplicationNote(const std::string& applicationId,
                                 const std::string& note,
                                 const std::string& authorId);

private:
    static void validateApplication(const ApplicationSubmission& submission);
    static void notifyCompanyOfNewApplication(const JobApplication& application);
    static void notifyApplicantOfStatusChange(const JobApplication& application);
    static double calculateMatchScore(const JobApplication& application,
                                    const JobPosting& job);
};
```

## 📄 Application Form Builder

### Dynamic Form System
```cpp
struct FormField {
    std::string fieldId;
    std::string label;
    FormFieldType type;             // TEXT, EMAIL, FILE_UPLOAD, SELECT, etc.
    bool isRequired = false;
    std::string placeholder;
    std::string helpText;
    std::vector<std::string> options; // For select/radio/checkbox
    std::map<std::string, std::string> validationRules;

    // Conditional logic
    std::string showIfField;        // Show only if another field has value
    std::string showIfValue;
};

class ApplicationFormBuilder {
public:
    static JobApplicationForm createStandardForm();
    static JobApplicationForm customizeForm(const JobApplicationForm& baseForm,
                                          const std::vector<FormField>& customFields);

    static std::string renderFormHtml(const JobApplicationForm& form,
                                    const std::string& jobId);

    static ApplicationSubmission parseFormSubmission(
        const std::string& formData,
        const JobApplicationForm& form);

private:
    static std::vector<FormField> getStandardFields();
    static bool validateFieldValue(const FormField& field,
                                 const std::string& value);
};
```

### Form Auto-Fill from Profile
```cpp
class ProfileAutoFillService {
public:
    static std::map<std::string, std::string> getAutoFillData(
        const std::string& userId);

    static void applyAutoFillToForm(JobApplicationForm& form,
                                  const std::map<std::string, std::string>& profileData);

    static std::vector<std::string> getMissingProfileFields(
        const std::string& userId,
        const JobApplicationForm& form);

private:
    static std::string extractProfileField(const Profile& profile,
                                         const std::string& fieldName);
    static bool isProfileDataComplete(const std::string& userId,
                                    const JobApplicationForm& form);
};
```

## ⏰ Application Workflow Management

### Status Workflow Engine
```cpp
class ApplicationWorkflowService {
public:
    static std::vector<ApplicationStatus> getNextValidStatuses(
        ApplicationStatus currentStatus);

    static bool canTransitionTo(ApplicationStatus from, ApplicationStatus to);

    static void executeStatusTransition(const std::string& applicationId,
                                      ApplicationStatus newStatus,
                                      const std::string& userId);

    static std::vector<ApplicationAction> getAvailableActions(
        const std::string& applicationId,
        const std::string& userId);

private:
    static void validateStatusTransition(ApplicationStatus from, ApplicationStatus to);
    static void executeTransitionActions(const std::string& applicationId,
                                       ApplicationStatus oldStatus,
                                       ApplicationStatus newStatus);
};

struct ApplicationAction {
    std::string actionId;
    std::string label;              // "Move to Interview", "Send Rejection Email"
    std::string description;
    ApplicationStatus resultingStatus;
    bool requiresNote = false;
    bool sendsNotification = true;
};
```

### Deadline Management
```cpp
class ApplicationDeadlineService {
public:
    static void setApplicationDeadline(const std::string& jobId,
                                     Date deadline);

    static void checkExpiredApplications();
    static void notifyExpiringDeadlines();

    static std::vector<JobApplication> getApplicationsNearDeadline(
        const std::string& jobId, int daysAhead = 7);

private:
    static void autoCloseExpiredJobs();
    static void notifyApplicantsOfExpiration(const std::string& jobId);
    static void sendDeadlineReminder(const std::string& jobId, int daysRemaining);
};
```

## 📧 Application Communication

### Automated Email System
```cpp
enum class ApplicationEmailType {
    APPLICATION_RECEIVED,           // Auto-reply to applicant
    APPLICATION_UNDER_REVIEW,       // Status update
    APPLICATION_SHORTLISTED,        // Moved to next round
    APPLICATION_REJECTED,           // Rejection notice
    INTERVIEW_INVITATION,           // Interview scheduled
    OFFER_EXTENDED,                 // Job offer
    APPLICATION_DEADLINE_WARNING,   // Deadline approaching
    JOB_CLOSED                      // Job no longer accepting applications
};

struct ApplicationEmailTemplate {
    ApplicationEmailType type;
    std::string subjectTemplate;
    std::string bodyTemplate;
    std::vector<std::string> placeholders;
    bool isAutomated = true;
};

class ApplicationEmailService {
public:
    static void sendApplicationEmail(const std::string& applicationId,
                                   ApplicationEmailType type,
                                   const std::map<std::string, std::string>& variables = {});

    static void sendBulkEmails(const std::vector<std::string>& applicationIds,
                             ApplicationEmailType type);

    static std::vector<ApplicationEmailType> getAvailableEmailTypes(
        ApplicationStatus status);

private:
    static std::string renderEmailTemplate(const ApplicationEmailTemplate& tmpl,
                                         const std::map<std::string, std::string>& variables);

    static ApplicationEmailTemplate getEmailTemplate(ApplicationEmailType type);
};
```

## 📋 Job Posting Templates

### Template System
```cpp
enum class JobTemplateCategory {
    TECHNOLOGY,
    MARKETING,
    SALES,
    DESIGN,
    MANAGEMENT,
    FINANCE,
    HEALTHCARE,
    EDUCATION
};

struct JobPostingTemplate {
    std::string templateId;
    std::string name;
    JobTemplateCategory category;
    std::string title;
    std::string description;
    std::vector<std::string> requirements;
    std::vector<std::string> responsibilities;
    std::vector<std::string> benefits;
    ExperienceLevel suggestedLevel;
    std::string salaryRange;
};

class JobTemplateService {
public:
    static std::vector<JobPostingTemplate> getTemplatesByCategory(
        JobTemplateCategory category);

    static JobPosting createJobFromTemplate(const std::string& templateId,
                                          const std::string& profileId,
                                          const JobCustomization& customizations);

    static JobPostingTemplate createCustomTemplate(
        const JobPosting& job,
        const std::string& templateName);

private:
    static std::vector<JobPostingTemplate> getDefaultTemplates();
    static void applyCustomizations(JobPosting& job,
                                  const JobCustomization& customizations);
};
```

## 💼 Application UI Components

### Job Posting Form
```html
<div class="job-posting-form">
    <div class="form-header">
        <h2>Post a New Job</h2>
        <div class="template-selector">
            <select id="job-template">
                <option value="">Start from scratch</option>
                <option value="software-engineer">Software Engineer</option>
                <option value="marketing-manager">Marketing Manager</option>
                <option value="sales-rep">Sales Representative</option>
            </select>
        </div>
    </div>

    <form class="job-form">
        <!-- Basic Information -->
        <div class="form-section">
            <h3>Basic Information</h3>

            <div class="form-row">
                <div class="form-group">
                    <label for="job-title">Job Title *</label>
                    <input type="text" id="job-title" required
                           placeholder="e.g. Senior Software Engineer">
                </div>

                <div class="form-group">
                    <label for="job-type">Job Type</label>
                    <select id="job-type">
                        <option value="FULL_TIME">Full-time</option>
                        <option value="PART_TIME">Part-time</option>
                        <option value="CONTRACT">Contract</option>
                        <option value="FREELANCE">Freelance</option>
                    </select>
                </div>
            </div>

            <div class="form-row">
                <div class="form-group">
                    <label for="location">Location</label>
                    <input type="text" id="location" placeholder="City, Country or Remote">
                </div>

                <div class="form-group">
                    <label for="experience-level">Experience Level</label>
                    <select id="experience-level">
                        <option value="ENTRY_LEVEL">Entry Level (0-2 years)</option>
                        <option value="MID_LEVEL">Mid Level (2-5 years)</option>
                        <option value="SENIOR_LEVEL">Senior Level (5-10 years)</option>
                        <option value="EXECUTIVE_LEVEL">Executive Level (10+ years)</option>
                    </select>
                </div>
            </div>
        </div>

        <!-- Job Description -->
        <div class="form-section">
            <h3>Job Description</h3>

            <div class="form-group">
                <label for="description">Description *</label>
                <textarea id="description" rows="6" required
                          placeholder="Describe the role, team, and what the candidate will do..."></textarea>
            </div>

            <div class="form-group">
                <label for="requirements">Requirements</label>
                <textarea id="requirements" rows="4"
                          placeholder="Required skills, experience, qualifications..."></textarea>
            </div>

            <div class="form-group">
                <label for="benefits">Benefits</label>
                <textarea id="benefits" rows="3"
                          placeholder="Salary, benefits, perks, work environment..."></textarea>
            </div>
        </div>

        <!-- Application Settings -->
        <div class="form-section">
            <h3>Application Settings</h3>

            <div class="form-row">
                <div class="form-group">
                    <label for="deadline">Application Deadline</label>
                    <input type="date" id="deadline">
                </div>

                <div class="form-group">
                    <label for="max-applications">Max Applications (0 = unlimited)</label>
                    <input type="number" id="max-applications" value="0" min="0">
                </div>
            </div>

            <div class="checkbox-group">
                <label class="checkbox-label">
                    <input type="checkbox" id="require-resume" checked>
                    Require resume upload
                </label>

                <label class="checkbox-label">
                    <input type="checkbox" id="require-cover-letter">
                    Require cover letter
                </label>
            </div>
        </div>

        <div class="form-actions">
            <button type="button" class="save-draft-btn">Save as Draft</button>
            <button type="submit" class="publish-btn">Publish Job</button>
        </div>
    </form>
</div>
```

## 📋 Implementation Plan

### Day 1: Job Posting + Basic Application
- Implement job posting creation and management
- Create job application form builder
- Add application submission and validation
- Build basic application workflow

### Day 1 Continued: Templates + Communication
- Implement job posting templates
- Add automated email system for applications
- Create deadline management
- Test end-to-end application process

## 🧪 Testing Strategy

### Job Posting Tests
```cpp
TEST(JobPostingTest, CreateAndPublishJob) {
    JobPostingRequest request{
        .profileId = "company-123",
        .title = "Senior Software Engineer",
        .companyName = "Tech Corp",
        .location = "San Francisco, CA",
        .jobType = JobType::FULL_TIME,
        .experienceLevel = ExperienceLevel::SENIOR_LEVEL,
        .description = "We're looking for an experienced engineer...",
        .requirements = {"5+ years experience", "React expertise"}
    };

    auto job = JobPostingService::createJobPosting(request);
    EXPECT_FALSE(job.jobId.empty());
    EXPECT_EQ(job.status, JobStatus::DRAFT);

    // Publish job
    JobPostingService::publishJobPosting(job.jobId);
    auto publishedJob = JobPostingService::getJobById(job.jobId);
    EXPECT_EQ(publishedJob.status, JobStatus::PUBLISHED);
}
```

### Application Submission Tests
```cpp
TEST(ApplicationTest, SubmitCompleteApplication) {
    // Create job and applicant
    auto jobId = createTestJob();
    auto applicantId = createTestApplicant();

    ApplicationSubmission submission{
        .jobId = jobId,
        .applicantId = applicantId,
        .fullName = "John Doe",
        .email = "john@example.com",
        .resumeUrl = "resumes/john-doe.pdf",
        .coverLetter = "I am very interested in this position...",
        .useProfileData = true
    };

    auto application = JobApplicationService::submitApplication(submission);

    // Verify application created
    EXPECT_FALSE(application.applicationId.empty());
    EXPECT_EQ(application.status, ApplicationStatus::SUBMITTED);
    EXPECT_EQ(application.jobId, jobId);
    EXPECT_EQ(application.applicantId, applicantId);
}
```

### Workflow Tests
```cpp
TEST(WorkflowTest, StatusTransitions) {
    // Create application
    auto applicationId = createTestApplication();

    // Move through workflow
    JobApplicationService::updateApplicationStatus(
        applicationId, ApplicationStatus::UNDER_REVIEW, "recruiter-123");

    auto app = JobApplicationService::getApplicationById(applicationId);
    EXPECT_EQ(app.status, ApplicationStatus::UNDER_REVIEW);

    // Shortlist application
    JobApplicationService::updateApplicationStatus(
        applicationId, ApplicationStatus::SHORTLISTED, "recruiter-123", "Strong candidate");

    app = JobApplicationService::getApplicationById(applicationId);
    EXPECT_EQ(app.status, ApplicationStatus::SHORTLISTED);
}
```

## 🎉 Success Criteria

### Job Posting
- ✅ **Job posting creation and publishing**
- ✅ **Job template system for quick setup**
- ✅ **Job search and filtering**
- ✅ **Job status management (draft, published, closed)**

### Application Process
- ✅ **Dynamic application form builder**
- ✅ **Resume upload and validation**
- ✅ **Profile auto-fill integration**
- ✅ **Application deadline management**

### Workflow Management
- ✅ **Application status tracking**
- ✅ **Automated email notifications**
- ✅ **Application review workflow**
- ✅ **Communication between applicants and companies**

### User Experience
- ✅ **Intuitive job posting interface**
- ✅ **Streamlined application process**
- ✅ **Mobile-responsive forms**
- ✅ **Clear application status updates**

This creates a **comprehensive job application system** that **connects companies with qualified candidates** through an **integrated profile-based hiring platform**.
