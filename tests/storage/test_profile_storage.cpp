#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "../../include/search_engine/storage/ProfileStorage.h"
#include <chrono>
#include <thread>

using namespace search_engine::storage;

// Test data helpers
namespace {
    Profile createTestProfile(const std::string& slug = "test-profile", const std::string& name = "Test Profile") {
        Profile profile;
        profile.slug = slug;
        profile.name = name;
        profile.type = ProfileType::PERSON;
        profile.bio = "A test profile for unit testing";
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();
        return profile;
    }

    Profile createPersianProfile(const std::string& slug = "علی-رضایی", const std::string& name = "علی رضایی") {
        Profile profile;
        profile.slug = slug;
        profile.name = name;
        profile.type = ProfileType::PERSON;
        profile.bio = "یک پروفایل آزمایشی برای تست واحد";
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();
        return profile;
    }

    Profile createMixedPersianEnglishProfile() {
        Profile profile;
        profile.slug = "ali-علی-123";
        profile.name = "Ali رضایی";
        profile.type = ProfileType::BUSINESS;
        profile.bio = "A Persian-English mixed profile for testing";
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();
        return profile;
    }
}

TEST_CASE("ProfileStorage - Connection and Initialization", "[profilestorage][storage]") {
    SECTION("Constructor with default parameters") {
        REQUIRE_NOTHROW(ProfileStorage());
    }

    SECTION("Constructor with custom parameters") {
        REQUIRE_NOTHROW(ProfileStorage("mongodb://localhost:27017", "test-db"));
    }

    SECTION("Test connection") {
        ProfileStorage storage("mongodb://localhost:27017", "test-search-engine");
        auto result = storage.testConnection();

        if (result.success) {
            REQUIRE(result.value == true);
            REQUIRE(result.message == "Connection test successful");
        } else {
            WARN("MongoDB not available: " + result.message);
            return;
        }
    }
}

TEST_CASE("ProfileStorage - Slug Validation", "[profilestorage][validation]") {
    SECTION("Valid slugs - English only") {
        REQUIRE(ProfileStorage::isValidSlug("john-doe"));
        REQUIRE(ProfileStorage::isValidSlug("test123"));
        REQUIRE(ProfileStorage::isValidSlug("user-name"));
        REQUIRE(ProfileStorage::isValidSlug("a"));
        REQUIRE(ProfileStorage::isValidSlug("testuserprofile"));
    }

    SECTION("Valid slugs - Persian only") {
        REQUIRE(ProfileStorage::isValidSlug("علی"));
        REQUIRE(ProfileStorage::isValidSlug("محمد-رضایی"));
        REQUIRE(ProfileStorage::isValidSlug("۱۲۳"));
        REQUIRE(ProfileStorage::isValidSlug("علیرضا"));
    }

    SECTION("Valid slugs - Mixed Persian-English") {
        REQUIRE(ProfileStorage::isValidSlug("ali-علی"));
        REQUIRE(ProfileStorage::isValidSlug("علی-ali"));
        REQUIRE(ProfileStorage::isValidSlug("ali-علی-123"));
        REQUIRE(ProfileStorage::isValidSlug("test-تست-۱۲۳"));
    }

    SECTION("Invalid slugs - Empty or spaces") {
        REQUIRE_FALSE(ProfileStorage::isValidSlug(""));
        REQUIRE_FALSE(ProfileStorage::isValidSlug(" "));
        REQUIRE_FALSE(ProfileStorage::isValidSlug("ali ali"));
        REQUIRE_FALSE(ProfileStorage::isValidSlug("علی رضایی"));
    }

    SECTION("Invalid slugs - Special characters") {
        REQUIRE_FALSE(ProfileStorage::isValidSlug("ali@doe"));
        REQUIRE_FALSE(ProfileStorage::isValidSlug("ali.doe"));
        REQUIRE_FALSE(ProfileStorage::isValidSlug("ali$doe"));
        REQUIRE_FALSE(ProfileStorage::isValidSlug("ali#doe"));
    }

    SECTION("Invalid slugs - Underscores") {
        REQUIRE_FALSE(ProfileStorage::isValidSlug("ali_doe"));
        REQUIRE_FALSE(ProfileStorage::isValidSlug("test_user"));
    }
}

