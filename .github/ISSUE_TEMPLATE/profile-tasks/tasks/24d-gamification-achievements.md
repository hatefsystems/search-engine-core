# 🚀 Social Engagement - Gamification & Achievements System

**Duration:** 1 day
**Dependencies:** 24c-comments-discussions.md (engagement foundation)
**Acceptance Criteria:**
- ✅ Achievement badge system with unlock criteria
- ✅ Profile completeness gamification
- ✅ Milestone tracking and celebrations
- ✅ Leaderboards and social comparison
- ✅ Reward system for engagement
- ✅ Achievement notifications and progress tracking
- ✅ Gamified profile optimization suggestions

## 🎯 Task Description

Implement a comprehensive gamification system that rewards users for engagement, profile completeness, and social interactions through badges, milestones, and leaderboards to increase platform stickiness and encourage quality content creation.

## 🏆 Achievement Badge System

### Badge Categories and Types
```cpp
enum class BadgeCategory {
    PROFILE_COMPLETENESS,       // Profile building achievements
    SOCIAL_ENGAGEMENT,          // Likes, follows, comments
    CONTENT_CREATION,           // Projects, posts, updates
    NETWORK_BUILDING,           // Connections and relationships
    VERIFICATION_STATUS,        // Trust and credibility
    TIME_BASED,                 // Consistency and longevity
    SPECIAL_EVENTS             // Seasonal or promotional
};

enum class BadgeRarity {
    COMMON,         // Easy to achieve (bronze)
    UNCOMMON,       // Moderate difficulty (silver)
    RARE,           // Challenging (gold)
    EPIC,           // Very challenging (platinum)
    LEGENDARY       // Exceptional achievements (diamond)
};

struct AchievementBadge {
    std::string badgeId;
    std::string name;
    std::string description;
    BadgeCategory category;
    BadgeRarity rarity;

    // Visual properties
    std::string iconUrl;
    std::string color;              // CSS color code
    std::string borderStyle;        // "solid", "dashed", "gradient"

    // Unlock criteria
    BadgeUnlockCriteria criteria;

    // Metadata
    Date createdAt;
    bool isActive = true;
    int timesAwarded = 0;          // Popularity metric
};
```

### Badge Unlock Criteria
```cpp
struct BadgeUnlockCriteria {
    std::string criteriaType;       // "profile_completeness", "follower_count", etc.

    // Threshold-based criteria
    int thresholdValue;             // e.g., 100 followers
    std::string thresholdUnit;      // "count", "percentage", "days"

    // Time-based criteria
    int timeWindowDays;             // e.g., 30 days for consistency badges
    bool requiresConsecutive;       // e.g., 7 consecutive days

    // Complex criteria
    std::vector<std::string> requiredActions;    // Multiple actions needed
    std::map<std::string, int> actionCounts;     // Action type -> count

    // Special conditions
    std::string prerequisiteBadge;   // Must have this badge first
    bool requiresVerification;       // Must be verified
    std::vector<std::string> locationRequirements; // Location-based badges
};
```

