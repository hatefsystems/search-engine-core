# 🚀 Viral Sharing & Referral System

**Duration:** 4 days
**Dependencies:** Profile database models, Social engagement (Task 24), Analytics system
**Priority:** 🔴 CRITICAL - Essential for viral growth and 1M daily visits goal
**Acceptance Criteria:**
- ✅ QR code generation for profiles
- ✅ Social media sharing optimization
- ✅ Referral program with rewards
- ✅ Share tracking and analytics
- ✅ Viral coefficient measurement
- ✅ Custom landing pages for shared profiles
- ✅ Share leaderboards and gamification
- ✅ Persian social platform integration
- ✅ Referral reward system

## 🎯 Task Description

Create a comprehensive viral sharing and referral system that makes it easy for users to share profiles across multiple platforms, tracks sharing effectiveness, rewards users for successful referrals, and creates viral growth mechanisms that can drive exponential user acquisition.

## 📋 Daily Breakdown

### Day 1: QR Code & Basic Sharing
- Create QR code generation system for profiles
- Implement basic share functionality
- Add share button components
- Create shareable profile URLs with tracking
- Add share analytics data model

### Day 2: Social Media Integration
- Implement social media sharing buttons
- Add Persian platforms (Telegram, Instagram, WhatsApp)
- Create optimized share previews for each platform
- Add share tracking for each platform
- Implement share click tracking

### Day 3: Referral Program & Rewards
- Create referral tracking system
- Implement referral code generation
- Add referral reward system (badges, visibility boosts)
- Create referral dashboard
- Add referral leaderboards

### Day 4: Custom Landing Pages & Viral Analytics
- Create custom landing pages for shared profiles
- Implement viral coefficient calculation
- Add share leaderboards
- Create viral analytics dashboard
- Implement A/B testing for share variations

## 🔧 Viral Sharing Data Structures

```cpp
struct ProfileQRCode {
    std::string profileId;
    std::string qrCodeUrl;
    std::string qrCodeData; // URL encoded in QR
    QRCodeStyle style; // DEFAULT, CUSTOM, BRANDED
    std::string customLogo; // Optional logo in center
    Date generatedAt;
    int scanCount = 0;
    std::vector<QRCodeScan> scans;
};

struct QRCodeScan {
    std::string scanId;
    std::string qrCodeId;
    std::string scannerId; // null if anonymous
    std::string location; // Geographic location
    std::string device; // Device type
    Date scannedAt;
    bool convertedToView = false;
    bool convertedToSignup = false;
};

struct ProfileShare {
    std::string id;
    std::string profileId;
    std::string sharerId; // User who shared
    SharePlatform platform; // TELEGRAM, WHATSAPP, INSTAGRAM, FACEBOOK, TWITTER, LINKEDIN, EMAIL, SMS, DIRECT_LINK
    std::string shareUrl; // Tracking URL
    std::string customMessage; // Custom share message
    Date sharedAt;
    int clickCount = 0;
    int viewCount = 0;
    int signupCount = 0;
    double conversionRate = 0.0;
    std::vector<ShareClick> clicks;
};

struct ShareClick {
    std::string clickId;
    std::string shareId;
    std::string clickerIP;
    std::string referrer;
    std::string userAgent;
    Date clickedAt;
    bool convertedToView = false;
    bool convertedToSignup = false;
};

struct ReferralCode {
    std::string code; // Unique referral code
    std::string profileId; // Profile that owns the code
    std::string customSlug; // Custom vanity code (optional)
    int useCount = 0;
    int signupCount = 0;
    Date createdAt;
    Date expiresAt; // Optional expiration
    bool isActive = true;
    ReferralRewards rewards;
};

struct ReferralSignup {
    std::string id;
    std::string referralCode;
    std::string referrerId; // Profile that referred
    std::string referredUserId; // New user that signed up
    Date signupDate;
    bool rewardClaimed = false;
    ReferralRewardType rewardType;
    Date rewardClaimedAt;
};

struct ShareLandingPage {
    std::string id;
    std::string profileId;
    std::string trackingCode;
    LandingPageVariant variant; // A/B test variants
    std::string customHeadline;
    std::string customDescription;
    std::string customCTA;
    std::string backgroundImage;
    int viewCount = 0;
    int conversionCount = 0;
    double conversionRate = 0.0;
};

struct ViralMetrics {
    std::string profileId;
    int totalShares = 0;
    int totalShareClicks = 0;
    int totalReferrals = 0;
    int totalReferralSignups = 0;
    double viralCoefficient = 0.0; // K-factor
    double shareConversionRate = 0.0;
    double referralConversionRate = 0.0;
    std::map<SharePlatform, int> sharesByPlatform;
    Date lastCalculated;
};
```

