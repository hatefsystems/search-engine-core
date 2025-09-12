#include "TemplatesController.h"
#include "../../include/Logger.h"
#include "../../include/search_engine/crawler/templates/TemplateRegistry.h"
#include "../../include/search_engine/crawler/templates/TemplateApplier.h"
#include "../../include/search_engine/crawler/templates/TemplateValidator.h"
#include "../../include/search_engine/crawler/templates/TemplateStorage.h"
#include "../../include/search_engine/crawler/CrawlerManager.h"
#include "../../include/search_engine/storage/ContentStorage.h"
#include "../../include/search_engine/crawler/models/CrawlConfig.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <optional>
#include <cstdlib>

void TemplatesController::listTemplates(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    using namespace search_engine::crawler::templates;
    auto list = TemplateRegistry::instance().listTemplates();
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& t : list) {
        nlohmann::json item;
        item["name"] = t.name;
        item["description"] = t.description;
        item["config"] = nlohmann::json::object();
        if (t.config.maxPages.has_value()) item["config"]["maxPages"] = *t.config.maxPages;
        if (t.config.maxDepth.has_value()) item["config"]["maxDepth"] = *t.config.maxDepth;
        if (t.config.spaRenderingEnabled.has_value()) item["config"]["spaRenderingEnabled"] = *t.config.spaRenderingEnabled;
        if (t.config.extractTextContent.has_value()) item["config"]["extractTextContent"] = *t.config.extractTextContent;
        if (t.config.politenessDelayMs.has_value()) item["config"]["politenessDelay"] = *t.config.politenessDelayMs;
        item["patterns"] = nlohmann::json::object();
        item["patterns"]["articleSelectors"] = t.patterns.articleSelectors;
        item["patterns"]["titleSelectors"] = t.patterns.titleSelectors;
        item["patterns"]["contentSelectors"] = t.patterns.contentSelectors;
        arr.push_back(item);
    }
    nlohmann::json response = { {"templates", arr} };
    json(res, response, "200 OK");
}

static std::string urlDecode(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%') {
            if (i + 2 < value.size()) {
                int h = std::stoi(value.substr(i + 1, 2), nullptr, 16);
                result.push_back(static_cast<char>(h));
                i += 2;
            }
        } else if (value[i] == '+') {
            result.push_back(' ');
        } else {
            result.push_back(value[i]);
        }
    }
    return result;
}

// Helper to extract :name from simple pattern /api/templates/:name
static std::optional<std::string> extractNameParam(uWS::HttpRequest* req) {
    std::string path = std::string(req->getUrl());
    const std::string prefix = "/api/templates/";
    if (path.rfind(prefix, 0) == 0 && path.size() > prefix.size()) {
        std::string encoded = path.substr(prefix.size());
        return urlDecode(encoded);
    }
    return std::nullopt;
}

void TemplatesController::getTemplateByName(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    using namespace search_engine::crawler::templates;
    auto maybeName = extractNameParam(req);
    if (!maybeName.has_value() || maybeName->empty()) {
        badRequest(res, "template name is required");
        return;
    }
    auto def = TemplateRegistry::instance().getTemplate(*maybeName);
    if (!def.has_value()) { notFound(res, "template not found"); return; }
    nlohmann::json item;
    item["name"] = def->name;
    item["description"] = def->description;
    item["config"] = nlohmann::json::object();
    if (def->config.maxPages.has_value()) item["config"]["maxPages"] = *def->config.maxPages;
    if (def->config.maxDepth.has_value()) item["config"]["maxDepth"] = *def->config.maxDepth;
    if (def->config.spaRenderingEnabled.has_value()) item["config"]["spaRenderingEnabled"] = *def->config.spaRenderingEnabled;
    if (def->config.extractTextContent.has_value()) item["config"]["extractTextContent"] = *def->config.extractTextContent;
    if (def->config.politenessDelayMs.has_value()) item["config"]["politenessDelay"] = *def->config.politenessDelayMs;
    item["patterns"] = nlohmann::json::object();
    item["patterns"]["articleSelectors"] = def->patterns.articleSelectors;
    item["patterns"]["titleSelectors"] = def->patterns.titleSelectors;
    item["patterns"]["contentSelectors"] = def->patterns.contentSelectors;
    json(res, item, "200 OK");
}

