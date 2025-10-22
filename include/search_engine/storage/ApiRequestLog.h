#pragma once

#include <string>
#include <chrono>
#include <optional>

namespace search_engine {
namespace storage {

struct ApiRequestLog {
    std::optional<std::string> id; // MongoDB ObjectId
    std::string endpoint;           // API endpoint path
    std::string method;             // HTTP method (GET, POST, etc.)
    std::string ipAddress;          // Client IP address
    std::string userAgent;          // User agent string
    std::chrono::system_clock::time_point createdAt; // Request timestamp
    std::optional<std::string> requestBody; // Optional request body for logging
    std::optional<std::string> sessionId;   // Optional session ID if applicable
    std::optional<std::string> userId;      // Optional user ID if authenticated
    std::string status;             // Response status (success, error, etc.)
    std::optional<std::string> errorMessage; // Error message if applicable
    int responseTimeMs;             // Response time in milliseconds
};

} // namespace storage
} // namespace search_engine
