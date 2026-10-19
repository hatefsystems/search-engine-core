#pragma once

#include "CrawlPriority.h"
#include "SessionPriorityQueue.h"
#include "models/CrawlConfig.h"
#include "models/CrawlResult.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace search_engine {
namespace storage {
class ContentStorage;
}
} // namespace search_engine
class CrawlerManager;
using CrawlCompletionCallback =
    std::function<void(const std::string &, const std::vector<CrawlResult> &, CrawlerManager *)>;

class CrawlQueueFull : public std::runtime_error {
  public:
    CrawlQueueFull()
        : std::runtime_error("Crawl capacity and pending queue are full. Try again later.") {}
};

// Owns the crawler worker. stop() joins it. Only the session worker calls
// start/stop; getResults() supports concurrent reads. This boundary lets tests
// exercise the real manager without MongoDB, HTTP, or Browserless.
class CrawlSessionRunner {
  public:
    virtual ~CrawlSessionRunner() = default;
    virtual void start() = 0;
    virtual bool isRunning() const = 0;
    virtual void stop() = 0;
    virtual std::vector<CrawlResult> getResults() = 0;
};

class CrawlerManager {
  public:
    using RunnerFactory = std::function<std::shared_ptr<CrawlSessionRunner>(
        const CrawlConfig &, const std::string &, const std::string &, bool)>;
    static constexpr size_t MAX_CONCURRENT_SESSIONS = 5;
    static constexpr size_t MAX_PENDING_SESSIONS = 100;

    explicit CrawlerManager(std::shared_ptr<search_engine::storage::ContentStorage> storage);
    CrawlerManager(std::shared_ptr<search_engine::storage::ContentStorage> storage,
                   RunnerFactory factory, size_t maxConcurrent, size_t maxPending);
    ~CrawlerManager();
    CrawlerManager(const CrawlerManager &) = delete;
    CrawlerManager &operator=(const CrawlerManager &) = delete;

    // Priority is for trusted C++ callers. The public API uses NORMAL only.
    std::string startCrawl(const std::string &url, const CrawlConfig &config, bool force = false,
                           CrawlCompletionCallback callback = nullptr,
                           CrawlPriority priority = CrawlPriority::NORMAL);
    std::vector<CrawlResult> getCrawlResults(const std::string &sessionId);
    std::string getCrawlStatus(const std::string &sessionId);
    // Cooperative cancellation; running slots remain reserved until stop joins.
    bool stopCrawl(const std::string &sessionId);
    // All unfinished sessions, including queued ones, for stopPreviousSessions.
    std::vector<std::string> getActiveSessions();
    size_t getActiveSessionCount();
    size_t getPendingSessionCount() const;
    size_t getMaxConcurrentSessions() const { return maxConcurrent_; }
    void cleanupCompletedSessions();
    std::shared_ptr<search_engine::storage::ContentStorage> getStorage() const { return storage_; }

  private:
    using Clock = std::chrono::steady_clock;
    enum class State {
        Queued,
        Starting,
        Running,
        Stopping,
        Completed,
        Failed,
        Cancelled,
        TimedOut
    };
    struct Session {
        std::string id;
        std::string url;
        CrawlConfig config;
        bool force = false;
        State state = State::Queued;
        bool cancelRequested = false;
        bool workerFinished = false;
        Clock::time_point finishedAt{};
        std::shared_ptr<CrawlSessionRunner> runner;
        std::vector<CrawlResult> results;
        CrawlCompletionCallback callback;
        std::thread thread;
    };

    std::shared_ptr<search_engine::storage::ContentStorage> storage_;
    RunnerFactory runnerFactory_;
    const size_t maxConcurrent_;
    mutable std::mutex sessionsMutex_;
    std::condition_variable wake_;
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
    SessionPriorityQueue pendingQueue_;
    size_t running_ = 0; // Reserved before thread creation; released after stop/callback.
    bool shouldStop_ = false;
    bool dispatchRequested_ = false;
    std::thread cleanupThread_;
    std::atomic<uint64_t> sessionCounter_{0};

    static bool terminal(State state);
    static const char *stateName(State state);
    static size_t environmentLimit(const char *name, size_t fallback, size_t minimum,
                                   size_t maximum);
    std::string generateSessionId();
    void launchLocked(const std::shared_ptr<Session> &session);
    void runSession(const std::shared_ptr<Session> &session) noexcept;
    void cleanupWorker();
};
