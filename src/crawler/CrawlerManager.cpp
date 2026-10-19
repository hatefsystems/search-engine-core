#include "search_engine/crawler/CrawlerManager.h"
#include "../../include/Logger.h"
#include <algorithm>
#include <cstdlib>

CrawlerManager::CrawlerManager(std::shared_ptr<search_engine::storage::ContentStorage> storage,
                               RunnerFactory factory, size_t maxConcurrent, size_t maxPending)
    : storage_(std::move(storage)), runnerFactory_(std::move(factory)),
      maxConcurrent_(maxConcurrent), pendingQueue_(maxPending) {
    if (!runnerFactory_ || maxConcurrent_ == 0) {
        throw std::invalid_argument(
            "CrawlerManager needs a runner factory and positive concurrency");
    }
    cleanupThread_ = std::thread(&CrawlerManager::cleanupWorker, this);
}

CrawlerManager::~CrawlerManager() {
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        shouldStop_ = true;
        pendingQueue_.clear();
        for (auto &[id, session] : sessions_)
            session->cancelRequested = true;
    }
    wake_.notify_all();
    // No joins, callbacks, or crawler stop operations under the manager mutex.
    cleanupThread_.join();
    std::vector<std::shared_ptr<Session>> remaining;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        for (auto &[id, session] : sessions_)
            remaining.push_back(session);
        sessions_.clear();
    }
    for (auto &session : remaining) {
        if (session->thread.joinable())
            session->thread.join();
    }
}

bool CrawlerManager::terminal(State state) {
    return state == State::Completed || state == State::Failed || state == State::Cancelled ||
           state == State::TimedOut;
}

const char *CrawlerManager::stateName(State state) {
    switch (state) {
    case State::Queued:
        return "queued";
    case State::Starting:
        return "starting";
    case State::Running:
        return "crawling";
    case State::Stopping:
        return "stopping";
    case State::Completed:
        return "completed";
    case State::Failed:
        return "failed";
    case State::Cancelled:
        return "cancelled";
    case State::TimedOut:
        return "timed_out";
    }
    return "failed";
}

std::string CrawlerManager::startCrawl(const std::string &url, const CrawlConfig &config,
                                       bool force, CrawlCompletionCallback callback,
                                       CrawlPriority priority) {
    if (url.compare(0, 7, "http://") != 0 && url.compare(0, 8, "https://") != 0) {
        throw std::invalid_argument("Crawl URL must use HTTP or HTTPS");
    }
    if (priority != CrawlPriority::LOW && priority != CrawlPriority::NORMAL &&
        priority != CrawlPriority::HIGH) {
        throw std::invalid_argument("Invalid session priority");
    }
    auto session = std::make_shared<Session>();
    session->id = generateSessionId();
    session->url = url;
    session->config = config;
    session->force = force;
    session->callback = std::move(callback);
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        if (shouldStop_)
            throw std::runtime_error("CrawlerManager is shutting down");
        // Admission, reservation, publication and dispatch use the same mutex.
        // New arrivals cannot bypass an existing backlog.
        bool immediate = running_ < maxConcurrent_ && pendingQueue_.empty();
        sessions_.emplace(session->id, session);
        try {
            if (immediate) {
                launchLocked(session);
            } else if (!pendingQueue_.push({session->id, priority})) {
                throw CrawlQueueFull();
            }
        } catch (...) {
            sessions_.erase(session->id);
            throw;
        }
        dispatchRequested_ = true;
    }
    wake_.notify_all();
    return session->id;
}

void CrawlerManager::launchLocked(const std::shared_ptr<Session> &session) {
    session->state = State::Starting;
    ++running_;
    try {
        // The session is already in the map before its thread can observe it.
        session->thread = std::thread(&CrawlerManager::runSession, this, session);
    } catch (...) {
        --running_;
        session->state = State::Failed;
        session->workerFinished = true;
        session->finishedAt = Clock::now();
        throw;
    }
}

void CrawlerManager::runSession(const std::shared_ptr<Session> &session) noexcept {
    State outcome = State::Completed;
    std::shared_ptr<CrawlSessionRunner> runner;
    std::vector<CrawlResult> results;
    try {
        bool cancelled;
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            cancelled = shouldStop_ || session->cancelRequested;
        }
        if (!cancelled) {
            runner = runnerFactory_(session->config, session->id, session->url, session->force);
            if (!runner)
                throw std::runtime_error("Crawler factory returned no runner");
            {
                std::lock_guard<std::mutex> lock(sessionsMutex_);
                session->runner = runner;
                cancelled = shouldStop_ || session->cancelRequested;
            }
        }
        if (cancelled) {
            outcome = State::Cancelled;
        } else {
            runner->start();
            auto deadline = Clock::now() + session->config.maxSessionDuration;
            std::unique_lock<std::mutex> lock(sessionsMutex_);
            session->state = session->cancelRequested ? State::Stopping : State::Running;
            while (true) {
                if (shouldStop_ || session->cancelRequested) {
                    outcome = State::Cancelled;
                    break;
                }
                if (Clock::now() >= deadline) {
                    outcome = State::TimedOut;
                    break;
                }
                if (!runner->isRunning())
                    break;
                // Monitor actual crawler liveness, including periods where its
                // URL frontier is waiting for retries. Results alone are insufficient.
                wake_.wait_for(lock, std::chrono::milliseconds(100),
                               [&] { return shouldStop_ || session->cancelRequested; });
            }
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Crawl session " + session->id + " failed: " + e.what());
        outcome = State::Failed;
    } catch (...) {
        LOG_ERROR("Crawl session " + session->id + " failed with an unknown error");
        outcome = State::Failed;
    }

    // Join the crawler's own worker before releasing its reserved capacity.
    // No same-id retry/replacement and no caller can concurrently join it.
    if (runner) {
        try {
            runner->stop();
            results = runner->getResults();
        } catch (...) {
            LOG_ERROR("Failed to stop/read crawler for session " + session->id);
            outcome = State::Failed;
        }
    }
    CrawlCompletionCallback callback;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        if (session->cancelRequested || shouldStop_)
            outcome = State::Cancelled;
        session->state = outcome;
        if (outcome == State::Completed)
            callback = std::move(session->callback);
    }
    if (callback) {
        try {
            callback(session->id, results, this);
        } catch (...) {
            LOG_ERROR("Completion callback failed for session " + session->id);
        }
    }
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        session->results = std::move(results);
        session->runner.reset();
    }
    // Retain results, not idle crawler instances/connections, in terminal records.
    runner.reset();
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        session->callback = nullptr;
        session->workerFinished = true;
        session->finishedAt = Clock::now();
        --running_;
        dispatchRequested_ = true;
    }
    wake_.notify_all();
}

