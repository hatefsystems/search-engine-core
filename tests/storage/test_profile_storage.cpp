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
        REQUIRE(ProfileStorage::isValidSlug("test_user_profile"));
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
    ProfileStorage storage("mongodb://localhost:27017", "test-search-engine");

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
        REQUIRE_FALSE(verifyResult.success);
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
    ProfileStorage storage("mongodb://localhost:27017", "test-search-engine");

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