TEST_CASE("ProfileStorage - Profile CRUD Operations", "[profilestorage][storage][crud]") {
    ProfileStorage storage("mongodb://admin:password123@localhost:27017", "test-search-engine");

    // Skip tests if MongoDB is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping ProfileStorage tests - MongoDB not available");
        return;
    }

    SECTION("Store and retrieve profile by slug - English") {
        Profile testProfile = createTestProfile("english-profile", "English Profile");

        // Store the profile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);
        REQUIRE(!storeResult.value.empty());

        std::string profileId = storeResult.value;

        // Retrieve by slug
        auto retrieveResult = storage.findBySlug("english-profile");
        REQUIRE(retrieveResult.success);
        REQUIRE(retrieveResult.value.has_value());

        Profile retrieved = retrieveResult.value.value();
        REQUIRE(retrieved.slug == testProfile.slug);
        REQUIRE(retrieved.name == testProfile.name);
        REQUIRE(retrieved.type == testProfile.type);
        REQUIRE(retrieved.isPublic == testProfile.isPublic);

        // Retrieve by ID
        auto retrieveByIdResult = storage.findById(profileId);
        REQUIRE(retrieveByIdResult.success);
        REQUIRE(retrieveByIdResult.value.slug == testProfile.slug);

        // Clean up
        storage.deleteProfile(profileId);
    }

    SECTION("Store and retrieve profile by slug - Persian") {
        Profile testProfile = createPersianProfile("علی-رضایی", "علی رضایی");

        // Store the profile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);
        REQUIRE(!storeResult.value.empty());

        std::string profileId = storeResult.value;

        // Retrieve by Persian slug
        auto retrieveResult = storage.findBySlug("علی-رضایی");
        REQUIRE(retrieveResult.success);
        REQUIRE(retrieveResult.value.has_value());

        Profile retrieved = retrieveResult.value.value();
        REQUIRE(retrieved.slug == testProfile.slug);
        REQUIRE(retrieved.name == testProfile.name);
        REQUIRE(retrieved.bio == testProfile.bio);

        // Clean up
        storage.deleteProfile(profileId);
    }

    SECTION("Store and retrieve profile by slug - Mixed Persian-English") {
        Profile testProfile = createMixedPersianEnglishProfile();

        // Store the profile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);
        REQUIRE(!storeResult.value.empty());

        std::string profileId = storeResult.value;

        // Retrieve by mixed slug
        auto retrieveResult = storage.findBySlug("ali-علی-123");
        REQUIRE(retrieveResult.success);
        REQUIRE(retrieveResult.value.has_value());

        Profile retrieved = retrieveResult.value.value();
        REQUIRE(retrieved.slug == testProfile.slug);
        REQUIRE(retrieved.name == testProfile.name);

        // Clean up
        storage.deleteProfile(profileId);
    }

    SECTION("Slug uniqueness enforcement") {
        Profile profile1 = createTestProfile("unique-slug", "Profile 1");
        Profile profile2 = createTestProfile("unique-slug", "Profile 2");

        // Store first profile
        auto storeResult1 = storage.store(profile1);
        REQUIRE(storeResult1.success);

        // Try to store second profile with same slug - should fail
        auto storeResult2 = storage.store(profile2);
        REQUIRE_FALSE(storeResult2.success);
        REQUIRE(storeResult2.message.find("already taken") != std::string::npos);

        // Clean up
        storage.deleteProfile(storeResult1.value);
    }

    SECTION("Update profile") {
        Profile testProfile = createTestProfile("update-test", "Original Name");

        // Store the profile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);

        // Retrieve and modify
        auto retrieveResult = storage.findBySlug("update-test");
        REQUIRE(retrieveResult.success);

        Profile retrieved = retrieveResult.value.value();
        retrieved.name = "Updated Name";
        retrieved.bio = "Updated bio with Persian: بروزرسانی شد";
        retrieved.id = storeResult.value;  // Set ID for update

        // Update
        auto updateResult = storage.update(retrieved);
        REQUIRE(updateResult.success);

        // Retrieve again and verify changes
        auto verifyResult = storage.findBySlug("update-test");
        REQUIRE(verifyResult.success);

        Profile verified = verifyResult.value.value();
        REQUIRE(verified.name == "Updated Name");
        REQUIRE(verified.bio == "Updated bio with Persian: بروزرسانی شد");

        // Clean up
        storage.deleteProfile(storeResult.value);
    }

    SECTION("Delete profile") {
        Profile testProfile = createTestProfile("delete-test", "Delete Test");

        // Store the profile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);

        // Verify it exists
        auto retrieveResult = storage.findBySlug("delete-test");
        REQUIRE(retrieveResult.success);

        // Delete
        auto deleteResult = storage.deleteProfile(storeResult.value);
        REQUIRE(deleteResult.success);

        // Verify it's gone
        auto verifyResult = storage.findBySlug("delete-test");
        REQUIRE(verifyResult.success);
        REQUIRE_FALSE(verifyResult.value.has_value());
    }

    SECTION("Non-existent profile retrieval") {
        auto result = storage.findBySlug("non-existent-slug");
        REQUIRE(result.success);
        REQUIRE_FALSE(result.value.has_value());
        REQUIRE(result.message.find("No profile found") != std::string::npos);
    }

    SECTION("Invalid slug validation on store") {
        Profile invalidProfile = createTestProfile("invalid slug", "Invalid Slug");

        auto storeResult = storage.store(invalidProfile);
        REQUIRE_FALSE(storeResult.success);
        REQUIRE(storeResult.message.find("Invalid slug format") != std::string::npos);
    }

    SECTION("Find by type") {
        Profile personProfile = createTestProfile("person-type", "Person Profile");
        personProfile.type = ProfileType::PERSON;

        Profile businessProfile = createTestProfile("business-type", "Business Profile");
        businessProfile.type = ProfileType::BUSINESS;

        // Store both profiles
        auto storePerson = storage.store(personProfile);
        REQUIRE(storePerson.success);

        auto storeBusiness = storage.store(businessProfile);
        REQUIRE(storeBusiness.success);

        // Find by type
        auto personResults = storage.findByType(ProfileType::PERSON);
        REQUIRE(personResults.success);
        REQUIRE(personResults.value.size() >= 1);

        auto businessResults = storage.findByType(ProfileType::BUSINESS);
        REQUIRE(businessResults.success);
        REQUIRE(businessResults.value.size() >= 1);

        // Verify types
        bool foundPerson = false, foundBusiness = false;
        for (const auto& p : personResults.value) {
            if (p.slug == "person-type") foundPerson = true;
        }
        for (const auto& p : businessResults.value) {
            if (p.slug == "business-type") foundBusiness = true;
        }

        REQUIRE(foundPerson);
        REQUIRE(foundBusiness);

        // Clean up
        storage.deleteProfile(storePerson.value);
        storage.deleteProfile(storeBusiness.value);
    }
}

