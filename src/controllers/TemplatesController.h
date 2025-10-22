#pragma once
#include "../../include/routing/Controller.h"
#include <nlohmann/json.hpp>

class TemplatesController : public routing::Controller {
public:
    TemplatesController() = default;

    // GET /api/templates - List available templates
    void listTemplates(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

    // POST /api/templates - Create a new template
    void createTemplate(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

    // POST /api/crawl/add-site-with-template - Use a template to crawl a site
    void addSiteWithTemplate(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

    // GET /api/templates/:name - Fetch single template
    void getTemplateByName(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

    // DELETE /api/templates/:name - Delete a template
    void deleteTemplateByName(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
};

// Route registration
ROUTE_CONTROLLER(TemplatesController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::GET, "/api/templates", listTemplates, TemplatesController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/templates/*", getTemplateByName, TemplatesController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/templates", createTemplate, TemplatesController);
    REGISTER_ROUTE(HttpMethod::DELETE, "/api/templates/*", deleteTemplateByName, TemplatesController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/crawl/add-site-with-template", addSiteWithTemplate, TemplatesController);
}


