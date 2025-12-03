# 🚀 Job Application System - Candidate Management Dashboard

**Duration:** 2 days
**Dependencies:** 20a-job-application-workflow.md (application workflow)
**Acceptance Criteria:**
- ✅ Candidate dashboard for companies
- ✅ Application screening and filtering tools
- ✅ Candidate comparison and ranking
- ✅ Interview scheduling and management
- ✅ Offer management and tracking
- ✅ Candidate communication tools
- ✅ Hiring analytics and reporting

## 🎯 Task Description

Implement comprehensive candidate management tools that allow companies to efficiently review, compare, and manage job applications throughout the hiring process, from initial screening to final offers.

## 📊 Candidate Management Dashboard

### Dashboard Overview
```cpp
struct CandidateDashboardData {
    std::string profileId;          // Company profile ID
    DateRange period;

    // Active jobs summary
    std::vector<JobSummary> activeJobs;
    int totalActiveJobs;
    int totalApplications;
    int applicationsThisWeek;

    // Application pipeline
    std::map<ApplicationStatus, int> applicationsByStatus;
    std::vector<PipelineStage> pipelineStages;

    // Recent activity
    std::vector<RecentApplication> recentApplications;
    std::vector<UpcomingInterview> upcomingInterviews;
    std::vector<PendingAction> pendingActions;

    // Key metrics
    DashboardMetric avgTimeToReview;
    DashboardMetric offerAcceptanceRate;
    DashboardMetric timeToHire;
    DashboardMetric qualityOfHire;
};

struct JobSummary {
    std::string jobId;
    std::string title;
    int applicationCount;
    int newApplications;            // Last 7 days
    Date deadline;
    bool isUrgent;                  // Deadline approaching
};
```

### Dashboard Service
```cpp
class CandidateDashboardService {
public:
    static CandidateDashboardData getDashboardData(
        const std::string& profileId,
        const DashboardFilter& filter = {});

    static std::vector<CandidateApplication> getApplicationsForJob(
        const std::string& jobId,
        const ApplicationFilter& filter = {});

    static CandidateComparison compareCandidates(
        const std::vector<std::string>& applicationIds);

private:
    static void calculatePipelineMetrics(CandidateDashboardData& data);
    static void identifyPendingActions(CandidateDashboardData& data);
    static void generateInsights(CandidateDashboardData& data);
};
```

## 🔍 Application Screening Tools

### Advanced Filtering System
```cpp
struct ApplicationFilter {
    // Basic filters
    std::vector<ApplicationStatus> statuses;
    DateRange submittedDateRange;
    std::vector<std::string> jobIds;

    // Candidate filters
    std::string location;
    ExperienceLevel experienceLevel;
    std::vector<std::string> requiredSkills;
    double minMatchScore = 0.0;

    // Application content filters
    bool hasResume = true;
    bool hasCoverLetter = false;
    std::vector<std::string> keywords;          // Must contain these words
    std::vector<std::string> excludedKeywords;  // Must NOT contain these

    // Review status
    bool reviewed = false;
    std::string reviewedBy;
    DateRange reviewDateRange;

    // Sorting
    ApplicationSortField sortBy = ApplicationSortField::SUBMITTED_DATE;
    SortDirection sortDirection = SortDirection::DESCENDING;
};

enum class ApplicationSortField {
    SUBMITTED_DATE,
    MATCH_SCORE,
    REVIEW_DATE,
    CANDIDATE_NAME,
    EXPERIENCE_LEVEL
};

class ApplicationFilteringService {
public:
    static std::vector<CandidateApplication> filterApplications(
        const std::vector<CandidateApplication>& applications,
        const ApplicationFilter& filter);

    static std::vector<ApplicationFilter> getRecommendedFilters(
        const std::string& jobId);

    static ApplicationFilter createSmartFilter(
        const std::string& jobId,
        const std::string& criteria);

private:
    static bool matchesFilter(const CandidateApplication& app,
                            const ApplicationFilter& filter);
    static double calculateKeywordRelevance(const std::string& text,
                                          const std::vector<std::string>& keywords);
};
```

