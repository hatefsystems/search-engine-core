#include <catch2/catch_all.hpp>
#include "../../include/search_engine/crawler/templates/TemplateValidator.h"

using namespace search_engine::crawler::templates;

TEST_CASE("TemplateValidator accepts valid RFC example", "[templates]") {
    nlohmann::json body = {
        {"name","news-site"},
        {"description","Template for news websites"},
        {"config", {{"maxPages",500},{"maxDepth",3},{"spaRenderingEnabled",true},{"extractTextContent",true},{"politenessDelay",1000}}},
        {"patterns", {{"articleSelectors", nlohmann::json::array({"article",".post",".story"})},
                       {"titleSelectors", nlohmann::json::array({"h1",".headline",".title"})},
                       {"contentSelectors", nlohmann::json::array({".content",".body",".article-body"})}}}
    };
    auto res = validateTemplateJson(body);
    REQUIRE(res.valid);
}

TEST_CASE("TemplateValidator rejects missing name", "[templates]") {
    nlohmann::json body = {
        {"description","x"}
    };
    auto res = validateTemplateJson(body);
    REQUIRE_FALSE(res.valid);
}

TEST_CASE("TemplateValidator rejects bad config types", "[templates]") {
    nlohmann::json body = {
        {"name","x"},
        {"config", {{"maxPages", -1}, {"politenessDelay", -5}}}
    };
    auto res = validateTemplateJson(body);
    REQUIRE_FALSE(res.valid);
}

TEST_CASE("TemplateValidator rejects non-string selector arrays", "[templates]") {
    nlohmann::json body = {
        {"name","x"},
        {"patterns", {{"articleSelectors", nlohmann::json::array({1,2,3})}}}
    };
    auto res = validateTemplateJson(body);
    REQUIRE_FALSE(res.valid);
}

TEST_CASE("TemplateValidator - Name Validation", "[templates]") {
    SECTION("Valid Template Names") {
        REQUIRE(isValidTemplateName("news-site"));
        REQUIRE(isValidTemplateName("ecommerce_site"));
        REQUIRE(isValidTemplateName("blog-template"));
        REQUIRE(isValidTemplateName("site123"));
        REQUIRE(isValidTemplateName("a")); // Single character
        REQUIRE(isValidTemplateName("a" + std::string(49, 'b'))); // 50 characters
    }
    
    SECTION("Invalid Template Names") {
        REQUIRE_FALSE(isValidTemplateName("")); // Empty
        REQUIRE_FALSE(isValidTemplateName("a" + std::string(50, 'b'))); // Too long
        REQUIRE_FALSE(isValidTemplateName("site with spaces"));
        REQUIRE_FALSE(isValidTemplateName("site@invalid"));
        REQUIRE_FALSE(isValidTemplateName("site.invalid"));
        REQUIRE_FALSE(isValidTemplateName("site#invalid"));
        REQUIRE_FALSE(isValidTemplateName("site$invalid"));
    }
    
    SECTION("Name Normalization") {
        REQUIRE(normalizeTemplateName("  NEWS-SITE  ") == "news-site");
        REQUIRE(normalizeTemplateName("Ecommerce_Site") == "ecommerce_site");
        REQUIRE(normalizeTemplateName("Blog Template") == "blog template");
        REQUIRE(normalizeTemplateName("") == "");
    }
}

TEST_CASE("TemplateValidator - Config Value Limits", "[templates]") {
    SECTION("MaxPages Limits") {
        nlohmann::json invalidTemplate = {
            {"name", "test-template"},
            {"config", {
                {"maxPages", 10001}  // Too high
            }}
        };
        
        auto result = validateTemplateJson(invalidTemplate);
        REQUIRE_FALSE(result.valid);
        REQUIRE(result.message.find("maxPages must be between 1 and 10000") != std::string::npos);
    }
    
    SECTION("MaxDepth Limits") {
        nlohmann::json invalidTemplate = {
            {"name", "test-template"},
            {"config", {
                {"maxDepth", 11}  // Too high
            }}
        };
        
        auto result = validateTemplateJson(invalidTemplate);
        REQUIRE_FALSE(result.valid);
        REQUIRE(result.message.find("maxDepth must be between 1 and 10") != std::string::npos);
    }
    
    SECTION("PolitenessDelay Limits") {
        nlohmann::json invalidTemplate = {
            {"name", "test-template"},
            {"config", {
                {"politenessDelay", 60001}  // Too high
            }}
        };
        
        auto result = validateTemplateJson(invalidTemplate);
        REQUIRE_FALSE(result.valid);
        REQUIRE(result.message.find("politenessDelay must be between 0 and 60000") != std::string::npos);
    }
}


