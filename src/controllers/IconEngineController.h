#pragma once
#include "../../include/routing/Controller.h"

class IconEngineController : public routing::Controller {
public:
    void get(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void suggest(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    static void asset(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
};
