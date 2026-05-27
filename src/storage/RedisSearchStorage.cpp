#include "../../include/search_engine/storage/RedisSearchStorage.h"
#include "../../include/Logger.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <chrono>
#include <unordered_set>

using namespace search_engine::storage;

namespace {
    // Persian and English stopwords
    const std::unordered_set<std::string> STOPWORDS = {
        // Persian stopwords
        "و", "در", "به", "از", "که", "این", "را", "با", "برای", "آن", "یک", "شود", "شده", 
        "خود", "ای", "یا", "تا", "کرد", "بر", "هم", "نیز", "می", "شد", "ها", "است", 
        "گفت", "می‌شود", "وی", "کرده", "دارد", "ما", "کند", "نیست", "باشد", "دیگر",
        // English stopwords (common ones)
        "the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "for", 
        "of", "with", "by", "from", "as", "is", "was", "are", "were", "be", "been"
    };
    
    // Helper function to escape Redis string values
    std::string escapeRedisString(const std::string& str) {
        std::string escaped = str;
        // Replace quotes and special characters
        std::replace(escaped.begin(), escaped.end(), '"', '\'');
        std::replace(escaped.begin(), escaped.end(), '\n', ' ');
        std::replace(escaped.begin(), escaped.end(), '\r', ' ');
        return escaped;
    }
    
    // Helper function to generate a hash from URL for key
    std::string urlToKey(const std::string& url) {
        std::hash<std::string> hasher;
        return std::to_string(hasher(url));
    }
    
    // Helper function to convert time_point to Unix timestamp
    int64_t timePointToUnixTimestamp(const std::chrono::system_clock::time_point& tp) {
        return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
    }
    
    // Tokenize query into words (simple whitespace split)
    std::vector<std::string> tokenizeQuery(const std::string& query) {
        std::vector<std::string> tokens;
        std::istringstream iss(query);
        std::string token;
        
        while (iss >> token) {
            // Remove common punctuation
            token.erase(std::remove_if(token.begin(), token.end(), 
                [](char c) { return std::ispunct(c) && c != '-' && c != '_'; }), 
                token.end());
            
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        
        return tokens;
    }
    
    // Filter stopwords from tokens
    std::vector<std::string> filterStopwords(const std::vector<std::string>& tokens) {
        std::vector<std::string> filtered;
        
        for (const auto& token : tokens) {
            // Convert to lowercase for comparison
            std::string lowerToken = token;
            std::transform(lowerToken.begin(), lowerToken.end(), lowerToken.begin(), ::tolower);
            
            // Keep token if it's not a stopword and has minimum length
            if (STOPWORDS.find(lowerToken) == STOPWORDS.end() && token.length() >= 2) {
                filtered.push_back(token);
            }
        }
        
        return filtered;
    }
    
    // Build Redis query string for all terms in a specific field
    std::string buildFieldQuery(const std::vector<std::string>& terms, const std::string& field) {
        if (terms.empty()) return "";
        
        std::ostringstream oss;
        oss << "@" << field << ":(";
        
        for (size_t i = 0; i < terms.size(); ++i) {
            if (i > 0) oss << " ";
            oss << terms[i];
        }
        
        oss << ")";
        return oss.str();
    }
    
    // Build Redis query for all terms across multiple fields (OR)
    std::string buildMultiFieldQuery(const std::vector<std::string>& terms, 
                                     const std::vector<std::string>& fields) {
        if (terms.empty() || fields.empty()) return "";
        
        std::ostringstream oss;
        oss << "(";
        
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) oss << "|";
            oss << "@" << fields[i] << ":(";
            for (size_t j = 0; j < terms.size(); ++j) {
                if (j > 0) oss << " ";
                oss << terms[j];
            }
            oss << ")";
        }
        
        oss << ")";
        return oss.str();
    }
}

