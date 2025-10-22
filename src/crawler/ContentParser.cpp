#include "ContentParser.h"
#include <algorithm>
#include <regex>
#include <sstream>
#include <cctype>
#include <iostream>
#include "../../include/Logger.h"
#include "../../include/search_engine/common/UrlSanitizer.h"

ContentParser::ContentParser() {
    LOG_DEBUG("ContentParser constructor called");
}

ContentParser::~ContentParser() {
    LOG_DEBUG("ContentParser destructor called");
}

ParsedContent ContentParser::parse(const std::string& html, const std::string& baseUrl) {
    LOG_INFO("ContentParser::parse called for URL: " + baseUrl + " with HTML length: " + std::to_string(html.length()) + " bytes");
    ParsedContent result;
    
    // Parse HTML using Gumbo
    GumboOutput* output = gumbo_parse(html.c_str());
    
    if (output) {
        LOG_DEBUG("HTML parsed successfully with Gumbo");
        
        // Extract title
        result.title = extractTitle(html).value_or("");
        LOG_DEBUG("Extracted title: " + result.title);
        
        // Extract meta description
        result.metaDescription = extractMetaDescription(html).value_or("");
        LOG_DEBUG("Extracted meta description: " + result.metaDescription);
        
        // Extract text content
        result.textContent = extractText(html);
        LOG_DEBUG("Extracted text content of length: " + std::to_string(result.textContent.length()) + " bytes");
        
        // Extract links
        result.links = extractLinks(html, baseUrl);
        LOG_INFO("Extracted " + std::to_string(result.links.size()) + " links");
        
        // Clean up Gumbo output
        gumbo_destroy_output(&kGumboDefaultOptions, output);
    } else {
        LOG_ERROR("Failed to parse HTML with Gumbo");
    }
    
    return result;
}

std::string ContentParser::extractText(const std::string& html) {
    LOG_DEBUG("ContentParser::extractText called with HTML length: " + std::to_string(html.length()) + " bytes");
    std::string text;
    GumboOutput* output = gumbo_parse(html.c_str());
    
    if (output) {
        extractTextFromNode(output->root, text);
        gumbo_destroy_output(&kGumboDefaultOptions, output);
        LOG_DEBUG("Extracted text of length: " + std::to_string(text.length()) + " bytes");
    } else {
        LOG_ERROR("Failed to parse HTML for text extraction");
    }
    
    return text;
}

std::vector<std::string> ContentParser::extractLinks(const std::string& html, const std::string& baseUrl) {
    LOG_DEBUG("ContentParser::extractLinks called for URL: " + baseUrl + " with HTML length: " + std::to_string(html.length()) + " bytes");
    std::vector<std::string> links;
    GumboOutput* output = gumbo_parse(html.c_str());
    
    if (output) {
        extractLinksFromNode(output->root, baseUrl, links);
        gumbo_destroy_output(&kGumboDefaultOptions, output);
        LOG_INFO("Extracted " + std::to_string(links.size()) + " links from page");
    } else {
        LOG_ERROR("Failed to parse HTML for link extraction");
    }
    
    return links;
}

std::optional<std::string> ContentParser::extractTitle(const std::string& html) {
    LOG_DEBUG("ContentParser::extractTitle called with HTML length: " + std::to_string(html.length()) + " bytes");
    GumboOutput* output = gumbo_parse(html.c_str());
    std::optional<std::string> title;
    
    if (output) {
        GumboNode* root = output->root;
        GumboNode* head = nullptr;
        
        // Find the head node
        for (unsigned int i = 0; i < root->v.element.children.length; ++i) {
            GumboNode* child = static_cast<GumboNode*>(root->v.element.children.data[i]);
            if (child->type == GUMBO_NODE_ELEMENT && child->v.element.tag == GUMBO_TAG_HEAD) {
                head = child;
                break;
            }
        }
        
        if (head) {
            // Find the title node
            for (unsigned int i = 0; i < head->v.element.children.length; ++i) {
                GumboNode* child = static_cast<GumboNode*>(head->v.element.children.data[i]);
                if (child->type == GUMBO_NODE_ELEMENT && child->v.element.tag == GUMBO_TAG_TITLE) {
                    if (child->v.element.children.length > 0) {
                        GumboNode* titleText = static_cast<GumboNode*>(child->v.element.children.data[0]);
                        if (titleText->type == GUMBO_NODE_TEXT) {
                            title = titleText->v.text.text;
                            LOG_DEBUG("Found title: " + title.value());
                        }
                    }
                    break;
                }
            }
        }
        
        gumbo_destroy_output(&kGumboDefaultOptions, output);
    } else {
        LOG_ERROR("Failed to parse HTML for title extraction");
    }
    
    if (!title) {
        LOG_DEBUG("No title found in HTML");
    }
    
    return title;
}

std::optional<std::string> ContentParser::extractMetaDescription(const std::string& html) {
    LOG_DEBUG("ContentParser::extractMetaDescription called with HTML length: " + std::to_string(html.length()) + " bytes");
    GumboOutput* output = gumbo_parse(html.c_str());
    std::optional<std::string> description;
    
    if (output) {
        description = findMetaTag(output->root, "description");
        gumbo_destroy_output(&kGumboDefaultOptions, output);
        
        if (description) {
            LOG_DEBUG("Found meta description: " + description.value());
        } else {
            LOG_DEBUG("No meta description found in HTML");
        }
    } else {
        LOG_ERROR("Failed to parse HTML for meta description extraction");
    }
    
    return description;
}

