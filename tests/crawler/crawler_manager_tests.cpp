#include "search_engine/crawler/CrawlerManager.h"
#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <future>
#include <thread>

using namespace std::chrono_literals;

namespace {
template <typename Predicate> bool eventually(Predicate predicate) {
    auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

struct Runner final : CrawlSessionRunner {
    std::atomic<bool> running{false};
    std::atomic<bool> finish{false};
    std::atomic<int> starts{0};
    std::atomic<int> stops{0};
    void start() override {
        ++starts;
        running = true;
    }
    bool isRunning() const override { return running && !finish; }
    void stop() override {
        ++stops;
        running = false;
    }
    std::vector<CrawlResult> getResults() override { return {}; }
};

struct Harness {
    std::mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<Runner>> runners;
    std::vector<std::string> order;
    CrawlerManager::RunnerFactory factory() {
        return [this](const CrawlConfig &, const std::string &, const std::string &url, bool) {
            if (url == "https://fail.test")
                throw std::runtime_error("simulated allocation/start failure");
            auto runner = std::make_shared<Runner>();
            std::lock_guard<std::mutex> lock(mutex);
            runners[url] = runner;
            order.push_back(url);
            return runner;
        };
    }
    std::shared_ptr<Runner> get(const std::string &url) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = runners.find(url);
        return it == runners.end() ? nullptr : it->second;
    }
    size_t count() {
        std::lock_guard<std::mutex> lock(mutex);
        return order.size();
    }
};
} // namespace

TEST_CASE("Saturation queues lazily, rejects overflow, and queued jobs can be cancelled",
          "[sessions]") {
    Harness h;
    CrawlerManager manager(nullptr, h.factory(), 1, 2);
    auto active = manager.startCrawl("https://active.test", {});
    REQUIRE(eventually([&] {
        auto r = h.get("https://active.test");
        return r && r->starts == 1;
    }));
    auto queued = manager.startCrawl("https://queued.test", {});
    manager.startCrawl("https://last.test", {});
    REQUIRE(manager.getCrawlStatus(queued) == "queued");
    REQUIRE(manager.getCrawlResults(queued).empty());
    REQUIRE(manager.getActiveSessions().size() == 3);
    REQUIRE(manager.getActiveSessionCount() == 1);
    REQUIRE(manager.getPendingSessionCount() == 2);
    REQUIRE(h.count() == 1);
    REQUIRE_THROWS_AS(manager.startCrawl("https://overflow.test", {}), CrawlQueueFull);
    REQUIRE(manager.stopCrawl(queued));
    REQUIRE(manager.getCrawlStatus(queued) == "cancelled");
    REQUIRE_FALSE(manager.stopCrawl(queued));
    REQUIRE_FALSE(manager.stopCrawl("missing"));
    REQUIRE(manager.getCrawlStatus("missing") == "not_found");
    REQUIRE(manager.getPendingSessionCount() == 1);
    REQUIRE_NOTHROW(manager.startCrawl("https://replacement.test", {}));
    REQUIRE_FALSE(h.get("https://queued.test"));
    REQUIRE(manager.stopCrawl(active));
    REQUIRE(eventually([&] { return manager.getCrawlStatus(active) == "cancelled"; }));
    REQUIRE(eventually([&] {
        auto r = h.get("https://last.test");
        return r && r->starts == 1;
    }));
    REQUIRE(h.get("https://active.test")->stops == 1);
}

TEST_CASE("Concurrent submissions never oversubscribe active or pending capacity", "[sessions]") {
    Harness h;
    CrawlerManager manager(nullptr, h.factory(), 3, 5);
    std::atomic<int> accepted{0}, rejected{0}, unexpected{0};
    std::promise<void> go;
    auto ready = go.get_future().share();
    std::vector<std::thread> producers;
    for (int i = 0; i < 32; ++i)
        producers.emplace_back([&, i] {
            ready.wait();
            try {
                manager.startCrawl("https://example.test/" + std::to_string(i), {});
                ++accepted;
            } catch (const CrawlQueueFull &) {
                ++rejected;
            } catch (...) {
                ++unexpected;
            }
        });
    go.set_value();
    for (auto &producer : producers)
        producer.join();
    REQUIRE(unexpected == 0);
    REQUIRE(accepted == 8);
    REQUIRE(rejected == 24);
    REQUIRE(manager.getActiveSessionCount() == 3);
    REQUIRE(manager.getPendingSessionCount() == 5);
    REQUIRE(eventually([&] { return h.count() == 3; }));
    for (const auto &id : manager.getActiveSessions())
        manager.stopCrawl(id);
    REQUIRE(eventually([&] { return manager.getActiveSessionCount() == 0; }));
}

