#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <nlohmann/json.hpp>
#include "Job.h"
#include "../../infrastructure.h"

namespace search_engine {
namespace models {

/**
 * Resource requirement specification for jobs
 */
struct ResourceRequirements {
    std::optional<int> cpuCores;        // Number of CPU cores required
    std::optional<size_t> memoryMB;     // Memory requirement in megabytes
    std::optional<size_t> diskSpaceMB;  // Disk space requirement in megabytes
    std::optional<int> networkBandwidth; // Network bandwidth requirement (Mbps)

    ResourceRequirements() = default;
    ResourceRequirements(int cpu, size_t memory, size_t disk = 0, int network = 0)
        : cpuCores(cpu), memoryMB(memory), diskSpaceMB(disk), networkBandwidth(network) {}

    bool operator==(const ResourceRequirements& other) const {
        return cpuCores == other.cpuCores &&
               memoryMB == other.memoryMB &&
               diskSpaceMB == other.diskSpaceMB &&
               networkBandwidth == other.networkBandwidth;
    }
};

/**
 * Retry policy configuration for failed jobs
 */
struct RetryPolicy {
    int maxRetries;                                     // Maximum number of retry attempts
    std::chrono::seconds initialDelay;                  // Initial delay before first retry
    std::chrono::seconds maxDelay;                      // Maximum delay between retries
    double backoffMultiplier;                           // Exponential backoff multiplier
    bool exponentialBackoff;                            // Whether to use exponential backoff

    RetryPolicy()
        : maxRetries(3),
          initialDelay(std::chrono::seconds(30)),
          maxDelay(std::chrono::seconds(3600)),
          backoffMultiplier(2.0),
          exponentialBackoff(true) {}

    RetryPolicy(int retries, std::chrono::seconds initial, std::chrono::seconds maximum, 
                double multiplier = 2.0, bool exponential = true)
        : maxRetries(retries), initialDelay(initial), maxDelay(maximum),
          backoffMultiplier(multiplier), exponentialBackoff(exponential) {}

    // Calculate delay for specific retry attempt
    std::chrono::seconds calculateDelay(int retryAttempt) const;

    bool operator==(const RetryPolicy& other) const {
        return maxRetries == other.maxRetries &&
               initialDelay == other.initialDelay &&
               maxDelay == other.maxDelay &&
               backoffMultiplier == other.backoffMultiplier &&
               exponentialBackoff == other.exponentialBackoff;
    }
};

/**
 * Scheduling configuration for jobs
 */
struct ScheduleConfig {
    std::optional<std::chrono::system_clock::time_point> scheduledAt; // When to execute job
    std::optional<std::string> cronExpression;                        // Cron expression for recurring jobs
    bool recurring;                                                   // Whether job is recurring
    std::optional<std::chrono::system_clock::time_point> expiresAt;   // When job expires

    ScheduleConfig() : recurring(false) {}

    bool isScheduled() const { return scheduledAt.has_value(); }
    bool isRecurring() const { return recurring && cronExpression.has_value(); }
    bool isExpired() const { 
        return expiresAt.has_value() && 
               std::chrono::system_clock::now() > expiresAt.value(); 
    }

    bool operator==(const ScheduleConfig& other) const {
        return scheduledAt == other.scheduledAt &&
               cronExpression == other.cronExpression &&
               recurring == other.recurring &&
               expiresAt == other.expiresAt;
    }
};

/**
 * Job configuration model that defines how jobs should be executed
 * 
 * This class encapsulates all configuration parameters for job execution
 * including timeouts, retry policies, resource requirements, and scheduling.
 */
class JobConfig {
private:
    std::string jobType_;                                   // Type of job (e.g., "crawl", "analysis")
    std::string name_;                                      // Human-readable job name
    std::string description_;                               // Job description
    std::chrono::seconds timeout_;                          // Job execution timeout
    JobPriority defaultPriority_;                          // Default priority level
    RetryPolicy retryPolicy_;                              // Retry configuration
    ResourceRequirements resourceRequirements_;            // Resource requirements
    ScheduleConfig scheduleConfig_;                        // Scheduling configuration
    std::unordered_map<std::string, std::string> parameters_; // Job-specific parameters
    std::unordered_map<std::string, std::string> tags_;    // Metadata tags
    bool enabled_;                                          // Whether job type is enabled
    std::optional<int> concurrencyLimit_;                  // Max concurrent jobs of this type
    std::chrono::system_clock::time_point createdAt_;      // Configuration creation time
    std::chrono::system_clock::time_point updatedAt_;      // Last update time

public:
    /**
     * Default constructor
     */
    JobConfig();

