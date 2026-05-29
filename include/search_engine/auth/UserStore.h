#pragma once

#include "User.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <optional>
#include <atomic>
#include <sstream>

namespace search_engine { namespace auth {

/**
 * @brief Storage interface for users (issue #13).
 *
 * In-memory default impl is provided. A MongoDB-backed implementation can
 * subclass without changing callers.
 */
class IUserStore {
public:
    virtual ~IUserStore() = default;

    // Returns false if username already exists. Generates id and stamps createdAt.
    virtual bool create(User& user) = 0;
    virtual std::optional<User> findByUsername(const std::string& username) const = 0;
    virtual std::optional<User> findById(const std::string& id) const = 0;
    virtual std::vector<User> all() const = 0;
    virtual size_t size() const = 0;
    virtual void clear() = 0;
};

class InMemoryUserStore : public IUserStore {
public:
    bool create(User& user) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (byUsername_.find(user.username) != byUsername_.end()) {
            return false;
        }
        if (user.id.empty()) {
            // Simple monotonic id; replace with UUID for production.
            user.id = "u_" + std::to_string(++counter_);
        }
        if (user.createdAt.time_since_epoch().count() == 0) {
            user.createdAt = std::chrono::system_clock::now();
        }
        users_[user.id] = user;
        byUsername_[user.username] = user.id;
        return true;
    }

    std::optional<User> findByUsername(const std::string& username) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = byUsername_.find(username);
        if (it == byUsername_.end()) return std::nullopt;
        auto uit = users_.find(it->second);
        if (uit == users_.end()) return std::nullopt;
        return uit->second;
    }

    std::optional<User> findById(const std::string& id) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = users_.find(id);
        if (it == users_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<User> all() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<User> out;
        out.reserve(users_.size());
        for (const auto& [_, u] : users_) out.push_back(u);
        return out;
    }

    size_t size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return users_.size();
    }

    void clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        users_.clear();
        byUsername_.clear();
        counter_ = 0;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, User> users_;        // id -> user
    std::unordered_map<std::string, std::string> byUsername_;  // username -> id
    uint64_t counter_{0};
};

}}  // namespace
