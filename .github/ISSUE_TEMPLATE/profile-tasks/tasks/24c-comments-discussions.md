# 🚀 Social Engagement - Comments & Discussions System

**Duration:** 2 days
**Dependencies:** 24b-likes-follows.md (social actions foundation)
**Acceptance Criteria:**
- ✅ Profile comment system with threading
- ✅ Comment moderation and management
- ✅ Comment notifications and engagement
- ✅ Comment analytics and insights
- ✅ Anti-spam and abuse prevention
- ✅ Comment search and filtering
- ✅ Nested discussion threads

## 🎯 Task Description

Implement a comprehensive comments and discussions system that allows users to engage with profiles through threaded conversations, while providing moderation tools and analytics to maintain quality and encourage meaningful interactions.

## 💬 Comments System Architecture

### Comment Structure
```cpp
enum class CommentStatus {
    PENDING,        // Awaiting moderation
    APPROVED,       // Published and visible
    REJECTED,       // Moderation rejected
    HIDDEN,         // Hidden by user/moderator
    DELETED         // Soft deleted
};

struct ProfileComment {
    std::string commentId;
    std::string profileId;           // Profile being commented on
    std::string authorId;            // Comment author
    std::string authorName;
    std::string authorAvatarUrl;

    // Content
    std::string content;
    std::string contentHtml;         // Sanitized HTML version
    Date createdAt;
    Date updatedAt;

    // Threading
    std::string parentCommentId;     // For replies (null for top-level)
    std::vector<std::string> childCommentIds; // Nested replies
    int threadDepth;                 // 0 = top level, 1+ = replies
    int replyCount;                  // Number of direct replies

    // Engagement
    int likeCount = 0;
    std::vector<std::string> likedBy; // User IDs who liked
    bool isPinned = false;           // Pinned to top
    bool isFeatured = false;         // Featured comment

    // Moderation
    CommentStatus status = CommentStatus::PENDING;
    std::string moderatedBy;         // Moderator user ID
    Date moderatedAt;
    std::string moderationReason;
    int reportCount = 0;             // Number of abuse reports

    // Metadata
    std::string ipAddress;           // For abuse prevention
    std::string userAgent;
    std::string sessionId;
    bool isEdited = false;
};
```

### Comment Threading System
```cpp
class CommentThreadingService {
public:
    static CommentResult addComment(const CommentSubmission& submission);
    static CommentResult addReply(const std::string& parentCommentId,
                                const CommentSubmission& submission);

    static std::vector<ProfileComment> getCommentThread(
        const std::string& profileId,
        const std::string& rootCommentId,
        int maxDepth = 3);

    static std::vector<ProfileComment> getProfileComments(
        const std::string& profileId,
        const CommentQuery& query = {});

    static bool updateComment(const std::string& commentId,
                            const std::string& newContent,
                            const std::string& userId);

    static bool deleteComment(const std::string& commentId,
                            const std::string& userId);

private:
    static void updateThreadMetadata(const std::string& rootCommentId);
    static void notifyCommentParticipants(const ProfileComment& comment);
    static bool canUserComment(const std::string& profileId, const std::string& userId);
};
```

## 🛡️ Comment Moderation System

### Moderation Workflow
```cpp
enum class ModerationAction {
    APPROVE,
    REJECT,
    HIDE,
    DELETE,
    PIN,
    UNPIN,
    FEATURE,
    UNFEATURE
};

struct CommentModerationRequest {
    std::string requestId;
    std::string commentId;
    ModerationAction action;
    std::string moderatorId;
    std::string reason;
    Date actionAt;

    // Additional context
    std::string commentContent;      // For logging
    std::string authorId;           // Comment author
    std::vector<std::string> reports; // Abuse reports that triggered this
};

class CommentModerationService {
public:
    static ModerationResult moderateComment(const CommentModerationRequest& request);

    static std::vector<ProfileComment> getCommentsNeedingModeration(
        const std::string& profileId, int limit = 50);

    static ModerationStats getModerationStats(const std::string& profileId);

    static bool canUserModerate(const std::string& profileId, const std::string& userId);

private:
    static void applyModerationAction(const CommentModerationRequest& request);
    static void notifyCommentAuthor(const CommentModerationRequest& request);
    static void updateCommentStatus(const std::string& commentId, CommentStatus newStatus);
};
```

