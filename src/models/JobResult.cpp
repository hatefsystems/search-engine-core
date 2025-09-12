#include "../../include/search_engine/models/JobResult.h"
#include "../../include/Logger.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <random>

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

// JobResult implementation
JobResult::JobResult()
    : id_(generateResultId()),
      finalStatus_(JobStatus::QUEUED),
      createdAt_(std::chrono::system_clock::now()) {
}

JobResult::JobResult(const std::string& jobId)
    : id_(generateResultId()),
      jobId_(jobId),
      finalStatus_(JobStatus::QUEUED),
      createdAt_(std::chrono::system_clock::now()) {
}

JobResult::JobResult(const std::string& jobId, const std::string& userId, 
                     const std::string& tenantId, JobStatus finalStatus)
    : id_(generateResultId()),
      jobId_(jobId),
      userId_(userId),
      tenantId_(tenantId),
      finalStatus_(finalStatus),
      createdAt_(std::chrono::system_clock::now()) {
}

// Result data management
void JobResult::setResultDataJson(const nlohmann::json& json) {
    resultData_ = json.dump();
}

std::optional<nlohmann::json> JobResult::getResultDataJson() const {
    if (!resultData_.has_value()) {
        return std::nullopt;
    }
    
    try {
        return nlohmann::json::parse(resultData_.value());
    } catch (const nlohmann::json::parse_error& e) {
        LOG_ERROR("Failed to parse result data as JSON: " + std::string(e.what()));
        return std::nullopt;
    }
}

// Error management
void JobResult::setError(const std::string& errorCode, const std::string& errorMessage) {
    error_ = JobError{errorCode, errorMessage};
    finalStatus_ = JobStatus::FAILED;
}

void JobResult::setError(const std::string& errorCode, const std::string& errorMessage, 
                         const std::string& stackTrace) {
    JobError jobError{errorCode, errorMessage};
    jobError.stackTrace = stackTrace;
    error_ = jobError;
    finalStatus_ = JobStatus::FAILED;
}

// Output file management
void JobResult::addOutputFile(const OutputFile& file) {
    // Check if file already exists and update or add
    auto it = std::find_if(outputFiles_.begin(), outputFiles_.end(),
                          [&file](const OutputFile& existing) {
                              return existing.filename == file.filename;
                          });
    
    if (it != outputFiles_.end()) {
        *it = file; // Update existing
        LOG_DEBUG("Updated output file: " + file.filename);
    } else {
        outputFiles_.push_back(file); // Add new
        LOG_DEBUG("Added output file: " + file.filename + " (" + std::to_string(file.fileSize) + " bytes)");
    }
}

void JobResult::addOutputFile(const std::string& filename, const std::string& filepath, 
                             const std::string& mimeType, size_t fileSize) {
    OutputFile file{filename, filepath, mimeType, fileSize};
    addOutputFile(file);
}

void JobResult::removeOutputFile(const std::string& filename) {
    auto it = std::remove_if(outputFiles_.begin(), outputFiles_.end(),
                            [&filename](const OutputFile& file) {
                                return file.filename == filename;
                            });
    
    if (it != outputFiles_.end()) {
        outputFiles_.erase(it, outputFiles_.end());
        LOG_DEBUG("Removed output file: " + filename);
    }
}

std::optional<OutputFile> JobResult::getOutputFile(const std::string& filename) const {
    auto it = std::find_if(outputFiles_.begin(), outputFiles_.end(),
                          [&filename](const OutputFile& file) {
                              return file.filename == filename;
                          });
    
    if (it != outputFiles_.end()) {
        return *it;
    }
    return std::nullopt;
}

size_t JobResult::getTotalOutputSize() const {
    return std::accumulate(outputFiles_.begin(), outputFiles_.end(), size_t{0},
                          [](size_t sum, const OutputFile& file) {
                              return sum + file.fileSize;
                          });
}

// Metadata management
void JobResult::setMetadata(const std::string& key, const std::string& value) {
    metadata_[key] = value;
}

