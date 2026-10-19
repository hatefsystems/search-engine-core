#include "PulseController.h"

#include "../../include/Logger.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

bool envFlagEnabled(const char* value) {
    if (!value) {
        return false;
    }
    std::string flag = value;
    std::transform(flag.begin(), flag.end(), flag.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    if (!value || std::string(value).empty()) {
        return fallback;
    }
    return std::string(value);
}

int positiveEnvOr(const char* name, int fallback, int minValue, int maxValue) {
    const char* value = std::getenv(name);
    if (!value) {
        return fallback;
    }
    try {
        int parsed = std::stoi(value);
        if (parsed < minValue || parsed > maxValue) {
            return fallback;
        }
        return parsed;
    } catch (...) {
        return fallback;
    }
}

} // namespace

void PulseController::pulsePage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    try {
        std::string url = std::string(req->getUrl());
        std::string language = url.rfind("/pulse", 0) == 0 ? "en" : "fa";
        nlohmann::json locale = loadLocale(language);

        nlohmann::json data = {
            {"t", locale},
            {"base_url", buildBaseUrl(req)},
            {"api_base", buildBaseUrl(req) + "/api/pulse"},
            {"language", language}
        };

        std::string content = renderTemplate("pulse.inja", data);
        if (content.empty()) {
            serverError(res, "Failed to render Pulse page");
            return;
        }

        html(res, content);
    } catch (const std::exception& e) {
        LOG_ERROR("Pulse page error: " + std::string(e.what()));
        serverError(res, "Failed to render Pulse page");
    }
}

void PulseController::getSummary(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    auto params = parseQuery(req);
    std::string range = getRange(params);
    auto storage = getStorage();
    if (!storage) {
        nlohmann::json response = {
            {"success", true},
            {"data", {
                {"range", range},
                {"enoughData", false},
                {"activityScore", 0},
                {"successScore", 0},
                {"zeroResultOpportunityScore", 0},
                {"speedScore", 0}
            }}
        };
        json(res, response);
        return;
    }

    auto result = storage->getSummary(range, getMinEvents());
    if (!result.success) {
        LOG_WARNING("Pulse summary unavailable: " + result.message);
        nlohmann::json response = {
            {"success", true},
            {"data", {
                {"range", range},
                {"enoughData", false},
                {"activityScore", 0},
                {"successScore", 0},
                {"zeroResultOpportunityScore", 0},
                {"speedScore", 0}
            }}
        };
        json(res, response);
        return;
    }

    const auto& summary = result.value;
    nlohmann::json response = {
        {"success", true},
        {"data", {
            {"range", range},
            {"enoughData", summary.enoughData},
            {"activityScore", summary.activityScore},
            {"successScore", summary.successScore},
            {"zeroResultOpportunityScore", summary.zeroResultOpportunityScore},
            {"speedScore", summary.speedScore}
        }}
    };
    json(res, response);
}

void PulseController::getTopQueries(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    auto params = parseQuery(req);
    int limit = getLimit(params);
    std::string range = getRange(params);
    auto storage = getStorage();
    if (!storage) {
        json(res, emptyListResponse(range));
        return;
    }

    auto result = storage->getTopQueries(limit, range, getMinEvents());
    if (!result.success) {
        LOG_WARNING("Pulse top queries unavailable: " + result.message);
        json(res, emptyListResponse(range));
        return;
    }

    nlohmann::json items = nlohmann::json::array();
    bool enoughData = false;
    int rank = 1;
    for (const auto& metric : result.value) {
        enoughData = enoughData || metric.enoughData;
        items.push_back(metricToJson(metric, rank++));
    }

    json(res, {{"success", true}, {"data", {{"window", range}, {"range", range}, {"enoughData", enoughData}, {"items", items}}}});
}

void PulseController::getRisingQueries(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    auto params = parseQuery(req);
    int limit = getLimit(params);
    std::string range = getRange(params);
    auto storage = getStorage();
    if (!storage) {
        json(res, emptyListResponse(range));
        return;
    }

    auto result = storage->getRisingQueries(limit, range, getMinEvents());
    if (!result.success) {
        LOG_WARNING("Pulse rising queries unavailable: " + result.message);
        json(res, emptyListResponse(range));
        return;
    }

    nlohmann::json items = nlohmann::json::array();
    bool enoughData = false;
    int rank = 1;
    for (const auto& metric : result.value) {
        enoughData = enoughData || metric.enoughData;
        items.push_back(metricToJson(metric, rank++));
    }

    json(res, {{"success", true}, {"data", {{"window", range}, {"range", range}, {"enoughData", enoughData}, {"items", items}}}});
}

void PulseController::getZeroResults(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    auto params = parseQuery(req);
    int limit = getLimit(params);
    std::string range = getRange(params);
    auto storage = getStorage();
    if (!storage) {
        json(res, emptyListResponse(range));
        return;
    }

    auto result = storage->getZeroResultQueries(limit, range, getMinEvents());
    if (!result.success) {
        LOG_WARNING("Pulse zero-result queries unavailable: " + result.message);
        json(res, emptyListResponse(range));
        return;
    }

    nlohmann::json items = nlohmann::json::array();
    bool enoughData = false;
    int rank = 1;
    for (const auto& metric : result.value) {
        enoughData = enoughData || metric.enoughData;
        items.push_back(metricToJson(metric, rank++));
    }

    json(res, {{"success", true}, {"data", {{"window", range}, {"range", range}, {"enoughData", enoughData}, {"items", items}}}});
}

