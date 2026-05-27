#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "../../include/search_engine/storage/ProfileValidator.h"
#include "../../include/search_engine/storage/Profile.h"

using namespace search_engine::storage;

TEST_CASE("ProfileValidator - Slug Validation", "[validator][slug]") {
    SECTION("Valid slugs - English") {
        REQUIRE(ProfileValidator::isValidSlug("john-doe"));
        REQUIRE(ProfileValidator::isValidSlug("test123"));
        REQUIRE(ProfileValidator::isValidSlug("user-name"));
        REQUIRE(ProfileValidator::isValidSlug("a"));
        REQUIRE(ProfileValidator::isValidSlug("testuserprofile"));
    }

    SECTION("Valid slugs - Persian") {
        REQUIRE(ProfileValidator::isValidSlug("علی"));
        REQUIRE(ProfileValidator::isValidSlug("محمد-رضایی"));
        REQUIRE(ProfileValidator::isValidSlug("۱۲۳"));
        REQUIRE(ProfileValidator::isValidSlug("علیرضا"));
    }

    SECTION("Valid slugs - Mixed Persian-English") {
        REQUIRE(ProfileValidator::isValidSlug("ali-علی"));
        REQUIRE(ProfileValidator::isValidSlug("علی-ali"));
        REQUIRE(ProfileValidator::isValidSlug("ali-علی-123"));
        REQUIRE(ProfileValidator::isValidSlug("test-تست-۱۲۳"));
    }

    SECTION("Invalid slugs - Empty or spaces") {
        REQUIRE_FALSE(ProfileValidator::isValidSlug(""));
        REQUIRE_FALSE(ProfileValidator::isValidSlug(" "));
        REQUIRE_FALSE(ProfileValidator::isValidSlug("ali ali"));
        REQUIRE_FALSE(ProfileValidator::isValidSlug("علی رضایی"));
    }

    SECTION("Invalid slugs - Special characters") {
        REQUIRE_FALSE(ProfileValidator::isValidSlug("ali@doe"));
        REQUIRE_FALSE(ProfileValidator::isValidSlug("ali.doe"));
        REQUIRE_FALSE(ProfileValidator::isValidSlug("ali$doe"));
        REQUIRE_FALSE(ProfileValidator::isValidSlug("ali#doe"));
    }

    SECTION("Invalid slugs - Underscores") {
        REQUIRE_FALSE(ProfileValidator::isValidSlug("ali_doe"));
        REQUIRE_FALSE(ProfileValidator::isValidSlug("test_user"));
    }
}

TEST_CASE("ProfileValidator - Email Validation", "[validator][email]") {
    SECTION("Valid emails") {
        REQUIRE(ProfileValidator::isValidEmail("user@example.com"));
        REQUIRE(ProfileValidator::isValidEmail("test.email@domain.co.uk"));
        REQUIRE(ProfileValidator::isValidEmail("user+tag@gmail.com"));
        REQUIRE(ProfileValidator::isValidEmail("a@bc.co"));
    }

    SECTION("Invalid emails") {
        REQUIRE_FALSE(ProfileValidator::isValidEmail(""));
        REQUIRE_FALSE(ProfileValidator::isValidEmail("invalid"));
        REQUIRE_FALSE(ProfileValidator::isValidEmail("@example.com"));
        REQUIRE_FALSE(ProfileValidator::isValidEmail("user@"));
        REQUIRE_FALSE(ProfileValidator::isValidEmail("user@@example.com"));
        REQUIRE_FALSE(ProfileValidator::isValidEmail("user.example.com"));
    }
}

