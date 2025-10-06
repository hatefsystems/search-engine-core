#include "../../include/search_engine/common/UrlCanonicalizer.h"
#include "../../include/Logger.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <regex>
#include <unordered_map>

namespace search_engine::common {

// Static member initialization
std::unordered_set<std::string> UrlCanonicalizer::trackingParams_;
bool UrlCanonicalizer::trackingParamsInitialized_ = false;

void UrlCanonicalizer::initializeTrackingParameters() {
    if (trackingParamsInitialized_) return;
    
    trackingParams_ = {
        // Google Analytics
        "utm_source", "utm_medium", "utm_campaign", "utm_term", "utm_content",
        "utm_id", "utm_source_platform", "utm_creative_format", "utm_marketing_tactic",
        
        // Facebook/Meta
        "fbclid", "fb_action_ids", "fb_action_types", "fb_source", "fb_ref",
        
        // Twitter/X
        "twclid", "s", "t", "ref_src", "ref_url",
        
        // LinkedIn
        "li_fat_id", "li_source", "li_medium", "li_campaign",
        
        // Microsoft/Bing
        "msclkid", "mc_cid", "mc_eid",
        
        // Amazon
        "tag", "linkCode", "camp", "creative", "creativeASIN",
        
        // Other common tracking
        "gclid", "gclsrc", "dclid", "wbraid", "gbraid",
        "ref", "referrer", "source", "campaign", "medium",
        "affiliate", "partner", "click_id", "clickid",
        "session_id", "sessionid", "sid", "token",
        "tracking_id", "trackingid", "tid", "cid",
        "email", "e", "newsletter", "subscriber",
        "promo", "promotion", "discount", "coupon",
        "variant", "test", "experiment", "ab_test",
        "timestamp", "ts", "time", "date",
        "user_id", "userid", "uid", "id",
        "ip", "ip_address", "ipaddr",
        "device", "platform", "os", "browser",
        "version", "v", "build", "release"
    };
    
    trackingParamsInitialized_ = true;
    LOG_DEBUG("Initialized " + std::to_string(trackingParams_.size()) + " tracking parameters");
}

std::string UrlCanonicalizer::canonicalize(const std::string& url) {
    if (url.empty()) return url;
    
    try {
        // Initialize tracking parameters if needed
        initializeTrackingParameters();
        
        // Basic URL parsing - find scheme, host, path, query, fragment
        std::string scheme, host, path, query, fragment;
        
        // Find scheme
        size_t schemeEnd = url.find("://");
        if (schemeEnd != std::string::npos) {
            scheme = url.substr(0, schemeEnd);
            size_t start = schemeEnd + 3;
            
            // Find host (everything until first /, ?, or #)
            size_t hostEnd = url.find_first_of("/?#", start);
            if (hostEnd == std::string::npos) {
                host = url.substr(start);
            } else {
                host = url.substr(start, hostEnd - start);
                
                // Find path
                if (url[hostEnd] == '/') {
                    size_t pathEnd = url.find_first_of("?#", hostEnd);
                    if (pathEnd == std::string::npos) {
                        path = url.substr(hostEnd);
                    } else {
                        path = url.substr(hostEnd, pathEnd - hostEnd);
                        
                        // Find query
                        if (url[pathEnd] == '?') {
                            size_t queryEnd = url.find('#', pathEnd);
                            if (queryEnd == std::string::npos) {
                                query = url.substr(pathEnd + 1);
                            } else {
                                query = url.substr(pathEnd + 1, queryEnd - pathEnd - 1);
                                fragment = url.substr(queryEnd + 1);
                            }
                        } else if (url[pathEnd] == '#') {
                            fragment = url.substr(pathEnd + 1);
                        }
                    }
                } else if (url[hostEnd] == '?') {
                    size_t queryEnd = url.find('#', hostEnd);
                    if (queryEnd == std::string::npos) {
                        query = url.substr(hostEnd + 1);
                    } else {
                        query = url.substr(hostEnd + 1, queryEnd - hostEnd - 1);
                        fragment = url.substr(queryEnd + 1);
                    }
                } else if (url[hostEnd] == '#') {
                    fragment = url.substr(hostEnd + 1);
                }
            }
        } else {
            // No scheme, treat as relative URL
            size_t pathStart = 0;
            if (url[0] == '/') pathStart = 0;
            else if (url[0] == '?') {
                query = url.substr(1);
                return "/?" + normalizeQuery(query);
            } else if (url[0] == '#') {
                return "/";
            } else {
                path = "/" + url;
            }
            
            if (pathStart == 0) {
                size_t pathEnd = url.find_first_of("?#", pathStart);
                if (pathEnd == std::string::npos) {
                    path = url.substr(pathStart);
                } else {
                    path = url.substr(pathStart, pathEnd - pathStart);
                    
                    if (url[pathEnd] == '?') {
                        size_t queryEnd = url.find('#', pathEnd);
                        if (queryEnd == std::string::npos) {
                            query = url.substr(pathEnd + 1);
                        } else {
                            query = url.substr(pathEnd + 1, queryEnd - pathEnd - 1);
                            fragment = url.substr(queryEnd + 1);
                        }
                    } else if (url[pathEnd] == '#') {
                        fragment = url.substr(pathEnd + 1);
                    }
                }
            }
        }
        
        // Normalize each component
        scheme = normalizeScheme(scheme);
        host = normalizeHost(host);
        path = normalizePath(path);
        query = normalizeQuery(query);
        // Fragment is always removed for canonicalization
        
        // Reconstruct URL
        std::string canonical;
        if (!scheme.empty() && !host.empty()) {
            canonical = scheme + "://" + host + path;
        } else {
            canonical = path;
        }
        
        if (!query.empty()) {
            canonical += "?" + query;
        }
        
        LOG_DEBUG("Canonicalized URL: " + url + " -> " + canonical);
        return canonical;
        
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to canonicalize URL: " + url + " - " + std::string(e.what()));
        return url; // Return original on error
    }
}

std::string UrlCanonicalizer::extractCanonicalHost(const std::string& url) {
    try {
        size_t schemeEnd = url.find("://");
        if (schemeEnd == std::string::npos) return "";
        
        size_t start = schemeEnd + 3;
        size_t hostEnd = url.find_first_of("/?#", start);
        
        std::string host;
        if (hostEnd == std::string::npos) {
            host = url.substr(start);
        } else {
            host = url.substr(start, hostEnd - start);
        }
        
        return normalizeHost(host);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to extract host from URL: " + url + " - " + std::string(e.what()));
        return "";
    }
}

std::string UrlCanonicalizer::extractCanonicalPath(const std::string& url) {
    try {
        size_t schemeEnd = url.find("://");
        size_t start = 0;
        
        if (schemeEnd != std::string::npos) {
            start = schemeEnd + 3;
            size_t hostEnd = url.find_first_of("/?#", start);
            if (hostEnd == std::string::npos) return "/";
            if (url[hostEnd] != '/') return "/";
            start = hostEnd;
        }
        
        size_t pathEnd = url.find_first_of("?#", start);
        std::string path;
        
        if (pathEnd == std::string::npos) {
            path = url.substr(start);
        } else {
            path = url.substr(start, pathEnd - start);
        }
        
        return normalizePath(path);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to extract path from URL: " + url + " - " + std::string(e.what()));
        return "/";
    }
}

std::string UrlCanonicalizer::extractCanonicalQuery(const std::string& url) {
    try {
        size_t queryStart = url.find('?');
        if (queryStart == std::string::npos) return "";
        
        size_t queryEnd = url.find('#', queryStart);
        std::string query;
        
        if (queryEnd == std::string::npos) {
            query = url.substr(queryStart + 1);
        } else {
            query = url.substr(queryStart + 1, queryEnd - queryStart - 1);
        }
        
        return normalizeQuery(query);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to extract query from URL: " + url + " - " + std::string(e.what()));
        return "";
    }
}

std::string UrlCanonicalizer::getCanonicalHash(const std::string& url) {
    std::string canonical = canonicalize(url);
    
    // Simple hash function - in production, consider using a proper hash like SHA-256
    std::hash<std::string> hasher;
    size_t hashValue = hasher(canonical);
    
    std::ostringstream oss;
    oss << std::hex << hashValue;
    return oss.str();
}

bool UrlCanonicalizer::isTrackingParameter(const std::string& param) {
    initializeTrackingParameters();
    
    // Convert to lowercase for case-insensitive comparison
    std::string lowerParam = param;
    std::transform(lowerParam.begin(), lowerParam.end(), lowerParam.begin(), ::tolower);
    
    return trackingParams_.find(lowerParam) != trackingParams_.end();
}

const std::unordered_set<std::string>& UrlCanonicalizer::getTrackingParameters() {
    initializeTrackingParameters();
    return trackingParams_;
}

std::string UrlCanonicalizer::normalizeScheme(const std::string& scheme) {
    if (scheme.empty()) return "http"; // Default scheme
    
    std::string lower = scheme;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Normalize common schemes
    if (lower == "https") return "https";
    if (lower == "http") return "http";
    if (lower == "ftp") return "ftp";
    if (lower == "ftps") return "ftps";
    
    return lower;
}

std::string UrlCanonicalizer::normalizeHost(const std::string& host) {
    if (host.empty()) return host;
    
    std::string normalized = host;
    
    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    
    // Remove www. prefix
    if (normalized.length() > 4 && normalized.substr(0, 4) == "www.") {
        normalized = normalized.substr(4);
    }
    
    // Remove default ports
    size_t portPos = normalized.find_last_of(':');
    if (portPos != std::string::npos) {
        std::string port = normalized.substr(portPos + 1);
        if (port == "80" || port == "443" || port == "21" || port == "22") {
            normalized = normalized.substr(0, portPos);
        }
    }
    
    // Convert Unicode to punycode
    normalized = toPunycode(normalized);
    
    return normalized;
}

std::string UrlCanonicalizer::normalizePath(const std::string& path) {
    if (path.empty()) return "/";
    
    std::string normalized = path;
    
    // URL decode
    normalized = urlDecode(normalized);
    
    // Collapse multiple slashes
    normalized = collapseSlashes(normalized);
    
    // Remove trailing slash for non-root paths
    if (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    // Ensure path starts with /
    if (normalized.empty() || normalized[0] != '/') {
        normalized = "/" + normalized;
    }
    
    return normalized;
}

std::string UrlCanonicalizer::normalizeQuery(const std::string& query) {
    if (query.empty()) return "";
    
    auto params = parseAndSortQuery(query);
    if (params.empty()) return "";
    
    std::ostringstream oss;
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) oss << "&";
        oss << urlEncode(params[i].first) << "=" << urlEncode(params[i].second);
    }
    
