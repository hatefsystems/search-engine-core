#include <gtest/gtest.h>
#include "../../include/search_engine/seo/SEOGenerator.h"
#include "../../include/search_engine/storage/Profile.h"
#include <nlohmann/json.hpp>

using namespace search_engine::seo;
using namespace search_engine::storage;

class SEOGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        baseUrl = "https://hatef.ir";
    }

    std::string baseUrl;
};

TEST_F(SEOGeneratorTest, PersonSchemaGenerationBasic) {
    // Create a basic person profile
    PersonProfile profile;
    profile.id = "test123";
    profile.slug = "john-doe";
    profile.name = "John Doe";
    profile.type = ProfileType::PERSON;
    profile.bio = "Software Engineer with 10 years of experience";
    profile.title = "Senior Software Engineer";
    profile.company = "Tech Corp";
    profile.skills = {"C++", "Python", "JavaScript"};
    
    // Generate Person schema
    auto schema = SEOGenerator::generatePersonSchema(profile, baseUrl);
    
    // Verify required fields
    EXPECT_EQ(schema["@context"], "https://schema.org");
    EXPECT_EQ(schema["@type"], "Person");
    EXPECT_EQ(schema["name"], "John Doe");
    EXPECT_EQ(schema["url"], baseUrl + "/john-doe");
    EXPECT_EQ(schema["description"], profile.bio.value());
    
    // Verify skills
    ASSERT_TRUE(schema.contains("knowsAbout"));
    EXPECT_EQ(schema["knowsAbout"].size(), 3);
    
    // Verify occupation
    ASSERT_TRUE(schema.contains("hasOccupation"));
    EXPECT_EQ(schema["hasOccupation"]["@type"], "Occupation");
    EXPECT_EQ(schema["hasOccupation"]["name"], "Senior Software Engineer");
}

TEST_F(SEOGeneratorTest, PersonSchemaWithSocialLinks) {
    PersonProfile profile;
    profile.slug = "jane-smith";
    profile.name = "Jane Smith";
    profile.type = ProfileType::PERSON;
    
    // Create link blocks with social media URLs
    nlohmann::json linkBlocks = nlohmann::json::array();
    linkBlocks.push_back({
        {"url", "https://github.com/janesmith"},
        {"title", "GitHub"}
    });
    linkBlocks.push_back({
        {"url", "https://linkedin.com/in/janesmith"},
        {"title", "LinkedIn"}
    });
    linkBlocks.push_back({
        {"url", "https://example.com/portfolio"},
        {"title", "Portfolio"}
    });
    
    auto schema = SEOGenerator::generatePersonSchema(profile, baseUrl, linkBlocks);
    
    // Verify sameAs contains only social media links
    ASSERT_TRUE(schema.contains("sameAs"));
    ASSERT_TRUE(schema["sameAs"].is_array());
    EXPECT_EQ(schema["sameAs"].size(), 2); // Only GitHub and LinkedIn
}

TEST_F(SEOGeneratorTest, OrganizationSchemaGenerationBasic) {
    BusinessProfile profile;
    profile.id = "biz123";
    profile.slug = "tech-startup";
    profile.name = "Tech Startup Inc";
    profile.type = ProfileType::BUSINESS;
    profile.companyName = "Tech Startup Inc";
    profile.description = "Leading technology company";
    profile.foundedYear = 2020;
    profile.industry = "Technology";
    profile.address = "123 Tech Street";
    profile.city = "San Francisco";
    profile.country = "USA";
    
    auto schema = SEOGenerator::generateOrganizationSchema(profile, baseUrl);
    
    // Verify required fields
    EXPECT_EQ(schema["@context"], "https://schema.org");
    EXPECT_EQ(schema["@type"], "LocalBusiness"); // Should be LocalBusiness because address is present
    EXPECT_EQ(schema["name"], "Tech Startup Inc");
    EXPECT_EQ(schema["url"], baseUrl + "/tech-startup");
    
    // Verify address
    ASSERT_TRUE(schema.contains("address"));
    EXPECT_EQ(schema["address"]["@type"], "PostalAddress");
    EXPECT_EQ(schema["address"]["streetAddress"], "123 Tech Street");
    EXPECT_EQ(schema["address"]["addressLocality"], "San Francisco");
    EXPECT_EQ(schema["address"]["addressCountry"], "USA");
    
    // Verify founding date
    EXPECT_EQ(schema["foundingDate"], "2020");
}

