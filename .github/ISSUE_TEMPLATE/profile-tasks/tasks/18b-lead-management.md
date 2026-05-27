# 🚀 Lead Generation - Lead Tracking, Analytics & Management Dashboard

**Duration:** 2 days
**Dependencies:** 18a-lead-forms.md (contact form system)
**Acceptance Criteria:**
- ✅ Lead status tracking and workflow management
- ✅ Lead analytics and conversion metrics
- ✅ Lead management dashboard for businesses
- ✅ Lead export and CRM integration
- ✅ Automated follow-up and reminder system
- ✅ Lead quality scoring and prioritization
- ✅ Lead response time tracking and analytics

## 🎯 Task Description

Implement comprehensive lead management capabilities that allow businesses to track, analyze, and convert leads from profile contact forms into customers, with powerful analytics and workflow management tools.

## 📊 Lead Management System

### Lead Status Workflow
```cpp
enum class LeadStatus {
    NEW,                    // Fresh submission
    VIEWED,                 // Business has seen the lead
    CONTACTED,              // Initial response sent
    QUALIFIED,              // Lead meets business criteria
    PROPOSAL_SENT,          // Quote/proposal provided
    NEGOTIATING,            // In negotiation phase
    CLOSED_WON,             // Converted to customer
    CLOSED_LOST,            // Lost opportunity
    SPAM,                   // Marked as spam
    ARCHIVED               // Old lead archived
};

enum class LeadPriority {
    LOW,                    // Standard priority
    MEDIUM,                 // Needs attention soon
    HIGH,                   // Urgent response needed
    CRITICAL               // Immediate action required
};

struct LeadWorkflow {
    std::string leadId;
    LeadStatus currentStatus;
    LeadPriority priority;
    std::string assignedTo;         // Business team member
    Date statusUpdatedAt;
    std::string statusUpdatedBy;

    // Workflow history
    std::vector<StatusChange> statusHistory;
    std::vector<LeadActivity> activities;

    // Deadlines
    Date firstResponseDeadline;     // When to first respond
    Date closeDeadline;             // When to close/follow up
    bool isOverdue = false;
};

struct StatusChange {
    LeadStatus oldStatus;
    LeadStatus newStatus;
    Date changedAt;
    std::string changedBy;
    std::string reason;
    std::string notes;
};

struct LeadActivity {
    std::string activityId;
    std::string activityType;       // "email_sent", "call_made", "meeting_scheduled"
    std::string description;
    Date activityAt;
    std::string performedBy;
    std::map<std::string, std::string> metadata;  // Additional data
};
```

### Lead Management Service
```cpp
class LeadManagementService {
public:
    static void updateLeadStatus(const std::string& leadId,
                               LeadStatus newStatus,
                               const std::string& userId,
                               const std::string& reason = "");

    static void assignLead(const std::string& leadId,
                         const std::string& assignToUserId,
                         const std::string& assignedByUserId);

    static std::vector<LeadWorkflow> getLeadsForUser(
        const std::string& userId,
        const LeadFilter& filter = {});

    static LeadAnalytics getLeadAnalytics(
        const std::string& profileId,
        DateRange range);

    static void addLeadActivity(const std::string& leadId,
                              const LeadActivity& activity);

private:
    static void notifyStatusChange(const LeadWorkflow& lead, LeadStatus oldStatus);
    static void updateLeadPriority(const std::string& leadId);
    static void checkDeadlines();
    static void sendDeadlineReminders();
};
```

## 📈 Lead Analytics & Metrics