    return oss.str();
}

std::vector<std::pair<std::string, std::string>> UrlCanonicalizer::parseAndSortQuery(const std::string& query) {
    std::vector<std::pair<std::string, std::string>> params;
    
    std::istringstream iss(query);
    std::string param;
    
    while (std::getline(iss, param, '&')) {
        if (param.empty()) continue;
        
        size_t eqPos = param.find('=');
        std::string key, value;
        
        if (eqPos == std::string::npos) {
            key = urlDecode(param);
            value = "";
        } else {
            key = urlDecode(param.substr(0, eqPos));
            value = urlDecode(param.substr(eqPos + 1));
        }
        
        // Skip tracking parameters
        if (!isTrackingParameter(key)) {
            params.emplace_back(key, value);
        }
    }
    
    // Sort parameters by key
    std::sort(params.begin(), params.end(), 
              [](const auto& a, const auto& b) { return a.first < b.first; });
    
    return params;
}

std::string UrlCanonicalizer::toPunycode(const std::string& host) {
    // Simple implementation - in production, use proper IDN conversion
    // For now, just return the host as-is since most hosts are ASCII
    return host;
}

std::string UrlCanonicalizer::removeDefaultPort(const std::string& url) {
    // This is handled in normalizeHost for individual components
    return url;
}

std::string UrlCanonicalizer::collapseSlashes(const std::string& path) {
    std::string result;
    result.reserve(path.length());
    
    bool inSlash = false;
    for (char c : path) {
        if (c == '/') {
            if (!inSlash) {
                result += c;
                inSlash = true;
            }
        } else {
            result += c;
            inSlash = false;
        }
    }
    
    return result;
}

std::string UrlCanonicalizer::urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            std::string hex = str.substr(i + 1, 2);
            char* end;
            long value = std::strtol(hex.c_str(), &end, 16);
            if (*end == '\0' && value >= 0 && value <= 255) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    
    return result;
}

std::string UrlCanonicalizer::urlEncode(const std::string& str) {
    std::ostringstream oss;
    
    for (unsigned char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            oss << c;
        } else {
            oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }
    
    return oss.str();
}

} // namespace search_engine::common
