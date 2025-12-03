# 🚀 Community & Groups System

**Duration:** 5 days
**Dependencies:** Profile database models, Social engagement (Task 24), Content feed (Task 22)
**Priority:** 🟡 MEDIUM - Important for community engagement and retention
**Acceptance Criteria:**
- ✅ Industry-specific groups
- ✅ Location-based communities
- ✅ Professional interest groups
- ✅ Group discussions and forums
- ✅ Group events and meetups
- ✅ Community leaderboards and recognition
- ✅ Group content sharing
- ✅ Group moderation tools
- ✅ Local community features

## 🎯 Task Description

Create a comprehensive community and groups system that enables users to form industry-specific groups, location-based communities, and professional interest groups with local language optimization, fostering engagement, networking, and knowledge sharing within the Hatef.ir platform.

## 📋 Daily Breakdown

### Day 1: Group Foundation & Data Models
- Create Group and GroupMember models
- Implement group creation and management
- Add group types (industry, location, interest)
- Create group discovery system
- Add basic group permissions

### Day 2: Group Discussions & Forums
- Implement discussion threads system
- Add post and reply functionality
- Create discussion categories
- Add discussion moderation tools
- Implement discussion search

### Day 3: Group Content & Sharing
- Add group content feed
- Implement group announcements
- Create group file sharing
- Add group media gallery
- Implement group activity feed

### Day 4: Group Events & Meetups
- Create group events system
- Implement event creation and management
- Add event RSVP and attendance
- Create event calendar
- Add event notifications

### Day 5: Community Features & Gamification
- Implement community leaderboards
- Add member recognition system
- Create community badges
- Add group analytics
- Implement local community features

## 🔧 Community Data Structures

