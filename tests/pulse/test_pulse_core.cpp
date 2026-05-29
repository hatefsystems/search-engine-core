#include "../../include/search_engine/pulse/PulseQueryNormalizer.h"
#include "../../include/search_engine/pulse/PulseScoring.h"
#include <catch2/catch_test_macros.hpp>

using search_engine::pulse::PulseQueryNormalizer;
using search_engine::pulse::PulseRelativeScore;
using search_engine::pulse::PulseTrendScore;

TEST_CASE("Pulse query normalization handles Persian search variants", "[pulse][normalizer]") {
    REQUIRE(PulseQueryNormalizer::normalize("  كیف   مدرسه  ") == "کیف مدرسه");
    REQUIRE(PulseQueryNormalizer::normalize("می‌روم") == "می روم");
    REQUIRE(PulseQueryNormalizer::normalize("خبرهای ۱۴۰۵، اقتصاد") == "خبرهای 1405 اقتصاد");
    REQUIRE(PulseQueryNormalizer::normalize("HATEF   Search!") == "hatef search");
}

TEST_CASE("Pulse language estimate is lightweight and script based", "[pulse][normalizer]") {
    REQUIRE(PulseQueryNormalizer::estimateLanguage("جست‌وجوی فارسی") == "fa");
    REQUIRE(PulseQueryNormalizer::estimateLanguage("hello search") == "en");
    REQUIRE(PulseQueryNormalizer::estimateLanguage("فارسی search") == "mixed");
}

TEST_CASE("Pulse public query safety rejects sensitive-looking values", "[pulse][safety]") {
    REQUIRE(PulseQueryNormalizer::isPublicSafeQuery("اخبار امروز", PulseQueryNormalizer::normalize("اخبار امروز")));
    REQUIRE_FALSE(PulseQueryNormalizer::isPublicSafeQuery("person@example.com", PulseQueryNormalizer::normalize("person@example.com")));
    REQUIRE_FALSE(PulseQueryNormalizer::isPublicSafeQuery("09123456789", PulseQueryNormalizer::normalize("09123456789")));
    REQUIRE_FALSE(PulseQueryNormalizer::isPublicSafeQuery("https://example.test/path", PulseQueryNormalizer::normalize("https://example.test/path")));
}

TEST_CASE("Pulse relative scores stay within public range", "[pulse][scoring]") {
    REQUIRE(PulseRelativeScore::fromCount(0, 10) == 0);
    REQUIRE(PulseRelativeScore::fromCount(5, 10) == 50);
    REQUIRE(PulseRelativeScore::fromCount(10, 10) == 100);
    REQUIRE(PulseRelativeScore::fromCount(20, 10) == 100);
}

TEST_CASE("Pulse trend score favors current lift without exposing counts", "[pulse][scoring]") {
    REQUIRE(PulseTrendScore::calculate(0, 10) == 0);
    REQUIRE(PulseTrendScore::calculate(2, 0) == 50);
    REQUIRE(PulseTrendScore::calculate(3, 0) == 100);
    REQUIRE(PulseTrendScore::calculate(15, 10) == 25);
    REQUIRE(PulseTrendScore::calculate(10, 10) == 0);
}

