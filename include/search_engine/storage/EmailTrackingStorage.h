#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <mongocxx/client.hpp>
#include <mongocxx/pool.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <nlohmann/json.hpp>
#include "../../infrastructure.h"

namespace search_engine { namespace storage {

/**
 * @brief Email tracking storage service for tracking email opens
 * 
 * This service handles storing and retrieving email tracking data,
 * including when emails are opened and from what IP address.
 */
class EmailTrackingStorage {
public:
    /**
     * @brief Email tracking event data structure
     */
    struct TrackingEvent {
        std::string trackingId;       // Unique tracking ID
        std::string emailAddress;     // Recipient email address
        std::string emailType;        // Type of email (crawling_notification, generic, etc.)
        std::string ipAddress;        // IP address of recipient when opened
        std::string userAgent;        // User agent string
        std::chrono::system_clock::time_point sentAt;     // When email was sent
        std::chrono::system_clock::time_point openedAt;   // When email was opened
        bool isOpened = false;        // Whether email has been opened
        int openCount = 0;            // Number of times opened
        std::string geoLocation;      // Geographic location (optional)
    };

public:
    /**
     * @brief Constructor
     */
    EmailTrackingStorage();

    /**
     * @brief Destructor
     */
    ~EmailTrackingStorage() = default;

    /**
     * @brief Create a new tracking record for an email
     * @param emailAddress Recipient email address
     * @param emailType Type of email being sent
     * @return Result with tracking ID on success
     */
    Result<std::string> createTrackingRecord(const std::string& emailAddress, 
                                             const std::string& emailType);

    /**
     * @brief Record an email open event
     * @param trackingId Unique tracking ID
     * @param ipAddress IP address of recipient
     * @param userAgent User agent string
     * @return Result indicating success or failure
     */
    Result<bool> recordEmailOpen(const std::string& trackingId, 
                                 const std::string& ipAddress,
                                 const std::string& userAgent);

    /**
     * @brief Get tracking event by tracking ID
     * @param trackingId Unique tracking ID
     * @return Result with tracking event on success
     */
    Result<TrackingEvent> getTrackingEvent(const std::string& trackingId);

    /**
     * @brief Get all tracking events for an email address
     * @param emailAddress Email address to query
     * @param limit Maximum number of results (default: 100)
     * @return Result with vector of tracking events
     */
    Result<std::vector<TrackingEvent>> getTrackingEventsByEmail(const std::string& emailAddress, 
                                                                int limit = 100);

    /**
     * @brief Get tracking statistics for an email address
     * @param emailAddress Email address to query
     * @return Result with JSON statistics (total_sent, total_opened, open_rate)
     */
    Result<std::string> getTrackingStats(const std::string& emailAddress);

    /**
     * @brief Get last error message
     * @return Last error message
     */
    std::string getLastError() const { return lastError_; }

private:
    /**
     * @brief Generate a unique tracking ID
     * @return Unique tracking ID string
     */
    std::string generateTrackingId();

    /**
     * @brief Parse tracking event from BSON document
     * @param doc BSON document
     * @return Tracking event
     */
    TrackingEvent parseTrackingEvent(const bsoncxx::document::view& doc);

    std::unique_ptr<mongocxx::client> client_;
    std::string lastError_;
};

} } // namespace search_engine::storage

