# 🚀 Community Groups - Basic Group Creation & Membership

**Duration:** 2 days
**Dependencies:** Profile database models, Profile routing CRUD
**Acceptance Criteria:**
- ✅ Group creation and configuration
- ✅ Group membership management (join/leave)
- ✅ Group privacy settings (public/private)
- ✅ Basic group content feed
- ✅ Group member directory
- ✅ Group invitation system
- ✅ Group activity notifications

## 🎯 Task Description

Implement the fundamental group functionality that allows users to create and join community groups, establishing the foundation for collaborative networking and content sharing around shared interests and professional goals.

## 👥 Group Creation System

### Group Data Model
```cpp
enum class GroupType {
    PROFESSIONAL_NETWORK,      // Professional networking
    INDUSTRY_SPECIFIC,         // Healthcare, Technology, etc.
    SKILL_BASED,              // Programming, Marketing, etc.
    LOCATION_BASED,           // City/region specific
    COMPANY_ALUMNI,           // Former employees
    PROJECT_COLLABORATION,    // Working on projects together
    INTEREST_BASED,           // Hobbies, causes, etc.
    SUPPORT_COMMUNITY         // Mutual support and advice
};

enum class GroupPrivacy {
    PUBLIC,                   // Anyone can find and join
    PRIVATE,                  // Invite-only, hidden from search
    SECRET                    // Completely hidden, invite-only
};

enum class GroupMembershipType {
    OPEN,                     // Anyone can join instantly
    REQUEST,                  // Request to join, approval needed
    INVITE_ONLY              // Only invited members
};

struct Group {
    std::string groupId;
    std::string name;
    std::string slug;                 // URL-friendly identifier
    std::string description;
    
    // Categorization
    GroupType type;
    std::vector<std::string> tags;
    std::vector<std::string> categories;
    
    // Privacy & Access
    GroupPrivacy privacy;
    GroupMembershipType membershipType;
    std::string password;             // Optional password for private groups
    
    // Ownership & Management
    std::string ownerId;
    std::vector<std::string> adminIds;
    std::vector<std::string> moderatorIds;
    
    // Visual Identity
    std::string avatarUrl;
    std::string coverImageUrl;
    std::string primaryColor;
    std::string bannerText;
    
    // Membership
    int memberCount = 0;
    int maxMembers = 0;              // 0 = unlimited
    Date createdAt;
    Date updatedAt;
    
    // Activity
    Date lastActivityAt;
    int postCount = 0;
    int discussionCount = 0;
    
    // Settings
    GroupSettings settings;
    bool isActive = true;
};

struct GroupSettings {
    // Content settings
    bool allowPosts = true;
    bool allowDiscussions = true;
    bool allowEvents = true;
    bool allowFiles = true;
    
    // Moderation settings
    bool requireApproval = false;     // Posts need approval
    bool allowAnonymousPosts = false;
    std::vector<std::string> blockedWords;
    
    // Notification settings
    bool notifyOnJoin = true;
    bool notifyOnPost = true;
    bool emailNotifications = true;
    
    // Member permissions
    bool membersCanInvite = false;
    bool membersCanCreateEvents = false;
    int maxPostsPerDay = 10;
};

class GroupService {
public:
    static Group createGroup(const GroupCreationRequest& request);
    static void updateGroup(const std::string& groupId,
                          const GroupUpdateRequest& update);
    static void deleteGroup(const std::string& groupId,
                          const std::string& userId);
    
    static std::vector<Group> getUserGroups(const std::string& userId,
                                          const GroupFilter& filter = {});
    
    static std::vector<Group> searchGroups(const GroupSearchQuery& query);
    
    static GroupAnalytics getGroupAnalytics(const std::string& groupId);
    
private:
    static std::string generateGroupSlug(const std::string& name);
    static bool validateGroupSettings(const GroupSettings& settings);
    static void notifyGroupCreation(const Group& group);
};
```

## 👤 Group Membership Management

