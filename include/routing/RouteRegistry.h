#pragma once
#include "Route.h"
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_set>

namespace routing {

class RouteRegistry {
public:
    static RouteRegistry& getInstance() {
        static RouteRegistry instance;
        return instance;
    }

    // Register a route
    void registerRoute(const Route& route);

    // Get all registered routes
    const std::vector<Route>& getRoutes() const { return routes; }

    // Apply routes to uWebSockets app
    template<bool SSL>
    void applyRoutes(uWS::TemplatedApp<SSL>& app);
    
    // Set middleware for request tracing
    void setTraceRequests(bool trace) { traceRequests = trace; }

private:
    RouteRegistry() = default;
    ~RouteRegistry() = default;
    RouteRegistry(const RouteRegistry&) = delete;
    RouteRegistry& operator=(const RouteRegistry&) = delete;

    std::vector<Route> routes;
    mutable std::mutex mutex;
    bool traceRequests = true;
};

// Helper to create a wrapped handler with request tracing
template<bool SSL>
std::function<void(uWS::HttpResponse<SSL>*, uWS::HttpRequest*)> wrapHandler(
    const std::function<void(uWS::HttpResponse<SSL>*, uWS::HttpRequest*)>& handler,
    const std::string& method,
    const std::string& path,
    bool trace) {
    
    return [handler, method, path, trace](uWS::HttpResponse<SSL>* res, uWS::HttpRequest* req) {
            // NOTE: Do not write headers before the handler to allow handlers
            // to set custom HTTP status codes (e.g., 202 Accepted).
        if (trace) {
            std::string_view queryString = req->getQuery();
            std::string logMessage = "[" + method + "] " + path;
            if (!queryString.empty()) {
                logMessage += "?" + std::string(queryString);
            }
            // Simple logging - in production you'd use proper logger
            std::cout << logMessage << std::endl;
        }
        handler(res, req);
    };
}

// Template implementation
template<bool SSL>
void RouteRegistry::applyRoutes(uWS::TemplatedApp<SSL>& app) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Deduplicate by method + path to avoid uWebSockets internal routing errors
    std::unordered_set<std::string> registered;
    registered.reserve(routes.size());

    for (const auto& route : routes) {
        const std::string dedupeKey = methodToString(route.method) + " " + route.path;
        if (registered.find(dedupeKey) != registered.end()) {
            // Skip duplicate registration
            continue;
        }
        registered.insert(dedupeKey);
        auto wrappedHandler = wrapHandler<SSL>(
            route.handler, 
            methodToString(route.method), 
            route.path, 
            traceRequests
        );
        
        switch (route.method) {
            case HttpMethod::GET:
                app.get(route.path, wrappedHandler);
                break;
            case HttpMethod::POST:
                app.post(route.path, wrappedHandler);
                break;
            case HttpMethod::PUT:
                app.put(route.path, wrappedHandler);
                break;
            case HttpMethod::DELETE:
                app.del(route.path, wrappedHandler);
                break;
            case HttpMethod::PATCH:
                app.patch(route.path, wrappedHandler);
                break;
            case HttpMethod::OPTIONS:
                app.options(route.path, wrappedHandler);
                break;
            case HttpMethod::HEAD:
                app.head(route.path, wrappedHandler);
                break;
        }
    }
}

} // namespace routing 