### Bulk Actions
```cpp
enum class BulkActionType {
    MOVE_TO_STATUS,         // Change status for multiple applications
    SEND_EMAIL,            // Send email to multiple candidates
    DOWNLOAD_RESUMES,      // Download resumes as ZIP
    ADD_TO_SHORTLIST,      // Add to shortlist
    SCHEDULE_INTERVIEWS,   // Schedule interviews
    REJECT_APPLICATIONS,   // Reject multiple applications
    EXPORT_DATA           // Export application data
};

struct BulkActionRequest {
    BulkActionType actionType;
    std::vector<std::string> applicationIds;
    std::map<std::string, std::string> actionParameters; // Status, email template, etc.
    std::string performedBy;
};

class BulkActionService {
public:
    static BulkActionResult executeBulkAction(const BulkActionRequest& request);

    static bool validateBulkAction(const BulkActionRequest& request);

    static BulkActionPreview previewBulkAction(const BulkActionRequest& request);

private:
    static void executeStatusChange(const BulkActionRequest& request);
    static void executeEmailSend(const BulkActionRequest& request);
    static void executeResumeDownload(const BulkActionRequest& request);
};
```

## ⚖️ Candidate Comparison Tools

### Comparison Framework
```cpp
struct CandidateComparison {
    std::vector<CandidateProfile> candidates;
    std::vector<ComparisonCriteria> criteria;
    std::vector<ComparisonResult> results;

    // Summary
    std::string bestOverallCandidate;
    std::map<std::string, std::string> topCandidatesByCriteria;
};

struct CandidateProfile {
    std::string applicationId;
    std::string candidateName;
    std::string candidateEmail;
    std::string currentPosition;
    std::string experienceLevel;
    std::vector<std::string> skills;
    double matchScore;
    ApplicationStatus status;

    // Detailed info
    std::string resumeUrl;
    std::string coverLetter;
    std::map<std::string, std::string> customAnswers;
    std::vector<ApplicationNote> notes;
};

struct ComparisonCriteria {
    std::string criteriaId;
    std::string name;               // "Experience", "Skills Match", "Culture Fit"
    CriteriaType type;              // NUMERIC, TEXT, SCORE
    double weight = 1.0;           // Importance weight
};

struct ComparisonResult {
    std::string criteriaId;
    std::map<std::string, CriteriaValue> candidateValues;
    std::string winner;             // Application ID of best candidate for this criteria
};

class CandidateComparisonService {
public:
    static CandidateComparison compareCandidates(
        const std::vector<std::string>& applicationIds,
        const std::vector<std::string>& criteriaIds = {});

    static std::vector<ComparisonCriteria> getDefaultCriteria();

    static CandidateRanking rankCandidates(
        const CandidateComparison& comparison);

private:
    static void extractCandidateData(CandidateProfile& profile,
                                   const JobApplication& application);
    static void calculateComparisonScores(CandidateComparison& comparison);
    static std::string determineCriteriaWinner(const ComparisonResult& result);
};
```

## 📅 Interview Scheduling System

### Interview Management
```cpp
enum class InterviewType {
    PHONE_SCREEN,
    VIDEO_INTERVIEW,
    IN_PERSON_INTERVIEW,
    TECHNICAL_TEST,
    CASE_STUDY,
    PANEL_INTERVIEW,
    FINAL_INTERVIEW
};

enum class InterviewStatus {
    SCHEDULED,
    CONFIRMED,
    COMPLETED,
    CANCELLED,
    NO_SHOW
};

struct Interview {
    std::string interviewId;
    std::string applicationId;
    InterviewType type;
    InterviewStatus status = InterviewStatus::SCHEDULED;

    // Scheduling
    Date scheduledAt;
    int durationMinutes = 60;
    std::string timeZone;
    std::string location;           // For in-person interviews

    // Participants
    std::vector<InterviewParticipant> interviewers;
    InterviewParticipant candidate;

    // Content
    std::string agenda;
    std::string preparationMaterials; // Links, documents for candidate
    std::vector<std::string> evaluationCriteria;

    // Communication
    std::string meetingLink;        // Zoom, Google Meet, etc.
    std::string dialInNumber;       // For phone interviews

    // Results
    InterviewResult result;
    Date completedAt;
    std::vector<InterviewFeedback> feedback;
};

struct InterviewParticipant {
    std::string userId;
    std::string name;
    std::string email;
    std::string role;              // "Interviewer", "Candidate", "Observer"
};

struct InterviewResult {
    std::string overallRating;      // "Strong Hire", "Hire", "Maybe", "No Hire"
    std::string recommendation;     // Next steps
    std::vector<std::string> strengths;
    std::vector<std::string> concerns;
    std::string notes;
};

class InterviewSchedulingService {
public:
    static Interview scheduleInterview(const InterviewRequest& request);

    static void updateInterviewStatus(const std::string& interviewId,
                                    InterviewStatus status);

    static std::vector<Interview> getUpcomingInterviews(
        const std::string& profileId);

    static InterviewResult submitInterviewResult(
        const std::string& interviewId,
        const InterviewResult& result);

private:
    static void sendInterviewInvitations(const Interview& interview);
    static void scheduleFollowUpActions(const Interview& interview);
    static void updateApplicationStatus(const std::string& applicationId,
                                      const InterviewResult& result);
};
```

