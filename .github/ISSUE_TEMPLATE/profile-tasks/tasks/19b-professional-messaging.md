# 🚀 Professional Networking - Messaging System

**Duration:** 2 days
**Dependencies:** 19a-connections-basic.md (connection system)
**Acceptance Criteria:**
- ✅ In-app messaging between connections
- ✅ Message threading and conversation management
- ✅ Message delivery status and read receipts
- ✅ Message encryption and privacy
- ✅ Professional messaging templates
- ✅ Message notifications and alerts
- ✅ Message search and archiving
- ✅ Anti-spam and moderation features

## 🎯 Task Description

Implement a professional messaging system that allows connections to communicate privately with secure, business-appropriate messaging features including templates, search, and moderation to facilitate professional networking and collaboration.

## 💬 Message System Architecture

### Message Data Model
```cpp
enum class MessageType {
    TEXT,                   // Regular text message
    IMAGE,                  // Image attachment
    FILE,                   // Document/file attachment
    LINK,                   // Shared link with preview
    TEMPLATE,               // Pre-written template message
    SYSTEM                  // System-generated messages
};

enum class MessageStatus {
    SENT,                   // Message sent
    DELIVERED,              // Message delivered to recipient
    READ,                   // Message read by recipient
    FAILED                  // Delivery failed
};

enum class ConversationType {
    DIRECT,                 // 1-on-1 conversation
    GROUP,                  // Group conversation (future)
    SYSTEM                  // System notifications
};

struct Message {
    std::string messageId;
    std::string conversationId;
    std::string senderId;
    std::string recipientId;        // For direct messages

    // Content
    MessageType type;
    std::string content;            // Text content or metadata
    std::string encryptedContent;   // Encrypted version for privacy
    std::vector<MessageAttachment> attachments;

    // Metadata
    Date sentAt;
    Date deliveredAt;
    Date readAt;
    MessageStatus status;

    // Threading (for replies)
    std::string parentMessageId;    // Reply to this message
    std::vector<std::string> replyIds; // Messages replying to this

    // Professional context
    std::string messageCategory;    // "introduction", "collaboration", "question"
    std::vector<std::string> tags;  // "urgent", "follow-up", "meeting"

    // Moderation
    bool isModerated = false;
    std::string moderationReason;
    Date moderatedAt;
};

struct MessageAttachment {
    std::string attachmentId;
    std::string filename;
    std::string mimeType;
    long fileSize;
    std::string storageUrl;         // Encrypted storage location
    std::string checksum;           // For integrity verification
};

struct Conversation {
    std::string conversationId;
    ConversationType type;
    std::vector<std::string> participantIds;

    // Conversation metadata
    std::string title;              // For group conversations
    std::string description;
    Date createdAt;
    Date lastMessageAt;
    std::string lastMessagePreview;

    // Professional context
    std::string context;            // "project_collaboration", "job_discussion", etc.
    std::vector<std::string> tags;

    // Settings
    bool isArchived = false;
    std::map<std::string, NotificationSettings> participantSettings;

    // Analytics
    int messageCount = 0;
    int unreadCount;               // Per participant
    std::map<std::string, Date> lastReadByParticipant;
};
```

### Messaging Service
```cpp
class MessagingService {
public:
    static Message sendMessage(const MessageRequest& request);
    static Conversation createConversation(const std::vector<std::string>& participantIds,
                                         ConversationType type = ConversationType::DIRECT);

    static std::vector<Message> getConversationMessages(
        const std::string& conversationId,
        const MessageFilter& filter = {},
        int limit = 50);

    static std::vector<Conversation> getUserConversations(
        const std::string& userId,
        const ConversationFilter& filter = {});

    static void markMessagesAsRead(const std::string& conversationId,
                                 const std::string& userId,
                                 const std::vector<std::string>& messageIds);

    static MessageSearchResult searchMessages(
        const std::string& userId,
        const std::string& query,
        const MessageFilter& filter = {});

private:
    static std::string generateConversationId(const std::vector<std::string>& participantIds);
    static void encryptMessageContent(Message& message);
    static void deliverMessage(const Message& message);
    static void notifyMessageRecipients(const Message& message);
};
```

