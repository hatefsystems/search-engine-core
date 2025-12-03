# 🚀 Content Feed - Content Creation Tools

**Duration:** 2 days
**Dependencies:** Profile database models, Profile routing CRUD
**Acceptance Criteria:**
- ✅ Rich text editor for content creation
- ✅ Media upload and management (images, videos)
- ✅ Content scheduling and publishing
- ✅ Advanced formatting and templates
- ✅ Content preview and editing
- ✅ Collaborative content creation
- ✅ Content analytics and optimization

## 🎯 Task Description

Implement sophisticated content creation tools that allow users to create rich, engaging content for their profiles with advanced editing features, media management, and publishing workflows.

## ✏️ Rich Text Editor System

### Editor Architecture
```cpp
enum class ContentType {
    TEXT_POST,          // Regular text post
    ARTICLE,            // Long-form article
    PROJECT_SHOWCASE,   // Project/portfolio piece
    EVENT_ANNOUNCEMENT, // Event promotion
    JOB_POSTING,        // Job announcement
    COMPANY_UPDATE,     // Company news/updates
    POLL,              // Interactive poll
    LINK_SHARE         // Link with description
};

struct ContentBlock {
    std::string blockId;
    ContentBlockType type;          // TEXT, IMAGE, VIDEO, QUOTE, etc.
    std::string content;
    std::map<std::string, std::string> metadata; // Formatting, styling
    int order;                      // Position in content
};

struct ContentDraft {
    std::string draftId;
    std::string profileId;
    std::string title;
    ContentType type;
    std::vector<ContentBlock> blocks;

    // Metadata
    std::vector<std::string> tags;
    std::string category;
    std::string featuredImageUrl;
    bool isPublished = false;

    // Draft management
    Date createdAt;
    Date lastModified;
    std::string lastEditedBy;
    std::vector<DraftVersion> versions;
};

class RichTextEditorService {
public:
    static ContentDraft createDraft(const std::string& profileId,
                                  ContentType type,
                                  const std::string& title = "");

    static void saveDraft(const ContentDraft& draft);

    static ContentDraft loadDraft(const std::string& draftId);

    static PublishedContent publishDraft(const std::string& draftId,
                                       const PublishOptions& options);

private:
    static void validateContentBlocks(const std::vector<ContentBlock>& blocks);
    static void sanitizeContent(ContentDraft& draft);
    static void generateContentPreview(const ContentDraft& draft);
};
```

### Block Types
```cpp
enum class ContentBlockType {
    PARAGRAPH,         // Regular text paragraph
    HEADING_1,         // H1 heading
    HEADING_2,         // H2 heading
    HEADING_3,         // H3 heading
    QUOTE,             // Blockquote
    CODE_BLOCK,        // Code with syntax highlighting
    BULLET_LIST,       // Unordered list
    NUMBERED_LIST,     // Ordered list
    IMAGE,             // Image with caption
    VIDEO,             // Embedded video
    EMBED,             // Embedded content (YouTube, etc.)
    DIVIDER,           // Visual separator
    CALL_TO_ACTION,    // CTA button/link
    POLL,              // Interactive poll
    LINK_PREVIEW       // Rich link preview
};

struct ImageBlock : ContentBlock {
    std::string imageUrl;
    std::string altText;
    std::string caption;
    ImageAlignment alignment = ImageAlignment::CENTER;
    ImageSize size = ImageSize::MEDIUM;
};

struct VideoBlock : ContentBlock {
    std::string videoUrl;
    std::string thumbnailUrl;
    VideoPlatform platform;         // YOUTUBE, VIMEO, etc.
    bool autoplay = false;
};

struct PollBlock : ContentBlock {
    std::string question;
    std::vector<PollOption> options;
    bool allowMultipleChoices = false;
    Date votingDeadline;
};
```

## 📸 Media Management System