TEST_CASE("ProfileStorage - Count Operations", "[profilestorage][storage]") {
    ProfileStorage storage("mongodb://admin:password123@localhost:27017", "test-search-engine");

    // Skip tests if MongoDB is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping ProfileStorage count tests - MongoDB not available");
        return;
    }

    SECTION("Count by type") {
        // Get initial counts
        auto initialPersonCount = storage.countByType(ProfileType::PERSON);
        REQUIRE(initialPersonCount.success);

        auto initialBusinessCount = storage.countByType(ProfileType::BUSINESS);
        REQUIRE(initialBusinessCount.success);

        // Add test profiles
        Profile personProfile = createTestProfile("count-person", "Count Person");
        personProfile.type = ProfileType::PERSON;

        Profile businessProfile = createTestProfile("count-business", "Count Business");
        businessProfile.type = ProfileType::BUSINESS;

        auto storePerson = storage.store(personProfile);
        REQUIRE(storePerson.success);

        auto storeBusiness = storage.store(businessProfile);
        REQUIRE(storeBusiness.success);

        // Check counts increased
        auto newPersonCount = storage.countByType(ProfileType::PERSON);
        REQUIRE(newPersonCount.success);
        REQUIRE(newPersonCount.value >= initialPersonCount.value + 1);

        auto newBusinessCount = storage.countByType(ProfileType::BUSINESS);
        REQUIRE(newBusinessCount.success);
        REQUIRE(newBusinessCount.value >= initialBusinessCount.value + 1);

        // Clean up
        storage.deleteProfile(storePerson.value);
        storage.deleteProfile(storeBusiness.value);
    }
}