### Membership Data Model
```cpp
enum class MembershipStatus {
    PENDING,                  // Requested to join, awaiting approval
    ACTIVE,                   // Full member
    INACTIVE,                 // Temporarily inactive
    SUSPENDED,                // Temporarily suspended
    BANNED,                   // Permanently banned
    LEFT                     // Voluntarily left
};

enum class MembershipRole {
    MEMBER,                   // Regular member
    MODERATOR,                // Can moderate content
    ADMIN,                    // Can manage group settings
    OWNER                     // Group creator, full control
};

struct GroupMembership {
    std::string membershipId;
    std::string groupId;
    std::string userId;
    MembershipStatus status;
    MembershipRole role = MembershipRole::MEMBER;
    
    // Membership details
    Date joinedAt;
    Date lastActiveAt;
    std::string invitedBy;            // Who invited them (if applicable)
    std::string joinReason;           // Why they want to join
    
    // Activity tracking
    int postsCount = 0;
    int commentsCount = 0;
    int eventsAttended = 0;
    Date lastPostAt;
    
    // Permissions & settings
    MembershipPermissions permissions;
    NotificationSettings notificationSettings;
    
    // Status history
    std::vector<MembershipStatusChange> statusHistory;
};

struct MembershipPermissions {
    bool canPost = true;
    bool canComment = true;
    bool canInvite = false;
    bool canModerate = false;
    bool canManageSettings = false;
    bool canDeleteContent = false;
    bool canViewMemberList = true;
};

struct MembershipStatusChange {
    MembershipStatus oldStatus;
    MembershipStatus newStatus;
    Date changedAt;
    std::string changedBy;            // Who made the change
    std::string reason;
};

class GroupMembershipService {
public:
    static GroupMembership joinGroup(const std::string& groupId,
                                   const std::string& userId,
                                   const std::string& joinReason = "");
    
    static void leaveGroup(const std::string& groupId,
                         const std::string& userId,
                         const std::string& reason = "");
    
    static void approveMembership(const std::string& membershipId,
                                const std::string& approvedBy);
    
    static void changeMembershipRole(const std::string& membershipId,
                                   MembershipRole newRole,
                                   const std::string& changedBy);
    
    static std::vector<GroupMembership> getGroupMembers(
        const std::string& groupId,
        const MemberFilter& filter = {});
    
    static GroupMembership getUserMembership(const std::string& groupId,
                                           const std::string& userId);
    
private:
    static bool canUserJoinGroup(const std::string& groupId,
                               const std::string& userId);
    
    static void updateMemberCount(const std::string& groupId);
    static void notifyMembershipChange(const GroupMembership& membership,
                                     MembershipStatusChange change);
};
```

## 🔍 Group Discovery & Search

### Group Search System
```cpp
struct GroupSearchQuery {
    std::string query;                // Search terms
    std::vector<GroupType> types;
    std::vector<std::string> categories;
    std::vector<std::string> tags;
    
    // Location filters
    std::string location;
    double latitude = 0.0;
    double longitude = 0.0;
    int radiusKm = 50;               // Search radius
    
    // Membership filters
    int minMembers = 0;
    int maxMembers = 0;
    bool hasRecentActivity = false;   // Active in last 30 days
    
    // Privacy filters
    bool includePrivate = false;      // Include private groups user can join
    bool onlyPublic = true;          // Only show public groups
    
    // Sorting
    GroupSearchSort sortBy = GroupSearchSort::RELEVANCE;
    SortDirection sortDirection = SortDirection::DESCENDING;
    
    // Pagination
    int page = 1;
    int pageSize = 20;
};

struct GroupSearchResult {
    std::vector<GroupSearchHit> hits;
    int totalResults;
    int totalPages;
    
    // Search insights
    std::vector<std::string> suggestedQueries;
    std::vector<std::string> popularCategories;
    std::vector<std::string> trendingTags;
    
    // Personalization
    std::vector<Group> recommendedGroups; // Based on user interests
};

struct GroupSearchHit {
    Group group;
    double relevanceScore;           // 0.0 - 1.0
    std::vector<std::string> matchedTerms;
    bool isUserMember;               // Is current user a member?
    bool canUserJoin;               // Can current user join?
    std::string joinStatus;          // "can_join", "pending", "member"
};

class GroupSearchService {
public:
    static GroupSearchResult searchGroups(const GroupSearchQuery& query);
    
    static std::vector<Group> getRecommendedGroups(
        const std::string& userId, int limit = 10);
    
    static std::vector<Group> getTrendingGroups(int limit = 10);
    
private:
    static void buildSearchIndex(const Group& group);
    static std::vector<GroupSearchHit> rankSearchResults(
        const std::vector<Group>& groups,
        const GroupSearchQuery& query);
    static double calculateGroupRelevance(const Group& group,
                                        const GroupSearchQuery& query,
                                        const std::string& userId);
};
```

## 📨 Group Invitation System

