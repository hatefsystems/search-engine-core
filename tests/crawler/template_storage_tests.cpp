#include <catch2/catch_test_macros.hpp>
#include "../../include/search_engine/crawler/templates/TemplateStorage.h"
#include "../../include/search_engine/crawler/templates/TemplateRegistry.h"
#include "../../include/search_engine/crawler/templates/TemplateTypes.h"
#include <fstream>
#include <filesystem>

using namespace search_engine::crawler::templates;

TEST_CASE("TemplateStorage - JSON Serialization", "[templates]") {
    SECTION("To JSON and From JSON Roundtrip") {
        TemplateDefinition original;
        original.name = "test-storage";
        original.description = "Storage test template";
        original.config.maxPages = 500;
        original.config.maxDepth = 3;
        original.config.spaRenderingEnabled = true;
        original.config.extractTextContent = true;
        original.config.politenessDelayMs = 1000;
        original.patterns.articleSelectors = {"article", ".post", ".story"};
        original.patterns.titleSelectors = {"h1", ".headline"};
        original.patterns.contentSelectors = {".content", ".body"};
        
        // Convert to JSON
        auto json = toJson(original);
        
        // Convert back from JSON
        auto restored = fromJson(json);
        
        // Verify roundtrip
        REQUIRE(restored.name == original.name);
        REQUIRE(restored.description == original.description);
        REQUIRE(restored.config.maxPages == original.config.maxPages);
        REQUIRE(restored.config.maxDepth == original.config.maxDepth);
        REQUIRE(restored.config.spaRenderingEnabled == original.config.spaRenderingEnabled);
        REQUIRE(restored.config.extractTextContent == original.config.extractTextContent);
        REQUIRE(restored.config.politenessDelayMs == original.config.politenessDelayMs);
        REQUIRE(restored.patterns.articleSelectors == original.patterns.articleSelectors);
        REQUIRE(restored.patterns.titleSelectors == original.patterns.titleSelectors);
        REQUIRE(restored.patterns.contentSelectors == original.patterns.contentSelectors);
    }
    
    SECTION("Partial Config JSON") {
        TemplateDefinition def;
        def.name = "partial";
        def.description = "Partial config";
        def.config.maxPages = 100; // Only this field set
        
        auto json = toJson(def);
        auto restored = fromJson(json);
        
        REQUIRE(restored.name == "partial");
        REQUIRE(restored.config.maxPages == 100);
        REQUIRE_FALSE(restored.config.maxDepth.has_value());
        REQUIRE_FALSE(restored.config.spaRenderingEnabled.has_value());
    }
    
    SECTION("Empty Patterns") {
        TemplateDefinition def;
        def.name = "empty-patterns";
        def.patterns.articleSelectors = {};
        def.patterns.titleSelectors = {};
        def.patterns.contentSelectors = {};
        
        auto json = toJson(def);
        auto restored = fromJson(json);
        
        REQUIRE(restored.patterns.articleSelectors.empty());
        REQUIRE(restored.patterns.titleSelectors.empty());
        REQUIRE(restored.patterns.contentSelectors.empty());
    }
}

TEST_CASE("TemplateStorage - File Operations", "[templates]") {
    const std::string testFile = "/tmp/test_templates.json";
    
    // Clean up any existing test file
    std::filesystem::remove(testFile);
    
    SECTION("Save and Load Templates") {
        auto& registry = TemplateRegistry::instance();
        
        // Add test templates
        TemplateDefinition def1, def2;
        def1.name = "file-test-1";
        def1.description = "First file test";
        def2.name = "file-test-2";
        def2.description = "Second file test";
        
        registry.upsertTemplate(def1);
        registry.upsertTemplate(def2);
        
        // Save to file
        saveTemplatesToFile(testFile);
        
        // Clear registry
        registry.removeTemplate("file-test-1");
        registry.removeTemplate("file-test-2");
        
        // Load from file
        loadTemplatesFromFile(testFile);
        
        // Verify loaded
        auto loaded1 = registry.getTemplate("file-test-1");
        auto loaded2 = registry.getTemplate("file-test-2");
        
        REQUIRE(loaded1.has_value());
        REQUIRE(loaded2.has_value());
        REQUIRE(loaded1->description == "First file test");
        REQUIRE(loaded2->description == "Second file test");
    }
    
    SECTION("Load Non-existent File") {
        // Should not crash
        loadTemplatesFromFile("/tmp/non_existent_file.json");
    }
    
    SECTION("Load Malformed JSON") {
        // Create malformed JSON file
        std::ofstream file(testFile);
        file << "{ invalid json }";
        file.close();
        
        // Should not crash
        loadTemplatesFromFile(testFile);
    }
    
    // Clean up
    std::filesystem::remove(testFile);
}