// ==================== PersonProfile and BusinessProfile Tests ====================

namespace {
    PersonProfile createTestPersonProfile(const std::string& slug = "john-doe", const std::string& name = "John Doe") {
        PersonProfile profile;
        profile.slug = slug;
        profile.name = name;
        profile.type = ProfileType::PERSON;
        profile.bio = "Software Engineer and Tech Enthusiast";
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();
        
        // PersonProfile-specific fields
        profile.title = "Senior Software Engineer";
        profile.company = "Tech Corp";
        profile.skills = {"C++", "Python", "MongoDB", "Redis"};
        profile.experienceLevel = "Senior";
        profile.education = "Computer Science";
        profile.school = "MIT";
        profile.linkedinUrl = "https://linkedin.com/in/johndoe";
        profile.githubUrl = "https://github.com/johndoe";
        profile.portfolioUrl = "https://johndoe.dev";
        profile.email = "john@example.com";
        profile.phone = "+1234567890";
        
        return profile;
    }

    BusinessProfile createTestBusinessProfile(const std::string& slug = "tech-corp", const std::string& name = "Tech Corp") {
        BusinessProfile profile;
        profile.slug = slug;
        profile.name = name;
        profile.type = ProfileType::BUSINESS;
        profile.bio = "Leading technology company specializing in software solutions";
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();
        
        // BusinessProfile-specific fields
        profile.companyName = "Tech Corporation Inc.";
        profile.industry = "Technology";
        profile.companySize = "51-200";
        profile.foundedYear = 2015;
        profile.address = "123 Main Street";
        profile.city = "San Francisco";
        profile.country = "USA";
        profile.website = "https://techcorp.com";
        profile.description = "We build innovative software solutions";
        profile.services = {"Software Development", "Consulting", "Cloud Services"};
        profile.businessEmail = "contact@techcorp.com";
        profile.businessPhone = "+1987654321";
        
        return profile;
    }
}