RedisSearchStorage::RedisSearchStorage(
    const std::string& connectionString,
    const std::string& indexName,
    const std::string& keyPrefix
) : indexName_(indexName), keyPrefix_(keyPrefix) {
    LOG_DEBUG("RedisSearchStorage constructor called with index: " + indexName);
    try {
        sw::redis::ConnectionOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 6379;
        opts.db = 0;
        
        // Parse connection string if provided
        if (connectionString.find("tcp://") == 0) {
            std::string hostPort = connectionString.substr(6);
            size_t colonPos = hostPort.find(':');
            if (colonPos != std::string::npos) {
                opts.host = hostPort.substr(0, colonPos);
                opts.port = std::stoi(hostPort.substr(colonPos + 1));
            }
        }
        
        LOG_INFO("Connecting to Redis at " + opts.host + ":" + std::to_string(opts.port));
        redis_ = std::make_unique<sw::redis::Redis>(opts);
        
        // Test connection and initialize index if needed
        testConnection();
        LOG_DEBUG("Redis connection test successful");
        
        if (!indexExists()) {
            LOG_INFO("Search index doesn't exist, creating: " + indexName_);
            initializeIndex();
        } else {
            LOG_INFO("Search index already exists: " + indexName_);
        }
        
    } catch (const sw::redis::Error& e) {
        LOG_ERROR("Failed to initialize Redis connection: " + std::string(e.what()));
        throw std::runtime_error("Failed to initialize Redis connection: " + std::string(e.what()));
    }
}

std::string RedisSearchStorage::generateDocumentKey(const std::string& url) const {
    return keyPrefix_ + urlToKey(url);
}

bool RedisSearchStorage::indexExists() const {
    try {
        // Try to get index info - if it throws, index doesn't exist
        auto reply = redis_->command("FT.INFO", indexName_);
        return true;
    } catch (const sw::redis::Error&) {
        return false;
    }
}

Result<bool> RedisSearchStorage::createSearchIndex() {
    try {
        // Create RediSearch index with schema
        std::vector<std::string> cmd = {
            "FT.CREATE", indexName_,
            "ON", "HASH",
            "PREFIX", "1", keyPrefix_,
            "SCHEMA",
            "url", "TEXT", "WEIGHT", "0.5", "SORTABLE",  // Searchable with low weight
            "title", "TEXT", "WEIGHT", "5.0",
            "content", "TEXT", "WEIGHT", "1.0",
            "domain", "TAG", "SORTABLE",
            "keywords", "TAG",
            "description", "TEXT", "WEIGHT", "2.0",
            "language", "TAG",
            "category", "TAG",
            "indexed_at", "NUMERIC", "SORTABLE",
            "score", "NUMERIC", "SORTABLE"
        };
        
        redis_->command(cmd.begin(), cmd.end());
        return Result<bool>::Success(true, "Search index created successfully");
        
    } catch (const sw::redis::Error& e) {
        std::string errorMsg = std::string(e.what());
        // Check if the error is because index already exists
        if (errorMsg.find("Index already exists") != std::string::npos) {
            return Result<bool>::Success(true, "Search index already exists");
        }
        return Result<bool>::Failure("Failed to create search index: " + errorMsg);
    }
}

Result<bool> RedisSearchStorage::initializeIndex() {
    return createSearchIndex();
}

Result<bool> RedisSearchStorage::indexDocument(const SearchDocument& document) {
    LOG_DEBUG("RedisSearchStorage::indexDocument called for URL: " + document.url);
    try {
        std::string key = generateDocumentKey(document.url);
        LOG_TRACE("Generated Redis key: " + key + " for URL: " + document.url);
        
        std::unordered_map<std::string, std::string> fields;
        fields["url"] = escapeRedisString(document.url);
        fields["title"] = escapeRedisString(document.title);
        fields["content"] = escapeRedisString(document.content);
        fields["domain"] = escapeRedisString(document.domain);
        fields["score"] = std::to_string(document.score);
        fields["indexed_at"] = std::to_string(timePointToUnixTimestamp(document.indexedAt));
        
        // Handle optional fields
        if (document.description) {
            fields["description"] = escapeRedisString(*document.description);
        }
        if (document.language) {
            fields["language"] = escapeRedisString(*document.language);
        }
        if (document.category) {
            fields["category"] = escapeRedisString(*document.category);
        }
        
        // Join keywords with pipes for TAG field
        if (!document.keywords.empty()) {
            std::ostringstream keywordStream;
            for (size_t i = 0; i < document.keywords.size(); ++i) {
                if (i > 0) keywordStream << "|";
                keywordStream << escapeRedisString(document.keywords[i]);
            }
            fields["keywords"] = keywordStream.str();
        }
        
        // Store the document as a Redis hash
        redis_->hmset(key, fields.begin(), fields.end());
        LOG_INFO("Document indexed successfully: " + document.url + " (title: " + document.title + ")");
        
        return Result<bool>::Success(true, "Document indexed successfully");
        
    } catch (const sw::redis::Error& e) {
        LOG_ERROR("Failed to index document for URL: " + document.url + " - " + std::string(e.what()));
        return Result<bool>::Failure("Failed to index document: " + std::string(e.what()));
    }
}

