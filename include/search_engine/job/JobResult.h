#pragma once

#include <string>
#include <chrono>
#include <optional>
#include <vector>
#include <unordered_map>
#include <memory>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <nlohmann/json.hpp>
#include "Job.h"
#include "../../infrastructure.h"

namespace search_engine {
namespace models {

/**
 * Performance metrics collected during job execution
 */
struct JobMetrics {
    std::chrono::milliseconds executionDuration;    // Total execution time
    std::optional<size_t> peakMemoryUsage;         // Peak memory usage in bytes
    std::optional<double> cpuUsage;                // Average CPU usage percentage
    std::optional<size_t> networkBytesReceived;    // Network bytes received
    std::optional<size_t> networkBytesSent;        // Network bytes sent
    std::optional<size_t> diskBytesRead;           // Disk bytes read
    std::optional<size_t> diskBytesWritten;        // Disk bytes written
    std::optional<int> itemsProcessed;             // Number of items processed
    std::optional<double> throughput;              // Items per second
    std::unordered_map<std::string, double> customMetrics; // Custom metrics

    JobMetrics() : executionDuration(std::chrono::milliseconds::zero()) {}

    bool operator==(const JobMetrics& other) const {
        return executionDuration == other.executionDuration &&
               peakMemoryUsage == other.peakMemoryUsage &&
               cpuUsage == other.cpuUsage &&
               networkBytesReceived == other.networkBytesReceived &&
               networkBytesSent == other.networkBytesSent &&
               diskBytesRead == other.diskBytesRead &&
               diskBytesWritten == other.diskBytesWritten &&
               itemsProcessed == other.itemsProcessed &&
               throughput == other.throughput &&
               customMetrics == other.customMetrics;
    }
};

/**
 * Error information for failed jobs
 */
struct JobError {
    std::string errorCode;                         // Error code identifier
    std::string errorMessage;                      // Human-readable error message
    std::optional<std::string> stackTrace;         // Stack trace if available
    std::optional<std::string> errorCategory;      // Error category (e.g., "network", "database")
    std::optional<int> httpStatusCode;             // HTTP status code if applicable
    std::unordered_map<std::string, std::string> context; // Additional error context
    std::chrono::system_clock::time_point timestamp; // When error occurred

    JobError() : timestamp(std::chrono::system_clock::now()) {}
    
    JobError(const std::string& code, const std::string& message)
        : errorCode(code), errorMessage(message), timestamp(std::chrono::system_clock::now()) {}

    bool operator==(const JobError& other) const {
        return errorCode == other.errorCode &&
               errorMessage == other.errorMessage &&
               stackTrace == other.stackTrace &&
               errorCategory == other.errorCategory &&
               httpStatusCode == other.httpStatusCode &&
               context == other.context;
    }
};

/**
 * Output file reference for job results
 */
struct OutputFile {
    std::string filename;                          // File name
    std::string filepath;                          // Full file path
    std::string mimeType;                          // MIME type of file
    size_t fileSize;                              // File size in bytes
    std::optional<std::string> checksum;          // File checksum (MD5/SHA256)
    std::optional<std::string> description;       // File description
    std::unordered_map<std::string, std::string> metadata; // Additional file metadata
    std::chrono::system_clock::time_point createdAt; // File creation time

    OutputFile() : fileSize(0), createdAt(std::chrono::system_clock::now()) {}
    
    OutputFile(const std::string& name, const std::string& path, 
               const std::string& mime, size_t size)
        : filename(name), filepath(path), mimeType(mime), fileSize(size),
          createdAt(std::chrono::system_clock::now()) {}

    bool operator==(const OutputFile& other) const {
        return filename == other.filename &&
               filepath == other.filepath &&
               mimeType == other.mimeType &&
               fileSize == other.fileSize &&
               checksum == other.checksum &&
               description == other.description &&
               metadata == other.metadata;
    }
};

/**
 * Job result model representing the outcome of job execution
 * 
 * This class encapsulates all result data, error information, performance metrics,
 * and output files produced by a job execution.
 */
class JobResult {
private:
    std::string id_;                                          // Unique result identifier
    std::string jobId_;                                       // Associated job ID
    std::string userId_;                                      // User who owns the job
    std::string tenantId_;                                    // Multi-tenant identifier
    JobStatus finalStatus_;                                   // Final job status
    std::optional<std::string> resultData_;                   // Main result data (JSON)
    std::optional<JobError> error_;                           // Error information if failed
    JobMetrics metrics_;                                      // Performance metrics
    std::vector<OutputFile> outputFiles_;                     // Output files produced
    std::unordered_map<std::string, std::string> metadata_;   // Additional metadata
    std::vector<std::string> logMessages_;                    // Job execution logs
    std::chrono::system_clock::time_point createdAt_;        // Result creation time
    std::optional<std::chrono::system_clock::time_point> expiresAt_; // Result expiration time

public:
    /**
     * Default constructor
     */
    JobResult();

