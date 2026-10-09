#include "ProfileController.h"
#include "search_engine/profile/ContentHttp.h"
#include "search_engine/profile/ProfileProjection.h"
#include "search_engine/profile/ProfileJson.h"
#include "search_engine/profile/PublicProfile.h"
#include "search_engine/profile/ProfileEditor.h"
#include <algorithm>
#include <set>
#include <filesystem>
using namespace search_engine;
using Json=nlohmann::json;
namespace {
void allowed(const Json& body,std::initializer_list<const char*> keys) {
    if(!body.is_object())throw std::invalid_argument("اطلاعات نامعتبر است.");
    for(auto it=body.begin();it!=body.end();++it)if(std::find(keys.begin(),keys.end(),it.key())==keys.end())throw std::invalid_argument("فیلد قابل ویرایش نیست: "+it.key());
}
int64_t version(const Json& body) {
    if(!body.contains("version") || !body["version"].is_number_integer() || body["version"].get<int64_t>()<0)throw std::invalid_argument("نسخهٔ اطلاعات لازم است.");
    return body["version"].get<int64_t>();
}
}
bool ProfileController::checkOwnerMutationRateLimit(uWS::HttpResponse<false>* res,const std::string& id) {
    static ApiRateLimiter limit(120,std::chrono::seconds(60));
    if(!limit.shouldThrottle(id))return false;
    res->writeStatus("429 Too Many Requests")->writeHeader("Retry-After",std::to_string(limit.getRetryAfter(id)));
    json(res,{{"success",false},{"message","کمی صبر کنید؛ ذخیره دوباره انجام می‌شود."}},"429 Too Many Requests");return true;
}
void ProfileController::profileContentSchema(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    if(checkRateLimit(res,req))return;
    json(res,{{"success",true},{"data",profile::contentEditorSchema()}});
}
void ProfileController::profileCompletion(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    try {
        auto result=getStorage()->findPersonById(std::string(req->getParameter(0)));
        if(!result.success || !result.value){notFound(res);return;}
        if(!checkOwnership(*result.value,getAuthToken(req))){json(res,{{"success",false}},"403 Forbidden");return;}
        json(res,{{"success",true},{"data",profile::profileCompletion(*result.value)}});
    }catch(...){serverError(res);}
}
void ProfileController::profileContent(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    const std::string id(req->getParameter(0)),section(req->getParameter(1)),method(req->getMethod()),token=getAuthToken(req);
    const std::string itemId(req->getParameter(2));
    try {
        profile::sectionDefinition(section);
        auto result=getStorage()->findPersonById(id);
        if(!result.success || !result.value || result.value->deletedAt){notFound(res);return;}
        const bool owner=checkOwnership(*result.value,token);
        if(method=="get") {
            if(checkRateLimit(res,req))return;
            if(!owner && !result.value->isPublic){notFound(res);return;}
            auto content=owner?profile::effectiveContent(*result.value):profile::publicPersonProfile(*result.value).content;
            const auto& items=content.sections[section];
            size_t offset=0,limit=20;
            if(!req->getQuery("offset").empty())offset=std::stoul(std::string(req->getQuery("offset")));
            if(!req->getQuery("limit").empty())limit=std::stoul(std::string(req->getQuery("limit")));
            limit=std::clamp(limit,size_t(1),size_t(100));Json list=Json::array();
            for(size_t i=std::min(offset,items.size());i<items.size() && list.size()<limit;++i)list.push_back(profile::itemJson(items[i]));
            json(res,{{"success",true},{"data",{{"items",list},{"total",items.size()},{"version",result.value->version}}}});return;
        }
        if(!owner){json(res,{{"success",false},{"message","دسترسی ویرایش لازم است."}},"403 Forbidden");return;}
        if(checkOwnerMutationRateLimit(res,id))return;
        profile::readContentBody(res,[this,res,id,section,itemId,method,token](const Json& body) {
            try {
                auto result=getStorage()->findPersonById(id);
                if(!result.success || !result.value || !checkOwnership(*result.value,token)){json(res,{{"success",false}},"403 Forbidden");return;}
                auto person=*result.value;const auto before=profile::effectiveContent(person);const auto expected=version(body);
                // Creation IDs are stable across retries. A replay never re-applies old values.
                profile::initializeContentSection(person,section);auto& items=person.content.sections[section];
                const bool reorder=itemId=="order";
                std::string target=itemId;
                if(method=="post")target=body.at("item").value("id","");
                auto found=std::find_if(items.begin(),items.end(),[&](const auto& item){return item.id==target;});
                if(method=="post" && found!=items.end()) {
                    allowed(body,{"version","item"});auto requested=body.at("item");
                    auto existing=profile::itemJson(*found);bool same=true;
                    for(auto it=requested.begin();it!=requested.end();++it)if(!existing.contains(it.key()) || existing[it.key()]!=it.value())same=false;
                    if(!same){json(res,{{"success",false},{"message","این شناسه قبلاً استفاده شده است."}},"409 Conflict");return;}
                    json(res,{{"success",true},{"data",personProfileToJson(person)},{"canEdit",true}});return;
                }
                if(person.version!=expected){json(res,{{"success",false},{"message","اطلاعات در جای دیگری تغییر کرده است."}},"409 Conflict");return;}
                if(reorder) {
                    allowed(body,{"version","ids"});const auto ids=body.at("ids").get<std::vector<std::string>>();
                    std::set<std::string> unique(ids.begin(),ids.end());
                    if(ids.size()!=items.size() || unique.size()!=ids.size())throw std::invalid_argument("ترتیب کامل و بدون تکرار لازم است.");
                    for(size_t i=0;i<ids.size();++i){auto item=std::find_if(items.begin(),items.end(),[&](const auto& v){return v.id==ids[i];});if(item==items.end())throw std::invalid_argument("آیتم متعلق به این بخش نیست.");item->displayOrder=int(i);}
                    std::sort(items.begin(),items.end(),[](const auto& a,const auto& b){return a.displayOrder<b.displayOrder;});
                } else if(method=="delete") {
                    allowed(body,{"version"});if(found==items.end()){notFound(res);return;}
                    items.erase(found);for(size_t i=0;i<items.size();++i)items[i].displayOrder=int(i);profile::prunePersonReferences(person);
                } else {
                    allowed(body,{"version","item"});auto input=body.at("item");
                    if(!input.is_object())throw std::invalid_argument("اطلاعات آیتم نامعتبر است.");
                    for(auto field:{"createdAt","updatedAt","displayOrder"})if(input.contains(field))throw std::invalid_argument("این فیلد توسط سرور تنظیم می‌شود.");
                    if(method!="post" && found==items.end()){notFound(res);return;}
                    Json data=found==items.end()?Json::object():profile::itemJson(*found);
                    if(input.contains("id") && input["id"]!=target)throw std::invalid_argument("شناسه قابل تغییر نیست.");
                    if((section=="projects" || section=="experiences" || section=="services" || section=="skills" || section=="achievements") && input.contains("media")) {
                        std::set<std::string> owned;
                        if(data.contains("media"))for(const auto& m:data["media"])owned.insert(m.at("id").get<std::string>());
                        for(const auto& m:input["media"])if(!owned.count(m.at("id").get<std::string>()))throw std::invalid_argument("تصویر را از بخش بارگذاری اضافه کنید.");
                    }
                    data.update(input);data["id"]=target;data["updatedAt"]=profile::contentNow();
                    if(found==items.end()){data["createdAt"]=profile::contentNow();data["displayOrder"]=items.size();}
                    auto item=profile::parseContentItem(section,data);
                    if(found==items.end())items.push_back(item);else *found=item;
                }
                profile::validateContent(profile::effectiveContent(person));
                auto saved=getStorage()->updatePersonFields(person,{"content"},expected);
                if(!saved.success){json(res,{{"success",false},{"message","ذخیره انجام نشد؛ نسخه را بررسی کنید."}},saved.message=="VERSION_CONFLICT"?"409 Conflict":"500 Internal Server Error");return;}
                if (section == "projects" || section == "experiences" || section == "services" || section == "skills" || section == "achievements") {
                    std::set<std::string> retained;
                    for (const auto& entry : person.content.sections[section]) {const auto data=profile::itemJson(entry);for (const auto& media : data["media"]) retained.insert(media.at("id").get<std::string>());}
                    if (before.sections.count(section)) for (const auto& entry : before.sections.at(section)) {
                        const auto& images=profile::itemMedia(entry);
                        for (const auto& media : images) if (!retained.count(media.id)) {
                        const char* root = std::getenv("PROFILE_MEDIA_DIR"); std::error_code ec;
                        std::filesystem::remove(std::filesystem::path(root ? root : "profile-media") / id / (media.id + ".webp"), ec);
                        }
                    }
                }
                person.version=expected+1;
                json(res,{{"success",true},{"data",personProfileToJson(person)},{"canEdit",true}});
            }catch(const std::exception& e){badRequest(res,e.what());}
        });
    }catch(const std::invalid_argument& e){badRequest(res,e.what());}catch(...){serverError(res);}
}
void ProfileController::profileLayout(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    const std::string id(req->getParameter(0)),token=getAuthToken(req);
    try {
        auto result=getStorage()->findPersonById(id);
        if(!result.success || !result.value){notFound(res);return;}
        if(!checkOwnership(*result.value,token)){json(res,{{"success",false}},"403 Forbidden");return;}
        if(req->getMethod()=="get"){auto data=profile::contentJson(profile::effectiveContent(*result.value));data.erase("sections");json(res,{{"success",true},{"data",data}});return;}
        if(checkOwnerMutationRateLimit(res,id))return;
        profile::readContentBody(res,[this,res,id,token](const Json& body){
            try {
                allowed(body,{"version","order","visibility","featured","goal","privacy"});
                auto result=getStorage()->findPersonById(id);
                if(!result.success || !result.value || !checkOwnership(*result.value,token)){json(res,{{"success",false}},"403 Forbidden");return;}
                auto person=*result.value;const auto expected=version(body);
                if(person.version!=expected){json(res,{{"success",false}},"409 Conflict");return;}
                auto data=profile::contentJson(person.content);
                for(auto key:{"order","visibility","featured","goal"})if(body.contains(key))data[key]=body[key];
                person.content=profile::parseContent(data);profile::validateContent(profile::effectiveContent(person));
                std::vector<std::string> fields={"content"};
                if(body.contains("privacy")) {
                    allowed(body["privacy"],{"showEmail","showPhone","showLocation","showAvailability"});
                    for(auto it=body["privacy"].begin();it!=body["privacy"].end();++it)if(!it.value().is_boolean())throw std::invalid_argument("حریم خصوصی معتبر نیست.");
                    auto& p=person.privacy;const auto& b=body["privacy"];
                    p.showEmail=b.value("showEmail",p.showEmail);p.showPhone=b.value("showPhone",p.showPhone);p.showLocation=b.value("showLocation",p.showLocation);p.showAvailability=b.value("showAvailability",p.showAvailability);fields.push_back("privacy");
                }
                auto saved=getStorage()->updatePersonFields(person,fields,expected);
                if(!saved.success){json(res,{{"success",false}},saved.message=="VERSION_CONFLICT"?"409 Conflict":"500 Internal Server Error");return;}
                person.version++;json(res,{{"success",true},{"data",personProfileToJson(person)},{"canEdit",true}});
            }catch(const std::exception& e){badRequest(res,e.what());}
        });
    }catch(...){serverError(res);}
}