std::optional<std::string> JobResult::getMetadata(const std::string& key) const {
    auto it = metadata_.find(key);
    if (it != metadata_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void JobResult::removeMetadata(const std::string& key) {
    metadata_.erase(key);
}

bool JobResult::hasMetadata(const std::string& key) const {
    return metadata_.find(key) != metadata_.end();
}

// Log management
void JobResult::addLogMessage(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "[" << timestamp << "] " << message;
    logMessages_.push_back(ss.str());
    
    // Limit log messages to prevent excessive memory usage (keep last 1000)
    if (logMessages_.size() > 1000) {
        logMessages_.erase(logMessages_.begin(), logMessages_.begin() + (logMessages_.size() - 1000));
    }
}

void JobResult::addLogMessages(const std::vector<std::string>& messages) {
    for (const auto& message : messages) {
        addLogMessage(message);
    }
}

// Metrics management
void JobResult::addCustomMetric(const std::string& name, double value) {
    metrics_.customMetrics[name] = value;
}

std::optional<double> JobResult::getCustomMetric(const std::string& name) const {
    auto it = metrics_.customMetrics.find(name);
    if (it != metrics_.customMetrics.end()) {
        return it->second;
    }
    return std::nullopt;
}

// Business logic methods
bool JobResult::isExpired() const {
    if (!expiresAt_.has_value()) {
        return false;
    }
    return std::chrono::system_clock::now() > expiresAt_.value();
}

std::chrono::milliseconds JobResult::getAge() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - createdAt_);
}

// Validation methods
bool JobResult::isValid() const {
    return !id_.empty() && !jobId_.empty();
}

Result<bool> JobResult::validate() const {
    if (id_.empty()) {
        return Result<bool>::Failure("Result ID cannot be empty");
    }
    if (jobId_.empty()) {
        return Result<bool>::Failure("Job ID cannot be empty");
    }
    
    // Validate output files
    for (const auto& file : outputFiles_) {
        if (file.filename.empty()) {
            return Result<bool>::Failure("Output file name cannot be empty");
        }
        if (file.filepath.empty()) {
            return Result<bool>::Failure("Output file path cannot be empty");
        }
    }
    
    // Validate JSON result data if present
    if (resultData_.has_value()) {
        try {
            auto parsed = nlohmann::json::parse(resultData_.value());
            (void)parsed; // Suppress unused variable warning
        } catch (const nlohmann::json::parse_error& e) {
            return Result<bool>::Failure("Invalid JSON in result data: " + std::string(e.what()));
        }
    }
    
    return Result<bool>::Success(true, "JobResult validation passed");
}

