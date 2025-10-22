#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <filesystem>
#include <uwebsockets/App.h>
#include "../include/routing/RouteRegistry.h"
#include "../include/Logger.h"

// Include all controllers to trigger their static initialization
#include "controllers/HomeController.h"
#include "controllers/SearchController.h"
#include "controllers/StaticFileController.h"
#include "controllers/TemplatesController.h"
#include "../include/search_engine/crawler/templates/PrebuiltTemplates.h"
#include "../include/search_engine/crawler/templates/TemplateStorage.h"

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
// Crash handler to log a backtrace on segfaults
void installCrashHandler() {
    auto handler = [](int sig) {
        void* array[64];
        size_t size = backtrace(array, 64);
        char** messages = backtrace_symbols(array, size);
        std::cerr << "[FATAL] Signal " << sig << " received. Backtrace (" << size << "):\n";
        if (messages) {
            for (size_t i = 0; i < size; ++i) {
                std::cerr << messages[i] << "\n";
            }
        }
        std::cerr.flush();
        _exit(128 + sig);
    };
    std::signal(SIGSEGV, handler);
    std::signal(SIGABRT, handler);
}



// Helper function to get current timestamp
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

// Request tracing middleware
void traceRequest(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    std::string_view method = req->getMethod();
    std::string_view path = req->getUrl();
    std::string_view query = req->getQuery();
    
    std::string logMessage = "[" + getCurrentTimestamp() + "] " + std::string(method) + " " + std::string(path);
    
    if (!query.empty()) {
        logMessage += "?" + std::string(query);
    }
    
    // Log headers
    logMessage += "\nHeaders:";
    // Note: uWebSockets doesn't provide direct header iteration
    // We can log specific headers we're interested in
    logMessage += "\n  User-Agent: " + std::string(req->getHeader("user-agent"));
    logMessage += "\n  Accept: " + std::string(req->getHeader("accept"));
    logMessage += "\n  Content-Type: " + std::string(req->getHeader("content-type"));
    
    LOG_INFO(logMessage);
}

