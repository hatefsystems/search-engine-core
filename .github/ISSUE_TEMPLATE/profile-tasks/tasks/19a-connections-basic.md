# 🚀 Professional Networking - Basic Connections System

**Duration:** 2 days
**Dependencies:** Profile database models, Profile routing CRUD
**Acceptance Criteria:**
- ✅ Connection request and acceptance workflow
- ✅ Connection list management and filtering
- ✅ Connection status tracking (pending, accepted, declined)
- ✅ Mutual connection discovery
- ✅ Connection privacy controls
- ✅ Connection notifications and alerts
- ✅ Basic networking analytics

## 🎯 Task Description

Implement the fundamental connection system that allows users to build professional networks by sending connection requests, accepting/declining requests, and managing their professional relationships with appropriate privacy controls.

## 🤝 Connection System Architecture

### Connection Data Model
```cpp
enum class ConnectionStatus {
    PENDING,                // Request sent, awaiting response
    ACCEPTED,               // Connection established
    DECLINED,               // Request declined
    BLOCKED,                // User blocked (no future requests)
    WITHDRAWN               // Request withdrawn by sender
};

enum class ConnectionType {
    PROFESSIONAL,           // Work-related connection
    COLLEAGUE,              // Current/former colleague
    MENTOR,                 // Mentorship relationship
    MENTEE,                 // Learning relationship
    CLIENT,                 // Business relationship
    PARTNER,                // Business partnership
    ACQUAINTANCE           // General professional acquaintance
};

struct ConnectionRequest {
    std::string connectionId;
    std::string requesterId;         // User sending request
    std::string targetId;            // User receiving request
    ConnectionStatus status;
    ConnectionType requestedType;

    // Request details
    std::string message;             // Personal message with request
    std::vector<std::string> sharedConnections;  // Common connections
    std::string mutualInterests;     // Shared skills/interests

    // Metadata
    Date requestedAt;
    Date respondedAt;               // When accepted/declined
    std::string responseMessage;    // Optional response message

    // Privacy & notifications
    bool isVisibleToOthers = true;  // Show in mutual connections
    NotificationPreferences notifications;

    // Analytics
    std::string requestSource;       // How they found each other
    std::string connectionStrength;  // "weak", "medium", "strong"
};

struct UserConnection {
    std::string userId;
    std::string connectionId;
    ConnectionType type;
    Date connectedAt;

    // Relationship metadata
    std::string relationshipNotes;   // Personal notes about connection
    std::vector<std::string> tags;   // Custom tags for organization
    int interactionCount = 0;       // Messages, profile views, etc.

    // Privacy settings
    bool showInNetwork = true;      // Visible in professional network
    bool allowTagging = true;       // Allow tagging in posts
    bool shareUpdates = true;       // Share profile updates

    // Connection strength
    double strengthScore = 1.0;     // 0.0 - 5.0 based on interactions
    Date lastInteraction;
};
```

### Connection Management Service
```cpp
class ConnectionService {
public:
    static ConnectionRequest sendConnectionRequest(
        const std::string& fromUserId,
        const std::string& toUserId,
        ConnectionType type,
        const std::string& message = "");

    static void respondToConnectionRequest(
        const std::string& connectionId,
        const std::string& userId,
        bool accepted,
        const std::string& responseMessage = "");

    static void withdrawConnectionRequest(
        const std::string& connectionId,
        const std::string& userId);

    static std::vector<UserConnection> getUserConnections(
        const std::string& userId,
        const ConnectionFilter& filter = {});

    static std::vector<ConnectionRequest> getPendingRequests(
        const std::string& userId);

    static ConnectionStats getConnectionStats(const std::string& userId);

    static std::vector<std::string> findMutualConnections(
        const std::string& userId1,
        const std::string& userId2);

private:
    static bool canSendRequest(const std::string& fromUserId,
                             const std::string& toUserId);

    static void notifyConnectionRequest(const ConnectionRequest& request);
    static void notifyConnectionResponse(const ConnectionRequest& request);
    static void updateConnectionStrength(const std::string& connectionId);
};
```

## 🔍 Connection Discovery System

