#include "PageFetcher.h"
#include "../../include/Logger.h"
#include "../../include/crawler/CrawlLogger.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <thread>
#include <filesystem>
#include <regex>
#include "../../include/search_engine/common/UrlSanitizer.h"

PageFetcher::PageFetcher(const std::string& userAgent,
                         std::chrono::milliseconds timeout,
                         bool followRedirects,
                         size_t maxRedirects)
    : userAgent(userAgent)
    , timeout(timeout)
    , followRedirects(followRedirects)
    , maxRedirects(maxRedirects)
    , verifySSL(true)
    , spaRenderingEnabled(false) {
    LOG_DEBUG("PageFetcher constructor called with userAgent: " + userAgent);
    
    // Create a unique cookie jar file for this instance
    cookieJarPath = "/tmp/crawler_cookies_" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + ".txt";
    LOG_DEBUG("Cookie jar path: " + cookieJarPath);
    
    initCurl();
}

PageFetcher::~PageFetcher() {
    LOG_DEBUG("PageFetcher destructor called");
    cleanupCurl();
    
    // Clean up cookie jar file
    if (!cookieJarPath.empty()) {
        try {
            if (std::filesystem::exists(cookieJarPath)) {
                std::filesystem::remove(cookieJarPath);
                LOG_DEBUG("Removed cookie jar file: " + cookieJarPath);
            }
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to remove cookie jar file: " + std::string(e.what()));
        }
    }
}

void PageFetcher::initCurl() {
    LOG_DEBUG("PageFetcher::initCurl called");
    curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL");
        throw std::runtime_error("Failed to initialize CURL");
    }
    LOG_DEBUG("CURL initialized successfully");
}

void PageFetcher::cleanupCurl() {
    LOG_DEBUG("PageFetcher::cleanupCurl called");
    if (curl) {
        curl_easy_cleanup(curl);
        curl = nullptr;
        LOG_DEBUG("CURL cleaned up");
    }
}

