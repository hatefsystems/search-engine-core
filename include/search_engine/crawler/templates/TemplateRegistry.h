#pragma once
#include "TemplateTypes.h"
#include <unordered_map>
#include <mutex>

namespace search_engine {
namespace crawler {
namespace templates {

class TemplateRegistry {
public:
    static TemplateRegistry& instance() {
        static TemplateRegistry inst;
        return inst;
    }

    void upsertTemplate(const TemplateDefinition& def) {
        std::lock_guard<std::mutex> lock(mutex_);
        templatesByName_[def.name] = def;
    }

    bool removeTemplate(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return templatesByName_.erase(name) > 0;
    }

    std::vector<TemplateDefinition> listTemplates() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TemplateDefinition> out;
        out.reserve(templatesByName_.size());
        for (const auto& [name, def] : templatesByName_) {
            out.push_back(def);
        }
        return out;
    }

    std::optional<TemplateDefinition> getTemplate(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = templatesByName_.find(name);
        if (it == templatesByName_.end()) return std::nullopt;
        return it->second;
    }

private:
    TemplateRegistry() = default;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, TemplateDefinition> templatesByName_;
};

} // namespace templates
} // namespace crawler
} // namespace search_engine