### Invitation Data Model
```cpp
enum class InvitationStatus {
    PENDING,                  // Sent but not responded
    ACCEPTED,                 // User accepted and joined
    DECLINED,                 // User declined
    EXPIRED                   // Invitation expired
};

enum class InvitationType {
    DIRECT_INVITE,            // Personal invitation
    BULK_INVITE,              // Mass invitation to multiple users
    EMAIL_INVITE,             // Email invitation (for non-users)
    LINK_INVITE               // Shareable invitation link
};

struct GroupInvitation {
    std::string invitationId;
    std::string groupId;
    std::string invitedBy;            // Who sent the invitation
    
    // Recipient
    std::string inviteeId;            // User ID (null for email invites)
    std::string inviteeEmail;         // Email address
    std::string inviteeName;          // Display name
    
    // Invitation details
    InvitationType type;
    std::string personalMessage;      // Custom message from inviter
    std::string invitationToken;      // For link-based invites
    
    // Status & tracking
    InvitationStatus status = InvitationStatus::PENDING;
    Date sentAt;
    Date respondedAt;
    Date expiresAt;
    
    // Conversion tracking
    bool viewedInvitation = false;
    bool clickedJoin = false;
    bool completedOnboarding = false;
    
    // Analytics
    std::string invitationSource;     // How invitation was sent
    std::string userAgent;           // Device/browser info
    std::string ipAddress;
};

class GroupInvitationService {
public:
    static GroupInvitation sendInvitation(const InvitationRequest& request);
    
    static void acceptInvitation(const std::string& invitationToken);
    
    static void declineInvitation(const std::string& invitationId,
                                const std::string& reason = "");
    
    static std::vector<GroupInvitation> getSentInvitations(
        const std::string& userId, const InvitationFilter& filter = {});
    
    static std::vector<GroupInvitation> getReceivedInvitations(
        const std::string& userId);
    
    static InvitationAnalytics getInvitationAnalytics(
        const std::string& groupId);
    
private:
    static std::string generateInvitationToken();
    static bool validateInvitationToken(const std::string& token);
    static void sendInvitationEmail(const GroupInvitation& invitation);
    static void trackInvitationConversion(const GroupInvitation& invitation);
};
```

## 🔔 Group Activity Notifications

### Notification System
```cpp
enum class GroupNotificationType {
    GROUP_INVITATION_RECEIVED,
    GROUP_JOIN_REQUEST,
    GROUP_JOIN_APPROVED,
    GROUP_JOIN_DECLINED,
    NEW_GROUP_POST,
    NEW_GROUP_MEMBER,
    GROUP_EVENT_CREATED,
    GROUP_MENTION,
    GROUP_ROLE_CHANGED,
    GROUP_SETTINGS_CHANGED
};

struct GroupNotification {
    std::string notificationId;
    std::string userId;                    // Notification recipient
    GroupNotificationType type;
    std::string groupId;
    
    // Context data
    std::string actorId;                  // Who performed the action
    std::string actorName;
    std::string groupName;
    std::string actionDescription;        // Human readable description
    
    // Related objects
    std::string postId;                   // If related to a post
    std::string memberId;                 // If related to a member
    std::string eventId;                  // If related to an event
    
    // Timing
    Date createdAt;
    bool isRead = false;
    bool isActionRequired = false;        // Requires user action (approve request, etc.)
};

class GroupNotificationService {
public:
    static void notifyGroupEvent(GroupNotificationType type,
                               const std::string& groupId,
                               const std::string& actorId,
                               const NotificationContext& context = {});
    
    static void notifyGroupMembers(GroupNotificationType type,
                                 const std::string& groupId,
                                 const std::string& actorId,
                                 const NotificationContext& context = {},
                                 const std::vector<std::string>& excludeUsers = {});
    
    static std::vector<GroupNotification> getUserNotifications(
        const std::string& userId,
        const NotificationFilter& filter = {});
    
    static void markNotificationsRead(const std::vector<std::string>& notificationIds,
                                    const std::string& userId);
    
private:
    static std::string generateNotificationMessage(
        GroupNotificationType type,
        const std::string& actorName,
        const std::string& groupName,
        const NotificationContext& context);
    
    static bool shouldNotifyUser(const std::string& userId,
                               GroupNotificationType type,
                               const std::string& groupId);
};
```

## 📊 Group Analytics

### Basic Analytics
```cpp
struct GroupAnalytics {
    std::string groupId;
    DateRange period;
    
    // Membership metrics
    int totalMembers;
    int activeMembers;              // Active in last 30 days
    int newMembers;                 // Joined in period
    double memberRetentionRate;     // % still active after 90 days
    
    // Activity metrics
    int totalPosts;
    int totalComments;
    int totalDiscussions;
    int totalEvents;
    double activityRate;            // Activities per member per week
    
    // Engagement metrics
    double averageEngagement;        // Average likes/comments per post
    int topContributors;            // Members with most contributions
    std::vector<TopContributor> topContributorsList;
    
    // Growth metrics
    double growthRate;              // Member growth rate
    std::vector<MonthlyGrowth> monthlyGrowth;
    
    // Content breakdown
    std::map<std::string, int> contentByType; // Posts, discussions, etc.
    std::map<std::string, int> activityByDayOfWeek;
    std::map<std::string, int> activityByHour;
};

struct TopContributor {
    std::string userId;
    std::string userName;
    int contributionCount;
    std::string contributionType;    // "posts", "comments", "events"
};

struct MonthlyGrowth {
    Date month;
    int newMembers;
    int totalMembers;
    double growthPercentage;
};

class GroupAnalyticsService {
public:
    static GroupAnalytics getGroupAnalytics(
        const std::string& groupId,
        DateRange range = last30Days());
    
    static std::vector<GroupInsight> getGroupInsights(
        const std::string& groupId);
    
    static GroupHealthReport getGroupHealthReport(
        const std::string& groupId);
    
private:
    static void calculateEngagementMetrics(GroupAnalytics& analytics);
    static void analyzeGrowthTrends(GroupAnalytics& analytics);
    static std::vector<GroupInsight> generateInsights(
        const GroupAnalytics& analytics);
};
```

