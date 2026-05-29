#pragma once

#include <string>
#include <chrono>

namespace search_engine { namespace auth {

/**
 * @brief User roles for role-based access control (issue #13).
 *
 * - USER: can only see and operate on their own resources (crawl sessions, etc.)
 * - ADMIN: can see and operate on resources of any user.
 */
enum class Role {
    USER = 0,
    ADMIN = 1
};

inline const char* roleToString(Role r) {
    switch (r) {
        case Role::USER: return "user";
        case Role::ADMIN: return "admin";
    }
    return "user";
}

inline Role roleFromString(const std::string& s) {
    if (s == "admin" || s == "ADMIN") return Role::ADMIN;
    return Role::USER;
}

/**
 * @brief Persistent user record.
 *
 * passwordHash stores the encoded PBKDF2 output ("iter$saltB64$hashB64");
 * never the plaintext password.
 */
struct User {
    std::string id;
    std::string username;
    std::string passwordHash;
    Role role{Role::USER};
    std::chrono::system_clock::time_point createdAt{std::chrono::system_clock::now()};
};

/**
 * @brief The authenticated user context for a single request.
 *
 * Built from a verified JWT — does NOT include the password hash. Passed to
 * any code that needs to make ownership / authorization decisions.
 */
struct AuthContext {
    std::string userId;
    std::string username;
    Role role{Role::USER};

    bool isAdmin() const { return role == Role::ADMIN; }
};

}}  // namespace