    /**
     * Constructor with job ID
     */
    explicit JobResult(const std::string& jobId);

    /**
     * Constructor with job information
     */
    JobResult(const std::string& jobId, const std::string& userId, 
              const std::string& tenantId, JobStatus finalStatus);

    // Copy and move constructors/assignment operators
    JobResult(const JobResult& other) = default;
    JobResult& operator=(const JobResult& other) = default;
    JobResult(JobResult&& other) noexcept = default;
    JobResult& operator=(JobResult&& other) noexcept = default;

    // Destructor
    ~JobResult() = default;

    // Getters
    const std::string& getId() const { return id_; }
    const std::string& getJobId() const { return jobId_; }
    const std::string& getUserId() const { return userId_; }
    const std::string& getTenantId() const { return tenantId_; }
    JobStatus getFinalStatus() const { return finalStatus_; }
    const std::optional<std::string>& getResultData() const { return resultData_; }
    const std::optional<JobError>& getError() const { return error_; }
    const JobMetrics& getMetrics() const { return metrics_; }
    const std::vector<OutputFile>& getOutputFiles() const { return outputFiles_; }
    const std::unordered_map<std::string, std::string>& getMetadata() const { return metadata_; }
    const std::vector<std::string>& getLogMessages() const { return logMessages_; }
    const std::chrono::system_clock::time_point& getCreatedAt() const { return createdAt_; }
    const std::optional<std::chrono::system_clock::time_point>& getExpiresAt() const { return expiresAt_; }

    // Setters
    void setId(const std::string& id) { id_ = id; }
    void setJobId(const std::string& jobId) { jobId_ = jobId; }
    void setUserId(const std::string& userId) { userId_ = userId; }
    void setTenantId(const std::string& tenantId) { tenantId_ = tenantId; }
    void setFinalStatus(JobStatus status) { finalStatus_ = status; }
    void setResultData(const std::string& data) { resultData_ = data; }
    void setError(const JobError& error) { error_ = error; }
    void setMetrics(const JobMetrics& metrics) { metrics_ = metrics; }
    void setExpiresAt(const std::chrono::system_clock::time_point& time) { expiresAt_ = time; }

    // Result data management
    void setResultDataJson(const nlohmann::json& json);
    std::optional<nlohmann::json> getResultDataJson() const;
    
    // Error management
    void setError(const std::string& errorCode, const std::string& errorMessage);
    void setError(const std::string& errorCode, const std::string& errorMessage, 
                  const std::string& stackTrace);
    bool hasError() const { return error_.has_value(); }
    void clearError() { error_.reset(); }

    // Output file management
    void addOutputFile(const OutputFile& file);
    void addOutputFile(const std::string& filename, const std::string& filepath, 
                      const std::string& mimeType, size_t fileSize);
    void removeOutputFile(const std::string& filename);
    std::optional<OutputFile> getOutputFile(const std::string& filename) const;
    size_t getOutputFileCount() const { return outputFiles_.size(); }
    size_t getTotalOutputSize() const;

    // Metadata management
    void setMetadata(const std::string& key, const std::string& value);
    std::optional<std::string> getMetadata(const std::string& key) const;
    void removeMetadata(const std::string& key);
    bool hasMetadata(const std::string& key) const;

    // Log management
    void addLogMessage(const std::string& message);
    void addLogMessages(const std::vector<std::string>& messages);
    void clearLogMessages() { logMessages_.clear(); }

    // Metrics management
    void updateMetrics(const JobMetrics& metrics) { metrics_ = metrics; }
    void addCustomMetric(const std::string& name, double value);
    std::optional<double> getCustomMetric(const std::string& name) const;

    // Business logic methods
    bool isSuccess() const { return finalStatus_ == JobStatus::COMPLETED && !hasError(); }
    bool isFailure() const { return finalStatus_ == JobStatus::FAILED || hasError(); }
    bool isExpired() const;
    std::chrono::milliseconds getAge() const;

    // Validation methods
    bool isValid() const;
    Result<bool> validate() const;

    // Serialization methods
    bsoncxx::document::value toBson() const;
    static JobResult fromBson(const bsoncxx::document::view& doc);
    nlohmann::json toJson() const;
    static JobResult fromJson(const nlohmann::json& json);

    // Utility methods
    std::string toString() const;
    std::string getSummary() const; // Brief summary for logging
    bool operator==(const JobResult& other) const;
    bool operator!=(const JobResult& other) const { return !(*this == other); }

    // Static helper methods
    static std::string generateResultId();
    static JobResult createFromJob(const Job& job, JobStatus finalStatus);
    static JobResult createSuccessResult(const Job& job, const std::string& resultData = "");
    static JobResult createFailureResult(const Job& job, const std::string& errorCode, 
                                        const std::string& errorMessage);
};

} // namespace models
} // namespace search_engine
