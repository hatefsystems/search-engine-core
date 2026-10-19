#include "search_engine/crawler/SessionPriorityQueue.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Session queue is bounded and FIFO among equal priorities", "[sessions]") {
    SessionPriorityQueue queue(3);
    REQUIRE(queue.push({"a", CrawlPriority::NORMAL}));
    REQUIRE(queue.push({"b", CrawlPriority::NORMAL}));
    REQUIRE(queue.push({"c", CrawlPriority::NORMAL}));
    REQUIRE_FALSE(queue.push({"overflow", CrawlPriority::HIGH}));
    REQUIRE(queue.tryPop()->sessionId == "a");
    REQUIRE(queue.tryPop()->sessionId == "b");
    REQUIRE(queue.tryPop()->sessionId == "c");
    REQUIRE_FALSE(queue.tryPop());
}

TEST_CASE("Trusted priority preserves FIFO between peers", "[sessions]") {
    SessionPriorityQueue queue;
    REQUIRE(queue.push({"low", CrawlPriority::LOW}));
    REQUIRE(queue.push({"normal", CrawlPriority::NORMAL}));
    REQUIRE(queue.push({"high-a", CrawlPriority::HIGH}));
    REQUIRE(queue.push({"high-b", CrawlPriority::HIGH}));
    REQUIRE(queue.tryPop()->sessionId == "high-a");
    REQUIRE(queue.tryPop()->sessionId == "high-b");
    REQUIRE(queue.tryPop()->sessionId == "normal");
    REQUIRE(queue.tryPop()->sessionId == "low");
}

TEST_CASE("Queue cancellation reclaims capacity and duplicate IDs are rejected", "[sessions]") {
    SessionPriorityQueue queue(2);
    REQUIRE(queue.push({"a"}));
    REQUIRE_FALSE(queue.push({"a"}));
    REQUIRE(queue.push({"b"}));
    REQUIRE_FALSE(queue.remove("missing"));
    REQUIRE(queue.remove("a"));
    REQUIRE(queue.push({"c"}));
    REQUIRE(queue.tryPop()->sessionId == "b");
    REQUIRE(queue.tryPop()->sessionId == "c");
    REQUIRE(queue.empty());
    REQUIRE(queue.push({"d"}));
    queue.clear();
    REQUIRE(queue.empty());
}

TEST_CASE("Zero waiting capacity rejects all queue entries", "[sessions]") {
    SessionPriorityQueue queue(0);
    REQUIRE_FALSE(queue.push({"a"}));
    REQUIRE_FALSE(queue.tryPop());
}