### Badge Examples
```cpp
// Profile completeness badges
const AchievementBadge PROFILE_STARTER = {
    .badgeId = "profile_starter",
    .name = "Profile Starter",
    .description = "Completed your basic profile information",
    .category = BadgeCategory::PROFILE_COMPLETENESS,
    .rarity = BadgeRarity::COMMON,
    .criteria = {
        .criteriaType = "profile_completeness",
        .thresholdValue = 60,        // 60% complete
        .thresholdUnit = "percentage"
    }
};

const AchievementBadge PROFILE_MASTER = {
    .badgeId = "profile_master",
    .name = "Profile Master",
    .description = "Achieved 100% profile completeness",
    .category = BadgeCategory::PROFILE_COMPLETENESS,
    .rarity = BadgeRarity::RARE,
    .criteria = {
        .criteriaType = "profile_completeness",
        .thresholdValue = 100,
        .thresholdUnit = "percentage"
    }
};

// Social engagement badges
const AchievementBadge SOCIAL_BUTTERFLY = {
    .badgeId = "social_butterfly",
    .name = "Social Butterfly",
    .description = "Connected with 50+ profiles",
    .category = BadgeCategory::NETWORK_BUILDING,
    .rarity = BadgeRarity::UNCOMMON,
    .criteria = {
        .criteriaType = "connection_count",
        .thresholdValue = 50,
        .thresholdUnit = "count"
    }
};

const AchievementBadge TRENDING_PROFILE = {
    .badgeId = "trending_profile",
    .name = "Trending Profile",
    .description = "Appeared in trending profiles for 7 consecutive days",
    .category = BadgeCategory::SOCIAL_ENGAGEMENT,
    .rarity = BadgeRarity::EPIC,
    .criteria = {
        .criteriaType = "trending_streak",
        .thresholdValue = 7,
        .thresholdUnit = "days",
        .requiresConsecutive = true
    }
};
```

## 🎖️ Badge Management System

### Badge Awarding Service
```cpp
class BadgeAwardingService {
public:
    static BadgeAwardResult checkAndAwardBadges(const std::string& userId);

    static bool awardBadge(const std::string& userId,
                         const std::string& badgeId,
                         const std::string& triggerReason = "");

    static std::vector<AchievementBadge> getEarnedBadges(const std::string& userId);

    static std::vector<AchievementBadge> getAvailableBadges(const std::string& userId);

    static BadgeProgress getBadgeProgress(const std::string& userId,
                                        const std::string& badgeId);

private:
    static void evaluateBadgeCriteria(const std::string& userId,
                                    const AchievementBadge& badge);

    static bool meetsCriteria(const std::string& userId,
                            const BadgeUnlockCriteria& criteria);

    static void notifyBadgeEarned(const std::string& userId,
                                const AchievementBadge& badge);

    static void updateBadgeStatistics(const AchievementBadge& badge);
};
```

### Badge Progress Tracking
```cpp
struct BadgeProgress {
    std::string badgeId;
    std::string badgeName;
    BadgeCategory category;
    BadgeRarity rarity;

    // Progress metrics
    int currentValue;               // Current progress (e.g., 25 followers)
    int targetValue;                // Required value (e.g., 100 followers)
    double progressPercentage;      // 0.0 - 1.0

    // Time tracking
    Date startedAt;                 // When progress started
    Date estimatedCompletion;       // When they might complete it
    bool isCompleted;               // Badge earned?

    // Tips and hints
    std::string nextStepHint;       // "Follow 25 more profiles to earn this badge"
    std::vector<std::string> actionSuggestions; // Specific actions to take
};
```

## 📊 Profile Completeness Gamification

### Completeness Scoring Engine
```cpp
struct CompletenessSection {
    std::string sectionId;
    std::string sectionName;        // "Basic Info", "Experience", "Projects"
    double weight;                  // Importance weight (0.0-1.0)

    // Fields in this section
    std::vector<CompletenessField> fields;
    int completedFields;
    int totalFields;
    double sectionScore;            // 0.0 - 1.0
};

struct CompletenessField {
    std::string fieldId;
    std::string fieldName;          // "Profile Photo", "Job Title"
    bool isCompleted;
    double weight;                  // Field importance
    std::string completionHint;     // How to complete it
    std::string value;              // Current field value (if any)
};

class ProfileCompletenessEngine {
public:
    static CompletenessScore calculateProfileCompleteness(const std::string& profileId);

    static std::vector<CompletenessSection> getCompletenessBreakdown(
        const std::string& profileId);

    static std::vector<std::string> getCompletionSuggestions(
        const std::string& profileId);

    static void updateCompletenessOnChange(const std::string& profileId,
                                         const std::string& fieldChanged);

private:
    static double calculateWeightedScore(const std::vector<CompletenessSection>& sections);
    static void generateSmartSuggestions(CompletenessScore& score);
};
```