### Media Upload & Processing
```cpp
enum class MediaType {
    IMAGE,
    VIDEO,
    DOCUMENT,
    AUDIO
};

struct MediaFile {
    std::string mediaId;
    std::string profileId;
    std::string filename;
    std::string originalFilename;
    MediaType type;
    std::string mimeType;
    long fileSize;

    // Storage
    std::string storageUrl;
    std::string thumbnailUrl;       // For videos/images
    std::string optimizedUrl;       // Compressed version

    // Metadata
    ImageMetadata imageMetadata;    // If image
    VideoMetadata videoMetadata;    // If video
    Date uploadedAt;

    // Processing status
    MediaProcessingStatus processingStatus = MediaProcessingStatus::PENDING;
    std::vector<MediaProcessingError> processingErrors;
};

struct ImageMetadata {
    int width;
    int height;
    std::string format;
    long fileSize;
    std::string colorSpace;
    double compressionRatio;
};

class MediaUploadService {
public:
    static MediaUploadResult uploadMedia(const MediaUploadRequest& request);

    static std::string getOptimizedUrl(const std::string& mediaId,
                                     ImageSize size = ImageSize::MEDIUM);

    static std::vector<MediaFile> getProfileMedia(
        const std::string& profileId,
        const MediaFilter& filter = {});

private:
    static void validateFile(const MediaUploadRequest& request);
    static MediaFile processUpload(const MediaUploadRequest& request);
    static void generateThumbnails(const MediaFile& media);
    static void optimizeImage(const MediaFile& media);
};
```

### Media Gallery
```cpp
class MediaGalleryService {
public:
    static std::vector<MediaFile> getGalleryContents(
        const std::string& profileId,
        const GalleryFilter& filter = {});

    static void organizeMediaIntoAlbums(const std::string& profileId,
                                      const std::vector<std::string>& mediaIds,
                                      const std::string& albumName);

    static std::vector<MediaAlbum> getMediaAlbums(const std::string& profileId);

private:
    static void updateMediaMetadata(const std::string& mediaId,
                                  const std::map<std::string, std::string>& metadata);
    static void deleteMediaFile(const std::string& mediaId);
};
```

## 📅 Content Scheduling & Publishing

### Publishing Workflow
```cpp
enum class PublishStatus {
    DRAFT,              // Being edited
    SCHEDULED,          // Scheduled for future publishing
    PUBLISHED,          // Live and visible
    ARCHIVED           // Archived (soft delete)
};

struct PublishOptions {
    PublishStatus status = PublishStatus::PUBLISHED;
    Date publishAt;                     // For scheduled publishing
    std::vector<std::string> targetAudiences; // Who can see this
    bool allowComments = true;
    bool allowSharing = true;
    bool isPinned = false;              // Pin to profile top
    std::string visibility;             // PUBLIC, CONNECTIONS_ONLY, etc.
};

struct PublishedContent {
    std::string contentId;
    std::string profileId;
    std::string authorId;
    std::string title;
    ContentType type;

    // Content
    std::vector<ContentBlock> blocks;
    std::string excerpt;                // Auto-generated summary
    std::string featuredImageUrl;

    // Publishing
    PublishStatus status;
    Date publishedAt;
    Date lastModified;

    // Metadata
    std::vector<std::string> tags;
    std::string category;
    int readTimeMinutes;               // Estimated reading time

    // Engagement
    int viewCount = 0;
    int likeCount = 0;
    int commentCount = 0;
    int shareCount = 0;

    // Analytics
    std::string canonicalUrl;
    std::string seoTitle;
    std::string seoDescription;
};

class ContentPublishingService {
public:
    static PublishedContent publishContent(const ContentDraft& draft,
                                         const PublishOptions& options);

    static void scheduleContent(const std::string& draftId,
                              Date publishAt,
                              const PublishOptions& options);

    static void processScheduledContent();    // Run by cron job

    static std::vector<PublishedContent> getProfileContent(
        const std::string& profileId,
        const ContentFilter& filter = {});

private:
    static void generateExcerpt(PublishedContent& content);
    static void calculateReadTime(PublishedContent& content);
    static void updateSearchIndex(const PublishedContent& content);
};
```

### Auto-Scheduling Features
```cpp
class ContentSchedulerService {
public:
    static std::vector<OptimalPublishTime> getOptimalPublishTimes(
        const std::string& profileId,
        const std::string& contentType);

    static PublishSchedule createPublishSchedule(
        const std::string& profileId,
        const ScheduleTemplate& template);

    static void optimizePublishingStrategy(const std::string& profileId);

private:
    static OptimalPublishTime analyzeAudienceBehavior(
        const std::string& profileId);

    static std::vector<ScheduleTemplate> getDefaultTemplates();
};
```

## 🎨 Advanced Formatting & Templates

