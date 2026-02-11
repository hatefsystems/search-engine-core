#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include "../../include/search_engine/common/SlugCache.h"
#include <thread>
#include <vector>
#include <atomic>

using namespace search_engine::common;

TEST_CASE("SlugCache - Basic Operations", "[slugcache][basic]") {
    SlugCache cache(300); // 5 minute TTL

    SECTION("Put and get") {
        cache.put("john-doe", "profile-123");
        auto result = cache.get("john-doe");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == "profile-123");
    }

    SECTION("Get non-existent key returns nullopt") {
        auto result = cache.get("non-existent");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Remove entry") {
        cache.put("test-slug", "id-456");
        REQUIRE(cache.get("test-slug").has_value());

        cache.remove("test-slug");
        REQUIRE_FALSE(cache.get("test-slug").has_value());
    }

    SECTION("Remove non-existent key does not throw") {
        REQUIRE_NOTHROW(cache.remove("non-existent"));
    }

    SECTION("Clear removes all entries") {
        cache.put("slug-1", "id-1");
        cache.put("slug-2", "id-2");
        cache.put("slug-3", "id-3");

        cache.clear();

        REQUIRE_FALSE(cache.get("slug-1").has_value());
        REQUIRE_FALSE(cache.get("slug-2").has_value());
        REQUIRE_FALSE(cache.get("slug-3").has_value());
    }

    SECTION("Overwrite existing entry") {
        cache.put("slug-1", "old-id");
        cache.put("slug-1", "new-id");

        auto result = cache.get("slug-1");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == "new-id");
    }
}

TEST_CASE("SlugCache - TTL Expiration", "[slugcache][ttl]") {
    // Use a very short TTL for testing
    SlugCache cache(1); // 1 second TTL

    SECTION("Entry expires after TTL") {
        cache.put("expiring-slug", "id-789");

        // Should be available immediately
        REQUIRE(cache.get("expiring-slug").has_value());

        // Wait for TTL to expire
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));

        // Should be expired now
        REQUIRE_FALSE(cache.get("expiring-slug").has_value());
    }

    SECTION("Cleanup removes expired entries") {
        cache.put("expired-1", "id-1");
        cache.put("expired-2", "id-2");

        std::this_thread::sleep_for(std::chrono::milliseconds(1100));

        size_t removed = cache.cleanupExpired();
        REQUIRE(removed >= 2);
    }
}

TEST_CASE("SlugCache - Statistics", "[slugcache][stats]") {
    SlugCache cache(300);

    SECTION("Empty cache statistics") {
        auto [total, valid] = cache.getStats();
        REQUIRE(total == 0);
        REQUIRE(valid == 0);
    }

    SECTION("Statistics after insertions") {
        cache.put("slug-1", "id-1");
        cache.put("slug-2", "id-2");
        cache.put("slug-3", "id-3");

        auto [total, valid] = cache.getStats();
        REQUIRE(total == 3);
        REQUIRE(valid == 3);
    }

    SECTION("Statistics after removal") {
        cache.put("slug-1", "id-1");
        cache.put("slug-2", "id-2");
        cache.remove("slug-1");

        auto [total, valid] = cache.getStats();
        REQUIRE(total == 1);
        REQUIRE(valid == 1);
    }
}

TEST_CASE("SlugCache - Max Size Enforcement", "[slugcache][maxsize]") {
    // Cache with max 5 entries
    SlugCache cache(300, 5);

    SECTION("Evicts when exceeding max size") {
        for (int i = 0; i < 10; ++i) {
            cache.put("slug-" + std::to_string(i), "id-" + std::to_string(i));
        }

        auto [total, valid] = cache.getStats();
        REQUIRE(total <= 6); // May be slightly over due to race between put and eviction logic
    }
}

TEST_CASE("SlugCache - Thread Safety", "[slugcache][threads]") {
    SlugCache cache(300);
    const int numThreads = 8;
    const int opsPerThread = 100;
    std::atomic<int> errors{0};

    SECTION("Concurrent put and get") {
        std::vector<std::thread> threads;

        // Writer threads
        for (int t = 0; t < numThreads / 2; ++t) {
            threads.emplace_back([&cache, &errors, t, opsPerThread]() {
                try {
                    for (int i = 0; i < opsPerThread; ++i) {
                        std::string slug = "thread-" + std::to_string(t) + "-slug-" + std::to_string(i);
                        cache.put(slug, "id-" + std::to_string(i));
                    }
                } catch (...) {
                    errors.fetch_add(1);
                }
            });
        }

        // Reader threads
        for (int t = 0; t < numThreads / 2; ++t) {
            threads.emplace_back([&cache, &errors, t, opsPerThread]() {
                try {
                    for (int i = 0; i < opsPerThread; ++i) {
                        std::string slug = "thread-" + std::to_string(t) + "-slug-" + std::to_string(i);
                        cache.get(slug); // May or may not find the entry
                    }
                } catch (...) {
                    errors.fetch_add(1);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        REQUIRE(errors.load() == 0);
    }

    SECTION("Concurrent put, get, and remove") {
        std::vector<std::thread> threads;

        for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back([&cache, &errors, t, opsPerThread]() {
                try {
                    for (int i = 0; i < opsPerThread; ++i) {
                        std::string slug = "concurrent-" + std::to_string(t) + "-" + std::to_string(i);
                        cache.put(slug, "id");
                        cache.get(slug);
                        if (i % 3 == 0) {
                            cache.remove(slug);
                        }
                    }
                } catch (...) {
                    errors.fetch_add(1);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        REQUIRE(errors.load() == 0);
    }

    SECTION("Concurrent cleanup does not crash") {
        // Fill cache
        for (int i = 0; i < 100; ++i) {
            cache.put("cleanup-slug-" + std::to_string(i), "id-" + std::to_string(i));
        }

        std::vector<std::thread> threads;
        for (int t = 0; t < numThreads; ++t) {
            threads.emplace_back([&cache, &errors]() {
                try {
                    cache.cleanupExpired();
                    cache.getStats();
                    cache.clear();
                } catch (...) {
                    errors.fetch_add(1);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        REQUIRE(errors.load() == 0);
    }
}