### Comprehensive Analytics
```cpp
struct LeadAnalytics {
    std::string profileId;
    DateRange period;

    // Volume metrics
    int totalLeads;
    int newLeads;
    std::map<LeadStatus, int> leadsByStatus;
    std::map<LeadCategory, int> leadsByCategory;

    // Quality metrics
    double averageLeadScore;        // Quality score 0-100
    double conversionRate;          // Leads that become customers
    double responseRate;            // Leads that get responses
    double closeRate;               // Won deals / total qualified leads

    // Time metrics
    double averageResponseTime;     // Hours to first response
    double averageCloseTime;        // Days to close won deals
    std::map<std::string, double> responseTimeByCategory;

    // Source metrics
    std::map<std::string, int> leadsBySource;    // "direct", "search", "social"
    std::map<std::string, double> conversionBySource;

    // Geographic metrics
    std::map<std::string, int> leadsByCountry;
    std::map<std::string, int> leadsByCity;

    // Trend metrics
    std::vector<DailyLeadMetrics> dailyTrends;
    double weekOverWeekChange;
    double monthOverMonthChange;

    // Business impact
    double estimatedRevenue;        // From closed deals
    double leadValue;               // Average value per lead
    std::vector<TopLeadSources> topSources;
};

struct DailyLeadMetrics {
    Date date;
    int newLeads;
    int respondedLeads;
    int closedLeads;
    double conversionRate;
};

struct TopLeadSources {
    std::string source;
    int leadCount;
    double conversionRate;
    double estimatedValue;
};
```

### Lead Quality Scoring
```cpp
class LeadScoringService {
public:
    static double calculateLeadScore(const LeadSubmission& lead);

    static LeadQualityAssessment assessLeadQuality(const LeadSubmission& lead);

private:
    static double scoreContactInfo(const LeadSubmission& lead);     // Email, phone validity
    static double scoreEngagement(const LeadSubmission& lead);      // Message quality, length
    static double scoreIntent(const LeadSubmission& lead);          // Category clarity, urgency
    static double scoreSource(const LeadSubmission& lead);          // Source reputation
    static double scoreTiming(const LeadSubmission& lead);          // Submission time patterns
};

struct LeadQualityAssessment {
    double overallScore;           // 0-100
    std::string qualityTier;       // "A", "B", "C", "D"
    std::vector<std::string> strengths;
    std::vector<std::string> concerns;
    std::vector<std::string> recommendations;  // How to improve engagement
    bool needsImmediateAttention;
};
```

## 📊 Lead Management Dashboard

### Dashboard Components
```cpp
struct LeadDashboardData {
    // Summary cards
    DashboardMetric totalLeads;
    DashboardMetric newLeadsToday;
    DashboardMetric responseRate;
    DashboardMetric conversionRate;

    // Status breakdown
    std::vector<StatusMetric> statusBreakdown;

    // Recent leads
    std::vector<RecentLead> recentLeads;

    // Priority alerts
    std::vector<PriorityAlert> priorityAlerts;

    // Charts data
    LeadChartData leadsOverTime;
    LeadChartData conversionFunnel;
    LeadChartData sourceBreakdown;

    // Action items
    std::vector<ActionItem> actionItems;
};

struct DashboardMetric {
    std::string label;
    std::string value;
    std::string change;             // "+12%", "-5%"
    std::string trend;              // "up", "down", "stable"
    std::string color;              // "success", "warning", "danger"
};

struct RecentLead {
    std::string leadId;
    std::string contactName;
    std::string company;
    LeadCategory category;
    LeadStatus status;
    Date submittedAt;
    bool isNew = true;
    bool isOverdue = false;
};

struct PriorityAlert {
    std::string alertId;
    std::string type;               // "overdue_response", "high_quality_lead", "bulk_submission"
    std::string message;
    std::string actionUrl;
    std::string severity;           // "low", "medium", "high", "critical"
};

class LeadDashboardService {
public:
    static LeadDashboardData getDashboardData(
        const std::string& profileId,
        const DashboardFilter& filter = {});

    static std::vector<PriorityAlert> getPriorityAlerts(
        const std::string& profileId);

    static std::vector<ActionItem> getRecommendedActions(
        const std::string& profileId);

private:
    static void calculateMetrics(LeadDashboardData& data, const std::string& profileId);
    static void generatePriorityAlerts(LeadDashboardData& data);
    static void prepareChartData(LeadDashboardData& data);
};
```