// Serialization methods
bsoncxx::document::value JobResult::toBson() const {
    auto builder = document{};
    
    builder << "id" << id_
            << "jobId" << jobId_
            << "userId" << userId_
            << "tenantId" << tenantId_
            << "finalStatus" << Job::statusToString(finalStatus_)
            << "createdAt" << timePointToBsonDate(createdAt_);

    if (resultData_.has_value()) {
        builder << "resultData" << resultData_.value();
    }

    // Error information
    if (error_.has_value()) {
        auto errorBuilder = document{};
        errorBuilder << "errorCode" << error_->errorCode
                     << "errorMessage" << error_->errorMessage
                     << "timestamp" << timePointToBsonDate(error_->timestamp);
        
        if (error_->stackTrace.has_value()) {
            errorBuilder << "stackTrace" << error_->stackTrace.value();
        }
        if (error_->errorCategory.has_value()) {
            errorBuilder << "errorCategory" << error_->errorCategory.value();
        }
        if (error_->httpStatusCode.has_value()) {
            errorBuilder << "httpStatusCode" << error_->httpStatusCode.value();
        }
        
        // Error context
        if (!error_->context.empty()) {
            auto contextBuilder = document{};
            for (const auto& [key, value] : error_->context) {
                contextBuilder << key << value;
            }
            errorBuilder << "context" << contextBuilder;
        }
        
        builder << "error" << errorBuilder;
    }

    // Metrics
    auto metricsBuilder = document{};
    metricsBuilder << "executionDuration" << static_cast<int64_t>(metrics_.executionDuration.count());
    
    if (metrics_.peakMemoryUsage.has_value()) {
        metricsBuilder << "peakMemoryUsage" << static_cast<int64_t>(metrics_.peakMemoryUsage.value());
    }
    if (metrics_.cpuUsage.has_value()) {
        metricsBuilder << "cpuUsage" << metrics_.cpuUsage.value();
    }
    if (metrics_.networkBytesReceived.has_value()) {
        metricsBuilder << "networkBytesReceived" << static_cast<int64_t>(metrics_.networkBytesReceived.value());
    }
    if (metrics_.networkBytesSent.has_value()) {
        metricsBuilder << "networkBytesSent" << static_cast<int64_t>(metrics_.networkBytesSent.value());
    }
    if (metrics_.diskBytesRead.has_value()) {
        metricsBuilder << "diskBytesRead" << static_cast<int64_t>(metrics_.diskBytesRead.value());
    }
    if (metrics_.diskBytesWritten.has_value()) {
        metricsBuilder << "diskBytesWritten" << static_cast<int64_t>(metrics_.diskBytesWritten.value());
    }
    if (metrics_.itemsProcessed.has_value()) {
        metricsBuilder << "itemsProcessed" << metrics_.itemsProcessed.value();
    }
    if (metrics_.throughput.has_value()) {
        metricsBuilder << "throughput" << metrics_.throughput.value();
    }
    
    // Custom metrics
    if (!metrics_.customMetrics.empty()) {
        auto customBuilder = document{};
        for (const auto& [key, value] : metrics_.customMetrics) {
            customBuilder << key << value;
        }
        metricsBuilder << "customMetrics" << customBuilder;
    }
    
    builder << "metrics" << metricsBuilder;

    // Output files
    if (!outputFiles_.empty()) {
        auto filesArray = array{};
        for (const auto& file : outputFiles_) {
            auto fileBuilder = document{};
            fileBuilder << "filename" << file.filename
                        << "filepath" << file.filepath
                        << "mimeType" << file.mimeType
                        << "fileSize" << static_cast<int64_t>(file.fileSize)
                        << "createdAt" << timePointToBsonDate(file.createdAt);
            
            if (file.checksum.has_value()) {
                fileBuilder << "checksum" << file.checksum.value();
            }
            if (file.description.has_value()) {
                fileBuilder << "description" << file.description.value();
            }
            
            // File metadata
            if (!file.metadata.empty()) {
                auto fileMetaBuilder = document{};
                for (const auto& [key, value] : file.metadata) {
                    fileMetaBuilder << key << value;
                }
                fileBuilder << "metadata" << fileMetaBuilder;
            }
            
            filesArray << fileBuilder;
        }
        builder << "outputFiles" << filesArray;
    }

    // Metadata
    if (!metadata_.empty()) {
        auto metaBuilder = document{};
        for (const auto& [key, value] : metadata_) {
            metaBuilder << key << value;
        }
        builder << "metadata" << metaBuilder;
    }

    // Log messages
    if (!logMessages_.empty()) {
        auto logsArray = array{};
        for (const auto& message : logMessages_) {
            logsArray << message;
        }
        builder << "logMessages" << logsArray;
    }

    if (expiresAt_.has_value()) {
        builder << "expiresAt" << timePointToBsonDate(expiresAt_.value());
    }

    return builder << finalize;
}

