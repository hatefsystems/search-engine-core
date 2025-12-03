# 🚀 Viral Sharing - Basic Sharing & QR Codes

**Duration:** 1 day
**Dependencies:** Profile database models
**Acceptance Criteria:**
- ✅ QR code generation for profile sharing
- ✅ Basic social media sharing buttons
- ✅ Shareable profile links with tracking
- ✅ Cross-platform sharing compatibility
- ✅ Share analytics and metrics
- ✅ Custom profile landing pages

## 🎯 Task Description

Implement fundamental sharing capabilities that make it easy for users to share profiles across different platforms with built-in tracking and analytics.

## 📱 QR Code System

### QR Code Generation
```cpp
enum class QRCodeStyle {
    DEFAULT,        // Standard black/white
    CUSTOM,         // Custom colors
    BRANDED,        // With profile branding
    ANIMATED        // Animated QR (for premium)
};

struct QRCodeConfig {
    std::string profileId;
    std::string profileSlug;
    QRCodeStyle style = QRCodeStyle::DEFAULT;
    
    // Visual customization
    std::string foregroundColor = "#000000";
    std::string backgroundColor = "#FFFFFF";
    std::string logoUrl;              // Profile logo in center
    int size = 256;                   // QR code size in pixels
    
    // Error correction
    QRErrorCorrectionLevel errorCorrection = QRErrorCorrectionLevel::MEDIUM;
    
    // Tracking
    std::string campaignId;           // For analytics
    std::string sourceTag;            // Track share source
};

struct GeneratedQRCode {
    std::string qrCodeId;
    std::string profileId;
    std::string qrCodeUrl;            // Direct link to QR image
    std::string profileUrl;           // Profile URL encoded in QR
    QRCodeConfig config;
    
    // Usage tracking
    Date generatedAt;
    int scanCount = 0;
    std::vector<QRCodeScan> scans;
    
    // Expiry (optional)
    Date expiresAt;
    bool isActive = true;
};

class QRCodeService {
public:
    static GeneratedQRCode generateQRCode(const QRCodeConfig& config);
    
    static std::string getQRCodeImageUrl(const std::string& qrCodeId);
    
    static void trackQRScan(const std::string& qrCodeId, 
                          const QRScanData& scanData);
    
    static QRCodeAnalytics getQRAnalytics(const std::string& profileId,
                                        DateRange range = {});
    
private:
    static std::string encodeProfileUrl(const std::string& profileSlug);
    static std::string generateQRCodeImage(const QRCodeConfig& config);
    static void addTrackingParameters(std::string& url, const QRCodeConfig& config);
};
```

### QR Code Analytics
```cpp
struct QRScanData {
    std::string scanId;
    std::string qrCodeId;
    Date scannedAt;
    
    // Device info
    std::string userAgent;
    std::string ipAddress;
    GeoLocation location;
    
    // Conversion tracking
    bool convertedToProfileView = false;
    bool convertedToConnectionRequest = false;
    bool convertedToMessage = false;
    Date conversionAt;
    
    // Source tracking
    std::string referrer;             // App/website where scanned
    std::string campaignId;
};

struct QRCodeAnalytics {
    std::string profileId;
    DateRange period;
    
    int totalScans;
    int uniqueScanners;
    std::map<std::string, int> scansByCountry;
    std::map<std::string, int> scansByDevice;
    
    // Conversion metrics
    int profileViewsFromScans;
    int connectionRequestsFromScans;
    int messagesFromScans;
    
    double scanToViewConversionRate;
    double scanToConnectionConversionRate;
    
    // Popular QR codes
    std::vector<QRCodePerformance> topPerformingQRCodes;
    
    // Trends
    std::vector<DailyScanData> dailyScanTrends;
};

struct QRCodePerformance {
    std::string qrCodeId;
    std::string qrCodeUrl;
    int scanCount;
    int conversionCount;
    double conversionRate;
    Date lastScanned;
};
```

## 🔗 Shareable Profile Links

### Link Generation & Tracking
```cpp
enum class SharePlatform {
    WHATSAPP,
    TELEGRAM,
    LINKEDIN,
    FACEBOOK,
    TWITTER,
    INSTAGRAM,
    EMAIL,
    SMS,
    DIRECT_LINK,
    CUSTOM
};

struct ShareableLink {
    std::string linkId;
    std::string profileId;
    std::string originalUrl;          // Base profile URL
    std::string shareUrl;             // URL with tracking parameters
    SharePlatform platform;
    
    // Tracking parameters
    std::string campaignId;
    std::string sourceTag;
    std::string mediumTag;
    
    // Customizations
    std::string customMessage;        // Platform-specific message
    std::string utmParameters;        // UTM tracking
    
    // Usage tracking
    Date createdAt;
    int clickCount = 0;
    int shareCount = 0;              // How many times shared further
    std::vector<LinkClick> clicks;
    
    // Expiry
    Date expiresAt;
    bool isActive = true;
};

class ShareableLinkService {
public:
    static ShareableLink generateShareableLink(
        const std::string& profileId,
        SharePlatform platform,
        const ShareCustomization& custom = {});
    
    static void trackLinkClick(const std::string& linkId,
                             const LinkClickData& clickData);
    
    static ShareAnalytics getShareAnalytics(const std::string& profileId,
                                          DateRange range = {});
    
private:
    static std::string buildTrackingUrl(const std::string& baseUrl,
                                      const ShareableLink& link);
    static std::string generateUtmParameters(const ShareableLink& link);
    static std::string customizeMessageForPlatform(const std::string& message,
                                                 SharePlatform platform);
};
```

