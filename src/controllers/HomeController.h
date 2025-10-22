#pragma once
#include "../../include/routing/Controller.h"
#include "../../include/inja/inja.hpp"
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

class HomeController : public routing::Controller {
public:
    // GET /
    void index(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // GET /test
    void searchPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // POST /api/v2/email-subscribe
    void emailSubscribe(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // GET /crawl-request
    void crawlRequestPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // GET /crawl-request/{lang}
    void crawlRequestPageWithLang(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

    // GET /sponsor
    void sponsorPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // GET /sponsor/{lang}
    void sponsorPageWithLang(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // POST /api/v2/sponsor-submit
    void sponsorSubmit(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void getSponsorPaymentAccounts(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // GET /crawling-notification
    void crawlingNotificationPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // GET /crawling-notification/{lang}
    void crawlingNotificationPageWithLang(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    
    // GET /about
    void aboutPage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

private:
    std::string getAvailableLocales();
    std::string getDefaultLocale();
    std::string loadFile(const std::string& path);
    std::string renderTemplate(const std::string& templateName, const nlohmann::json& data);
    
    // Private overloaded method for crawling notification with explicit language
    void crawlingNotificationPageWithLang(uWS::HttpResponse<false>* res, uWS::HttpRequest* req, const std::string& lang);
};

// Route registration using macros (similar to .NET Core attributes)
ROUTE_CONTROLLER(HomeController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::GET, "/", index, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/test", searchPage, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/crawl-request", crawlRequestPage, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/crawl-request.html", crawlRequestPage, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/crawl-request/*", crawlRequestPageWithLang, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/sponsor", sponsorPage, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/sponsor.html", sponsorPage, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/sponsor/*", sponsorPageWithLang, HomeController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/v2/email-subscribe", emailSubscribe, HomeController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/v2/sponsor-submit", sponsorSubmit, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/v2/sponsor-payment-accounts", getSponsorPaymentAccounts, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/crawling-notification", crawlingNotificationPage, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/crawling-notification.html", crawlingNotificationPage, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/crawling-notification/*", crawlingNotificationPageWithLang, HomeController);
    REGISTER_ROUTE(HttpMethod::GET, "/about", aboutPage, HomeController);
} 