JobResult JobResult::fromBson(const bsoncxx::document::view& doc) {
    JobResult result;
    
    if (auto id = doc["id"]) {
        result.id_ = std::string(id.get_string().value);
    }
    if (auto jobId = doc["jobId"]) {
        result.jobId_ = std::string(jobId.get_string().value);
    }
    if (auto userId = doc["userId"]) {
        result.userId_ = std::string(userId.get_string().value);
    }
    if (auto tenantId = doc["tenantId"]) {
        result.tenantId_ = std::string(tenantId.get_string().value);
    }
    if (auto finalStatus = doc["finalStatus"]) {
        result.finalStatus_ = Job::stringToStatus(std::string(finalStatus.get_string().value));
    }
    if (auto createdAt = doc["createdAt"]) {
        result.createdAt_ = bsonDateToTimePoint(createdAt.get_date());
    }
    if (auto resultData = doc["resultData"]) {
        result.resultData_ = std::string(resultData.get_string().value);
    }

    // Error information
    if (auto error = doc["error"]) {
        auto errorDoc = error.get_document().value;
        JobError jobError;
        
        if (auto errorCode = errorDoc["errorCode"]) {
            jobError.errorCode = std::string(errorCode.get_string().value);
        }
        if (auto errorMessage = errorDoc["errorMessage"]) {
            jobError.errorMessage = std::string(errorMessage.get_string().value);
        }
        if (auto timestamp = errorDoc["timestamp"]) {
            jobError.timestamp = bsonDateToTimePoint(timestamp.get_date());
        }
        if (auto stackTrace = errorDoc["stackTrace"]) {
            jobError.stackTrace = std::string(stackTrace.get_string().value);
        }
        if (auto errorCategory = errorDoc["errorCategory"]) {
            jobError.errorCategory = std::string(errorCategory.get_string().value);
        }
        if (auto httpStatusCode = errorDoc["httpStatusCode"]) {
            jobError.httpStatusCode = httpStatusCode.get_int32().value;
        }
        if (auto context = errorDoc["context"]) {
            auto contextDoc = context.get_document().value;
            for (auto element : contextDoc) {
                jobError.context[std::string(element.key())] = std::string(element.get_string().value);
            }
        }
        
        result.error_ = jobError;
    }

    // Metrics
    if (auto metrics = doc["metrics"]) {
        auto metricsDoc = metrics.get_document().value;
        
        if (auto executionDuration = metricsDoc["executionDuration"]) {
            result.metrics_.executionDuration = std::chrono::milliseconds(executionDuration.get_int64().value);
        }
        if (auto peakMemoryUsage = metricsDoc["peakMemoryUsage"]) {
            result.metrics_.peakMemoryUsage = static_cast<size_t>(peakMemoryUsage.get_int64().value);
        }
        if (auto cpuUsage = metricsDoc["cpuUsage"]) {
            result.metrics_.cpuUsage = cpuUsage.get_double().value;
        }
        if (auto networkBytesReceived = metricsDoc["networkBytesReceived"]) {
            result.metrics_.networkBytesReceived = static_cast<size_t>(networkBytesReceived.get_int64().value);
        }
        if (auto networkBytesSent = metricsDoc["networkBytesSent"]) {
            result.metrics_.networkBytesSent = static_cast<size_t>(networkBytesSent.get_int64().value);
        }
        if (auto diskBytesRead = metricsDoc["diskBytesRead"]) {
            result.metrics_.diskBytesRead = static_cast<size_t>(diskBytesRead.get_int64().value);
        }
        if (auto diskBytesWritten = metricsDoc["diskBytesWritten"]) {
            result.metrics_.diskBytesWritten = static_cast<size_t>(diskBytesWritten.get_int64().value);
        }
        if (auto itemsProcessed = metricsDoc["itemsProcessed"]) {
            result.metrics_.itemsProcessed = itemsProcessed.get_int32().value;
        }
        if (auto throughput = metricsDoc["throughput"]) {
            result.metrics_.throughput = throughput.get_double().value;
        }
        
        // Custom metrics
        if (auto customMetrics = metricsDoc["customMetrics"]) {
            auto customDoc = customMetrics.get_document().value;
            for (auto element : customDoc) {
                result.metrics_.customMetrics[std::string(element.key())] = element.get_double().value;
            }
        }
    }

    // Output files
    if (auto outputFiles = doc["outputFiles"]) {
        auto filesArray = outputFiles.get_array().value;
        for (auto fileElement : filesArray) {
            auto fileDoc = fileElement.get_document().value;
            OutputFile file;
            
            if (auto filename = fileDoc["filename"]) {
                file.filename = std::string(filename.get_string().value);
            }
            if (auto filepath = fileDoc["filepath"]) {
                file.filepath = std::string(filepath.get_string().value);
            }
            if (auto mimeType = fileDoc["mimeType"]) {
                file.mimeType = std::string(mimeType.get_string().value);
            }
            if (auto fileSize = fileDoc["fileSize"]) {
                file.fileSize = static_cast<size_t>(fileSize.get_int64().value);
            }
            if (auto createdAt = fileDoc["createdAt"]) {
                file.createdAt = bsonDateToTimePoint(createdAt.get_date());
            }
            if (auto checksum = fileDoc["checksum"]) {
                file.checksum = std::string(checksum.get_string().value);
            }
            if (auto description = fileDoc["description"]) {
                file.description = std::string(description.get_string().value);
            }
            
            // File metadata
            if (auto fileMetadata = fileDoc["metadata"]) {
                auto metaDoc = fileMetadata.get_document().value;
                for (auto element : metaDoc) {
                    file.metadata[std::string(element.key())] = std::string(element.get_string().value);
                }
            }
            
            result.outputFiles_.push_back(file);
        }
    }

    // Metadata
    if (auto metadata = doc["metadata"]) {
        auto metaDoc = metadata.get_document().value;
        for (auto element : metaDoc) {
            result.metadata_[std::string(element.key())] = std::string(element.get_string().value);
        }
    }

    // Log messages
    if (auto logMessages = doc["logMessages"]) {
        auto logsArray = logMessages.get_array().value;
        for (auto logElement : logsArray) {
            result.logMessages_.push_back(std::string(logElement.get_string().value));
        }
    }

    if (auto expiresAt = doc["expiresAt"]) {
        result.expiresAt_ = bsonDateToTimePoint(expiresAt.get_date());
    }

    return result;
}

