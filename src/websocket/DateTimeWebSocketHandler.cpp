#include "websocket/DateTimeWebSocketHandler.h"
#include <chrono>
#include <ctime>

DateTimeWebSocketHandler::DateTimeWebSocketHandler() : running(true) {
    broadcastThread = std::thread([this]() { broadcastLoop(); });
}

DateTimeWebSocketHandler::~DateTimeWebSocketHandler() {
    running = false;
    if (broadcastThread.joinable()) broadcastThread.join();
}

void DateTimeWebSocketHandler::registerEndpoint(uWS::App& app) {
    // Store app reference for pub/sub
    this->app = &app;
    
    app.ws<PerSocketData>("/datetime", {
        .upgrade = [](auto* res, auto* req, auto* context) {
            res->writeStatus("101 Switching Protocols")->writeHeader("Server", "HatefEngine 1.0");
            res->template upgrade<PerSocketData>({}, req->getHeader("sec-websocket-key"),
                req->getHeader("sec-websocket-protocol"), req->getHeader("sec-websocket-extensions"), context);
        },
        .open = [this](auto* ws) { 
            // Mark socket as open for lifecycle management
            ws->getUserData()->open = true;
            // Subscribe to datetime topic for pub/sub broadcasting
            ws->subscribe(DATETIME_TOPIC);
        },
        .close = [this](auto* ws, int, std::string_view) { 
            // Mark socket as closed for lifecycle management
            ws->getUserData()->open = false;
            // uWS automatically unsubscribes on close
        }
    });
}

void DateTimeWebSocketHandler::broadcastLoop() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string datetime = std::ctime(&now);
        datetime.erase(datetime.find_last_not_of(" \n\r\t")+1);
        
        // Use loop->defer() for thread-safe broadcasting from external thread
        auto* loop = uWS::Loop::get();
        loop->defer([this, datetime]() {
            // This lambda runs on the correct uWS thread
            if (app) {
                // Use pub/sub pattern - automatically handles lifecycle
                app->publish(DATETIME_TOPIC, datetime, uWS::OpCode::TEXT);
            }
        });
    }
} 