TEST_F(SEOGeneratorTest, OrganizationSchemaWithoutAddress) {
    BusinessProfile profile;
    profile.slug = "online-business";
    profile.name = "Online Business";
    profile.type = ProfileType::BUSINESS;
    profile.companyName = "Online Business LLC";
    
    auto schema = SEOGenerator::generateOrganizationSchema(profile, baseUrl);
    
    // Should be Organization (not LocalBusiness) when no address
    EXPECT_EQ(schema["@type"], "Organization");
}

TEST_F(SEOGeneratorTest, OpenGraphTagsGeneration) {
    Profile profile;
    profile.slug = "test-profile";
    profile.name = "Test Profile";
    profile.type = ProfileType::PERSON;
    profile.bio = "This is a test bio";
    
    auto tags = SEOGenerator::generateOpenGraphTags(profile, baseUrl, "person");
    
    // Verify it's an array
    ASSERT_TRUE(tags.is_array());
    EXPECT_GT(tags.size(), 0);
    
    // Check for required OG tags
    bool hasTitle = false;
    bool hasUrl = false;
    bool hasDescription = false;
    bool hasType = false;
    
    for (const auto& tag : tags) {
        ASSERT_TRUE(tag.contains("property"));
        ASSERT_TRUE(tag.contains("content"));
        
        std::string property = tag["property"];
        if (property == "og:title") hasTitle = true;
        if (property == "og:url") hasUrl = true;
        if (property == "og:description") hasDescription = true;
        if (property == "og:type") hasType = true;
    }
    
    EXPECT_TRUE(hasTitle);
    EXPECT_TRUE(hasUrl);
    EXPECT_TRUE(hasDescription);
    EXPECT_TRUE(hasType);
}

TEST_F(SEOGeneratorTest, TwitterCardTagsGeneration) {
    Profile profile;
    profile.slug = "test-profile";
    profile.name = "Test Profile";
    profile.type = ProfileType::PERSON;
    profile.bio = "This is a test bio";
    
    auto tags = SEOGenerator::generateTwitterCardTags(profile, baseUrl, "person");
    
    // Verify it's an array
    ASSERT_TRUE(tags.is_array());
    EXPECT_GT(tags.size(), 0);
    
    // Check for required Twitter Card tags
    bool hasCard = false;
    bool hasTitle = false;
    bool hasDescription = false;
    
    for (const auto& tag : tags) {
        ASSERT_TRUE(tag.contains("name"));
        ASSERT_TRUE(tag.contains("content"));
        
        std::string name = tag["name"];
        if (name == "twitter:card") {
            hasCard = true;
            EXPECT_EQ(tag["content"], "summary");
        }
        if (name == "twitter:title") hasTitle = true;
        if (name == "twitter:description") hasDescription = true;
    }
    
    EXPECT_TRUE(hasCard);
    EXPECT_TRUE(hasTitle);
    EXPECT_TRUE(hasDescription);
}

TEST_F(SEOGeneratorTest, MetaDescriptionGeneration) {
    Profile profile;
    profile.name = "Test User";
    profile.bio = "This is a test bio that describes the user in detail";
    
    auto description = SEOGenerator::generateMetaDescription(profile);
    
    // Should use bio
    EXPECT_EQ(description, profile.bio.value());
    
    // Test without bio
    Profile profile2;
    profile2.name = "Test User 2";
    
    auto description2 = SEOGenerator::generateMetaDescription(profile2);
    EXPECT_NE(description2, "");
}

TEST_F(SEOGeneratorTest, MetaDescriptionTruncation) {
    Profile profile;
    profile.name = "Test User";
    profile.bio = std::string(200, 'a'); // 200 characters
    
    auto description = SEOGenerator::generateMetaDescription(profile, 160);
    
    // Should be truncated to 160 characters
    EXPECT_LE(description.length(), 160);
    EXPECT_TRUE(description.find("...") != std::string::npos);
}

TEST_F(SEOGeneratorTest, PageTitleGeneration) {
    Profile profile;
    profile.name = "John Doe";
    profile.type = ProfileType::PERSON;
    
    auto title = SEOGenerator::generatePageTitle(profile);
    
    EXPECT_NE(title.find("John Doe"), std::string::npos);
    EXPECT_NE(title.find("Hatef.ir"), std::string::npos);
}

// Main function only for standalone test execution
#ifndef SEO_TESTS_COMBINED
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