### Completeness Sections Definition
```cpp
const std::vector<CompletenessSection> PROFILE_SECTIONS = {
    {
        .sectionId = "basic_info",
        .sectionName = "Basic Information",
        .weight = 0.25,
        .fields = {
            {"avatar", "Profile Photo", false, 0.3, "Upload a professional photo"},
            {"name", "Display Name", false, 0.4, "Add your full name"},
            {"bio", "Bio/About", false, 0.3, "Write a short bio"}
        }
    },
    {
        .sectionId = "professional_info",
        .sectionName = "Professional Information",
        .weight = 0.30,
        .fields = {
            {"title", "Job Title", false, 0.3, "Add your current job title"},
            {"company", "Company", false, 0.2, "Add your company name"},
            {"skills", "Skills", false, 0.3, "Add at least 3 skills"},
            {"experience", "Experience Level", false, 0.2, "Select your experience level"}
        }
    },
    {
        .sectionId = "content_showcase",
        .sectionName = "Content Showcase",
        .weight = 0.25,
        .fields = {
            {"projects", "Projects", false, 0.4, "Add at least one project"},
            {"education", "Education", false, 0.3, "Add your education"},
            {"social_links", "Social Links", false, 0.3, "Add LinkedIn or GitHub"}
        }
    },
    {
        .sectionId = "network_social",
        .sectionName = "Network & Social",
        .weight = 0.20,
        .fields = {
            {"connections", "Professional Connections", false, 0.5, "Connect with 5+ profiles"},
            {"verification", "Profile Verification", false, 0.5, "Get your profile verified"}
        }
    }
};
```

## 🏅 Milestone Celebration System

### Milestone Types
```cpp
enum class MilestoneType {
    PROFILE_VIEWS,              // "Your profile reached 1000 views!"
    FOLLOWERS_COUNT,            // "You gained your 100th follower!"
    CONNECTIONS_MADE,           // "Connected with 50 professionals!"
    COMMENTS_RECEIVED,          // "Received 25 profile comments!"
    PROJECTS_ADDED,             // "Showcased 5 projects!"
    BADGES_EARNED,              // "Earned 10 achievement badges!"
    VERIFICATION_STATUS,        // "Profile verified - Gold tier!"
    ENGAGEMENT_STREAK           // "7-day engagement streak!"
};

struct MilestoneCelebration {
    std::string milestoneId;
    std::string profileId;
    MilestoneType type;
    std::string title;               // Celebration title
    std::string message;             // Celebration message
    std::string iconUrl;             // Celebration icon
    Date achievedAt;

    // Celebration properties
    bool showConfetti = false;       // Visual celebration
    bool playSound = false;          // Audio celebration
    std::string celebrationColor;    // UI theme color
    int celebrationDuration;         // How long to show (seconds)

    // Sharing
    bool isShareable = true;         // Can be shared on social media
    std::string shareMessage;        // Pre-written share text
};
```

### Milestone Detection and Celebration
```cpp
class MilestoneCelebrationService {
public:
    static void checkForMilestones(const std::string& profileId);

    static void triggerMilestoneCelebration(const std::string& profileId,
                                          MilestoneType type,
                                          const std::string& achievementValue);

    static std::vector<MilestoneCelebration> getRecentMilestones(
        const std::string& profileId, int limit = 10);

private:
    static bool hasMilestoneBeenAchieved(const std::string& profileId,
                                       MilestoneType type,
                                       const std::string& value);

    static std::string generateMilestoneTitle(MilestoneType type, const std::string& value);
    static std::string generateMilestoneMessage(MilestoneType type, const std::string& value);
    static void createCelebrationNotification(const MilestoneCelebration& milestone);
};
```

## 🏆 Leaderboards and Social Comparison

