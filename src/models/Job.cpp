#include "../../include/search_engine/models/Job.h"
#include "../../include/Logger.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/oid.hpp>
#include <bsoncxx/types.hpp>
#include <sstream>
#include <iomanip>
#include <random>
#include <thread>

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

// Default constructor
Job::Job()
    : id_(generateJobId()),
      status_(JobStatus::QUEUED),
      priority_(JobPriority::NORMAL),
      progress_(0),
      createdAt_(std::chrono::system_clock::now()),
      retryCount_(0),
      maxRetries_(3) {
}

// Constructor with job type and user ID
Job::Job(const std::string& jobType, const std::string& userId)
    : id_(generateJobId()),
      userId_(userId),
      jobType_(jobType),
      status_(JobStatus::QUEUED),
      priority_(JobPriority::NORMAL),
      progress_(0),
      createdAt_(std::chrono::system_clock::now()),
      retryCount_(0),
      maxRetries_(3) {
}

// Constructor with full parameters
Job::Job(const std::string& id,
         const std::string& userId,
         const std::string& tenantId,
         const std::string& jobType,
         JobStatus status,
         JobPriority priority)
    : id_(id),
      userId_(userId),
      tenantId_(tenantId),
      jobType_(jobType),
      status_(status),
      priority_(priority),
      progress_(0),
      createdAt_(std::chrono::system_clock::now()),
      retryCount_(0),
      maxRetries_(3) {
}

// Business logic methods
void Job::setProgress(int progress) {
    if (progress < 0 || progress > 100) {
        LOG_WARNING("Invalid progress value: " + std::to_string(progress) + ". Must be 0-100.");
        return;
    }
    progress_ = progress;
}

void Job::start() {
    status_ = JobStatus::PROCESSING;
    startedAt_ = std::chrono::system_clock::now();
    LOG_DEBUG("Job " + id_ + " started at " + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(startedAt_->time_since_epoch()).count()));
}

void Job::complete() {
    status_ = JobStatus::COMPLETED;
    completedAt_ = std::chrono::system_clock::now();
    progress_ = 100;
    LOG_INFO("Job " + id_ + " completed successfully");
}

void Job::fail(const std::string& errorMessage) {
    status_ = JobStatus::FAILED;
    completedAt_ = std::chrono::system_clock::now();
    errorMessage_ = errorMessage;
    LOG_ERROR("Job " + id_ + " failed: " + errorMessage);
}

void Job::cancel() {
    status_ = JobStatus::CANCELLED;
    completedAt_ = std::chrono::system_clock::now();
    LOG_INFO("Job " + id_ + " cancelled");
}

bool Job::canRetry() const {
    return status_ == JobStatus::FAILED && retryCount_ < maxRetries_;
}

void Job::incrementRetry() {
    if (canRetry()) {
        retryCount_++;
        status_ = JobStatus::RETRYING;
        LOG_DEBUG("Job " + id_ + " retry attempt " + std::to_string(retryCount_) + 
                  " of " + std::to_string(maxRetries_));
    }
}

bool Job::isExpired() const {
    if (!timeout_.has_value() || !startedAt_.has_value()) {
        return false;
    }
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startedAt_.value());
    return elapsed > timeout_.value();
}

std::chrono::milliseconds Job::getDuration() const {
    if (!startedAt_.has_value()) {
        return std::chrono::milliseconds::zero();
    }
    auto endTime = completedAt_.value_or(std::chrono::system_clock::now());
    return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startedAt_.value());
}

// Validation methods
bool Job::isValid() const {
    return !id_.empty() && !userId_.empty() && !jobType_.empty() && 
           progress_ >= 0 && progress_ <= 100 && retryCount_ >= 0 && maxRetries_ >= 0;
}

Result<bool> Job::validate() const {
    if (id_.empty()) {
        return Result<bool>::Failure("Job ID cannot be empty");
    }
    if (userId_.empty()) {
        return Result<bool>::Failure("User ID cannot be empty");
    }
    if (jobType_.empty()) {
        return Result<bool>::Failure("Job type cannot be empty");
    }
    if (progress_ < 0 || progress_ > 100) {
        return Result<bool>::Failure("Progress must be between 0 and 100");
    }
    if (retryCount_ < 0) {
        return Result<bool>::Failure("Retry count cannot be negative");
    }
    if (maxRetries_ < 0) {
        return Result<bool>::Failure("Max retries cannot be negative");
    }
    return Result<bool>::Success(true, "Job validation passed");
}

