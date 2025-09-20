#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <memory>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>

namespace search_engine {
namespace storage {

/**
 * @brief Unsubscribe record structure
 */
struct UnsubscribeRecord {
    // Unique identifier (MongoDB ObjectId will be auto-generated)
    std::optional<std::string> id;
    
    // Core unsubscribe data
    std::string email;                   // Email address that unsubscribed
    std::string token;                   // Unique unsubscribe token
    std::chrono::system_clock::time_point unsubscribedAt;  // When unsubscribed
    
    // Request tracking data
    std::string ipAddress;               // IP address of unsubscribe request
    std::string userAgent;               // User agent string
    
    // Optional fields
    std::optional<std::string> reason;   // Unsubscribe reason (if provided)
    std::optional<std::string> source;   // Source of unsubscribe (email, web, etc.)
    
    // Metadata
    std::chrono::system_clock::time_point createdAt;
    bool isActive = true;                // Whether unsubscribe is still active
};

/**
 * @brief Email unsubscribe service for managing unsubscribe records
 * 
 * This service handles one-click email unsubscribe functionality including:
 * - Token generation and validation
 * - Unsubscribe record management in MongoDB
 * - Checking if email addresses are unsubscribed
 */
class UnsubscribeService {
public:
    /**
     * @brief Constructor - initializes MongoDB connection
     */
    UnsubscribeService();
    
    /**
     * @brief Destructor
     */
    ~UnsubscribeService() = default;

    /**
     * @brief Generate a unique unsubscribe token for an email
     * @param email Email address to generate token for
     * @return Generated token string
     */
    std::string generateUnsubscribeToken(const std::string& email);
    
    /**
     * @brief Process unsubscribe request
     * @param token Unsubscribe token from URL
     * @param ipAddress IP address of the request
     * @param userAgent User agent string
     * @param reason Optional reason for unsubscribing
     * @return true if unsubscribe was successful, false otherwise
     */
    bool processUnsubscribe(const std::string& token, 
                           const std::string& ipAddress,
                           const std::string& userAgent,
                           const std::optional<std::string>& reason = std::nullopt);
    
    /**
     * @brief Check if an email address is unsubscribed
     * @param email Email address to check
     * @return true if email is unsubscribed, false otherwise
     */
    bool isEmailUnsubscribed(const std::string& email);
    
    /**
     * @brief Get unsubscribe record by token
     * @param token Unsubscribe token
     * @return UnsubscribeRecord if found, std::nullopt otherwise
     */
    std::optional<UnsubscribeRecord> getUnsubscribeByToken(const std::string& token);
    
    /**
     * @brief Get unsubscribe record by email
     * @param email Email address
     * @return UnsubscribeRecord if found, std::nullopt otherwise
     */
    std::optional<UnsubscribeRecord> getUnsubscribeByEmail(const std::string& email);
    
    /**
     * @brief Create unsubscribe token and store in database
     * @param email Email address
     * @param ipAddress IP address (for future reference)
     * @param userAgent User agent (for future reference)
     * @return Generated token string, empty on failure
     */
    std::string createUnsubscribeToken(const std::string& email,
                                      const std::string& ipAddress = "",
                                      const std::string& userAgent = "");
    
    /**
     * @brief Reactivate a previously unsubscribed email (admin function)
     * @param email Email address to reactivate
     * @return true if reactivation successful, false otherwise
     */
    bool reactivateEmail(const std::string& email);

private:
    // MongoDB client and collection
    std::unique_ptr<mongocxx::client> client_;
    mongocxx::collection collection_;
    
    /**
     * @brief Initialize MongoDB connection
     */
    void initializeDatabase();
    
    /**
     * @brief Generate a cryptographically secure random token
     * @return Random token string
     */
    std::string generateSecureToken();
    
    /**
     * @brief Convert UnsubscribeRecord to BSON document
     * @param record UnsubscribeRecord to convert
     * @return BSON document
     */
    bsoncxx::document::value recordToBson(const UnsubscribeRecord& record);
    
    /**
     * @brief Convert BSON document to UnsubscribeRecord
     * @param doc BSON document to convert
     * @return UnsubscribeRecord
     */
    UnsubscribeRecord bsonToRecord(const bsoncxx::document::view& doc);
};

} // namespace storage
} // namespace search_engine