Result<bool> RedisSearchStorage::indexSiteProfile(const IndexedPage& page, const std::string& content) {
    SearchDocument doc = siteProfileToSearchDocument(page, content);
    return indexDocument(doc);
}

SearchDocument RedisSearchStorage::siteProfileToSearchDocument(
    const IndexedPage& page, 
    const std::string& content
) {
    SearchDocument doc;
    doc.url = page.url;
    doc.title = page.title;
    doc.content = content;
    doc.domain = page.domain;
    doc.keywords = page.keywords;
    doc.description = page.description;
    doc.language = page.language;
    doc.category = page.category;
    doc.indexedAt = page.indexedAt;
    doc.score = page.contentQuality.value_or(0.0);
    
    return doc;
}

Result<bool> RedisSearchStorage::updateDocument(const SearchDocument& document) {
    // For Redis, update is the same as index
    return indexDocument(document);
}

Result<bool> RedisSearchStorage::deleteDocument(const std::string& url) {
    try {
        std::string key = generateDocumentKey(url);
        auto deleted = redis_->del(key);
        
        if (deleted > 0) {
            return Result<bool>::Success(true, "Document deleted successfully");
        } else {
            return Result<bool>::Failure("Document not found for URL: " + url);
        }
        
    } catch (const sw::redis::Error& e) {
        return Result<bool>::Failure("Failed to delete document: " + std::string(e.what()));
    }
}

std::vector<std::string> RedisSearchStorage::buildSearchCommand(const SearchQuery& query) const {
    std::vector<std::string> cmd = {"FT.SEARCH", indexName_};
    
    // Build query string
    std::string queryStr = query.query;
    
    // Add filters
    for (const auto& filter : query.filters) {
        queryStr += " " + filter;
    }
    
    // Add language filter if specified
    if (query.language) {
        queryStr += " @language:{" + *query.language + "}";
    }
    
    // Add category filter if specified
    if (query.category) {
        queryStr += " @category:{" + *query.category + "}";
    }
    
    cmd.push_back(queryStr);
    
    // Add LIMIT
    if (query.limit > 0) {
        cmd.push_back("LIMIT");
        cmd.push_back(std::to_string(query.offset));
        cmd.push_back(std::to_string(query.limit));
    }
    
    // Add sorting (by score descending)
    cmd.push_back("SORTBY");
    cmd.push_back("score");
    cmd.push_back("DESC");
    
    // Add highlighting if requested
    if (query.highlight) {
        cmd.push_back("HIGHLIGHT");
        cmd.push_back("FIELDS");
        cmd.push_back("2");
        cmd.push_back("title");
        cmd.push_back("content");
    }
    
    return cmd;
}

SearchResult RedisSearchStorage::parseSearchResult(const std::vector<std::string>& result) const {
    SearchResult searchResult;
    
    if (result.size() < 2) {
        return searchResult;
    }
    
    // First element is the document key, skip it
    // Remaining elements are field-value pairs
    for (size_t i = 1; i < result.size(); i += 2) {
        if (i + 1 >= result.size()) break;
        
        const std::string& field = result[i];
        const std::string& value = result[i + 1];
        
        if (field == "url") {
            searchResult.url = value;
        } else if (field == "title") {
            searchResult.title = value;
        } else if (field == "content") {
            // Use first 200 characters as snippet
            searchResult.snippet = value.length() > 200 ? 
                value.substr(0, 200) + "..." : value;
        } else if (field == "domain") {
            searchResult.domain = value;
        } else if (field == "score") {
            try {
                searchResult.score = std::stod(value);
            } catch (...) {
                searchResult.score = 0.0;
            }
        }
    }
    
    return searchResult;
}

