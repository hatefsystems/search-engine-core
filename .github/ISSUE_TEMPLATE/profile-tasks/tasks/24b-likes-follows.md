# 🚀 Social Engagement - Likes & Follows System

**Duration:** 1 day
**Dependencies:** 24a-profile-views-tracking.md (view tracking foundation)
**Acceptance Criteria:**
- ✅ Profile likes system with reactions
- ✅ Follow/unfollow functionality
- ✅ Social action tracking and analytics
- ✅ Like/follow notifications
- ✅ Engagement feed for profile owners
- ✅ Social proof metrics display
- ✅ Privacy controls for social actions

## 🎯 Task Description

Implement a comprehensive likes and follows system that allows users to show appreciation for profiles, follow updates from interesting people/businesses, and build social connections while providing valuable engagement analytics.

## 👍 Likes System

### Like Structure and Types
```cpp
enum class LikeType {
    LIKE,           // Standard like
    LOVE,           // Heart/love reaction
    SUPPORT,        // Support/thumbs up
    INTERESTED,     // Interested/sparkles
    INSIGHTFUL      // Insightful/lightbulb
};

struct ProfileLike {
    std::string likeId;
    std::string profileId;           // Profile being liked
    std::string likerId;             // User doing the liking
    LikeType type;
    Date likedAt;
    bool isActive = true;           // Can be unliked

    // Context
    std::string sessionId;          // For analytics
    std::string referrer;           // How they found the profile
    std::string userAgent;          // Device info

    // Privacy & notifications
    bool triggersNotification;      // Should notify profile owner
    bool isVisibleToOwner;          // Respects privacy settings
};

struct LikeStats {
    int totalLikes;
    std::map<LikeType, int> likeTypeBreakdown;
    int uniqueLikers;
    Date lastLikedAt;
    double engagementRate;          // Likes per view
};
```

### Likes Service
```cpp
class LikesService {
public:
    static LikeResult addLike(const std::string& profileId,
                            const std::string& likerId,
                            LikeType type = LikeType::LIKE);

    static bool removeLike(const std::string& profileId,
                         const std::string& likerId);

    static std::vector<ProfileLike> getProfileLikes(
        const std::string& profileId,
        const LikeQuery& query = {});

    static LikeStats getLikeStats(const std::string& profileId);

    static bool hasUserLiked(const std::string& profileId,
                           const std::string& userId);

private:
    static void notifyProfileOwner(const ProfileLike& like);
    static void updateLikeAnalytics(const ProfileLike& like);
    static bool canUserLike(const std::string& profileId, const std::string& likerId);
};
```

### Like Reactions UI
```html
<div class="profile-like-section">
    <div class="like-button-container">
        <button class="like-button" data-profile-id="profile-123" onclick="toggleLike('profile-123')">
            <span class="like-icon">👍</span>
            <span class="like-text">Like</span>
            <span class="like-count">42</span>
        </button>

        <!-- Reaction picker (shows on hover/long press) -->
        <div class="reaction-picker hidden">
            <button class="reaction-btn" data-type="LIKE" title="Like">👍</button>
            <button class="reaction-btn" data-type="LOVE" title="Love">❤️</button>
            <button class="reaction-btn" data-type="SUPPORT" title="Support">👏</button>
            <button class="reaction-btn" data-type="INTERESTED" title="Interested">✨</button>
            <button class="reaction-btn" data-type="INSIGHTFUL" title="Insightful">💡</button>
        </div>
    </div>

    <!-- Like breakdown -->
    <div class="like-breakdown">
        <span class="like-summary">42 people liked this</span>
        <div class="reaction-summary">
            <span class="reaction-item">❤️ 12</span>
            <span class="reaction-item">👍 8</span>
            <span class="reaction-item">💡 6</span>
        </div>
    </div>
</div>
```

## 👥 Follows System

