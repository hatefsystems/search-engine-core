#include "websocket/CrawlLogsWebSocketHandler.h"
#include "../include/Logger.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

// Static member definitions
uWS::App* CrawlLogsWebSocketHandler::globalApp = nullptr;
uWS::Loop* CrawlLogsWebSocketHandler::serverLoop = nullptr;
std::deque<std::chrono::steady_clock::time_point> CrawlLogsWebSocketHandler::messageTimes;
std::mutex CrawlLogsWebSocketHandler::rateLimitMutex;

CrawlLogsWebSocketHandler::CrawlLogsWebSocketHandler() {
    LOG_INFO("CrawlLogsWebSocketHandler initialized");
}

void CrawlLogsWebSocketHandler::registerEndpoint(uWS::App& app) {
    LOG_INFO("🔌 CrawlLogsWebSocketHandler::registerEndpoint - Registering WebSocket endpoint /crawl-logs");
    LOG_DEBUG("CrawlLogsWebSocketHandler::registerEndpoint - Initializing global app reference");

    // Store global app reference for pub/sub broadcasting
    globalApp = &app;

    LOG_TRACE("CrawlLogsWebSocketHandler::registerEndpoint - Capturing server loop for deferred operations");
    // Capture the server thread's loop to use for deferred publishes
    serverLoop = uWS::Loop::get();

    LOG_DEBUG("CrawlLogsWebSocketHandler::registerEndpoint - Configuring WebSocket handlers");
    app.ws<PerSocketData>("/crawl-logs", {
        .open = [this](auto* ws) {
            LOG_TRACE("CrawlLogsWebSocketHandler::registerEndpoint - Open handler triggered");
            onOpen(ws);
        },
        .message = [this](auto* ws, std::string_view message, uWS::OpCode opCode) {
            LOG_TRACE("CrawlLogsWebSocketHandler::registerEndpoint - Message handler triggered with opCode: " + std::to_string((int)opCode));
            // Handle incoming messages (heartbeat, etc.)
            onMessage(ws, message, opCode);
        },
        .drain = [this](auto* ws) {
            LOG_TRACE("CrawlLogsWebSocketHandler::registerEndpoint - Drain handler triggered (backpressure relieved)");
            // Called when backpressure is relieved
            onDrain(ws);
        },
        .close = [this](auto* ws, int code, std::string_view message) {
            LOG_TRACE("CrawlLogsWebSocketHandler::registerEndpoint - Close handler triggered with code: " + std::to_string(code));
            onClose(ws, code, message);
        }
    });

    LOG_INFO("✅ CrawlLogsWebSocketHandler registered endpoint: /crawl-logs with session-aware pub/sub pattern");
    LOG_DEBUG("CrawlLogsWebSocketHandler::registerEndpoint - WebSocket endpoint registration completed");
}

void CrawlLogsWebSocketHandler::onOpen(uWS::WebSocket<false, true, PerSocketData>* ws) {
    LOG_INFO("🔗 CrawlLogsWebSocketHandler::onOpen - New WebSocket client connected");
    LOG_DEBUG("CrawlLogsWebSocketHandler::onOpen - Initializing client connection");

    // Mark socket as open for lifecycle management
    ws->getUserData()->open = true;
    LOG_TRACE("CrawlLogsWebSocketHandler::onOpen - Socket marked as open for lifecycle management");

    // Default to admin access (for backward compatibility)
    // Client will send sessionId message to switch to session-specific logs
    ws->getUserData()->isAdmin = true;
    ws->subscribe(CRAWL_LOGS_ADMIN_TOPIC);

    LOG_DEBUG("CrawlLogsWebSocketHandler::onOpen - Client subscribed to admin topic: " + std::string(CRAWL_LOGS_ADMIN_TOPIC));
    LOG_INFO("✅ Client connected to crawl logs WebSocket with admin access");

    // Send welcome message directly (safe in lifecycle callback)
    LOG_TRACE("CrawlLogsWebSocketHandler::onOpen - Preparing welcome message");
    nlohmann::json welcomeMsg = {
        {"level", "info"},
        {"message", "Connected to crawl logs WebSocket"},
        {"timestamp", getCurrentTimestamp()}
    };

    std::string welcomeJson = welcomeMsg.dump();
    LOG_DEBUG("CrawlLogsWebSocketHandler::onOpen - Welcome message prepared (" + std::to_string(welcomeJson.size()) + " bytes)");

    if (!ws->send(welcomeJson, uWS::OpCode::TEXT)) {
        // Backpressure detected, will retry on drain
        LOG_WARNING("⚠️ CrawlLogsWebSocketHandler::onOpen - Welcome message backpressure detected, will retry on drain");
        LOG_DEBUG("CrawlLogsWebSocketHandler::onOpen - Backpressure indicates client may be overwhelmed");
    } else {
        LOG_DEBUG("✅ CrawlLogsWebSocketHandler::onOpen - Welcome message sent successfully");
        LOG_TRACE("CrawlLogsWebSocketHandler::onOpen - WebSocket connection initialization completed");
    }
}