Result<SearchResponse> RedisSearchStorage::executeSingleSearch(
    const std::string& queryString,
    const SearchQuery& originalQuery,
    int limit
) const {
    try {
        // Build command for this specific query
        std::vector<std::string> cmd = {"FT.SEARCH", indexName_};
        
        std::string finalQuery = queryString;
        
        // Add language filter if specified
        if (originalQuery.language) {
            finalQuery += " @language:{" + *originalQuery.language + "}";
        }
        
        // Add category filter if specified
        if (originalQuery.category) {
            finalQuery += " @category:{" + *originalQuery.category + "}";
        }
        
        cmd.push_back(finalQuery);
        
        // Add LIMIT
        cmd.push_back("LIMIT");
        cmd.push_back("0");
        cmd.push_back(std::to_string(limit));
        
        // Add sorting (by score descending)
        cmd.push_back("SORTBY");
        cmd.push_back("score");
        cmd.push_back("DESC");
        
        // Execute search
        auto reply = redis_->command(cmd.begin(), cmd.end());
        
        SearchResponse response;
        response.indexName = indexName_;
        
        // Parse reply
        if (reply && reply->type == REDIS_REPLY_ARRAY) {
            auto replyArray = reply.get();
            if (replyArray->elements > 0) {
                // First element is total count
                if (replyArray->element[0]->type == REDIS_REPLY_INTEGER) {
                    response.totalResults = replyArray->element[0]->integer;
                }
                
                // Remaining elements are documents (groups of key + fields)
                for (size_t i = 1; i < replyArray->elements; i += 2) {
                    if (i + 1 >= replyArray->elements) break;
                    
                    std::vector<std::string> docData;
                    
                    // Document key
                    if (replyArray->element[i]->type == REDIS_REPLY_STRING) {
                        docData.push_back(std::string(replyArray->element[i]->str, replyArray->element[i]->len));
                    }
                    
                    // Document fields
                    if (replyArray->element[i + 1]->type == REDIS_REPLY_ARRAY) {
                        auto fieldsReply = replyArray->element[i + 1];
                        for (size_t j = 0; j < fieldsReply->elements; ++j) {
                            if (fieldsReply->element[j]->type == REDIS_REPLY_STRING) {
                                docData.push_back(std::string(fieldsReply->element[j]->str, fieldsReply->element[j]->len));
                            }
                        }
                    }
                    
                    auto searchResult = parseSearchResult(docData);
                    if (!searchResult.url.empty()) {
                        response.results.push_back(searchResult);
                    }
                }
            }
        }
        
        return Result<SearchResponse>::Success(
            std::move(response),
            "Single search completed successfully"
        );
        
    } catch (const sw::redis::Error& e) {
        return Result<SearchResponse>::Failure("Single search failed: " + std::string(e.what()));
    }
}