## 🔐 Message Encryption & Privacy

### End-to-End Encryption
```cpp
class MessageEncryptionService {
public:
    static std::string encryptMessage(const std::string& content,
                                    const std::string& conversationId);

    static std::string decryptMessage(const std::string& encryptedContent,
                                    const std::string& conversationId);

    static std::string generateConversationKey(const std::string& conversationId);

    static void rotateConversationKey(const std::string& conversationId);

private:
    static std::string deriveKeyFromConversation(const std::string& conversationId);
    static const std::string ENCRYPTION_ALGORITHM = "AES-256-GCM";
};

// Usage in message sending
Message encryptedMessage = request.toMessage();
encryptedMessage.encryptedContent = MessageEncryptionService::encryptMessage(
    request.content, request.conversationId);
encryptedMessage.content = "";  // Clear plain text from database
```

### Message Privacy Controls
```cpp
struct MessagePrivacySettings {
    // Encryption settings
    bool enableEncryption = true;      // End-to-end encryption
    bool selfDestructMessages = false; // Auto-delete after time
    int selfDestructHours = 24;        // Hours until auto-deletion

    // Visibility settings
    bool allowMessageSearch = true;    // Allow searching message history
    bool showReadReceipts = true;      // Show when messages are read
    bool showTypingIndicators = true;  // Show typing status

    // Moderation settings
    bool enableSpamFiltering = true;   // Filter spam messages
    bool allowFileAttachments = true;  // Allow file sharing
    std::vector<std::string> blockedFileTypes; // "exe", "bat", etc.

    // Notification preferences
    bool desktopNotifications = true;
    bool emailNotifications = false;   // For important messages only
    bool soundNotifications = true;
    std::vector<std::string> quietHours; // "22:00-08:00"
};

class MessagePrivacyService {
public:
    static bool canSendMessage(const std::string& senderId,
                             const std::string& recipientId);

    static std::string filterMessageContent(const std::string& content,
                                          const MessagePrivacySettings& settings);

    static void applyPrivacySettings(Message& message,
                                   const MessagePrivacySettings& settings);

    static void cleanupExpiredMessages();
};
```

## 📝 Professional Message Templates

### Template System
```cpp
enum class MessageTemplateCategory {
    INTRODUCTION,           // First contact messages
    COLLABORATION,          // Project collaboration
    NETWORKING,             // Professional networking
    JOB_INQUIRY,            // Job-related messages
    FOLLOW_UP,              // Follow-up messages
    GRATITUDE,              // Thank you messages
    APOLOGY,                // Apology messages
};

struct MessageTemplate {
    std::string templateId;
    std::string name;
    MessageTemplateCategory category;
    std::string subject;             // For email notifications
    std::string content;             // Template with placeholders
    std::vector<std::string> placeholders; // Variables to fill

    // Usage tracking
    int usageCount = 0;
    double successRate = 0.0;        // % that get responses
    Date createdAt;
    bool isActive = true;
};

class MessageTemplateService {
public:
    static std::vector<MessageTemplate> getTemplatesByCategory(
        MessageTemplateCategory category);

    static std::string renderTemplate(const MessageTemplate& tmpl,
                                    const std::map<std::string, std::string>& variables);

    static std::vector<MessageTemplate> getRecommendedTemplates(
        const std::string& context,    // "first_contact", "follow_up"
        const std::string& recipientIndustry);

    static void trackTemplateUsage(const std::string& templateId,
                                 bool gotResponse);

private:
    static std::vector<MessageTemplate> getDefaultTemplates();
    static std::string replacePlaceholders(const std::string& content,
                                         const std::map<std::string, std::string>& variables);
};
```