### Automated Moderation
```cpp
class AutomatedModerationService {
public:
    static ModerationResult checkCommentForSpam(const ProfileComment& comment);

    static bool containsSpamPatterns(const std::string& content);
    static bool hasExcessiveLinks(const std::string& content);
    static bool isRepetitiveContent(const std::string& content,
                                  const std::string& authorId);
    static bool violatesRateLimits(const std::string& authorId,
                                 const std::string& profileId);

private:
    static double calculateSpamScore(const ProfileComment& comment);
    static bool checkAgainstBlacklist(const std::string& content);
    static int getRecentCommentCount(const std::string& authorId, int minutes = 60);
};
```

## 📊 Comment Analytics & Insights

### Comment Engagement Metrics
```cpp
struct CommentAnalytics {
    std::string profileId;
    DateRange period;

    // Volume metrics
    int totalComments;
    int topLevelComments;            // Non-reply comments
    int replyComments;               // Replies to comments
    int uniqueCommenters;
    int moderatedComments;           // Required moderation

    // Engagement metrics
    double commentsPerView;          // Comments per profile view
    double replyRate;                // % of comments that get replies
    double engagementRate;           // Comments + replies per view

    // Quality metrics
    double averageCommentLength;
    int featuredComments;            // Pinned/featured comments
    int reportedComments;            // Abuse reports
    double approvalRate;             // % of comments approved

    // Timing analytics
    std::map<std::string, int> commentsByHour;    // Peak commenting hours
    std::map<std::string, int> commentsByDay;     // Peak days
    double averageResponseTime;      // Time to first reply

    // Top contributors
    std::vector<TopCommenter> topCommenters;
    std::vector<PopularComment> popularComments;
};

struct TopCommenter {
    std::string userId;
    std::string userName;
    int commentCount;
    int likeCount;                   // Likes on their comments
    double engagement;               // Their comments' engagement
};

struct PopularComment {
    std::string commentId;
    std::string content;
    int likeCount;
    int replyCount;
    Date createdAt;
};
```

### Comment Dashboard
```cpp
class CommentAnalyticsService {
public:
    static CommentAnalytics getCommentAnalytics(
        const std::string& profileId, DateRange range);

    static std::vector<PopularComment> getPopularComments(
        const std::string& profileId, int limit = 10);

    static std::vector<TopCommenter> getTopCommenters(
        const std::string& profileId, int limit = 10);

    static CommentEngagementReport generateEngagementReport(
        const std::string& profileId, DateRange range);

private:
    static void calculateEngagementMetrics(CommentAnalytics& analytics);
    static void analyzeCommentQuality(CommentAnalytics& analytics);
    static void identifyPopularContent(CommentAnalytics& analytics);
};
```

## 🔔 Comment Notifications

### Notification Types
```cpp
enum class CommentNotificationType {
    COMMENT_RECEIVED,            // New comment on profile
    REPLY_RECEIVED,              // Reply to your comment
    COMMENT_LIKED,               // Someone liked your comment
    COMMENT_MENTIONED,           // Mentioned in comment (@username)
    COMMENT_MODERATED,           // Comment moderation action
    COMMENT_FEATURED             // Comment was featured/pinned
};

struct CommentNotification {
    std::string notificationId;
    std::string profileId;
    CommentNotificationType type;
    std::string commentId;
    std::string actorId;              // Who performed the action
    std::string actorName;
    std::string message;
    Date createdAt;
    bool isRead = false;

    // Context
    std::string commentPreview;       // First 100 chars of comment
    std::string profileName;          // Profile being commented on
    bool isReply;                     // Is this a reply notification
};
```

### Smart Notification System
```cpp
class CommentNotificationService {
public:
    static void notifyCommentReceived(const ProfileComment& comment);
    static void notifyReplyReceived(const ProfileComment& reply);
    static void notifyCommentLiked(const std::string& commentId,
                                 const std::string& likerId);
    static void notifyCommentMentioned(const ProfileComment& comment,
                                     const std::vector<std::string>& mentionedUsers);

private:
    static bool shouldNotifyUser(const std::string& userId,
                               CommentNotificationType type,
                               const std::string& profileId);
    static std::string generateNotificationMessage(CommentNotificationType type,
                                                 const std::string& actorName,
                                                 const std::string& commentPreview);
    static void batchSimilarNotifications(const std::string& userId);
};
```

