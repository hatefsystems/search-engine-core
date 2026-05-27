# 🚀 Social Engagement - Profile Views & Visitor Tracking

**Duration:** 1 day
**Dependencies:** Profile database models, Analytics system
**Acceptance Criteria:**
- ✅ Profile view tracking system
- ✅ Visitor analytics (location, device, referrer)
- ✅ "Who viewed your profile" feature
- ✅ View notifications and alerts
- ✅ Anonymous vs authenticated view tracking
- ✅ Profile view statistics dashboard
- ✅ Privacy controls for view tracking

## 🎯 Task Description

Implement comprehensive profile view tracking that allows users to see who has viewed their profiles, understand their audience demographics, and receive notifications about profile engagement while respecting privacy preferences.

## 👁️ Profile View Tracking System

### View Event Structure
```cpp
struct ProfileViewEvent {
    std::string viewId;
    std::string profileId;           // Profile being viewed
    std::string viewerId;            // null if anonymous
    std::string viewerType;          // "ANONYMOUS", "REGISTERED", "OWNER"

    // Timing
    Date viewedAt;
    int viewDuration;                // seconds spent viewing
    std::string sessionId;           // User session tracking

    // Location & Device
    std::string ipAddress;           // For geo lookup (encrypted)
    std::string userAgent;
    std::string referrer;            // How they found the profile

    // Derived data (computed)
    GeoLocation geoLocation;
    DeviceInfo deviceInfo;
    ReferrerInfo referrerInfo;

    // Privacy & Settings
    bool isVisibleToOwner;           // Respects privacy settings
    bool triggersNotification;       // Should notify profile owner
};

struct GeoLocation {
    std::string country;
    std::string region;              // Province/State
    std::string city;
    double latitude;                 // Optional, city-level only
    double longitude;                // Optional, city-level only
};

struct DeviceInfo {
    std::string deviceType;          // "Desktop", "Mobile", "Tablet"
    std::string os;                  // "Windows", "iOS", "Android"
    std::string browser;             // "Chrome", "Safari", "Firefox"
};

struct ReferrerInfo {
    std::string source;              // "search", "direct", "social", "link"
    std::string medium;              // "organic", "paid", "referral"
    std::string campaign;            // Campaign identifier
};
```

### View Tracking Service
```cpp
class ProfileViewTracker {
public:
    static void trackProfileView(const ProfileViewEvent& event);
    static std::vector<ProfileViewEvent> getProfileViews(
        const std::string& profileId,
        const ViewQuery& query);

    static ProfileViewStats getViewStatistics(
        const std::string& profileId,
        DateRange range);

private:
    static GeoLocation resolveGeoLocation(const std::string& ip);
    static DeviceInfo parseDeviceInfo(const std::string& userAgent);
    static ReferrerInfo parseReferrerInfo(const std::string& referrer);
    static bool shouldTrackView(const ProfileViewEvent& event);
};
```

## 📊 Profile View Statistics

### Statistics Dashboard Data
```cpp
struct ProfileViewStats {
    std::string profileId;
    DateRange period;

    // Basic metrics
    int totalViews;
    int uniqueViewers;
    int anonymousViews;
    int registeredViews;
    double avgViewDuration;          // seconds

    // Geographic breakdown
    std::vector<LocationStat> topLocations;
    std::map<std::string, int> countryViews;

    // Device breakdown
    std::map<std::string, int> deviceTypeViews;    // Desktop: 45, Mobile: 32
    std::map<std::string, int> osViews;           // iOS: 28, Android: 25
    std::map<std::string, int> browserViews;      // Chrome: 40, Safari: 15

    // Traffic sources
    std::map<std::string, int> referrerViews;     // Search: 30, Direct: 20
    std::vector<ReferrerStat> topReferrers;

    // Trends
    std::vector<DailyStats> dailyBreakdown;
    double weekOverWeekChange;
    double monthOverMonthChange;

    // Engagement
    int profileCompletionViews;      // Views when profile was incomplete
    int verifiedViews;               // Views when profile was verified
    double engagementRate;           // Views leading to interactions
};

struct LocationStat {
    std::string location;             // "Tehran, Iran"
    int viewCount;
    double percentage;
};

struct DailyStats {
    Date date;
    int views;
    int uniqueViewers;
    double avgDuration;
};
```