### Follow Structure
```cpp
enum class FollowType {
    PROFILE_FOLLOW,         // Follow profile updates
    COMPANY_FOLLOW,         // Follow company updates
    INDUSTRY_FOLLOW         // Follow industry updates
};

struct ProfileFollow {
    std::string followId;
    std::string profileId;           // Profile being followed
    std::string followerId;          // User doing the following
    FollowType type;
    Date followedAt;
    bool isActive = true;           // Can unfollow

    // Notification preferences
    NotificationSettings notifications;

    // Context
    std::string followReason;        // Why they followed (optional)
    std::string sessionId;

    // Privacy
    bool isVisibleToOwner;          // Show in followers list
    bool triggersNotification;
};

struct NotificationSettings {
    bool notifyOnPosts = true;       // New content/updates
    bool notifyOnMilestones = true;  // Achievements, job changes
    bool notifyOnEvents = false;     // Events, webinars
    bool emailNotifications = true;  // Email in addition to in-app
    int notificationFrequency = 1;   // 1=daily, 7=weekly, 0=instant
};

struct FollowStats {
    int totalFollowers;
    int activeFollowers;             // Engaged in last 30 days
    int newFollowersThisMonth;
    std::vector<FollowerSegment> followerSegments; // By industry, location, etc.
    Date lastFollowerAt;
};
```

### Follows Service
```cpp
class FollowsService {
public:
    static FollowResult followProfile(const std::string& profileId,
                                    const std::string& followerId,
                                    FollowType type = FollowType::PROFILE_FOLLOW,
                                    NotificationSettings notifications = {});

    static bool unfollowProfile(const std::string& profileId,
                              const std::string& followerId);

    static std::vector<ProfileFollow> getProfileFollowers(
        const std::string& profileId,
        const FollowQuery& query = {});

    static std::vector<std::string> getUserFollowing(
        const std::string& userId);

    static FollowStats getFollowStats(const std::string& profileId);

    static bool isUserFollowing(const std::string& profileId,
                              const std::string& userId);

private:
    static void notifyProfileOwner(const ProfileFollow& follow);
    static void updateFollowAnalytics(const ProfileFollow& follow);
    static void sendWelcomeNotification(const ProfileFollow& follow);
};
```

### Follow Button UI
```html
<div class="profile-follow-section">
    <button class="follow-button following" data-profile-id="profile-123" onclick="toggleFollow('profile-123')">
        <span class="follow-icon">✓</span>
        <span class="follow-text">Following</span>
        <span class="follower-count">1.2K followers</span>
    </button>

    <!-- Follow dropdown (for different follow types) -->
    <div class="follow-options-dropdown hidden">
        <button class="follow-option" data-type="PROFILE_FOLLOW">
            <span class="option-icon">👤</span>
            <span class="option-text">Follow Profile</span>
            <span class="option-desc">Get updates on their activity</span>
        </button>

        <button class="follow-option" data-type="COMPANY_FOLLOW">
            <span class="option-icon">🏢</span>
            <span class="option-text">Follow Company</span>
            <span class="option-desc">Get company news and updates</span>
        </button>
    </div>

    <!-- Follower preview -->
    <div class="follower-preview">
        <div class="follower-avatars">
            <img src="/avatars/follower1.jpg" class="follower-avatar">
            <img src="/avatars/follower2.jpg" class="follower-avatar">
            <img src="/avatars/follower3.jpg" class="follower-avatar">
        </div>
        <span class="follower-names">Sarah, John, and 1.2K others follow this profile</span>
    </div>
</div>
```

## 📊 Social Action Analytics

### Engagement Metrics
```cpp
struct SocialEngagementMetrics {
    std::string profileId;
    DateRange period;

    // Like metrics
    int totalLikes;
    int uniqueLikers;
    double likeRate;                 // Likes per profile view
    std::map<LikeType, int> likeTypeDistribution;
    std::vector<LikeTrend> likeTrends; // Daily/weekly trends

    // Follow metrics
    int totalFollowers;
    int newFollowers;
    int unfollows;
    double followRate;               // Follows per profile view
    double followerRetention;        // % of followers still following after 30 days

    // Combined engagement
    int totalSocialActions;          // Likes + follows
    double engagementRate;           // Social actions per view
    double socialProofScore;         // Overall social credibility score

    // Demographics
    std::vector<EngagerDemographic> likerDemographics;
    std::vector<EngagerDemographic> followerDemographics;

    // Timing
    std::map<std::string, int> engagementByHour;  // Peak engagement times
    std::map<std::string, int> engagementByDay;   // Peak days
};

struct EngagerDemographic {
    std::string category;             // "industry", "location", "company_size"
    std::string value;                // "Technology", "Tehran", "51-200"
    int count;
    double percentage;
};
```

