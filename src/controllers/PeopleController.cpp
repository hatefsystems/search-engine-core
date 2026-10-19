#include "ProfileController.h"
#include "search_engine/profile/ProfileEditor.h"
#include <inja/inja.hpp>
void ProfileController::peoplePage(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    if(checkRateLimit(res,req))return;
    try {
        inja::Environment env("templates/"); auto html=env.render_file("people.inja",nlohmann::json::object());
        res->writeStatus("200 OK")->writeHeader("Content-Type","text/html; charset=utf-8")
            ->writeHeader("Cache-Control","no-cache, must-revalidate")->writeHeader("Server","HatefEngine 1.0")->end(html);
    }catch(...){serverError(res);}
}
void ProfileController::searchPeople(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    if(checkRateLimit(res,req))return;
    try {
        auto param=[&](const char* key){std::string value(req->getQuery(key));if(search_engine::profile::textLength(value)>200)throw std::invalid_argument("عبارت جست‌وجو طولانی است.");return value;};
        auto number=[&](const char* key,int fallback) {
            auto value=param(key);if(value.empty())return fallback;
            if(value.size()>5 || value.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("صفحه‌بندی نامعتبر است.");
            return std::stoi(value);
        };
        int page=number("page",1),limit=number("limit",20);
        if(page<1 || page>100 || limit<1 || limit>50)throw std::invalid_argument("صفحه‌بندی نامعتبر است.");
        auto result=getStorage()->searchPeople(param("q"),param("skill"),param("location"),param("availability"),page,limit);
        if(!result.success){serverError(res,result.message);return;}
        json(res,{{"success",true},{"data",result.value}});
    }catch(const std::invalid_argument& e){badRequest(res,e.what());}catch(...){serverError(res);}
}