### Dashboard UI Structure
```html
<div class="lead-management-dashboard">
    <!-- Header -->
    <div class="dashboard-header">
        <div class="dashboard-title">
            <h1>Lead Management</h1>
            <div class="date-range-selector">
                <button class="active">Last 7 days</button>
                <button>Last 30 days</button>
                <button>Last 90 days</button>
            </div>
        </div>

        <div class="dashboard-actions">
            <button class="export-btn">Export Leads</button>
            <button class="settings-btn">Dashboard Settings</button>
        </div>
    </div>

    <!-- Metrics Cards -->
    <div class="metrics-grid">
        <div class="metric-card">
            <div class="metric-icon">📥</div>
            <div class="metric-content">
                <div class="metric-value">47</div>
                <div class="metric-label">Total Leads</div>
                <div class="metric-change positive">+23%</div>
            </div>
        </div>

        <div class="metric-card">
            <div class="metric-content">
                <div class="metric-value">12</div>
                <div class="metric-label">New Today</div>
                <div class="metric-change positive">+8%</div>
            </div>
        </div>

        <div class="metric-card">
            <div class="metric-content">
                <div class="metric-value">94%</div>
                <div class="metric-label">Response Rate</div>
                <div class="metric-change positive">+5%</div>
            </div>
        </div>

        <div class="metric-card">
            <div class="metric-content">
                <div class="metric-value">23%</div>
                <div class="metric-label">Conversion Rate</div>
                <div class="metric-change negative">-2%</div>
            </div>
        </div>
    </div>

    <!-- Priority Alerts -->
    <div class="priority-alerts">
        <div class="alert alert-critical">
            <div class="alert-icon">⚠️</div>
            <div class="alert-content">
                <div class="alert-title">5 leads waiting response for 24+ hours</div>
                <div class="alert-action">
                    <a href="/leads?filter=overdue">View Overdue Leads</a>
                </div>
            </div>
        </div>

        <div class="alert alert-high">
            <div class="alert-icon">⭐</div>
            <div class="alert-content">
                <div class="alert-title">High-quality lead from Fortune 500 company</div>
                <div class="alert-action">
                    <a href="/leads/lead-123">View Lead</a>
                </div>
            </div>
        </div>
    </div>

    <!-- Recent Leads Table -->
    <div class="leads-table-container">
        <div class="table-header">
            <h3>Recent Leads</h3>
            <div class="table-filters">
                <select id="status-filter">
                    <option value="all">All Status</option>
                    <option value="new">New</option>
                    <option value="contacted">Contacted</option>
                    <option value="qualified">Qualified</option>
                </select>
            </div>
        </div>

        <table class="leads-table">
            <thead>
                <tr>
                    <th>Contact</th>
                    <th>Category</th>
                    <th>Status</th>
                    <th>Submitted</th>
                    <th>Score</th>
                    <th>Actions</th>
                </tr>
            </thead>
            <tbody>
                <tr class="lead-row" data-lead-id="lead-123">
                    <td>
                        <div class="contact-info">
                            <div class="contact-name">John Smith</div>
                            <div class="contact-company">Tech Corp</div>
                            <div class="contact-email">john@techcorp.com</div>
                        </div>
                    </td>
                    <td><span class="category-badge quote">Quote Request</span></td>
                    <td><span class="status-badge new">New</span></td>
                    <td>2 hours ago</td>
                    <td>
                        <div class="score-container">
                            <div class="score-bar">
                                <div class="score-fill" style="width: 85%"></div>
                            </div>
                            <span class="score-text">85</span>
                        </div>
                    </td>
                    <td>
                        <button class="action-btn view-btn">View</button>
                        <button class="action-btn respond-btn">Respond</button>
                    </td>
                </tr>
            </tbody>
        </table>
    </div>

    <!-- Charts Section -->
    <div class="charts-section">
        <div class="chart-container">
            <h4>Leads Over Time</h4>
            <canvas id="leads-chart"></canvas>
        </div>

        <div class="chart-container">
            <h4>Conversion Funnel</h4>
            <canvas id="funnel-chart"></canvas>
        </div>

        <div class="chart-container">
            <h4>Lead Sources</h4>
            <canvas id="sources-chart"></canvas>
        </div>
    </div>
</div>
```