### Template Examples
```cpp
// Introduction template
const MessageTemplate INTRODUCTION_TEMPLATE = {
    .templateId = "intro_professional",
    .name = "Professional Introduction",
    .category = MessageTemplateCategory::INTRODUCTION,
    .content = "Hi {{recipient_name}},\n\n"
               "I came across your profile and was impressed by your work in {{recipient_field}}. "
               "I'm currently working as {{sender_title}} at {{sender_company}}, "
               "and I'm interested in {{connection_reason}}.\n\n"
               "Would you be open to connecting and potentially discussing {{discussion_topic}}?\n\n"
               "Best regards,\n{{sender_name}}",
    .placeholders = {"recipient_name", "recipient_field", "sender_title",
                    "sender_company", "connection_reason", "discussion_topic", "sender_name"}
};

// Follow-up template
const MessageTemplate FOLLOW_UP_TEMPLATE = {
    .templateId = "follow_up_meeting",
    .name = "Meeting Follow-up",
    .category = MessageTemplateCategory::FOLLOW_UP,
    .content = "Hi {{recipient_name}},\n\n"
               "Thank you for taking the time to meet with me earlier. "
               "I enjoyed our discussion about {{discussion_topic}} and "
               "learning more about {{recipient_expertise}}.\n\n"
               "As we discussed, I'll {{next_steps}}. "
               "Please let me know if you need any additional information.\n\n"
               "Best,\n{{sender_name}}",
    .placeholders = {"recipient_name", "discussion_topic", "recipient_expertise",
                    "next_steps", "sender_name"}
};
```

## 🔔 Message Notifications

### Notification System
```cpp
enum class MessageNotificationType {
    NEW_MESSAGE,
    MESSAGE_READ,
    TYPING_INDICATOR,
    MESSAGE_DELIVERED,
    CONVERSATION_ARCHIVED,
    MESSAGE_REACTION
};

struct MessageNotification {
    std::string notificationId;
    std::string userId;
    MessageNotificationType type;
    std::string conversationId;
    std::string messageId;
    std::string senderId;
    std::string messagePreview;
    Date createdAt;
    bool isRead = false;

    // Context
    std::string senderName;
    std::string conversationTitle;
    int unreadCount;
    bool isUrgent = false;
};

class MessageNotificationService {
public:
    static void notifyNewMessage(const Message& message);
    static void notifyMessageRead(const std::string& conversationId,
                                const std::string& readerId);
    static void notifyTypingStatus(const std::string& conversationId,
                                 const std::string& typerId,
                                 bool isTyping);

    static void batchNotifications(const std::string& userId);

private:
    static bool shouldNotifyUser(const std::string& userId,
                               MessageNotificationType type,
                               const std::string& conversationId);

    static std::string generateNotificationMessage(
        MessageNotificationType type,
        const std::string& senderName,
        const std::string& preview);
};
```

## 🔍 Message Search & Organization

### Advanced Search
```cpp
struct MessageSearchFilter {
    std::string query;                   // Search text
    DateRange dateRange;
    std::vector<std::string> conversationIds;
    std::vector<std::string> senderIds;
    MessageType messageType;
    std::vector<std::string> tags;
    bool includeArchived = false;
    int maxResults = 100;

    // Advanced filters
    bool hasAttachments = false;
    bool isReply = false;
    bool isUnread = false;
};

struct MessageSearchResult {
    std::vector<MessageSearchHit> hits;
    int totalResults;
    std::map<std::string, int> resultsByConversation;
    SearchMetadata metadata;
};

struct MessageSearchHit {
    std::string messageId;
    std::string conversationId;
    std::string senderId;
    std::string senderName;
    std::string contentSnippet;          // Highlighted search result
    Date sentAt;
    std::vector<std::string> highlights; // Highlighted search terms
    double relevanceScore;
};

class MessageSearchService {
public:
    static MessageSearchResult searchMessages(
        const std::string& userId,
        const MessageSearchFilter& filter);

    static std::vector<std::string> getMessageSuggestions(
        const std::string& userId,
        const std::string& partialQuery);

private:
    static void indexMessage(const Message& message);
    static std::vector<std::string> extractSearchTerms(const Message& message);
    static std::string generateContentSnippet(const Message& message,
                                            const std::string& query);
};
```