Result<SearchResponse> RedisSearchStorage::search(const SearchQuery& query) {
    try {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Tokenize and filter stopwords
        auto tokens = tokenizeQuery(query.query);
        auto filteredTokens = filterStopwords(tokens);
        
        LOG_DEBUG("Original query: '" + query.query + "' -> " + std::to_string(tokens.size()) + " tokens -> " + 
                  std::to_string(filteredTokens.size()) + " after stopword filtering");
        
        // If no meaningful tokens after filtering, use original query
        if (filteredTokens.empty()) {
            filteredTokens = tokens;
        }
        
        SearchResponse response;
        response.indexName = indexName_;
        std::unordered_set<std::string> seenUrls; // For deduplication
        
        // Get total count from simple search (without field restrictions) first
        int totalResultsFromRedis = 0;
        auto countQuery = executeSingleSearch(query.query, query, 0); // LIMIT 0 to get only count
        if (countQuery.success) {
            totalResultsFromRedis = countQuery.value.totalResults;
            LOG_DEBUG("Total results in Redis for full search: " + std::to_string(totalResultsFromRedis));
        }
        
        // Multi-tier search strategy with clear priority:
        // Tier 1: Terms in TITLE only (highest priority - 3x boost)
        // Tier 2: Terms in CONTENT/DESCRIPTION only (medium priority - 1.5x boost)
        // Tier 3: ANY match (fallback - 1x)
        
        // Tier 1: ALL terms in title (ALWAYS, even for single words)
        // Fetch up to 500 best results for ranking, but count all matching
        std::string titleQuery = buildFieldQuery(filteredTokens, "title");
        LOG_DEBUG("Tier 1 query (terms in title): " + titleQuery);
        
        auto tier1Results = executeSingleSearch(titleQuery, query, 500);
        if (tier1Results.success) {
            for (const auto& result : tier1Results.value.results) {
                if (seenUrls.find(result.url) == seenUrls.end()) {
                    // Additive boost for tier 1 results (in title)
                    SearchResult boostedResult = result;
                    boostedResult.score = (boostedResult.score * 2.0) + 1000.0; // High priority: multiply base + large bonus
                    response.results.push_back(boostedResult);
                    seenUrls.insert(result.url);
                }
            }
            LOG_INFO("Tier 1 (in title): fetched " + std::to_string(tier1Results.value.results.size()) + 
                     " unique results (total matching: " + std::to_string(tier1Results.value.totalResults) + ")");
        }
        
        // Tier 2: ALL terms in content/description (separate from title)
        // Continue fetching up to 1000 total results for good ranking
        if (response.results.size() < 1000) {
            std::string contentQuery = buildMultiFieldQuery(filteredTokens, {"content", "description"});
            LOG_DEBUG("Tier 2 query (terms in content/description): " + contentQuery);
            
            int tier2Limit = std::min(500, static_cast<int>(1000 - response.results.size()));
            auto tier2Results = executeSingleSearch(contentQuery, query, tier2Limit);
            if (tier2Results.success) {
                for (const auto& result : tier2Results.value.results) {
                    if (seenUrls.find(result.url) == seenUrls.end()) {
                        // Moderate additive boost for tier 2 results (in content)
                        SearchResult boostedResult = result;
                        boostedResult.score = (boostedResult.score * 1.5) + 100.0; // Medium priority: multiply base + medium bonus
                        response.results.push_back(boostedResult);
                        seenUrls.insert(result.url);
                    }
                }
                LOG_INFO("Tier 2 (in content): fetched " + std::to_string(tier2Results.value.results.size()) + 
                         " unique results (total matching: " + std::to_string(tier2Results.value.totalResults) + ")");
            }
        }
        
        // Tier 3: ANY term matching (fallback)
        // Fill remaining slots up to 1000 total
        if (response.results.size() < 1000) {
            int tier3Limit = std::min(500, static_cast<int>(1000 - response.results.size()));
            auto tier3Results = executeSingleSearch(query.query, query, tier3Limit);
            if (tier3Results.success) {
                for (const auto& result : tier3Results.value.results) {
                    if (seenUrls.find(result.url) == seenUrls.end()) {
                        // Keep base score for tier 3 (lowest priority)
                        response.results.push_back(result);
                        seenUrls.insert(result.url);
                    }
                }
                LOG_INFO("Tier 3 (any match): fetched " + std::to_string(tier3Results.value.results.size()) + 
                         " unique results (total matching: " + std::to_string(tier3Results.value.totalResults) + ")");
            }
        }
        
        // Sort by score (descending) after combining all tiers
        std::sort(response.results.begin(), response.results.end(), 
                  [](const SearchResult& a, const SearchResult& b) {
                      return a.score > b.score;
                  });
        
        // Store number of unique results fetched BEFORE limiting
        int uniqueResultsFetched = response.results.size();
        
        // Limit to requested number
        if (response.results.size() > static_cast<size_t>(query.limit)) {
            response.results.resize(query.limit);
        }
        
        // Use total count from Redis, not fetched count
        // Note: This might include duplicates across tiers, but gives accurate total matches
        response.totalResults = totalResultsFromRedis;
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        response.queryTime = duration.count();
        
        LOG_INFO("Multi-tier search completed: " + std::to_string(totalResultsFromRedis) + 
                 " total in Redis (" + std::to_string(uniqueResultsFetched) + " unique fetched), returning " + 
                 std::to_string(response.results.size()) + " in " + std::to_string(response.queryTime) + "ms");
        
        return Result<SearchResponse>::Success(
            std::move(response),
            "Multi-tier search completed successfully"
        );
        
    } catch (const sw::redis::Error& e) {
        return Result<SearchResponse>::Failure("Search failed: " + std::string(e.what()));
    }
}