### Mutual Connection Discovery
```cpp
struct MutualConnection {
    std::string userId;
    std::string name;
    std::string profileSlug;
    std::string avatarUrl;
    std::string headline;

    // Connection details
    int mutualConnectionsCount;
    std::vector<std::string> mutualConnectionIds;  // For detailed view
    std::vector<std::string> sharedInterests;      // Common skills/interests

    // Relevance scoring
    double relevanceScore;         // How relevant this suggestion is
    std::string connectionReason;  // "Works at same company", "Same industry", etc.
};

class ConnectionDiscoveryService {
public:
    static std::vector<MutualConnection> findMutualConnections(
        const std::string& userId, int limit = 20);

    static std::vector<MutualConnection> suggestNewConnections(
        const std::string& userId, int limit = 10);

    static std::vector<MutualConnection> findConnectionsBySkill(
        const std::string& userId,
        const std::string& skill,
        int limit = 10);

    static std::vector<MutualConnection> findAlumniConnections(
        const std::string& userId,
        const std::string& school,
        int limit = 10);

private:
    static double calculateConnectionRelevance(
        const std::string& userId,
        const std::string& potentialConnectionId);

    static std::vector<std::string> findSharedInterests(
        const std::string& userId1,
        const std::string& userId2);

    static std::string generateConnectionReason(
        const std::string& userId,
        const MutualConnection& mutual);
};
```

### Connection Suggestions Algorithm
```cpp
class ConnectionSuggestionEngine {
public:
    static std::vector<ConnectionSuggestion> generateSuggestions(
        const std::string& userId, int limit = 20);

private:
    static std::vector<ConnectionSuggestion> suggestBySharedConnections(
        const std::string& userId);

    static std::vector<ConnectionSuggestion> suggestByIndustry(
        const std::string& userId);

    static std::vector<ConnectionSuggestion> suggestByLocation(
        const std::string& userId);

    static std::vector<ConnectionSuggestion> suggestBySkills(
        const std::string& userId);

    static std::vector<ConnectionSuggestion> suggestByAlumni(
        const std::string& userId);

    static void deduplicateAndRank(std::vector<ConnectionSuggestion>& suggestions);
};

struct ConnectionSuggestion {
    std::string suggestedUserId;
    std::string name;
    std::string headline;
    double relevanceScore;          // 0.0 - 1.0
    std::vector<std::string> reasons;  // Why this suggestion
    std::string primaryReason;      // Most important reason
    Date suggestedAt;
    bool isDismissed = false;
};
```

## 📊 Connection Analytics

### Connection Statistics
```cpp
struct ConnectionStats {
    std::string userId;
    DateRange period;

    // Basic metrics
    int totalConnections;
    int pendingRequestsSent;
    int pendingRequestsReceived;
    int connectionsThisMonth;

    // Network composition
    std::map<ConnectionType, int> connectionsByType;
    std::map<std::string, int> connectionsByIndustry;
    std::map<std::string, int> connectionsByLocation;

    // Network quality
    double averageConnectionStrength;
    int highValueConnections;       // Connections with strong engagement
    int activeConnections;          // Interacted with in last 30 days

    // Growth metrics
    double networkGrowthRate;       // Connections added per month
    std::vector<MonthlyGrowth> monthlyGrowth;
    Date networkStartedAt;

    // Engagement metrics
    double responseRate;            // % of connection requests accepted
    double connectionRetention;     // % of connections still active after 6 months
    int messagesExchanged;
    int profileViewsOfConnections;

    // Network influence
    int secondDegreeConnections;    // Connections of connections
    int networkReach;               // Total unique people in extended network
};

struct MonthlyGrowth {
    Date month;
    int newConnections;
    int lostConnections;
    int netGrowth;
};
```

### Network Analysis Service
```cpp
class NetworkAnalysisService {
public:
    static ConnectionStats getConnectionStats(
        const std::string& userId, DateRange range = {});

    static NetworkInsights getNetworkInsights(const std::string& userId);

    static std::vector<NetworkGap> identifyNetworkGaps(const std::string& userId);

private:
    static void calculateNetworkMetrics(ConnectionStats& stats);
    static void analyzeNetworkComposition(ConnectionStats& stats);
    static void trackNetworkGrowth(ConnectionStats& stats);
};

struct NetworkInsights {
    std::string strongestIndustryConnection;
    std::string mostConnectedLocation;
    std::string primaryNetworkType;     // "industry", "location", "educational"
    std::vector<std::string> networkStrengths;
    std::vector<std::string> networkOpportunities;
    int networkDiversityScore;          // 0-100 based on industry/location spread
};

struct NetworkGap {
    std::string gapType;                // "industry", "location", "skill"
    std::string description;            // "Missing connections in tech industry"
    std::vector<std::string> suggestedActions;
    int potentialImpact;                // 1-10 scale
};
```

