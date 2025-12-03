# 🚀 Viral Sharing - Referral Program & Rewards

**Duration:** 1 day
**Dependencies:** 26b-viral-mechanics.md (viral tracking system)
**Acceptance Criteria:**
- ✅ Referral program with unique referral codes
- ✅ Reward system for successful referrals
- ✅ Referral tracking and analytics
- ✅ Reward redemption and management
- ✅ Leaderboard system for top referrers
- ✅ Automated reward distribution

## 🎯 Task Description

Implement a comprehensive referral program that rewards users for successfully bringing new users to the platform, creating sustainable viral growth through incentives and gamification.

## 🎫 Referral Code System

### Referral Code Generation
```cpp
enum class ReferralCodeType {
    PERSONAL,           // User's personal referral code
    CAMPAIGN,           // Campaign-specific codes
    LIMITED_TIME,       // Time-limited promotional codes
    AFFILIATE           // Affiliate/partner codes
};

struct ReferralCode {
    std::string codeId;
    std::string code;                   // The actual referral code (e.g., "JOHN2024")
    ReferralCodeType type;
    std::string ownerId;               // Who owns this code
    
    // Code properties
    Date createdAt;
    Date expiresAt;
    bool isActive = true;
    int maxUses = 0;                   // 0 = unlimited
    
    // Usage tracking
    int totalUses = 0;
    int successfulReferrals = 0;       // Converted to registered users
    std::vector<ReferralUse> uses;
    
    // Customization
    std::string customMessage;          // Message to show with code
    std::string landingPageUrl;         // Custom landing page
    
    // Analytics
    std::map<std::string, int> usesBySource; // How code was shared
    std::map<std::string, int> conversionsBySource;
};

struct ReferralUse {
    std::string useId;
    std::string referralCode;
    std::string userId;                // Who used the code (null if anonymous)
    Date usedAt;
    
    // Context
    std::string ipAddress;
    std::string userAgent;
    std::string referrer;
    std::string campaignId;
    
    // Conversion tracking
    bool convertedToSignup = false;
    bool convertedToProfile = false;
    bool convertedToActiveUser = false; // Completed onboarding
    Date conversionDate;
    
    // Attribution
    std::string referringUserId;       // Who shared the code
};

class ReferralCodeService {
public:
    static ReferralCode generateReferralCode(
        const std::string& userId,
        ReferralCodeType type = ReferralCodeType::PERSONAL);
    
    static ReferralCode getReferralCode(const std::string& code);
    
    static void trackReferralUse(const std::string& code,
                               const ReferralUseData& useData);
    
    static ReferralCodeAnalytics getCodeAnalytics(
        const std::string& codeId);
    
private:
    static std::string generateUniqueCode(const std::string& userId);
    static bool validateCodeFormat(const std::string& code);
    static void updateCodeStats(const std::string& codeId,
                              const ReferralUse& use);
};
```

### Code Analytics
```cpp
struct ReferralCodeAnalytics {
    std::string codeId;
    std::string code;
    DateRange period;
    
    // Usage metrics
    int totalClicks;
    int totalSignups;
    int totalProfilesCreated;
    int totalActiveUsers;
    
    // Conversion rates
    double clickToSignupRate;
    double signupToProfileRate;
    double profileToActiveRate;
    double overallConversionRate;
    
    // Source breakdown
    std::map<std::string, int> clicksBySource;
    std::map<std::string, int> conversionsBySource;
    
    // Geographic insights
    std::map<std::string, int> clicksByCountry;
    std::map<std::string, int> conversionsByCountry;
    
    // Time-based trends
    std::vector<DailyReferralStats> dailyTrends;
    std::string peakDay;               // Day with most activity
    std::string peakHour;              // Hour with most activity
    
    // Performance score
    double performanceScore;           // 0-100 based on conversion rates
    std::vector<std::string> optimizationTips;
};

struct DailyReferralStats {
    Date date;
    int clicks;
    int signups;
    int conversions;
    double conversionRate;
};
```

## 🎁 Reward System Architecture