## 🔄 Automated Follow-up System

### Follow-up Automation
```cpp
enum class FollowUpType {
    WELCOME_EMAIL,              // Auto-send when lead submitted
    RESPONSE_REMINDER,          // Remind business to respond
    FOLLOW_UP_SEQUENCE,         // Automated follow-up emails
    RE_ENGAGEMENT,              // Re-engage cold leads
    SATISFACTION_SURVEY        // Post-conversion feedback
};

struct FollowUpRule {
    std::string ruleId;
    FollowUpType type;
    std::string triggerCondition;    // "lead_submitted", "no_response_24h", etc.
    int delayHours;                  // When to send after trigger
    std::string templateId;          // Email template to use
    bool isActive = true;

    // Targeting
    std::vector<LeadCategory> applicableCategories;
    std::vector<LeadStatus> applicableStatuses;
    int minimumScore = 0;            // Only for high-quality leads
};

class FollowUpAutomationService {
public:
    static void processFollowUpTriggers();
    static void scheduleFollowUp(const std::string& leadId,
                               FollowUpType type,
                               int delayHours);
    static void sendFollowUpEmail(const std::string& leadId,
                                const FollowUpRule& rule);

private:
    static std::vector<FollowUpRule> getApplicableRules(
        const LeadSubmission& lead, FollowUpType type);

    static bool shouldSendFollowUp(const LeadSubmission& lead,
                                 const FollowUpRule& rule);

    static std::string personalizeEmailTemplate(
        const std::string& templateId,
        const LeadSubmission& lead);
};
```

## 📤 Lead Export & CRM Integration

### Export System
```cpp
enum class ExportFormat {
    CSV,
    XLSX,
    JSON,
    XML
};

struct LeadExportRequest {
    std::string profileId;
    ExportFormat format;
    LeadFilter filter;                // Which leads to export
    std::vector<std::string> fields; // Which fields to include
    bool includeActivities = true;   // Include activity history
    bool includeAnalytics = true;    // Include scoring/analytics

    // Security
    std::string requestedBy;
    Date requestedAt;
    std::string purpose;             // Audit trail
};

class LeadExportService {
public:
    static std::string exportLeads(const LeadExportRequest& request);
    static std::vector<std::string> getAvailableFields();
    static ExportPreview previewExport(const LeadExportRequest& request);

private:
    static std::vector<LeadData> filterLeads(const LeadExportRequest& request);
    static std::string generateCSV(const std::vector<LeadData>& leads,
                                 const std::vector<std::string>& fields);
    static std::string generateXLSX(const std::vector<LeadData>& leads,
                                  const std::vector<std::string>& fields);
};
```

### CRM Integration
```cpp
struct CRMIntegration {
    std::string integrationId;
    std::string profileId;
    std::string crmType;             // "salesforce", "hubspot", "pipedrive", etc.
    std::string apiEndpoint;
    std::string apiKey;
    bool isActive = true;

    // Field mapping
    std::map<std::string, std::string> fieldMapping;  // Local field -> CRM field
    std::map<std::string, std::string> statusMapping; // Local status -> CRM status

    // Sync settings
    bool autoSyncNewLeads = true;
    bool autoSyncStatusChanges = true;
    int syncFrequencyMinutes = 15;
    Date lastSyncAt;
};

class CRMIntegrationService {
public:
    static void syncLeadToCRM(const std::string& leadId,
                            const CRMIntegration& integration);

    static void syncStatusChangeToCRM(const std::string& leadId,
                                    LeadStatus newStatus,
                                    const CRMIntegration& integration);

    static std::vector<CRMIntegration> getIntegrations(
        const std::string& profileId);

    static IntegrationHealth checkIntegrationHealth(
        const CRMIntegration& integration);

private:
    static std::string mapLeadToCRMFormat(const LeadSubmission& lead,
                                        const CRMIntegration& integration);

    static bool sendToCRM(const std::string& crmData,
                        const CRMIntegration& integration);
};
```

