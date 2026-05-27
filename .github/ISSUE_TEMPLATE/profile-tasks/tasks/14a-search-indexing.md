# 🚀 Search Integration - Profile Indexing

**Duration:** 2 days
**Dependencies:** Profile database models, Search engine core
**Acceptance Criteria:**
- ✅ Profile indexing system integrated with search engine
- ✅ Profile metadata extraction and storage
- ✅ Search ranking factors implemented
- ✅ Profile search visibility controls
- ✅ Index update triggers for profile changes
- ✅ Search schema optimized for profile data
- ✅ Index performance monitoring

## 🎯 Task Description

Integrate profiles with the search engine's indexing system. This task focuses on making profiles discoverable through search while implementing proper ranking factors and metadata extraction.

## 🔍 Profile Search Index Structure

### Search Document Schema
```cpp
struct ProfileSearchDocument {
    std::string profileId;
    std::string slug;
    ProfileType type;

    // Core search fields
    std::string title;           // Profile name or company name
    std::string description;     // Bio or company description
    std::string keywords;        // Combined searchable text

    // Location data (for geo search)
    std::string city;
    std::string province;
    std::string country;

    // Profile type specific data
    PersonSearchData personData;     // Skills, job title, etc.
    BusinessSearchData businessData; // Industry, services, etc.

    // Search ranking factors
    double searchScore;          // Base relevance score
    int profileCompleteness;     // 0-100, affects ranking
    Date lastUpdated;           // Recency boost
    bool isVerified;            // Verification boost
    int viewCount;              // Popularity indicator

    // Search metadata
    Date indexedAt;
    std::string indexVersion;
};
```

### Person Profile Search Data
```cpp
struct PersonSearchData {
    std::string jobTitle;
    std::string company;
    std::vector<std::string> skills;
    std::string experienceLevel;
    std::string education;
    std::string industry;        // Derived from company/industry data
};
```

### Business Profile Search Data
```cpp
struct BusinessSearchData {
    std::string companyName;
    std::string industry;
    std::string companySize;
    std::vector<std::string> services;
    std::string businessType;    // Local, national, international
};
```

## 📊 Search Ranking Algorithm

### Ranking Factors (Weighted)
```cpp
class ProfileSearchRanker {
public:
    static double calculateSearchScore(const ProfileSearchDocument& doc,
                                     const std::string& query);

private:
    // Weight constants
    static constexpr double TITLE_MATCH_WEIGHT = 1.0;
    static constexpr double DESCRIPTION_MATCH_WEIGHT = 0.8;
    static constexpr double SKILLS_MATCH_WEIGHT = 0.9;
    static constexpr double LOCATION_MATCH_WEIGHT = 0.6;
    static constexpr double COMPLETENESS_WEIGHT = 0.3;
    static constexpr double VERIFICATION_WEIGHT = 0.4;
    static constexpr double RECENCY_WEIGHT = 0.2;
    static constexpr double POPULARITY_WEIGHT = 0.1;

    static double calculateTextRelevance(const std::string& text, const std::string& query);
    static double calculateCompletenessScore(const ProfileSearchDocument& doc);
    static double calculateRecencyScore(Date lastUpdated);
    static double calculatePopularityScore(int viewCount);
};
```

### Ranking Calculation
```cpp
double ProfileSearchRanker::calculateSearchScore(const ProfileSearchDocument& doc,
                                                const std::string& query) {
    double score = 0.0;

    // Text relevance (primary factor)
    score += TITLE_MATCH_WEIGHT * calculateTextRelevance(doc.title, query);
    score += DESCRIPTION_MATCH_WEIGHT * calculateTextRelevance(doc.description, query);
    score += KEYWORDS_MATCH_WEIGHT * calculateTextRelevance(doc.keywords, query);

    // Profile-specific relevance
    if (doc.type == ProfileType::PERSON) {
        score += SKILLS_MATCH_WEIGHT * calculateSkillsMatch(doc.personData.skills, query);
    } else {
        score += SERVICES_MATCH_WEIGHT * calculateServicesMatch(doc.businessData.services, query);
    }

    // Quality factors
    score += COMPLETENESS_WEIGHT * calculateCompletenessScore(doc);
    score += VERIFICATION_WEIGHT * (doc.isVerified ? 1.0 : 0.0);
    score += RECENCY_WEIGHT * calculateRecencyScore(doc.lastUpdated);
    score += POPULARITY_WEIGHT * calculatePopularityScore(doc.viewCount);

    // Location boost (if query includes location)
    if (queryContainsLocation(query, doc)) {
        score += LOCATION_MATCH_WEIGHT;
    }

    return score;
}
```