int main() {
    std::cout << "[MAIN-DEBUG] ============== SEARCH ENGINE STARTING ==============" << std::endl;
    installCrashHandler();
    
    // Initialize logger
    std::cout << "[MAIN-DEBUG] Initializing logger..." << std::endl;
    Logger::getInstance().init(LogLevel::INFO, true, "server.log");
    std::cout << "[MAIN-DEBUG] Logger initialized successfully" << std::endl;
    
    // Seed prebuilt templates (Phase 3)
    try {
        // Load persisted templates if available, then seed defaults (no-op if names exist)
        const char* templatesPathEnv = std::getenv("TEMPLATES_PATH");
        std::string templatesPath = templatesPathEnv ? templatesPathEnv : "/app/config/templates";
        
        // Check if path is a directory or file
        if (std::filesystem::exists(templatesPath) && std::filesystem::is_directory(templatesPath)) {
            search_engine::crawler::templates::loadTemplatesFromDirectory(templatesPath);
        } else {
            // Fallback to single file loading
            search_engine::crawler::templates::loadTemplatesFromFile(templatesPath);
        }
        
        search_engine::crawler::templates::seedPrebuiltTemplates();
        LOG_INFO("Prebuilt templates seeded");
    } catch (...) {
        LOG_WARNING("Failed to seed prebuilt templates (non-fatal)");
    }

    // Log registered routes
    std::cout << "[MAIN-DEBUG] Logging registered routes..." << std::endl;
    LOG_INFO("=== Registered Routes ===");
    for (const auto& route : routing::RouteRegistry::getInstance().getRoutes()) {
        LOG_INFO(routing::methodToString(route.method) + " " + route.path + 
                 " -> " + route.controllerName + "::" + route.actionName);
    }
    LOG_INFO("========================");
    std::cout << "[MAIN-DEBUG] All routes logged" << std::endl;

    // Get port from environment variable or use default
    const char* port_env = std::getenv("PORT");
    int port = port_env ? std::stoi(port_env) : 3000;
    std::cout << "[MAIN-DEBUG] Using port: " << port << std::endl;
    LOG_INFO("Using port: " + std::to_string(port));

    // Create app and apply all registered routes
    std::cout << "[MAIN-DEBUG] Creating uWebSockets app..." << std::endl;
    auto app = uWS::App();
    std::cout << "[MAIN-DEBUG] uWebSockets app created successfully" << std::endl;

    // WebSocket registry and handler injection
    std::cout << "[MAIN-DEBUG] Setting up WebSocket registry..." << std::endl;
    WebSocketRegistry wsRegistry;
    std::cout << "[MAIN-DEBUG] Adding DateTime WebSocket handler..." << std::endl;
    wsRegistry.addHandler(std::make_shared<DateTimeWebSocketHandler>());
    // Create and register crawl logs WebSocket handler
    std::cout << "[MAIN-DEBUG] Adding CrawlLogs WebSocket handler..." << std::endl;
    wsRegistry.addHandler(std::make_shared<CrawlLogsWebSocketHandler>());
    std::cout << "[MAIN-DEBUG] Registering all WebSocket handlers..." << std::endl;
    wsRegistry.registerAll(app);
    std::cout << "[MAIN-DEBUG] All WebSocket handlers registered successfully" << std::endl;
    
    // Connect CrawlLogger to WebSocket handler for real-time logging
    std::cout << "[MAIN-DEBUG] Setting up WebSocket broadcast functions..." << std::endl;
    
    // General broadcast function (for admin and legacy support)
    CrawlLogger::setLogBroadcastFunction([](const std::string& message, const std::string& level) {
        std::cout << "[MAIN-DEBUG] Lambda called for WebSocket broadcast: [" << level << "] " << message << std::endl;
        CrawlLogsWebSocketHandler::broadcastLog(message, level);
    });
    
    // Session-specific broadcast function
    CrawlLogger::setSessionLogBroadcastFunction([](const std::string& sessionId, const std::string& message, const std::string& level) {
        std::cout << "[MAIN-DEBUG] Lambda called for session WebSocket broadcast: [" << level << "] " << message << " (Session: " << sessionId << ")" << std::endl;
        CrawlLogsWebSocketHandler::broadcastToSession(sessionId, message, level);
    });
    
    std::cout << "[MAIN-DEBUG] WebSocket broadcast functions set successfully" << std::endl;
    
    // Add request tracing middleware wrapper
    std::cout << "[MAIN-DEBUG] Applying routes to app..." << std::endl;
    routing::RouteRegistry::getInstance().applyRoutes(app);
    std::cout << "[MAIN-DEBUG] Routes applied successfully" << std::endl;
    
    // Start the server
    std::cout << "[MAIN-DEBUG] Starting server on port " << port << "..." << std::endl;
    app.listen(port, [port](auto* listen_socket) {
        
        if (listen_socket) {
            std::cout << "[MAIN-DEBUG] ✅ SERVER STARTED SUCCESSFULLY! Port: " << port << std::endl;
            std::cout << "[MAIN-DEBUG] ✅ WebSocket endpoint: ws://localhost:" << port << "/crawl-logs" << std::endl;
            std::cout << "[MAIN-DEBUG] ✅ Crawl tester page: http://localhost:" << port << "/crawl-tester.html" << std::endl;
            LOG_INFO("Server listening on port " + std::to_string(port));
            LOG_INFO("Access the search engine at: http://localhost:" + std::to_string(port) + "/test");
            LOG_INFO("Coming soon page at: http://localhost:" + std::to_string(port) + "/");
        }
        else {
            std::cout << "[MAIN-DEBUG] ❌ FAILED TO START SERVER on port " << port << std::endl;
            LOG_ERROR("Failed to listen on port " + std::to_string(port));
        }
    }).run();
    
    std::cout << "[MAIN-DEBUG] ============== SEARCH ENGINE STOPPED ==============" << std::endl;

    return 0;
}

