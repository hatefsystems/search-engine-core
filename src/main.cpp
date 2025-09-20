#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <filesystem>
#include <algorithm>
#include <uwebsockets/App.h>
#include "../include/routing/RouteRegistry.h"
#include "../include/Logger.h"

// Include all controllers to trigger their static initialization
#include "controllers/HomeController.h"
#include "controllers/SearchController.h"
#include "controllers/StaticFileController.h"
#include "controllers/UnsubscribeController.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>
#include <unordered_set>
#include "websocket/WebSocketRegistry.h"
#include "websocket/DateTimeWebSocketHandler.h"
#include "websocket/CrawlLogsWebSocketHandler.h"
#include <iostream>
#include "../include/crawler/CrawlLogger.h"
#include <csignal>
#include <execinfo.h>

using namespace std;

// Helper function to get current timestamp for logging
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    return ss.str();
}

// Crash handler to log a backtrace on segfaults
void installCrashHandler() {
    LOG_INFO("Installing crash handler for signals: SEGV, ABRT, ILL, FPE, TERM, INT");

    auto handler = [](int sig) {
        LOG_ERROR("FATAL SIGNAL RECEIVED: " + std::to_string(sig));

        // Get backtrace
        void* array[64];
        size_t size = backtrace(array, 64);
        char** messages = backtrace_symbols(array, size);

        std::cerr << "[FATAL] Signal " << sig << " received. Backtrace (" << size << "):\n";
        if (messages) {
            for (size_t i = 0; i < size; ++i) {
                std::cerr << messages[i] << "\n";
                LOG_ERROR("Backtrace[" + std::to_string(i) + "]: " + std::string(messages[i]));
            }
            free(messages);
        }
        std::cerr.flush();

        LOG_ERROR("Application terminating due to fatal signal: " + std::to_string(sig));
        _exit(128 + sig);
    };

    // Install handlers for common crash signals
    std::signal(SIGSEGV, handler);  // Segmentation fault
    std::signal(SIGABRT, handler);  // Abort signal
    std::signal(SIGILL, handler);   // Illegal instruction
    std::signal(SIGFPE, handler);   // Floating point exception
    std::signal(SIGTERM, handler);  // Termination request
    std::signal(SIGINT, handler);   // Interrupt (Ctrl+C)

    LOG_INFO("Crash handler installed successfully");
}



// Request tracing middleware
void traceRequest(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    std::string_view method = req->getMethod();
    std::string_view path = req->getUrl();
    std::string_view query = req->getQuery();
    std::string_view userAgent = req->getHeader("user-agent");
    std::string_view contentType = req->getHeader("content-type");

    LOG_DEBUG("Incoming request: " + std::string(method) + " " + std::string(path) +
              (!query.empty() ? "?" + std::string(query) : "") +
              " | User-Agent: " + std::string(userAgent) +
              " | Content-Type: " + std::string(contentType));
}

