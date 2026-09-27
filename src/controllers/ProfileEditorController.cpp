#include "ProfileController.h"
#include "../../include/search_engine/profile/ProfileEditor.h"
#include "../../include/search_engine/profile/ProfileJson.h"
#include "../../include/search_engine/profile/PublicProfile.h"
#include "../../include/search_engine/storage/ProfileValidator.h"
#include "../../include/search_engine/common/SlugGenerator.h"
#include <inja/inja.hpp>
#include <set>
#include "../../include/search_engine/profile/ProfileProjection.h"

namespace {
using Json = nlohmann::json;
using namespace search_engine;
template<class Callback>
void readEditorBody(uWS::HttpResponse<false>* res, Callback callback) {
    struct Body { std::string data; bool finished = false; };
    auto state = std::make_shared<Body>();
    res->onAborted([state] { state->finished = true; });
    res->onData([res, state, callback](std::string_view chunk, bool last) {
        if (state->finished) return;
        if (state->data.size() + chunk.size() > 65536) {
            state->finished = true;
            res->writeStatus("413 Payload Too Large")->writeHeader("Server", "HatefEngine 1.0")->end(); return;
        }
        state->data.append(chunk);
        if (!last) return;
        state->finished = true;
        try { callback(Json::parse(state->data)); }
        catch (const std::exception&) {
            res->writeStatus("400 Bad Request")->writeHeader("Content-Type", "application/json")
                ->writeHeader("Server", "HatefEngine 1.0")->end(R"({"success":false,"message":"اطلاعات ارسالی نامعتبر است."})");
        }
    });
}
}

void ProfileController::renderProfileEntry(uWS::HttpResponse<false>* res, const std::string& slug,
    const std::string& state, const std::string& id) {
    const auto encoded = common::encodeProfileSlug(slug);
    Json data = {{"slug", slug}, {"encodedSlug", encoded}, {"state", state}, {"profileId", id},
        {"editUrl", "/profiles/" + encoded + "/edit"}, {"createUrl", "/profiles/new?slug=" + encoded}};
    inja::Environment env("templates/");
    auto html = env.render_file(state == "editor" || state == "new" ? "profile_editor.inja" : "profile_entry.inja",
        profile::escapeTemplateStrings(data));
    const auto status = state == "missing" || state == "unavailable" ? "404 Not Found" : state == "private" ? "403 Forbidden" : "200 OK";
    res->writeStatus(status)->writeHeader("Content-Type", "text/html; charset=utf-8")
        ->writeHeader("Cache-Control", "private, no-store")->writeHeader("Vary", "Accept")
        ->writeHeader("X-Robots-Tag", "noindex, nofollow")->writeHeader("Server", "HatefEngine 1.0")->end(html);
}

void ProfileController::newProfilePage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    if (checkRateLimit(res, req)) return;
    // getQuery(key) already decodes; read the raw value to reject double encoding.
    auto query = req->getQuery();
    std::string_view rawSlug;
    while (!query.empty()) {
        const auto end = query.find('&');
        const auto part = query.substr(0, end);
        if (part.substr(0, 5) == "slug=") { rawSlug = part.substr(5); break; }
        if (end == std::string_view::npos) break;
        query.remove_prefix(end + 1);
    }
    const auto decoded = common::decodeProfileSlug(rawSlug, true);
    const auto slug = decoded ? common::canonicalProfileSlug(*decoded) : std::nullopt;
    if (!slug || common::SlugGenerator::isReservedSlug(*slug)) { badRequest(res, "آدرس معتبر نیست."); return; }
    try {
        auto available = getStorage()->checkSlugAvailability(*slug);
        if (!available.success) { serverError(res, "امکان بررسی آدرس وجود ندارد."); return; }
        if (!available.value) {
            res->writeStatus("303 See Other")->writeHeader("Location", "/profiles/" + common::encodeProfileSlug(*slug) + "/edit")->writeHeader("Server", "HatefEngine 1.0")->end();
            return;
        }
        renderProfileEntry(res, *slug, "new");
    } catch (const std::exception&) { serverError(res, "امکان بارگذاری صفحه وجود ندارد."); }
}

