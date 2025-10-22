#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <vector>
#include "../../include/search_engine/crawler/templates/TemplateRegistry.h"
#include "../../include/search_engine/crawler/templates/TemplateApplier.h"
#include "../../include/search_engine/crawler/templates/TemplateStorage.h"
#include "../../include/search_engine/crawler/templates/PrebuiltTemplates.h"
#include "../../include/search_engine/crawler/models/CrawlConfig.h"

using namespace search_engine::crawler::templates;

TEST_CASE("Template System Performance", "[templates][performance]") {
    SECTION("Template Registry Performance") {
        auto& registry = TemplateRegistry::instance();
        
        // Clear registry
        registry.removeTemplate("news-site");
        registry.removeTemplate("ecommerce-site");
        registry.removeTemplate("blog-site");
        registry.removeTemplate("corporate-site");
        registry.removeTemplate("documentation-site");
        registry.removeTemplate("forum-site");
        registry.removeTemplate("social-media");
        
        // Seed prebuilt templates
        seedPrebuiltTemplates();
        
        // Test template retrieval performance
        const int iterations = 10000;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            auto newsTemplate = registry.getTemplate("news-site");
            auto ecommerceTemplate = registry.getTemplate("ecommerce-site");
            auto blogTemplate = registry.getTemplate("blog-site");
            REQUIRE(newsTemplate.has_value());
            REQUIRE(ecommerceTemplate.has_value());
            REQUIRE(blogTemplate.has_value());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Should be able to retrieve templates very quickly
        REQUIRE(duration.count() < 100000); // Less than 100ms for 10k operations
        
        // Test template listing performance
        start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 1000; ++i) {
            auto templates = registry.listTemplates();
            REQUIRE(templates.size() >= 7);
        }
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Should be able to list templates quickly
        REQUIRE(duration.count() < 50000); // Less than 50ms for 1k operations
    }
    
    SECTION("Template Application Performance") {
        auto& registry = TemplateRegistry::instance();
        seedPrebuiltTemplates();
        
        auto newsTemplate = registry.getTemplate("news-site");
        REQUIRE(newsTemplate.has_value());
        
        // Test template application performance
        const int iterations = 10000;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            CrawlConfig config;
            applyTemplateToConfig(*newsTemplate, config);
            
            // Verify application worked
            REQUIRE(config.maxPages == 500);
            REQUIRE(config.maxDepth == 3);
            REQUIRE(config.spaRenderingEnabled == true);
            REQUIRE(config.extractTextContent == true);
            REQUIRE(config.politenessDelay.count() == 1000);
            REQUIRE(config.articleSelectors.size() > 0);
            REQUIRE(config.titleSelectors.size() > 0);
            REQUIRE(config.contentSelectors.size() > 0);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Should be able to apply templates very quickly
        REQUIRE(duration.count() < 200000); // Less than 200ms for 10k operations
    }
    
    SECTION("Template JSON Serialization Performance") {
        // Create a test template
        TemplateDefinition testTemplate;
        testTemplate.name = "performance-test-template";
        testTemplate.description = "Template for performance testing";
        testTemplate.config.maxPages = 1000;
        testTemplate.config.maxDepth = 5;
        testTemplate.config.spaRenderingEnabled = true;
        testTemplate.config.extractTextContent = true;
        testTemplate.config.politenessDelayMs = 1000;
        testTemplate.patterns.articleSelectors = {"article", ".post", ".story", ".news-item", ".article-item"};
        testTemplate.patterns.titleSelectors = {"h1", ".headline", ".title", ".article-title", ".post-title"};
        testTemplate.patterns.contentSelectors = {".content", ".body", ".article-body", ".post-content", ".story-content"};
        
        // Test JSON serialization performance
        const int iterations = 5000;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            auto json = toJson(testTemplate);
            auto deserialized = fromJson(json);
            
            // Verify serialization worked
            REQUIRE(deserialized.name == testTemplate.name);
            REQUIRE(deserialized.config.maxPages.value() == testTemplate.config.maxPages.value());
            REQUIRE(deserialized.patterns.articleSelectors.size() == testTemplate.patterns.articleSelectors.size());
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Should be able to serialize/deserialize quickly
        REQUIRE(duration.count() < 100000); // Less than 100ms for 5k operations
    }
    
    SECTION("Template Registry Concurrent Access") {
        auto& registry = TemplateRegistry::instance();
        
        // Clear registry
        registry.removeTemplate("news-site");
        registry.removeTemplate("ecommerce-site");
        registry.removeTemplate("blog-site");
        registry.removeTemplate("corporate-site");
        registry.removeTemplate("documentation-site");
        registry.removeTemplate("forum-site");
        registry.removeTemplate("social-media");
        
        // Seed prebuilt templates
        seedPrebuiltTemplates();
        
        // Test concurrent access (simulated with rapid sequential access)
        const int iterations = 1000;
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::string> templateNames = {
            "news-site", "ecommerce-site", "blog-site", "corporate-site",
            "documentation-site", "forum-site", "social-media"
        };
        
        for (int i = 0; i < iterations; ++i) {
            for (const auto& name : templateNames) {
                auto template = registry.getTemplate(name);
                REQUIRE(template.has_value());
                
                // Simulate some work with the template
                CrawlConfig config;
                applyTemplateToConfig(*template, config);
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Should handle concurrent access efficiently
        REQUIRE(duration.count() < 500000); // Less than 500ms for 7k operations
    }
}