Result<SearchResponse> RedisSearchStorage::searchSimple(const std::string& query, int limit) {
    SearchQuery searchQuery;
    searchQuery.query = query;
    searchQuery.limit = limit;
    searchQuery.highlight = true;
    
    return search(searchQuery);
}

Result<std::vector<std::string>> RedisSearchStorage::suggest(const std::string& prefix, int limit) {
    try {
        // Use FT.SUGGET for autocomplete suggestions
        std::vector<std::string> cmd = {
            "FT.SUGGET", indexName_ + ":suggestions", prefix, "MAX", std::to_string(limit)
        };
        
        auto reply = redis_->command(cmd.begin(), cmd.end());
        
        std::vector<std::string> suggestions;
        if (reply && reply->type == REDIS_REPLY_ARRAY) {
            auto replyArray = reply.get();
            for (size_t i = 0; i < replyArray->elements; ++i) {
                if (replyArray->element[i]->type == REDIS_REPLY_STRING) {
                    suggestions.push_back(std::string(replyArray->element[i]->str, replyArray->element[i]->len));
                }
            }
        }
        
        return Result<std::vector<std::string>>::Success(
            std::move(suggestions),
            "Suggestions retrieved successfully"
        );
        
    } catch (const sw::redis::Error& e) {
        return Result<std::vector<std::string>>::Failure("Suggestion failed: " + std::string(e.what()));
    }
}

Result<bool> RedisSearchStorage::dropIndex() {
    try {
        redis_->command("FT.DROPINDEX", indexName_);
        return Result<bool>::Success(true, "Index dropped successfully");
    } catch (const sw::redis::Error& e) {
        return Result<bool>::Failure("Failed to drop index: " + std::string(e.what()));
    }
}

Result<int64_t> RedisSearchStorage::getDocumentCount() {
    try {
        LOG_DEBUG("RedisSearchStorage::getDocumentCount() - Starting to get document count");
        
        // Use getIndexInfo and extract the num_docs field from it
        LOG_DEBUG("RedisSearchStorage::getDocumentCount() - Calling getIndexInfo()");
        auto infoResult = getIndexInfo();
        if (!infoResult.success) {
            LOG_DEBUG("RedisSearchStorage::getDocumentCount() - getIndexInfo failed: " + infoResult.message);
            return Result<int64_t>::Failure("Failed to get index info: " + infoResult.message);
        }
        
        LOG_DEBUG("RedisSearchStorage::getDocumentCount() - Successfully got index info");
        auto info = infoResult.value;
        LOG_DEBUG("RedisSearchStorage::getDocumentCount() - Looking for num_docs field");
        auto it = info.find("num_docs");
        if (it != info.end()) {
            LOG_DEBUG("RedisSearchStorage::getDocumentCount() - Found num_docs field with value: " + it->second);
            try {
                int64_t count = std::stoll(it->second);
                LOG_DEBUG("RedisSearchStorage::getDocumentCount() - Successfully parsed count: " + std::to_string(count));
                return Result<int64_t>::Success(count, "Document count retrieved successfully");
            } catch (const std::exception& e) {
                LOG_DEBUG("RedisSearchStorage::getDocumentCount() - Failed to parse num_docs value: " + it->second);
                return Result<int64_t>::Failure("Failed to parse num_docs value: " + it->second);
            }
        }
        
        LOG_DEBUG("RedisSearchStorage::getDocumentCount() - num_docs field not found in index info");
        return Result<int64_t>::Failure("num_docs field not found in index info");
        
    } catch (const sw::redis::Error& e) {
        LOG_DEBUG("RedisSearchStorage::getDocumentCount() - Redis error: " + std::string(e.what()));
        return Result<int64_t>::Failure("Failed to get document count: " + std::string(e.what()));
    }
}