### Statistics Calculation
```cpp
ProfileViewStats ProfileViewTracker::getViewStatistics(
    const std::string& profileId, DateRange range) {

    auto views = viewRepository.findByProfileId(profileId, range);

    ProfileViewStats stats;
    stats.profileId = profileId;
    stats.period = range;
    stats.totalViews = views.size();

    // Calculate unique viewers
    std::set<std::string> uniqueViewerIds;
    for (const auto& view : views) {
        if (!view.viewerId.empty()) {
            uniqueViewerIds.insert(view.viewerId);
        }
    }
    stats.uniqueViewers = uniqueViewerIds.size();

    // Geographic analysis
    std::map<std::string, int> countryCount;
    for (const auto& view : views) {
        countryCount[view.geoLocation.country]++;
    }

    // Convert to top locations
    for (const auto& [country, count] : countryCount) {
        stats.countryViews[country] = count;
        stats.topLocations.push_back({
            .location = country,
            .viewCount = count,
            .percentage = (count * 100.0) / stats.totalViews
        });
    }

    // Sort by view count
    std::sort(stats.topLocations.begin(), stats.topLocations.end(),
        [](const LocationStat& a, const LocationStat& b) {
            return a.viewCount > b.viewCount;
        });

    return stats;
}
```

## 👥 "Who Viewed Your Profile" Feature

### Viewer Information Structure
```cpp
struct ProfileViewer {
    std::string viewerId;
    std::string viewerName;
    std::string viewerSlug;
    std::string viewerAvatarUrl;
    std::string viewerHeadline;       // Job title or company

    // View details
    Date lastViewedAt;
    int totalViews;                  // How many times they've viewed
    double totalViewDuration;        // Total time spent

    // Relationship
    bool isConnected;                // Are they connected?
    bool viewedRecently;             // Viewed in last 7 days
    std::string connectionType;      // "none", "connection", "follower"

    // Privacy consideration
    bool canSeeViewerDetails;        // Based on viewer's privacy settings
};

struct WhoViewedResult {
    std::vector<ProfileViewer> recentViewers;      // Last 7 days
    std::vector<ProfileViewer> frequentViewers;    // Most frequent viewers
    int totalAnonymousViews;       // Views from anonymous users
    Date lastUpdated;
};
```

### Privacy-Aware Viewer Discovery
```cpp
WhoViewedResult ProfileViewTracker::getWhoViewedProfile(
    const std::string& profileId, int limit = 20) {

    WhoViewedResult result;

    // Get recent views (last 30 days)
    auto recentViews = viewRepository.findRecentViews(profileId, 30);

    // Group by viewer
    std::map<std::string, ViewerAggregate> viewerMap;
    for (const auto& view : recentViews) {
        if (view.viewerId.empty()) {
            result.totalAnonymousViews++;
            continue;  // Skip anonymous views
        }

        viewerMap[view.viewerId].addView(view);
    }

    // Convert to viewer objects
    for (const auto& [viewerId, aggregate] : viewerMap) {
        auto viewer = profileRepository.findById(viewerId);
        if (!viewer) continue;

        // Check privacy settings
        auto viewerSettings = privacySettingsRepository.findByProfileId(viewerId);
        if (viewerSettings && !viewerSettings->showInWhoViewed) {
            continue;  // Respect viewer's privacy
        }

        ProfileViewer profileViewer{
            .viewerId = viewerId,
            .viewerName = viewer->name,
            .viewerSlug = viewer->slug,
            .viewerAvatarUrl = viewer->avatarUrl,
            .viewerHeadline = getViewerHeadline(*viewer),
            .lastViewedAt = aggregate.lastViewedAt,
            .totalViews = aggregate.viewCount,
            .totalViewDuration = aggregate.totalDuration,
            .isConnected = checkConnection(profileId, viewerId),
            .viewedRecently = aggregate.lastViewedAt > (now() - 7_days),
            .canSeeViewerDetails = true
        };

        result.recentViewers.push_back(profileViewer);
    }

    // Sort by recency
    std::sort(result.recentViewers.begin(), result.recentViewers.end(),
        [](const ProfileViewer& a, const ProfileViewer& b) {
            return a.lastViewedAt > b.lastViewedAt;
        });

    // Limit results
    if (result.recentViewers.size() > limit) {
        result.recentViewers.resize(limit);
    }

    result.lastUpdated = now();
    return result;
}
```

