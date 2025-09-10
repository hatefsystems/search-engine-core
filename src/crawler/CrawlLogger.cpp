#include "../../include/crawler/CrawlLogger.h"
#include "../../include/Logger.h"
#include <iostream>

// Static member definitions
CrawlLogger::LogBroadcastFunction CrawlLogger::logBroadcastFunction_ = nullptr;
CrawlLogger::SessionLogBroadcastFunction CrawlLogger::sessionLogBroadcastFunction_ = nullptr;

void CrawlLogger::setLogBroadcastFunction(LogBroadcastFunction func) {
    logBroadcastFunction_ = func;
}

void CrawlLogger::setSessionLogBroadcastFunction(SessionLogBroadcastFunction func) {
    sessionLogBroadcastFunction_ = func;
}

void CrawlLogger::broadcastLog(const std::string& message, const std::string& level) {
    LOG_DEBUG("📡 CrawlLogger::broadcastLog - Broadcasting message: [" + level + "] " + message);

    if (logBroadcastFunction_) {
        LOG_TRACE("CrawlLogger::broadcastLog - Calling WebSocket broadcast function");
        logBroadcastFunction_(message, level);
        LOG_TRACE("✅ CrawlLogger::broadcastLog - WebSocket broadcast function completed");
    } else {
        LOG_DEBUG("⚠️ CrawlLogger::broadcastLog - No WebSocket broadcast function set - message not sent");
    }
    // If no function is set, this is a no-op (safe for tests)
}

void CrawlLogger::broadcastSessionLog(const std::string& sessionId, const std::string& message, const std::string& level) {
    LOG_DEBUG("📡 CrawlLogger::broadcastSessionLog - Broadcasting session message: [" + level + "] " +
              message + " (Session: " + sessionId + ")");

    if (sessionLogBroadcastFunction_) {
        LOG_TRACE("CrawlLogger::broadcastSessionLog - Calling session WebSocket broadcast function");
        sessionLogBroadcastFunction_(sessionId, message, level);
        LOG_TRACE("✅ CrawlLogger::broadcastSessionLog - Session WebSocket broadcast function completed");
    } else {
        LOG_DEBUG("⚠️ CrawlLogger::broadcastSessionLog - No session WebSocket broadcast function set - using general broadcast");
        // Fallback to general broadcast if session function not available
        broadcastLog(message + " (Session: " + sessionId + ")", level);
    }
}