Result<std::unordered_map<std::string, std::string>> RedisSearchStorage::getIndexInfo() {
    try {
        LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Starting to get index info");
        auto reply = redis_->command("FT.INFO", indexName_);
        LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Command executed, checking reply");
        
        if (!reply) {
            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - No reply received");
            return Result<std::unordered_map<std::string, std::string>>::Failure("No reply from Redis");
        }
        
        std::unordered_map<std::string, std::string> info;
        LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Created empty info map");
        
        if (reply && reply->type == REDIS_REPLY_ARRAY) {
            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Reply is an array");
            auto replyArray = reply.get();
            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Reply array elements: " + std::to_string(replyArray->elements));
            
            // Process elements sequentially looking for key-value pairs
            for (size_t i = 0; i < replyArray->elements; ++i) {
                LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Processing element " + std::to_string(i));
                
                // Log the type and content of current element
                if (replyArray->element[i]->type == REDIS_REPLY_STRING) {
                    std::string elementStr(replyArray->element[i]->str, replyArray->element[i]->len);
                    LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Element " + std::to_string(i) + 
                              " is STRING: '" + elementStr + "'");
                } else if (replyArray->element[i]->type == REDIS_REPLY_INTEGER) {
                    LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Element " + std::to_string(i) + 
                              " is INTEGER: " + std::to_string(replyArray->element[i]->integer));
                } else if (replyArray->element[i]->type == REDIS_REPLY_DOUBLE) {
                    LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Element " + std::to_string(i) + 
                              " is DOUBLE: " + std::to_string(replyArray->element[i]->dval));
                } else if (replyArray->element[i]->type == REDIS_REPLY_ARRAY) {
                    LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Element " + std::to_string(i) + 
                              " is ARRAY with " + std::to_string(replyArray->element[i]->elements) + " elements");
                } else if (replyArray->element[i]->type == REDIS_REPLY_NIL) {
                    LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Element " + std::to_string(i) + " is NIL");
                } else {
                    LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Element " + std::to_string(i) + 
                              " has unknown type: " + std::to_string(replyArray->element[i]->type));
                }
                
                // Handle both string and status types as potential keys
                if (replyArray->element[i]->type == REDIS_REPLY_STRING || 
                    replyArray->element[i]->type == 5) { // REDIS_REPLY_STATUS
                    std::string key(replyArray->element[i]->str, replyArray->element[i]->len);
                    LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Found key: " + key);
                    
                    // Check if the next element is a simple value (not array/nil)
                    LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Checking if next element exists");
                    if (i + 1 < replyArray->elements) {
                        LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Next element exists, getting element");
                        auto nextElement = replyArray->element[i + 1];
                        LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Next element type: " + std::to_string(nextElement->type));
                        std::string value;
                        bool hasValue = false;
                        
                        LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Checking element type and converting to string");
                        if (nextElement->type == REDIS_REPLY_STRING) {
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Element is string type");
                            value = std::string(nextElement->str, nextElement->len);
                            hasValue = true;
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Converted string value: " + value);
                        } else if (nextElement->type == REDIS_REPLY_INTEGER) {
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Element is integer type");
                            value = std::to_string(nextElement->integer);
                            hasValue = true;
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Converted integer value: " + value);
                        } else if (nextElement->type == REDIS_REPLY_DOUBLE) {
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Element is double type");
                            value = std::to_string(nextElement->dval);
                            hasValue = true;
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Converted double value: " + value);
                        } else if (nextElement->type == 5) { // REDIS_REPLY_STATUS
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Element is status type");
                            value = std::string(nextElement->str, nextElement->len);
                            hasValue = true;
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Converted status value: " + value);
                        } else if (nextElement->type == 6) { // REDIS_REPLY_ERROR
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Element is error type");
                            value = std::string(nextElement->str, nextElement->len);
                            hasValue = true;
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Converted error value: " + value);
                        }
                        
                        LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Checking if value was successfully converted");
                        if (hasValue) {
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Adding key-value pair to info map: " + key + " = " + value);
                            info[key] = value;
                            ++i; // Skip the value element since we processed it
                            LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Incremented index to skip processed value");
                        }
                    }
                }
            }
        }
        
        LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Preparing to return success result");
        return Result<std::unordered_map<std::string, std::string>>::Success(
            std::move(info),
            "Index info retrieved successfully"
        );
        
    } catch (const sw::redis::Error& e) {
        LOG_DEBUG("RedisSearchStorage::getIndexInfo() - Caught Redis error: " + std::string(e.what()));
        return Result<std::unordered_map<std::string, std::string>>::Failure(
            "Failed to get index info: " + std::string(e.what())
        );
    }
}