## 🔄 Index Update System

### Profile Change Triggers
```cpp
class ProfileIndexUpdater {
public:
    static void onProfileCreated(const Profile& profile);
    static void onProfileUpdated(const Profile& oldProfile, const Profile& newProfile);
    static void onProfileDeleted(const std::string& profileId);

    static void onPersonProfileUpdated(const PersonProfile& oldProfile,
                                     const PersonProfile& newProfile);
    static void onBusinessProfileUpdated(const BusinessProfile& oldProfile,
                                       const BusinessProfile& newProfile);

private:
    static void updateSearchIndex(const std::string& profileId);
    static void removeFromSearchIndex(const std::string& profileId);
    static ProfileSearchDocument buildSearchDocument(const Profile& profile);
};
```

### Automatic Index Updates
```cpp
// Profile creation trigger
void ProfileService::createProfile(const Profile& profile) {
    // Save to database
    std::string profileId = profileRepository.save(profile);

    // Update search index
    ProfileIndexUpdater::onProfileCreated(profile);

    // Audit log
    AuditLogger::logProfileCreate(profile, userId);

    return profileId;
}

// Profile update trigger
void ProfileService::updateProfile(const std::string& profileId,
                                 const Profile& updates) {
    // Get old profile
    auto oldProfile = profileRepository.findById(profileId);
    if (!oldProfile) {
        throw ProfileNotFoundException(profileId);
    }

    // Update database
    profileRepository.update(profileId, updates);

    // Update search index
    ProfileIndexUpdater::onProfileUpdated(*oldProfile, updates);

    // Audit log
    AuditLogger::logProfileUpdate(*oldProfile, updates, userId);
}
```

## 📈 Profile Completeness Scoring

### Completeness Factors
```cpp
struct ProfileCompleteness {
    // Common fields (weight: 40%)
    bool hasName = false;        // 10%
    bool hasBio = false;         // 15%
    bool hasAvatar = false;      // 10%
    bool hasPublic = false;      // 5%

    // Person-specific fields (weight: 35%)
    bool hasTitle = false;       // 10%
    bool hasSkills = false;      // 15%
    bool hasExperience = false;  // 10%

    // Business-specific fields (weight: 35%)
    bool hasCompanyName = false; // 10%
    bool hasIndustry = false;    // 10%
    bool hasServices = false;    // 10%
    bool hasLocation = false;    // 5%

    // Bonus fields (weight: 25%)
    bool hasVerification = false; // 10%
    bool hasWebsite = false;      // 5%
    bool hasSocialLinks = false;  // 5%
    bool hasContactInfo = false;  // 5%
};

class CompletenessCalculator {
public:
    static int calculateCompletenessScore(const Profile& profile);
    static ProfileCompleteness analyzeCompleteness(const Profile& profile);

private:
    static int calculateCommonCompleteness(const Profile& profile);
    static int calculatePersonCompleteness(const PersonProfile& profile);
    static int calculateBusinessCompleteness(const BusinessProfile& profile);
};
```

## 🔍 Search Visibility Controls