### Leaderboard Categories
```cpp
enum class LeaderboardType {
    MOST_FOLLOWERS,             // Most followers
    MOST_PROFILE_VIEWS,         // Most profile views this month
    MOST_ENGAGED,               // Highest engagement rate
    MOST_COMPLETE_PROFILES,     // Highest completeness score
    MOST_BADGES_EARNED,         // Most achievement badges
    FASTEST_GROWING,            // Fastest follower growth this month
    MOST_HELPFUL,               // Most comments liked/replied
    TRENDING_PROFILES           // Currently trending
};

struct LeaderboardEntry {
    std::string profileId;
    std::string profileName;
    std::string profileSlug;
    std::string profileAvatarUrl;

    // Ranking data
    int rank;                       // Current rank (1, 2, 3...)
    int previousRank;               // Last period rank
    int rankChange;                 // Change from last period (+2, -1, etc.)

    // Metric value
    int metricValue;                // e.g., 1250 followers
    std::string metricLabel;        // e.g., "1.2K followers"

    // Trend indicators
    bool isRising = false;          // Gaining rank
    bool isFalling = false;         // Losing rank
    bool isNew = false;             // New to leaderboard
};

class LeaderboardService {
public:
    static std::vector<LeaderboardEntry> getLeaderboard(
        LeaderboardType type, int limit = 50);

    static LeaderboardEntry getUserRank(const std::string& userId,
                                      LeaderboardType type);

    static std::vector<LeaderboardEntry> getNearbyRanks(
        const std::string& userId, LeaderboardType type, int radius = 5);

private:
    static void updateLeaderboards();   // Run daily/weekly
    static void calculateRankings(LeaderboardType type);
    static void detectRankChanges();
};
```

## 🎁 Reward System

### Reward Types
```cpp
enum class RewardType {
    PROFILE_BOOST,               // Temporary profile visibility boost
    FEATURED_PLACEMENT,          // Featured in explore/discovery
    CUSTOM_BADGE,                // Special achievement badge
    PREMIUM_FEATURES,            // Temporary premium features
    SOCIAL_RECOGNITION,          // Highlighted in community
    EXCLUSIVE_CONTENT           // Access to exclusive content/features
};

struct AchievementReward {
    std::string rewardId;
    std::string badgeId;             // Which badge earns this reward
    RewardType type;
    std::string title;
    std::string description;

    // Reward properties
    int durationDays;                // How long reward lasts
    std::string rewardValue;         // e.g., "2x visibility", "custom badge"
    Date grantedAt;
    Date expiresAt;

    // Redemption status
    bool isRedeemed = false;
    Date redeemedAt;
};

class RewardSystemService {
public:
    static void grantBadgeReward(const std::string& userId,
                               const std::string& badgeId);

    static std::vector<AchievementReward> getAvailableRewards(
        const std::string& userId);

    static bool redeemReward(const std::string& userId,
                           const std::string& rewardId);

    static void checkExpiredRewards();    // Clean up expired rewards

private:
    static AchievementReward createRewardForBadge(const AchievementBadge& badge);
    static void applyRewardEffect(const std::string& userId,
                                const AchievementReward& reward);
};
```

## 📋 Implementation Plan

### Day 1: Badge System + Completeness Engine
- Implement achievement badge system with unlock criteria
- Create profile completeness gamification engine
- Add badge awarding and progress tracking
- Build basic leaderboards

### Day 1 Continued: Milestones + Rewards
- Implement milestone celebration system
- Create reward system for achievements
- Add leaderboard calculations and display
- Test end-to-end gamification flow

## 🧪 Testing Strategy