## 📊 Social Media Integration

### Platform-Specific Sharing
```cpp
struct PlatformShareConfig {
    SharePlatform platform;
    std::string platformName;
    std::string iconUrl;
    std::string baseShareUrl;
    
    // Platform limitations
    int maxMessageLength;
    bool supportsImages = true;
    bool supportsLinks = true;
    std::vector<std::string> supportedImageFormats;
    
    // Default messages
    std::string defaultMessage;
    std::string longMessage;          // For platforms with longer limits
};

class SocialShareService {
public:
    static std::string generateShareUrl(
        const std::string& profileId,
        SharePlatform platform,
        const std::string& customMessage = "");
    
    static std::vector<PlatformShareConfig> getAvailablePlatforms();
    
    static SharePreview generateSharePreview(
        const std::string& profileId,
        SharePlatform platform);
    
    static bool validateShareMessage(
        const std::string& message,
        SharePlatform platform);
    
private:
    static std::string buildPlatformSpecificUrl(
        SharePlatform platform,
        const std::string& profileUrl,
        const std::string& message);
        
    static std::string optimizeMessageForPlatform(
        const std::string& message,
        SharePlatform platform);
        
    static SharePreview createSharePreview(
        const Profile& profile,
        SharePlatform platform,
        const std::string& message);
};
```

### Share Preview System
```cpp
struct SharePreview {
    SharePlatform platform;
    
    // Visual preview
    std::string title;
    std::string description;
    std::string imageUrl;
    std::string profileUrl;
    
    // Platform-specific formatting
    std::string formattedMessage;
    std::string truncatedMessage;     // If message is too long
    bool messageTruncated = false;
    
    // Compatibility warnings
    std::vector<std::string> warnings; // "Message too long", "Image not supported"
    std::vector<std::string> suggestions; // How to fix issues
    
    // Estimated reach
    int estimatedReach;              // Based on platform and profile metrics
    std::string reachExplanation;
};

class SharePreviewService {
public:
    static SharePreview generatePreview(
        const std::string& profileId,
        SharePlatform platform,
        const std::string& customMessage = "");
    
    static std::vector<SharePreview> generateAllPreviews(
        const std::string& profileId,
        const std::string& customMessage = "");
    
private:
    static void optimizePreviewForPlatform(SharePreview& preview);
    static int estimatePlatformReach(const std::string& profileId,
                                   SharePlatform platform);
};
```

## 📈 Share Analytics & Tracking

### Comprehensive Analytics
```cpp
struct ShareAnalytics {
    std::string profileId;
    DateRange period;
    
    // Overall sharing metrics
    int totalShares;
    int uniqueSharers;
    int totalClicks;
    int totalViewsFromShares;
    
    // Platform breakdown
    std::map<SharePlatform, PlatformShareStats> platformStats;
    
    // Link performance
    std::vector<LinkPerformance> topPerformingLinks;
    double averageClickThroughRate;
    
    // QR code performance
    std::vector<QRCodePerformance> topPerformingQRCodes;
    int totalQRScans;
    double qrToViewConversionRate;
    
    // Geographic insights
    std::map<std::string, int> sharesByCountry;
    std::map<std::string, int> clicksByCountry;
    
    // Timing insights
    std::map<std::string, int> sharesByHour;
    std::map<std::string, int> sharesByDay;
    
    // Viral potential
    double averageSharesPerView;     // How viral the content is
    int multiShareInstances;         // Links shared multiple times
};

struct PlatformShareStats {
    SharePlatform platform;
    int shareCount;
    int clickCount;
    int viewCount;
    double clickThroughRate;
    double viewThroughRate;
    double engagementRate;
};

struct LinkPerformance {
    std::string linkId;
    SharePlatform platform;
    std::string shareUrl;
    int clickCount;
    int viewCount;
    double conversionRate;
    Date createdAt;
};

class ShareAnalyticsService {
public:
    static ShareAnalytics getShareAnalytics(
        const std::string& profileId,
        DateRange range = last30Days());
    
    static PlatformShareStats getPlatformPerformance(
        const std::string& profileId,
        SharePlatform platform,
        DateRange range = {});
    
    static std::vector<ShareOptimizationTip> getOptimizationTips(
        const std::string& profileId);
    
private:
    static void calculatePlatformMetrics(ShareAnalytics& analytics);
    static void analyzeGeographicDistribution(ShareAnalytics& analytics);
    static std::vector<ShareOptimizationTip> generateTips(
        const ShareAnalytics& analytics);
};
```

## 🎨 Custom Profile Landing Pages

