#pragma once

#include "../../include/routing/Controller.h"
#include "../../include/search_engine/storage/PulseAnalyticsStorage.h"
#include "../../include/inja/inja.hpp"
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

class PulseController : public routing::Controller {
public:
    void pulsePage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getSummary(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getTopQueries(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getRisingQueries(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getZeroResults(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getQuery(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

private:
    search_engine::storage::PulseAnalyticsStorage* getStorage() const;
    nlohmann::json loadLocale(const std::string& language) const;
    std::string loadFile(const std::string& path) const;
    std::string renderTemplate(const std::string& templateName, const nlohmann::json& data) const;
    std::string buildBaseUrl(uWS::HttpRequest* req) const;
    std::string urlDecode(const std::string& encoded) const;
    int getLimit(const std::map<std::string, std::string>& params, int fallback = 10) const;
    std::string getRange(const std::map<std::string, std::string>& params) const;
    int getMinEvents() const;
    nlohmann::json metricToJson(const search_engine::storage::PulseQueryMetric& metric, int rank) const;
    nlohmann::json emptyListResponse(const std::string& window) const;

    mutable std::unique_ptr<search_engine::storage::PulseAnalyticsStorage> storage_;
};

ROUTE_CONTROLLER(PulseController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::GET, "/نبض", pulsePage, PulseController);
    REGISTER_ROUTE(HttpMethod::GET, "/%D9%86%D8%A8%D8%B6", pulsePage, PulseController);
    REGISTER_ROUTE(HttpMethod::GET, "/pulse", pulsePage, PulseController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/pulse/summary", getSummary, PulseController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/pulse/top-queries", getTopQueries, PulseController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/pulse/rising", getRisingQueries, PulseController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/pulse/zero-results", getZeroResults, PulseController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/pulse/query", getQuery, PulseController);
}
