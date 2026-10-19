#pragma once
#include <uwebsockets/App.h>
#include <nlohmann/json.hpp>
#include <memory>
namespace search_engine::profile {
template<class Callback>
void readContentBody(uWS::HttpResponse<false>* res, Callback callback, size_t maximum = 65536) {
    struct State { std::string data; bool done = false; }; auto state = std::make_shared<State>();
    res->onAborted([state]{state->done=true;});
    res->onData([res,state,maximum,callback](std::string_view chunk,bool last) {
        if(state->done)return;
        auto fail=[&](const char* status){state->done=true;res->writeStatus(status)->writeHeader("Content-Type","application/json")
            ->writeHeader("Cache-Control","private, no-store")->writeHeader("Server","HatefEngine 1.0")
            ->end(R"({"success":false,"message":"اطلاعات ارسالی نامعتبر یا بیش از حد مجاز است."})");};
        if(state->data.size()+chunk.size()>maximum){fail("413 Payload Too Large");return;}
        state->data.append(chunk);if(!last)return;
        try {
            auto value=nlohmann::json::parse(state->data,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){if(depth>16)throw std::invalid_argument("depth");return true;});
            state->done=true; callback(value);
        } catch(...) {if(!state->done)fail("400 Bad Request");}
    });
}
}
