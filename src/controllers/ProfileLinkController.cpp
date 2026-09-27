#include "ProfileController.h"
#include "search_engine/profile/ContentHttp.h"
#include "search_engine/profile/ProfileContent.h"
#include <set>
using Json=nlohmann::json;
using namespace search_engine::storage;
void ProfileController::createLink(uWS::HttpResponse<false>* r,uWS::HttpRequest* q){writeLink(r,q);}
void ProfileController::updateLink(uWS::HttpResponse<false>* r,uWS::HttpRequest* q){writeLink(r,q);}
void ProfileController::deleteLink(uWS::HttpResponse<false>* r,uWS::HttpRequest* q){writeLink(r,q);}
void ProfileController::writeLink(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    const std::string id(req->getParameter(0)),linkId(req->getParameter(1)),method(req->getMethod()),token=getAuthToken(req);
    try {
        auto owner=getStorage()->findById(id);
        if(!owner.success||!checkOwnership(owner.value,token)){json(res,{{"success",false}},"403 Forbidden");return;}
        if(checkOwnerMutationRateLimit(res,id))return;
        search_engine::profile::readContentBody(res,[this,res,id,linkId,method,token](const Json& body){
            try {
                auto owner=getStorage()->findById(id);
                if(!owner.success||!checkOwnership(owner.value,token)){json(res,{{"success",false}},"403 Forbidden");return;}
                const std::set<std::string> keys={"id","version","url","title","description","iconUrl","isActive","privacy","visibility","tags","sortOrder"};
                for(auto it=body.begin();it!=body.end();++it)if(!keys.count(it.key()))throw std::invalid_argument("فیلد لینک قابل ویرایش نیست.");
                LinkBlock link;link.profileId=id;
                if(method!="post") {
                    auto existing=getLinkBlockStorage()->findById(linkId);
                    if(!existing.success||!existing.value||existing.value->profileId!=id){notFound(res);return;}
                    if(body.contains("id") && body["id"]!=linkId)throw std::invalid_argument("شناسه قابل تغییر نیست.");
                    link=*existing.value;
                    if(!body.contains("version")||!body["version"].is_number_integer())throw std::invalid_argument("نسخهٔ لینک لازم است.");
                    if(body["version"].get<int64_t>()!=link.version){json(res,{{"success",false},{"message","لینک در جای دیگری تغییر کرده است."}},"409 Conflict");return;}
                    if(method=="delete") {
                        auto result=getLinkBlockStorage()->deleteLink(linkId,link.version);
                        json(res,{{"success",result.success},{"message",result.success?"لینک حذف شد.":"نسخه تغییر کرده است."}},result.success?"200 OK":"409 Conflict");return;
                    }
                } else if(body.contains("id")) {
                    auto stable=body.at("id").get<std::string>();
                    if(stable.size()!=24||stable.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::invalid_argument("شناسهٔ لینک معتبر نیست.");
                    auto existing=getLinkBlockStorage()->findById(stable);
                    if(existing.success&&existing.value) {
                        if(existing.value->profileId!=id){json(res,{{"success",false}},"409 Conflict");return;}
                        auto stored=linkToJson(*existing.value);for(auto it=body.begin();it!=body.end();++it)if(it.key()!="version"&&(!stored.contains(it.key())||stored[it.key()]!=it.value())){json(res,{{"success",false}},"409 Conflict");return;}
                        json(res,{{"success",true},{"data",stored}});return;
                    }
                    link.id=stable;
                }
                if(body.contains("url"))link.url=body.at("url").get<std::string>();
                if(body.contains("title"))link.title=body.at("title").get<std::string>();
                if(body.contains("description"))link.description=body.at("description").is_null()?"":body.at("description").get<std::string>();
                if(body.contains("iconUrl"))link.iconUrl=body.at("iconUrl").is_null()?"":body.at("iconUrl").get<std::string>();
                if(body.contains("isActive"))link.isActive=body.at("isActive").get<bool>();
                if(body.contains("privacy"))link.privacy=stringToLinkPrivacy(body.at("privacy").get<std::string>());
                if(body.contains("visibility"))link.visibility=body.at("visibility").get<std::string>();
                if(body.contains("tags"))link.tags=body.at("tags").get<std::vector<std::string>>();
                if(body.contains("sortOrder"))link.sortOrder=body.at("sortOrder").get<int>();
                if((link.visibility!="PUBLIC"&&link.visibility!="HIDDEN")||!link.isValid()||!(search_engine::profile::safeContentUrl(link.url)||(link.visibility=="HIDDEN"&&search_engine::profile::validDraftUrl(link.url)))||link.tags.size()>50||link.sortOrder<0||link.sortOrder>9999)throw std::invalid_argument("اطلاعات لینک معتبر نیست.");
                if(link.iconUrl&&!link.iconUrl->empty()&&!search_engine::profile::safeContentUrl(*link.iconUrl))throw std::invalid_argument("آدرس تصویر معتبر نیست.");
                if(method=="post") {
                    if(getLinkBlockStorage()->countByProfile(id).value>=50)throw std::invalid_argument("حداکثر پنجاه لینک مجاز است.");
                    auto result=getLinkBlockStorage()->store(link);if(!result.success){serverError(res);return;}link.id=result.value;
                }else {
                    auto result=getLinkBlockStorage()->update(link);if(!result.success){json(res,{{"success",false}},result.message=="VERSION_CONFLICT"?"409 Conflict":"500 Internal Server Error");return;}link.version++;
                }
                json(res,{{"success",true},{"data",linkToJson(link)},{"message","لینک ذخیره شد."}});
            }catch(const std::exception& e){badRequest(res,e.what());}
        });
    }catch(...){serverError(res);}
}
void ProfileController::getLinks(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    try {
        const std::string id(req->getParameter(0));auto p=getStorage()->findById(id);
        if(!p.success||p.value.deletedAt){notFound(res);return;}
        const bool owner=checkOwnership(p.value,getAuthToken(req));if(!owner&&!p.value.isPublic){notFound(res);return;}
        auto links=getLinkBlockStorage()->findByProfile(id);if(!links.success){serverError(res);return;}
        Json list=Json::array();for(const auto& link:links.value)if(owner||(link.visibility=="PUBLIC"&&link.isActive&&link.privacy==LinkPrivacy::PUBLIC))list.push_back(linkToJson(link));
        json(res,{{"success",true},{"data",list}});
    }catch(...){serverError(res);}
}
void ProfileController::getLinkById(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    try {
        const std::string id(req->getParameter(0)),linkId(req->getParameter(1));auto p=getStorage()->findById(id);auto l=getLinkBlockStorage()->findById(linkId);
        if(!p.success||p.value.deletedAt||!l.success||!l.value||l.value->profileId!=id){notFound(res);return;}
        const auto& link=*l.value;
        if(!checkOwnership(p.value,getAuthToken(req))&&(!p.value.isPublic||link.visibility!="PUBLIC"||!link.isActive||link.privacy!=LinkPrivacy::PUBLIC)){notFound(res);return;}
        json(res,{{"success",true},{"data",linkToJson(link)}});
    }catch(...){serverError(res);}
}