// Serialization methods
bsoncxx::document::value Job::toBson() const {
    auto builder = document{};
    
    builder << "id" << id_
            << "userId" << userId_
            << "tenantId" << tenantId_
            << "jobType" << jobType_
            << "status" << statusToString(status_)
            << "priority" << priorityToString(priority_)
            << "progress" << progress_
            << "createdAt" << timePointToBsonDate(createdAt_)
            << "retryCount" << retryCount_
            << "maxRetries" << maxRetries_;

    if (startedAt_.has_value()) {
        builder << "startedAt" << timePointToBsonDate(startedAt_.value());
    }
    if (completedAt_.has_value()) {
        builder << "completedAt" << timePointToBsonDate(completedAt_.value());
    }
    if (scheduledAt_.has_value()) {
        builder << "scheduledAt" << timePointToBsonDate(scheduledAt_.value());
    }
    if (errorMessage_.has_value()) {
        builder << "errorMessage" << errorMessage_.value();
    }
    if (metadata_.has_value()) {
        builder << "metadata" << metadata_.value();
    }
    if (timeout_.has_value()) {
        builder << "timeout" << static_cast<int64_t>(timeout_->count());
    }

    return builder << finalize;
}

Job Job::fromBson(const bsoncxx::document::view& doc) {
    Job job;
    
    if (auto id = doc["id"]) {
        job.id_ = std::string(id.get_string().value);
    }
    if (auto userId = doc["userId"]) {
        job.userId_ = std::string(userId.get_string().value);
    }
    if (auto tenantId = doc["tenantId"]) {
        job.tenantId_ = std::string(tenantId.get_string().value);
    }
    if (auto jobType = doc["jobType"]) {
        job.jobType_ = std::string(jobType.get_string().value);
    }
    if (auto status = doc["status"]) {
        job.status_ = stringToStatus(std::string(status.get_string().value));
    }
    if (auto priority = doc["priority"]) {
        job.priority_ = stringToPriority(std::string(priority.get_string().value));
    }
    if (auto progress = doc["progress"]) {
        job.progress_ = progress.get_int32().value;
    }
    if (auto createdAt = doc["createdAt"]) {
        job.createdAt_ = bsonDateToTimePoint(createdAt.get_date());
    }
    if (auto startedAt = doc["startedAt"]) {
        job.startedAt_ = bsonDateToTimePoint(startedAt.get_date());
    }
    if (auto completedAt = doc["completedAt"]) {
        job.completedAt_ = bsonDateToTimePoint(completedAt.get_date());
    }
    if (auto scheduledAt = doc["scheduledAt"]) {
        job.scheduledAt_ = bsonDateToTimePoint(scheduledAt.get_date());
    }
    if (auto errorMessage = doc["errorMessage"]) {
        job.errorMessage_ = std::string(errorMessage.get_string().value);
    }
    if (auto metadata = doc["metadata"]) {
        job.metadata_ = std::string(metadata.get_string().value);
    }
    if (auto retryCount = doc["retryCount"]) {
        job.retryCount_ = retryCount.get_int32().value;
    }
    if (auto maxRetries = doc["maxRetries"]) {
        job.maxRetries_ = maxRetries.get_int32().value;
    }
    if (auto timeout = doc["timeout"]) {
        job.timeout_ = std::chrono::seconds(timeout.get_int64().value);
    }

    return job;
}