### Reward Types and Structure
```cpp
enum class RewardType {
    PROFILE_BOOST,             // Temporary profile visibility boost
    FEATURE_UNLOCK,            // Unlock premium features
    BADGE_AWARD,               // Special achievement badge
    PROFILE_CUSTOMIZATION,     // Custom profile elements
    PRIORITY_SUPPORT,          // Priority customer support
    EXCLUSIVE_ACCESS,          // Access to beta features/events
    MONETARY_CREDIT           // Platform credit for future use
};

enum class RewardTier {
    BRONZE,                    // Basic rewards
    SILVER,                    // Medium value rewards
    GOLD,                      // High value rewards
    PLATINUM                   // Premium rewards
};

struct ReferralReward {
    std::string rewardId;
    std::string rewardName;
    std::string description;
    RewardType type;
    RewardTier tier;
    
    // Value and requirements
    double monetaryValue;              // For credit rewards
    int requiredReferrals;             // Referrals needed to unlock
    int pointsCost;                    // Alternative point system
    
    // Availability
    Date availableFrom;
    Date expiresAt;
    int maxClaims = 0;                 // Limited availability
    int currentClaims = 0;
    
    // Visual
    std::string iconUrl;
    std::string badgeColor;
    
    // Usage tracking
    int timesClaimed = 0;
    double satisfactionScore = 0.0;    // User satisfaction rating
};

struct UserReward {
    std::string userRewardId;
    std::string userId;
    std::string rewardId;
    
    // Reward instance
    RewardStatus status = RewardStatus::EARNED;
    Date earnedAt;
    Date claimedAt;
    Date expiresAt;
    
    // Usage
    bool isUsed = false;
    Date usedAt;
    std::string usageDetails;          // How it was used
    
    // Value tracking
    double monetaryValue;              // If applicable
    int pointsValue;
};

enum class RewardStatus {
    EARNED,                    // Available to claim
    CLAIMED,                   // Claimed but not used
    USED,                      // Successfully used
    EXPIRED,                   // Expired without use
    REVOKED                    // Revoked due to policy violation
};

class RewardSystemService {
public:
    static std::vector<ReferralReward> getAvailableRewards(
        const std::string& userId);
    
    static UserReward claimReward(const std::string& userId,
                                const std::string& rewardId);
    
    static std::vector<UserReward> getUserRewards(
        const std::string& userId,
        RewardStatus status = RewardStatus::ALL);
    
    static void useReward(const std::string& userRewardId,
                        const std::string& usageDetails = "");
    
    static RewardAnalytics getRewardAnalytics(DateRange range = {});
    
private:
    static bool canUserClaimReward(const std::string& userId,
                                  const ReferralReward& reward);
    
    static void applyRewardEffect(const std::string& userId,
                                const ReferralReward& reward);
    
    static void trackRewardUsage(const UserReward& userReward);
};
```

## 🏆 Leaderboard System

### Referral Leaderboards
```cpp
enum class LeaderboardType {
    MOST_REFERRALS,            // Most successful referrals
    HIGHEST_CONVERSION_RATE,   // Best conversion rates
    MOST_INFLUENTIAL,          // Biggest impact on platform growth
    FASTEST_GROWTH,            // Fastest referral growth this month
    MOST_ACTIVE,               // Most active referrers this week
    TOP_PERFORMER              // Overall top performer
};

enum class LeaderboardPeriod {
    ALL_TIME,
    THIS_MONTH,
    THIS_WEEK,
    TODAY
};

struct LeaderboardEntry {
    std::string userId;
    std::string userName;
    std::string userSlug;
    std::string userAvatarUrl;
    
    // Ranking
    int rank;
    int previousRank;               // Last period rank
    bool isRising = false;
    bool isNew = false;
    
    // Metrics (varies by leaderboard type)
    int primaryMetric;              // Main stat (referrals, conversion %)
    std::string primaryMetricLabel; // "42 referrals", "87% conversion"
    
    // Secondary metrics
    int secondaryMetric;
    std::string secondaryMetricLabel;
    
    // Trend
    int rankChange;                 // +2, -1, etc.
    double growthRate;              // % growth from last period
    
    // Rewards/achievements
    std::vector<std::string> earnedBadges;
    std::vector<std::string> availableRewards;
};

struct ReferralLeaderboard {
    LeaderboardType type;
    LeaderboardPeriod period;
    Date generatedAt;
    
    std::vector<LeaderboardEntry> entries;
    int totalParticipants;
    
    // Metadata
    std::string title;
    std::string description;
    std::string metricDescription;   // What the primary metric represents
};

class LeaderboardService {
public:
    static ReferralLeaderboard getLeaderboard(
        LeaderboardType type,
        LeaderboardPeriod period = LeaderboardPeriod::THIS_MONTH,
        int limit = 50);
    
    static LeaderboardEntry getUserRank(
        const std::string& userId,
        LeaderboardType type,
        LeaderboardPeriod period = LeaderboardPeriod::THIS_MONTH);
    
    static void updateLeaderboards();    // Run periodically
    
    static std::vector<ReferralMilestone> getMilestoneRewards(
        LeaderboardType type);
    
private:
    static void calculateLeaderboardRanks(
        LeaderboardType type, LeaderboardPeriod period);
    
    static int calculateUserScore(const std::string& userId,
                                LeaderboardType type,
                                LeaderboardPeriod period);
    
    static void awardMilestoneRewards(const ReferralLeaderboard& leaderboard);
};
```