```cpp
struct Group {
    std::string id;
    std::string name; // Local language or English name
    std::string slug; // URL-friendly identifier
    std::string description;
    std::string longDescription;
    GroupType type; // INDUSTRY, LOCATION, INTEREST, PROFESSIONAL
    GroupPrivacy privacy; // PUBLIC, PRIVATE, SECRET
    std::string coverImageUrl;
    std::string logoUrl;
    
    // Category and location
    std::string industry; // For industry groups
    std::string location; // For location groups (city, province)
    std::vector<std::string> tags; // Interest tags
    
    // Membership
    std::string creatorId;
    int memberCount = 0;
    int activeMembers = 0; // Active in last 30 days
    std::vector<std::string> adminIds;
    std::vector<std::string> moderatorIds;
    
    // Activity metrics
    int postCount = 0;
    int discussionCount = 0;
    int eventCount = 0;
    double engagementScore = 0.0;
    
    // Settings
    GroupSettings settings;
    Date createdAt;
    Date lastActivityAt;
    bool isVerified = false;
    bool isFeatured = false;
};

struct GroupMember {
    std::string id;
    std::string groupId;
    std::string profileId;
    MemberRole role; // OWNER, ADMIN, MODERATOR, MEMBER
    MemberStatus status; // ACTIVE, INVITED, BANNED, LEFT
    Date joinedAt;
    Date lastActiveAt;
    
    // Member activity
    int postCount = 0;
    int commentCount = 0;
    int eventAttendance = 0;
    double contributionScore = 0.0;
    
    // Recognition
    std::vector<std::string> badges;
    int reputationPoints = 0;
    bool isTopContributor = false;
};

struct GroupDiscussion {
    std::string id;
    std::string groupId;
    std::string authorId;
    std::string title; // Local language or English
    std::string content;
    DiscussionCategory category; // QUESTION, DISCUSSION, ANNOUNCEMENT, NEWS
    std::vector<std::string> tags;
    
    // Engagement
    int viewCount = 0;
    int replyCount = 0;
    int likeCount = 0;
    bool isPinned = false;
    bool isLocked = false;
    
    // Moderation
    DiscussionStatus status; // ACTIVE, CLOSED, DELETED, MODERATED
    Date createdAt;
    Date lastReplyAt;
    std::string lastReplyById;
};

struct GroupPost {
    std::string id;
    std::string groupId;
    std::string authorId;
    PostType type; // TEXT, IMAGE, VIDEO, LINK, EVENT, ANNOUNCEMENT
    std::string content;
    std::vector<std::string> imageUrls;
    std::vector<std::string> attachmentUrls;
    
    // Engagement
    int likeCount = 0;
    int commentCount = 0;
    int shareCount = 0;
    
    // Visibility
    PostVisibility visibility; // ALL_MEMBERS, ADMINS_ONLY
    bool isPinned = false;
    Date createdAt;
    Date editedAt;
};

struct GroupEvent {
    std::string id;
    std::string groupId;
    std::string organizerId;
    std::string title; // Local language title
    std::string description;
    EventType type; // MEETUP, WORKSHOP, NETWORKING, CONFERENCE, WEBINAR
    
    // Date and location
    Date eventDate;
    std::string startTime;
    std::string endTime;
    std::string timezone; // Local timezone
    EventLocation location; // ONLINE, OFFLINE, HYBRID
    std::string venue; // Physical venue or online link
    GeographicLocation geoLocation;
    
    // Capacity and RSVP
    int maxAttendees = 0;
    int confirmedAttendees = 0;
    std::vector<EventRSVP> rsvps;
    
    // Event status
    EventStatus status; // DRAFT, PUBLISHED, ONGOING, COMPLETED, CANCELLED
    Date createdAt;
    Date publishedAt;
};

struct EventRSVP {
    std::string eventId;
    std::string profileId;
    RSVPStatus status; // GOING, MAYBE, NOT_GOING
    Date rsvpDate;
    bool attended = false;
    std::string attendanceCode; // For verification
};

struct CommunityLeaderboard {
    std::string groupId;
    LeaderboardType type; // TOP_CONTRIBUTORS, MOST_ACTIVE, MOST_HELPFUL
    LeaderboardPeriod period; // WEEKLY, MONTHLY, ALL_TIME
    std::vector<LeaderboardEntry> entries;
    Date lastUpdated;
};

struct LeaderboardEntry {
    int rank;
    std::string profileId;
    std::string profileName;
    int score;
    std::string metric; // posts, comments, likes, events_attended
    std::string badge; // Recognition badge
};

struct GroupSettings {
    bool allowMemberPosts = true;
    bool requirePostApproval = false;
    bool allowEvents = true;
    bool allowDiscussions = true;
    bool allowFileSharing = true;
    bool showMemberList = true;
    std::string joinApprovalType; // AUTO, MANUAL, INVITE_ONLY
    int maxMembersPerGroup = 10000;
};
```

## ⚠️ What This Is NOT

### Hatef Groups ≠ Social Media Groups

**Critical Distinction:** These are PROFESSIONAL groups, not social communities.

```markdown
❌ **We Are NOT:**
- A Facebook Groups clone
- A Telegram channel replacement
- A place for memes and casual chat
- An entertainment community
- A "local language Reddit"

✅ **We ARE:**
- Professional networking groups
- Industry-specific knowledge sharing
- Business opportunity platforms
- Career development communities
- Goal-oriented professional spaces
```

### Purpose: Professional Advancement

Every group feature must serve professional goals:

| Feature | Professional Purpose | NOT For |
|---------|---------------------|---------|
| **Industry Groups** | Network with professionals | Social hangout |
| **Discussions** | Knowledge sharing, problem-solving | Random chat |
| **Events** | Professional meetups, networking | Social parties |
| **Content** | Work updates, insights | Personal stories |

### Group Content Guidelines

```cpp
struct GroupContentPolicy {
    // ✅ Encouraged content:
    std::vector<std::string> professional = {
        "Job opportunities",
        "Industry insights",
        "Professional questions",
        "Business partnerships",
        "Project collaborations",
        "Skill development"
    };
    
    // ❌ Discouraged content:
    std::vector<std::string> social = {
        "Personal life updates",
        "Entertainment/memes",
        "Political discussions",
        "Religious debates",
        "Off-topic chat"
    };
    
    // Groups focus on: Jobs, Customers, Credibility
    bool alignsWithGoals(const Post& post) {
        return post.helpsCareer || 
               post.helpsBusiness || 
               post.buildsProfessionalNetwork;
    }
};
```