void CrawlLogsWebSocketHandler::onMessage(uWS::WebSocket<false, true, PerSocketData>* ws, std::string_view message, uWS::OpCode opCode) {
    // Check socket lifecycle state
    if (!ws->getUserData()->open) {
        LOG_TRACE("CrawlLogsWebSocketHandler::onMessage - Ignoring message from closed socket");
        return;
    }

    LOG_DEBUG("📨 CrawlLogsWebSocketHandler::onMessage - Received message: '" + std::string(message) + "' (OpCode: " + std::to_string((int)opCode) + ")");
    LOG_TRACE("CrawlLogsWebSocketHandler::onMessage - Message length: " + std::to_string(message.length()) + " bytes");

    // Handle heartbeat or client messages
    if (message == "ping") {
        LOG_DEBUG("💓 CrawlLogsWebSocketHandler::onMessage - Handling PING heartbeat");
        // Safe to send synchronously in lifecycle callback
        if (!ws->send("pong", uWS::OpCode::TEXT)) {
            // Backpressure detected, will retry on drain
            LOG_WARNING("⚠️ CrawlLogsWebSocketHandler::onMessage - PONG backpressure detected, will retry on drain");
            LOG_DEBUG("CrawlLogsWebSocketHandler::onMessage - Client experiencing backpressure during heartbeat");
        } else {
            LOG_DEBUG("✅ CrawlLogsWebSocketHandler::onMessage - PONG sent successfully");
            LOG_TRACE("CrawlLogsWebSocketHandler::onMessage - Heartbeat response completed");
        }
    }
    // Explicitly (re)subscribe to admin topic MUST be handled before generic subscribe
    else if (message == "subscribe:admin") {
        LOG_INFO("🔄 CrawlLogsWebSocketHandler::onMessage - Processing admin topic subscription request");

        // If previously on a session topic, unsubscribe from it
        if (!ws->getUserData()->userTopic.empty()) {
            LOG_DEBUG("CrawlLogsWebSocketHandler::onMessage - Unsubscribing from previous session topic: " + ws->getUserData()->userTopic);
            ws->unsubscribe(ws->getUserData()->userTopic);
            ws->getUserData()->userTopic.clear();
        }

        LOG_DEBUG("CrawlLogsWebSocketHandler::onMessage - Subscribing to admin topic: " + std::string(CRAWL_LOGS_ADMIN_TOPIC));
        ws->subscribe(CRAWL_LOGS_ADMIN_TOPIC);
        ws->getUserData()->isAdmin = true;
        ws->getUserData()->sessionId.clear();

        LOG_INFO("✅ Client switched to admin logs topic: " + std::string(CRAWL_LOGS_ADMIN_TOPIC));
        
        nlohmann::json confirmMsg = {
            {"level", "info"},
            {"message", "Subscribed to admin logs"},
            {"timestamp", getCurrentTimestamp()}
        };
        std::string confirmJson = confirmMsg.dump();
        ws->send(confirmJson, uWS::OpCode::TEXT);
    }
    // Handle session subscription requests
    else if (message.length() > 10 && message.substr(0, 10) == "subscribe:") {
        std::string sessionId = std::string(message.substr(10)); // Remove "subscribe:" prefix
        LOG_INFO("🔄 CrawlLogsWebSocketHandler::onMessage - Processing session subscription request for: " + sessionId);

        if (!sessionId.empty()) {
            // Unsubscribe from admin topic
            LOG_DEBUG("CrawlLogsWebSocketHandler::onMessage - Unsubscribing from admin topic");
            ws->unsubscribe(CRAWL_LOGS_ADMIN_TOPIC);

            // Subscribe to session-specific topic
            std::string sessionTopic = std::string(CRAWL_LOGS_SESSION_PREFIX) + sessionId;
            LOG_DEBUG("CrawlLogsWebSocketHandler::onMessage - Subscribing to session topic: " + sessionTopic);
            ws->subscribe(sessionTopic);

            // Update user data
            ws->getUserData()->isAdmin = false;
            ws->getUserData()->sessionId = sessionId;
            ws->getUserData()->userTopic = sessionTopic;

            LOG_INFO("✅ Client switched to session-specific logs for session: " + sessionId);
            LOG_DEBUG("CrawlLogsWebSocketHandler::onMessage - Session topic subscription completed: " + sessionTopic);

            // Send confirmation
            LOG_TRACE("CrawlLogsWebSocketHandler::onMessage - Preparing session subscription confirmation");
            nlohmann::json confirmMsg = {
                {"level", "info"},
                {"message", "Subscribed to session logs: " + sessionId},
                {"timestamp", getCurrentTimestamp()}
            };
            std::string confirmJson = confirmMsg.dump();
            ws->send(confirmJson, uWS::OpCode::TEXT);
            LOG_TRACE("CrawlLogsWebSocketHandler::onMessage - Session subscription confirmation sent");
        } else {
            LOG_WARNING("⚠️ CrawlLogsWebSocketHandler::onMessage - Empty session ID in subscription request");
        }
    }
    // Log other messages for debugging
    else if (!message.empty()) {
        LOG_DEBUG("📨 CrawlLogsWebSocketHandler::onMessage - Non-ping message received: " + std::string(message));
        LOG_INFO("Received WebSocket message: " + std::string(message));
    } else {
        LOG_TRACE("CrawlLogsWebSocketHandler::onMessage - Empty message received");
    }
}