TEST_CASE("ProfileValidator - Phone Validation", "[validator][phone]") {
    SECTION("Valid international phone numbers") {
        REQUIRE(ProfileValidator::isValidPhone("+989123456789"));
        REQUIRE(ProfileValidator::isValidPhone("+14155551234"));
        REQUIRE(ProfileValidator::isValidPhone("+442071234567"));
        REQUIRE(ProfileValidator::isValidPhone("+12025550123"));
        REQUIRE(ProfileValidator::isValidPhone("+123456789012345"));  // Max length
    }

    SECTION("Invalid phone numbers") {
        REQUIRE_FALSE(ProfileValidator::isValidPhone("09123456789"));  // Missing +
        REQUIRE_FALSE(ProfileValidator::isValidPhone("+123"));  // Too short
        REQUIRE_FALSE(ProfileValidator::isValidPhone("+1234567890123456789"));  // Too long (15 digits after country code)
        REQUIRE_FALSE(ProfileValidator::isValidPhone("+abc123456789"));  // Non-numeric
    }
}

TEST_CASE("ProfileValidator - URL Validation", "[validator][url]") {
    SECTION("Valid URLs") {
        REQUIRE(ProfileValidator::isValidUrl("https://example.com"));
        REQUIRE(ProfileValidator::isValidUrl("http://example.com"));
        REQUIRE(ProfileValidator::isValidUrl("https://www.example.com/path"));
        REQUIRE(ProfileValidator::isValidUrl("https://example.com:8080/path?query=value"));
        REQUIRE(ProfileValidator::isValidUrl("http://sub.example.com/path#fragment"));
    }

    SECTION("Invalid URLs") {
        REQUIRE_FALSE(ProfileValidator::isValidUrl(""));
        REQUIRE_FALSE(ProfileValidator::isValidUrl("example.com"));
        REQUIRE_FALSE(ProfileValidator::isValidUrl("ftp://example.com"));
        REQUIRE_FALSE(ProfileValidator::isValidUrl("https://"));
        REQUIRE_FALSE(ProfileValidator::isValidUrl("://example.com"));
    }
}

TEST_CASE("ProfileValidator - Profile Validation", "[validator][profile]") {
    SECTION("Valid base profile") {
        Profile profile;
        profile.slug = "john-doe";
        profile.name = "John Doe";
        profile.type = ProfileType::PERSON;
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();

        auto result = ProfileValidator::validate(profile);
        REQUIRE(result.isValid);
        REQUIRE(result.errors.empty());
    }

    SECTION("Invalid profile - missing slug") {
        Profile profile;
        profile.name = "John Doe";
        profile.type = ProfileType::PERSON;
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();

        auto result = ProfileValidator::validate(profile);
        REQUIRE_FALSE(result.isValid);
        REQUIRE(result.errors.size() == 1);
        REQUIRE(result.errors[0] == "Slug is required");
    }

    SECTION("Invalid profile - invalid slug") {
        Profile profile;
        profile.slug = "John Doe";  // Invalid - contains space and uppercase
        profile.name = "John Doe";
        profile.type = ProfileType::PERSON;
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();

        auto result = ProfileValidator::validate(profile);
        REQUIRE_FALSE(result.isValid);
        REQUIRE(result.errors[0] == "Invalid slug format. Slug must contain only Persian letters, English letters, numbers, and hyphens.");
    }

    SECTION("Profile with warnings") {
        Profile profile;
        profile.slug = "john-doe";
        profile.name = "John Doe";
        profile.type = ProfileType::PERSON;
        profile.bio = "";  // Empty bio - should warn
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();

        auto result = ProfileValidator::validate(profile);
        REQUIRE(result.isValid);
        REQUIRE(result.warnings.size() >= 1);
        REQUIRE(result.warnings[0] == "Bio is empty; consider removing or adding content");
    }
}