### Profile Search Settings
```cpp
struct ProfileSearchSettings {
    bool isSearchable = true;        // Profile appears in search
    bool showInDirectory = true;     // Appears in profile directory
    bool allowContactRequests = true; // Can be contacted through search

    // Advanced controls
    std::vector<std::string> blockedKeywords;  // Don't appear for these searches
    std::vector<std::string> boostedKeywords;  // Extra ranking for these terms

    // Privacy controls
    bool hideFromAnonymousUsers = false;  // Only logged-in users can find
    bool requireContactApproval = false;  // Manual approval for contacts
};

class ProfileSearchController {
public:
    static bool isProfileSearchable(const Profile& profile);
    static bool shouldShowInResults(const Profile& profile, const SearchQuery& query);
    static void applySearchSettings(ProfileSearchDocument& doc, const ProfileSearchSettings& settings);
};
```

## 📋 Implementation Plan

### Day 1: Search Document Schema + Indexing
- Design ProfileSearchDocument schema
- Implement profile data extraction for search
- Create search index update system
- Add profile change triggers
- Test basic indexing functionality

### Day 2: Ranking Algorithm + Completeness
- Implement search ranking algorithm
- Add profile completeness scoring
- Create search visibility controls
- Optimize search schema for performance
- Performance testing and monitoring

## 🧪 Testing Strategy

### Index Tests
```cpp
TEST(SearchIndexTest, ProfileIndexing) {
    // Create test profile
    auto profile = createTestProfile();

    // Index profile
    ProfileIndexUpdater::onProfileCreated(profile);

    // Verify indexed
    auto searchDoc = searchIndex.findById(profile.id);
    EXPECT_TRUE(searchDoc.has_value());
    EXPECT_EQ(searchDoc->title, profile.name);
}

TEST(SearchIndexTest, ProfileUpdatesIndexed) {
    // Create and index profile
    auto profile = createTestProfile();
    ProfileIndexUpdater::onProfileCreated(profile);

    // Update profile
    PersonProfile updatedProfile = profile;
    updatedProfile.title = "Senior Engineer";
    ProfileIndexUpdater::onPersonProfileUpdated(profile, updatedProfile);

    // Verify index updated
    auto searchDoc = searchIndex.findById(profile.id);
    EXPECT_EQ(searchDoc->personData.jobTitle, "Senior Engineer");
}
```

### Ranking Tests
```cpp
TEST(SearchRankingTest, CompletenessAffectsRanking) {
    // Create complete profile
    auto completeProfile = createCompleteProfile();
    double completeScore = ProfileSearchRanker::calculateSearchScore(
        buildSearchDoc(completeProfile), "engineer");

    // Create incomplete profile
    auto incompleteProfile = createIncompleteProfile();
    double incompleteScore = ProfileSearchRanker::calculateSearchScore(
        buildSearchDoc(incompleteProfile), "engineer");

    // Complete profile should rank higher
    EXPECT_GT(completeScore, incompleteScore);
}

TEST(SearchRankingTest, VerificationBoost) {
    auto unverifiedProfile = createProfile(false);  // not verified
    auto verifiedProfile = createProfile(true);     // verified

    double unverifiedScore = ProfileSearchRanker::calculateSearchScore(
        buildSearchDoc(unverifiedProfile), "developer");
    double verifiedScore = ProfileSearchRanker::calculateSearchScore(
        buildSearchDoc(verifiedProfile), "developer");

    EXPECT_GT(verifiedScore, unverifiedScore);
}
```

## 🎉 Success Criteria

### Search Integration
- ✅ **Profiles indexed in search engine**
- ✅ **Search documents contain all relevant metadata**
- ✅ **Index updates automatically on profile changes**
- ✅ **Search schema optimized for query performance**

### Ranking & Relevance
- ✅ **Ranking algorithm implemented**
- ✅ **Profile completeness affects search ranking**
- ✅ **Verification status boosts ranking**
- ✅ **Location and recency factors included**

### Performance
- ✅ **Index update latency < 100ms**
- ✅ **Search queries < 50ms average**
- ✅ **Index size optimized**
- ✅ **Memory usage within limits**

### Control & Privacy
- ✅ **Profile search visibility controls implemented**
- ✅ **Privacy settings respected in search results**
- ✅ **Search blocking and boosting keywords work**

This lays the foundation for **search-driven profile discovery** while ensuring **relevant and high-quality results**.