void PulseController::getQuery(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    auto params = parseQuery(req);
    std::string range = getRange(params);
    auto qIt = params.find("q");
    if (qIt == params.end() || qIt->second.empty()) {
        badRequest(res, "Query parameter is required");
        return;
    }

    std::string query = urlDecode(qIt->second);
    auto storage = getStorage();
    if (!storage) {
        json(res, {{"success", true}, {"data", {{"query", query}, {"range", range}, {"enoughData", false}, {"points", nlohmann::json::array()}}}});
        return;
    }

    auto result = storage->getQueryTrend(query, range, getMinEvents());
    if (!result.success) {
        LOG_WARNING("Pulse query trend unavailable: " + result.message);
        json(res, {{"success", true}, {"data", {{"query", query}, {"range", range}, {"enoughData", false}, {"points", nlohmann::json::array()}}}});
        return;
    }

    nlohmann::json points = nlohmann::json::array();
    bool enoughData = false;
    for (const auto& point : result.value) {
        enoughData = enoughData || point.score > 0;
        points.push_back({
            {"timestamp", search_engine::storage::PulseAnalyticsStorage::toIsoString(point.bucketStart)},
            {"score", point.score}
        });
    }

    json(res, {{"success", true}, {"data", {{"query", query}, {"range", range}, {"enoughData", enoughData}, {"points", points}}}});
}

search_engine::storage::PulseAnalyticsStorage* PulseController::getStorage() const {
    if (!envFlagEnabled(std::getenv("PULSE_ANALYTICS_ENABLED"))) {
        return nullptr;
    }

    if (!storage_) {
        try {
            std::string uri = envOr("PULSE_MONGODB_URI", envOr("MONGODB_URI", "mongodb://localhost:27017"));
            std::string database = envOr("PULSE_MONGODB_DATABASE", "hatef_pulse");
            int retentionDays = positiveEnvOr("PULSE_RAW_RETENTION_DAYS", 14, 1, 90);
            storage_ = std::make_unique<search_engine::storage::PulseAnalyticsStorage>(uri, database, retentionDays);
        } catch (const std::exception& e) {
            LOG_WARNING("Pulse storage unavailable: " + std::string(e.what()));
            return nullptr;
        }
    }

    return storage_.get();
}

nlohmann::json PulseController::loadLocale(const std::string& language) const {
    std::string path = "locales/" + language + "/pulse.json";
    std::string content = loadFile(path);
    if (content.empty() && language != "fa") {
        content = loadFile("locales/fa/pulse.json");
    }

    if (!content.empty()) {
        try {
            return nlohmann::json::parse(content);
        } catch (const std::exception& e) {
            LOG_WARNING("Failed to parse Pulse locale: " + std::string(e.what()));
        }
    }

    return {
        {"language", {{"code", "fa"}, {"direction", "rtl"}}},
        {"meta", {{"title", "نبض هاتف"}, {"description", "نبض جست‌وجوی فارسی‌زبان‌ها"}}},
        {"copy", {{"title", "نبض هاتف"}, {"subtitle", "نبض جست‌وجوی فارسی‌زبان‌ها"}}}
    };
}

std::string PulseController::loadFile(const std::string& path) const {
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
        return "";
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string PulseController::renderTemplate(const std::string& templateName, const nlohmann::json& data) const {
    try {
        std::string templateDir = "/app/templates/";
        if (!std::filesystem::exists(templateDir)) {
            templateDir = "templates/";
        }
        inja::Environment env(templateDir);
        return env.render_file(templateName, data);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to render Pulse template: " + std::string(e.what()));
        return "";
    }
}

std::string PulseController::buildBaseUrl(uWS::HttpRequest* req) const {
    std::string host = std::string(req->getHeader("host"));
    if (host.empty()) {
        host = "localhost:3000";
    }

    std::string protocol = "http://";
    std::string forwardedProto = std::string(req->getHeader("x-forwarded-proto"));
    if (!forwardedProto.empty()) {
        protocol = forwardedProto + "://";
    }

    return protocol + host;
}

std::string PulseController::urlDecode(const std::string& encoded) const {
    std::string decoded;
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            std::string hex = encoded.substr(i + 1, 2);
            char ch = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            decoded.push_back(ch);
            i += 2;
        } else if (encoded[i] == '+') {
            decoded.push_back(' ');
        } else {
            decoded.push_back(encoded[i]);
        }
    }
    return decoded;
}

int PulseController::getLimit(const std::map<std::string, std::string>& params, int fallback) const {
    auto it = params.find("limit");
    if (it == params.end()) {
        return fallback;
    }
    try {
        int limit = std::stoi(it->second);
        if (limit < 1 || limit > 50) {
            return fallback;
        }
        return limit;
    } catch (...) {
        return fallback;
    }
}

std::string PulseController::getRange(const std::map<std::string, std::string>& params) const {
    auto it = params.find("range");
    if (it == params.end()) {
        return "today";
    }

    std::string range = it->second;
    std::transform(range.begin(), range.end(), range.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (range == "today" || range == "week" || range == "month" || range == "year" || range == "all") {
        return range;
    }
    return "today";
}

int PulseController::getMinEvents() const {
    return positiveEnvOr("PULSE_PUBLIC_MIN_EVENTS", 3, 1, 1000);
}

nlohmann::json PulseController::metricToJson(const search_engine::storage::PulseQueryMetric& metric, int rank) const {
    nlohmann::json item = {
        {"query", metric.query},
        {"score", metric.score},
        {"rank", rank},
        {"languageEstimate", metric.languageEstimate},
        {"enoughData", metric.enoughData}
    };
    if (metric.trendScore > 0) {
        item["trendScore"] = metric.trendScore;
    }
    return item;
}

nlohmann::json PulseController::emptyListResponse(const std::string& window) const {
    return {
        {"success", true},
        {"data", {
            {"window", window},
            {"range", window},
            {"enoughData", false},
            {"items", nlohmann::json::array()}
        }}
    };
}