### Constant Reminder: Professional Focus

```markdown
## Throughout Group UI:

**Group Header:**
💼 "این یک گروه حرفه‌ای است - برای شبکه‌سازی و پیشرفت شغلی"

**Before Posting:**
💡 Tip: Share professional insights, opportunities, questions

**Group Rules (Auto-displayed):**
✅ Professional content only
✅ Respectful knowledge sharing
✅ Job/business opportunities welcome
❌ No personal/social content
❌ No off-topic discussions

**Moderation Message:**
"این گروه برای پیشرفت حرفه‌ای است، نه گپ‌وگفت اجتماعی"
```

### Group Success Metrics (Not Vanity)

```cpp
struct GroupMetrics {
    // ❌ Don't measure (vanity):
    // int totalPosts;
    // int totalMembers;
    // int totalLikes;
    
    // ✅ Do measure (professional value):
    int jobsPosted = 0;
    int jobsFilled = 0;
    int businessPartnerships = 0;
    int problemsSolved = 0;
    int knowledgeShared = 0;
    int professionalConnections = 0;
    
    // Success = group helps members achieve goals
    bool isSuccessful() {
        return jobsFilled > 0 || 
               businessPartnerships > 0 ||
               professionalConnections > 50;
    }
};
```

## 👥 Group Types

### Industry Groups (گروه‌های صنعت)
**Purpose:** Connect professionals in the same industry

**Examples:**
- **فناوری اطلاعات** (Information Technology)
- **املاک و مستغلات** (Real Estate)
- **پزشکی و سلامت** (Medical & Health)
- **آموزش** (Education)
- **مالی و بانکداری** (Finance & Banking)
- **بازاریابی و تبلیغات** (Marketing & Advertising)
- **ساختمان و معماری** (Construction & Architecture)

**Features:**
- Industry news and updates
- Job opportunities
- Professional discussions
- Industry events and conferences
- Resource sharing

### Location Groups (گروه‌های محلی)
**Purpose:** Connect people in the same geographic area

**Examples:**
- **Local Entrepreneurs** (Local Entrepreneurs)
- **کسب‌وکارهای اصفهان** (Isfahan Businesses)
- **فعالان فناوری تبریز** (Tabriz Tech Professionals)
- **District 1** (District 1)

**Features:**
- Local networking events
- Local business opportunities
- Community support
- Local market insights
- Regional news

### Interest Groups (گروه‌های علاقه‌مندی)
**Purpose:** Connect people with shared professional interests

**Examples:**
- **استارتاپ‌های محلی** (Local Startups)
- **بازاریابی دیجیتال** (Digital Marketing)
- **طراحی UI/UX** (UI/UX Design)
- **برنامه‌نویسی** (Programming)
- **مدیریت پروژه** (Project Management)
- **رهبری و مدیریت** (Leadership & Management)

**Features:**
- Knowledge sharing
- Best practices
- Resource recommendations
- Skill development
- Collaboration opportunities

### Professional Groups (گروه‌های حرفه‌ای)
**Purpose:** Formal professional associations

**Examples:**
- **انجمن مهندسان نرم‌افزار** (Software Engineers Association)
- **کانون وکلا** (Lawyers Association)
- **انجمن پزشکان** (Medical Doctors Association)
- **سازمان نظام مهندسی** (Engineering Organization)

**Features:**
- Professional certifications
- Continuing education
- Industry standards
- Professional networking
- Advocacy and representation

## 💬 Group Discussions & Forums

### Discussion Features

#### Thread Types
- **Questions**: Ask and answer questions
- **Discussions**: Open-ended discussions
- **Announcements**: Important group announcements
- **News**: Share industry or community news
- **Resources**: Share helpful resources
- **Polls**: Create polls and surveys

#### Discussion Management
- **Categories**: Organize discussions by category
- **Tags**: Tag discussions for easy discovery
- **Pin important**: Pin important discussions
- **Lock threads**: Lock discussions to prevent replies
- **Archive old**: Archive old discussions
- **Search**: Search discussions by keywords