    /**
     * Constructor with job type
     */
    explicit JobConfig(const std::string& jobType);

    /**
     * Full constructor
     */
    JobConfig(const std::string& jobType,
              const std::string& name,
              std::chrono::seconds timeout,
              JobPriority defaultPriority = JobPriority::NORMAL,
              const RetryPolicy& retryPolicy = RetryPolicy{});

    // Copy and move constructors/assignment operators
    JobConfig(const JobConfig& other) = default;
    JobConfig& operator=(const JobConfig& other) = default;
    JobConfig(JobConfig&& other) noexcept = default;
    JobConfig& operator=(JobConfig&& other) noexcept = default;

    // Destructor
    ~JobConfig() = default;

    // Getters
    const std::string& getJobType() const { return jobType_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    const std::chrono::seconds& getTimeout() const { return timeout_; }
    JobPriority getDefaultPriority() const { return defaultPriority_; }
    const RetryPolicy& getRetryPolicy() const { return retryPolicy_; }
    const ResourceRequirements& getResourceRequirements() const { return resourceRequirements_; }
    const ScheduleConfig& getScheduleConfig() const { return scheduleConfig_; }
    const std::unordered_map<std::string, std::string>& getParameters() const { return parameters_; }
    const std::unordered_map<std::string, std::string>& getTags() const { return tags_; }
    bool isEnabled() const { return enabled_; }
    const std::optional<int>& getConcurrencyLimit() const { return concurrencyLimit_; }
    const std::chrono::system_clock::time_point& getCreatedAt() const { return createdAt_; }
    const std::chrono::system_clock::time_point& getUpdatedAt() const { return updatedAt_; }

    // Setters
    void setJobType(const std::string& jobType) { jobType_ = jobType; updateTimestamp(); }
    void setName(const std::string& name) { name_ = name; updateTimestamp(); }
    void setDescription(const std::string& description) { description_ = description; updateTimestamp(); }
    void setTimeout(const std::chrono::seconds& timeout) { timeout_ = timeout; updateTimestamp(); }
    void setDefaultPriority(JobPriority priority) { defaultPriority_ = priority; updateTimestamp(); }
    void setRetryPolicy(const RetryPolicy& policy) { retryPolicy_ = policy; updateTimestamp(); }
    void setResourceRequirements(const ResourceRequirements& requirements) { 
        resourceRequirements_ = requirements; updateTimestamp(); 
    }
    void setScheduleConfig(const ScheduleConfig& config) { scheduleConfig_ = config; updateTimestamp(); }
    void setEnabled(bool enabled) { enabled_ = enabled; updateTimestamp(); }
    void setConcurrencyLimit(int limit) { concurrencyLimit_ = limit; updateTimestamp(); }

    // Parameter management
    void setParameter(const std::string& key, const std::string& value);
    std::optional<std::string> getParameter(const std::string& key) const;
    void removeParameter(const std::string& key);
    bool hasParameter(const std::string& key) const;

    // Tag management
    void setTag(const std::string& key, const std::string& value);
    std::optional<std::string> getTag(const std::string& key) const;
    void removeTag(const std::string& key);
    bool hasTag(const std::string& key) const;

    // Business logic methods
    Job createJob(const std::string& userId, const std::string& tenantId = "") const;
    bool canExecuteNow() const;
    std::chrono::seconds getNextRetryDelay(int retryAttempt) const;
    bool meetsResourceRequirements(const ResourceRequirements& available) const;

    // Validation methods
    bool isValid() const;
    Result<bool> validate() const;

    // Serialization methods
    bsoncxx::document::value toBson() const;
    static JobConfig fromBson(const bsoncxx::document::view& doc);
    nlohmann::json toJson() const;
    static JobConfig fromJson(const nlohmann::json& json);

    // Utility methods
    std::string toString() const;
    bool operator==(const JobConfig& other) const;
    bool operator!=(const JobConfig& other) const { return !(*this == other); }

private:
    void updateTimestamp() { updatedAt_ = std::chrono::system_clock::now(); }
};

} // namespace models
} // namespace search_engine
