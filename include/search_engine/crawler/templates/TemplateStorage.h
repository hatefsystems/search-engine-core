#pragma once
#include "TemplateRegistry.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace search_engine {
namespace crawler {
namespace templates {

inline nlohmann::json toJson(const TemplateDefinition& def) {
    nlohmann::json j;
    j["name"] = def.name;
    j["description"] = def.description;
    j["config"] = nlohmann::json::object();
    if (def.config.maxPages.has_value()) j["config"]["maxPages"] = *def.config.maxPages;
    if (def.config.maxDepth.has_value()) j["config"]["maxDepth"] = *def.config.maxDepth;
    if (def.config.spaRenderingEnabled.has_value()) j["config"]["spaRenderingEnabled"] = *def.config.spaRenderingEnabled;
    if (def.config.extractTextContent.has_value()) j["config"]["extractTextContent"] = *def.config.extractTextContent;
    if (def.config.politenessDelayMs.has_value()) j["config"]["politenessDelay"] = *def.config.politenessDelayMs;
    j["patterns"]["articleSelectors"] = def.patterns.articleSelectors;
    j["patterns"]["titleSelectors"] = def.patterns.titleSelectors;
    j["patterns"]["contentSelectors"] = def.patterns.contentSelectors;
    return j;
}

inline TemplateDefinition fromJson(const nlohmann::json& j) {
    TemplateDefinition def;
    def.name = j.value("name", "");
    def.description = j.value("description", "");
    if (j.contains("config") && j["config"].is_object()) {
        const auto& cfg = j["config"];
        if (cfg.contains("maxPages")) def.config.maxPages = cfg.value("maxPages", 0);
        if (cfg.contains("maxDepth")) def.config.maxDepth = cfg.value("maxDepth", 0);
        if (cfg.contains("spaRenderingEnabled")) def.config.spaRenderingEnabled = cfg.value("spaRenderingEnabled", false);
        if (cfg.contains("extractTextContent")) def.config.extractTextContent = cfg.value("extractTextContent", false);
        if (cfg.contains("politenessDelay")) def.config.politenessDelayMs = cfg.value("politenessDelay", 0);
    }
    if (j.contains("patterns") && j["patterns"].is_object()) {
        const auto& pat = j["patterns"];
        if (pat.contains("articleSelectors")) def.patterns.articleSelectors = pat["articleSelectors"].get<std::vector<std::string>>();
        if (pat.contains("titleSelectors")) def.patterns.titleSelectors = pat["titleSelectors"].get<std::vector<std::string>>();
        if (pat.contains("contentSelectors")) def.patterns.contentSelectors = pat["contentSelectors"].get<std::vector<std::string>>();
    }
    return def;
}

inline void loadTemplatesFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return;
    nlohmann::json root;
    try {
        in >> root;
        if (root.is_array()) {
            for (const auto& j : root) {
                TemplateRegistry::instance().upsertTemplate(fromJson(j));
            }
        }
    } catch (...) {
        // ignore malformed file
    }
}

inline void loadTemplatesFromDirectory(const std::string& dirPath) {
    try {
        if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
            return;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::ifstream file(entry.path());
                if (file.is_open()) {
                    try {
                        nlohmann::json templateJson;
                        file >> templateJson;
                        TemplateDefinition def = fromJson(templateJson);
                        if (!def.name.empty()) {
                            TemplateRegistry::instance().upsertTemplate(def);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to load template from " << entry.path() << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load templates from directory " << dirPath << ": " << e.what() << std::endl;
    }
}

inline void saveTemplatesToFile(const std::string& path) {
    auto list = TemplateRegistry::instance().listTemplates();
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& def : list) arr.push_back(toJson(def));
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;
    out << arr.dump(2);
}

inline void saveTemplatesToDirectory(const std::string& dirPath) {
    try {
        if (!std::filesystem::exists(dirPath)) {
            std::filesystem::create_directories(dirPath);
        }
        
        auto list = TemplateRegistry::instance().listTemplates();
        for (const auto& def : list) {
            std::string filename = dirPath + "/" + def.name + ".json";
            std::ofstream out(filename, std::ios::trunc);
            if (out.is_open()) {
                out << toJson(def).dump(2);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to save templates to directory " << dirPath << ": " << e.what() << std::endl;
    }
}

} // namespace templates
} // namespace crawler
} // namespace search_engine