int main() {
    // Log application startup
    LOG_INFO("============== SEARCH ENGINE STARTING ==============");
    LOG_DEBUG("Application startup initiated at: " + getCurrentTimestamp());

    // Install crash handler first
    LOG_DEBUG("Installing crash handler...");
    installCrashHandler();

    // Initialize logger with configurable log level
    LOG_DEBUG("Initializing logger with configurable log level...");

    // Get log level from environment variable or use default
    LogLevel logLevel = LogLevel::INFO; // Default
    const char* logLevel_env = std::getenv("LOG_LEVEL");
    if (logLevel_env) {
        std::string logLevelStr = logLevel_env;
        std::transform(logLevelStr.begin(), logLevelStr.end(), logLevelStr.begin(), ::tolower);
        if (logLevelStr == "trace") {
            logLevel = LogLevel::TRACE;
            LOG_INFO("Log level set to TRACE (maximum verbosity)");
        } else if (logLevelStr == "debug") {
            logLevel = LogLevel::DEBUG;
            LOG_INFO("Log level set to DEBUG (development mode)");
        } else if (logLevelStr == "info") {
            logLevel = LogLevel::INFO;
            LOG_INFO("Log level set to INFO (production mode)");
        } else if (logLevelStr == "warning") {
            logLevel = LogLevel::WARNING;
            LOG_INFO("Log level set to WARNING (minimal mode)");
        } else if (logLevelStr == "error") {
            logLevel = LogLevel::ERR;
            LOG_INFO("Log level set to ERROR (critical only)");
        } else if (logLevelStr == "none") {
            logLevel = LogLevel::NONE;
            LOG_INFO("Log level set to NONE (silent mode)");
        } else {
            LOG_WARNING("Invalid LOG_LEVEL environment variable: " + std::string(logLevel_env) + ". Using default INFO level.");
        }
    } else {
        LOG_INFO("No LOG_LEVEL environment variable set, using default INFO level");
    }

    Logger::getInstance().init(logLevel, true, "server.log");
    LOG_DEBUG("Logger initialized successfully with level: " + std::to_string(static_cast<int>(logLevel)));

    // Log registered routes
    LOG_DEBUG("Loading and logging registered routes...");
    LOG_INFO("=== REGISTERED ROUTES ===");
    const auto& routes = routing::RouteRegistry::getInstance().getRoutes();
    LOG_INFO("Total routes registered: " + std::to_string(routes.size()));

    for (const auto& route : routes) {
        LOG_INFO(routing::methodToString(route.method) + " " + route.path +
                 " -> " + route.controllerName + "::" + route.actionName);
    }
    LOG_INFO("=============================");
    LOG_DEBUG("Route registration completed successfully");

    // Get port from environment variable or use default
    const char* port_env = std::getenv("PORT");
    int port = 3000; // Default port
    if (port_env) {
        try {
            port = std::stoi(port_env);
            if (port < 1 || port > 65535) {
                LOG_WARNING("Invalid port number in PORT environment variable: " + std::string(port_env) + ". Using default port 3000.");
                port = 3000;
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Invalid PORT environment variable: " + std::string(port_env ? port_env : "null") + ". Using default port 3000. Error: " + e.what());
            port = 3000;
        }
    } else {
        LOG_DEBUG("No PORT environment variable set, using default port 3000");
    }

    LOG_INFO("Server will listen on port: " + std::to_string(port));
    LOG_DEBUG("Port configuration: " + std::to_string(port) + " (valid range: 1-65535)");

    // Create app and apply all registered routes
    LOG_DEBUG("Creating uWebSockets application instance...");
    auto app = uWS::App();
    LOG_DEBUG("uWebSockets application created successfully");

    // WebSocket registry and handler injection
    LOG_DEBUG("Setting up WebSocket registry and handlers...");
    LOG_DEBUG("Initializing WebSocketRegistry instance...");
    WebSocketRegistry wsRegistry;

    LOG_DEBUG("Adding DateTime WebSocket handler...");
    auto dateTimeHandler = std::make_shared<DateTimeWebSocketHandler>();
    wsRegistry.addHandler(dateTimeHandler);
    LOG_INFO("DateTime WebSocket handler added successfully");

    LOG_DEBUG("Adding CrawlLogs WebSocket handler...");
    auto crawlLogsHandler = std::make_shared<CrawlLogsWebSocketHandler>();
    wsRegistry.addHandler(crawlLogsHandler);
    LOG_INFO("CrawlLogs WebSocket handler added successfully");

    LOG_DEBUG("Registering all WebSocket handlers with uWS application...");
    wsRegistry.registerAll(app);
    LOG_INFO("All WebSocket handlers registered successfully - endpoints: /datetime, /crawl-logs");
    
    // Connect CrawlLogger to WebSocket handler for real-time logging
    LOG_DEBUG("Setting up WebSocket broadcast functions for CrawlLogger...");

    // General broadcast function (for admin and legacy support)
    LOG_DEBUG("Configuring general log broadcast function...");
    CrawlLogger::setLogBroadcastFunction([](const std::string& message, const std::string& level) {
        LOG_DEBUG("WebSocket broadcast triggered: [" + level + "] " + message);
        CrawlLogsWebSocketHandler::broadcastLog(message, level);
        LOG_TRACE("General WebSocket broadcast completed for message: " + message.substr(0, 100) + "...");
    });
    LOG_INFO("General log broadcast function configured successfully");

    // Session-specific broadcast function
    LOG_DEBUG("Configuring session-specific log broadcast function...");
    CrawlLogger::setSessionLogBroadcastFunction([](const std::string& sessionId, const std::string& message, const std::string& level) {
        LOG_DEBUG("Session WebSocket broadcast triggered: [" + level + "] " + message + " (Session: " + sessionId + ")");
        CrawlLogsWebSocketHandler::broadcastToSession(sessionId, message, level);
        LOG_TRACE("Session WebSocket broadcast completed for session: " + sessionId);
    });
    LOG_INFO("Session-specific log broadcast function configured successfully");

    LOG_DEBUG("WebSocket broadcast functions setup completed");
    
    // Add request tracing middleware wrapper
    LOG_DEBUG("Applying registered routes to uWebSockets application...");
    routing::RouteRegistry::getInstance().applyRoutes(app);
    LOG_INFO("All routes applied successfully to application");

    // Start the server
    LOG_INFO("Starting HTTP/WebSocket server on port " + std::to_string(port) + "...");
    LOG_DEBUG("Server startup configuration: Port=" + std::to_string(port) +
              ", WebSocket endpoints enabled, Request tracing enabled");

    app.listen(port, [port](auto* listen_socket) {
        if (listen_socket) {
            LOG_INFO("🎉 SERVER STARTED SUCCESSFULLY!");
            LOG_INFO("📡 Listening on port: " + std::to_string(port));
            LOG_INFO("🌐 HTTP endpoints available at: http://localhost:" + std::to_string(port));
            LOG_INFO("🔌 WebSocket endpoints:");
            LOG_INFO("  • Crawl logs: ws://localhost:" + std::to_string(port) + "/crawl-logs");
            LOG_INFO("  • DateTime: ws://localhost:" + std::to_string(port) + "/datetime");
            LOG_INFO("📄 Test pages:");
            LOG_INFO("  • Search interface: http://localhost:" + std::to_string(port) + "/test");
            LOG_INFO("  • Crawl tester: http://localhost:" + std::to_string(port) + "/crawl-tester.html");
            LOG_INFO("  • Coming soon: http://localhost:" + std::to_string(port) + "/");

            LOG_DEBUG("Server initialization completed successfully at: " + getCurrentTimestamp());
        } else {
            LOG_ERROR("❌ FAILED TO START SERVER on port " + std::to_string(port));
            LOG_ERROR("Possible causes: Port already in use, insufficient permissions, or system resource limits");
            LOG_ERROR("Try using a different port or check system logs for details");
        }
    }).run();

    LOG_INFO("============== SEARCH ENGINE STOPPED ==============");
    LOG_DEBUG("Application shutdown completed at: " + getCurrentTimestamp());

    return 0;
}

