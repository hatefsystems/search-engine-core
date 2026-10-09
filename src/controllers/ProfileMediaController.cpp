#include "ProfileController.h"
#include "search_engine/profile/ContentHttp.h"
#include "search_engine/profile/ProfileEditor.h"
#include "search_engine/profile/ProfileProjection.h"
#include "search_engine/profile/ProfileJson.h"
#include "search_engine/profile/PublicProfile.h"
#include "search_engine/common/Base64.h"
#include "search_engine/common/ImageValidator.h"
#include <vips/vips.h>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <thread>
#include <semaphore>
#include <mutex>
using namespace search_engine;
namespace {
std::filesystem::path mediaRoot() {
    const char* value=std::getenv("PROFILE_MEDIA_DIR");return value?value:"profile-media";
}
bool hexId(const std::string& value) {return !value.empty() && value.size()<=64 && std::all_of(value.begin(),value.end(),[](unsigned char c){return (c>='0'&&c<='9') || (c>='a'&&c<='f');});}
std::string cleanImage(const std::vector<unsigned char>& bytes) {
    using Type=common::ImageValidator::ImageType;
    const auto info=common::ImageValidator::validate(bytes,5*1024*1024);
    if(!info.isValid || (info.type!=Type::JPEG && info.type!=Type::PNG && info.type!=Type::WEBP))throw std::invalid_argument("فقط تصویر JPEG، PNG یا WebP ثابت تا ۵ مگابایت مجاز است.");
    static std::once_flag initialized;std::call_once(initialized,[]{if(vips_init("hatef-profile-media"))throw std::runtime_error("image initialization");vips_cache_set_max(0);vips_concurrency_set(1);});
    // Inspect chunk boundaries, not arbitrary compressed bytes which may coincidentally
    // contain the four animation marker letters.
    auto word=[&](size_t p,bool little) {
        uint32_t value=0;for(int i=0;i<4;++i)value|=uint32_t(bytes[p+i]) << ((little?i:3-i)*8);return value;
    };
    if(info.type==Type::PNG || info.type==Type::WEBP) {
        const bool webp=info.type==Type::WEBP;
        size_t pos=webp?12:8;
        while(pos+(webp?8:12)<=bytes.size()) {
            const size_t label=pos+(webp?0:4),length=word(pos+(webp?4:0),webp);
            const std::string marker(reinterpret_cast<const char*>(bytes.data()+label),4);
            if(marker=="acTL" || marker=="ANIM" || marker=="ANMF")throw std::invalid_argument("تصویر متحرک مجاز نیست.");
            const size_t overhead=webp?8:12;
            if(length>bytes.size()-pos-overhead)break;
            pos+=length+overhead+(webp?(length%2):0);
        }
    }
    auto unref=[](VipsImage* p){if(p)g_object_unref(p);};
    std::unique_ptr<VipsImage,decltype(unref)> input(vips_image_new_from_buffer(bytes.data(),bytes.size(),"","access",VIPS_ACCESS_SEQUENTIAL,"fail_on",VIPS_FAIL_ON_ERROR,nullptr),unref);
    if(!input)throw std::invalid_argument("فایل تصویر قابل خواندن نیست.");
    const auto width=vips_image_get_width(input.get()),height=vips_image_get_height(input.get());
    if(width<=0 || height<=0 || width>8000 || height>8000 || int64_t(width)*height>16000000)throw std::invalid_argument("ابعاد تصویر بیش از حد مجاز است.");
    int pages=1;if(vips_image_get_typeof(input.get(),"n-pages"))vips_image_get_int(input.get(),"n-pages",&pages);
    if(pages!=1)throw std::invalid_argument("تصویر باید ثابت باشد.");
    VipsImage* rotated=nullptr;if(vips_autorot(input.get(),&rotated,nullptr))throw std::invalid_argument("تصویر نامعتبر است.");
    std::unique_ptr<VipsImage,decltype(unref)> oriented(rotated,unref);
    VipsImage* scaled=nullptr;const auto ratio=std::min(1.,1600./std::max(width,height));
    if(vips_resize(oriented.get(),&scaled,ratio,nullptr))throw std::invalid_argument("پردازش تصویر انجام نشد.");
    std::unique_ptr<VipsImage,decltype(unref)> output(scaled,unref);void* buffer=nullptr;size_t length=0;
    if(vips_webpsave_buffer(output.get(),&buffer,&length,"Q",85,"strip",TRUE,nullptr))throw std::invalid_argument("پردازش تصویر انجام نشد.");
    std::string result(reinterpret_cast<char*>(buffer),length);g_free(buffer);return result;
}
}
void ProfileController::profileMedia(uWS::HttpResponse<false>* res,uWS::HttpRequest* req) {
    const std::string id(req->getParameter(0)),target(req->getParameter(1)),token=getAuthToken(req);
    std::string section;
    for (const auto* name : {"projects", "experiences", "services", "skills", "achievements"})
        if (req->getUrl().find(std::string("/") + name + "/") != std::string_view::npos) section = name;
    try {
        if(id.size()!=24 || !hexId(id)){notFound(res);return;}
        auto result=getStorage()->findPersonById(id);
        if(!result.success || !result.value || result.value->deletedAt){notFound(res);return;}
        const bool owner=checkOwnership(*result.value,token);
        if(req->getMethod()=="get") {
            if(target.size()!=32 || !hexId(target) || (!owner && !result.value->isPublic)){notFound(res);return;}
            auto content=owner?profile::effectiveContent(*result.value):profile::publicPersonProfile(*result.value).content;
            bool visible=false;
            for(const auto& key:{"projects","experiences","services","skills","achievements"})for(const auto& item:content.sections[key]) {
                const auto& media=profile::itemMedia(item);
                if(std::any_of(media.begin(),media.end(),[&](const auto& image){return image.id==target;}))visible=true;
            }
            if(!visible){notFound(res);return;}
            std::ifstream file(mediaRoot()/id/(target+".webp"),std::ios::binary);
            if(!file){notFound(res);return;}
            std::string data((std::istreambuf_iterator<char>(file)),{});
            res->writeStatus("200 OK")->writeHeader("Content-Type","image/webp")->writeHeader("X-Content-Type-Options","nosniff")
                ->writeHeader("Cache-Control","private, no-store")->writeHeader("Server","HatefEngine 1.0")->end(data);return;
        }
        if(!owner){json(res,{{"success",false}},"403 Forbidden");return;}
        if(checkOwnerMutationRateLimit(res,id))return;
        profile::readContentBody(res,[this,res,id,target,section,token](const nlohmann::json& body){
            try {
                for(auto it=body.begin();it!=body.end();++it)if(it.key()!="version" && it.key()!="image" && it.key()!="alt")throw std::invalid_argument("فیلد معتبر نیست.");
                if(!body.at("version").is_number_integer())throw std::invalid_argument("نسخه لازم است.");
                const auto expected=body.at("version").get<int64_t>();const auto alt=body.value("alt","");
                if(profile::textLength(alt)>300)throw std::invalid_argument("توضیح تصویر طولانی است.");
                auto encoded=body.at("image").get<std::string>();if(encoded.starts_with("data:")){auto comma=encoded.find(',');if(comma==std::string::npos)throw std::invalid_argument("تصویر نامعتبر است.");encoded.erase(0,comma+1);}
                auto bytes=common::Base64::decode(encoded);
                static std::counting_semaphore<2> workers(2);
                if(!workers.try_acquire()){res->writeStatus("429 Too Many Requests")->writeHeader("Retry-After","2");json(res,{{"message","بارگذاری را دوباره امتحان کنید."}},"429 Too Many Requests");return;}
                auto permit=std::shared_ptr<void>(reinterpret_cast<void*>(1),[](void*){workers.release();});
                auto aborted=std::make_shared<std::atomic<bool>>(false);res->onAborted([aborted]{*aborted=true;});auto* loop=uWS::Loop::get();
                std::thread([this,res,id,target,section,token,expected,alt,bytes=std::move(bytes),aborted,loop,permit]() {
                    std::string output,error;try{output=cleanImage(bytes);}catch(const std::exception& e){error=e.what();}
                    loop->defer([this,res,id,target,section,token,expected,alt,aborted,output=std::move(output),error=std::move(error)]{
                        if(*aborted)return;
                        std::filesystem::path path;
                        try {
                            if(!error.empty()){badRequest(res,error);return;}
                            auto found=getStorage()->findPersonById(id);
                            if(!found.success || !found.value || found.value->deletedAt || !checkOwnership(*found.value,token)){json(res,{{"success",false}},"403 Forbidden");return;}
                            auto person=*found.value;if(person.version!=expected){json(res,{{"success",false}},"409 Conflict");return;}
                            profile::initializeContentSection(person,section);auto& items=person.content.sections[section];
                            auto item=std::find_if(items.begin(),items.end(),[&](const auto& value){return value.id==target;});
                            if(item==items.end()){notFound(res);return;}
                            auto& media=profile::itemMedia(*item);
                            if(media.size()>=10)throw std::invalid_argument("حداکثر ده تصویر برای هر آیتم مجاز است.");
                            const auto mediaId=profile::newOwnerKey().substr(0,32);path=mediaRoot()/id/(mediaId+".webp");std::filesystem::create_directories(path.parent_path());
                            {std::ofstream file(path,std::ios::binary);file.write(output.data(),output.size());if(!file)throw std::runtime_error("image write");}
                            media.push_back({mediaId,alt});item->updatedAt=profile::contentNow();*item=profile::parseContentItem(section,profile::itemJson(*item));profile::validateContent(profile::effectiveContent(person));
                            auto saved=getStorage()->updatePersonFields(person,{"content"},expected);
                            if(!saved.success){std::filesystem::remove(path);json(res,{{"success",false}},saved.message=="VERSION_CONFLICT"?"409 Conflict":"500 Internal Server Error");return;}
                            person.version++;json(res,{{"success",true},{"data",personProfileToJson(person)},{"canEdit",true}});
                        }catch(const std::exception&){if(!path.empty()){std::error_code ec;std::filesystem::remove(path,ec);}badRequest(res,"ذخیرهٔ تصویر انجام نشد.");}
                    });
                }).detach();
            }catch(const std::exception& e){badRequest(res,e.what());}
        },8*1024*1024);
    }catch(...){serverError(res);}
}