### Analytics Dashboard
```cpp
class SocialAnalyticsService {
public:
    static SocialEngagementMetrics getEngagementMetrics(
        const std::string& profileId, DateRange range);

    static std::vector<EngagerDemographic> getEngagerDemographics(
        const std::string& profileId, const std::string& category);

    static std::vector<SocialAction> getRecentSocialActivity(
        const std::string& profileId, int limit = 20);

private:
    static void calculateEngagementTrends(SocialEngagementMetrics& metrics);
    static void analyzeDemographics(SocialEngagementMetrics& metrics);
    static double calculateSocialProofScore(const SocialEngagementMetrics& metrics);
};
```

## 🔔 Social Notifications

### Notification Types
```cpp
enum class SocialNotificationType {
    PROFILE_LIKED,
    PROFILE_FOLLOWED,
    LIKE_RECEIVED,
    FOLLOWER_MILESTONE,          // "You reached 100 followers!"
    ENGAGEMENT_SPIKE,            // Unusual engagement activity
    TOP_FAN_BADGE,              // Most engaged follower
    SOCIAL_PROOF_ACHIEVEMENT    // High social proof score
};

struct SocialNotification {
    std::string notificationId;
    std::string profileId;
    SocialNotificationType type;
    std::string actorId;              // Who performed the action
    std::string actorName;
    std::string message;
    Date createdAt;
    bool isRead = false;

    // Context data
    LikeType likeType;               // For like notifications
    FollowType followType;           // For follow notifications
    int milestoneCount;              // For milestone notifications
};
```

### Smart Notification System
```cpp
class SocialNotificationService {
public:
    static void notifyProfileLike(const ProfileLike& like);
    static void notifyProfileFollow(const ProfileFollow& follow);
    static void checkAndSendMilestoneNotifications(const std::string& profileId);

private:
    static bool shouldNotifyForAction(const std::string& profileId,
                                    SocialNotificationType type);
    static std::string generateNotificationMessage(SocialNotificationType type,
                                                 const std::string& actorName,
                                                 const std::string& profileName);
    static void batchSimilarNotifications(const std::string& profileId);
};
```

## 📈 Engagement Feed

### Activity Stream
```cpp
struct SocialActivity {
    std::string activityId;
    std::string profileId;
    SocialActivityType type;
    Date activityAt;

    // Actor information
    std::string actorId;
    std::string actorName;
    std::string actorAvatarUrl;

    // Activity details
    LikeType likeType;               // For likes
    FollowType followType;           // For follows
    std::string activityMessage;      // Human readable description

    // Engagement metrics
    int likeCount;                   // How many likes this activity got
    int commentCount;                // Comments on this activity
    bool isOwnerActivity;            // Activity by profile owner
};

enum class SocialActivityType {
    RECEIVED_LIKE,
    GAINED_FOLLOWER,
    MILESTONE_ACHIEVED,
    ENGAGEMENT_SPIKE,
    PROFILE_UPDATE_SHARED
};

class EngagementFeedService {
public:
    static std::vector<SocialActivity> getProfileActivityFeed(
        const std::string& profileId, int limit = 50, Date since = {});

    static void addActivityToFeed(const SocialActivity& activity);
    static void updateActivityEngagement(const std::string& activityId,
                                       int likeCount, int commentCount);

private:
    static std::string generateActivityMessage(const SocialActivity& activity);
    static void cleanOldActivities(const std::string& profileId);
};
```

## 🔒 Privacy Controls

### Social Privacy Settings
```cpp
struct SocialPrivacySettings {
    // Like visibility
    bool showLikeCount = true;        // Show total likes on profile
    bool showLikeBreakdown = true;    // Show reaction types
    bool showWhoLiked = false;        // Show who liked (privacy concern)

    // Follow visibility
    bool showFollowerCount = true;    // Show follower numbers
    bool showFollowerList = false;    // Show follower names (privacy concern)
    bool approveFollowRequests = false; // Manual approval for follows

    // Activity visibility
    bool showActivityFeed = true;     // Show engagement activity
    bool showSocialProof = true;      // Show badges and achievements

    // Notification preferences
    bool notifyOnLikes = true;
    bool notifyOnFollows = true;
    bool notifyOnMilestones = true;
    bool batchNotifications = true;   // Group similar notifications
};
```

