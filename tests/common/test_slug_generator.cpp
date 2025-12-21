#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "../../include/search_engine/common/SlugGenerator.h"

using namespace search_engine::common;

TEST_CASE("SlugGenerator - Basic Slug Generation", "[sluggenerator][basic]") {
    SECTION("Simple ASCII names") {
        REQUIRE(SlugGenerator::generateSlug("John Doe") == "john-doe");
        REQUIRE(SlugGenerator::generateSlug("Test User") == "test-user");
        REQUIRE(SlugGenerator::generateSlug("Hello World") == "hello-world");
    }

    SECTION("Names with special characters") {
        REQUIRE(SlugGenerator::generateSlug("John's Profile") == "john-s-profile");
        REQUIRE(SlugGenerator::generateSlug("User@Example.com") == "user-example-com");
        REQUIRE(SlugGenerator::generateSlug("Test.User_Name") == "test-user-name");
    }

    SECTION("Names with numbers") {
        REQUIRE(SlugGenerator::generateSlug("User123") == "user123");
        REQUIRE(SlugGenerator::generateSlug("Test 2023") == "test-2023");
    }

    SECTION("Empty and whitespace names") {
        REQUIRE(SlugGenerator::generateSlug("") == "profile");
        REQUIRE(SlugGenerator::generateSlug("   ") == "profile");
        REQUIRE(SlugGenerator::generateSlug("\t\n") == "profile");
    }

    SECTION("Single character names") {
        REQUIRE(SlugGenerator::generateSlug("A") == "a");
        REQUIRE(SlugGenerator::generateSlug("1") == "1");
        REQUIRE(SlugGenerator::generateSlug("-") == "profile");
    }
}

TEST_CASE("SlugGenerator - Unicode and Persian Support", "[sluggenerator][unicode][persian]") {
    SECTION("Persian names") {
        REQUIRE(SlugGenerator::generateSlug("علی رضایی") == "علی-رضایی");
        REQUIRE(SlugGenerator::generateSlug("محمد علی") == "محمد-علی");
        REQUIRE(SlugGenerator::generateSlug("فاطمه کریمی") == "فاطمه-کریمی");
    }

    SECTION("Mixed Persian-English names") {
        REQUIRE(SlugGenerator::generateSlug("Ali رضایی") == "ali-رضایی");
        REQUIRE(SlugGenerator::generateSlug("علی Reza") == "علی-reza");
        REQUIRE(SlugGenerator::generateSlug("Ali رضا 123") == "ali-رضا-123");
    }

    SECTION("Persian normalization") {
        REQUIRE(SlugGenerator::generateSlug("يوسف") == "یوسف"); // Arabic ye -> Persian ye
        REQUIRE(SlugGenerator::generateSlug("كتاب") == "کتاب"); // Arabic kaf -> Persian kaf
    }

    SECTION("Arabic numerals to ASCII") {
        REQUIRE(SlugGenerator::generateSlug("علی۱۲۳") == "علی۱۲۳");
        REQUIRE(SlugGenerator::generateSlug("user٠١٢") == "user012");
    }
}

TEST_CASE("SlugGenerator - Collision Resolution", "[sluggenerator][collision]") {
    SECTION("Basic collision resolution") {
        auto existsFunc = [](const std::string& slug) {
            return slug == "john-doe";
        };

        REQUIRE(SlugGenerator::resolveSlugConflict("john-doe", existsFunc) == "john-doe-2");
    }

    SECTION("Multiple collisions") {
        auto existsFunc = [](const std::string& slug) {
            return slug == "test" || slug == "test-2" || slug == "test-3";
        };

        REQUIRE(SlugGenerator::resolveSlugConflict("test", existsFunc) == "test-4");
    }

    SECTION("No collision") {
        auto existsFunc = [](const std::string& slug) {
            return false; // Nothing exists
        };

        REQUIRE(SlugGenerator::resolveSlugConflict("available-slug", existsFunc) == "available-slug");
    }

    SECTION("Collision with existing numbered slugs") {
        auto existsFunc = [](const std::string& slug) {
            return slug == "user" || slug == "user-2" || slug == "user-3" || slug == "user-4";
        };

        REQUIRE(SlugGenerator::resolveSlugConflict("user", existsFunc) == "user-5");
    }
}

TEST_CASE("SlugGenerator - Reserved Words", "[sluggenerator][reserved]") {
    SECTION("Common reserved words") {
        REQUIRE(SlugGenerator::isReservedSlug("api"));
        REQUIRE(SlugGenerator::isReservedSlug("admin"));
        REQUIRE(SlugGenerator::isReservedSlug("login"));
        REQUIRE(SlugGenerator::isReservedSlug("profile"));
        REQUIRE(SlugGenerator::isReservedSlug("search"));
    }

    SECTION("Case insensitive") {
        REQUIRE(SlugGenerator::isReservedSlug("API"));
        REQUIRE(SlugGenerator::isReservedSlug("Admin"));
        REQUIRE(SlugGenerator::isReservedSlug("PROFILE"));
    }

    SECTION("Non-reserved words") {
        REQUIRE_FALSE(SlugGenerator::isReservedSlug("john-doe"));
        REQUIRE_FALSE(SlugGenerator::isReservedSlug("علی-رضایی"));
        REQUIRE_FALSE(SlugGenerator::isReservedSlug("my-company"));
        REQUIRE_FALSE(SlugGenerator::isReservedSlug("developer123"));
    }

    SECTION("Empty string") {
        REQUIRE(SlugGenerator::isReservedSlug(""));
    }
}

