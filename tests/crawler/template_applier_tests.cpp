#include <catch2/catch_test_macros.hpp>
#include "../../include/search_engine/crawler/templates/TemplateApplier.h"
#include "../../include/search_engine/crawler/templates/TemplateTypes.h"
#include "../../include/search_engine/crawler/models/CrawlConfig.h"

using namespace search_engine::crawler::templates;

TEST_CASE("TemplateApplier - Config Application", "[templates]") {
    SECTION("Apply All Template Config Fields") {
        TemplateDefinition templateDef;
        templateDef.config.maxPages = 500;
        templateDef.config.maxDepth = 3;
        templateDef.config.politenessDelayMs = 2000;
        templateDef.config.spaRenderingEnabled = true;
        templateDef.config.extractTextContent = true;
        
        CrawlConfig config;
        // Set some defaults
        config.maxPages = 1000;
        config.maxDepth = 5;
        config.politenessDelay = std::chrono::milliseconds(500);
        config.spaRenderingEnabled = false;
        config.extractTextContent = false;
        
        applyTemplateToConfig(templateDef, config);
        
        REQUIRE(config.maxPages == 500);
        REQUIRE(config.maxDepth == 3);
        REQUIRE(config.politenessDelay.count() == 2000);
        REQUIRE(config.spaRenderingEnabled == true);
        REQUIRE(config.extractTextContent == true);
    }
    
    SECTION("Apply Partial Template Config") {
        TemplateDefinition templateDef;
        templateDef.config.maxPages = 300;
        templateDef.config.spaRenderingEnabled = true;
        // Other fields not set
        
        CrawlConfig config;
        config.maxPages = 1000;
        config.maxDepth = 5;
        config.politenessDelay = std::chrono::milliseconds(500);
        config.spaRenderingEnabled = false;
        config.extractTextContent = false;
        
        applyTemplateToConfig(templateDef, config);
        
        // Only set fields should be updated
        REQUIRE(config.maxPages == 300);
        REQUIRE(config.spaRenderingEnabled == true);
        
        // Unset fields should remain unchanged
        REQUIRE(config.maxDepth == 5);
        REQUIRE(config.politenessDelay.count() == 500);
        REQUIRE(config.extractTextContent == false);
    }
    
    SECTION("Apply Empty Template Config") {
        TemplateDefinition templateDef;
        // No config fields set
        
        CrawlConfig config;
        config.maxPages = 1000;
        config.maxDepth = 5;
        config.politenessDelay = std::chrono::milliseconds(500);
        config.spaRenderingEnabled = false;
        config.extractTextContent = false;
        
        applyTemplateToConfig(templateDef, config);
        
        // All fields should remain unchanged
        REQUIRE(config.maxPages == 1000);
        REQUIRE(config.maxDepth == 5);
        REQUIRE(config.politenessDelay.count() == 500);
        REQUIRE(config.spaRenderingEnabled == false);
        REQUIRE(config.extractTextContent == false);
    }
    
    SECTION("Apply Selector Patterns") {
        TemplateDefinition templateDef;
        templateDef.patterns.articleSelectors = {"article", ".post", ".story"};
        templateDef.patterns.titleSelectors = {"h1", ".headline", ".title"};
        templateDef.patterns.contentSelectors = {".content", ".body", ".article-body"};
        
        CrawlConfig config;
        
        applyTemplateToConfig(templateDef, config);
        
        REQUIRE(config.articleSelectors.size() == 3);
        REQUIRE(config.titleSelectors.size() == 3);
        REQUIRE(config.contentSelectors.size() == 3);
        
        REQUIRE(config.articleSelectors[0] == "article");
        REQUIRE(config.articleSelectors[1] == ".post");
        REQUIRE(config.articleSelectors[2] == ".story");
        
        REQUIRE(config.titleSelectors[0] == "h1");
        REQUIRE(config.titleSelectors[1] == ".headline");
        REQUIRE(config.titleSelectors[2] == ".title");
        
        REQUIRE(config.contentSelectors[0] == ".content");
        REQUIRE(config.contentSelectors[1] == ".body");
        REQUIRE(config.contentSelectors[2] == ".article-body");
    }
}