### Badge System Tests
```cpp
TEST(BadgeSystemTest, BadgeUnlockCriteria) {
    // Test profile completeness badge
    auto badge = PROFILE_STARTER;

    // Create incomplete profile
    auto incompleteProfile = createIncompleteProfile();
    bool shouldUnlockIncomplete = BadgeAwardingService::meetsCriteria(
        incompleteProfile.id, badge.criteria);
    EXPECT_FALSE(shouldUnlockIncomplete);

    // Complete profile
    completeProfile(incompleteProfile.id);
    bool shouldUnlockComplete = BadgeAwardingService::meetsCriteria(
        incompleteProfile.id, badge.criteria);
    EXPECT_TRUE(shouldUnlockComplete);
}

TEST(BadgeSystemTest, BadgeAwarding) {
    // Create user and earn badge
    auto userId = createTestUser();
    completeProfile(userId);  // Meet badge criteria

    // Check and award badges
    auto result = BadgeAwardingService::checkAndAwardBadges(userId);
    EXPECT_TRUE(result.badgesAwarded > 0);

    // Verify badge earned
    auto earnedBadges = BadgeAwardingService::getEarnedBadges(userId);
    EXPECT_FALSE(earnedBadges.empty());
    EXPECT_EQ(earnedBadges[0].badgeId, "profile_starter");
}
```

### Completeness Tests
```cpp
TEST(CompletenessTest, CompletenessCalculation) {
    // Create minimal profile
    auto profileId = createMinimalProfile();

    // Check initial completeness
    auto score1 = ProfileCompletenessEngine::calculateProfileCompleteness(profileId);
    EXPECT_LT(score1.overallScore, 0.5);  // Less than 50% complete

    // Add required fields
    addRequiredFields(profileId);

    // Check updated completeness
    auto score2 = ProfileCompletenessEngine::calculateProfileCompleteness(profileId);
    EXPECT_GT(score2.overallScore, 0.8);  // More than 80% complete
}

TEST(CompletenessTest, SmartSuggestions) {
    // Create profile missing photo and bio
    auto profileId = createIncompleteProfile();

    // Get suggestions
    auto suggestions = ProfileCompletenessEngine::getCompletionSuggestions(profileId);

    // Should suggest adding photo and bio
    EXPECT_TRUE(containsSuggestion(suggestions, "avatar"));
    EXPECT_TRUE(containsSuggestion(suggestions, "bio"));
}
```

### Leaderboard Tests
```cpp
TEST(LeaderboardTest, RankingCalculation) {
    // Create users with different follower counts
    auto user1 = createUserWithFollowers(10);
    auto user2 = createUserWithFollowers(50);
    auto user3 = createUserWithFollowers(25);

    // Update leaderboards
    LeaderboardService::updateLeaderboards();

    // Check rankings
    auto leaderboard = LeaderboardService::getLeaderboard(LeaderboardType::MOST_FOLLOWERS, 10);

    // User2 should be #1, User3 #2, User1 #3
    EXPECT_EQ(leaderboard[0].profileId, user2);
    EXPECT_EQ(leaderboard[1].profileId, user3);
    EXPECT_EQ(leaderboard[2].profileId, user1);
}
```

## 🎉 Success Criteria

### Badge System
- ✅ **Achievement badges with clear unlock criteria**
- ✅ **Automatic badge awarding based on user actions**
- ✅ **Badge progress tracking and hints**
- ✅ **Visual badge display on profiles**

### Gamification Engine
- ✅ **Profile completeness scoring and suggestions**
- ✅ **Milestone detection and celebration**
- ✅ **Reward system for achievements**
- ✅ **Progress tracking and motivation**

### Social Comparison
- ✅ **Leaderboards with fair ranking algorithms**
- ✅ **Nearby ranking display for context**
- ✅ **Trend indicators and rank changes**
- ✅ **Privacy controls for leaderboard participation**

### User Engagement
- ✅ **Achievement notifications and celebrations**
- ✅ **Gamified profile optimization**
- ✅ **Social recognition through leaderboards**
- ✅ **Continuous engagement through rewards**

This creates a **comprehensive gamification ecosystem** that **rewards quality contributions** and **encourages platform stickiness** through meaningful achievements and social comparison.