## 📊 Referral Analytics & Tracking

### Comprehensive Analytics
```cpp
struct ReferralAnalytics {
    std::string userId;              // Individual user analytics
    DateRange period;
    
    // Personal referral performance
    int totalReferrals;
    int successfulReferrals;
    int pendingReferrals;            // Signed up but not active yet
    double conversionRate;
    
    // Code performance
    std::vector<ReferralCodePerformance> codePerformance;
    std::string bestPerformingCode;
    std::string mostUsedCode;
    
    // Reward earnings
    int rewardsEarned;
    int rewardsClaimed;
    int rewardsUsed;
    double totalRewardValue;         // Monetary value earned
    
    // Network impact
    int networkSize;                 // Total users in referral network
    int networkDepth;                // Deepest referral chain
    double networkGrowthRate;
    
    // Trends and insights
    std::vector<MonthlyReferralStats> monthlyTrends;
    std::vector<std::string> topReferralSources;
    std::vector<std::string> optimizationTips;
    
    // Platform contribution
    double platformGrowthContribution; // % of platform growth from this user
    int estimatedNewUsers;            // Users brought to platform
};

struct ReferralCodePerformance {
    std::string code;
    int uses;
    int conversions;
    double conversionRate;
    Date lastUsed;
    std::string topSource;           // How it's most shared
};

struct MonthlyReferralStats {
    Date month;
    int referrals;
    int conversions;
    double conversionRate;
    int rewardsEarned;
    double rewardValue;
};

class ReferralAnalyticsService {
public:
    static ReferralAnalytics getReferralAnalytics(
        const std::string& userId,
        DateRange range = last30Days());
    
    static PlatformReferralAnalytics getPlatformAnalytics(
        DateRange range = last30Days());
    
    static std::vector<ReferralInsight> generateReferralInsights(
        const std::string& userId);
    
private:
    static void calculateNetworkMetrics(ReferralAnalytics& analytics);
    static void analyzeReferralTrends(ReferralAnalytics& analytics);
    static std::vector<ReferralInsight> identifyOptimizationOpportunities(
        const ReferralAnalytics& analytics);
};
```

## 🎯 Automated Reward Distribution

### Reward Automation Engine
```cpp
struct RewardRule {
    std::string ruleId;
    std::string ruleName;
    RewardTrigger trigger;
    
    // Conditions
    int thresholdValue;              // e.g., 5 successful referrals
    std::string thresholdMetric;     // "successful_referrals", "conversion_rate"
    RewardTier minimumTier = RewardTier::BRONZE;
    
    // Reward
    std::string rewardId;
    bool autoGrant = true;           // Grant automatically or require claim
    
    // Limits
    int maxGrantsPerUser = 1;        // How many times user can get this reward
    Date expiresAt;
    bool isActive = true;
};

enum class RewardTrigger {
    REFERRAL_MILESTONE,          // Reach X successful referrals
    CONVERSION_RATE_ACHIEVEMENT, // Achieve X% conversion rate
    NETWORK_SIZE_MILESTONE,      // Build network of X users
    LEADERBOARD_RANKING,         // Reach top X on leaderboard
    TIME_BASED_ACHIEVEMENT,      // Consistent referrals over time
    PLATFORM_CONTRIBUTION        // Significant platform growth contribution
};

class RewardAutomationService {
public:
    static void checkAndGrantRewards(const std::string& userId);
    
    static void processReferralMilestone(const std::string& userId,
                                       int milestoneValue);
    
    static void processLeaderboardAchievement(const std::string& userId,
                                            LeaderboardType type,
                                            int rank);
    
    static std::vector<RewardRule> getApplicableRules(
        const std::string& userId);
    
private:
    static bool meetsRuleConditions(const std::string& userId,
                                  const RewardRule& rule);
    
    static void grantRewardToUser(const std::string& userId,
                                const RewardRule& rule);
    
    static void notifyRewardEarned(const std::string& userId,
                                 const ReferralReward& reward);
};
```