PageFetchResult PageFetcher::fetch(const std::string& url) {
    using namespace search_engine::common;
    const std::string cleanedUrl = sanitizeUrl(url);
    LOG_INFO("PageFetcher::fetch called for URL: " + cleanedUrl);
    PageFetchResult result;
    result.success = false;
    result.statusCode = 0;  // Initialize status code to prevent garbage data
    
    if (!curl) {
        result.errorMessage = "CURL not initialized";
        LOG_ERROR("Error: " + result.errorMessage);
        // Attempt to re-initialize curl
        try {
            initCurl();
            LOG_DEBUG("Re-initialized CURL");
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to re-initialize CURL: " + std::string(e.what()));
            return result;
        }
    }
    
    // Use a local curl handle for this request to avoid multi-threading issues
    CURL* localCurl = curl_easy_init();
    if (!localCurl) {
        result.errorMessage = "Failed to create local CURL handle";
        LOG_ERROR("Error: " + result.errorMessage);
        return result;
    }
    
    LOG_DEBUG("Created local CURL handle");
    
    // Reset CURL options
    LOG_TRACE("Setting CURL options");
    
    // Set URL
    LOG_TRACE("Setting URL: " + cleanedUrl);
    curl_easy_setopt(localCurl, CURLOPT_URL, cleanedUrl.c_str());
    curl_easy_setopt(localCurl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    // Set error buffer for better diagnostics
    char errbuf[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(localCurl, CURLOPT_ERRORBUFFER, errbuf);
    
    // Set user agent
    LOG_TRACE("Setting user agent: " + userAgent);
    curl_easy_setopt(localCurl, CURLOPT_USERAGENT, userAgent.c_str());
    
    // Set timeout
    long timeoutMs = static_cast<long>(timeout.count());
    LOG_DEBUG("Setting timeout: " + std::to_string(timeoutMs) + "ms");
    curl_easy_setopt(localCurl, CURLOPT_TIMEOUT_MS, timeoutMs);
    
    // Set connection timeout to 10 seconds (increased for container environments)
    LOG_DEBUG("Setting connection timeout: 10000ms");
    curl_easy_setopt(localCurl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    
    // Enhanced DNS resolution settings for container environments
    LOG_DEBUG("Setting DNS timeout: 30 seconds");
    curl_easy_setopt(localCurl, CURLOPT_DNS_CACHE_TIMEOUT, 300L);  // Cache DNS for 5 minutes
    curl_easy_setopt(localCurl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);  // Allow both IPv4 and IPv6
    
    // Set custom DNS servers for better resolution in container environments
    curl_easy_setopt(localCurl, CURLOPT_DNS_SERVERS, "8.8.8.8,8.8.4.4,1.1.1.1");
    
    // Set low-speed time and limit to abort slow connections
    // Avoid misconfigured low speed options causing argument errors on some libcurl builds
    
    // Set redirect options
    LOG_TRACE("Setting follow location: " + std::string(followRedirects ? "true" : "false"));
    curl_easy_setopt(localCurl, CURLOPT_FOLLOWLOCATION, followRedirects ? 1L : 0L);
    LOG_TRACE("Setting max redirects: " + std::to_string(maxRedirects));
    curl_easy_setopt(localCurl, CURLOPT_MAXREDIRS, static_cast<long>(maxRedirects));
    
    // Set SSL verification
    LOG_TRACE("Setting SSL verification: " + std::string(verifySSL ? "true" : "false"));
    curl_easy_setopt(localCurl, CURLOPT_SSL_VERIFYPEER, verifySSL ? 1L : 0L);
    curl_easy_setopt(localCurl, CURLOPT_SSL_VERIFYHOST, verifySSL ? 2L : 0L);
    
    // Set proxy if configured
    if (!proxy.empty()) {
        LOG_DEBUG("Setting proxy: " + proxy);
        curl_easy_setopt(localCurl, CURLOPT_PROXY, proxy.c_str());
    }
    
    // Set custom headers
    struct curl_slist* headers = nullptr;
    for (const auto& header : customHeaders) {
        std::string headerStr = header.first + ": " + header.second;
        LOG_TRACE("Adding custom header: " + headerStr);
        headers = curl_slist_append(headers, headerStr.c_str());
    }
    if (headers) {
        curl_easy_setopt(localCurl, CURLOPT_HTTPHEADER, headers);
    }
    
    // Set write callback
    std::string responseData;
    LOG_TRACE("Setting write callback");
    curl_easy_setopt(localCurl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(localCurl, CURLOPT_WRITEDATA, &responseData);
    
    // Set progress callback if configured
    if (userProgressCallback) {
        LOG_DEBUG("Setting progress callback");
        curl_easy_setopt(localCurl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(localCurl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(localCurl, CURLOPT_XFERINFODATA, &userProgressCallback);
    }

    // Set cookie handling
    LOG_DEBUG("Setting cookie handling");
    curl_easy_setopt(localCurl, CURLOPT_COOKIEFILE, cookieJarPath.c_str());
    curl_easy_setopt(localCurl, CURLOPT_COOKIEJAR, cookieJarPath.c_str());
    
    // Perform the request
    LOG_INFO("Performing CURL request");
    CURLcode res = curl_easy_perform(localCurl);
    
    // Clean up headers
    if (headers) {
        LOG_TRACE("Cleaning up headers");
        curl_slist_free_all(headers);
    }
    
    if (res != CURLE_OK) {
        result.errorMessage = std::string(curl_easy_strerror(res)) + " | errbuf=" + errbuf + " | url_hex=" + hexDump(cleanedUrl);
        result.curlCode = res;  // Store CURL error code
        LOG_ERROR("CURL error: " + result.errorMessage);
        curl_easy_cleanup(localCurl);
        return result;
    }
    
    // Get status code
    long statusCode;
    curl_easy_getinfo(localCurl, CURLINFO_RESPONSE_CODE, &statusCode);
    result.statusCode = static_cast<int>(statusCode);
    LOG_INFO("===> HTTP Response status code: " + std::to_string(result.statusCode) + " for URL: " + cleanedUrl);
    
    // Log different status code categories
    if (result.statusCode >= 200 && result.statusCode < 300) {
        LOG_INFO("HTTP SUCCESS (" + std::to_string(result.statusCode) + "): " + url);
    } else if (result.statusCode >= 300 && result.statusCode < 400) {
        LOG_INFO("HTTP REDIRECT (" + std::to_string(result.statusCode) + "): " + url);
    } else if (result.statusCode >= 400 && result.statusCode < 500) {
        LOG_WARNING("HTTP CLIENT ERROR (" + std::to_string(result.statusCode) + "): " + url);
    } else if (result.statusCode >= 500) {
        LOG_ERROR("HTTP SERVER ERROR (" + std::to_string(result.statusCode) + "): " + url);
    }
    
    // Get content type
    char* contentType = nullptr;
    curl_easy_getinfo(localCurl, CURLINFO_CONTENT_TYPE, &contentType);
    if (contentType) {
        result.contentType = contentType;
        LOG_DEBUG("Response content type: " + result.contentType);
    }
    
    // Get final URL (after redirects)
    char* finalUrl = nullptr;
    curl_easy_getinfo(localCurl, CURLINFO_EFFECTIVE_URL, &finalUrl);
    if (finalUrl) {
        result.finalUrl = finalUrl;
        LOG_DEBUG("Final URL (after redirects): " + result.finalUrl);
    }
    
    result.content = std::move(responseData);
    
    // Set success based on HTTP status code
    // 2xx status codes are considered successful
    result.success = (result.statusCode >= 200 && result.statusCode < 300);
    
    if (result.success) {
        LOG_INFO("Request successful, content size: " + std::to_string(result.content.size()) + " bytes");
    } else {
        LOG_INFO("Request completed with error status " + std::to_string(result.statusCode) + ", content size: " + std::to_string(result.content.size()) + " bytes");
    }
    
    // Clean up the local curl handle
    curl_easy_cleanup(localCurl);
    LOG_DEBUG("Cleaned up local CURL handle");
    
    // Check if this is a SPA and render if enabled
    if (result.success && spaRenderingEnabled && isSpaPage(result.content, cleanedUrl)) {
        LOG_INFO("SPA detected, attempting to render with browserless: " + cleanedUrl);
        CrawlLogger::broadcastLog("🕷️ SPA detected for: " + cleanedUrl + " - switching to headless browser", "info");
        
        if (browserlessClient) {
            try {
                // Measure SPA rendering time with both steady and system clocks for logging
                auto spaStartSteady = std::chrono::steady_clock::now();
                auto spaStartSys = std::chrono::system_clock::now();
                long long spaStartMs = std::chrono::duration_cast<std::chrono::milliseconds>(spaStartSys.time_since_epoch()).count();

                // Get SPA timeout from environment variable or use the passed timeout
                int spaTimeout = std::max(static_cast<int>(timeout.count()), 15000);
                const char* envSpaTimeout = std::getenv("SPA_RENDERING_TIMEOUT");
                if (envSpaTimeout) {
                    try {
                        int envTimeout = std::stoi(envSpaTimeout);
                        spaTimeout = std::max(spaTimeout, envTimeout);
                        LOG_INFO("Using SPA_RENDERING_TIMEOUT from environment: " + std::to_string(envTimeout) + "ms, final timeout: " + std::to_string(spaTimeout) + "ms");
                    } catch (...) {
                        LOG_WARNING("Invalid SPA_RENDERING_TIMEOUT, using calculated timeout: " + std::to_string(spaTimeout) + "ms");
                    }
                }
                auto renderResult = browserlessClient->renderUrl(cleanedUrl, spaTimeout);

                auto spaEndSteady = std::chrono::steady_clock::now();
                auto spaEndSys = std::chrono::system_clock::now();
                long long spaEndMs = std::chrono::duration_cast<std::chrono::milliseconds>(spaEndSys.time_since_epoch()).count();
                auto spaDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(spaEndSteady - spaStartSteady).count();

                if (renderResult.success || !renderResult.html.empty()) {
                    LOG_INFO("Successfully rendered SPA, content size: " + std::to_string(renderResult.html.size()) +
                             " bytes, render_time_ms=" + std::to_string(renderResult.render_time.count()));
                    CrawlLogger::broadcastLog(
                        "✅ SPA successfully rendered for: " + cleanedUrl +
                        " (size=" + std::to_string(renderResult.html.size()) +
                        " bytes, duration_ms=" + std::to_string(renderResult.render_time.count()) +
                        ", ~" + std::to_string(static_cast<double>(renderResult.render_time.count()) / 1000.0) + "s)",
                        "info");
                    result.content = renderResult.html;
                    if (renderResult.status_code > 0) {
                        result.statusCode = renderResult.status_code;
                    }
                } else {

                    result.content = renderResult.html;
                    result.statusCode = renderResult.status_code;
                    
                    std::string msg =
                        "⚠️ SPA rendering failed for: " + cleanedUrl +
                        " - " + renderResult.error +
                        " (using original content)" +
                        " [start_ts_ms=" + std::to_string(spaStartMs) +
                        ", end_ts_ms=" + std::to_string(spaEndMs) +
                        ", duration_ms=" + std::to_string(spaDurationMs) +
                        ", ~" + std::to_string(static_cast<double>(spaDurationMs) / 1000.0) + "s" +
                        ", timeout_ms=" + std::to_string(static_cast<long long>(timeout.count())) + "]";
                    LOG_WARNING(msg);
                    CrawlLogger::broadcastLog(msg, "warning");
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Exception during SPA rendering: " + std::string(e.what()) + ", using original content");
                CrawlLogger::broadcastLog("❌ SPA rendering exception for: " + cleanedUrl + " - " + std::string(e.what()), "error");
            }
        } else {
            LOG_WARNING("BrowserlessClient not available for SPA rendering");
            CrawlLogger::broadcastLog("⚠️ BrowserlessClient not available for SPA: " + url, "warning");
        }
    }
    
    return result;
}

void PageFetcher::setProgressCallback(std::function<void(size_t, size_t)> callback) {
    LOG_DEBUG("PageFetcher::setProgressCallback called");
    userProgressCallback = std::move(callback);
}

void PageFetcher::setCustomHeaders(const std::vector<std::pair<std::string, std::string>>& headers) {
    LOG_DEBUG("PageFetcher::setCustomHeaders called with " + std::to_string(headers.size()) + " headers");
    customHeaders = headers;
}

void PageFetcher::setProxy(const std::string& proxyUrl) {
    LOG_DEBUG("PageFetcher::setProxy called with: " + proxyUrl);
    proxy = proxyUrl;
}

void PageFetcher::setVerifySSL(bool verify) {
    LOG_DEBUG("PageFetcher::setVerifySSL called with: " + std::string(verify ? "true" : "false"));
    verifySSL = verify;
}

size_t PageFetcher::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::string* responseData = static_cast<std::string*>(userp);
    size_t totalSize = size * nmemb;
    responseData->append(static_cast<char*>(contents), totalSize);
    // Too verbose for every chunk:
    // LOG_TRACE("CURL write callback received " + std::to_string(totalSize) + " bytes");
    return totalSize;
}

int PageFetcher::progressCallback(void* clientp, double dltotal, double dlnow, double ultotal, double ulnow) {
    // Mark parameters as unused to avoid compiler warnings
    (void)ultotal;  // Explicitly mark as unused
    (void)ulnow;    // Explicitly mark as unused
    
    auto* callback = static_cast<std::function<void(size_t, size_t)>*>(clientp);
    if (callback && *callback) {
        (*callback)(static_cast<size_t>(dlnow), static_cast<size_t>(dltotal));
    }
    return 0;
}

void PageFetcher::setSpaRendering(bool enable, const std::string& browserless_url, bool useWebsocket, size_t wsConnectionsPerCpu) {
    using namespace search_engine::common;
    const std::string cleaned = sanitizeUrl(browserless_url);
    LOG_DEBUG("PageFetcher::setSpaRendering called with enable: " + std::string(enable ? "true" : "false") + ", url: " + cleaned +
              (cleaned != browserless_url ? (" (sanitized from: " + browserless_url + ")") : ""));
    spaRenderingEnabled = enable;
    
    if (enable) {
        browserlessClient = std::make_unique<BrowserlessClient>(cleaned);
        // Also create transport wrapper with WS preferred based on provided params
        size_t hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 2;
        size_t pool = std::max<size_t>(1, wsConnectionsPerCpu) * hw;
        browserlessTransport = std::make_unique<search_engine::crawler::BrowserlessTransportClient>(
            cleaned,
            useWebsocket,
            pool
        );
        LOG_INFO("BrowserlessClient initialized for SPA rendering");
        CrawlLogger::broadcastLog("🤖 Headless browser enabled for SPA rendering (URL: " + cleaned + ")", "info");
    } else {
        browserlessClient.reset();
        browserlessTransport.reset();
        LOG_INFO("BrowserlessClient disabled");
        CrawlLogger::broadcastLog("🚫 Headless browser disabled", "info");
    }
}

bool PageFetcher::isSpaPage(const std::string& html, const std::string& url) {
    LOG_DEBUG("SPA detection called for URL: " + url + " with HTML size: " + std::to_string(html.size()));
    
    // Only check for VERY specific SPA framework patterns that are DEFINITIVE and UNAMBIGUOUS
    std::vector<std::string> definiteSpaIndicators = {
        // React specific - these are only found in actual React SPAs
        "data-reactroot", "ReactDOM.render", "ReactDOM.createRoot", "ReactDOM.hydrate",
        "window.React", "window.ReactDOM",
        
        // Next.js specific - these are definitive Next.js patterns
        "__NEXT_DATA__", "window.__NEXT_DATA__", "_next/static/chunks/", 
        "next-head-count", "_next/static/css/",
        
        // Nuxt.js specific - these are definitive Nuxt.js patterns
        "__NUXT__", "window.__NUXT__", "_nuxt/static/", "window.$nuxt",
        
        // Gatsby specific - these are definitive Gatsby patterns
        "___gatsby", "window.___gatsby", "__GATSBY", "window.___loader",
        
        // AngularJS (1.x) specific - legacy Angular patterns
        "ng-app=\"", "ng-controller=\"", "[ng-app]", "[ng-controller]", 
        "window.angular", "angular.module",
        
        // Modern Angular (2+) specific - these are definitive modern Angular patterns
        "<app-root>", "<app-root ", "</app-root>",
        "runtime.", "polyfills.", "main.", // Angular CLI build artifacts
        "ng-version", "ng-reflect-", 
        
        // Vue specific - must have Vue-specific patterns, not just @click (which Alpine.js also uses)
        "window.Vue", "Vue.createApp", "Vue.component", "new Vue(",
        "v-if=\"", "v-for=\"", "v-model=\"", "{{", "v-bind:", "v-on:",
        
        // Ember specific 
        "ember-application", "window.Ember", "Ember.Application",
        
        // State management objects that are SPA-specific
        "window.__REDUX_DEVTOOLS_EXTENSION__", "window.__PRELOADED_STATE__",
        "window.__STATE__", "window.__INITIAL_STATE__"
    };
    
    // Count definitive indicators
    int definitiveCount = 0;
    std::vector<std::string> foundIndicators;
    
    for (const auto& indicator : definiteSpaIndicators) {
        if (html.find(indicator) != std::string::npos) {
            definitiveCount++;
            foundIndicators.push_back(indicator);
            LOG_INFO("🚨 SPA indicator found: " + indicator + " in URL: " + url);
        }
    }
    
    // Only return true if we have DEFINITIVE proof it's a SPA
    if (definitiveCount > 0) {
        LOG_INFO("🚨 SPA detected by " + std::to_string(definitiveCount) + " definitive indicators in URL: " + url);
        LOG_INFO("🚨 Found indicators: " + std::accumulate(foundIndicators.begin(), foundIndicators.end(), std::string(), 
            [](const std::string& a, const std::string& b) { return a + (a.empty() ? "" : ", ") + b; }));
        return true;
    }
    
    LOG_DEBUG("No SPA indicators found for URL: " + url);
    return false;
} 