## 🔍 Comment Search & Filtering

### Search and Filter System
```cpp
struct CommentQuery {
    std::string searchText;           // Search within comments
    CommentStatus status = CommentStatus::APPROVED;
    DateRange dateRange;
    std::string authorId;             // Filter by specific user
    bool includeReplies = true;       // Include reply comments
    int maxDepth = 3;                 // Threading depth
    SortOrder sortBy = SortOrder::NEWEST;

    // Moderation filters
    bool showReported = false;        // Show reported comments
    bool showPending = false;         // Show pending moderation
    int minLikes = 0;                 // Minimum likes filter
};

enum class SortOrder {
    NEWEST,
    OLDEST,
    MOST_LIKED,
    MOST_REPLIES,
    FEATURED_FIRST
};

class CommentSearchService {
public:
    static std::vector<ProfileComment> searchComments(
        const std::string& profileId, const CommentQuery& query);

    static std::vector<ProfileComment> getCommentsByAuthor(
        const std::string& profileId, const std::string& authorId);

    static std::vector<ProfileComment> getReportedComments(
        const std::string& profileId, int limit = 50);

private:
    static void applyFilters(std::vector<ProfileComment>& comments,
                           const CommentQuery& query);
    static void applySorting(std::vector<ProfileComment>& comments,
                           SortOrder sortBy);
    static bool matchesSearchText(const ProfileComment& comment,
                                const std::string& searchText);
};
```

## 🛡️ Anti-Abuse & Spam Prevention

### Abuse Prevention Measures
```cpp
class CommentAbusePreventionService {
public:
    static AbuseCheckResult checkForAbuse(const CommentSubmission& submission);

    // Rate limiting
    static bool isRateLimited(const std::string& userId,
                            const std::string& profileId);

    // Content filtering
    static bool containsForbiddenContent(const std::string& content);
    static bool hasTooManyLinks(const std::string& content);
    static bool isDuplicateContent(const std::string& content,
                                 const std::string& authorId);

    // User behavior analysis
    static bool isTrollBehavior(const std::string& userId,
                              const std::string& profileId);

private:
    static double calculateAbuseScore(const CommentSubmission& submission);
    static std::vector<std::string> extractForbiddenWords(const std::string& content);
    static int countUrls(const std::string& content);
};

struct AbuseCheckResult {
    bool isAllowed;
    double abuseScore;                // 0.0 - 1.0
    std::vector<std::string> flags;   // Issues found
    std::string recommendedAction;    // "ALLOW", "FLAG", "BLOCK"
};
```

## 💬 Comment UI Components

### Threaded Comments Display
```html
<div class="comments-section">
    <!-- Comment form -->
    <div class="comment-form">
        <div class="comment-input-container">
            <img src="/avatars/user123.jpg" class="commenter-avatar">
            <textarea class="comment-input" placeholder="Share your thoughts..."></textarea>
        </div>
        <div class="comment-actions">
            <button class="comment-submit-btn">Post Comment</button>
        </div>
    </div>

    <!-- Comments list -->
    <div class="comments-list">
        <!-- Top-level comment -->
        <div class="comment-thread" data-comment-id="comment-1">
            <div class="comment-item">
                <div class="comment-header">
                    <img src="/avatars/user456.jpg" class="commenter-avatar">
                    <div class="commenter-info">
                        <span class="commenter-name">Sarah Johnson</span>
                        <span class="comment-time">2 hours ago</span>
                    </div>
                    <div class="comment-actions">
                        <button class="like-btn">👍 5</button>
                        <button class="reply-btn">Reply</button>
                        <button class="report-btn">Report</button>
                    </div>
                </div>

                <div class="comment-content">
                    <p>This is such an impressive profile! The project showcase is really well done.</p>
                </div>

                <!-- Replies -->
                <div class="comment-replies">
                    <div class="reply-item">
                        <img src="/avatars/user789.jpg" class="commenter-avatar">
                        <div class="reply-content">
                            <span class="reply-author">Mike Chen</span>
                            <span class="reply-text">Thanks Sarah! I put a lot of work into it.</span>
                            <div class="reply-actions">
                                <button class="like-btn">👍 2</button>
                                <span class="reply-time">1 hour ago</span>
                            </div>
                        </div>
                    </div>
                </div>

                <!-- Reply form (shown when clicking reply) -->
                <div class="reply-form hidden">
                    <textarea class="reply-input" placeholder="Write a reply..."></textarea>
                    <button class="reply-submit-btn">Reply</button>
                </div>
            </div>
        </div>
    </div>
</div>
```

