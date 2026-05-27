#pragma once

#include <string>
#include <unordered_set>
#include <vector>

namespace search_engine::common {

/**
 * @brief URL canonicalization utility for consistent URL handling and deduplication
 * 
 * This class provides comprehensive URL canonicalization to ensure consistent
 * URL identity across the search engine. It handles:
 * - Scheme and host normalization
 * - Port normalization (default ports)
 * - Path normalization (trailing slashes, multiple slashes)
 * - Query parameter normalization (sorting, deduplication)
 * - Fragment removal
 * - Unicode normalization
 * - Tracking parameter removal
 */
class UrlCanonicalizer {
public:
    /**
     * @brief Canonicalize a URL to its standard form
     * 
     * @param url The raw URL to canonicalize
     * @return The canonicalized URL
     */
    static std::string canonicalize(const std::string& url);
    
    /**
     * @brief Extract the canonical host from a URL
     * 
     * @param url The URL to extract host from
     * @return The canonicalized host (lowercase, no www prefix)
     */
    static std::string extractCanonicalHost(const std::string& url);
    
    /**
     * @brief Extract the canonical path from a URL
     * 
     * @param url The URL to extract path from
     * @return The canonicalized path
     */
    static std::string extractCanonicalPath(const std::string& url);
    
    /**
     * @brief Extract the canonical query string from a URL
     * 
     * @param url The URL to extract query from
     * @return The canonicalized query string (sorted parameters, no tracking params)
     */
    static std::string extractCanonicalQuery(const std::string& url);
    
    /**
     * @brief Generate a hash of the canonical URL for fast comparison
     * 
     * @param url The URL to hash
     * @return A hash string of the canonical URL
     */
    static std::string getCanonicalHash(const std::string& url);
    
    /**
     * @brief Check if a URL parameter is a tracking parameter
     * 
     * @param param The parameter name to check
     * @return True if it's a tracking parameter
     */
    static bool isTrackingParameter(const std::string& param);
    
    /**
     * @brief Get the list of known tracking parameters
     *
     * @return Set of tracking parameter names
     */
    static const std::unordered_set<std::string>& getTrackingParameters();

    /**
     * @brief URL decode a string (handles percent-encoded characters and + to space conversion)
     *
     * @param str The string to decode
     * @return The decoded string with Unicode characters properly decoded
     */
    static std::string urlDecode(const std::string& str);

private:
    /**
     * @brief Normalize the scheme part of a URL
     * 
     * @param scheme The scheme to normalize
     * @return The normalized scheme
     */
    static std::string normalizeScheme(const std::string& scheme);
    
    /**
     * @brief Normalize the host part of a URL
     * 
     * @param host The host to normalize
     * @return The normalized host
     */
    static std::string normalizeHost(const std::string& host);
    
    /**
     * @brief Normalize the path part of a URL
     * 
     * @param path The path to normalize
     * @return The normalized path
     */
    static std::string normalizePath(const std::string& path);
    
    /**
     * @brief Normalize the query string part of a URL
     * 
     * @param query The query string to normalize
     * @return The normalized query string
     */
    static std::string normalizeQuery(const std::string& query);
    
    /**
     * @brief Parse and sort query parameters
     * 
     * @param query The query string to parse
     * @return Vector of sorted parameter pairs
     */
    static std::vector<std::pair<std::string, std::string>> parseAndSortQuery(const std::string& query);
    
    /**
     * @brief Convert Unicode hostname to punycode
     * 
     * @param host The hostname to convert
     * @return The punycode representation
     */
    static std::string toPunycode(const std::string& host);
    
    /**
     * @brief Remove default port from URL
     * 
     * @param url The URL to process
     * @return The URL without default port
     */
    static std::string removeDefaultPort(const std::string& url);
    
    /**
     * @brief Collapse multiple consecutive slashes in path
     * 
     * @param path The path to process
     * @return The path with collapsed slashes
     */
    static std::string collapseSlashes(const std::string& path);

    /**
     * @brief URL encode a string
     * 
     * @param str The string to encode
     * @return The encoded string
     */
    static std::string urlEncode(const std::string& str);
    
    // Static tracking parameters set
    static std::unordered_set<std::string> trackingParams_;
    static bool trackingParamsInitialized_;
    
    /**
     * @brief Initialize the tracking parameters set
     */
    static void initializeTrackingParameters();
};

} // namespace search_engine::common