### Conversation Organization
```cpp
struct ConversationFolder {
    std::string folderId;
    std::string userId;
    std::string name;                    // "Work", "Clients", "Projects"
    std::string color;                   // UI color coding
    std::vector<std::string> conversationIds;
    bool isSystemFolder = false;         // "Inbox", "Archived", "Spam"
};

class ConversationOrganizationService {
public:
    static void createFolder(const std::string& userId,
                           const std::string& name,
                           const std::string& color = "");

    static void addConversationToFolder(const std::string& conversationId,
                                      const std::string& folderId);

    static void archiveConversation(const std::string& conversationId,
                                  const std::string& userId);

    static std::vector<ConversationFolder> getUserFolders(const std::string& userId);

    static std::map<std::string, std::vector<Conversation>> getConversationsByFolder(
        const std::string& userId);

private:
    static std::vector<ConversationFolder> getDefaultFolders(const std::string& userId);
};
```

## 🛡️ Anti-Spam & Moderation

### Spam Detection
```cpp
class MessageSpamDetectionService {
public:
    static SpamCheckResult checkMessageForSpam(const Message& message);

    static bool isSpamPattern(const std::string& content);
    static bool hasExcessiveLinks(const std::string& content);
    static bool isRepetitiveMessage(const Message& message);
    static bool violatesRateLimits(const std::string& senderId);

    static void reportSpamMessage(const std::string& messageId,
                                const std::string& reporterId,
                                const std::string& reason);

private:
    static double calculateSpamScore(const Message& message);
    static std::vector<std::string> getSpamPatterns();
    static int getMessagesInTimeWindow(const std::string& senderId, int minutes);
};

struct SpamCheckResult {
    bool isSpam = false;
    double spamScore;               // 0.0 - 1.0
    std::string riskLevel;          // "low", "medium", "high"
    std::vector<std::string> flags; // Reasons for spam score
    std::string recommendedAction;  // "allow", "flag", "block"
};
```

### Content Moderation
```cpp
class MessageModerationService {
public:
    static ModerationResult moderateMessage(const Message& message);

    static std::vector<Message> getMessagesNeedingModeration(int limit = 50);

    static void approveMessage(const std::string& messageId,
                             const std::string& moderatorId);

    static void rejectMessage(const std::string& messageId,
                            const std::string& moderatorId,
                            const std::string& reason);

private:
    static bool containsInappropriateContent(const std::string& content);
    static bool violatesCommunityGuidelines(const Message& message);
    static void notifySenderOfModeration(const Message& message,
                                       ModerationResult result);
};
```

## 💬 Messaging UI Components