void ProfileController::editProfilePage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    if (checkRateLimit(res, req)) return;
    const auto slug = common::decodeProfileSlug(req->getParameter(0), true);
    if (!slug || common::SlugGenerator::isReservedSlug(*slug)) { notFound(res); return; }
    try {
        const auto canonical = common::canonicalProfileSlug(*slug).value();
        if (canonical != *slug) {
            auto target = getStorage()->findBySlug(canonical);
            if (!target.success) { serverError(res); return; }
            if (target.value) {
                res->writeStatus("301 Moved Permanently")
                    ->writeHeader("Location", "/profiles/" + common::encodeProfileSlug(canonical) + "/edit")
                    ->writeHeader("Cache-Control", "private, no-store")
                    ->writeHeader("Server", "HatefEngine 1.0")->end(); return;
            }
        }
        auto result = getStorage()->findBySlug(*slug);
        if (!result.success) { serverError(res, "امکان بارگذاری صفحه وجود ندارد."); return; }
        if (!result.value) {
            auto free = getStorage()->checkSlugAvailability(canonical);
            if (!free.success) { serverError(res); return; }
            renderProfileEntry(res, canonical, free.value ? "missing" : "unavailable"); return;
        }
        if (result.value->type != storage::ProfileType::PERSON) { badRequest(res, "این ویرایشگر برای پروفایل شخصی است."); return; }
        // Only the public identifier is rendered. All private data requires authentication via the API.
        renderProfileEntry(res, *slug, "editor", result.value->id.value_or(""));
    } catch (const std::exception&) { serverError(res, "امکان بارگذاری صفحه وجود ندارد."); }
}

void ProfileController::createProfile(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    if (checkRateLimit(res, req)) return;
    if (!req->getHeader("origin").empty() && !sameOrigin(req)) { json(res, {{"message", "مبدأ درخواست معتبر نیست."}}, "403 Forbidden"); return; }
    const bool secure = secureCookies();
    readEditorBody(res, [this, res, secure](const Json& body) {
        try {
            if (!body.is_object()) throw std::invalid_argument("اطلاعات نامعتبر است.");
            const auto requestedSlug = body.value("slug", "");
            const auto normalized = common::canonicalProfileSlug(requestedSlug);
            if (!normalized) throw std::invalid_argument(common::profileSlugValidationError(requestedSlug));
            const auto slug = *normalized;
            if (!common::isValidProfileSlug(slug) || common::SlugGenerator::isReservedSlug(slug))
                throw std::invalid_argument("آدرس صفحه معتبر نیست.");
            const auto key = generateOwnerToken();
            storage::Profile base;
            base.slug = slug; base.name = body.value("name", "");
            base.type = stringToProfileType(body.value("type", "PERSON"));
            base.isPublic = body.value("isPublic", base.type != storage::ProfileType::PERSON);
            if (base.type == storage::ProfileType::PERSON && base.isPublic)
                throw std::invalid_argument("ابتدا پیش‌نویس را بسازید و سپس انتشار صفحه را انتخاب کنید.");
            base.createdAt = std::chrono::system_clock::now();
            base.ownerTokenHash = profile::keyHash(key);
            std::string id;
            Json publicData;
            if (base.type == storage::ProfileType::PERSON) {
                storage::PersonProfile person;
                static_cast<storage::Profile&>(person) = base;
                Json fields = body; fields.erase("slug"); fields.erase("type");
                profile::applyEditorPatch(person, fields);
                auto result = getStorage()->store(person);
                if (!result.success) {
                    if (result.message.find("taken") != std::string::npos || result.message.find("E11000") != std::string::npos || result.message.find("duplicate key") != std::string::npos) {
                        json(res, {{"message", "این آدرس قبلاً گرفته شده است."}}, "409 Conflict"); return;
                    }
                    serverError(res, "ساخت صفحه انجام نشد."); return;
                }
                person.id = id = result.value; publicData = personProfileToJson(person);
            } else {
                base.bio = body.value("bio", "");
                auto validation = storage::ProfileValidator::validate(base);
                if (!validation.isValid) throw std::invalid_argument("اطلاعات پروفایل معتبر نیست.");
                auto result = getStorage()->store(base);
                if (!result.success) { json(res, {{"message", "ساخت صفحه انجام نشد؛ ممکن است آدرس قبلاً گرفته شده باشد."}}, "409 Conflict"); return; }
                base.id = id = result.value; publicData = profileToJson(base);
            }
            res->writeStatus("201 Created")->writeHeader("Cache-Control", "private, no-store")
                ->writeHeader("Set-Cookie", profile::ownerCookie(id, key, secure));
            json(res, {{"success", true}, {"data", publicData}, {"canEdit", true}, {"ownerToken", key}}, "201 Created");
        } catch (const std::invalid_argument& e) { json(res, {{"success", false}, {"message", e.what()}}, "400 Bad Request"); }
        catch (const std::exception&) { serverError(res, "ساخت صفحه انجام نشد."); }
    });
}

