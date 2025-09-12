#include <catch2/catch_test_macros.hpp>
#include "../../include/search_engine/crawler/templates/TemplateRegistry.h"
#include "../../include/search_engine/crawler/templates/TemplateApplier.h"
#include "../../include/search_engine/crawler/templates/TemplateStorage.h"
#include "../../include/search_engine/crawler/templates/PrebuiltTemplates.h"
#include "../../include/search_engine/crawler/models/CrawlConfig.h"
#include <nlohmann/json.hpp>

using namespace search_engine::crawler::templates;

TEST_CASE("Template System Integration", "[templates][integration]") {
    SECTION("Prebuilt Templates Loading") {
        // Clear registry first
        auto& registry = TemplateRegistry::instance();
        
        // Seed prebuilt templates
        seedPrebuiltTemplates();
        
        // Verify all expected templates are loaded
        auto templates = registry.listTemplates();
        REQUIRE(templates.size() >= 7); // news, ecommerce, blog, corporate, documentation, forum, social-media
        
        // Check specific templates exist
        auto newsTemplate = registry.getTemplate("news-site");
        REQUIRE(newsTemplate.has_value());
        REQUIRE(newsTemplate->name == "news-site");
        REQUIRE(newsTemplate->config.maxPages.value() == 500);
        REQUIRE(newsTemplate->config.maxDepth.value() == 3);
        REQUIRE(newsTemplate->config.spaRenderingEnabled.value() == true);
        
        auto ecommerceTemplate = registry.getTemplate("ecommerce-site");
        REQUIRE(ecommerceTemplate.has_value());
        REQUIRE(ecommerceTemplate->name == "ecommerce-site");
        REQUIRE(ecommerceTemplate->config.maxPages.value() == 800);
        REQUIRE(ecommerceTemplate->config.maxDepth.value() == 4);
        
        auto blogTemplate = registry.getTemplate("blog-site");
        REQUIRE(blogTemplate.has_value());
        REQUIRE(blogTemplate->name == "blog-site");
        REQUIRE(blogTemplate->config.maxPages.value() == 300);
        REQUIRE(blogTemplate->config.maxDepth.value() == 2);
        REQUIRE(blogTemplate->config.spaRenderingEnabled.value() == false);
    }
    
    SECTION("Template Application to CrawlConfig") {
        // Get a template
        auto& registry = TemplateRegistry::instance();
        seedPrebuiltTemplates();
        auto newsTemplate = registry.getTemplate("news-site");
        REQUIRE(newsTemplate.has_value());
        
        // Create a default config
        CrawlConfig config;
        config.maxPages = 1000; // Different from template
        config.maxDepth = 5;    // Different from template
        config.spaRenderingEnabled = false; // Different from template
        config.extractTextContent = false;  // Different from template
        config.politenessDelay = std::chrono::milliseconds(500); // Different from template
        
        // Apply template
        applyTemplateToConfig(*newsTemplate, config);
        
        // Verify template values were applied
        REQUIRE(config.maxPages == 500);
        REQUIRE(config.maxDepth == 3);
        REQUIRE(config.spaRenderingEnabled == true);
        REQUIRE(config.extractTextContent == true);
        REQUIRE(config.politenessDelay.count() == 1000);
        
        // Verify selector patterns were applied
        REQUIRE(config.articleSelectors.size() > 0);
        REQUIRE(config.titleSelectors.size() > 0);
        REQUIRE(config.contentSelectors.size() > 0);
        
        // Check specific selectors
        REQUIRE(std::find(config.articleSelectors.begin(), config.articleSelectors.end(), "article") != config.articleSelectors.end());
        REQUIRE(std::find(config.titleSelectors.begin(), config.titleSelectors.end(), "h1") != config.titleSelectors.end());
        REQUIRE(std::find(config.contentSelectors.begin(), config.contentSelectors.end(), ".content") != config.contentSelectors.end());
    }
    
    SECTION("Template JSON Serialization") {
        // Create a test template
        TemplateDefinition testTemplate;
        testTemplate.name = "test-template";
        testTemplate.description = "Test template for serialization";
        testTemplate.config.maxPages = 100;
        testTemplate.config.maxDepth = 2;
        testTemplate.config.spaRenderingEnabled = true;
        testTemplate.config.extractTextContent = true;
        testTemplate.config.politenessDelayMs = 500;
        testTemplate.patterns.articleSelectors = {"article", ".post"};
        testTemplate.patterns.titleSelectors = {"h1", ".title"};
        testTemplate.patterns.contentSelectors = {".content", ".body"};
        
        // Convert to JSON
        auto json = toJson(testTemplate);
        
        // Verify JSON structure
        REQUIRE(json["name"] == "test-template");
        REQUIRE(json["description"] == "Test template for serialization");
        REQUIRE(json["config"]["maxPages"] == 100);
        REQUIRE(json["config"]["maxDepth"] == 2);
        REQUIRE(json["config"]["spaRenderingEnabled"] == true);
        REQUIRE(json["config"]["extractTextContent"] == true);
        REQUIRE(json["config"]["politenessDelay"] == 500);
        REQUIRE(json["patterns"]["articleSelectors"].size() == 2);
        REQUIRE(json["patterns"]["titleSelectors"].size() == 2);
        REQUIRE(json["patterns"]["contentSelectors"].size() == 2);
        
        // Convert back from JSON
        auto deserializedTemplate = fromJson(json);
        
        // Verify deserialization
        REQUIRE(deserializedTemplate.name == testTemplate.name);
        REQUIRE(deserializedTemplate.description == testTemplate.description);
        REQUIRE(deserializedTemplate.config.maxPages.value() == testTemplate.config.maxPages.value());
        REQUIRE(deserializedTemplate.config.maxDepth.value() == testTemplate.config.maxDepth.value());
        REQUIRE(deserializedTemplate.config.spaRenderingEnabled.value() == testTemplate.config.spaRenderingEnabled.value());
        REQUIRE(deserializedTemplate.config.extractTextContent.value() == testTemplate.config.extractTextContent.value());
        REQUIRE(deserializedTemplate.config.politenessDelayMs.value() == testTemplate.config.politenessDelayMs.value());
        REQUIRE(deserializedTemplate.patterns.articleSelectors == testTemplate.patterns.articleSelectors);
        REQUIRE(deserializedTemplate.patterns.titleSelectors == testTemplate.patterns.titleSelectors);
        REQUIRE(deserializedTemplate.patterns.contentSelectors == testTemplate.patterns.contentSelectors);
    }
    
    SECTION("Template Registry Operations") {
        auto& registry = TemplateRegistry::instance();
        
        // Clear registry
        registry.removeTemplate("news-site");
        registry.removeTemplate("ecommerce-site");
        registry.removeTemplate("blog-site");
        registry.removeTemplate("corporate-site");
        registry.removeTemplate("documentation-site");
        registry.removeTemplate("forum-site");
        registry.removeTemplate("social-media");
        
        // Test upsert
        TemplateDefinition testTemplate;
        testTemplate.name = "test-upsert";
        testTemplate.description = "Test upsert functionality";
        testTemplate.config.maxPages = 50;
        
        registry.upsertTemplate(testTemplate);
        
        auto retrieved = registry.getTemplate("test-upsert");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "test-upsert");
        REQUIRE(retrieved->config.maxPages.value() == 50);
        
        // Test update
        testTemplate.config.maxPages = 75;
        registry.upsertTemplate(testTemplate);
        
        auto updated = registry.getTemplate("test-upsert");
        REQUIRE(updated.has_value());
        REQUIRE(updated->config.maxPages.value() == 75);
        
        // Test removal
        bool removed = registry.removeTemplate("test-upsert");
        REQUIRE(removed == true);
        
        auto notFound = registry.getTemplate("test-upsert");
        REQUIRE(!notFound.has_value());
    }
}