## 🔒 Connection Privacy Controls

### Privacy Settings
```cpp
struct ConnectionPrivacySettings {
    // Visibility settings
    bool showConnectionCount = true;    // Show total connections publicly
    bool showConnectionList = false;    // Show who connections are (privacy risk)
    bool showMutualConnections = true;  // Show mutual connection counts

    // Connection request settings
    bool allowConnectionRequests = true;  // Accept new connection requests
    bool requireRequestMessage = true;    // Require personalized message
    bool autoDeclineSpam = true;          // Auto-decline suspicious requests

    // Network visibility
    bool appearInSuggestions = true;     // Show in "People you may know"
    bool allowDiscoveryByEmail = true;   // Find connections by email
    bool allowDiscoveryByCompany = true; // Find connections by company

    // Notification preferences
    bool notifyOnRequests = true;
    bool notifyOnAcceptances = true;
    bool notifyOnSuggestions = false;    // Weekly digest instead
    bool notifyOnNetworkGrowth = false;  // Monthly summary

    // Data sharing
    bool shareConnectionData = false;    // Share anonymized network data for research
    bool allowNetworkAnalytics = true;  // Personal network insights
};

class ConnectionPrivacyService {
public:
    static bool canSeeConnection(const std::string& viewerId,
                               const std::string& connectionId,
                               const std::string& ownerId);

    static bool canSendConnectionRequest(const std::string& fromUserId,
                                       const std::string& toUserId);

    static std::vector<UserConnection> filterVisibleConnections(
        const std::vector<UserConnection>& connections,
        const std::string& viewerId,
        const std::string& ownerId);

    static void applyPrivacySettings(std::vector<ConnectionSuggestion>& suggestions,
                                   const ConnectionPrivacySettings& settings);

private:
    static bool isBlockedUser(const std::string& userId1, const std::string& userId2);
    static bool hasCommonConnections(const std::string& userId1, const std::string& userId2);
    static bool isWithinPrivacyBounds(const ConnectionPrivacySettings& settings,
                                    const std::string& viewerId);
};
```

## 🔔 Connection Notifications

### Notification Types
```cpp
enum class ConnectionNotificationType {
    CONNECTION_REQUEST_RECEIVED,
    CONNECTION_REQUEST_ACCEPTED,
    CONNECTION_REQUEST_DECLINED,
    CONNECTION_SUGGESTION_AVAILABLE,
    MUTUAL_CONNECTION_ADDED,
    CONNECTION_MILESTONE_REACHED,
    NETWORK_GROWTH_INSIGHT
};

struct ConnectionNotification {
    std::string notificationId;
    std::string userId;                    // Notification recipient
    ConnectionNotificationType type;
    std::string relatedUserId;             // The other person involved
    std::string relatedConnectionId;       // Connection request ID
    std::string message;
    Date createdAt;
    bool isRead = false;

    // Context data
    std::string requesterName;
    std::string requesterHeadline;
    std::string mutualConnectionsCount;    // "3 mutual connections"
    std::string milestoneDescription;      // For milestone notifications
};

class ConnectionNotificationService {
public:
    static void notifyConnectionRequest(const ConnectionRequest& request);
    static void notifyConnectionResponse(const ConnectionRequest& request,
                                       bool accepted);
    static void notifyNewSuggestions(const std::string& userId,
                                   int suggestionCount);
    static void notifyMilestoneReached(const std::string& userId,
                                     const std::string& milestone);

private:
    static std::string generateNotificationMessage(
        ConnectionNotificationType type,
        const std::string& actorName,
        const std::string& context = "");

    static bool shouldNotifyUser(const std::string& userId,
                               ConnectionNotificationType type);

    static void batchSimilarNotifications(const std::string& userId);
};
```

## 👥 Connection Management UI

