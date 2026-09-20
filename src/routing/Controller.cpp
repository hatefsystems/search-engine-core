#include "../../include/routing/Controller.h"
#include <sstream>
#include <random>
#include <iomanip>
#include <regex>

namespace routing {

// Generate a random nonce for CSP
std::string generateNonce() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << std::setfill('0') << std::setw(2) << dis(gen);
    }
    return ss.str();
}

// Inject nonce into HTML content for inline scripts and styles
std::string injectNonce(const std::string& content, const std::string& nonce) {
    std::string result = content;
    
    // Add nonce to <script> tags without src attribute (inline scripts)
    std::regex scriptRegex(R"(<script(?![^>]*\ssrc=)([^>]*)>)");
    result = std::regex_replace(result, scriptRegex, "<script nonce=\"" + nonce + "\"$1>");
    
    // Add nonce to <style> tags
    std::regex styleRegex(R"(<style([^>]*)>)");
    result = std::regex_replace(result, styleRegex, "<style nonce=\"" + nonce + "\"$1>");
    
    return result;
}

void Controller::json(uWS::HttpResponse<false>* res, const nlohmann::json& data, const std::string& status) {
    res->writeStatus(status)
       ->writeHeader("Content-Type", "application/json")
       ->writeHeader("Cache-Control", "private, no-store")
       ->writeHeader("Access-Control-Allow-Origin", "*")
       ->writeHeader("Server", "HatefEngine 1.0")->end(data.dump());
}

void Controller::text(uWS::HttpResponse<false>* res, const std::string& content, const std::string& status) {
    res->writeStatus(status)
       ->writeHeader("Content-Type", "text/plain")
       ->writeHeader("Server", "HatefEngine 1.0")->end(content);
}

void Controller::html(uWS::HttpResponse<false>* res, const std::string& content, const std::string& status) {
    // Generate a unique nonce for this response
    std::string nonce = generateNonce();
    
    // Inject nonce into HTML content
    std::string processedContent = injectNonce(content, nonce);
    
    // Create CSP with nonce and unsafe-inline for backward compatibility
    std::string csp = "default-src 'self'; "
                      "script-src 'self' 'unsafe-inline' 'nonce-" + nonce + "'; "
                      "style-src 'self' 'unsafe-inline' 'nonce-" + nonce + "'; "
                      "img-src 'self' data: https:; "
                      "font-src 'self' data:; "
                      "connect-src 'self'; "
                      "frame-ancestors 'none'; "
                      "base-uri 'self'; "
                      "form-action 'self'";
    
    res->writeStatus(status)
       ->writeHeader("Content-Type", "text/html; charset=utf-8")
       ->writeHeader("Content-Security-Policy", csp)
       ->writeHeader("X-Content-Type-Options", "nosniff")
       ->writeHeader("X-Frame-Options", "DENY")
       ->writeHeader("X-XSS-Protection", "1; mode=block")
       ->writeHeader("Referrer-Policy", "strict-origin-when-cross-origin")
       ->writeHeader("Cross-Origin-Opener-Policy", "same-origin")
       ->writeHeader("Strict-Transport-Security", "max-age=31536000; includeSubDomains; preload")
       ->writeHeader("Server", "HatefEngine 1.0")->end(processedContent);
}

void Controller::notFound(uWS::HttpResponse<false>* res, const std::string& message) {
    nlohmann::json error = {
        {"error", {
            {"code", "NOT_FOUND"},
            {"message", message}
        }}
    };
    json(res, error, "404 Not Found");
}

void Controller::badRequest(uWS::HttpResponse<false>* res, const std::string& message) {
    nlohmann::json error = {
        {"error", {
            {"code", "BAD_REQUEST"},
            {"message", message}
        }}
    };
    json(res, error, "400 Bad Request");
}

void Controller::serverError(uWS::HttpResponse<false>* res, const std::string& message) {
    nlohmann::json error = {
        {"error", {
            {"code", "INTERNAL_ERROR"},
            {"message", message}
        }}
    };
    json(res, error, "500 Internal Server Error");
}

std::map<std::string, std::string> Controller::parseQuery(uWS::HttpRequest* req) {
    std::map<std::string, std::string> params;
    std::string_view queryString = req->getQuery();
    
    if (queryString.empty()) {
        return params;
    }
    
    std::string query(queryString);
    std::istringstream stream(query);
    std::string pair;
    
    while (std::getline(stream, pair, '&')) {
        size_t equalPos = pair.find('=');
        if (equalPos != std::string::npos) {
            std::string key = pair.substr(0, equalPos);
            std::string value = pair.substr(equalPos + 1);
            
            // URL decode (basic implementation)
            size_t pos = 0;
            while ((pos = value.find('+', pos)) != std::string::npos) {
                value.replace(pos, 1, " ");
                pos++;
            }
            
            params[key] = value;
        }
    }
    
    return params;
}

std::string Controller::getParam(uWS::HttpRequest* req, const std::string& name) {
    // This would be implemented when we add path parameter support
    return "";
}

} // namespace routing 