## 📋 Implementation Plan

### Day 1: Referral Codes + Rewards
- Implement referral code generation and tracking
- Create reward system with different reward types
- Add referral analytics and conversion tracking
- Build leaderboard system for top referrers

### Day 1 Continued: Automation + Analytics
- Implement automated reward distribution
- Create comprehensive referral analytics
- Add referral insights and optimization tips
- Test end-to-end referral flow

## 🧪 Testing Strategy

### Referral Code Tests
```cpp
TEST(ReferralCodeTest, GenerateAndTrackCode) {
    auto code = ReferralCodeService::generateReferralCode("user-123");
    
    EXPECT_FALSE(code.code.empty());
    EXPECT_EQ(code.ownerId, "user-123");
    EXPECT_TRUE(code.isActive);
    
    // Test code usage tracking
    ReferralUseData useData{.usedAt = now()};
    ReferralCodeService::trackReferralUse(code.code, useData);
    
    auto updatedCode = ReferralCodeService::getReferralCode(code.code);
    EXPECT_EQ(updatedCode.totalUses, 1);
}
```

### Reward System Tests
```cpp
TEST(RewardSystemTest, EarnAndClaimReward) {
    // Simulate successful referrals
    createSuccessfulReferrals("user-123", 5);
    
    // Check available rewards
    auto rewards = RewardSystemService::getAvailableRewards("user-123");
    EXPECT_FALSE(rewards.empty());
    
    // Claim a reward
    auto userReward = RewardSystemService::claimReward("user-123", rewards[0].rewardId);
    EXPECT_EQ(userReward.status, RewardStatus::CLAIMED);
    
    // Use the reward
    RewardSystemService::useReward(userReward.userRewardId, "Used for profile boost");
    EXPECT_EQ(userReward.status, RewardStatus::USED);
}
```

### Leaderboard Tests
```cpp
TEST(LeaderboardTest, CalculateReferralRanks) {
    // Create users with different referral counts
    createUserWithReferrals("user-a", 10);
    createUserWithReferrals("user-b", 25);
    createUserWithReferrals("user-c", 5);
    
    // Update leaderboards
    LeaderboardService::updateLeaderboards();
    
    // Get leaderboard
    auto leaderboard = LeaderboardService::getLeaderboard(
        LeaderboardType::MOST_REFERRALS);
    
    // User B should be #1
    EXPECT_EQ(leaderboard.entries[0].userId, "user-b");
    EXPECT_EQ(leaderboard.entries[0].primaryMetric, 25);
}
```

## 🎉 Success Criteria

### Referral Program
- ✅ **Unique referral code generation**
- ✅ **Referral link tracking and attribution**
- ✅ **Multi-level referral chain support**
- ✅ **Referral source analytics**

### Reward System
- ✅ **Diverse reward types and tiers**
- ✅ **Automatic reward qualification**
- ✅ **Reward claiming and redemption**
- ✅ **Reward usage tracking**

### Gamification
- ✅ **Referral leaderboards and rankings**
- ✅ **Achievement badges for milestones**
- ✅ **Social comparison features**
- ✅ **Motivational progress tracking**

### Analytics & Optimization
- ✅ **Comprehensive referral analytics**
- ✅ **Conversion rate optimization**
- ✅ **Referral channel effectiveness**
- ✅ **Predictive referral modeling**

This creates a **sustainable viral growth engine** through **incentivized referrals** that **rewards users for platform growth** while providing **comprehensive analytics** to optimize referral performance.
