#include "../../include/search_engine/models/JobConfig.h"
#include "../../include/Logger.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <sstream>
#include <algorithm>
#include <cmath>

using namespace bsoncxx::builder::stream;
using namespace search_engine::models;

namespace {
    // Helper function to convert time_point to BSON date
    bsoncxx::types::b_date timePointToBsonDate(const std::chrono::system_clock::time_point& tp) {
        auto duration = tp.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        return bsoncxx::types::b_date{std::chrono::milliseconds{millis}};
    }
    
    // Helper function to convert BSON date to time_point
    std::chrono::system_clock::time_point bsonDateToTimePoint(const bsoncxx::types::b_date& date) {
        return std::chrono::system_clock::time_point{date.value};
    }
}

// RetryPolicy implementation
std::chrono::seconds RetryPolicy::calculateDelay(int retryAttempt) const {
    if (retryAttempt <= 0) {
        return initialDelay;
    }

    if (!exponentialBackoff) {
        return initialDelay;
    }

    // Calculate exponential backoff with jitter
    double delaySeconds = initialDelay.count() * std::pow(backoffMultiplier, retryAttempt);
    
    // Cap at maximum delay
    delaySeconds = std::min(delaySeconds, static_cast<double>(maxDelay.count()));
    
    return std::chrono::seconds(static_cast<int64_t>(delaySeconds));
}

// JobConfig implementation
JobConfig::JobConfig()
    : timeout_(std::chrono::seconds(3600)), // 1 hour default
      defaultPriority_(JobPriority::NORMAL),
      enabled_(true),
      createdAt_(std::chrono::system_clock::now()),
      updatedAt_(std::chrono::system_clock::now()) {
}

JobConfig::JobConfig(const std::string& jobType)
    : jobType_(jobType),
      name_(jobType),
      timeout_(std::chrono::seconds(3600)),
      defaultPriority_(JobPriority::NORMAL),
      enabled_(true),
      createdAt_(std::chrono::system_clock::now()),
      updatedAt_(std::chrono::system_clock::now()) {
}

JobConfig::JobConfig(const std::string& jobType,
                     const std::string& name,
                     std::chrono::seconds timeout,
                     JobPriority defaultPriority,
                     const RetryPolicy& retryPolicy)
    : jobType_(jobType),
      name_(name),
      timeout_(timeout),
      defaultPriority_(defaultPriority),
      retryPolicy_(retryPolicy),
      enabled_(true),
      createdAt_(std::chrono::system_clock::now()),
      updatedAt_(std::chrono::system_clock::now()) {
}

// Parameter management
void JobConfig::setParameter(const std::string& key, const std::string& value) {
    parameters_[key] = value;
    updateTimestamp();
}