void ContentParser::extractTextFromNode(const GumboNode* node, std::string& text) {
    if (node->type == GUMBO_NODE_TEXT) {
        text += node->v.text.text;
        text += " ";
    }
    else if (node->type == GUMBO_NODE_ELEMENT) {
        // Skip script and style tags
        if (node->v.element.tag != GUMBO_TAG_SCRIPT && 
            node->v.element.tag != GUMBO_TAG_STYLE) {
            for (unsigned int i = 0; i < node->v.element.children.length; ++i) {
                extractTextFromNode(static_cast<GumboNode*>(node->v.element.children.data[i]), text);
            }
        }
    }
}

void ContentParser::extractLinksFromNode(const GumboNode* node, const std::string& baseUrl, std::vector<std::string>& links) {
    if (node->type == GUMBO_NODE_ELEMENT) {
        if (node->v.element.tag == GUMBO_TAG_A) {
            GumboAttribute* href = gumbo_get_attribute(&node->v.element.attributes, "href");
            if (href) {
                std::string url = normalizeUrl(search_engine::common::sanitizeUrl(href->value), baseUrl);
                if (isValidUrl(url)) {
                    links.push_back(url);
                }
            }
        }
        
        for (unsigned int i = 0; i < node->v.element.children.length; ++i) {
            extractLinksFromNode(static_cast<GumboNode*>(node->v.element.children.data[i]), baseUrl, links);
        }
    }
}

std::optional<std::string> ContentParser::findMetaTag(const GumboNode* node, const std::string& name) {
    if (node->type == GUMBO_NODE_ELEMENT) {
        if (node->v.element.tag == GUMBO_TAG_META) {
            GumboAttribute* nameAttr = gumbo_get_attribute(&node->v.element.attributes, "name");
            GumboAttribute* contentAttr = gumbo_get_attribute(&node->v.element.attributes, "content");
            
            if (nameAttr && contentAttr && nameAttr->value == name) {
                return contentAttr->value;
            }
        }
        
        for (unsigned int i = 0; i < node->v.element.children.length; ++i) {
            auto result = findMetaTag(static_cast<GumboNode*>(node->v.element.children.data[i]), name);
            if (result) {
                return result;
            }
        }
    }
    
    return std::nullopt;
}

std::string ContentParser::normalizeUrl(const std::string& url, const std::string& baseUrl) {
    if (url.empty()) {
        return "";
    }
    
    // If URL is absolute, return as is
    if (url.find("http://") == 0 || url.find("https://") == 0) {
        return url;
    }
    
    // If URL starts with //, add https:
    if (url.find("//") == 0) {
        return "https:" + url;
    }
    
    // If URL starts with /, append to base URL's domain
    if (url[0] == '/') {
        size_t protocolEnd = baseUrl.find("://");
        if (protocolEnd != std::string::npos) {
            size_t domainEnd = baseUrl.find('/', protocolEnd + 3);
            if (domainEnd != std::string::npos) {
                return baseUrl.substr(0, domainEnd) + url;
            }
        }
        return baseUrl + url;
    }
    
    // Otherwise, append to base URL's directory
    size_t lastSlash = baseUrl.find_last_of('/');
    if (lastSlash != std::string::npos) {
        return baseUrl.substr(0, lastSlash + 1) + url;
    }
    
    return baseUrl + "/" + url;
}

bool ContentParser::isValidUrl(const std::string& url) {
    if (url.empty()) {
        return false;
    }
    
    // Check for invalid schemes that should not be crawled
    std::string lowerUrl = url;
    std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);
    
    // List of schemes that should not be crawled
    std::vector<std::string> invalidSchemes = {
        "mailto:", "tel:", "javascript:", "data:", "ftp:", "file:", "about:", 
        "chrome:", "edge:", "safari:", "opera:", "moz-extension:", "chrome-extension:"
    };
    
    // Check if URL starts with any invalid scheme
    for (const auto& scheme : invalidSchemes) {
        if (lowerUrl.find(scheme) == 0) {
            LOG_DEBUG("Rejected URL with invalid scheme: " + url);
            return false;
        }
    }
    
    // Check for malformed URLs that might have invalid schemes embedded
    // Examples: "http://example.com/mailto:info@example.com"
    for (const auto& scheme : invalidSchemes) {
        if (lowerUrl.find("/" + scheme) != std::string::npos) {
            LOG_DEBUG("Rejected URL with embedded invalid scheme: " + url);
            return false;
        }
    }
    
    // Use regex to validate HTTP/HTTPS URL format
    static const std::regex urlRegex(
        R"(^(https?:\/\/)[^\s\/:?#]+(\.[^\s\/:?#]+)*(?::\d+)?(\/[^\s?#]*)?(\?[^\s#]*)?(#[^\s]*)?$)",
        std::regex::ECMAScript | std::regex::icase
    );
    
    bool isValid = std::regex_match(url, urlRegex);
    if (!isValid) {
        LOG_DEBUG("Rejected URL - failed regex validation: " + url);
    }
    
    return isValid;
} 