TEST_CASE("Profile Models - Unit Tests", "[profile][models][unit]") {
    SECTION("Profile type conversion - profileTypeToString") {
        using namespace search_engine::storage;
        REQUIRE(profileTypeToString(ProfileType::PERSON) == "PERSON");
        REQUIRE(profileTypeToString(ProfileType::BUSINESS) == "BUSINESS");
    }

    SECTION("Profile type conversion - stringToProfileType valid") {
        using namespace search_engine::storage;
        REQUIRE(stringToProfileType("PERSON") == ProfileType::PERSON);
        REQUIRE(stringToProfileType("BUSINESS") == ProfileType::BUSINESS);
    }

    SECTION("Profile type conversion - stringToProfileType invalid throws") {
        using namespace search_engine::storage;
        REQUIRE_THROWS_AS(stringToProfileType("INVALID"), std::invalid_argument);
        REQUIRE_THROWS_AS(stringToProfileType(""), std::invalid_argument);
        REQUIRE_THROWS_AS(stringToProfileType("person"), std::invalid_argument);
    }

    SECTION("PersonProfile - Create valid profile") {
        PersonProfile profile = createTestPersonProfile();
        
        REQUIRE(profile.isValid());
        REQUIRE(profile.type == ProfileType::PERSON);
        REQUIRE(profile.slug == "john-doe");
        REQUIRE(profile.name == "John Doe");
        REQUIRE(profile.title.value() == "Senior Software Engineer");
        REQUIRE(profile.company.value() == "Tech Corp");
        REQUIRE(profile.skills.size() == 4);
        REQUIRE(profile.experienceLevel.value() == "Senior");
        REQUIRE(profile.email.value() == "john@example.com");
    }

    SECTION("PersonProfile - Validation: type must be PERSON") {
        PersonProfile profile = createTestPersonProfile();
        profile.type = ProfileType::BUSINESS;
        
        REQUIRE_FALSE(profile.isValid());
    }

    SECTION("PersonProfile - Validation: experienceLevel constraints") {
        PersonProfile profile = createTestPersonProfile();
        
        // Valid experience levels
        profile.experienceLevel = "Entry";
        REQUIRE(profile.isValid());
        
        profile.experienceLevel = "Mid";
        REQUIRE(profile.isValid());
        
        profile.experienceLevel = "Senior";
        REQUIRE(profile.isValid());
        
        profile.experienceLevel = "Executive";
        REQUIRE(profile.isValid());
        
        // Invalid experience level
        profile.experienceLevel = "InvalidLevel";
        REQUIRE_FALSE(profile.isValid());
    }

    SECTION("PersonProfile - Validation: base profile constraints") {
        PersonProfile profile = createTestPersonProfile();
        
        // Empty slug should fail
        profile.slug = "";
        REQUIRE_FALSE(profile.isValid());
        
        // Reset and test empty name
        profile = createTestPersonProfile();
        profile.name = "";
        REQUIRE_FALSE(profile.isValid());
        
        // Reset and test bio length
        profile = createTestPersonProfile();
        profile.bio = std::string(501, 'a'); // 501 characters
        REQUIRE_FALSE(profile.isValid());
    }

    SECTION("BusinessProfile - Create valid profile") {
        BusinessProfile profile = createTestBusinessProfile();
        
        REQUIRE(profile.isValid());
        REQUIRE(profile.type == ProfileType::BUSINESS);
        REQUIRE(profile.slug == "tech-corp");
        REQUIRE(profile.name == "Tech Corp");
        REQUIRE(profile.companyName.value() == "Tech Corporation Inc.");
        REQUIRE(profile.industry.value() == "Technology");
        REQUIRE(profile.companySize.value() == "51-200");
        REQUIRE(profile.foundedYear.value() == 2015);
        REQUIRE(profile.services.size() == 3);
        REQUIRE(profile.businessEmail.value() == "contact@techcorp.com");
    }

    SECTION("BusinessProfile - Validation: type must be BUSINESS") {
        BusinessProfile profile = createTestBusinessProfile();
        profile.type = ProfileType::PERSON;
        
        REQUIRE_FALSE(profile.isValid());
    }

    SECTION("BusinessProfile - Validation: companySize constraints") {
        BusinessProfile profile = createTestBusinessProfile();
        
        // Valid company sizes
        profile.companySize = "1-10";
        REQUIRE(profile.isValid());
        
        profile.companySize = "11-50";
        REQUIRE(profile.isValid());
        
        profile.companySize = "51-200";
        REQUIRE(profile.isValid());
        
        profile.companySize = "201-1000";
        REQUIRE(profile.isValid());
        
        profile.companySize = "1000+";
        REQUIRE(profile.isValid());
        
        // Invalid company size
        profile.companySize = "InvalidSize";
        REQUIRE_FALSE(profile.isValid());
    }

    SECTION("BusinessProfile - Validation: foundedYear constraints") {
        BusinessProfile profile = createTestBusinessProfile();
        
        // Valid years
        profile.foundedYear = 1900;
        REQUIRE(profile.isValid());
        
        profile.foundedYear = 2024;
        REQUIRE(profile.isValid());
        
        // Invalid years (too old or in future)
        profile.foundedYear = 1799;
        REQUIRE_FALSE(profile.isValid());
        
        profile.foundedYear = 2101;
        REQUIRE_FALSE(profile.isValid());
    }
}