std::optional<std::string> JobConfig::getParameter(const std::string& key) const {
    auto it = parameters_.find(key);
    if (it != parameters_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void JobConfig::removeParameter(const std::string& key) {
    parameters_.erase(key);
    updateTimestamp();
}

bool JobConfig::hasParameter(const std::string& key) const {
    return parameters_.find(key) != parameters_.end();
}

// Tag management
void JobConfig::setTag(const std::string& key, const std::string& value) {
    tags_[key] = value;
    updateTimestamp();
}

std::optional<std::string> JobConfig::getTag(const std::string& key) const {
    auto it = tags_.find(key);
    if (it != tags_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void JobConfig::removeTag(const std::string& key) {
    tags_.erase(key);
    updateTimestamp();
}

bool JobConfig::hasTag(const std::string& key) const {
    return tags_.find(key) != tags_.end();
}

// Business logic methods
Job JobConfig::createJob(const std::string& userId, const std::string& tenantId) const {
    Job job(jobType_, userId);
    job.setTenantId(tenantId);
    job.setPriority(defaultPriority_);
    job.setTimeout(timeout_);
    job.setMaxRetries(retryPolicy_.maxRetries);
    
    if (scheduleConfig_.scheduledAt.has_value()) {
        job.setScheduledAt(scheduleConfig_.scheduledAt.value());
    }
    
    // Add metadata from parameters
    if (!parameters_.empty()) {
        nlohmann::json metadata;
        for (const auto& [key, value] : parameters_) {
            metadata[key] = value;
        }
        job.setMetadata(metadata.dump());
    }
    
    LOG_DEBUG("Created job from config: " + jobType_ + " for user: " + userId);
    return job;
}

bool JobConfig::canExecuteNow() const {
    if (!enabled_) {
        return false;
    }
    
    if (scheduleConfig_.isExpired()) {
        return false;
    }
    
    if (scheduleConfig_.scheduledAt.has_value()) {
        return std::chrono::system_clock::now() >= scheduleConfig_.scheduledAt.value();
    }
    
    return true;
}

std::chrono::seconds JobConfig::getNextRetryDelay(int retryAttempt) const {
    return retryPolicy_.calculateDelay(retryAttempt);
}

bool JobConfig::meetsResourceRequirements(const ResourceRequirements& available) const {
    if (resourceRequirements_.cpuCores.has_value() && available.cpuCores.has_value()) {
        if (resourceRequirements_.cpuCores.value() > available.cpuCores.value()) {
            return false;
        }
    }
    
    if (resourceRequirements_.memoryMB.has_value() && available.memoryMB.has_value()) {
        if (resourceRequirements_.memoryMB.value() > available.memoryMB.value()) {
            return false;
        }
    }
    
    if (resourceRequirements_.diskSpaceMB.has_value() && available.diskSpaceMB.has_value()) {
        if (resourceRequirements_.diskSpaceMB.value() > available.diskSpaceMB.value()) {
            return false;
        }
    }
    
    if (resourceRequirements_.networkBandwidth.has_value() && available.networkBandwidth.has_value()) {
        if (resourceRequirements_.networkBandwidth.value() > available.networkBandwidth.value()) {
            return false;
        }
    }
    
    return true;
}

// Validation methods
bool JobConfig::isValid() const {
    return !jobType_.empty() && !name_.empty() && timeout_.count() > 0 && 
           retryPolicy_.maxRetries >= 0 && retryPolicy_.initialDelay.count() >= 0;
}

Result<bool> JobConfig::validate() const {
    if (jobType_.empty()) {
        return Result<bool>::Failure("Job type cannot be empty");
    }
    if (name_.empty()) {
        return Result<bool>::Failure("Name cannot be empty");
    }
    if (timeout_.count() <= 0) {
        return Result<bool>::Failure("Timeout must be positive");
    }
    if (retryPolicy_.maxRetries < 0) {
        return Result<bool>::Failure("Max retries cannot be negative");
    }
    if (retryPolicy_.initialDelay.count() < 0) {
        return Result<bool>::Failure("Initial delay cannot be negative");
    }
    if (retryPolicy_.maxDelay.count() < retryPolicy_.initialDelay.count()) {
        return Result<bool>::Failure("Max delay must be greater than or equal to initial delay");
    }
    if (retryPolicy_.backoffMultiplier <= 0) {
        return Result<bool>::Failure("Backoff multiplier must be positive");
    }
    if (concurrencyLimit_.has_value() && concurrencyLimit_.value() <= 0) {
        return Result<bool>::Failure("Concurrency limit must be positive");
    }
    
    return Result<bool>::Success(true, "JobConfig validation passed");
}

// Serialization methods
bsoncxx::document::value JobConfig::toBson() const {
    auto builder = document{};
    
    builder << "jobType" << jobType_
            << "name" << name_
            << "description" << description_
            << "timeout" << static_cast<int64_t>(timeout_.count())
            << "defaultPriority" << Job::priorityToString(defaultPriority_)
            << "enabled" << enabled_
            << "createdAt" << timePointToBsonDate(createdAt_)
            << "updatedAt" << timePointToBsonDate(updatedAt_);

    // Retry policy
    auto retryBuilder = document{};
    retryBuilder << "maxRetries" << retryPolicy_.maxRetries
                 << "initialDelay" << static_cast<int64_t>(retryPolicy_.initialDelay.count())
                 << "maxDelay" << static_cast<int64_t>(retryPolicy_.maxDelay.count())
                 << "backoffMultiplier" << retryPolicy_.backoffMultiplier
                 << "exponentialBackoff" << retryPolicy_.exponentialBackoff;
    builder << "retryPolicy" << retryBuilder;

    // Resource requirements
    if (resourceRequirements_.cpuCores.has_value() || resourceRequirements_.memoryMB.has_value() ||
        resourceRequirements_.diskSpaceMB.has_value() || resourceRequirements_.networkBandwidth.has_value()) {
        auto resourceBuilder = document{};
        if (resourceRequirements_.cpuCores.has_value()) {
            resourceBuilder << "cpuCores" << resourceRequirements_.cpuCores.value();
        }
        if (resourceRequirements_.memoryMB.has_value()) {
            resourceBuilder << "memoryMB" << static_cast<int64_t>(resourceRequirements_.memoryMB.value());
        }
        if (resourceRequirements_.diskSpaceMB.has_value()) {
            resourceBuilder << "diskSpaceMB" << static_cast<int64_t>(resourceRequirements_.diskSpaceMB.value());
        }
        if (resourceRequirements_.networkBandwidth.has_value()) {
            resourceBuilder << "networkBandwidth" << resourceRequirements_.networkBandwidth.value();
        }
        builder << "resourceRequirements" << resourceBuilder;
    }

    // Schedule config
    if (scheduleConfig_.scheduledAt.has_value() || scheduleConfig_.cronExpression.has_value() ||
        scheduleConfig_.recurring || scheduleConfig_.expiresAt.has_value()) {
        auto scheduleBuilder = document{};
        if (scheduleConfig_.scheduledAt.has_value()) {
            scheduleBuilder << "scheduledAt" << timePointToBsonDate(scheduleConfig_.scheduledAt.value());
        }
        if (scheduleConfig_.cronExpression.has_value()) {
            scheduleBuilder << "cronExpression" << scheduleConfig_.cronExpression.value();
        }
        scheduleBuilder << "recurring" << scheduleConfig_.recurring;
        if (scheduleConfig_.expiresAt.has_value()) {
            scheduleBuilder << "expiresAt" << timePointToBsonDate(scheduleConfig_.expiresAt.value());
        }
        builder << "scheduleConfig" << scheduleBuilder;
    }

    // Parameters
    if (!parameters_.empty()) {
        auto paramBuilder = document{};
        for (const auto& [key, value] : parameters_) {
            paramBuilder << key << value;
        }
        builder << "parameters" << paramBuilder;
    }

    // Tags
    if (!tags_.empty()) {
        auto tagBuilder = document{};
        for (const auto& [key, value] : tags_) {
            tagBuilder << key << value;
        }
        builder << "tags" << tagBuilder;
    }

    if (concurrencyLimit_.has_value()) {
        builder << "concurrencyLimit" << concurrencyLimit_.value();
    }

    return builder << finalize;
}

JobConfig JobConfig::fromBson(const bsoncxx::document::view& doc) {
    JobConfig config;
    
    if (auto jobType = doc["jobType"]) {
        config.jobType_ = std::string(jobType.get_string().value);
    }
    if (auto name = doc["name"]) {
        config.name_ = std::string(name.get_string().value);
    }
    if (auto description = doc["description"]) {
        config.description_ = std::string(description.get_string().value);
    }
    if (auto timeout = doc["timeout"]) {
        config.timeout_ = std::chrono::seconds(timeout.get_int64().value);
    }
    if (auto defaultPriority = doc["defaultPriority"]) {
        config.defaultPriority_ = Job::stringToPriority(std::string(defaultPriority.get_string().value));
    }
    if (auto enabled = doc["enabled"]) {
        config.enabled_ = enabled.get_bool().value;
    }
    if (auto createdAt = doc["createdAt"]) {
        config.createdAt_ = bsonDateToTimePoint(createdAt.get_date());
    }
    if (auto updatedAt = doc["updatedAt"]) {
        config.updatedAt_ = bsonDateToTimePoint(updatedAt.get_date());
    }

    // Retry policy
    if (auto retryPolicy = doc["retryPolicy"]) {
        auto retryDoc = retryPolicy.get_document().value;
        if (auto maxRetries = retryDoc["maxRetries"]) {
            config.retryPolicy_.maxRetries = maxRetries.get_int32().value;
        }
        if (auto initialDelay = retryDoc["initialDelay"]) {
            config.retryPolicy_.initialDelay = std::chrono::seconds(initialDelay.get_int64().value);
        }
        if (auto maxDelay = retryDoc["maxDelay"]) {
            config.retryPolicy_.maxDelay = std::chrono::seconds(maxDelay.get_int64().value);
        }
        if (auto backoffMultiplier = retryDoc["backoffMultiplier"]) {
            config.retryPolicy_.backoffMultiplier = backoffMultiplier.get_double().value;
        }
        if (auto exponentialBackoff = retryDoc["exponentialBackoff"]) {
            config.retryPolicy_.exponentialBackoff = exponentialBackoff.get_bool().value;
        }
    }

    // Resource requirements
    if (auto resourceReq = doc["resourceRequirements"]) {
        auto resourceDoc = resourceReq.get_document().value;
        if (auto cpuCores = resourceDoc["cpuCores"]) {
            config.resourceRequirements_.cpuCores = cpuCores.get_int32().value;
        }
        if (auto memoryMB = resourceDoc["memoryMB"]) {
            config.resourceRequirements_.memoryMB = static_cast<size_t>(memoryMB.get_int64().value);
        }
        if (auto diskSpaceMB = resourceDoc["diskSpaceMB"]) {
            config.resourceRequirements_.diskSpaceMB = static_cast<size_t>(diskSpaceMB.get_int64().value);
        }
        if (auto networkBandwidth = resourceDoc["networkBandwidth"]) {
            config.resourceRequirements_.networkBandwidth = networkBandwidth.get_int32().value;
        }
    }

    // Schedule config
    if (auto scheduleConf = doc["scheduleConfig"]) {
        auto scheduleDoc = scheduleConf.get_document().value;
        if (auto scheduledAt = scheduleDoc["scheduledAt"]) {
            config.scheduleConfig_.scheduledAt = bsonDateToTimePoint(scheduledAt.get_date());
        }
        if (auto cronExpression = scheduleDoc["cronExpression"]) {
            config.scheduleConfig_.cronExpression = std::string(cronExpression.get_string().value);
        }
        if (auto recurring = scheduleDoc["recurring"]) {
            config.scheduleConfig_.recurring = recurring.get_bool().value;
        }
        if (auto expiresAt = scheduleDoc["expiresAt"]) {
            config.scheduleConfig_.expiresAt = bsonDateToTimePoint(expiresAt.get_date());
        }
    }

    // Parameters
    if (auto parameters = doc["parameters"]) {
        auto paramDoc = parameters.get_document().value;
        for (auto element : paramDoc) {
            config.parameters_[std::string(element.key())] = std::string(element.get_string().value);
        }
    }

    // Tags
    if (auto tags = doc["tags"]) {
        auto tagDoc = tags.get_document().value;
        for (auto element : tagDoc) {
            config.tags_[std::string(element.key())] = std::string(element.get_string().value);
        }
    }

    if (auto concurrencyLimit = doc["concurrencyLimit"]) {
        config.concurrencyLimit_ = concurrencyLimit.get_int32().value;
    }

    return config;
}

nlohmann::json JobConfig::toJson() const {
    nlohmann::json json;
    
    json["jobType"] = jobType_;
    json["name"] = name_;
    json["description"] = description_;
    json["timeout"] = timeout_.count();
    json["defaultPriority"] = Job::priorityToString(defaultPriority_);
    json["enabled"] = enabled_;
    json["createdAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(createdAt_.time_since_epoch()).count();
    json["updatedAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(updatedAt_.time_since_epoch()).count();

    // Retry policy
    json["retryPolicy"] = {
        {"maxRetries", retryPolicy_.maxRetries},
        {"initialDelay", retryPolicy_.initialDelay.count()},
        {"maxDelay", retryPolicy_.maxDelay.count()},
        {"backoffMultiplier", retryPolicy_.backoffMultiplier},
        {"exponentialBackoff", retryPolicy_.exponentialBackoff}
    };

    // Resource requirements (only include non-empty values)
    nlohmann::json resourceReq;
    if (resourceRequirements_.cpuCores.has_value()) {
        resourceReq["cpuCores"] = resourceRequirements_.cpuCores.value();
    }
    if (resourceRequirements_.memoryMB.has_value()) {
        resourceReq["memoryMB"] = resourceRequirements_.memoryMB.value();
    }
    if (resourceRequirements_.diskSpaceMB.has_value()) {
        resourceReq["diskSpaceMB"] = resourceRequirements_.diskSpaceMB.value();
    }
    if (resourceRequirements_.networkBandwidth.has_value()) {
        resourceReq["networkBandwidth"] = resourceRequirements_.networkBandwidth.value();
    }
    if (!resourceReq.empty()) {
        json["resourceRequirements"] = resourceReq;
    }

    // Schedule config
    nlohmann::json scheduleConf;
    if (scheduleConfig_.scheduledAt.has_value()) {
        scheduleConf["scheduledAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            scheduleConfig_.scheduledAt->time_since_epoch()).count();
    }
    if (scheduleConfig_.cronExpression.has_value()) {
        scheduleConf["cronExpression"] = scheduleConfig_.cronExpression.value();
    }
    scheduleConf["recurring"] = scheduleConfig_.recurring;
    if (scheduleConfig_.expiresAt.has_value()) {
        scheduleConf["expiresAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            scheduleConfig_.expiresAt->time_since_epoch()).count();
    }
    if (!scheduleConf.empty()) {
        json["scheduleConfig"] = scheduleConf;
    }

    if (!parameters_.empty()) {
        json["parameters"] = parameters_;
    }
    if (!tags_.empty()) {
        json["tags"] = tags_;
    }
    if (concurrencyLimit_.has_value()) {
        json["concurrencyLimit"] = concurrencyLimit_.value();
    }

    return json;
}

JobConfig JobConfig::fromJson(const nlohmann::json& json) {
    JobConfig config;
    
    if (json.contains("jobType")) {
        config.jobType_ = json["jobType"];
    }
    if (json.contains("name")) {
        config.name_ = json["name"];
    }
    if (json.contains("description")) {
        config.description_ = json["description"];
    }
    if (json.contains("timeout")) {
        config.timeout_ = std::chrono::seconds(json["timeout"]);
    }
    if (json.contains("defaultPriority")) {
        config.defaultPriority_ = Job::stringToPriority(json["defaultPriority"]);
    }
    if (json.contains("enabled")) {
        config.enabled_ = json["enabled"];
    }
    if (json.contains("createdAt")) {
        config.createdAt_ = std::chrono::system_clock::time_point{std::chrono::milliseconds{json["createdAt"]}};
    }
    if (json.contains("updatedAt")) {
        config.updatedAt_ = std::chrono::system_clock::time_point{std::chrono::milliseconds{json["updatedAt"]}};
    }

    // Retry policy
    if (json.contains("retryPolicy")) {
        const auto& retryJson = json["retryPolicy"];
        if (retryJson.contains("maxRetries")) {
            config.retryPolicy_.maxRetries = retryJson["maxRetries"];
        }
        if (retryJson.contains("initialDelay")) {
            config.retryPolicy_.initialDelay = std::chrono::seconds(retryJson["initialDelay"]);
        }
        if (retryJson.contains("maxDelay")) {
            config.retryPolicy_.maxDelay = std::chrono::seconds(retryJson["maxDelay"]);
        }
        if (retryJson.contains("backoffMultiplier")) {
            config.retryPolicy_.backoffMultiplier = retryJson["backoffMultiplier"];
        }
        if (retryJson.contains("exponentialBackoff")) {
            config.retryPolicy_.exponentialBackoff = retryJson["exponentialBackoff"];
        }
    }

    // Resource requirements
    if (json.contains("resourceRequirements")) {
        const auto& resourceJson = json["resourceRequirements"];
        if (resourceJson.contains("cpuCores")) {
            config.resourceRequirements_.cpuCores = resourceJson["cpuCores"];
        }
        if (resourceJson.contains("memoryMB")) {
            config.resourceRequirements_.memoryMB = resourceJson["memoryMB"];
        }
        if (resourceJson.contains("diskSpaceMB")) {
            config.resourceRequirements_.diskSpaceMB = resourceJson["diskSpaceMB"];
        }
        if (resourceJson.contains("networkBandwidth")) {
            config.resourceRequirements_.networkBandwidth = resourceJson["networkBandwidth"];
        }
    }

    // Schedule config
    if (json.contains("scheduleConfig")) {
        const auto& scheduleJson = json["scheduleConfig"];
        if (scheduleJson.contains("scheduledAt")) {
            config.scheduleConfig_.scheduledAt = std::chrono::system_clock::time_point{
                std::chrono::milliseconds{scheduleJson["scheduledAt"]}};
        }
        if (scheduleJson.contains("cronExpression")) {
            config.scheduleConfig_.cronExpression = scheduleJson["cronExpression"];
        }
        if (scheduleJson.contains("recurring")) {
            config.scheduleConfig_.recurring = scheduleJson["recurring"];
        }
        if (scheduleJson.contains("expiresAt")) {
            config.scheduleConfig_.expiresAt = std::chrono::system_clock::time_point{
                std::chrono::milliseconds{scheduleJson["expiresAt"]}};
        }
    }

    if (json.contains("parameters")) {
        config.parameters_ = json["parameters"];
    }
    if (json.contains("tags")) {
        config.tags_ = json["tags"];
    }
    if (json.contains("concurrencyLimit")) {
        config.concurrencyLimit_ = json["concurrencyLimit"];
    }

    return config;
}

// Utility methods
std::string JobConfig::toString() const {
    std::stringstream ss;
    ss << "JobConfig[type=" << jobType_ << ", name=" << name_ 
       << ", timeout=" << timeout_.count() << "s, enabled=" << (enabled_ ? "true" : "false") << "]";
    return ss.str();
}

bool JobConfig::operator==(const JobConfig& other) const {
    return jobType_ == other.jobType_ && name_ == other.name_ && description_ == other.description_ &&
           timeout_ == other.timeout_ && defaultPriority_ == other.defaultPriority_ &&
           retryPolicy_ == other.retryPolicy_ && resourceRequirements_ == other.resourceRequirements_ &&
           scheduleConfig_ == other.scheduleConfig_ && parameters_ == other.parameters_ &&
           tags_ == other.tags_ && enabled_ == other.enabled_ && 
           concurrencyLimit_ == other.concurrencyLimit_;
}