### Landing Page Customization
```cpp
struct CustomLandingPage {
    std::string pageId;
    std::string profileId;
    std::string shareCampaignId;     // Associated sharing campaign
    
    // Page content
    std::string headline;
    std::string subheadline;
    std::string description;
    std::string callToAction;        // "Connect", "View Profile", "Send Message"
    
    // Visual customization
    std::string backgroundColor;
    std::string textColor;
    std::string accentColor;
    std::string backgroundImageUrl;
    std::string profileImageUrl;
    
    // Layout options
    LandingPageLayout layout = LandingPageLayout::STANDARD;
    bool showSocialProof = true;
    bool showSkills = true;
    bool showRecentContent = false;
    
    // Tracking
    Date createdAt;
    int viewCount = 0;
    int conversionCount = 0;        // Actions taken (connects, messages)
    double conversionRate = 0.0;
    
    // Expiry
    Date expiresAt;
    bool isActive = true;
};

class CustomLandingPageService {
public:
    static CustomLandingPage createLandingPage(
        const std::string& profileId,
        const LandingPageConfig& config);
    
    static std::string getLandingPageUrl(const std::string& pageId);
    
    static void trackLandingPageView(const std::string& pageId,
                                   const LandingPageViewData& viewData);
    
    static LandingPageAnalytics getLandingPageAnalytics(
        const std::string& pageId);
    
private:
    static std::string generateLandingPageSlug();
    static void renderLandingPage(const CustomLandingPage& page);
    static std::string buildTrackingUrl(const std::string& baseUrl,
                                      const CustomLandingPage& page);
};
```

## 📋 Implementation Plan

### Day 1: QR Codes + Basic Sharing
- Implement QR code generation and tracking
- Create shareable link system with analytics
- Add basic social media sharing buttons
- Build share preview system

### Day 1 Continued: Custom Pages + Analytics
- Implement custom landing pages
- Create comprehensive share analytics
- Add cross-platform compatibility
- Test end-to-end sharing flow

## 🧪 Testing Strategy

### QR Code Tests
```cpp
TEST(QRCodeTest, GenerateAndTrackQRCode) {
    QRCodeConfig config{
        .profileId = "profile-123",
        .profileSlug = "john-doe",
        .style = QRCodeStyle::DEFAULT,
        .size = 256
    };
    
    auto qrCode = QRCodeService::generateQRCode(config);
    
    EXPECT_FALSE(qrCode.qrCodeId.empty());
    EXPECT_FALSE(qrCode.qrCodeUrl.empty());
    EXPECT_TRUE(qrCode.profileUrl.find("john-doe") != std::string::npos);
    
    // Test scanning
    QRScanData scanData{.scannedAt = now()};
    QRCodeService::trackQRScan(qrCode.qrCodeId, scanData);
    
    auto analytics = QRCodeService::getQRAnalytics("profile-123");
    EXPECT_EQ(analytics.totalScans, 1);
}
```

### Sharing Tests
```cpp
TEST(SharingTest, GenerateShareableLink) {
    auto link = ShareableLinkService::generateShareableLink(
        "profile-123", SharePlatform::WHATSAPP);
    
    EXPECT_FALSE(link.linkId.empty());
    EXPECT_FALSE(link.shareUrl.empty());
    EXPECT_EQ(link.platform, SharePlatform::WHATSAPP);
    
    // Verify tracking parameters
    EXPECT_TRUE(link.shareUrl.find("utm_source") != std::string::npos);
}
```

### Analytics Tests
```cpp
TEST(ShareAnalyticsTest, TrackSharePerformance) {
    // Simulate shares and clicks
    createTestShares("profile-123", 10);
    createTestClicks("profile-123", 5);
    
    auto analytics = ShareAnalyticsService::getShareAnalytics("profile-123");
    
    EXPECT_EQ(analytics.totalShares, 10);
    EXPECT_EQ(analytics.totalClicks, 5);
    EXPECT_GE(analytics.averageClickThroughRate, 0.0);
    
    // Verify platform breakdown
    EXPECT_FALSE(analytics.platformStats.empty());
}
```

## 🎉 Success Criteria

### QR Code System
- ✅ **QR code generation with customization**
- ✅ **Scan tracking and conversion analytics**
- ✅ **Cross-device compatibility**
- ✅ **Branded QR code options**

### Sharing Infrastructure
- ✅ **Shareable links with tracking parameters**
- ✅ **Social media platform integration**
- ✅ **Share preview and optimization**
- ✅ **Custom landing pages**

### Analytics & Insights
- ✅ **Comprehensive share analytics**
- ✅ **Platform performance comparison**
- ✅ **Geographic and demographic insights**
- ✅ **Conversion tracking and attribution**

### User Experience
- ✅ **One-click sharing across platforms**
- ✅ **Mobile-optimized sharing options**
- ✅ **Share success feedback**
- ✅ **Privacy controls for shared content**

This creates a **comprehensive sharing infrastructure** that makes it **effortless for users to share profiles** while providing **detailed analytics** to understand sharing effectiveness.
