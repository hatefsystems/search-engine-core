#include <catch2/catch_test_macros.hpp>
#include "search_engine/crawler/SessionPriorityQueue.h"

#include <chrono>
#include <thread>
#include <vector>
#include <string>

using namespace std::chrono_literals;

static PendingSessionEntry makeEntry(const std::string& id,
                                     CrawlPriority priority,
                                     std::chrono::system_clock::time_point queuedAt,
                                     std::chrono::system_clock::time_point readyAt = {}) {
    PendingSessionEntry e;
    e.sessionId = id;
    e.url = "https://example.com/" + id;
    e.priority = priority;
    e.queuedAt = queuedAt;
    e.readyAt = (readyAt.time_since_epoch().count() == 0) ? queuedAt : readyAt;
    return e;
}

TEST_CASE("SessionPriorityQueue basic FIFO at equal priority", "[SessionPriorityQueue]") {
    SessionPriorityQueue q;
    auto t0 = std::chrono::system_clock::now();

    q.push(makeEntry("a", CrawlPriority::NORMAL, t0));
    q.push(makeEntry("b", CrawlPriority::NORMAL, t0 + 1ms));
    q.push(makeEntry("c", CrawlPriority::NORMAL, t0 + 2ms));

    REQUIRE(q.size() == 3);
    auto e1 = q.tryPopReady();
    auto e2 = q.tryPopReady();
    auto e3 = q.tryPopReady();
    REQUIRE(e1.has_value());
    REQUIRE(e2.has_value());
    REQUIRE(e3.has_value());
    REQUIRE(e1->sessionId == "a");
    REQUIRE(e2->sessionId == "b");
    REQUIRE(e3->sessionId == "c");
    REQUIRE(q.empty());
}

TEST_CASE("SessionPriorityQueue HIGH preempts NORMAL", "[SessionPriorityQueue]") {
    SessionPriorityQueue q;
    auto t0 = std::chrono::system_clock::now();

    q.push(makeEntry("normal1", CrawlPriority::NORMAL, t0));
    q.push(makeEntry("normal2", CrawlPriority::NORMAL, t0 + 1ms));
    q.push(makeEntry("high1", CrawlPriority::HIGH, t0 + 2ms));  // arrived later but higher

    auto first = q.tryPopReady();
    REQUIRE(first.has_value());
    REQUIRE(first->sessionId == "high1");

    // Then FIFO among remaining NORMAL.
    auto second = q.tryPopReady();
    REQUIRE(second.has_value());
    REQUIRE(second->sessionId == "normal1");
}

TEST_CASE("SessionPriorityQueue RETRY beats HIGH", "[SessionPriorityQueue]") {
    SessionPriorityQueue q;
    auto t0 = std::chrono::system_clock::now();
    q.push(makeEntry("highA", CrawlPriority::HIGH, t0));
    q.push(makeEntry("retryA", CrawlPriority::RETRY, t0 + 1ms));

    auto first = q.tryPopReady();
    REQUIRE(first.has_value());
    REQUIRE(first->sessionId == "retryA");
}

TEST_CASE("SessionPriorityQueue LOW is processed last", "[SessionPriorityQueue]") {
    SessionPriorityQueue q;
    auto t0 = std::chrono::system_clock::now();
    q.push(makeEntry("lowA", CrawlPriority::LOW, t0));
    q.push(makeEntry("normalA", CrawlPriority::NORMAL, t0 + 1ms));

    auto first = q.tryPopReady();
    REQUIRE(first.has_value());
    REQUIRE(first->sessionId == "normalA");
    auto second = q.tryPopReady();
    REQUIRE(second.has_value());
    REQUIRE(second->sessionId == "lowA");
}

TEST_CASE("SessionPriorityQueue respects readyAt for retry backoff", "[SessionPriorityQueue]") {
    SessionPriorityQueue q;
    auto now = std::chrono::system_clock::now();
    auto soon = now + 100ms;

    // A retry entry not yet eligible.
    auto delayed = makeEntry("retryDelayed", CrawlPriority::RETRY, now, soon);
    q.push(delayed);
    // Plus an immediately-ready normal entry.
    q.push(makeEntry("normalNow", CrawlPriority::NORMAL, now, now));

    // At "now", only normalNow should be returned even though retry has higher priority.
    auto popped = q.tryPopReady(now);
    REQUIRE(popped.has_value());
    REQUIRE(popped->sessionId == "normalNow");

    // tryPopReady at "now" again — retry still not ready.
    auto popped2 = q.tryPopReady(now);
    REQUIRE_FALSE(popped2.has_value());

    // After the readyAt time, retry should come out.
    auto popped3 = q.tryPopReady(soon);
    REQUIRE(popped3.has_value());
    REQUIRE(popped3->sessionId == "retryDelayed");
}

TEST_CASE("SessionPriorityQueue remove cancels pending entry", "[SessionPriorityQueue]") {
    SessionPriorityQueue q;
    auto t0 = std::chrono::system_clock::now();
    q.push(makeEntry("toKeep", CrawlPriority::NORMAL, t0));
    q.push(makeEntry("toCancel", CrawlPriority::NORMAL, t0 + 1ms));
    REQUIRE(q.size() == 2);
    REQUIRE(q.contains("toCancel"));

    REQUIRE(q.remove("toCancel"));
    REQUIRE_FALSE(q.contains("toCancel"));
    REQUIRE(q.size() == 1);

    // Removing again should fail.
    REQUIRE_FALSE(q.remove("toCancel"));

    auto e = q.tryPopReady();
    REQUIRE(e.has_value());
    REQUIRE(e->sessionId == "toKeep");
}

TEST_CASE("SessionPriorityQueue snapshot reflects priority order", "[SessionPriorityQueue]") {
    SessionPriorityQueue q;
    auto t0 = std::chrono::system_clock::now();
    q.push(makeEntry("low", CrawlPriority::LOW, t0));
    q.push(makeEntry("normal", CrawlPriority::NORMAL, t0 + 1ms));
    q.push(makeEntry("high", CrawlPriority::HIGH, t0 + 2ms));
    q.push(makeEntry("retry", CrawlPriority::RETRY, t0 + 3ms));

    auto snap = q.snapshot();
    REQUIRE(snap.size() == 4);
    REQUIRE(snap[0].sessionId == "retry");
    REQUIRE(snap[1].sessionId == "high");
    REQUIRE(snap[2].sessionId == "normal");
    REQUIRE(snap[3].sessionId == "low");

    // Snapshot must not consume the queue.
    REQUIRE(q.size() == 4);
}

TEST_CASE("SessionPriorityQueue empty / clear", "[SessionPriorityQueue]") {
    SessionPriorityQueue q;
    REQUIRE(q.empty());
    REQUIRE_FALSE(q.tryPopReady().has_value());

    q.push(makeEntry("x", CrawlPriority::NORMAL, std::chrono::system_clock::now()));
    REQUIRE_FALSE(q.empty());
    q.clear();
    REQUIRE(q.empty());
}
