#pragma once

#include "User.h"
#include <string>
#include <chrono>
#include <optional>

namespace search_engine { namespace auth {

/**
 * @brief Claims encoded into and decoded from a JWT — issue #13.
 *
 * Minimal set tailored to this app:
 *   sub:      user id
 *   username: user's username
 *   role:     "user" | "admin"
 *   iat:      issued-at epoch seconds
 *   exp:      expiry epoch seconds
 */
struct JwtClaims {
    std::string sub;
    std::string username;
    Role role{Role::USER};
    int64_t iat{0};
    int64_t exp{0};
};

/**
 * @brief HS256 JWT issuer/verifier backed by OpenSSL HMAC.
 *
 * The secret should come from env JWT_SECRET (production). For dev a
 * fallback is allowed but a warning is logged at first use.
 *
 * Implementation in src/auth/Jwt.cpp.
 */
class JwtIssuer {
public:
    explicit JwtIssuer(std::string secret,
                       std::chrono::seconds defaultTtl = std::chrono::hours(24))
        : secret_(std::move(secret)), defaultTtl_(defaultTtl) {}

    // Build the secret from the JWT_SECRET env var; falls back to a built-in
    // dev secret (and the caller should log a warning).
    static std::string secretFromEnvOrDev();

    // Issue a signed JWT for the given user.
    std::string issue(const User& user) const;
    std::string issue(const User& user, std::chrono::seconds ttl) const;

    // Verify token signature + expiration. Returns claims on success.
    std::optional<JwtClaims> verify(const std::string& token) const;

private:
    std::string secret_;
    std::chrono::seconds defaultTtl_;
};

}}  // namespace