## 📋 Implementation Plan

### Day 1: Core Comments + Threading
- Implement comment creation and threading system
- Add basic moderation (approve/reject)
- Create comment display with nested replies
- Test comment CRUD operations

### Day 2: Moderation + Analytics + Anti-Abuse
- Build comprehensive moderation dashboard
- Implement automated spam detection
- Add comment analytics and insights
- Create notification system and abuse prevention

## 🧪 Testing Strategy

### Comment Threading Tests
```cpp
TEST(CommentThreadingTest, CreateCommentAndReply) {
    // Create top-level comment
    auto comment = CommentThreadingService::addComment({
        .profileId = "profile-123",
        .authorId = "user-456",
        .content = "Great profile!"
    });

    EXPECT_FALSE(comment.commentId.empty());
    EXPECT_EQ(comment.threadDepth, 0);

    // Add reply
    auto reply = CommentThreadingService::addReply(comment.commentId, {
        .profileId = "profile-123",
        .authorId = "user-789",
        .content = "Thanks!"
    });

    EXPECT_EQ(reply.parentCommentId, comment.commentId);
    EXPECT_EQ(reply.threadDepth, 1);

    // Verify thread structure
    auto thread = CommentThreadingService::getCommentThread("profile-123", comment.commentId);
    EXPECT_EQ(thread.size(), 2);
    EXPECT_EQ(thread[0].commentId, comment.commentId);
    EXPECT_EQ(thread[1].commentId, reply.commentId);
}
```

### Moderation Tests
```cpp
TEST(CommentModerationTest, ApproveAndRejectComments) {
    // Create pending comment
    auto comment = createPendingComment();

    // Approve comment
    auto approveResult = CommentModerationService::moderateComment({
        .commentId = comment.commentId,
        .action = ModerationAction::APPROVE,
        .moderatorId = "moderator-123",
        .reason = "Good comment"
    });

    EXPECT_TRUE(approveResult.success);

    // Verify status updated
    auto updatedComment = commentRepository.findById(comment.commentId);
    EXPECT_EQ(updatedComment->status, CommentStatus::APPROVED);
}

TEST(CommentModerationTest, SpamDetection) {
    // Test spam comment
    CommentSubmission spamSubmission{
        .content = "Buy cheap watches!!! http://spam-site.com"
    };

    auto spamCheck = AutomatedModerationService::checkCommentForSpam(spamSubmission);
    EXPECT_TRUE(spamCheck.containsSpamPatterns);
    EXPECT_GT(spamCheck.spamScore, 0.8);
}
```

### Analytics Tests
```cpp
TEST(CommentAnalyticsTest, EngagementMetrics) {
    // Create test comments and engagement
    createTestCommentActivity("profile-123", 50);

    // Get analytics
    auto analytics = CommentAnalyticsService::getCommentAnalytics("profile-123", last30Days());

    // Verify calculations
    EXPECT_GT(analytics.totalComments, 0);
    EXPECT_GE(analytics.averageCommentLength, 0);
    EXPECT_TRUE(analytics.replyRate >= 0.0 && analytics.replyRate <= 1.0);
    EXPECT_FALSE(analytics.topCommenters.empty());
}
```

## 🎉 Success Criteria

### Comments & Threading
- ✅ **Threaded comment system implemented**
- ✅ **Nested replies with proper depth limits**
- ✅ **Comment editing and deletion**
- ✅ **Rich text formatting support**

### Moderation & Safety
- ✅ **Comprehensive moderation workflow**
- ✅ **Automated spam and abuse detection**
- ✅ **Comment reporting system**
- ✅ **Rate limiting and abuse prevention**

### Analytics & Engagement
- ✅ **Comment analytics dashboard**
- ✅ **Engagement metrics and insights**
- ✅ **Popular comments identification**
- ✅ **Comment quality assessment**

### User Experience
- ✅ **Intuitive commenting interface**
- ✅ **Real-time comment updates**
- ✅ **Smart notifications system**
- ✅ **Mobile-responsive design**

This creates a **robust discussion platform** that **fosters meaningful engagement** while **maintaining quality and safety** through comprehensive moderation and analytics.