TEST_CASE("Completion dispatches priorities and real runner liveness gates the slot",
          "[sessions]") {
    Harness h;
    CrawlerManager manager(nullptr, h.factory(), 1, 4);
    auto first = manager.startCrawl("https://first.test", {});
    auto low = manager.startCrawl("https://low.test", {}, false, nullptr, CrawlPriority::LOW);
    auto high = manager.startCrawl("https://high.test", {}, false, nullptr, CrawlPriority::HIGH);
    REQUIRE(eventually([&] {
        auto r = h.get("https://first.test");
        return r && r->starts == 1;
    }));
    // Empty results while the runner is live must not be treated as completion.
    REQUIRE(manager.getCrawlStatus(first) == "crawling");
    REQUIRE(h.count() == 1);
    h.get("https://first.test")->finish = true;
    REQUIRE(eventually([&] {
        auto r = h.get("https://high.test");
        return r && r->starts == 1;
    }));
    REQUIRE(manager.getCrawlStatus(first) == "completed");
    REQUIRE(manager.getCrawlStatus(low) == "queued");
    h.get("https://high.test")->finish = true;
    REQUIRE(eventually([&] {
        auto r = h.get("https://low.test");
        return r && r->starts == 1;
    }));
    REQUIRE(manager.getCrawlStatus(high) == "completed");
}

TEST_CASE("Factory failure remains inspectable and does not lose the next queued job",
          "[sessions]") {
    Harness h;
    CrawlerManager manager(nullptr, h.factory(), 1, 3);
    auto first = manager.startCrawl("https://first.test", {});
    auto failed = manager.startCrawl("https://fail.test", {});
    auto next = manager.startCrawl("https://next.test", {});
    REQUIRE(eventually([&] {
        auto r = h.get("https://first.test");
        return r && r->starts == 1;
    }));
    h.get("https://first.test")->finish = true;
    REQUIRE(eventually([&] { return manager.getCrawlStatus(failed) == "failed"; }));
    REQUIRE(eventually([&] {
        auto r = h.get("https://next.test");
        return r && r->starts == 1;
    }));
    REQUIRE(manager.getCrawlStatus(first) == "completed");
    REQUIRE(manager.getCrawlStatus(next) == "crawling");
    REQUIRE(manager.getActiveSessionCount() == 1);
}

TEST_CASE("Callbacks run once outside the mutex and exceptions do not stall dispatch",
          "[sessions]") {
    Harness h;
    std::atomic<int> callbacks{0};
    std::atomic<bool> inspected{false};
    CrawlerManager manager(nullptr, h.factory(), 1, 2);
    auto id = manager.startCrawl("https://first.test", {}, false,
                                 [&](const auto &current, const auto &, CrawlerManager *owner) {
                                     inspected = owner->getCrawlStatus(current) == "completed" &&
                                                 owner->getCrawlResults(current).empty();
                                     owner->cleanupCompletedSessions();
                                     ++callbacks;
                                     throw std::runtime_error("simulated email failure");
                                 });
    manager.startCrawl("https://next.test", {});
    REQUIRE(eventually([&] {
        auto r = h.get("https://first.test");
        return r && r->starts == 1;
    }));
    h.get("https://first.test")->finish = true;
    REQUIRE(eventually([&] {
        auto r = h.get("https://next.test");
        return r && r->starts == 1;
    }));
    REQUIRE(inspected);
    REQUIRE(callbacks == 1);
    REQUIRE(manager.getCrawlStatus(id) == "completed");
}

TEST_CASE("Timeout stops before releasing capacity and does not send completion email",
          "[sessions]") {
    Harness h;
    std::atomic<int> callbacks{0};
    CrawlerManager manager(nullptr, h.factory(), 1, 1);
    CrawlConfig config;
    config.maxSessionDuration = std::chrono::minutes(0);
    auto id = manager.startCrawl("https://timeout.test", config, false,
                                 [&](const auto &, const auto &, auto *) { ++callbacks; });
    REQUIRE(eventually([&] {
        return manager.getCrawlStatus(id) == "timed_out" && manager.getActiveSessionCount() == 0;
    }));
    REQUIRE(h.get("https://timeout.test")->stops == 1);
    REQUIRE(callbacks == 0);
}

TEST_CASE("Shutdown joins running workers and never dispatches queued sessions", "[sessions]") {
    Harness h;
    std::atomic<int> callbacks{0};
    auto manager = std::make_unique<CrawlerManager>(nullptr, h.factory(), 1, 1);
    manager->startCrawl("https://active.test", {}, false,
                        [&](const auto &, const auto &, auto *) { ++callbacks; });
    REQUIRE(eventually([&] {
        auto r = h.get("https://active.test");
        return r && r->starts == 1;
    }));
    manager->startCrawl("https://pending.test", {});
    manager.reset();
    REQUIRE(h.get("https://active.test")->stops == 1);
    REQUIRE_FALSE(h.get("https://pending.test"));
    REQUIRE(callbacks == 0);
}