### Connection Request Interface
```html
<div class="connection-request-modal">
    <div class="modal-header">
        <h3>Connect with Sarah Johnson</h3>
        <div class="connection-context">
            <img src="/avatars/sarah.jpg" class="profile-avatar">
            <div class="profile-info">
                <div class="profile-name">Sarah Johnson</div>
                <div class="profile-headline">Senior Product Manager at TechCorp</div>
                <div class="mutual-connections">3 mutual connections</div>
            </div>
        </div>
    </div>

    <div class="modal-body">
        <div class="connection-type-selector">
            <label>How do you know Sarah?</label>
            <select id="connection-type">
                <option value="professional">Professional</option>
                <option value="colleague">Colleague</option>
                <option value="client">Client</option>
                <option value="partner">Business Partner</option>
                <option value="acquaintance">Professional Acquaintance</option>
            </select>
        </div>

        <div class="personal-message">
            <label for="message">Add a personal note (optional)</label>
            <textarea id="message" placeholder="Hi Sarah, I'd like to connect with you because..." maxlength="300" rows="3"></textarea>
            <div class="character-count">0/300</div>
        </div>

        <div class="connection-benefits">
            <h4>Why connect?</h4>
            <ul>
                <li>View Sarah's full profile and contact information</li>
                <li>Send direct messages and collaborate</li>
                <li>Get notified about her professional updates</li>
                <li>Expand your professional network</li>
            </ul>
        </div>
    </div>

    <div class="modal-footer">
        <button class="cancel-btn">Cancel</button>
        <button class="send-request-btn" disabled>Send Connection Request</button>
    </div>
</div>
```

### Connection List Interface
```html
<div class="connections-page">
    <div class="page-header">
        <h1>My Network</h1>
        <div class="network-stats">
            <div class="stat">1st: 42 connections</div>
            <div class="stat">2nd: 156 connections</div>
            <div class="stat">Network reach: 2,847</div>
        </div>
    </div>

    <div class="connections-toolbar">
        <div class="search-filter">
            <input type="text" placeholder="Search connections..." id="connection-search">
            <select id="type-filter">
                <option value="all">All Connections</option>
                <option value="professional">Professional</option>
                <option value="colleague">Colleague</option>
                <option value="client">Client</option>
            </select>
        </div>

        <div class="view-options">
            <button class="view-btn active" data-view="grid">Grid</button>
            <button class="view-btn" data-view="list">List</button>
        </div>
    </div>

    <div class="connections-grid">
        <div class="connection-card" data-connection-id="conn-123">
            <div class="card-header">
                <img src="/avatars/john.jpg" class="connection-avatar">
                <div class="connection-actions">
                    <button class="message-btn">Message</button>
                    <div class="dropdown-menu">
                        <button class="more-btn">⋯</button>
                        <div class="dropdown-content">
                            <a href="#" class="edit-relationship">Edit Relationship</a>
                            <a href="#" class="remove-connection">Remove Connection</a>
                        </div>
                    </div>
                </div>
            </div>

            <div class="card-body">
                <h3 class="connection-name">John Smith</h3>
                <div class="connection-headline">Software Engineer at TechCorp</div>
                <div class="connection-type">Colleague</div>
                <div class="connection-strength">
                    <div class="strength-indicator" style="width: 75%"></div>
                    <span>Strong connection</span>
                </div>
            </div>

            <div class="card-footer">
                <div class="interaction-info">
                    Connected 6 months ago • 12 interactions
                </div>
                <div class="shared-tags">
                    <span class="tag">technology</span>
                    <span class="tag">software</span>
                </div>
            </div>
        </div>
    </div>

    <!-- Connection Suggestions Sidebar -->
    <div class="suggestions-sidebar">
        <h3>People you may know</h3>
        <div class="suggestion-card">
            <img src="/avatars/mike.jpg" class="suggestion-avatar">
            <div class="suggestion-info">
                <div class="suggestion-name">Mike Chen</div>
                <div class="suggestion-reason">Works at TechCorp • 2 mutual connections</div>
            </div>
            <button class="connect-btn">Connect</button>
        </div>
    </div>
</div>
```

## 📋 Implementation Plan

### Day 1: Core Connection System
- Implement connection request/acceptance workflow
- Create connection list management and filtering
- Add connection status tracking and notifications
- Build basic connection discovery (mutual connections)

### Day 1 Continued: Privacy + Analytics
- Implement connection privacy controls
- Add connection analytics and statistics
- Create connection suggestions system
- Test end-to-end connection flow

