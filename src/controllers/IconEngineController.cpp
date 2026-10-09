#include "IconEngineController.h"
#include <curl/curl.h>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <regex>
#include <string>
#include <thread>

namespace {
std::atomic<unsigned> active{0};
struct RequestState { bool ended = false; std::string body; };
void error(uWS::HttpResponse<false>* res, const char* status) {
    res->writeStatus(status)->writeHeader("Content-Type", "application/json")
       ->writeHeader("X-Content-Type-Options", "nosniff")
       ->end(R"({"error":"icon_service_unavailable","suggestions":[]})");
}
std::string clientKey(uWS::HttpResponse<false>* res) {
    constexpr char hex[]="0123456789abcdef"; std::string result;
    for (unsigned char c : res->getRemoteAddress()) { result += hex[c >> 4]; result += hex[c & 15]; }
    return result;
}
size_t collect(char* data, size_t size, size_t count, void* target) {
    auto& out=*static_cast<std::string*>(target);
    if (count > 2097152 || size > 2097152 || size * count > 2097152 - out.size()) return 0;
    out.append(data,size*count); return size*count;
}
void forward(uWS::HttpResponse<false>* res, std::shared_ptr<RequestState> state,
             std::string path, std::string client, bool post, bool svg) {
    const char* configured=std::getenv("ICON_ENGINE_URL");
    const std::string base=configured?configured:"";
    static const std::regex origin(R"(^http://[a-zA-Z0-9.-]+:[0-9]{1,5}$)");
    if (!std::regex_match(base,origin)) {state->ended=true;error(res,"503 Service Unavailable");return;}
    if (active.fetch_add(1) >= 8) {--active;state->ended=true;error(res,"429 Too Many Requests");return;}
    auto* loop=uWS::Loop::get();
    try {
        std::thread([res,state,loop,url=base+path,client=std::move(client),post,svg] {
            std::string output; long status=503;
            CURL* curl=curl_easy_init();
            if (curl) {
                struct curl_slist* headers=nullptr;
                headers=curl_slist_append(headers,("X-Icon-Client: "+client).c_str());
                headers=curl_slist_append(headers,"Content-Type: application/json");
                curl_easy_setopt(curl,CURLOPT_URL,url.c_str());curl_easy_setopt(curl,CURLOPT_PROXY,"");
                curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,200L);
                curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,2500L);curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
                curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,collect);
                curl_easy_setopt(curl,CURLOPT_WRITEDATA,&output);
                if (post) {curl_easy_setopt(curl,CURLOPT_POST,1L);curl_easy_setopt(curl,CURLOPT_POSTFIELDS,state->body.c_str());curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,static_cast<long>(state->body.size()));}
                if (curl_easy_perform(curl)==CURLE_OK) curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);
                curl_slist_free_all(headers);curl_easy_cleanup(curl);
            }
            loop->defer([res,state,status,output=std::move(output),svg] {
                --active;
                if (state->ended) return;
                state->ended=true;
                const char* code = status==200?"200 OK":status==400?"400 Bad Request":status==404?"404 Not Found":status==422?"422 Unprocessable Entity":status==429?"429 Too Many Requests":nullptr;
                if (!code) {error(res,"503 Service Unavailable");return;}
                res->cork([&] {
                    res->writeStatus(code)->writeHeader("Content-Type",svg && status==200?"image/svg+xml":"application/json")
                       ->writeHeader("X-Content-Type-Options","nosniff")
                       ->writeHeader("Content-Security-Policy","default-src 'none'; sandbox");
                    if (svg && status==200) res->writeHeader("Cache-Control","public, max-age=3600");
                    if (status==429) res->writeHeader("Retry-After","1");
                    res->end(output);
                });
            });
        }).detach();
    } catch (...) {--active;state->ended=true;error(res,"503 Service Unavailable");}
}
}
void IconEngineController::get(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    const std::string path(req->getUrl()), query(req->getQuery());
    static const std::regex route(R"(^/api/icons(?:/[a-zA-Z0-9:_-]+)?$)");
    if (path.size()>240 || !std::regex_match(path,route) || query.size()>2048 || query.find_first_of("\r\n#")!=std::string::npos) {error(res,"400 Bad Request");return;}
    auto state=std::make_shared<RequestState>();res->onAborted([state]{state->ended=true;});
    forward(res,state,"/v1"+path.substr(4)+(query.empty()?"":"?"+query),clientKey(res),false,false);
}
void IconEngineController::suggest(uWS::HttpResponse<false>* res,uWS::HttpRequest*) {
    auto state=std::make_shared<RequestState>();const auto client=clientKey(res);
    res->onAborted([state]{state->ended=true;});
    res->onData([res,state,client](std::string_view data,bool last) {
        if (state->ended) return;
        if (state->body.size()+data.size()>16384) {state->ended=true;error(res,"413 Payload Too Large");return;}
        state->body.append(data);
        if (last) forward(res,state,"/v1/icons/suggest",client,true,false);
    });
}
void IconEngineController::asset(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    const std::string path(req->getUrl());
    static const std::regex route(R"(^/assets/icons/(lucide/[a-z0-9][a-z0-9-]*|simple-icons/[a-z0-9][a-z0-9_-]*|iconify/[a-z0-9][a-z0-9-]*/[a-z0-9][a-z0-9-]*)\.svg$)");
    if(path.size()>240 || !std::regex_match(path,route)) {error(res,"400 Bad Request");return;}
    auto state=std::make_shared<RequestState>();res->onAborted([state]{state->ended=true;});
    forward(res,state,path,clientKey(res),false,true);
}