### Content Templates
```cpp
struct ContentTemplate {
    std::string templateId;
    std::string name;
    std::string description;
    ContentType contentType;
    std::string category;

    // Template structure
    std::vector<ContentBlock> defaultBlocks;
    std::map<std::string, std::string> defaultMetadata;

    // Customization options
    std::vector<TemplateVariable> variables;
    std::string previewImageUrl;

    // Usage stats
    int usageCount = 0;
    double averageEngagement = 0.0;
};

struct TemplateVariable {
    std::string variableId;
    std::string label;
    std::string type;              // "text", "image", "select"
    std::string defaultValue;
    std::vector<std::string> options; // For select type
};

class ContentTemplateService {
public:
    static std::vector<ContentTemplate> getTemplatesByCategory(
        const std::string& category);

    static ContentDraft createFromTemplate(const std::string& templateId,
                                         const std::string& profileId,
                                         const std::map<std::string, std::string>& variables = {});

    static ContentTemplate createCustomTemplate(
        const ContentDraft& draft,
        const std::string& templateName);

private:
    static void applyTemplateVariables(ContentDraft& draft,
                                     const std::map<std::string, std::string>& variables);
};
```

## 👥 Collaborative Creation

### Collaboration Features
```cpp
struct ContentCollaboration {
    std::string collaborationId;
    std::string contentId;
    std::string ownerId;                   // Content owner

    // Collaborators
    std::vector<Collaborator> collaborators;
    CollaborationPermissions defaultPermissions;

    // Activity tracking
    std::vector<CollaborationActivity> activities;
};

struct Collaborator {
    std::string userId;
    std::string name;
    CollaborationPermissions permissions;
    Date invitedAt;
    bool hasAccepted = false;
};

enum class CollaborationPermission {
    VIEW,              // Can view draft
    COMMENT,           // Can add comments
    EDIT,              // Can edit content
    PUBLISH,           // Can publish content
    MANAGE             // Can manage collaborators
};

class ContentCollaborationService {
public:
    static void inviteCollaborator(const std::string& contentId,
                                 const std::string& collaboratorId,
                                 CollaborationPermissions permissions);

    static void acceptCollaborationInvite(const std::string& collaborationId,
                                        const std::string& userId);

    static std::vector<CollaborationActivity> getCollaborationActivity(
        const std::string& contentId);

private:
    static void notifyCollaborator(const Collaborator& collaborator,
                                 const std::string& contentId);
    static void trackCollaborationActivity(const std::string& contentId,
                                         const std::string& userId,
                                         const std::string& action);
};
```

## 📊 Content Analytics & Optimization

### Content Performance
```cpp
struct ContentAnalytics {
    std::string contentId;
    DateRange period;

    // Consumption metrics
    int viewCount;
    int uniqueViewers;
    double averageViewDuration;
    double bounceRate;

    // Engagement metrics
    int likeCount;
    int commentCount;
    int shareCount;
    double engagementRate;        // Engagements per view

    // Social metrics
    std::map<std::string, int> sharesByPlatform;
    double viralCoefficient;

    // Geographic data
    std::map<std::string, int> viewsByCountry;
    std::map<std::string, int> viewsByCity;

    // Device data
    std::map<std::string, int> viewsByDevice;
    std::map<std::string, int> viewsByBrowser;

    // Time-based data
    std::vector<HourlyStats> hourlyBreakdown;
    std::vector<DailyStats> dailyBreakdown;

    // Optimization suggestions
    std::vector<ContentOptimizationTip> optimizationTips;
};

struct ContentOptimizationTip {
    std::string tipType;          // "title_length", "image_alt", "publish_time"
    std::string message;
    std::string actionUrl;        // Link to fix the issue
    int potentialImpact;          // 1-10 scale
};

class ContentAnalyticsService {
public:
    static ContentAnalytics getContentAnalytics(
        const std::string& contentId,
        DateRange range = last30Days());

    static std::vector<ContentOptimizationTip> getOptimizationTips(
        const std::string& contentId);

    static ContentPerformanceReport generatePerformanceReport(
        const std::string& profileId,
        DateRange range);

private:
    static void calculateEngagementMetrics(ContentAnalytics& analytics);
    static void analyzeAudienceDemographics(ContentAnalytics& analytics);
    static std::vector<ContentOptimizationTip> generateOptimizationTips(
        const ContentAnalytics& analytics);
};
```

## 💡 A/B Testing Framework