## 📋 Implementation Plan

### Day 1: Group Creation + Membership
- Implement group creation with configuration options
- Create group membership management (join/leave/approve)
- Add group privacy settings and access control
- Build basic group member directory

### Day 1 Continued: Discovery + Invitations
- Implement group search and discovery
- Create group invitation system
- Add group activity notifications
- Build basic group analytics

## 🧪 Testing Strategy

### Group Creation Tests
```cpp
TEST(GroupCreationTest, CreateAndConfigureGroup) {
    GroupCreationRequest request{
        .name = "Tech Professionals Network",
        .description = "Connecting technology professionals worldwide",
        .type = GroupType::PROFESSIONAL_NETWORK,
        .privacy = GroupPrivacy::PUBLIC,
        .membershipType = GroupMembershipType::OPEN,
        .ownerId = "user-123"
    };
    
    auto group = GroupService::createGroup(request);
    
    EXPECT_FALSE(group.groupId.empty());
    EXPECT_EQ(group.name, "Tech Professionals Network");
    EXPECT_EQ(group.ownerId, "user-123");
    EXPECT_TRUE(group.isActive);
}
```

### Membership Tests
```cpp
TEST(GroupMembershipTest, JoinAndLeaveGroup) {
    // Create group
    auto groupId = createTestGroup();
    
    // User joins
    auto membership = GroupMembershipService::joinGroup(groupId, "user-456");
    EXPECT_EQ(membership.status, MembershipStatus::ACTIVE);
    EXPECT_EQ(membership.role, MembershipRole::MEMBER);
    
    // Verify membership
    auto userMembership = GroupMembershipService::getUserMembership(groupId, "user-456");
    EXPECT_EQ(userMembership.status, MembershipStatus::ACTIVE);
    
    // User leaves
    GroupMembershipService::leaveGroup(groupId, "user-456");
    userMembership = GroupMembershipService::getUserMembership(groupId, "user-456");
    EXPECT_EQ(userMembership.status, MembershipStatus::LEFT);
}
```

### Invitation Tests
```cpp
TEST(GroupInvitationTest, SendAndAcceptInvitation) {
    // Create group
    auto groupId = createTestGroup();
    
    // Send invitation
    InvitationRequest request{
        .groupId = groupId,
        .inviteeId = "user-789",
        .invitedBy = "user-123",
        .personalMessage = "You would be a great addition to our group!"
    };
    
    auto invitation = GroupInvitationService::sendInvitation(request);
    EXPECT_EQ(invitation.status, InvitationStatus::PENDING);
    
    // Accept invitation
    GroupInvitationService::acceptInvitation(invitation.invitationToken);
    
    // Verify user joined
    auto membership = GroupMembershipService::getUserMembership(groupId, "user-789");
    EXPECT_EQ(membership.status, MembershipStatus::ACTIVE);
    EXPECT_EQ(invitation.status, InvitationStatus::ACCEPTED);
}
```

## 🎉 Success Criteria

### Group Management
- ✅ **Group creation with customizable settings**
- ✅ **Flexible privacy and membership controls**
- ✅ **Group ownership and administration**
- ✅ **Group deletion and archiving**

### Membership System
- ✅ **Multiple membership types (open/request/invite)**
- ✅ **Membership approval workflow**
- ✅ **Role-based permissions (member/moderator/admin)**
- ✅ **Membership status tracking**

### Discovery & Access
- ✅ **Group search with filters and sorting**
- ✅ **Group recommendations based on interests**
- ✅ **Invitation system with tracking**
- ✅ **Public and private group handling**

### Communication
- ✅ **Group activity notifications**
- ✅ **Membership change notifications**
- ✅ **Invitation response notifications**
- ✅ **Customizable notification preferences**

This establishes the **core foundation for community building** with **flexible group structures** that support **diverse community needs** while maintaining **proper access controls** and **engagement features**.