## 💰 Offer Management System

### Offer Tracking
```cpp
enum class OfferStatus {
    DRAFT,              // Being prepared
    SENT,               // Sent to candidate
    VIEWED,             // Candidate viewed offer
    UNDER_REVIEW,       // Candidate reviewing
    ACCEPTED,           // Candidate accepted
    DECLINED,           // Candidate declined
    COUNTER_OFFER,      // Candidate made counter-offer
    EXPIRED            // Offer expired
};

struct JobOffer {
    std::string offerId;
    std::string applicationId;
    OfferStatus status = OfferStatus::DRAFT;

    // Offer details
    std::string jobTitle;
    std::string compensation;       // Salary, benefits package
    Date startDate;
    std::string employmentType;    // Full-time, Contract, etc.
    std::string location;

    // Terms
    std::string offerLetterUrl;    // PDF of formal offer
    Date expirationDate;
    std::string specialConditions;

    // Communication
    Date sentAt;
    Date viewedAt;
    Date respondedAt;
    std::string response;          // Acceptance/decline reason
    std::vector<OfferCommunication> communications;

    // Analytics
    int viewCount = 0;
    bool requiresSignature = true;
    Date signedAt;                // When candidate signed
};

struct OfferCommunication {
    std::string messageId;
    std::string senderId;
    std::string senderName;
    std::string content;
    Date sentAt;
    bool isFromCompany;           // Company -> Candidate or Candidate -> Company
};

class OfferManagementService {
public:
    static JobOffer createOffer(const OfferRequest& request);

    static void sendOffer(const std::string& offerId);

    static void updateOfferStatus(const std::string& offerId,
                                OfferStatus status,
                                const std::string& notes = "");

    static std::vector<JobOffer> getOffersForJob(
        const std::string& jobId,
        const OfferFilter& filter = {});

    static OfferAnalytics getOfferAnalytics(const std::string& profileId);

private:
    static void generateOfferLetter(const JobOffer& offer);
    static void notifyCandidateOfOffer(const JobOffer& offer);
    static void trackOfferResponse(const JobOffer& offer);
};
```

## 📊 Hiring Analytics & Reporting

### Comprehensive Analytics
```cpp
struct HiringAnalytics {
    std::string profileId;
    DateRange period;

    // Application metrics
    int totalApplications;
    int applicationsPerJob;
    double applicationConversionRate;  // Applications -> Hires

    // Time metrics
    double avgTimeToReview;           // Days from application to review
    double avgTimeToInterview;        // Days from review to interview
    double avgTimeToOffer;           // Days from interview to offer
    double avgTimeToHire;            // Days from application to hire

    // Quality metrics
    double offerAcceptanceRate;
    double hireRetentionRate;        // % of hires still employed after 90 days
    double candidateSatisfaction;    // From exit interviews/surveys

    // Source effectiveness
    std::map<std::string, SourceMetrics> applicationSources;

    // Diversity metrics (optional)
    DiversityMetrics diversityStats;

    // Cost metrics
    double costPerHire;
    double costPerApplication;
    std::vector<CostBreakdown> costBreakdown;
};

struct SourceMetrics {
    std::string source;              // "Profile Application", "Referral", "LinkedIn"
    int applicationCount;
    int hireCount;
    double conversionRate;
    double qualityScore;             // Average hire performance
};

class HiringAnalyticsService {
public:
    static HiringAnalytics getHiringAnalytics(
        const std::string& profileId,
        DateRange range);

    static HiringReport generateHiringReport(
        const std::string& profileId,
        DateRange range);

    static std::vector<HiringInsight> getHiringInsights(
        const std::string& profileId);

private:
    static void calculateTimeMetrics(HiringAnalytics& analytics);
    static void analyzeSourceEffectiveness(HiringAnalytics& analytics);
    static std::vector<HiringInsight> generateInsights(const HiringAnalytics& analytics);
};
```