nlohmann::json JobResult::toJson() const {
    nlohmann::json json;
    
    json["id"] = id_;
    json["jobId"] = jobId_;
    json["userId"] = userId_;
    json["tenantId"] = tenantId_;
    json["finalStatus"] = Job::statusToString(finalStatus_);
    json["createdAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(createdAt_.time_since_epoch()).count();

    if (resultData_.has_value()) {
        try {
            json["resultData"] = nlohmann::json::parse(resultData_.value());
        } catch (const nlohmann::json::parse_error&) {
            json["resultData"] = resultData_.value(); // Store as string if not valid JSON
        }
    }

    // Error information
    if (error_.has_value()) {
        nlohmann::json errorJson;
        errorJson["errorCode"] = error_->errorCode;
        errorJson["errorMessage"] = error_->errorMessage;
        errorJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            error_->timestamp.time_since_epoch()).count();
        
        if (error_->stackTrace.has_value()) {
            errorJson["stackTrace"] = error_->stackTrace.value();
        }
        if (error_->errorCategory.has_value()) {
            errorJson["errorCategory"] = error_->errorCategory.value();
        }
        if (error_->httpStatusCode.has_value()) {
            errorJson["httpStatusCode"] = error_->httpStatusCode.value();
        }
        if (!error_->context.empty()) {
            errorJson["context"] = error_->context;
        }
        
        json["error"] = errorJson;
    }

    // Metrics
    nlohmann::json metricsJson;
    metricsJson["executionDuration"] = metrics_.executionDuration.count();
    
    if (metrics_.peakMemoryUsage.has_value()) {
        metricsJson["peakMemoryUsage"] = metrics_.peakMemoryUsage.value();
    }
    if (metrics_.cpuUsage.has_value()) {
        metricsJson["cpuUsage"] = metrics_.cpuUsage.value();
    }
    if (metrics_.networkBytesReceived.has_value()) {
        metricsJson["networkBytesReceived"] = metrics_.networkBytesReceived.value();
    }
    if (metrics_.networkBytesSent.has_value()) {
        metricsJson["networkBytesSent"] = metrics_.networkBytesSent.value();
    }
    if (metrics_.diskBytesRead.has_value()) {
        metricsJson["diskBytesRead"] = metrics_.diskBytesRead.value();
    }
    if (metrics_.diskBytesWritten.has_value()) {
        metricsJson["diskBytesWritten"] = metrics_.diskBytesWritten.value();
    }
    if (metrics_.itemsProcessed.has_value()) {
        metricsJson["itemsProcessed"] = metrics_.itemsProcessed.value();
    }
    if (metrics_.throughput.has_value()) {
        metricsJson["throughput"] = metrics_.throughput.value();
    }
    if (!metrics_.customMetrics.empty()) {
        metricsJson["customMetrics"] = metrics_.customMetrics;
    }
    
    json["metrics"] = metricsJson;

    // Output files
    if (!outputFiles_.empty()) {
        nlohmann::json filesJson = nlohmann::json::array();
        for (const auto& file : outputFiles_) {
            nlohmann::json fileJson;
            fileJson["filename"] = file.filename;
            fileJson["filepath"] = file.filepath;
            fileJson["mimeType"] = file.mimeType;
            fileJson["fileSize"] = file.fileSize;
            fileJson["createdAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                file.createdAt.time_since_epoch()).count();
            
            if (file.checksum.has_value()) {
                fileJson["checksum"] = file.checksum.value();
            }
            if (file.description.has_value()) {
                fileJson["description"] = file.description.value();
            }
            if (!file.metadata.empty()) {
                fileJson["metadata"] = file.metadata;
            }
            
            filesJson.push_back(fileJson);
        }
        json["outputFiles"] = filesJson;
    }

    if (!metadata_.empty()) {
        json["metadata"] = metadata_;
    }
    if (!logMessages_.empty()) {
        json["logMessages"] = logMessages_;
    }
    if (expiresAt_.has_value()) {
        json["expiresAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            expiresAt_->time_since_epoch()).count();
    }

    return json;
}

JobResult JobResult::fromJson(const nlohmann::json& json) {
    JobResult result;
    
    if (json.contains("id")) {
        result.id_ = json["id"];
    }
    if (json.contains("jobId")) {
        result.jobId_ = json["jobId"];
    }
    if (json.contains("userId")) {
        result.userId_ = json["userId"];
    }
    if (json.contains("tenantId")) {
        result.tenantId_ = json["tenantId"];
    }
    if (json.contains("finalStatus")) {
        result.finalStatus_ = Job::stringToStatus(json["finalStatus"]);
    }
    if (json.contains("createdAt")) {
        result.createdAt_ = std::chrono::system_clock::time_point{std::chrono::milliseconds{json["createdAt"]}};
    }
    if (json.contains("resultData")) {
        if (json["resultData"].is_string()) {
            result.resultData_ = json["resultData"];
        } else {
            result.resultData_ = json["resultData"].dump();
        }
    }

    // Error information
    if (json.contains("error")) {
        const auto& errorJson = json["error"];
        JobError jobError;
        
        if (errorJson.contains("errorCode")) {
            jobError.errorCode = errorJson["errorCode"];
        }
        if (errorJson.contains("errorMessage")) {
            jobError.errorMessage = errorJson["errorMessage"];
        }
        if (errorJson.contains("timestamp")) {
            jobError.timestamp = std::chrono::system_clock::time_point{
                std::chrono::milliseconds{errorJson["timestamp"]}};
        }
        if (errorJson.contains("stackTrace")) {
            jobError.stackTrace = errorJson["stackTrace"];
        }
        if (errorJson.contains("errorCategory")) {
            jobError.errorCategory = errorJson["errorCategory"];
        }
        if (errorJson.contains("httpStatusCode")) {
            jobError.httpStatusCode = errorJson["httpStatusCode"];
        }
        if (errorJson.contains("context")) {
            jobError.context = errorJson["context"];
        }
        
        result.error_ = jobError;
    }

    // Metrics
    if (json.contains("metrics")) {
        const auto& metricsJson = json["metrics"];
        
        if (metricsJson.contains("executionDuration")) {
            result.metrics_.executionDuration = std::chrono::milliseconds(metricsJson["executionDuration"]);
        }
        if (metricsJson.contains("peakMemoryUsage")) {
            result.metrics_.peakMemoryUsage = metricsJson["peakMemoryUsage"];
        }
        if (metricsJson.contains("cpuUsage")) {
            result.metrics_.cpuUsage = metricsJson["cpuUsage"];
        }
        if (metricsJson.contains("networkBytesReceived")) {
            result.metrics_.networkBytesReceived = metricsJson["networkBytesReceived"];
        }
        if (metricsJson.contains("networkBytesSent")) {
            result.metrics_.networkBytesSent = metricsJson["networkBytesSent"];
        }
        if (metricsJson.contains("diskBytesRead")) {
            result.metrics_.diskBytesRead = metricsJson["diskBytesRead"];
        }
        if (metricsJson.contains("diskBytesWritten")) {
            result.metrics_.diskBytesWritten = metricsJson["diskBytesWritten"];
        }
        if (metricsJson.contains("itemsProcessed")) {
            result.metrics_.itemsProcessed = metricsJson["itemsProcessed"];
        }
        if (metricsJson.contains("throughput")) {
            result.metrics_.throughput = metricsJson["throughput"];
        }
        if (metricsJson.contains("customMetrics")) {
            result.metrics_.customMetrics = metricsJson["customMetrics"];
        }
    }

    // Output files
    if (json.contains("outputFiles")) {
        for (const auto& fileJson : json["outputFiles"]) {
            OutputFile file;
            
            if (fileJson.contains("filename")) {
                file.filename = fileJson["filename"];
            }
            if (fileJson.contains("filepath")) {
                file.filepath = fileJson["filepath"];
            }
            if (fileJson.contains("mimeType")) {
                file.mimeType = fileJson["mimeType"];
            }
            if (fileJson.contains("fileSize")) {
                file.fileSize = fileJson["fileSize"];
            }
            if (fileJson.contains("createdAt")) {
                file.createdAt = std::chrono::system_clock::time_point{
                    std::chrono::milliseconds{fileJson["createdAt"]}};
            }
            if (fileJson.contains("checksum")) {
                file.checksum = fileJson["checksum"];
            }
            if (fileJson.contains("description")) {
                file.description = fileJson["description"];
            }
            if (fileJson.contains("metadata")) {
                file.metadata = fileJson["metadata"];
            }
            
            result.outputFiles_.push_back(file);
        }
    }

    if (json.contains("metadata")) {
        result.metadata_ = json["metadata"];
    }
    if (json.contains("logMessages")) {
        const auto& logMessages = json["logMessages"];
        if (logMessages.is_array()) {
            result.logMessages_.clear();
            for (const auto& logMessage : logMessages) {
                result.logMessages_.push_back(logMessage.get<std::string>());
            }
        }
    }
    if (json.contains("expiresAt")) {
        result.expiresAt_ = std::chrono::system_clock::time_point{
            std::chrono::milliseconds{json["expiresAt"]}};
    }

    return result;
}

// Utility methods
std::string JobResult::toString() const {
    std::stringstream ss;
    ss << "JobResult[id=" << id_ << ", jobId=" << jobId_ 
       << ", status=" << Job::statusToString(finalStatus_) << ", files=" << outputFiles_.size() << "]";
    return ss.str();
}

std::string JobResult::getSummary() const {
    std::stringstream ss;
    ss << "Job " << jobId_ << " " << Job::statusToString(finalStatus_);
    
    if (isSuccess()) {
        ss << " (" << outputFiles_.size() << " files, " << getTotalOutputSize() << " bytes)";
    } else if (hasError()) {
        ss << " (Error: " << error_->errorCode << ")";
    }
    
    ss << " Duration: " << metrics_.executionDuration.count() << "ms";
    return ss.str();
}

bool JobResult::operator==(const JobResult& other) const {
    return id_ == other.id_ && jobId_ == other.jobId_ && userId_ == other.userId_ &&
           tenantId_ == other.tenantId_ && finalStatus_ == other.finalStatus_ &&
           resultData_ == other.resultData_ && error_ == other.error_ &&
           metrics_ == other.metrics_ && outputFiles_ == other.outputFiles_ &&
           metadata_ == other.metadata_ && logMessages_ == other.logMessages_;
}

// Static helper methods
std::string JobResult::generateResultId() {
    // Generate a UUID-like string using timestamp and random components
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "result_" << std::hex << timestamp;
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

JobResult JobResult::createFromJob(const Job& job, JobStatus finalStatus) {
    JobResult result{job.getId(), job.getUserId(), job.getTenantId(), finalStatus};
    
    // Copy metrics from job duration
    result.metrics_.executionDuration = job.getDuration();
    
    return result;
}

JobResult JobResult::createSuccessResult(const Job& job, const std::string& resultData) {
    JobResult result = createFromJob(job, JobStatus::COMPLETED);
    
    if (!resultData.empty()) {
        result.resultData_ = resultData;
    }
    
    LOG_INFO("Created success result for job: " + job.getId());
    return result;
}

JobResult JobResult::createFailureResult(const Job& job, const std::string& errorCode, 
                                        const std::string& errorMessage) {
    JobResult result = createFromJob(job, JobStatus::FAILED);
    result.setError(errorCode, errorMessage);
    
    LOG_ERROR("Created failure result for job: " + job.getId() + " - " + errorCode + ": " + errorMessage);
    return result;
}