TEST_CASE("ProfileStorage - PersonProfile CRUD Operations", "[profilestorage][personprofile][crud]") {
    ProfileStorage storage("mongodb://admin:password123@localhost:27017", "test-search-engine");

    // Skip tests if MongoDB is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping PersonProfile tests - MongoDB not available");
        return;
    }

    SECTION("Store and retrieve PersonProfile by slug") {
        PersonProfile testProfile = createTestPersonProfile("test-person-slug", "Test Person");

        // Store the profile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);
        REQUIRE(!storeResult.value.empty());

        std::string profileId = storeResult.value;

        // Retrieve by slug
        auto retrieveResult = storage.findPersonBySlug("test-person-slug");
        REQUIRE(retrieveResult.success);
        REQUIRE(retrieveResult.value.has_value());

        PersonProfile retrieved = retrieveResult.value.value();
        REQUIRE(retrieved.slug == testProfile.slug);
        REQUIRE(retrieved.name == testProfile.name);
        REQUIRE(retrieved.type == ProfileType::PERSON);
        REQUIRE(retrieved.title.value() == testProfile.title.value());
        REQUIRE(retrieved.company.value() == testProfile.company.value());
        REQUIRE(retrieved.skills.size() == testProfile.skills.size());
        REQUIRE(retrieved.email.value() == testProfile.email.value());

        // Clean up
        storage.deleteProfile(profileId);
    }

    SECTION("Store and retrieve PersonProfile by ID") {
        PersonProfile testProfile = createTestPersonProfile("test-person-id", "Test Person ID");

        // Store the profile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);

        std::string profileId = storeResult.value;

        // Retrieve by ID
        auto retrieveResult = storage.findPersonById(profileId);
        REQUIRE(retrieveResult.success);
        REQUIRE(retrieveResult.value.has_value());

        PersonProfile retrieved = retrieveResult.value.value();
        REQUIRE(retrieved.id.value() == profileId);
        REQUIRE(retrieved.slug == testProfile.slug);

        // Clean up
        storage.deleteProfile(profileId);
    }

    SECTION("Update PersonProfile") {
        PersonProfile testProfile = createTestPersonProfile("update-person", "Original Person");

        // Store the profile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);

        // Retrieve and modify
        auto retrieveResult = storage.findPersonBySlug("update-person");
        REQUIRE(retrieveResult.success);

        PersonProfile retrieved = retrieveResult.value.value();
        retrieved.name = "Updated Person Name";
        retrieved.title = "Lead Engineer";
        retrieved.skills.push_back("Rust");
        retrieved.company = "New Company";

        // Update
        auto updateResult = storage.update(retrieved);
        REQUIRE(updateResult.success);

        // Retrieve again and verify changes
        auto verifyResult = storage.findPersonBySlug("update-person");
        REQUIRE(verifyResult.success);

        PersonProfile verified = verifyResult.value.value();
        REQUIRE(verified.name == "Updated Person Name");
        REQUIRE(verified.title.value() == "Lead Engineer");
        REQUIRE(verified.skills.size() == 5);
        REQUIRE(verified.company.value() == "New Company");

        // Clean up
        storage.deleteProfile(storeResult.value);
    }

    SECTION("Type discrimination - PersonProfile not returned as BusinessProfile") {
        PersonProfile testProfile = createTestPersonProfile("person-discrimination", "Person Test");

        // Store PersonProfile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);

        // Try to retrieve as BusinessProfile - should return nullopt
        auto businessResult = storage.findBusinessBySlug("person-discrimination");
        REQUIRE(businessResult.success);
        REQUIRE_FALSE(businessResult.value.has_value());

        // But should work as PersonProfile
        auto personResult = storage.findPersonBySlug("person-discrimination");
        REQUIRE(personResult.success);
        REQUIRE(personResult.value.has_value());

        // Clean up
        storage.deleteProfile(storeResult.value);
    }

    SECTION("PersonProfile - Invalid validation on store") {
        PersonProfile invalidProfile = createTestPersonProfile("invalid-person", "Invalid");
        invalidProfile.type = ProfileType::BUSINESS; // Wrong type

        auto storeResult = storage.store(invalidProfile);
        REQUIRE_FALSE(storeResult.success);
        REQUIRE(storeResult.message.find("validation failed") != std::string::npos);
    }
}