#### Reply System
- **Nested replies**: Threaded conversation
- **Quote replies**: Quote previous replies
- **Mention users**: @mention group members
- **Rich formatting**: Bold, italic, lists, code blocks
- **Attachments**: Attach files and images
- **Reactions**: React to replies (like, helpful, agree)

### Moderation Tools

#### Content Moderation
- **Auto-moderation**: Spam and abuse detection
- **Manual review**: Review flagged content
- **Edit content**: Edit inappropriate content
- **Delete content**: Remove violating content
- **Member warnings**: Warn problematic members
- **Member bans**: Ban violating members

#### Moderation Features
- **Moderation queue**: Review pending posts
- **Report system**: Members report content
- **Moderation log**: Track moderation actions
- **Content filters**: Keyword filtering
- **User reputation**: Track member behavior

## 📱 Group Content & Sharing

### Content Types

#### Group Posts
- **Text posts**: Share thoughts and updates
- **Image posts**: Share photos and graphics
- **Video posts**: Share videos
- **Link posts**: Share external links
- **Event posts**: Promote group events
- **Announcements**: Important group announcements

#### Shared Resources
- **Documents**: PDFs, presentations, spreadsheets
- **Images**: Photos, infographics, designs
- **Videos**: Training videos, recordings
- **Links**: Useful resources and articles
- **Templates**: Reusable templates

### Group Feed

#### Feed Algorithm
```cpp
feedRelevance = (
    recencyScore * 0.3 +
    engagementScore * 0.25 +
    authorReputationScore * 0.2 +
    contentQualityScore * 0.15 +
    memberInterestScore * 0.1
) * pinnedBoost
```

#### Feed Types
- **Recent activity**: Latest group activity
- **Top posts**: Most engaged posts
- **Announcements**: Important announcements
- **Events**: Upcoming events
- **Discussions**: Active discussions

## 🎉 Group Events & Meetups

### Event Types

#### In-Person Events
- **Networking meetups**: Casual networking
- **Workshops**: Skill-building workshops
- **Conferences**: Industry conferences
- **Seminars**: Educational seminars
- **Social events**: Social gatherings

#### Online Events
- **Webinars**: Online presentations
- **Virtual meetups**: Online networking
- **Online workshops**: Virtual training
- **Live Q&A**: Interactive sessions
- **Virtual conferences**: Online conferences

#### Hybrid Events
- **Mixed format**: In-person + online
- **Simulcast**: Broadcast in-person to online
- **Interactive**: Online participation in physical events

### Event Management

#### Event Creation
- **Event details**: Title, description, agenda
- **Date and time**: Local calendar support
- **Location**: Venue or online link
- **Capacity**: Maximum attendees
- **Registration**: RSVP and tickets
- **Custom fields**: Collect attendee info

#### Event Promotion
- **Group announcements**: Announce to group
- **Email invitations**: Email group members
- **Social sharing**: Share on social media
- **Calendar integration**: Add to calendar
- **Reminders**: Automated reminders

#### Event Attendance
- **RSVP tracking**: Track RSVPs
- **Check-in system**: Attendance verification
- **QR codes**: QR code check-in
- **Attendance badges**: Award attendance badges
- **Post-event survey**: Gather feedback

## 🏆 Community Leaderboards & Recognition

### Leaderboard Types

#### Activity Leaderboards
- **Most active members**: Most posts and comments
- **Top contributors**: Highest contribution score
- **Most helpful**: Most helpful answers
- **Event champions**: Most event attendance
- **Rising stars**: Fastest growing members

#### Engagement Leaderboards
- **Most liked**: Most liked posts
- **Most commented**: Most discussed posts
- **Most shared**: Most shared content
- **Best discussions**: Highest engagement discussions

#### Community Leaderboards
- **Group leaders**: Top groups by activity
- **Fastest growing**: Fastest growing groups
- **Most engaged**: Highest engagement groups
- **Best events**: Best attended events

### Recognition System

#### Member Badges

**Activity Badges:**
- **First Post**: Made first group post
- **Active Member**: Active for 30 days
- **Super Active**: Active for 90 days
- **Group Veteran**: Member for 1 year

