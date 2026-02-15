#include <gtest/gtest.h>
#include "../../include/search_engine/seo/SitemapGenerator.h"
#include "../../include/search_engine/storage/Profile.h"

using namespace search_engine::seo;
using namespace search_engine::storage;

class SitemapGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        baseUrl = "https://hatef.ir";
    }

    std::string baseUrl;
};

TEST_F(SitemapGeneratorTest, GenerateEmptyProfilesSitemap) {
    std::vector<Profile> profiles;
    
    auto sitemap = SitemapGenerator::generateProfilesSitemap(profiles, baseUrl);
    
    // Should still have XML declaration and urlset
    EXPECT_NE(sitemap.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"), std::string::npos);
    EXPECT_NE(sitemap.find("<urlset"), std::string::npos);
    EXPECT_NE(sitemap.find("</urlset>"), std::string::npos);
}

TEST_F(SitemapGeneratorTest, GenerateProfilesSitemapWithOneProfile) {
    std::vector<Profile> profiles;
    
    Profile profile;
    profile.slug = "john-doe";
    profile.name = "John Doe";
    profile.type = ProfileType::PERSON;
    profile.isPublic = true;
    profile.createdAt = std::chrono::system_clock::now();
    
    profiles.push_back(profile);
    
    auto sitemap = SitemapGenerator::generateProfilesSitemap(profiles, baseUrl);
    
    // Check for profile URL
    EXPECT_NE(sitemap.find("<loc>" + baseUrl + "/john-doe</loc>"), std::string::npos);
    EXPECT_NE(sitemap.find("<url>"), std::string::npos);
    EXPECT_NE(sitemap.find("<changefreq>"), std::string::npos);
    EXPECT_NE(sitemap.find("<priority>"), std::string::npos);
}

TEST_F(SitemapGeneratorTest, GenerateProfilesSitemapWithMultipleProfiles) {
    std::vector<Profile> profiles;
    
    for (int i = 0; i < 5; i++) {
        Profile profile;
        profile.slug = "profile-" + std::to_string(i);
        profile.name = "Profile " + std::to_string(i);
        profile.type = ProfileType::PERSON;
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();
        profiles.push_back(profile);
    }
    
    auto sitemap = SitemapGenerator::generateProfilesSitemap(profiles, baseUrl);
    
    // Check all profiles are included
    for (int i = 0; i < 5; i++) {
        std::string expectedUrl = "<loc>" + baseUrl + "/profile-" + std::to_string(i) + "</loc>";
        EXPECT_NE(sitemap.find(expectedUrl), std::string::npos);
    }
}

TEST_F(SitemapGeneratorTest, ProfilesSitemapSkipsPrivateProfiles) {
    std::vector<Profile> profiles;
    
    // Public profile
    Profile publicProfile;
    publicProfile.slug = "public-profile";
    publicProfile.name = "Public Profile";
    publicProfile.type = ProfileType::PERSON;
    publicProfile.isPublic = true;
    publicProfile.createdAt = std::chrono::system_clock::now();
    profiles.push_back(publicProfile);
    
    // Private profile
    Profile privateProfile;
    privateProfile.slug = "private-profile";
    privateProfile.name = "Private Profile";
    privateProfile.type = ProfileType::PERSON;
    privateProfile.isPublic = false;
    privateProfile.createdAt = std::chrono::system_clock::now();
    profiles.push_back(privateProfile);
    
    auto sitemap = SitemapGenerator::generateProfilesSitemap(profiles, baseUrl);
    
    // Public profile should be included
    EXPECT_NE(sitemap.find("/public-profile"), std::string::npos);
    
    // Private profile should NOT be included
    EXPECT_EQ(sitemap.find("/private-profile"), std::string::npos);
}

TEST_F(SitemapGeneratorTest, ProfilesSitemapIncludesLastModDate) {
    std::vector<Profile> profiles;
    
    Profile profile;
    profile.slug = "john-doe";
    profile.name = "John Doe";
    profile.type = ProfileType::PERSON;
    profile.isPublic = true;
    profile.createdAt = std::chrono::system_clock::now();
    profile.updatedAt = std::chrono::system_clock::now();
    
    profiles.push_back(profile);
    
    auto sitemap = SitemapGenerator::generateProfilesSitemap(profiles, baseUrl);
    
    // Should include lastmod tag
    EXPECT_NE(sitemap.find("<lastmod>"), std::string::npos);
}

TEST_F(SitemapGeneratorTest, GenerateSitemapIndex) {
    int totalProfiles = 150000; // More than 50000
    
    auto sitemapIndex = SitemapGenerator::generateSitemapIndex(totalProfiles, baseUrl);
    
    // Should be sitemap index
    EXPECT_NE(sitemapIndex.find("<sitemapindex"), std::string::npos);
    EXPECT_NE(sitemapIndex.find("</sitemapindex>"), std::string::npos);
    
    // Should include static sitemap
    EXPECT_NE(sitemapIndex.find("/sitemap-static.xml"), std::string::npos);
    
    // Should include profile sitemaps (150000 / 50000 = 3 sitemaps)
    EXPECT_NE(sitemapIndex.find("/sitemap-profiles-1.xml"), std::string::npos);
    EXPECT_NE(sitemapIndex.find("/sitemap-profiles-2.xml"), std::string::npos);
    EXPECT_NE(sitemapIndex.find("/sitemap-profiles-3.xml"), std::string::npos);
}

TEST_F(SitemapGeneratorTest, GenerateSitemapIndexSmallCount) {
    int totalProfiles = 100; // Less than 50000
    
    auto sitemapIndex = SitemapGenerator::generateSitemapIndex(totalProfiles, baseUrl);
    
    // Should still generate proper structure
    EXPECT_NE(sitemapIndex.find("<sitemapindex"), std::string::npos);
    EXPECT_NE(sitemapIndex.find("/sitemap-profiles-1.xml"), std::string::npos);
}

TEST_F(SitemapGeneratorTest, GenerateStaticPagesSitemap) {
    auto sitemap = SitemapGenerator::generateStaticPagesSitemap(baseUrl);
    
    // Should have XML structure
    EXPECT_NE(sitemap.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"), std::string::npos);
    EXPECT_NE(sitemap.find("<urlset"), std::string::npos);
    
    // Should include common static pages
    EXPECT_NE(sitemap.find(baseUrl + "/"), std::string::npos); // Home
    EXPECT_NE(sitemap.find(baseUrl + "/search"), std::string::npos); // Search
    EXPECT_NE(sitemap.find(baseUrl + "/sponsor"), std::string::npos); // Sponsor
    
    // Should have priorities
    EXPECT_NE(sitemap.find("<priority>"), std::string::npos);
}

TEST_F(SitemapGeneratorTest, XmlEscaping) {
    std::vector<Profile> profiles;
    
    Profile profile;
    profile.slug = "test-&-special<>chars";
    profile.name = "Test Profile";
    profile.type = ProfileType::PERSON;
    profile.isPublic = true;
    profile.createdAt = std::chrono::system_clock::now();
    
    profiles.push_back(profile);
    
    auto sitemap = SitemapGenerator::generateProfilesSitemap(profiles, baseUrl);
    
    // Special characters should be escaped
    EXPECT_NE(sitemap.find("&amp;"), std::string::npos);
    EXPECT_NE(sitemap.find("&lt;"), std::string::npos);
    EXPECT_NE(sitemap.find("&gt;"), std::string::npos);
    
    // Raw characters should not appear in URLs
    EXPECT_EQ(sitemap.find("<loc>" + baseUrl + "/test-&-special<>chars</loc>"), std::string::npos);
}

TEST_F(SitemapGeneratorTest, ValidXmlStructure) {
    std::vector<Profile> profiles;
    
    Profile profile;
    profile.slug = "test";
    profile.name = "Test";
    profile.type = ProfileType::PERSON;
    profile.isPublic = true;
    profile.createdAt = std::chrono::system_clock::now();
    profiles.push_back(profile);
    
    auto sitemap = SitemapGenerator::generateProfilesSitemap(profiles, baseUrl);
    
    // Check proper XML structure
    EXPECT_EQ(sitemap.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"), 0);
    EXPECT_NE(sitemap.find("xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\""), std::string::npos);
    
    // Ensure tags are properly closed
    size_t urlsetOpen = sitemap.find("<urlset");
    size_t urlsetClose = sitemap.find("</urlset>");
    EXPECT_LT(urlsetOpen, urlsetClose);
}

// Main function only for standalone test execution
#ifndef SEO_TESTS_COMBINED
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