TEST_CASE("ProfileStorage - BusinessProfile CRUD Operations", "[profilestorage][businessprofile][crud]") {
    ProfileStorage storage("mongodb://admin:password123@localhost:27017", "test-search-engine");

    // Skip tests if MongoDB is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping BusinessProfile tests - MongoDB not available");
        return;
    }

    SECTION("Store and retrieve BusinessProfile by slug") {
        BusinessProfile testProfile = createTestBusinessProfile("test-business-slug", "Test Business");

        // Store the profile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);
        REQUIRE(!storeResult.value.empty());

        std::string profileId = storeResult.value;

        // Retrieve by slug
        auto retrieveResult = storage.findBusinessBySlug("test-business-slug");
        REQUIRE(retrieveResult.success);
        REQUIRE(retrieveResult.value.has_value());

        BusinessProfile retrieved = retrieveResult.value.value();
        REQUIRE(retrieved.slug == testProfile.slug);
        REQUIRE(retrieved.name == testProfile.name);
        REQUIRE(retrieved.type == ProfileType::BUSINESS);
        REQUIRE(retrieved.companyName.value() == testProfile.companyName.value());
        REQUIRE(retrieved.industry.value() == testProfile.industry.value());
        REQUIRE(retrieved.services.size() == testProfile.services.size());
        REQUIRE(retrieved.businessEmail.value() == testProfile.businessEmail.value());

        // Clean up
        storage.deleteProfile(profileId);
    }

    SECTION("Store and retrieve BusinessProfile by ID") {
        BusinessProfile testProfile = createTestBusinessProfile("test-business-id", "Test Business ID");

        // Store the profile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);

        std::string profileId = storeResult.value;

        // Retrieve by ID
        auto retrieveResult = storage.findBusinessById(profileId);
        REQUIRE(retrieveResult.success);
        REQUIRE(retrieveResult.value.has_value());

        BusinessProfile retrieved = retrieveResult.value.value();
        REQUIRE(retrieved.id.value() == profileId);
        REQUIRE(retrieved.slug == testProfile.slug);

        // Clean up
        storage.deleteProfile(profileId);
    }

    SECTION("Update BusinessProfile") {
        BusinessProfile testProfile = createTestBusinessProfile("update-business", "Original Business");

        // Store the profile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);

        // Retrieve and modify
        auto retrieveResult = storage.findBusinessBySlug("update-business");
        REQUIRE(retrieveResult.success);

        BusinessProfile retrieved = retrieveResult.value.value();
        retrieved.name = "Updated Business Name";
        retrieved.companyName = "Updated Corp";
        retrieved.services.push_back("AI Solutions");
        retrieved.foundedYear = 2020;

        // Update
        auto updateResult = storage.update(retrieved);
        REQUIRE(updateResult.success);

        // Retrieve again and verify changes
        auto verifyResult = storage.findBusinessBySlug("update-business");
        REQUIRE(verifyResult.success);

        BusinessProfile verified = verifyResult.value.value();
        REQUIRE(verified.name == "Updated Business Name");
        REQUIRE(verified.companyName.value() == "Updated Corp");
        REQUIRE(verified.services.size() == 4);
        REQUIRE(verified.foundedYear.value() == 2020);

        // Clean up
        storage.deleteProfile(storeResult.value);
    }

    SECTION("Type discrimination - BusinessProfile not returned as PersonProfile") {
        BusinessProfile testProfile = createTestBusinessProfile("business-discrimination", "Business Test");

        // Store BusinessProfile
        auto storeResult = storage.store(testProfile);
        REQUIRE(storeResult.success);

        // Try to retrieve as PersonProfile - should return nullopt
        auto personResult = storage.findPersonBySlug("business-discrimination");
        REQUIRE(personResult.success);
        REQUIRE_FALSE(personResult.value.has_value());

        // But should work as BusinessProfile
        auto businessResult = storage.findBusinessBySlug("business-discrimination");
        REQUIRE(businessResult.success);
        REQUIRE(businessResult.value.has_value());

        // Clean up
        storage.deleteProfile(storeResult.value);
    }

    SECTION("BusinessProfile - Invalid validation on store") {
        BusinessProfile invalidProfile = createTestBusinessProfile("invalid-business", "Invalid");
        invalidProfile.type = ProfileType::PERSON; // Wrong type

        auto storeResult = storage.store(invalidProfile);
        REQUIRE_FALSE(storeResult.success);
        REQUIRE(storeResult.message.find("validation failed") != std::string::npos);
    }
}