## 📱 QR Code Generation

### QR Code Features
- **Profile QR codes**: Unique QR code for each profile
- **Custom styling**: Branded QR codes with logos
- **High resolution**: Print-quality QR codes
- **Dynamic QR codes**: Update destination without changing code
- **Scan tracking**: Track QR code scans
- **Analytics**: Scan location, device, conversion

### QR Code Use Cases
- **Business cards**: Print QR on business cards
- **Marketing materials**: Add to flyers, posters, brochures
- **Event badges**: Conference and event badges
- **Shop displays**: Physical store displays
- **Offline to online**: Bridge offline and online presence

### QR Code Customization
- **Colors**: Custom color schemes
- **Logos**: Center logo/brand image
- **Shapes**: Custom corner shapes
- **Patterns**: Different dot patterns
- **Call-to-action**: Text around QR code

## 📢 Social Media Sharing

### Supported Platforms

#### Persian Platforms (Priority)
- **Telegram**: Optimized for Telegram sharing
- **Instagram**: Instagram story/post integration
- **WhatsApp**: WhatsApp message sharing
- **Soroush**: Iranian messaging app
- **Rubika**: Iranian messaging app

#### International Platforms
- **Facebook**: Facebook post sharing
- **Twitter/X**: Tweet sharing
- **LinkedIn**: Professional network sharing
- **Email**: Email sharing
- **SMS**: SMS sharing

### Share Optimization

#### Open Graph Meta Tags
```html
<meta property="og:title" content="[Profile Name] - Hatef.ir">
<meta property="og:description" content="[Profile Bio]">
<meta property="og:image" content="[Profile Image]">
<meta property="og:url" content="[Tracking URL]">
<meta property="og:type" content="profile">
```

#### Persian Language Optimization
- RTL text support in share previews
- Persian character encoding
- Persian social platform optimizations
- Local image hosting for faster loads

#### Share Templates
- **Personal profile**: "Check out my profile on Hatef.ir"
- **Business profile**: "Discover [Business Name] on Hatef.ir"
- **Custom message**: User-defined share message
- **Call-to-action**: Strong CTA in share text

## 🎁 Referral Program

### Referral Mechanics

#### Referral Codes
- **Unique codes**: Each profile gets unique code
- **Custom slugs**: Vanity URLs (hatef.ir/ref/custom-name)
- **Multiple codes**: Create multiple codes for campaigns
- **Expiration**: Optional time-limited codes
- **Usage limits**: Optional usage limits

#### Referral Tracking
- **Click tracking**: Track referral link clicks
- **Signup attribution**: Attribute signups to referrer
- **Conversion tracking**: Track referred user activity
- **Multi-touch attribution**: Credit multiple touchpoints

### Referral Rewards

#### Reward Types

**For Referrer (Person who shares):**
- **Visibility Boost**: +10% search ranking for 7 days
- **Feature Badge**: "Top Referrer" badge on profile
- **Priority Support**: Priority customer support
- **Analytics Access**: Advanced analytics unlock
- **Profile Highlights**: Featured in discovery section