void TemplatesController::deleteTemplateByName(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    using namespace search_engine::crawler::templates;
    auto maybeName = extractNameParam(req);
    if (!maybeName.has_value() || maybeName->empty()) {
        badRequest(res, "template name is required");
        return;
    }
    bool removed = TemplateRegistry::instance().removeTemplate(*maybeName);
    if (!removed) { notFound(res, "template not found"); return; }
    const char* templatesPathEnv = std::getenv("TEMPLATES_PATH");
    std::string templatesPath = templatesPathEnv ? templatesPathEnv : "/app/config/templates";
    
    // Check if path is a directory or file
    if (std::filesystem::exists(templatesPath) && std::filesystem::is_directory(templatesPath)) {
        saveTemplatesToDirectory(templatesPath);
    } else {
        saveTemplatesToFile(templatesPath);
    }
    nlohmann::json response = {{"status", "deleted"}, {"name", *maybeName}};
    json(res, response, "200 OK");
}

void TemplatesController::createTemplate(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    res->onAborted([]() {});
    std::string buffer;
    res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        if (!last) return;
        try {
            auto body = nlohmann::json::parse(buffer);
            using namespace search_engine::crawler::templates;
            auto vr = validateTemplateJson(body);
            if (!vr.valid) { badRequest(res, vr.message); return; }
            
            // Normalize and check for uniqueness
            std::string normalizedName = normalizeTemplateName(body.value("name", ""));
            if (TemplateRegistry::instance().getTemplate(normalizedName).has_value()) {
                badRequest(res, "Template with this name already exists");
                return;
            }
            
            TemplateDefinition def;
            def.name = normalizedName;
            def.description = body.value("description", "");
            if (body.contains("config") && body["config"].is_object()) {
                const auto& cfg = body["config"];
                if (cfg.contains("maxPages")) def.config.maxPages = cfg.value("maxPages", 0);
                if (cfg.contains("maxDepth")) def.config.maxDepth = cfg.value("maxDepth", 0);
                if (cfg.contains("spaRenderingEnabled")) def.config.spaRenderingEnabled = cfg.value("spaRenderingEnabled", false);
                if (cfg.contains("extractTextContent")) def.config.extractTextContent = cfg.value("extractTextContent", false);
                if (cfg.contains("politenessDelay")) def.config.politenessDelayMs = cfg.value("politenessDelay", 0);
            }
            if (body.contains("patterns") && body["patterns"].is_object()) {
                const auto& pat = body["patterns"];
                if (pat.contains("articleSelectors")) def.patterns.articleSelectors = pat["articleSelectors"].get<std::vector<std::string>>();
                if (pat.contains("titleSelectors")) def.patterns.titleSelectors = pat["titleSelectors"].get<std::vector<std::string>>();
                if (pat.contains("contentSelectors")) def.patterns.contentSelectors = pat["contentSelectors"].get<std::vector<std::string>>();
            }
            TemplateRegistry::instance().upsertTemplate(def);
            // Persist templates to disk after upsert
            {
                const char* templatesPathEnv = std::getenv("TEMPLATES_PATH");
                std::string templatesPath = templatesPathEnv ? templatesPathEnv : "/app/config/templates";
                
                // Check if path is a directory or file
                if (std::filesystem::exists(templatesPath) && std::filesystem::is_directory(templatesPath)) {
                    saveTemplatesToDirectory(templatesPath);
                } else {
                    saveTemplatesToFile(templatesPath);
                }
            }
            nlohmann::json response = {
                {"status", "accepted"},
                {"message", "Template stored"},
                {"name", def.name}
            };
            json(res, response, "202 Accepted");
        } catch (const std::exception& ) {
            badRequest(res, "Invalid JSON format");
        }
    });
}