**Contribution Badges:**
- **Helpful**: 10 helpful answers
- **Expert**: 50 helpful answers
- **Guru**: 100 helpful answers
- **Mentor**: Helped 50 members

**Event Badges:**
- **Event Goer**: Attended 5 events
- **Event Regular**: Attended 20 events
- **Event Champion**: Attended 50 events

**Leadership Badges:**
- **Group Creator**: Created a group
- **Group Admin**: Admin of a group
- **Moderator**: Moderator role
- **Community Leader**: Led major initiatives

#### Reputation System
- **Reputation points**: Earn points for contributions
- **Reputation levels**: Levels based on points
- **Reputation benefits**: Unlock features with levels
- **Reputation display**: Show on profile

### Community Recognition

#### Member Spotlight
- **Member of the week**: Featured member
- **Success stories**: Share member achievements
- **Expert profiles**: Highlight experts
- **Community stars**: Recognize top contributors

#### Achievement Celebrations
- **Milestone celebrations**: Celebrate group milestones
- **Member achievements**: Celebrate member achievements
- **Group anniversaries**: Celebrate group birthdays
- **Success announcements**: Share success stories

## 🧪 Testing Strategy

### Group Tests
```cpp
TEST(GroupTest, CreateIndustryGroup) {
    Group group{
        .name = "فناوری اطلاعات",
        .type = GroupType::INDUSTRY,
        .privacy = GroupPrivacy::PUBLIC,
        .industry = "Technology"
    };
    EXPECT_TRUE(createGroup(group));
    EXPECT_EQ(getGroupCount(), 1);
}

TEST(GroupTest, JoinGroup) {
    auto group = createTestGroup();
    auto member = joinGroup(group.id, "user123");
    EXPECT_TRUE(member.has_value());
    EXPECT_EQ(getMemberCount(group.id), 1);
}
```

### Discussion Tests
```cpp
TEST(DiscussionTest, CreateDiscussion) {
    GroupDiscussion discussion{
        .groupId = "group123",
        .authorId = "user456",
        .title = "سوال درباره...",
        .content = "محتوای سوال",
        .category = DiscussionCategory::QUESTION
    };
    EXPECT_TRUE(createDiscussion(discussion));
}

TEST(DiscussionTest, ReplyToDiscussion) {
    auto discussion = createTestDiscussion();
    auto reply = replyToDiscussion(discussion.id, "user789", "پاسخ من");
    EXPECT_TRUE(reply.has_value());
    EXPECT_EQ(getReplyCount(discussion.id), 1);
}
```

### Event Tests
```cpp
TEST(EventTest, CreateGroupEvent) {
    GroupEvent event{
        .groupId = "group123",
        .organizerId = "user456",
        .title = "رویداد شبکه‌سازی",
        .eventDate = parseDate("2024-06-01"),
        .type = EventType::MEETUP
    };
    EXPECT_TRUE(createEvent(event));
}

TEST(EventTest, RSVPToEvent) {
    auto event = createTestEvent();
    auto rsvp = rsvpToEvent(event.id, "user789", RSVPStatus::GOING);
    EXPECT_TRUE(rsvp.has_value());
    EXPECT_EQ(getAttendeeCount(event.id), 1);
}
```

### Integration Tests
```bash
# Test group creation
curl -X POST http://localhost:3000/api/groups \
  -H "Authorization: Bearer token" \
  -d '{"name":"فناوری اطلاعات","type":"INDUSTRY","privacy":"PUBLIC"}'

# Test joining group
curl -X POST http://localhost:3000/api/groups/group123/join \
  -H "Authorization: Bearer token"

# Test creating discussion
curl -X POST http://localhost:3000/api/groups/group123/discussions \
  -H "Authorization: Bearer token" \
  -d '{"title":"سوال","content":"محتوا","category":"QUESTION"}'

# Test creating event
curl -X POST http://localhost:3000/api/groups/group123/events \
  -H "Authorization: Bearer token" \
  -d '{"title":"رویداد","eventDate":"2024-06-01","type":"MEETUP"}'
```

## 🎨 User Interface