TEST_CASE("ProfileValidator - PersonProfile Validation", "[validator][person]") {
    SECTION("Valid person profile") {
        PersonProfile profile;
        profile.slug = "john-doe";
        profile.name = "John Doe";
        profile.type = ProfileType::PERSON;
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();
        profile.email = "john@example.com";
        profile.phone = "+989123456789";
        profile.skills = {"C++", "MongoDB"};
        profile.experienceLevel = "Senior";

        auto result = ProfileValidator::validatePersonFields(profile);
        REQUIRE(result.isValid);
        REQUIRE(result.errors.empty());
    }

    SECTION("Invalid email in person profile") {
        PersonProfile profile;
        profile.slug = "john-doe";
        profile.name = "John Doe";
        profile.type = ProfileType::PERSON;
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();
        profile.email = "invalid-email";

        auto result = ProfileValidator::validatePersonFields(profile);
        REQUIRE_FALSE(result.isValid);
        REQUIRE(std::find(result.errors.begin(), result.errors.end(), "Invalid email format") != result.errors.end());
    }

    SECTION("Invalid phone in person profile") {
        PersonProfile profile;
        profile.slug = "john-doe";
        profile.name = "John Doe";
        profile.type = ProfileType::PERSON;
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();
        profile.phone = "09123456789";  // Missing +

        auto result = ProfileValidator::validatePersonFields(profile);
        REQUIRE_FALSE(result.isValid);
        REQUIRE(std::find(result.errors.begin(), result.errors.end(),
            "Invalid phone format. Use international format: +CountryCodeNumber (e.g., +989123456789)") != result.errors.end());
    }
}

TEST_CASE("ProfileValidator - BusinessProfile Validation", "[validator][business]") {
    SECTION("Valid business profile") {
        BusinessProfile profile;
        profile.slug = "acme-corp";
        profile.name = "Acme Corporation";
        profile.type = ProfileType::BUSINESS;
        profile.companyName = "Acme Corporation";  // Required for business
        profile.industry = "Technology";
        profile.foundedYear = 2020;
        profile.businessEmail = "contact@acme.com";
        profile.businessPhone = "+14155551234";
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();

        auto result = ProfileValidator::validateBusinessFields(profile);
        REQUIRE(result.isValid);
        REQUIRE(result.errors.empty());
    }

    SECTION("Business profile missing company name") {
        BusinessProfile profile;
        profile.slug = "acme-corp";
        profile.name = "Acme Corporation";
        profile.type = ProfileType::BUSINESS;
        // companyName is missing - required for business
        profile.industry = "Technology";
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();

        auto result = ProfileValidator::validateBusinessFields(profile);
        REQUIRE_FALSE(result.isValid);
        REQUIRE(result.errors[0] == "Company name is required for business profiles");
    }

    SECTION("Invalid industry") {
        BusinessProfile profile;
        profile.slug = "acme-corp";
        profile.name = "Acme Corporation";
        profile.type = ProfileType::BUSINESS;
        profile.companyName = "Acme Corporation";
        profile.industry = "InvalidIndustry";  // Not in allowed list
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();

        auto result = ProfileValidator::validateBusinessFields(profile);
        REQUIRE_FALSE(result.isValid);
        REQUIRE(result.errors[0].find("Invalid industry category") != std::string::npos);
    }

    SECTION("Invalid founded year - too old") {
        BusinessProfile profile;
        profile.slug = "acme-corp";
        profile.name = "Acme Corporation";
        profile.type = ProfileType::BUSINESS;
        profile.companyName = "Acme Corporation";
        profile.industry = "Technology";
        profile.foundedYear = 1700;  // Too old
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();

        auto result = ProfileValidator::validateBusinessFields(profile);
        REQUIRE_FALSE(result.isValid);
        REQUIRE(result.errors[0] == "Founded year must be 1800 or later");
    }

    SECTION("Invalid founded year - future") {
        BusinessProfile profile;
        profile.slug = "acme-corp";
        profile.name = "Acme Corporation";
        profile.type = ProfileType::BUSINESS;
        profile.companyName = "Acme Corporation";
        profile.industry = "Technology";
        int futureYear = ProfileValidator::getCurrentYear() + 2;
        profile.foundedYear = futureYear;  // Too far in future
        profile.isPublic = true;
        profile.createdAt = std::chrono::system_clock::now();

        auto result = ProfileValidator::validateBusinessFields(profile);
        REQUIRE_FALSE(result.isValid);
        REQUIRE(result.errors[0].find("Founded year cannot be more than 1 year in the future") != std::string::npos);
    }
}