#pragma once

#include <string>
#include <memory>
#include <future>
#include <map>
#include <chrono>
#include <curl/curl.h>

struct BrowserlessRenderResult {
    bool success;
    std::string html;
    std::string error;
    int status_code;
    std::chrono::milliseconds render_time;
};

class BrowserlessClient {
public:
    BrowserlessClient(const std::string& browserless_url);
    ~BrowserlessClient();
    
    // Render a URL and return the fully rendered HTML
    BrowserlessRenderResult renderUrl(const std::string& url, 
                                     int timeout_ms = 60000, 
                                     bool wait_for_network_idle = true);
    
    // Check if browserless service is available
    bool isAvailable();
    
    // Set custom headers for requests
    void setHeaders(const std::map<std::string, std::string>& headers);
    
    // Set custom user agent
    void setUserAgent(const std::string& user_agent);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
}; 