TEST_CASE("Zero backlog preserves direct execution and rejects busy requests", "[sessions]") {
    Harness h;
    CrawlerManager manager(nullptr, h.factory(), 1, 0);
    manager.startCrawl("https://active.test", {});
    REQUIRE_THROWS_AS(manager.startCrawl("https://pending.test", {}), CrawlQueueFull);
    REQUIRE_THROWS_AS(manager.startCrawl("file:///etc/passwd", {}), std::invalid_argument);
    REQUIRE_THROWS_AS(
        manager.startCrawl("https://retry.test", {}, false, nullptr, CrawlPriority::RETRY),
        std::invalid_argument);
    REQUIRE(manager.getPendingSessionCount() == 0);
}

TEST_CASE("Invalid manager construction fails before any worker is created", "[sessions]") {
    Harness h;
    REQUIRE_THROWS_AS(CrawlerManager(nullptr, h.factory(), 0, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(CrawlerManager(nullptr, {}, 1, 1), std::invalid_argument);
}

TEST_CASE("A stopping runner retains its slot until stop has joined", "[sessions]") {
    struct BlockingStop final : CrawlSessionRunner {
        std::promise<void> started;
        std::promise<void> stopping;
        std::shared_future<void> release;
        explicit BlockingStop(std::shared_future<void> gate) : release(std::move(gate)) {}
        void start() override { started.set_value(); }
        bool isRunning() const override { return true; }
        void stop() override {
            stopping.set_value();
            release.wait();
        }
        std::vector<CrawlResult> getResults() override { return {}; }
    };
    std::promise<void> release;
    auto blocker = std::make_shared<BlockingStop>(release.get_future().share());
    auto started = blocker->started.get_future();
    auto stopping = blocker->stopping.get_future();
    auto next = std::make_shared<Runner>();
    CrawlerManager manager(
        nullptr,
        [=](const auto &, const auto &, const auto &url,
            bool) -> std::shared_ptr<CrawlSessionRunner> {
            if (url == "https://block.test")
                return blocker;
            return next;
        },
        1, 1);
    // Release on assertion failure as well, so the manager can always join.
    struct ReleaseOnExit {
        std::promise<void> &promise;
        bool released = false;
        void release() {
            if (!released) {
                released = true;
                promise.set_value();
            }
        }
        ~ReleaseOnExit() { release(); }
    } guard{release};
    auto first = manager.startCrawl("https://block.test", {});
    REQUIRE(started.wait_for(3s) == std::future_status::ready);
    manager.startCrawl("https://next.test", {});
    REQUIRE(manager.stopCrawl(first));
    REQUIRE(stopping.wait_for(3s) == std::future_status::ready);
    REQUIRE(manager.getActiveSessionCount() == 1);
    REQUIRE(manager.getPendingSessionCount() == 1);
    REQUIRE(next->starts == 0);
    guard.release();
    REQUIRE(eventually([&] { return next->starts == 1; }));
    REQUIRE(manager.getCrawlStatus(first) == "cancelled");
}

TEST_CASE("A callback can enqueue another session without deadlocking", "[sessions]") {
    Harness h;
    std::atomic<bool> enqueued{false};
    CrawlerManager manager(nullptr, h.factory(), 1, 1);
    manager.startCrawl("https://first.test", {}, false,
                       [&](const auto &, const auto &, auto *owner) {
                           owner->startCrawl("https://callback.test", {});
                           enqueued = true;
                       });
    REQUIRE(eventually([&] {
        auto r = h.get("https://first.test");
        return r && r->starts == 1;
    }));
    h.get("https://first.test")->finish = true;
    REQUIRE(eventually([&] {
        auto r = h.get("https://callback.test");
        return r && r->starts == 1;
    }));
    REQUIRE(enqueued);
}

TEST_CASE("Terminal records retain results without retaining crawler instances", "[sessions]") {
    struct ResultRunner final : CrawlSessionRunner {
        void start() override {}
        bool isRunning() const override { return false; }
        void stop() override {}
        std::vector<CrawlResult> getResults() override {
            CrawlResult result{};
            result.url = "https://retained.test";
            result.success = true;
            result.crawlStatus = "downloaded";
            return {result};
        }
    };
    std::promise<std::weak_ptr<CrawlSessionRunner>> created;
    auto weakFuture = created.get_future();
    CrawlerManager manager(
        nullptr,
        [&](const auto &, const auto &, const auto &, bool) {
            auto runner = std::make_shared<ResultRunner>();
            created.set_value(runner);
            return runner;
        },
        1, 1);
    auto id = manager.startCrawl("https://retained.test", {});
    REQUIRE(weakFuture.wait_for(3s) == std::future_status::ready);
    auto weak = weakFuture.get();
    REQUIRE(eventually([&] { return weak.expired() && manager.getActiveSessionCount() == 0; }));
    REQUIRE(manager.getCrawlStatus(id) == "completed");
    auto results = manager.getCrawlResults(id);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].url == "https://retained.test");
}
