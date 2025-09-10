#include "search_core/SearchClient.hpp"
#include <sw/redis++/redis++.h>
#include <hiredis/hiredis.h>
#include <atomic>
#include <vector>
#include <mutex>
#include <sstream>
#include <iostream>
#include "../../include/Logger.h"

namespace hatef::search {

struct SearchClient::Impl {
    std::vector<std::unique_ptr<sw::redis::Redis>> connections;
    std::atomic<std::size_t> cursor{0};
    RedisConfig config;

    explicit Impl(RedisConfig cfg) : config(std::move(cfg)) {
        LOG_INFO("🔗 SearchClient::Impl - Initializing Redis client pool");
        LOG_DEBUG("SearchClient::Impl - URI: " + config.uri + ", pool_size: " + std::to_string(config.pool_size));

        try {
            connections.reserve(config.pool_size);
            LOG_DEBUG("SearchClient::Impl - Creating " + std::to_string(config.pool_size) + " Redis connections");

            for (std::size_t i = 0; i < config.pool_size; ++i) {
                LOG_DEBUG("SearchClient::Impl - Creating connection " + std::to_string(i+1) +
                          "/" + std::to_string(config.pool_size));

                auto redis = std::make_unique<sw::redis::Redis>(config.uri);

                // Test the connection by pinging
                LOG_DEBUG("SearchClient::Impl - Testing connection " + std::to_string(i+1) + " with PING");
                redis->ping();
                LOG_DEBUG("✅ SearchClient::Impl - PING successful for connection " + std::to_string(i+1));

                connections.push_back(std::move(redis));
            }

            LOG_INFO("✅ SearchClient::Impl - Successfully initialized " + std::to_string(connections.size()) +
                     " Redis connections");
        } catch (const sw::redis::Error& e) {
            LOG_ERROR("💥 SearchClient::Impl - Redis error during initialization: " + std::string(e.what()));
            throw SearchError("Failed to initialize Redis connections: " + std::string(e.what()));
        } catch (const std::exception& e) {
            LOG_ERROR("💥 SearchClient::Impl - Standard exception during initialization: " + std::string(e.what()));
            throw SearchError("Failed to connect to Redis: " + std::string(e.what()));
        }
    }

    sw::redis::Redis& getConnection() {
        auto idx = cursor.fetch_add(1, std::memory_order_relaxed) % connections.size();
        return *connections[idx];
    }
};

SearchClient::SearchClient(RedisConfig cfg)
    : p_(std::make_unique<Impl>(std::move(cfg))) {
    LOG_INFO("✅ SearchClient::SearchClient - Constructor completed successfully");
    LOG_DEBUG("SearchClient::SearchClient - Redis client ready for search operations");
}

SearchClient::~SearchClient() = default;

std::string SearchClient::search(std::string_view index,
                                std::string_view query,
                                const std::vector<std::string>& args) {
    LOG_INFO("🔍 SearchClient::search - Starting search operation");
    LOG_DEBUG("SearchClient::search - Index: " + std::string(index) +
              ", Query: " + std::string(query) +
              ", Args: " + std::to_string(args.size()));

    try {
        auto& redis = p_->getConnection();
        LOG_DEBUG("SearchClient::search - Acquired Redis connection from pool");

        // Build the command arguments
        std::vector<std::string> cmd_args;
        cmd_args.reserve(3 + args.size());
        cmd_args.emplace_back("FT.SEARCH");
        cmd_args.emplace_back(index);
        cmd_args.emplace_back(query);

        // Add additional arguments
        for (const auto& arg : args) {
            cmd_args.push_back(arg);
        }

        LOG_DEBUG("SearchClient::search - Executing FT.SEARCH with " +
                  std::to_string(cmd_args.size()) + " arguments");

        // Execute the command and get the result
        auto reply = redis.command(cmd_args.begin(), cmd_args.end());

        LOG_DEBUG("✅ SearchClient::search - Redis command executed successfully");
        
        // Convert raw Redis reply to a simple string representation
        // This is a basic implementation - in production you'd want more robust parsing
        std::ostringstream oss;
        
        if (reply && reply->type == REDIS_REPLY_ARRAY) {
            oss << "[";
            for (size_t i = 0; i < reply->elements; ++i) {
                if (i > 0) oss << ",";
                auto element = reply->element[i];
                if (element && element->type == REDIS_REPLY_STRING) {
                    oss << "\"" << std::string(element->str, element->len) << "\"";
                } else if (element && element->type == REDIS_REPLY_INTEGER) {
                    oss << element->integer;
                } else {
                    oss << "null";
                }
            }
            oss << "]";
        } else if (reply && reply->type == REDIS_REPLY_STRING) {
            oss << "\"" << std::string(reply->str, reply->len) << "\"";
        } else if (reply && reply->type == REDIS_REPLY_INTEGER) {
            oss << reply->integer;
        } else {
            oss << "null";
        }
        
        return oss.str();
    } catch (const sw::redis::Error& e) {
        throw SearchError("Search failed: " + std::string(e.what()));
    }
}

} // namespace hatef::search 