std::vector<CrawlResult> CrawlerManager::getCrawlResults(const std::string &id) {
    std::shared_ptr<CrawlSessionRunner> runner;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        auto it = sessions_.find(id);
        if (it == sessions_.end())
            return {};
        runner = it->second->runner;
        if (!runner)
            return it->second->results;
    }
    return runner ? runner->getResults() : std::vector<CrawlResult>{};
}

std::string CrawlerManager::getCrawlStatus(const std::string &id) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    auto it = sessions_.find(id);
    return it == sessions_.end() ? "not_found" : stateName(it->second->state);
}

bool CrawlerManager::stopCrawl(const std::string &id) {
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        auto it = sessions_.find(id);
        if (it == sessions_.end() || terminal(it->second->state))
            return false;
        auto &session = it->second;
        session->cancelRequested = true;
        if (session->state == State::Queued) {
            pendingQueue_.remove(id);
            session->state = State::Cancelled;
            session->workerFinished = true;
            session->finishedAt = Clock::now();
            session->callback = nullptr;
        } else {
            session->state = State::Stopping;
        }
        dispatchRequested_ = true;
    }
    wake_.notify_all();
    return true;
}

std::vector<std::string> CrawlerManager::getActiveSessions() {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    std::vector<std::string> ids;
    for (const auto &[id, session] : sessions_)
        if (!terminal(session->state))
            ids.push_back(id);
    return ids;
}

size_t CrawlerManager::getActiveSessionCount() {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return running_;
}

size_t CrawlerManager::getPendingSessionCount() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return pendingQueue_.size();
}

void CrawlerManager::cleanupCompletedSessions() {
    std::vector<std::shared_ptr<Session>> retired;
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        std::vector<std::shared_ptr<Session>> completed;
        for (const auto &[id, session] : sessions_) {
            if (session->workerFinished && session->thread.get_id() != std::this_thread::get_id()) {
                completed.push_back(session);
            }
        }
        std::sort(completed.begin(), completed.end(),
                  [](const auto &a, const auto &b) { return a->finishedAt < b->finishedAt; });
        for (size_t i = 0; i < completed.size(); ++i) {
            // Bound both retention time and the number of retained result sets.
            if (completed.size() - i <= 100 &&
                Clock::now() - completed[i]->finishedAt < std::chrono::seconds(60))
                break;
            auto session = completed[i];
            sessions_.erase(session->id);
            retired.push_back(std::move(session));
        }
    }
    for (auto &session : retired)
        if (session->thread.joinable())
            session->thread.join();
}

void CrawlerManager::cleanupWorker() {
    std::unique_lock<std::mutex> lock(sessionsMutex_);
    while (!shouldStop_) {
        wake_.wait_for(lock, std::chrono::seconds(1),
                       [&] { return shouldStop_ || dispatchRequested_; });
        if (shouldStop_)
            break;
        dispatchRequested_ = false;
        while (running_ < maxConcurrent_) {
            auto pending = pendingQueue_.tryPop();
            if (!pending)
                break;
            auto session = sessions_.at(pending->sessionId);
            try {
                launchLocked(session);
            } catch (...) {
                LOG_ERROR("Unable to create worker for queued session " + session->id);
            }
        }
        lock.unlock();
        cleanupCompletedSessions();
        lock.lock();
    }
}

size_t CrawlerManager::environmentLimit(const char *name, size_t fallback, size_t minimum,
                                        size_t maximum) {
    const char *raw = std::getenv(name);
    if (!raw)
        return fallback;
    std::string value(raw);
    try {
        if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
            throw std::invalid_argument(name);
        size_t result = std::stoull(value);
        if (result < minimum || result > maximum)
            throw std::out_of_range(name);
        return result;
    } catch (...) {
        LOG_WARNING(std::string("Invalid ") + name + "; using " + std::to_string(fallback));
        return fallback;
    }
}

std::string CrawlerManager::generateSessionId() {
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    return "crawl_" + std::to_string(timestamp) + "_" +
           std::to_string(sessionCounter_.fetch_add(1));
}
