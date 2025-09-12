#pragma once
#include "TemplateRegistry.h"

namespace search_engine {
namespace crawler {
namespace templates {

inline void seedPrebuiltTemplates() {
    // News template
    TemplateDefinition news;
    news.name = "news-site";
    news.description = "Template for news websites";
    news.config.maxPages = 500;
    news.config.maxDepth = 3;
    news.config.spaRenderingEnabled = true;
    news.config.extractTextContent = true;
    news.config.politenessDelayMs = 1000;
    news.patterns.articleSelectors = {"article", ".post", ".story"};
    news.patterns.titleSelectors = {"h1", ".headline", ".title"};
    news.patterns.contentSelectors = {".content", ".body", ".article-body"};
    TemplateRegistry::instance().upsertTemplate(news);

    // E-commerce template (basic)
    TemplateDefinition ecommerce;
    ecommerce.name = "ecommerce-site";
    ecommerce.description = "Template for ecommerce product listings";
    ecommerce.config.maxPages = 800;
    ecommerce.config.maxDepth = 4;
    ecommerce.config.extractTextContent = true;
    ecommerce.config.politenessDelayMs = 800;
    ecommerce.patterns.articleSelectors = {".product", ".product-item", ".product-card"};
    ecommerce.patterns.titleSelectors = {"h1", ".product-title", ".title"};
    ecommerce.patterns.contentSelectors = {".description", ".product-description", ".details"};
    TemplateRegistry::instance().upsertTemplate(ecommerce);

    // Blog template
    TemplateDefinition blog;
    blog.name = "blog-site";
    blog.description = "Template for personal blogs and content management systems";
    blog.config.maxPages = 300;
    blog.config.maxDepth = 2;
    blog.config.spaRenderingEnabled = false;
    blog.config.extractTextContent = true;
    blog.config.politenessDelayMs = 1200;
    blog.patterns.articleSelectors = {"article", ".post", ".blog-post", ".entry"};
    blog.patterns.titleSelectors = {"h1", ".post-title", ".entry-title", ".blog-title"};
    blog.patterns.contentSelectors = {".content", ".post-content", ".entry-content", ".blog-content"};
    TemplateRegistry::instance().upsertTemplate(blog);

    // Corporate template
    TemplateDefinition corporate;
    corporate.name = "corporate-site";
    corporate.description = "Template for business websites and corporate pages";
    corporate.config.maxPages = 150;
    corporate.config.maxDepth = 2;
    corporate.config.spaRenderingEnabled = false;
    corporate.config.extractTextContent = true;
    corporate.config.politenessDelayMs = 1000;
    corporate.patterns.articleSelectors = {".page-content", ".main-content", ".content", ".page"};
    corporate.patterns.titleSelectors = {"h1", ".page-title", ".title", ".heading"};
    corporate.patterns.contentSelectors = {".content", ".main-content", ".page-content", ".body"};
    TemplateRegistry::instance().upsertTemplate(corporate);

    // Documentation template
    TemplateDefinition documentation;
    documentation.name = "documentation-site";
    documentation.description = "Template for technical documentation and API references";
    documentation.config.maxPages = 1000;
    documentation.config.maxDepth = 5;
    documentation.config.spaRenderingEnabled = true;
    documentation.config.extractTextContent = true;
    documentation.config.politenessDelayMs = 600;
    documentation.patterns.articleSelectors = {".documentation", ".doc-content", ".content", ".page"};
    documentation.patterns.titleSelectors = {"h1", ".page-title", ".doc-title", ".title"};
    documentation.patterns.contentSelectors = {".content", ".doc-content", ".main-content", ".body"};
    TemplateRegistry::instance().upsertTemplate(documentation);

    // Forum template
    TemplateDefinition forum;
    forum.name = "forum-site";
    forum.description = "Template for discussion forums and community sites";
    forum.config.maxPages = 400;
    forum.config.maxDepth = 3;
    forum.config.spaRenderingEnabled = false;
    forum.config.extractTextContent = true;
    forum.config.politenessDelayMs = 1500;
    forum.patterns.articleSelectors = {".post", ".topic", ".thread", ".message"};
    forum.patterns.titleSelectors = {"h1", ".post-title", ".topic-title", ".thread-title"};
    forum.patterns.contentSelectors = {".content", ".post-content", ".message-content", ".body"};
    TemplateRegistry::instance().upsertTemplate(forum);

    // Social media template
    TemplateDefinition socialMedia;
    socialMedia.name = "social-media";
    socialMedia.description = "Template for social platforms and user-generated content";
    socialMedia.config.maxPages = 200;
    socialMedia.config.maxDepth = 2;
    socialMedia.config.spaRenderingEnabled = true;
    socialMedia.config.extractTextContent = true;
    socialMedia.config.politenessDelayMs = 2000;
    socialMedia.patterns.articleSelectors = {".post", ".tweet", ".status", ".update"};
    socialMedia.patterns.titleSelectors = {"h1", ".post-title", ".status-title", ".title"};
    socialMedia.patterns.contentSelectors = {".content", ".post-content", ".status-content", ".body"};
    TemplateRegistry::instance().upsertTemplate(socialMedia);
}

} // namespace templates
} // namespace crawler
} // namespace search_engine