TEST_CASE("SlugGenerator - Edge Cases", "[sluggenerator][edge-cases]") {
    SECTION("Very long names") {
        std::string longName(200, 'a');
        std::string slug = SlugGenerator::generateSlug(longName);
        REQUIRE(slug.length() <= 100);
        REQUIRE(slug.substr(0, 100) == std::string(100, 'a'));
    }

    SECTION("Names with only special characters") {
        REQUIRE(SlugGenerator::generateSlug("!@#$%^&*()") == "profile");
        REQUIRE(SlugGenerator::generateSlug("...---...") == "profile");
        REQUIRE(SlugGenerator::generateSlug("!@#$%") == "profile");
    }

    SECTION("Mixed special and valid characters") {
        REQUIRE(SlugGenerator::generateSlug("John@Doe.com") == "john-doe-com");
        REQUIRE(SlugGenerator::generateSlug("Test.User_123") == "test-user-123");
    }

    SECTION("Multiple spaces and hyphens") {
        REQUIRE(SlugGenerator::generateSlug("John    Doe") == "john-doe");
        REQUIRE(SlugGenerator::generateSlug("test--user") == "test-user");
        REQUIRE(SlugGenerator::generateSlug("---test---") == "test");
    }

    SECTION("Unicode edge cases") {
        REQUIRE(SlugGenerator::generateSlug("тест") == "test"); // Cyrillic
        REQUIRE(SlugGenerator::generateSlug("José María") == "jose-maria"); // Spanish accents
        REQUIRE(SlugGenerator::generateSlug("Müller") == "muller"); // German umlaut
    }
}

TEST_CASE("SlugGenerator - Transliterations", "[sluggenerator][transliteration]") {
    SECTION("German umlauts") {
        REQUIRE(SlugGenerator::generateSlug("Müller") == "muller");
        REQUIRE(SlugGenerator::generateSlug("Schröder") == "schroder");
        REQUIRE(SlugGenerator::generateSlug("Göteborg") == "goteborg");
    }

    SECTION("French accents") {
        REQUIRE(SlugGenerator::generateSlug("François") == "francois");
        REQUIRE(SlugGenerator::generateSlug("Mérignac") == "merignac");
        REQUIRE(SlugGenerator::generateSlug("Naïve") == "naive");
    }

    SECTION("Spanish accents") {
        REQUIRE(SlugGenerator::generateSlug("José") == "jose");
        REQUIRE(SlugGenerator::generateSlug("Niño") == "nino");
        REQUIRE(SlugGenerator::generateSlug("Señor") == "senor");
    }

    SECTION("Basic Cyrillic") {
        REQUIRE(SlugGenerator::generateSlug("тест") == "test");
        REQUIRE(SlugGenerator::generateSlug("привет") == "privet");
        REQUIRE(SlugGenerator::generateSlug("Александр") == "aleksandr");
    }
}

TEST_CASE("SlugGenerator - Normalization Consistency", "[sluggenerator][normalization]") {
    SECTION("Consistent results for same input") {
        std::string input = "John Doe & Company!";
        std::string result1 = SlugGenerator::generateSlug(input);
        std::string result2 = SlugGenerator::generateSlug(input);
        REQUIRE(result1 == result2);
        REQUIRE(result1 == "john-doe-company");
    }

    SECTION("Case insensitive") {
        REQUIRE(SlugGenerator::generateSlug("John Doe") == SlugGenerator::generateSlug("JOHN DOE"));
        REQUIRE(SlugGenerator::generateSlug("علی") == SlugGenerator::generateSlug("علی"));
    }

    SECTION("Trimming and cleaning") {
        REQUIRE(SlugGenerator::generateSlug("  John Doe  ") == "john-doe");
        REQUIRE(SlugGenerator::generateSlug("-John-Doe-") == "john-doe");
        REQUIRE(SlugGenerator::generateSlug("John   Doe") == "john-doe");
    }
}

TEST_CASE("SlugGenerator - Integration Tests", "[sluggenerator][integration]") {
    SECTION("Complete workflow") {
        // Generate slug
        std::string slug = SlugGenerator::generateSlug("علی Reza & Company!");
        REQUIRE(slug == "علی-reza-company");

        // Check if reserved
        REQUIRE_FALSE(SlugGenerator::isReservedSlug(slug));

        // Resolve collision
        auto existsFunc = [](const std::string& s) { return s == "علی-reza-company"; };
        std::string resolved = SlugGenerator::resolveSlugConflict(slug, existsFunc);
        REQUIRE(resolved == "علی-reza-company-2");
    }

    SECTION("Real-world examples") {
        std::vector<std::pair<std::string, std::string>> testCases = {
            {"محمد علی رضایی", "محمد-علی-رضایی"},
            {"John Smith Jr.", "john-smith-jr"},
            {"Tech Corp & Co.", "tech-corp-co"},
            {"José María González", "jose-maria-gonzalez"},
            {"Михаил Горбачёв", "mihail-gorbachev"},
            {"李小明", "li"}, // Limited Chinese support
            {"Yamada Taro", "yamada-taro"}
        };

        for (const auto& [input, expected] : testCases) {
            std::string result = SlugGenerator::generateSlug(input);
            INFO("Input: " << input << ", Expected: " << expected << ", Got: " << result);
            REQUIRE(result == expected);
        }
    }
}