## 🔔 View Notifications System

### Notification Types
```cpp
enum class ViewNotificationType {
    PROFILE_VIEWED,                 // Someone viewed your profile
    FREQUENT_VIEWER,               // Someone viewed multiple times
    HIGH_PROFILE_VIEWER,           // Someone with many followers viewed
    CONNECTION_VIEWED,             // A connection viewed your profile
    LOCATION_SPIKE,               // Unusual location activity
    DEVICE_SPIKE                  // Unusual device activity
};

struct ViewNotification {
    std::string notificationId;
    std::string profileId;           // Profile owner
    ViewNotificationType type;
    std::string triggerViewerId;     // Who triggered the notification
    std::string message;             // Human readable message
    Date triggeredAt;
    bool isRead = false;

    // Notification data
    std::string viewerName;
    std::string viewerHeadline;
    std::string location;            // "from Tehran, Iran"
    int viewCount;                   // For frequent viewer notifications
};
```

### Smart Notification Logic
```cpp
class ViewNotificationService {
public:
    static void checkAndSendNotifications(const ProfileViewEvent& viewEvent);

private:
    static bool shouldNotifyForView(const ProfileViewEvent& event);
    static ViewNotificationType determineNotificationType(const ProfileViewEvent& event);
    static std::string generateNotificationMessage(ViewNotificationType type,
                                                 const ProfileViewEvent& event);
    static bool isFrequentViewer(const std::string& viewerId, const std::string& profileId);
    static bool isHighProfileViewer(const std::string& viewerId);
};

void ViewNotificationService::checkAndSendNotifications(const ProfileViewEvent& viewEvent) {
    // Skip if owner viewed their own profile
    if (viewEvent.viewerType == "OWNER") return;

    // Check notification preferences
    auto preferences = notificationPreferencesRepository.findByProfileId(viewEvent.profileId);
    if (!preferences || !preferences->enableViewNotifications) return;

    // Determine if notification should be sent
    if (!shouldNotifyForView(viewEvent)) return;

    // Create notification
    auto notificationType = determineNotificationType(viewEvent);
    auto message = generateNotificationMessage(notificationType, viewEvent);

    ViewNotification notification{
        .notificationId = generateUUID(),
        .profileId = viewEvent.profileId,
        .type = notificationType,
        .triggerViewerId = viewEvent.viewerId,
        .message = message,
        .triggeredAt = now(),
        .viewerName = getViewerName(viewEvent),
        .location = formatLocation(viewEvent.geoLocation)
    };

    // Save and send
    notificationRepository.save(notification);
    notificationService.sendNotification(viewEvent.profileId, notification);
}
```

## 🔒 Privacy Controls

### View Tracking Privacy Settings
```cpp
struct ViewTrackingPrivacySettings {
    bool enableViewTracking = true;      // Track views at all
    bool showWhoViewed = true;           // Allow others to see who viewed them
    bool showInWhoViewed = true;         // Show in others' "who viewed" lists
    bool anonymizeOldViews = true;       // Anonymize views older than 30 days

    // Notification preferences
    bool enableViewNotifications = true;
    bool notifyOnConnectionViews = true; // Special notification for connections
    bool notifyOnFrequentViews = true;   // When someone views multiple times

    // Data retention
    int viewHistoryRetentionDays = 365;  // How long to keep view history
};

class ViewPrivacyController {
public:
    static bool canTrackView(const std::string& profileId, const ProfileViewEvent& event);
    static bool canShowViewerToOwner(const std::string& viewerId, const std::string& profileId);
    static void anonymizeOldViews(const std::string& profileId);
    static void deleteViewHistory(const std::string& profileId);
};
```