## 🧪 Testing Strategy

### Connection Workflow Tests
```cpp
TEST(ConnectionTest, RequestAcceptWorkflow) {
    // User A sends request to User B
    auto request = ConnectionService::sendConnectionRequest(
        "user-a", "user-b", ConnectionType::PROFESSIONAL,
        "I'd like to connect with you regarding our shared interest in AI.");

    EXPECT_FALSE(request.connectionId.empty());
    EXPECT_EQ(request.status, ConnectionStatus::PENDING);

    // User B accepts request
    ConnectionService::respondToConnectionRequest(
        request.connectionId, "user-b", true, "Happy to connect!");

    // Verify connection established
    auto connectionsA = ConnectionService::getUserConnections("user-a");
    auto connectionsB = ConnectionService::getUserConnections("user-b");

    EXPECT_EQ(connectionsA.size(), 1);
    EXPECT_EQ(connectionsB.size(), 1);
    EXPECT_EQ(connectionsA[0].connectionId, connectionsB[0].connectionId);
}

TEST(ConnectionTest, MutualConnectionsDiscovery) {
    // Create network: A-B, A-C, B-C (A and B have mutual connection C)
    createConnection("user-a", "user-b");
    createConnection("user-a", "user-c");
    createConnection("user-b", "user-c");

    // Find mutual connections between A and B
    auto mutual = ConnectionService::findMutualConnections("user-a", "user-b");

    EXPECT_EQ(mutual.size(), 1);
    EXPECT_EQ(mutual[0], "user-c");
}
```

### Privacy Tests
```cpp
TEST(ConnectionPrivacyTest, VisibilityControls) {
    // User A has private connection settings
    ConnectionPrivacySettings privateSettings{
        .showConnectionList = false,
        .showConnectionCount = true
    };

    // User B views User A's profile
    auto visibleConnections = ConnectionPrivacyService::filterVisibleConnections(
        getAllConnections("user-a"), "user-b", "user-a");

    // Should not see individual connections, only count
    EXPECT_TRUE(visibleConnections.empty());  // No individual connections visible

    // But connection count should be visible
    auto stats = ConnectionService::getConnectionStats("user-a");
    EXPECT_TRUE(stats.totalConnections > 0);  // Count is visible
}

TEST(ConnectionPrivacyTest, ConnectionRequestValidation) {
    // Blocked user tries to send request
    createBlock("user-a", "user-b");

    // Request should be rejected
    EXPECT_THROW(
        ConnectionService::sendConnectionRequest("user-b", "user-a",
                                               ConnectionType::PROFESSIONAL),
        ConnectionBlockedException
    );
}
```

### Analytics Tests
```cpp
TEST(ConnectionAnalyticsTest, NetworkMetrics) {
    // Create diverse connection network
    createTestNetwork("user-123", {
        {"colleague", 5},
        {"professional", 10},
        {"client", 3},
        {"partner", 2}
    });

    // Get analytics
    auto stats = ConnectionService::getConnectionStats("user-123");

    // Verify calculations
    EXPECT_EQ(stats.totalConnections, 20);
    EXPECT_EQ(stats.connectionsByType[ConnectionType::PROFESSIONAL], 10);
    EXPECT_TRUE(stats.averageConnectionStrength > 0);
    EXPECT_TRUE(stats.networkGrowthRate >= 0);
}
```

## 🎉 Success Criteria

### Connection Management
- ✅ **Connection request and acceptance workflow**
- ✅ **Connection list management with filtering**
- ✅ **Connection status tracking (pending/accepted/declined)**
- ✅ **Connection categorization and tagging**

### Discovery & Networking
- ✅ **Mutual connection discovery**
- ✅ **Connection suggestions algorithm**
- ✅ **Network analytics and insights**
- ✅ **Connection strength scoring**

### Privacy & Control
- ✅ **Granular privacy controls for connections**
- ✅ **Connection visibility settings**
- ✅ **Block/unblock functionality**
- ✅ **Connection data export controls**

### User Experience
- ✅ **Intuitive connection request interface**
- ✅ **Rich connection profile cards**
- ✅ **Smart notifications and alerts**
- ✅ **Mobile-responsive connection management**

This establishes the **foundation for professional networking** with **privacy-first design** and **intelligent connection discovery** to help users build meaningful professional relationships.
