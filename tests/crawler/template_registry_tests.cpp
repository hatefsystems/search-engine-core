#include <catch2/catch_test_macros.hpp>
#include "../../include/search_engine/crawler/templates/TemplateRegistry.h"
#include "../../include/search_engine/crawler/templates/TemplateTypes.h"

using namespace search_engine::crawler::templates;

TEST_CASE("TemplateRegistry - Basic Operations", "[templates]") {
    // Clear registry for clean test
    auto& registry = TemplateRegistry::instance();
    
    SECTION("Upsert and Get Template") {
        TemplateDefinition def;
        def.name = "test-template";
        def.description = "Test template";
        def.config.maxPages = 100;
        def.patterns.articleSelectors = {"article", ".post"};
        
        registry.upsertTemplate(def);
        
        auto retrieved = registry.getTemplate("test-template");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->name == "test-template");
        REQUIRE(retrieved->description == "Test template");
        REQUIRE(retrieved->config.maxPages == 100);
        REQUIRE(retrieved->patterns.articleSelectors.size() == 2);
    }
    
    SECTION("List Templates") {
        TemplateDefinition def1, def2;
        def1.name = "template-1";
        def2.name = "template-2";
        
        registry.upsertTemplate(def1);
        registry.upsertTemplate(def2);
        
        auto list = registry.listTemplates();
        REQUIRE(list.size() >= 2);
        
        std::set<std::string> names;
        for (const auto& t : list) {
            names.insert(t.name);
        }
        REQUIRE(names.count("template-1") == 1);
        REQUIRE(names.count("template-2") == 1);
    }
    
    SECTION("Remove Template") {
        TemplateDefinition def;
        def.name = "to-remove";
        registry.upsertTemplate(def);
        
        REQUIRE(registry.getTemplate("to-remove").has_value());
        
        bool removed = registry.removeTemplate("to-remove");
        REQUIRE(removed);
        REQUIRE_FALSE(registry.getTemplate("to-remove").has_value());
        
        // Try removing non-existent
        bool notRemoved = registry.removeTemplate("non-existent");
        REQUIRE_FALSE(notRemoved);
    }
    
    SECTION("Name Uniqueness") {
        TemplateDefinition def1, def2;
        def1.name = "duplicate";
        def1.description = "First";
        def2.name = "duplicate";
        def2.description = "Second";
        
        registry.upsertTemplate(def1);
        registry.upsertTemplate(def2); // Should overwrite
        
        auto retrieved = registry.getTemplate("duplicate");
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->description == "Second"); // Last one wins
    }
}