## 📋 Implementation Plan

### Day 1: Lead Workflow + Analytics
- Implement lead status tracking and workflow management
- Create lead analytics and quality scoring system
- Build lead dashboard with metrics and charts
- Add lead assignment and team collaboration features

### Day 2: Automation + Integration
- Implement automated follow-up and reminder system
- Create lead export functionality
- Add CRM integration capabilities
- Build comprehensive analytics and reporting

## 🧪 Testing Strategy

### Workflow Tests
```cpp
TEST(LeadWorkflowTest, StatusTransitions) {
    // Create new lead
    auto leadId = createTestLead();

    // Update status to contacted
    LeadManagementService::updateLeadStatus(
        leadId, LeadStatus::CONTACTED, "user-123", "Sent initial email");

    // Verify status changed
    auto lead = leadRepository.findById(leadId);
    EXPECT_EQ(lead->status, LeadStatus::CONTACTED);
    EXPECT_EQ(lead->statusHistory.size(), 2);  // NEW -> CONTACTED

    // Verify activity logged
    auto activities = leadActivityRepository.findByLeadId(leadId);
    EXPECT_EQ(activities.size(), 1);
    EXPECT_EQ(activities[0].activityType, "status_change");
}
```

### Analytics Tests
```cpp
TEST(LeadAnalyticsTest, ConversionMetrics) {
    // Create test leads with different statuses
    createTestLeadsWithStatuses("profile-123", {
        {LeadStatus::CLOSED_WON, 5},
        {LeadStatus::CLOSED_LOST, 10},
        {LeadStatus::QUALIFIED, 15},
        {LeadStatus::NEW, 20}
    });

    // Get analytics
    auto analytics = LeadManagementService::getLeadAnalytics(
        "profile-123", last30Days());

    // Verify calculations
    EXPECT_EQ(analytics.totalLeads, 50);
    EXPECT_EQ(analytics.conversionRate, 0.1);  // 5 won / 50 total
    EXPECT_EQ(analytics.closeRate, 5.0/15.0);  // 5 won / 15 qualified
}
```

### Export Tests
```cpp
TEST(LeadExportTest, CSVExport) {
    // Create test leads
    auto leadIds = createTestLeads(10);

    // Export to CSV
    LeadExportRequest request{
        .profileId = "profile-123",
        .format = ExportFormat::CSV,
        .fields = {"name", "email", "category", "status", "submittedAt"}
    };

    auto csvData = LeadExportService::exportLeads(request);

    // Verify CSV format
    EXPECT_TRUE(csvData.find("Name,Email,Category,Status,Submitted At") != std::string::npos);
    EXPECT_TRUE(csvData.find("John Doe") != std::string::npos);

    // Verify row count (header + 10 data rows)
    auto lines = splitByNewline(csvData);
    EXPECT_EQ(lines.size(), 11);
}
```

## 🎉 Success Criteria

### Lead Management
- ✅ **Comprehensive lead status workflow**
- ✅ **Lead assignment and team collaboration**
- ✅ **Lead quality scoring and prioritization**
- ✅ **Automated deadline tracking**

### Analytics & Insights
- ✅ **Detailed lead analytics dashboard**
- ✅ **Conversion rate tracking and optimization**
- ✅ **Lead source attribution and analysis**
- ✅ **Response time monitoring**

### Automation & Integration
- ✅ **Automated follow-up email sequences**
- ✅ **CRM integration with major platforms**
- ✅ **Lead export in multiple formats**
- ✅ **Priority alert system**

### Business Intelligence
- ✅ **Revenue attribution from leads**
- ✅ **Lead scoring and predictive analytics**
- ✅ **A/B testing for lead forms**
- ✅ **Performance benchmarking**

This creates a **professional lead management system** that **transforms contact form submissions into measurable business results** with **comprehensive analytics and automation**.