### Group Discovery Page
- **Featured groups**: Showcase featured groups
- **Group categories**: Browse by type
- **Popular groups**: Most popular groups
- **Recommended groups**: Personalized recommendations
- **Search groups**: Search by name, industry, location

### Group Page
- **Group header**: Cover image, name, description
- **Group stats**: Members, posts, events
- **Join button**: Join or request to join
- **Group feed**: Activity feed
- **Tabs**: Discussions, Events, Members, About

### Discussion Interface
- **Discussion list**: List of discussions
- **Discussion detail**: Thread view
- **Reply editor**: Rich text editor
- **Reactions**: Like, helpful reactions
- **Share button**: Share discussion

### Event Interface
- **Event list**: Upcoming and past events
- **Event detail**: Full event information
- **RSVP button**: RSVP to event
- **Calendar view**: Calendar of events
- **Event reminders**: Notification settings

### Member Directory
- **Member list**: All group members
- **Member filters**: Filter by role, activity
- **Member search**: Search members
- **Member profiles**: View member profiles
- **Leaderboard**: Top contributors

## 🎉 Success Criteria
- Groups can be created and managed successfully
- Discussions load and post in <2 seconds
- Group feed updates in real-time
- Events RSVP system works smoothly
- Leaderboards update every 10 minutes
- Local language support works perfectly
- Moderation tools prevent 99% of spam
- Group discovery surfaces relevant groups
- Member directory loads in <1 second
- System handles 1K+ groups with 100K+ members
- Event system handles 500+ events per month

## 📊 Expected Impact

### Community Engagement
- **Active groups**: 500-1000 active groups
- **Member participation**: 30-40% members active monthly
- **Discussion activity**: 5-10 discussions per group per week
- **Event attendance**: 60-70% RSVP conversion to attendance
- **Time on platform**: 2-3x increase from community features

### Network Effects
- **Connection growth**: 50-100% increase in connections
- **Knowledge sharing**: 10K+ discussions per month
- **Event networking**: 500+ events per month
- **Community building**: Strong sense of community
- **Platform stickiness**: Community drives retention

### Business Value
- **User retention**: 40-50% improvement in retention
- **User engagement**: 3-5x increase in engagement
- **Professional networking**: Enhanced networking value
- **Knowledge exchange**: Valuable content creation
- **Platform differentiation**: Unique community features

### Platform Growth
- **Organic growth**: Communities drive 20-30% growth
- **User satisfaction**: Higher satisfaction scores
- **Network density**: Stronger network through communities
- **Content creation**: 50K+ community posts per month
- **Platform value**: Increased perceived value

## 🎯 Community Strategy

### Launch Strategy
1. **Seed groups**: Create 20-30 seed groups
2. **Invite influencers**: Invite industry leaders
3. **Content seeding**: Seed initial discussions
4. **Event kickoff**: Launch with networking events

### Growth Strategy
1. **Group recommendations**: Recommend relevant groups
2. **Member invitations**: Enable member invitations
3. **Cross-promotion**: Promote groups to profiles
4. **Featured groups**: Feature active groups

### Engagement Strategy
1. **Discussion prompts**: Suggest discussion topics
2. **Event promotion**: Promote upcoming events
3. **Member recognition**: Recognize active members
4. **Content curation**: Curate best discussions

### Moderation Strategy
1. **Community guidelines**: Clear community rules
2. **Moderator training**: Train group moderators
3. **Automated moderation**: AI-powered moderation
4. **Escalation process**: Handle violations effectively

## 💡 Local Community Features

### Local Language Optimization
- **RTL interface**: Right-to-left layout
- **Local calendar**: Jalali calendar integration
- **Local dates**: Local date formatting
- **Local numbers**: Local number formatting
- **Local search**: Local keyword search

### Local Cultural Features
- **Local holidays**: Local holiday calendar
- **Local customs**: Respect local customs
- **Local etiquette**: Local communication norms
- **Cultural sensitivity**: Culturally appropriate content

### Local Networking
- **Local cities**: City-based groups
- **Local industries**: Regional industry groups
- **Local professionals**: Connect local language speakers globally
- **Diaspora communities**: Connect diaspora communities