### Message Interface
```html
<div class="messaging-interface">
    <!-- Conversation List Sidebar -->
    <div class="conversations-sidebar">
        <div class="sidebar-header">
            <h3>Messages</h3>
            <button class="new-message-btn">New Message</button>
        </div>

        <div class="conversation-search">
            <input type="text" placeholder="Search conversations..." id="conversation-search">
        </div>

        <div class="conversations-list">
            <div class="conversation-item active" data-conversation-id="conv-123">
                <img src="/avatars/john.jpg" class="conversation-avatar">
                <div class="conversation-info">
                    <div class="conversation-name">John Smith</div>
                    <div class="conversation-preview">Thanks for the introduction! Looking forward...</div>
                    <div class="conversation-time">2 min ago</div>
                </div>
                <div class="conversation-unread">3</div>
            </div>

            <div class="conversation-item" data-conversation-id="conv-456">
                <img src="/avatars/sarah.jpg" class="conversation-avatar">
                <div class="conversation-info">
                    <div class="conversation-name">Sarah Johnson</div>
                    <div class="conversation-preview">The project proposal looks great. When...</div>
                    <div class="conversation-time">1 hour ago</div>
                </div>
            </div>
        </div>
    </div>

    <!-- Message Thread -->
    <div class="message-thread">
        <div class="thread-header">
            <div class="thread-participants">
                <img src="/avatars/john.jpg" class="participant-avatar">
                <div class="participant-info">
                    <div class="participant-name">John Smith</div>
                    <div class="participant-status">Online</div>
                </div>
            </div>

            <div class="thread-actions">
                <button class="template-btn">Templates</button>
                <button class="attach-btn">Attach</button>
                <button class="more-btn">⋯</button>
            </div>
        </div>

        <div class="messages-container">
            <div class="message-group">
                <div class="message-date">Today</div>

                <div class="message received">
                    <img src="/avatars/john.jpg" class="message-avatar">
                    <div class="message-content">
                        <div class="message-text">Hi! Thanks for connecting. I saw you're working on AI projects.</div>
                        <div class="message-time">10:30 AM</div>
                    </div>
                </div>

                <div class="message sent">
                    <div class="message-content">
                        <div class="message-text">Hi John! Yes, we're doing some interesting work with NLP. Would love to hear about your experience.</div>
                        <div class="message-time">10:32 AM</div>
                        <div class="message-status">✓✓ Read</div>
                    </div>
                </div>
            </div>
        </div>

        <!-- Message Composer -->
        <div class="message-composer">
            <div class="composer-toolbar">
                <button class="template-selector">📝</button>
                <button class="attachment-btn">📎</button>
                <button class="emoji-btn">😊</button>
            </div>

            <div class="composer-input">
                <textarea placeholder="Type your message..." id="message-input" rows="1"></textarea>
                <button class="send-btn" disabled>Send</button>
            </div>

            <!-- Template Selector -->
            <div class="template-dropdown hidden">
                <div class="template-category">
                    <h4>Introduction</h4>
                    <div class="template-item" data-template="intro_professional">
                        Professional Introduction
                    </div>
                </div>
                <div class="template-category">
                    <h4>Follow-up</h4>
                    <div class="template-item" data-template="follow_up_meeting">
                        Meeting Follow-up
                    </div>
                </div>
            </div>
        </div>
    </div>
</div>
```

## 📋 Implementation Plan

### Day 1: Core Messaging + Encryption
- Implement message sending and receiving system
- Add message encryption and privacy controls
- Create conversation management
- Build basic message threading

### Day 2: Templates + Search + Moderation
- Implement professional message templates
- Add message search and organization features
- Create spam detection and content moderation
- Build notification system and testing

## 🧪 Testing Strategy

### Message Delivery Tests
```cpp
TEST(MessagingTest, SendAndReceiveMessage) {
    // Create conversation
    auto conversation = MessagingService::createConversation({"user-a", "user-b"});

    // Send message
    MessageRequest request{
        .conversationId = conversation.conversationId,
        .senderId = "user-a",
        .recipientId = "user-b",
        .content = "Hello! How are you doing?",
        .type = MessageType::TEXT
    };

    auto message = MessagingService::sendMessage(request);

    // Verify message created
    EXPECT_FALSE(message.messageId.empty());
    EXPECT_EQ(message.status, MessageStatus::SENT);

    // Verify recipient can see message
    auto messages = MessagingService::getConversationMessages(conversation.conversationId);
    EXPECT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].content, request.content);
}

TEST(MessagingTest, MessageEncryption) {
    std::string originalContent = "This is a secret message";
    std::string conversationId = "conv-123";

    // Encrypt
    std::string encrypted = MessageEncryptionService::encryptMessage(
        originalContent, conversationId);

    // Verify encrypted (not the same as original)
    EXPECT_NE(encrypted, originalContent);

    // Decrypt
    std::string decrypted = MessageEncryptionService::decryptMessage(
        encrypted, conversationId);

    // Verify decryption works
    EXPECT_EQ(decrypted, originalContent);
}
```