void TemplatesController::addSiteWithTemplate(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    res->onAborted([]() {});
    std::string buffer;
    res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        if (!last) return;
        try {
            auto body = nlohmann::json::parse(buffer);
            if (!body.contains("url") || !body["url"].is_string() || body["url"].get<std::string>().empty()) {
                badRequest(res, "url is required and must be a string");
                return;
            }
            if (!body.contains("template") || !body["template"].is_string() || body["template"].get<std::string>().empty()) {
                badRequest(res, "template is required and must be a string");
                return;
            }
            std::string url = body["url"].get<std::string>();
            std::string templateName = body["template"].get<std::string>();
            using namespace search_engine::crawler::templates;
            auto maybeDef = TemplateRegistry::instance().getTemplate(templateName);
            if (!maybeDef.has_value()) {
                notFound(res, "template not found");
                return;
            }
            // Build crawl config from template
            CrawlConfig cfg;
            applyTemplateToConfig(*maybeDef, cfg);

            // Optional overrides from request
            bool force = body.value("force", true);
            bool stopPreviousSessions = body.value("stopPreviousSessions", false);
            std::string browserlessUrl = body.value("browserlessUrl", cfg.browserlessUrl);
            int requestTimeoutMs = body.value("requestTimeout", static_cast<int>(cfg.requestTimeout.count()));

            cfg.browserlessUrl = browserlessUrl;
            cfg.requestTimeout = std::chrono::milliseconds(requestTimeoutMs);

            // Initialize a crawler manager (mirrors SearchController initialization)
            static std::unique_ptr<CrawlerManager> s_templateCrawlerManager;
            static std::once_flag s_templateCrawlerManagerInit;
            std::call_once(s_templateCrawlerManagerInit, []() {
                try {
                    const char* mongoUri = std::getenv("MONGODB_URI");
                    std::string mongoConnectionString = mongoUri ? mongoUri : "mongodb://localhost:27017";
                    const char* redisUri = std::getenv("SEARCH_REDIS_URI");
                    std::string redisConnectionString = redisUri ? redisUri : "tcp://127.0.0.1:6379";
                    auto storage = std::make_shared<search_engine::storage::ContentStorage>(
                        mongoConnectionString,
                        "search-engine",
                        redisConnectionString,
                        "search_index"
                    );
                    s_templateCrawlerManager = std::make_unique<CrawlerManager>(storage);
                    LOG_INFO("Template crawler manager initialized");
                } catch (const std::exception& e) {
                    LOG_ERROR(std::string("Failed to initialize template crawler manager: ") + e.what());
                }
            });

            if (!s_templateCrawlerManager) {
                serverError(res, "CrawlerManager not initialized");
                return;
            }

            if (stopPreviousSessions) {
                auto activeSessions = s_templateCrawlerManager->getActiveSessions();
                for (const auto& sid : activeSessions) {
                    s_templateCrawlerManager->stopCrawl(sid);
                }
            }

            // Start the crawl
            std::string sessionId = s_templateCrawlerManager->startCrawl(url, cfg, force);

            nlohmann::json response = {
                {"success", true},
                {"message", "Crawl session started with template"},
                {"data", {
                    {"sessionId", sessionId},
                    {"url", url},
                    {"template", templateName},
                    {"appliedConfig", {
                        {"maxPages", static_cast<int>(cfg.maxPages)},
                        {"maxDepth", static_cast<int>(cfg.maxDepth)},
                        {"politenessDelay", static_cast<int>(cfg.politenessDelay.count())},
                        {"spaRenderingEnabled", cfg.spaRenderingEnabled},
                        {"extractTextContent", cfg.extractTextContent},
                        {"requestTimeout", static_cast<int>(cfg.requestTimeout.count())},
                        {"browserlessUrl", cfg.browserlessUrl}
                    }}
                }}
            };
            json(res, response, "202 Accepted");
        } catch (...) {
            badRequest(res, "Invalid JSON format");
        }
    });
}