## 💬 Candidate Communication Tools

### Communication Hub
```cpp
class CandidateCommunicationService {
public:
    static void sendMessageToCandidate(const std::string& applicationId,
                                     const std::string& message,
                                     const std::string& senderId);

    static std::vector<CandidateMessage> getCandidateMessages(
        const std::string& applicationId);

    static void scheduleAutomatedMessage(const std::string& applicationId,
                                       const std::string& templateId,
                                       Date sendAt);

    static std::vector<CommunicationTemplate> getCommunicationTemplates(
        ApplicationStatus status);

private:
    static void logCommunication(const CandidateMessage& message);
    static void updateApplicationLastContacted(const std::string& applicationId);
    static bool shouldSendAutomatedMessage(const std::string& applicationId,
                                         const std::string& templateId);
};
```

## 📋 Implementation Plan

### Day 1: Dashboard + Screening Tools
- Implement candidate management dashboard
- Create application filtering and search tools
- Add bulk actions for efficient processing
- Build candidate comparison framework

### Day 2: Interview + Offer Management
- Implement interview scheduling system
- Create offer management and tracking
- Add hiring analytics and reporting
- Build candidate communication tools

## 🧪 Testing Strategy

### Dashboard Tests
```cpp
TEST(DashboardTest, LoadDashboardData) {
    auto profileId = createCompanyWithApplications();

    auto dashboard = CandidateDashboardService::getDashboardData(profileId);

    EXPECT_GT(dashboard.totalApplications, 0);
    EXPECT_FALSE(dashboard.activeJobs.empty());
    EXPECT_FALSE(dashboard.applicationsByStatus.empty());
}
```

### Filtering Tests
```cpp
TEST(FilteringTest, AdvancedApplicationFiltering) {
    auto applications = createTestApplications();

    ApplicationFilter filter{
        .statuses = {ApplicationStatus::SUBMITTED, ApplicationStatus::UNDER_REVIEW},
        .requiredSkills = {"JavaScript", "React"},
        .minMatchScore = 70.0,
        .keywords = {"team player"}
    };

    auto filtered = ApplicationFilteringService::filterApplications(applications, filter);

    // Verify all filtered applications meet criteria
    for (const auto& app : filtered) {
        EXPECT_GE(app.matchScore, 70.0);
        EXPECT_TRUE(containsSkills(app.skills, {"JavaScript", "React"}));
    }
}
```

### Comparison Tests
```cpp
TEST(ComparisonTest, CandidateComparison) {
    auto applicationIds = createTestApplicationsForJob(5);

    auto comparison = CandidateComparisonService::compareCandidates(applicationIds);

    EXPECT_EQ(comparison.candidates.size(), 5);
    EXPECT_FALSE(comparison.results.empty());
    EXPECT_FALSE(comparison.bestOverallCandidate.empty());
}
```

## 🎉 Success Criteria

### Dashboard & Overview
- ✅ **Comprehensive candidate dashboard**
- ✅ **Real-time application pipeline view**
- ✅ **Key hiring metrics and KPIs**
- ✅ **Pending actions and reminders**

### Screening & Filtering
- ✅ **Advanced application filtering**
- ✅ **Bulk actions for efficiency**
- ✅ **Smart search and sorting**
- ✅ **Automated candidate scoring**

### Comparison & Evaluation
- ✅ **Side-by-side candidate comparison**
- ✅ **Customizable evaluation criteria**
- ✅ **Candidate ranking and shortlisting**
- ✅ **Interview feedback collection**

### Interview & Offer Management
- ✅ **Interview scheduling and tracking**
- ✅ **Offer creation and management**
- ✅ **Candidate communication tools**
- ✅ **Hiring process automation**

### Analytics & Reporting
- ✅ **Comprehensive hiring analytics**
- ✅ **Source effectiveness tracking**
- ✅ **Time-to-hire and cost metrics**
- ✅ **Performance benchmarking**

This creates a **professional-grade ATS (Applicant Tracking System)** integrated with profiles, enabling **efficient and effective hiring workflows** from application to offer.
