# 🚀 Link Blocks & Analytics System

**Duration:** 4 days
**Dependencies:** Profile database models, Clean URL routing
**Acceptance Criteria:**
- ✅ Link block storage and management
- ✅ Click tracking and analytics collection
- ✅ Real-time analytics dashboard
- ✅ Link customization (titles, descriptions, icons)
- ✅ Privacy controls for analytics data
- ✅ Performance monitoring for link redirects
- ✅ GDPR-compliant data retention policies

## 🎯 Task Description

Implement the link blocks feature with comprehensive analytics. Users can add links to their profiles (website, social media, etc.) and track how visitors interact with each link.

## 📋 Daily Breakdown

### Day 1: Link Block Storage
- Create LinkBlock model with MongoDB schema
- Implement link validation (URL format, HTTPS preference)
- Add link metadata (title, description, favicon)
- Create link ordering and grouping features
- Add privacy controls per link

### Day 2: Click Tracking System
- Implement click tracking middleware
- Create analytics event logging
- Add referrer and user-agent tracking
- Implement rate limiting to prevent spam
- Create background job for analytics aggregation

### Day 3: Analytics Dashboard API
- Create analytics endpoints for profile owners
- Implement time-based analytics (daily, weekly, monthly)
- Add link performance comparison
- Create visitor demographics tracking
- Implement data export features

### Day 4: Advanced Analytics Features
- Add conversion tracking for specific actions
- Implement A/B testing for link placement
- Create analytics alerts and notifications
- Add data visualization components
- Implement analytics data retention policies

## 🔧 Link Block Structure

```cpp
struct LinkBlock {
    std::string id;
    std::string profileId;
    std::string url;
    std::string title;
    std::string description;
    std::string iconUrl;
    bool isActive = true;
    PrivacyLevel privacy = PrivacyLevel::PUBLIC;
    std::vector<std::string> tags;
    int sortOrder = 0;
};
```

## 📊 Analytics Data Structure

```cpp
struct LinkAnalytics {
    std::string linkId;
    std::string date;
    int totalClicks = 0;
    int uniqueVisitors = 0;
    std::map<std::string, int> referrerStats;
    std::map<std::string, int> deviceStats;
    std::map<std::string, int> countryStats;
    double avgSessionDuration = 0.0;
};
```

## 🧪 Testing Strategy

### Link Management Tests
```cpp
TEST(LinkBlockTest, CreateAndUpdateLink) {
    LinkBlock link{
        .url = "https://github.com/user",
        .title = "My GitHub",
        .privacy = PrivacyLevel::PUBLIC
    };
    EXPECT_TRUE(link.isValid());
    EXPECT_TRUE(saveLink(link));
}
```

### Analytics Tests
```bash
# Test link click tracking
curl -H "Referer: https://hatef.ir/profile" \
     http://localhost:3000/l/abc123

# Test analytics API
curl http://localhost:3000/api/profiles/links/analytics \
  -H "Authorization: Bearer token"
```

### Performance Tests
- Test 1000+ concurrent link clicks
- Verify analytics aggregation doesn't slow down profile loading
- Test data retention policies work correctly

## 🔒 Privacy & Security

### Data Protection
- Analytics data encrypted at rest
- User IP addresses anonymized
- Configurable data retention periods
- Opt-out options for tracking

### Security Measures
- Rate limiting on analytics endpoints
- CSRF protection for link management
- Input validation for all URLs
- Secure redirect to prevent open redirect attacks

## 🎉 Success Criteria
- Link blocks render correctly on profiles
- Click tracking works with 99% accuracy
- Analytics data loads within 200ms
- Privacy controls work as expected
- Link redirects are secure and fast (< 50ms)
- GDPR compliance verified