## 📋 Implementation Plan

### Day 1: Likes System + Basic Follows
- Implement profile likes with reaction types
- Create follow/unfollow functionality
- Add basic social action tracking
- Test like/follow CRUD operations

### Day 1 Continued: Analytics + Notifications
- Build social engagement analytics
- Implement notification system
- Create engagement feed
- Add privacy controls and testing

## 🧪 Testing Strategy

### Likes Tests
```cpp
TEST(LikesTest, AddAndRemoveLike) {
    // Add like
    auto result = LikesService::addLike("profile-123", "user-456", LikeType::LOVE);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.likeId.empty());

    // Verify like exists
    EXPECT_TRUE(LikesService::hasUserLiked("profile-123", "user-456"));

    // Remove like
    bool removed = LikesService::removeLike("profile-123", "user-456");
    EXPECT_TRUE(removed);

    // Verify like removed
    EXPECT_FALSE(LikesService::hasUserLiked("profile-123", "user-456"));
}

TEST(LikesTest, LikeStatsCalculation) {
    // Create multiple likes
    LikesService::addLike("profile-123", "user-1", LikeType::LIKE);
    LikesService::addLike("profile-123", "user-2", LikeType::LOVE);
    LikesService::addLike("profile-123", "user-3", LikeType::SUPPORT);

    // Get stats
    auto stats = LikesService::getLikeStats("profile-123");

    // Verify calculations
    EXPECT_EQ(stats.totalLikes, 3);
    EXPECT_EQ(stats.likeTypeBreakdown[LikeType::LOVE], 1);
    EXPECT_EQ(stats.uniqueLikers, 3);
}
```

### Follows Tests
```cpp
TEST(FollowsTest, FollowUnfollowWorkflow) {
    // Follow profile
    auto result = FollowsService::followProfile("profile-123", "user-456");
    EXPECT_TRUE(result.success);

    // Verify following
    EXPECT_TRUE(FollowsService::isUserFollowing("profile-123", "user-456"));

    // Get followers
    auto followers = FollowsService::getProfileFollowers("profile-123");
    EXPECT_EQ(followers.size(), 1);
    EXPECT_EQ(followers[0].followerId, "user-456");

    // Unfollow
    bool unfollowed = FollowsService::unfollowProfile("profile-123", "user-456");
    EXPECT_TRUE(unfollowed);

    // Verify unfollowed
    EXPECT_FALSE(FollowsService::isUserFollowing("profile-123", "user-456"));
}

TEST(FollowsTest, FollowStatsAndAnalytics) {
    // Create multiple follows
    createTestFollows("profile-123", 25);

    // Get stats
    auto stats = FollowsService::getFollowStats("profile-123");

    // Verify calculations
    EXPECT_EQ(stats.totalFollowers, 25);
    EXPECT_TRUE(stats.lastFollowerAt > yesterday());
    EXPECT_TRUE(!stats.followerSegments.empty());
}
```

## 🎉 Success Criteria

### Likes System
- ✅ **Multiple like types (reactions) supported**
- ✅ **Like/unlike functionality works**
- ✅ **Like statistics and breakdowns calculated**
- ✅ **Privacy controls for like visibility**

### Follows System
- ✅ **Follow/unfollow functionality implemented**
- ✅ **Follower management and statistics**
- ✅ **Notification preferences supported**
- ✅ **Follow types (profile, company, industry)**

### Analytics & Engagement
- ✅ **Social engagement metrics calculated**
- ✅ **Demographic analysis available**
- ✅ **Engagement feed populated**
- ✅ **Social proof scores computed**

### Notifications & Privacy
- ✅ **Smart notification system implemented**
- ✅ **Milestone notifications triggered**
- ✅ **Privacy controls enforced**
- ✅ **No unwanted notifications**

This creates a **rich social engagement layer** that encourages **interaction and community building** while maintaining **user privacy and control**.
