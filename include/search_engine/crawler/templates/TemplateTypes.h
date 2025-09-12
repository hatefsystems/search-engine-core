#pragma once
#include <string>
#include <vector>
#include <optional>

namespace search_engine {
namespace crawler {
namespace templates {

struct CrawlConfigOverrides {
    std::optional<int> maxPages;
    std::optional<int> maxDepth;
    std::optional<bool> spaRenderingEnabled;
    std::optional<bool> extractTextContent;
    std::optional<int> politenessDelayMs;
};

struct SelectorPatterns {
    std::vector<std::string> articleSelectors;
    std::vector<std::string> titleSelectors;
    std::vector<std::string> contentSelectors;
};

struct TemplateDefinition {
    std::string name;
    std::string description;
    CrawlConfigOverrides config;
    SelectorPatterns patterns;
};

} // namespace templates
} // namespace crawler
} // namespace search_engine