void ProfileController::createOwnerSession(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    if (checkRateLimit(res, req)) return;
    if (!sameOrigin(req)) { json(res, {{"message", "مبدأ درخواست معتبر نیست."}}, "403 Forbidden"); return; }
    const std::string id(req->getParameter(0)); const bool secure = secureCookies();
    readEditorBody(res, [this, res, id, secure](const Json& body) {
        const auto key = body.value("key", "");
        auto result = getStorage()->findById(id);
        if (!result.success || !checkOwnership(result.value, key)) {
            json(res, {{"success", false}, {"message", "کلید دسترسی صحیح نیست."}}, "403 Forbidden"); return;
        }
        res->writeStatus("200 OK")->writeHeader("Cache-Control", "private, no-store")
            ->writeHeader("Set-Cookie", profile::ownerCookie(id, key, secure));
        json(res, {{"success", true}});
    });
}

void ProfileController::deleteOwnerSession(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    if (!sameOrigin(req)) { json(res, {{"message", "مبدأ درخواست معتبر نیست."}}, "403 Forbidden"); return; }
    const std::string id(req->getParameter(0));
    if (id.size() != 24 || id.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) { badRequest(res); return; }
    res->writeStatus("200 OK")->writeHeader("Cache-Control", "private, no-store")
        ->writeHeader("Set-Cookie", profile::ownerCookie(id, "", secureCookies(), true));
    json(res, {{"success", true}});
}

void ProfileController::savePersonPatch(uWS::HttpResponse<false>* res, const storage::PersonProfile& stored, const Json& body) {
    if (checkOwnerMutationRateLimit(res, stored.id.value_or(""))) return;
    try {
        if (!body.contains("version") || !body["version"].is_number_integer()) {
            json(res, {{"message", "نسخهٔ اطلاعات لازم است."}}, "400 Bad Request"); return;
        }
        const auto version = body["version"].get<int64_t>();
        if (version != stored.version) { json(res, {{"message", "اطلاعات در جای دیگری تغییر کرده است."}}, "409 Conflict"); return; }
        auto person = stored;
        profile::applyEditorPatch(person, body);
        profile::bridgeLegacyPatch(person, body);
        std::set<std::string> fields;
        for (auto it = body.begin(); it != body.end(); ++it) if (it.key() != "version") fields.insert(it.key());
        if (fields.count("name")) fields.insert("displayName");
        if (fields.count("skillsWithLevel")) fields.insert("skills");
        if (body.contains("skillsWithLevel") && person.content.sections.count("skills")) fields.insert("content");
        auto result = getStorage()->updatePersonFields(person, {fields.begin(), fields.end()}, version);
        if (!result.success) {
            json(res, {{"message", result.message == "VERSION_CONFLICT" ? "اطلاعات در جای دیگری تغییر کرده است." : "ذخیره انجام نشد."}},
                result.message == "VERSION_CONFLICT" ? "409 Conflict" : "500 Internal Server Error"); return;
        }
        person.version = version + 1;
        person.updatedAt = std::chrono::system_clock::now();
        res->writeStatus("200 OK")->writeHeader("Cache-Control", "private, no-store");
        json(res, {{"success", true}, {"data", personProfileToJson(person)}, {"canEdit", true}});
    } catch (const std::exception& e) { json(res, {{"message", e.what()}}, "400 Bad Request"); }
}