Result<bool> RedisSearchStorage::testConnection() {
    try {
        auto pong = redis_->ping();
        return Result<bool>::Success(true, "Redis connection is healthy");
    } catch (const sw::redis::Error& e) {
        return Result<bool>::Failure("Redis connection test failed: " + std::string(e.what()));
    }
}

Result<bool> RedisSearchStorage::ping() {
    return testConnection();
}

Result<std::vector<bool>> RedisSearchStorage::indexDocuments(const std::vector<SearchDocument>& documents) {
    std::vector<bool> results;
    
    try {
        for (const auto& document : documents) {
            auto result = indexDocument(document);
            results.push_back(result.success);
        }
        
        return Result<std::vector<bool>>::Success(
            std::move(results),
            "Batch document indexing completed"
        );
        
    } catch (const sw::redis::Error& e) {
        return Result<std::vector<bool>>::Failure("Batch indexing failed: " + std::string(e.what()));
    }
}

Result<bool> RedisSearchStorage::deleteDocumentsByDomain(const std::string& domain) {
    try {
        // Use FT.SEARCH to find all documents for this domain
        std::vector<std::string> searchCmd = {
            "FT.SEARCH", indexName_, "@domain:" + escapeRedisString(domain), "RETURN", "1", "url"
        };
        
        auto reply = redis_->command(searchCmd.begin(), searchCmd.end());
        
        std::vector<std::string> urlsToDelete;
        
        if (reply && reply->type == REDIS_REPLY_ARRAY) {
            auto replyArray = reply.get();
            if (replyArray->elements > 0) {
                // Skip the count (first element) and iterate through document results
                for (size_t i = 1; i < replyArray->elements; i += 2) {
                    if (i + 1 >= replyArray->elements) break;
                    
                    // Check if the next element contains fields
                    if (replyArray->element[i + 1]->type == REDIS_REPLY_ARRAY) {
                        auto fieldsReply = replyArray->element[i + 1];
                        for (size_t j = 0; j < fieldsReply->elements - 1; j += 2) {
                            if (fieldsReply->element[j]->type == REDIS_REPLY_STRING &&
                                fieldsReply->element[j + 1]->type == REDIS_REPLY_STRING) {
                                std::string field(fieldsReply->element[j]->str, fieldsReply->element[j]->len);
                                std::string value(fieldsReply->element[j + 1]->str, fieldsReply->element[j + 1]->len);
                                
                                if (field == "url") {
                                    urlsToDelete.push_back(value);
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Delete each document found
        int deletedCount = 0;
        for (const auto& url : urlsToDelete) {
            auto deleteResult = deleteDocument(url);
            if (deleteResult.success) {
                deletedCount++;
            }
        }
        
        return Result<bool>::Success(
            true,
            "Deleted " + std::to_string(deletedCount) + " documents for domain: " + domain
        );
        
    } catch (const sw::redis::Error& e) {
        return Result<bool>::Failure("Failed to delete documents by domain: " + std::string(e.what()));
    }
}

Result<bool> RedisSearchStorage::reindexAll() {
    try {
        // Get all document keys
        std::vector<std::string> searchCmd = {
            "FT.SEARCH", indexName_, "*", "RETURN", "0"
        };
        
        auto reply = redis_->command(searchCmd.begin(), searchCmd.end());
        
        if (reply && reply->type == REDIS_REPLY_ARRAY) {
            auto replyArray = reply.get();
            if (replyArray->elements > 0 && replyArray->element[0]->type == REDIS_REPLY_INTEGER) {
                int64_t totalDocs = replyArray->element[0]->integer;
                
                if (totalDocs > 0) {
                    // Drop and recreate the index
                    auto dropResult = dropIndex();
                    if (!dropResult.success) {
                        return Result<bool>::Failure("Failed to drop index for reindexing: " + dropResult.message);
                    }
                    
                    auto initResult = initializeIndex();
                    if (!initResult.success) {
                        return Result<bool>::Failure("Failed to recreate index for reindexing: " + initResult.message);
                    }
                    
                    return Result<bool>::Success(
                        true,
                        "Reindexing completed. Index recreated for " + std::to_string(totalDocs) + " documents."
                    );
                }
            }
        }
        
        return Result<bool>::Success(true, "No documents found to reindex");
        
    } catch (const sw::redis::Error& e) {
        return Result<bool>::Failure("Reindexing failed: " + std::string(e.what()));
    }
} 