**For Referee (New user):**
- **Welcome Badge**: "Referred by [Name]" badge
- **Profile Boost**: Initial visibility boost
- **Premium Trial**: 30-day premium trial
- **Tutorial Access**: Onboarding tutorial

#### Tiered Rewards
- **Bronze**: 5 referrals → Basic badge
- **Silver**: 25 referrals → Profile boost + badge
- **Gold**: 100 referrals → Featured profile + analytics
- **Platinum**: 500 referrals → Lifetime benefits
- **Diamond**: 1000+ referrals → Special recognition

### Referral Leaderboards
- **Top Referrers**: Most referrals all-time
- **Monthly Leaders**: Top referrers this month
- **Industry Leaders**: Top referrers by industry
- **Location Leaders**: Top referrers by location
- **Rising Stars**: Fastest growing referrers

## 🌐 Custom Landing Pages

### Landing Page Features

#### Dynamic Content
- **Personalized greeting**: "You've been invited by [Name]"
- **Referrer info**: Show referrer profile preview
- **Social proof**: Show platform metrics
- **Testimonials**: Show user testimonials
- **Trust badges**: Verification and trust indicators

#### A/B Testing Variants
- **Headline variations**: Test different headlines
- **CTA variations**: Test different call-to-actions
- **Design variations**: Test different layouts
- **Message variations**: Test different value propositions

#### Persian Optimization
- **RTL layout**: Right-to-left for Persian
- **Persian fonts**: Optimized Persian typography
- **Cultural elements**: Persian cultural design elements
- **Local testimonials**: Persian user testimonials

### Landing Page Templates
- **Personal invite**: For personal profile shares
- **Business invite**: For business profile shares
- **Event invite**: For event-related shares
- **Job invite**: For job posting shares
- **Custom**: Fully customizable templates

## 📊 Viral Analytics

### Viral Coefficient (K-Factor)

#### Calculation
```cpp
viralCoefficient = (totalInvitesSent / totalUsers) * (signupConversionRate)

// Example:
// 100 users send 300 invites (3 per user)
// 60 signups from invites (20% conversion)
// K = 3 * 0.20 = 0.6

// K > 1.0 = Viral growth (exponential)
// K = 1.0 = Sustainable growth (linear)
// K < 1.0 = Requires paid acquisition
```

#### Viral Loop Optimization
- **Invite rate**: Increase invites sent per user
- **Conversion rate**: Improve signup conversion
- **Activation rate**: Get users to invite others
- **Cycle time**: Reduce time to next invite

### Share Analytics

#### Share Performance Metrics
- **Total shares**: All-time shares
- **Share rate**: Shares per profile view
- **Click-through rate**: Share clicks / shares
- **Conversion rate**: Signups / share clicks
- **Platform performance**: Best performing platforms
- **Time analysis**: Best time to share
- **Geographic analysis**: Where shares perform best

#### Referral Analytics
- **Total referrals**: All-time referrals
- **Referral rate**: Referrals per user
- **Signup rate**: Signups / referrals
- **Activation rate**: Active users / signups
- **Revenue attribution**: Revenue from referrals
- **Lifetime value**: LTV of referred users

### Viral Leaderboards

#### Share Leaderboards
- **Most shared profiles**: Profiles with most shares
- **Most viral profiles**: Highest viral coefficient
- **Trending shares**: Profiles trending in shares
- **Platform leaders**: Best performing on each platform

#### Referral Leaderboards
- **Top referrers**: Most referrals
- **Best converters**: Highest conversion rate
- **Most valuable**: Highest LTV of referrals
- **Most engaged**: Referrals with highest engagement

## 🧪 Testing Strategy