TEST_CASE("ProfileStorage - Extended Profiles Integration", "[profilestorage][integration]") {
    ProfileStorage storage("mongodb://admin:password123@localhost:27017", "test-search-engine");

    // Skip tests if MongoDB is not available
    auto connectionTest = storage.testConnection();
    if (!connectionTest.success) {
        WARN("Skipping integration tests - MongoDB not available");
        return;
    }

    SECTION("Base findBySlug still works after storing extended profiles") {
        PersonProfile personProfile = createTestPersonProfile("integration-person", "Integration Person");
        BusinessProfile businessProfile = createTestBusinessProfile("integration-business", "Integration Business");

        // Store both
        auto storePersonResult = storage.store(personProfile);
        REQUIRE(storePersonResult.success);

        auto storeBusinessResult = storage.store(businessProfile);
        REQUIRE(storeBusinessResult.success);

        // Retrieve using base Profile methods
        auto personBaseResult = storage.findBySlug("integration-person");
        REQUIRE(personBaseResult.success);
        REQUIRE(personBaseResult.value.has_value());
        REQUIRE(personBaseResult.value.value().type == ProfileType::PERSON);

        auto businessBaseResult = storage.findBySlug("integration-business");
        REQUIRE(businessBaseResult.success);
        REQUIRE(businessBaseResult.value.has_value());
        REQUIRE(businessBaseResult.value.value().type == ProfileType::BUSINESS);

        // Clean up
        storage.deleteProfile(storePersonResult.value);
        storage.deleteProfile(storeBusinessResult.value);
    }

    SECTION("Slug uniqueness enforced across all profile types") {
        PersonProfile personProfile = createTestPersonProfile("unique-test", "Person");
        
        // Store person profile
        auto storeResult1 = storage.store(personProfile);
        REQUIRE(storeResult1.success);

        // Try to store business profile with same slug - should fail
        BusinessProfile businessProfile = createTestBusinessProfile("unique-test", "Business");
        auto storeResult2 = storage.store(businessProfile);
        REQUIRE_FALSE(storeResult2.success);
        REQUIRE(storeResult2.message.find("already taken") != std::string::npos);

        // Clean up
        storage.deleteProfile(storeResult1.value);
    }
}