### Template System Tests
```cpp
TEST(TemplateTest, RenderTemplateWithVariables) {
    MessageTemplate tmpl = INTRODUCTION_TEMPLATE;

    std::map<std::string, std::string> variables = {
        {"recipient_name", "John Smith"},
        {"recipient_field", "machine learning"},
        {"sender_title", "Data Scientist"},
        {"sender_company", "Tech Corp"},
        {"connection_reason", "collaboration opportunities"},
        {"discussion_topic", "AI research"},
        {"sender_name", "Jane Doe"}
    };

    std::string rendered = MessageTemplateService::renderTemplate(tmpl, variables);

    // Verify all placeholders replaced
    EXPECT_THAT(rendered, HasSubstr("John Smith"));
    EXPECT_THAT(rendered, HasSubstr("machine learning"));
    EXPECT_THAT(rendered, HasSubstr("Data Scientist"));
    EXPECT_THAT(rendered, Not(HasSubstr("{{recipient_name}}")));  // No remaining placeholders
}
```

### Spam Detection Tests
```cpp
TEST(SpamDetectionTest, SpamPatternRecognition) {
    // Clean message
    Message cleanMessage{
        .content = "Hi, I'd like to discuss the project proposal."
    };
    auto cleanResult = MessageSpamDetectionService::checkMessageForSpam(cleanMessage);
    EXPECT_LT(cleanResult.spamScore, 0.3);

    // Spam message
    Message spamMessage{
        .content = "BUY NOW!!! CHEAP VIAGRA!!! LIMITED TIME OFFER!!! http://spam-site.ru/discount"
    };
    auto spamResult = MessageSpamDetectionService::checkMessageForSpam(spamMessage);
    EXPECT_GT(spamResult.spamScore, 0.8);
    EXPECT_TRUE(spamResult.isSpam);
}
```

### Search Tests
```cpp
TEST(MessageSearchTest, SearchMessagesByContent) {
    // Create conversation with messages
    auto conversationId = createConversationWithMessages("user-123", {
        "Hello, how are you?",
        "I'm working on a machine learning project",
        "That sounds interesting! Tell me more about it",
        "We're using neural networks for image recognition"
    });

    // Search for "machine learning"
    MessageSearchFilter filter{.query = "machine learning"};
    auto results = MessageSearchService::searchMessages("user-123", filter);

    // Should find relevant messages
    EXPECT_GE(results.hits.size(), 2);
    EXPECT_THAT(results.hits[0].contentSnippet, HasSubstr("machine learning"));

    // Verify highlights
    EXPECT_FALSE(results.hits[0].highlights.empty());
}
```

## 🎉 Success Criteria

### Core Messaging
- ✅ **Secure message sending and receiving**
- ✅ **Message threading and conversation management**
- ✅ **Real-time message delivery and read receipts**
- ✅ **End-to-end message encryption**

### Professional Features
- ✅ **Professional message templates**
- ✅ **Message categorization and tagging**
- ✅ **Message search and organization**
- ✅ **Conversation archiving and folders**

### Safety & Moderation
- ✅ **Spam detection and filtering**
- ✅ **Content moderation system**
- ✅ **Rate limiting and abuse prevention**
- ✅ **Message reporting and blocking**

### User Experience
- ✅ **Intuitive messaging interface**
- ✅ **Rich message formatting and attachments**
- ✅ **Smart notifications and alerts**
- ✅ **Mobile-responsive messaging**

This creates a **professional-grade messaging system** that **facilitates meaningful business communication** while **maintaining privacy and security** standards expected in professional networking.