nlohmann::json Job::toJson() const {
    nlohmann::json json;
    
    json["id"] = id_;
    json["userId"] = userId_;
    json["tenantId"] = tenantId_;
    json["jobType"] = jobType_;
    json["status"] = statusToString(status_);
    json["priority"] = priorityToString(priority_);
    json["progress"] = progress_;
    json["createdAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(createdAt_.time_since_epoch()).count();
    json["retryCount"] = retryCount_;
    json["maxRetries"] = maxRetries_;

    if (startedAt_.has_value()) {
        json["startedAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(startedAt_->time_since_epoch()).count();
    }
    if (completedAt_.has_value()) {
        json["completedAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(completedAt_->time_since_epoch()).count();
    }
    if (scheduledAt_.has_value()) {
        json["scheduledAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(scheduledAt_->time_since_epoch()).count();
    }
    if (errorMessage_.has_value()) {
        json["errorMessage"] = errorMessage_.value();
    }
    if (metadata_.has_value()) {
        json["metadata"] = nlohmann::json::parse(metadata_.value());
    }
    if (timeout_.has_value()) {
        json["timeout"] = timeout_->count();
    }

    return json;
}

Job Job::fromJson(const nlohmann::json& json) {
    Job job;
    
    if (json.contains("id")) {
        job.id_ = json["id"];
    }
    if (json.contains("userId")) {
        job.userId_ = json["userId"];
    }
    if (json.contains("tenantId")) {
        job.tenantId_ = json["tenantId"];
    }
    if (json.contains("jobType")) {
        job.jobType_ = json["jobType"];
    }
    if (json.contains("status")) {
        job.status_ = stringToStatus(json["status"]);
    }
    if (json.contains("priority")) {
        job.priority_ = stringToPriority(json["priority"]);
    }
    if (json.contains("progress")) {
        job.progress_ = json["progress"];
    }
    if (json.contains("createdAt")) {
        job.createdAt_ = std::chrono::system_clock::time_point{std::chrono::milliseconds{json["createdAt"]}};
    }
    if (json.contains("startedAt")) {
        job.startedAt_ = std::chrono::system_clock::time_point{std::chrono::milliseconds{json["startedAt"]}};
    }
    if (json.contains("completedAt")) {
        job.completedAt_ = std::chrono::system_clock::time_point{std::chrono::milliseconds{json["completedAt"]}};
    }
    if (json.contains("scheduledAt")) {
        job.scheduledAt_ = std::chrono::system_clock::time_point{std::chrono::milliseconds{json["scheduledAt"]}};
    }
    if (json.contains("errorMessage")) {
        job.errorMessage_ = json["errorMessage"];
    }
    if (json.contains("metadata")) {
        job.metadata_ = json["metadata"].dump();
    }
    if (json.contains("retryCount")) {
        job.retryCount_ = json["retryCount"];
    }
    if (json.contains("maxRetries")) {
        job.maxRetries_ = json["maxRetries"];
    }
    if (json.contains("timeout")) {
        job.timeout_ = std::chrono::seconds(json["timeout"]);
    }

    return job;
}

// Utility methods
std::string Job::toString() const {
    std::stringstream ss;
    ss << "Job[id=" << id_ << ", type=" << jobType_ << ", status=" << statusToString(status_)
       << ", progress=" << progress_ << "%, user=" << userId_ << "]";
    return ss.str();
}

bool Job::operator==(const Job& other) const {
    return id_ == other.id_ && userId_ == other.userId_ && tenantId_ == other.tenantId_ &&
           jobType_ == other.jobType_ && status_ == other.status_ && priority_ == other.priority_ &&
           progress_ == other.progress_ && retryCount_ == other.retryCount_ && maxRetries_ == other.maxRetries_;
}

// Static helper methods
std::string Job::generateJobId() {
    // Generate a UUID-like string using timestamp and random components
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "job_" << std::hex << timestamp;
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

std::string Job::statusToString(JobStatus status) {
    switch (status) {
        case JobStatus::QUEUED: return "queued";
        case JobStatus::PROCESSING: return "processing";
        case JobStatus::COMPLETED: return "completed";
        case JobStatus::FAILED: return "failed";
        case JobStatus::CANCELLED: return "cancelled";
        case JobStatus::RETRYING: return "retrying";
        default: return "unknown";
    }
}

JobStatus Job::stringToStatus(const std::string& status) {
    if (status == "queued") return JobStatus::QUEUED;
    if (status == "processing") return JobStatus::PROCESSING;
    if (status == "completed") return JobStatus::COMPLETED;
    if (status == "failed") return JobStatus::FAILED;
    if (status == "cancelled") return JobStatus::CANCELLED;
    if (status == "retrying") return JobStatus::RETRYING;
    return JobStatus::QUEUED; // default
}

std::string Job::priorityToString(JobPriority priority) {
    switch (priority) {
        case JobPriority::LOW: return "low";
        case JobPriority::NORMAL: return "normal";
        case JobPriority::HIGH: return "high";
        case JobPriority::CRITICAL: return "critical";
        default: return "normal";
    }
}

JobPriority Job::stringToPriority(const std::string& priority) {
    if (priority == "low") return JobPriority::LOW;
    if (priority == "normal") return JobPriority::NORMAL;
    if (priority == "high") return JobPriority::HIGH;
    if (priority == "critical") return JobPriority::CRITICAL;
    return JobPriority::NORMAL; // default
}