### Content Testing
```cpp
struct ContentVariant {
    std::string variantId;
    std::string contentId;
    std::string variantName;      // "Version A", "Version B"
    std::string title;
    std::vector<ContentBlock> blocks;

    // Test configuration
    double trafficAllocation;     // % of traffic to this variant
    std::vector<std::string> targetAudience; // Audience segments

    // Results
    int viewCount = 0;
    int conversionCount = 0;      // Goal completions
    double conversionRate = 0.0;
};

struct ContentExperiment {
    std::string experimentId;
    std::string contentId;
    std::string experimentName;
    std::string goal;             // "maximize_engagement", "maximize_shares"
    std::vector<ContentVariant> variants;

    // Experiment settings
    Date startDate;
    Date endDate;
    int minSampleSize = 1000;    // Minimum views before declaring winner
    std::string winnerDetermination; // "automatic", "manual"

    // Results
    std::string winnerVariantId;
    Date concludedAt;
    ExperimentReport report;
};

class ContentABTestingService {
public:
    static ContentExperiment createExperiment(
        const std::string& contentId,
        const std::vector<ContentVariant>& variants,
        const ExperimentConfig& config);

    static ContentVariant getVariantForUser(
        const std::string& experimentId,
        const std::string& userId);

    static void trackConversion(const std::string& experimentId,
                              const std::string& variantId,
                              const std::string& userId);

    static ExperimentReport concludeExperiment(const std::string& experimentId);

private:
    static ContentVariant selectVariant(const ContentExperiment& experiment,
                                      const std::string& userId);
    static std::string determineExperimentWinner(const ContentExperiment& experiment);
};
```

## 📋 Implementation Plan

### Day 1: Editor + Media Management
- Implement rich text editor with block system
- Create media upload and processing pipeline
- Add content draft management
- Build basic template system

### Day 1 Continued: Publishing + Analytics
- Implement content scheduling and publishing
- Add collaborative editing features
- Create content analytics dashboard
- Build A/B testing framework

## 🧪 Testing Strategy

### Editor Tests
```cpp
TEST(EditorTest, CreateAndEditContent) {
    auto draft = RichTextEditorService::createDraft("profile-123", ContentType::ARTICLE, "My Article");

    // Add content blocks
    ContentBlock paragraph{
        .type = ContentBlockType::PARAGRAPH,
        .content = "This is a test article about content creation."
    };

    draft.blocks.push_back(paragraph);
    RichTextEditorService::saveDraft(draft);

    // Verify saved
    auto loadedDraft = RichTextEditorService::loadDraft(draft.draftId);
    EXPECT_EQ(loadedDraft.blocks.size(), 1);
    EXPECT_EQ(loadedDraft.blocks[0].content, paragraph.content);
}
```

### Publishing Tests
```cpp
TEST(PublishingTest, ScheduleAndPublishContent) {
    // Create draft
    auto draft = createTestDraft();

    // Schedule for publishing
    Date publishAt = now() + 1_day;
    ContentPublishingService::scheduleContent(draft.draftId, publishAt, {});

    // Simulate scheduler running
    ContentPublishingService::processScheduledContent();

    // Content should be published
    auto publishedContent = ContentPublishingService::getContentById(draft.draftId);
    EXPECT_EQ(publishedContent.status, PublishStatus::PUBLISHED);
    EXPECT_EQ(publishedContent.publishedAt, publishAt);
}
```

### Media Tests
```cpp
TEST(MediaTest, UploadAndProcessImage) {
    MediaUploadRequest request{
        .profileId = "profile-123",
        .filename = "test-image.jpg",
        .fileData = loadTestImageData(),
        .type = MediaType::IMAGE
    };

    auto result = MediaUploadService::uploadMedia(request);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.mediaId.empty());

    // Verify processing
    auto media = MediaGalleryService::getMediaById(result.mediaId);
    EXPECT_EQ(media.processingStatus, MediaProcessingStatus::COMPLETED);
    EXPECT_FALSE(media.thumbnailUrl.empty());
}
```

## 🎉 Success Criteria

### Content Creation
- ✅ **Rich text editor with advanced formatting**
- ✅ **Media upload and management system**
- ✅ **Content block system for flexible layouts**
- ✅ **Template system for quick content creation**

### Publishing & Scheduling
- ✅ **Content scheduling and automated publishing**
- ✅ **Content preview and editing workflows**
- ✅ **SEO optimization and metadata management**
- ✅ **Content versioning and draft management**

### Collaboration & Analytics
- ✅ **Collaborative content creation tools**
- ✅ **Content performance analytics**
- ✅ **A/B testing framework for optimization**
- ✅ **Content optimization suggestions**

### User Experience
- ✅ **Intuitive content creation interface**
- ✅ **Real-time collaboration features**
- ✅ **Mobile-responsive editing tools**
- ✅ **Content discovery and inspiration features**

This creates a **professional content creation platform** that empowers users to **create engaging, rich content** with **advanced tools and analytics** for optimal reach and engagement.