### QR Code Tests
```cpp
TEST(QRCodeTest, GenerateProfileQRCode) {
    auto profile = createTestProfile();
    auto qrCode = generateQRCode(profile.id);
    EXPECT_FALSE(qrCode.qrCodeUrl.empty());
    EXPECT_FALSE(qrCode.qrCodeData.empty());
    EXPECT_TRUE(isValidQRCode(qrCode.qrCodeUrl));
}

TEST(QRCodeTest, TrackQRCodeScan) {
    auto qrCode = createTestQRCode();
    auto scan = trackScan(qrCode.id, "192.168.1.1", "iPhone");
    EXPECT_EQ(getQRCodeScanCount(qrCode.id), 1);
}
```

### Share Tracking Tests
```cpp
TEST(ShareTest, TrackProfileShare) {
    ProfileShare share{
        .profileId = "profile123",
        .sharerId = "user456",
        .platform = SharePlatform::TELEGRAM,
        .shareUrl = generateShareUrl("profile123", "user456")
    };
    EXPECT_TRUE(trackShare(share));
    EXPECT_FALSE(share.shareUrl.empty());
}

TEST(ShareTest, CalculateShareConversion) {
    auto share = createTestShare();
    trackClick(share.id);
    trackClick(share.id);
    trackSignup(share.id);
    auto conversion = calculateConversion(share.id);
    EXPECT_EQ(conversion, 0.5); // 1 signup / 2 clicks
}
```

### Referral Tests
```cpp
TEST(ReferralTest, GenerateReferralCode) {
    auto code = generateReferralCode("profile123");
    EXPECT_FALSE(code.code.empty());
    EXPECT_EQ(code.profileId, "profile123");
    EXPECT_TRUE(code.isActive);
}

TEST(ReferralTest, TrackReferralSignup) {
    auto code = createTestReferralCode();
    auto signup = trackReferralSignup(code.code, "newuser789");
    EXPECT_TRUE(signup.has_value());
    EXPECT_EQ(getReferralSignupCount(code.code), 1);
}
```

### Viral Coefficient Tests
```cpp
TEST(ViralTest, CalculateViralCoefficient) {
    ViralMetrics metrics{
        .totalShares = 300,
        .totalShareClicks = 150,
        .totalReferralSignups = 60
    };
    int totalUsers = 100;
    double k = calculateViralCoefficient(metrics, totalUsers);
    EXPECT_GT(k, 0.0);
    EXPECT_LT(k, 10.0);
}
```

### Integration Tests
```bash
# Test QR code generation
curl -X POST http://localhost:3000/api/profiles/qr-code \
  -H "Authorization: Bearer token" \
  -d '{"profileId":"profile123","style":"CUSTOM","logo":"logo.png"}'

# Test share tracking
curl -X POST http://localhost:3000/api/profiles/share \
  -H "Authorization: Bearer token" \
  -d '{"profileId":"profile123","platform":"TELEGRAM"}'

# Test referral code generation
curl -X POST http://localhost:3000/api/profiles/referral-code \
  -H "Authorization: Bearer token" \
  -d '{"profileId":"profile123","customSlug":"my-code"}'

# Test viral analytics
curl http://localhost:3000/api/profiles/viral-analytics \
  -H "Authorization: Bearer token"
```

## 🎨 User Interface

### Share Modal
- **Platform selection**: Choose platform to share
- **Custom message**: Edit share message
- **Preview**: Preview share appearance
- **Copy link**: Copy tracking link
- **Share button**: Direct share to platform

### QR Code Display
- **QR code image**: Display generated QR code
- **Download button**: Download high-res QR code
- **Print button**: Print-optimized QR code
- **Customize button**: Customize QR appearance
- **Scan stats**: Show scan analytics

### Referral Dashboard
- **Referral code**: Display referral code
- **Share buttons**: Quick share buttons
- **Referral stats**: Total referrals and conversions
- **Leaderboard position**: Current ranking
- **Rewards earned**: Badges and benefits earned
- **Referral history**: List of referred users

### Landing Page Builder
- **Template selection**: Choose landing page template
- **Content editor**: Edit landing page content
- **Preview**: Live preview of landing page
- **A/B testing**: Set up A/B test variants
- **Analytics**: Landing page performance