void CrawlLogsWebSocketHandler::onDrain(uWS::WebSocket<false, true, PerSocketData>* ws) {
    // Called when backpressure is relieved - connection can handle more data
    LOG_INFO("WebSocket backpressure relieved for client");
}

void CrawlLogsWebSocketHandler::onClose(uWS::WebSocket<false, true, PerSocketData>* ws, int code, std::string_view message) {
    LOG_INFO("🔌 CrawlLogsWebSocketHandler::onClose - WebSocket client disconnected");

    // Mark socket as closed for lifecycle management
    ws->getUserData()->open = false;
    LOG_TRACE("CrawlLogsWebSocketHandler::onClose - Socket marked as closed");

    // uWS automatically unsubscribes on close
    LOG_DEBUG("CrawlLogsWebSocketHandler::onClose - uWS will automatically unsubscribe from topics");

    LOG_DEBUG("CrawlLogsWebSocketHandler::onClose - Disconnect details - Code: " + std::to_string(code) +
              ", Message: '" + std::string(message) + "'");

    LOG_INFO("👋 Client disconnected from crawl logs WebSocket. Code: " + std::to_string(code) +
             ", Message: " + std::string(message));
}

void CrawlLogsWebSocketHandler::broadcastLog(const std::string& message, const std::string& level) {
    LOG_TRACE("📢 CrawlLogsWebSocketHandler::broadcastLog - Broadcasting message: [" + level + "] " + message.substr(0, 100) + "...");

    // Check rate limiting first
    if (shouldThrottleMessage()) {
        LOG_WARNING("⚠️ CrawlLogsWebSocketHandler::broadcastLog - Message throttled due to rate limiting");
        LOG_DEBUG("CrawlLogsWebSocketHandler::broadcastLog - Rate limit exceeded, skipping message broadcast");
        return; // Skip this message due to rate limiting
    }

    // Ensure we have an app reference for pub/sub
    if (!globalApp) {
        LOG_ERROR("❌ CrawlLogsWebSocketHandler::broadcastLog - No globalApp reference, cannot broadcast");
        LOG_DEBUG("CrawlLogsWebSocketHandler::broadcastLog - WebSocket handler not properly initialized");
        return;
    }

    // Console log for debugging
    LOG_DEBUG("📡 CrawlLogsWebSocketHandler::broadcastLog - Broadcasting log message: [" + level + "] " + message.substr(0, 200) + "...");
    
    // Create JSON message with timestamp
    nlohmann::json logMsg = {
        {"level", level},
        {"message", message},
        {"timestamp", getCurrentTimestamp()}
    };
    
    std::string jsonString = logMsg.dump();
    
    // Check message size to prevent overwhelming clients
    if (jsonString.size() > 15 * 1024) { // 15KB limit
        LOG_WARNING("WebSocket message too large (" + std::to_string(jsonString.size()) + " bytes), truncating");
        logMsg["message"] = message.substr(0, 1000) + "... [MESSAGE TRUNCATED]";
        jsonString = logMsg.dump();
    }
    
    // Use loop->defer() for thread-safe broadcasting from external threads
    uWS::Loop* loop = serverLoop ? serverLoop : uWS::Loop::get();
    loop->defer([jsonString]() {
        // This lambda runs on the correct uWS thread
        if (globalApp) {
            // Broadcast to admin topic (admin clients see all logs)
            globalApp->publish(CRAWL_LOGS_ADMIN_TOPIC, jsonString, uWS::OpCode::TEXT);
            LOG_DEBUG("Message published to admin topic: " + std::string(CRAWL_LOGS_ADMIN_TOPIC));
        } else {
            LOG_DEBUG("No globalApp in deferred lambda");
        }
    });
}

