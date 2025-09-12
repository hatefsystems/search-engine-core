#pragma once
#include "TemplateTypes.h"
#include "../../crawler/models/CrawlConfig.h"

namespace search_engine {
namespace crawler {
namespace templates {

inline void applyTemplateToConfig(const TemplateDefinition& def, CrawlConfig& cfg) {
    // Apply config overrides
    if (def.config.maxPages.has_value()) {
        cfg.maxPages = static_cast<size_t>(*def.config.maxPages);
    }
    if (def.config.maxDepth.has_value()) {
        cfg.maxDepth = static_cast<size_t>(*def.config.maxDepth);
    }
    if (def.config.politenessDelayMs.has_value()) {
        cfg.politenessDelay = std::chrono::milliseconds(*def.config.politenessDelayMs);
    }
    if (def.config.spaRenderingEnabled.has_value()) {
        cfg.spaRenderingEnabled = *def.config.spaRenderingEnabled;
    }
    if (def.config.extractTextContent.has_value()) {
        cfg.extractTextContent = *def.config.extractTextContent;
    }
    
    // Apply selector patterns
    if (!def.patterns.articleSelectors.empty()) {
        cfg.articleSelectors = def.patterns.articleSelectors;
    }
    if (!def.patterns.titleSelectors.empty()) {
        cfg.titleSelectors = def.patterns.titleSelectors;
    }
    if (!def.patterns.contentSelectors.empty()) {
        cfg.contentSelectors = def.patterns.contentSelectors;
    }
}

} // namespace templates
} // namespace crawler
} // namespace search_engine


