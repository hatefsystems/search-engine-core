#include "search_engine/auth/Jwt.h"
#include "search_engine/auth/Base64Url.h"

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <stdexcept>
#include <sstream>
#include <vector>

namespace search_engine { namespace auth {

namespace {
    int64_t nowEpochSeconds() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string hmacSha256(const std::string& key, const std::string& msg) {
        unsigned char mac[EVP_MAX_MD_SIZE];
        unsigned int macLen = 0;
        if (HMAC(EVP_sha256(),
                 key.data(), static_cast<int>(key.size()),
                 reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
                 mac, &macLen) == nullptr) {
            throw std::runtime_error("HMAC failed");
        }
        return Base64Url::encode(mac, macLen);
    }

    // Constant-time string comparison.
    bool constantTimeEqual(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        unsigned char diff = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
        }
        return diff == 0;
    }
}

std::string JwtIssuer::secretFromEnvOrDev() {
    const char* env = std::getenv("JWT_SECRET");
    if (env && *env) return std::string(env);
    // Dev fallback — caller should log a warning at startup.
    return "dev-jwt-secret-please-override-via-JWT_SECRET-env";
}

std::string JwtIssuer::issue(const User& user) const {
    return issue(user, defaultTtl_);
}

std::string JwtIssuer::issue(const User& user, std::chrono::seconds ttl) const {
    nlohmann::json header = {
        {"alg", "HS256"},
        {"typ", "JWT"}
    };
    auto iat = nowEpochSeconds();
    auto exp = iat + ttl.count();
    nlohmann::json payload = {
        {"sub", user.id},
        {"username", user.username},
        {"role", roleToString(user.role)},
        {"iat", iat},
        {"exp", exp}
    };
    auto h = header.dump();
    auto p = payload.dump();
    std::string hb = Base64Url::encode(h);
    std::string pb = Base64Url::encode(p);
    std::string signingInput = hb + "." + pb;
    std::string sig = hmacSha256(secret_, signingInput);
    return signingInput + "." + sig;
}

std::optional<JwtClaims> JwtIssuer::verify(const std::string& token) const {
    auto firstDot = token.find('.');
    if (firstDot == std::string::npos) return std::nullopt;
    auto secondDot = token.find('.', firstDot + 1);
    if (secondDot == std::string::npos) return std::nullopt;

    std::string headerB64 = token.substr(0, firstDot);
    std::string payloadB64 = token.substr(firstDot + 1, secondDot - firstDot - 1);
    std::string signature = token.substr(secondDot + 1);

    // Re-compute signature.
    std::string signingInput = headerB64 + "." + payloadB64;
    std::string expectedSig;
    try {
        expectedSig = hmacSha256(secret_, signingInput);
    } catch (...) {
        return std::nullopt;
    }
    if (!constantTimeEqual(signature, expectedSig)) return std::nullopt;

    // Decode header — sanity check alg=HS256.
    auto headerBytes = Base64Url::decode(headerB64);
    if (headerBytes.empty()) return std::nullopt;
    nlohmann::json header;
    try {
        header = nlohmann::json::parse(std::string(headerBytes.begin(), headerBytes.end()));
    } catch (...) { return std::nullopt; }
    if (!header.contains("alg") || header["alg"] != "HS256") return std::nullopt;

    // Decode payload.
    auto payloadBytes = Base64Url::decode(payloadB64);
    if (payloadBytes.empty()) return std::nullopt;
    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(std::string(payloadBytes.begin(), payloadBytes.end()));
    } catch (...) { return std::nullopt; }

    JwtClaims c;
    if (payload.contains("sub")) c.sub = payload["sub"].get<std::string>();
    if (payload.contains("username")) c.username = payload["username"].get<std::string>();
    if (payload.contains("role")) c.role = roleFromString(payload["role"].get<std::string>());
    if (payload.contains("iat")) c.iat = payload["iat"].get<int64_t>();
    if (payload.contains("exp")) c.exp = payload["exp"].get<int64_t>();

    // Expiry check.
    if (c.exp > 0 && nowEpochSeconds() >= c.exp) return std::nullopt;
    return c;
}

}}  // namespace