void CrawlLogsWebSocketHandler::broadcastToSession(const std::string& sessionId, const std::string& message, const std::string& level) {
    // Check rate limiting first
    if (shouldThrottleMessage()) {
        LOG_DEBUG("Session message throttled due to rate limiting");
        return; // Skip this message due to rate limiting
    }
    
    // Ensure we have an app reference for pub/sub
    if (!globalApp) {
        LOG_DEBUG("No globalApp reference, cannot broadcast to session");
        return;
    }
    
    // Console log for debugging
    LOG_DEBUG("Broadcasting session log message: [" + level + "] " + message + " (Session: " + sessionId + ")");
    
    // Create JSON message with timestamp and session info
    nlohmann::json logMsg = {
        {"level", level},
        {"message", message},
        {"sessionId", sessionId},
        {"timestamp", getCurrentTimestamp()}
    };
    
    std::string jsonString = logMsg.dump();
    
    // Check message size to prevent overwhelming clients
    if (jsonString.size() > 15 * 1024) { // 15KB limit
        LOG_WARNING("WebSocket session message too large (" + std::to_string(jsonString.size()) + " bytes), truncating");
        logMsg["message"] = message.substr(0, 1000) + "... [MESSAGE TRUNCATED]";
        jsonString = logMsg.dump();
    }
    
    // Use loop->defer() for thread-safe broadcasting from external threads
    uWS::Loop* loop = serverLoop ? serverLoop : uWS::Loop::get();
    loop->defer([jsonString, sessionId]() {
        // This lambda runs on the correct uWS thread
        if (globalApp) {
            // Broadcast to both admin topic and session-specific topic
            std::string sessionTopic = std::string(CRAWL_LOGS_SESSION_PREFIX) + sessionId;
            
            // Send to admin (admin sees all sessions)
            globalApp->publish(CRAWL_LOGS_ADMIN_TOPIC, jsonString, uWS::OpCode::TEXT);
            // Send to specific session (user sees only their session)
            globalApp->publish(sessionTopic, jsonString, uWS::OpCode::TEXT);
            
            LOG_DEBUG("Message published to admin and session topic: " + sessionTopic);
        } else {
            LOG_DEBUG("No globalApp in session deferred lambda");
        }
    });
}

// Helper function to get current timestamp
std::string CrawlLogsWebSocketHandler::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    return ss.str();
}

// Rate limiting function to prevent overwhelming WebSocket clients
bool CrawlLogsWebSocketHandler::shouldThrottleMessage() {
    std::lock_guard<std::mutex> lock(rateLimitMutex);
    
    auto now = std::chrono::steady_clock::now();
    
    // Remove old entries outside the rate limit window
    size_t oldSize = messageTimes.size();
    while (!messageTimes.empty() && 
           now - messageTimes.front() > RATE_LIMIT_WINDOW) {
        messageTimes.pop_front();
    }
    if (oldSize != messageTimes.size()) {
        LOG_DEBUG("Rate limit window cleanup: " + std::to_string(oldSize) + " -> " + std::to_string(messageTimes.size()) + " messages");
    }
    
    // Check if we've exceeded the rate limit
    if (messageTimes.size() >= MAX_MESSAGES_PER_SECOND) {
        static auto lastThrottleLog = std::chrono::steady_clock::now();
        // Log throttling message only once per second to avoid spam
        if (now - lastThrottleLog > std::chrono::seconds(1)) {
            LOG_DEBUG("⚠️ RATE LIMITING ACTIVE! Messages: " + std::to_string(messageTimes.size()) + "/" + std::to_string(MAX_MESSAGES_PER_SECOND));
            LOG_WARNING("WebSocket message rate limiting active - dropping messages");
            lastThrottleLog = now;
        }
        return true;
    }
    
    // Add current time to the queue
    messageTimes.push_back(now);
    LOG_DEBUG("Rate limit check passed: " + std::to_string(messageTimes.size()) + "/" + std::to_string(MAX_MESSAGES_PER_SECOND) + " messages");
    return false;
}

// Helper function to parse session ID from query parameters
std::string CrawlLogsWebSocketHandler::parseSessionIdFromQuery(const std::string& query) {
    // Simple query parser for sessionId parameter
    // Format: sessionId=crawl_123456789_001 or sessionId=crawl_123456789_001&other=value
    std::string sessionIdPrefix = "sessionId=";
    size_t pos = query.find(sessionIdPrefix);
    
    if (pos != std::string::npos) {
        size_t start = pos + sessionIdPrefix.length();
        size_t end = query.find('&', start);
        
        if (end == std::string::npos) {
            return query.substr(start);
        } else {
            return query.substr(start, end - start);
        }
    }
    
    return ""; // No session ID found
}