## 🎉 Success Criteria
- QR codes generate in <1 second
- Share tracking accuracy >99%
- Referral attribution accuracy >95%
- Landing page loads in <1 second
- Viral coefficient calculation updates in real-time
- Share buttons work on all major platforms
- Persian platforms (Telegram, WhatsApp) integration works perfectly
- Referral rewards distribute automatically
- Leaderboards update every 5 minutes
- System handles 10K+ shares per day
- A/B testing provides statistically significant results

## 📊 Expected Impact

### Viral Growth Metrics
- **Viral coefficient**: Target K = 1.2-1.5
- **Share rate**: 5-10% of profile views result in shares
- **Referral conversion**: 15-25% of referrals convert to signups
- **Organic growth**: 30-50% of new signups from referrals
- **Network effects**: Exponential growth from viral loops

### User Engagement
- **Sharing activity**: 20-30% of users share profiles
- **QR code usage**: 10-15% of users generate QR codes
- **Referral participation**: 15-25% of users use referral codes
- **Social amplification**: 3-5x reach through shares
- **Return on shares**: 2-3 new users per active sharer

### Business Value
- **Customer acquisition cost**: Reduce CAC by 50-70%
- **Organic growth**: 30-50% organic user acquisition
- **User quality**: Referred users have 2x retention rate
- **Network density**: Stronger network through referrals
- **Platform growth**: Exponential growth trajectory

### Platform Metrics
- **Daily shares**: 50K-100K shares per day
- **Monthly referrals**: 10K-20K referrals per month
- **QR code scans**: 5K-10K scans per day
- **Viral reach**: 500K-1M people reached through shares
- **Growth rate**: 20-30% monthly user growth from virality

## 🚀 Viral Growth Strategy

### Phase 1: Launch (Week 1-2)
- Deploy QR code generation
- Launch basic sharing features
- Enable referral tracking
- Create initial rewards

**Target:** 1-2% of users start sharing

### Phase 2: Incentivize (Week 3-4)
- Launch referral reward program
- Add share leaderboards
- Create share campaigns
- Optimize share conversion

**Target:** 5-10% of users actively sharing

### Phase 3: Optimize (Week 5-8)
- A/B test landing pages
- Optimize viral coefficient
- Improve conversion rates
- Expand reward tiers

**Target:** K-factor reaches 1.0+

### Phase 4: Scale (Week 9+)
- Full viral loop optimization
- Automated viral campaigns
- Advanced analytics
- Platform-wide viral features

**Target:** Sustainable viral growth (K > 1.2)

## 🎯 Competitive Advantage

### vs. LinkedIn
- **Better sharing**: Optimized for Persian platforms
- **Stronger incentives**: Tangible rewards for referrals
- **Lower friction**: One-click sharing with QR codes
- **Higher conversion**: Custom landing pages optimize conversion

### vs. Linktree
- **Viral mechanics**: Built-in referral program
- **Rich profiles**: Full profile pages, not just links
- **Better tracking**: Comprehensive viral analytics
- **Persian optimization**: Native Persian platform integration

### vs. Google Business
- **Social sharing**: Strong social sharing features
- **Referral rewards**: Incentivized growth
- **QR codes**: Easy offline-to-online bridge
- **Viral growth**: Exponential growth mechanisms

## 💡 Growth Hacks

### Offline-to-Online Bridge
- QR codes on business cards
- QR codes at events and conferences
- QR codes in physical stores
- QR codes on marketing materials

### Social Proof Amplification
- Show "X people viewed this profile today"
- Display "Shared X times"
- Show referral success stories
- Highlight top referrers

### Incentive Optimization
- Limited-time referral bonuses
- Milestone celebrations (100th referral!)
- Referral contests and competitions
- Exclusive benefits for top referrers

### Viral Content Strategy
- Make profiles "brag-worthy"
- Add social proof indicators
- Create shareable achievements
- Generate "share-worthy" moments