## 📋 Implementation Plan

### Day 1: View Tracking + Analytics
- Implement profile view event tracking
- Add geo location and device parsing
- Create basic view statistics dashboard
- Add privacy controls for view tracking

### Day 1 Continued: Who Viewed + Notifications
- Implement "who viewed your profile" feature
- Add view notifications system
- Create notification preferences
- Test privacy controls and anonymization

## 🧪 Testing Strategy

### View Tracking Tests
```cpp
TEST(ViewTrackingTest, BasicViewTracking) {
    // Create view event
    ProfileViewEvent event{
        .viewId = generateUUID(),
        .profileId = "profile-123",
        .viewerId = "viewer-456",
        .viewedAt = now(),
        .viewDuration = 45
    };

    // Track view
    ProfileViewTracker::trackProfileView(event);

    // Verify tracked
    auto views = ProfileViewTracker::getProfileViews("profile-123", {});
    EXPECT_EQ(views.size(), 1);
    EXPECT_EQ(views[0].viewerId, "viewer-456");
}

TEST(ViewTrackingTest, AnonymousViewHandling) {
    // Track anonymous view
    ProfileViewEvent anonymousEvent{
        .viewId = generateUUID(),
        .profileId = "profile-123",
        .viewerId = "",  // Anonymous
        .viewedAt = now()
    };

    ProfileViewTracker::trackProfileView(anonymousEvent);

    // Should not appear in "who viewed"
    auto whoViewed = ProfileViewTracker::getWhoViewedProfile("profile-123");
    EXPECT_EQ(whoViewed.recentViewers.size(), 0);
    EXPECT_EQ(whoViewed.totalAnonymousViews, 1);
}
```

### Statistics Tests
```cpp
TEST(ViewStatsTest, StatisticsCalculation) {
    // Create multiple views
    createTestViews("profile-123", 50);

    // Get statistics
    auto stats = ProfileViewTracker::getViewStatistics("profile-123", last30Days());

    // Verify calculations
    EXPECT_GE(stats.totalViews, 50);
    EXPECT_GE(stats.uniqueViewers, 1);
    EXPECT_TRUE(!stats.topLocations.empty());
    EXPECT_TRUE(stats.avgViewDuration > 0);
}

TEST(ViewStatsTest, GeographicAnalysis) {
    // Create views from different locations
    createViewFromLocation("profile-123", "Tehran", "Iran");
    createViewFromLocation("profile-123", "Tehran", "Iran");
    createViewFromLocation("profile-123", "Shiraz", "Iran");

    auto stats = ProfileViewTracker::getViewStatistics("profile-123", last30Days());

    // Tehran should be top location
    EXPECT_EQ(stats.topLocations[0].location, "Tehran, Iran");
    EXPECT_GE(stats.topLocations[0].viewCount, 2);
}
```

## 🎉 Success Criteria

### View Tracking
- ✅ **Profile views tracked accurately**
- ✅ **Geo location and device info captured**
- ✅ **Anonymous vs authenticated views handled**
- ✅ **Privacy settings respected**

### Analytics Dashboard
- ✅ **View statistics calculated correctly**
- ✅ **Geographic and device breakdowns**
- ✅ **Trend analysis and comparisons**
- ✅ **Real-time statistics updates**

### Who Viewed Feature
- ✅ **Viewer information displayed**
- ✅ **Privacy controls for viewers**
- ✅ **Connection status shown**
- ✅ **Anonymous views summarized**

### Notifications
- ✅ **Smart notification triggers**
- ✅ **Privacy-respecting notifications**
- ✅ **Notification preferences honored**
- ✅ **No spam notifications**

This creates a **comprehensive view tracking system** that provides **valuable insights** to profile owners while **respecting privacy** and avoiding **annoying notifications**.
