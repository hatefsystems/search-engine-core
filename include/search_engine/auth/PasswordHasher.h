#pragma once

#include <string>

namespace search_engine { namespace auth {

/**
 * @brief Password hashing (PBKDF2-HMAC-SHA256) — issue #13.
 *
 * Encoded form: "iter$saltB64$hashB64" (base64url, no padding).
 * Stored on User::passwordHash; never the plaintext.
 *
 * Implementation lives in src/auth/PasswordHasher.cpp (links OpenSSL).
 */
class PasswordHasher {
public:
    // Default iteration count. Tune up over time; verify is parameterized
    // by the iteration count embedded in the stored string, so existing
    // hashes keep working.
    static constexpr int DEFAULT_ITERATIONS = 100000;
    static constexpr size_t SALT_BYTES = 16;
    static constexpr size_t HASH_BYTES = 32;  // SHA-256

    // Hash a plaintext password. Returns encoded string ready to store.
    static std::string hash(const std::string& password,
                            int iterations = DEFAULT_ITERATIONS);

    // Verify a candidate password against a stored encoded hash.
    // Returns false for any malformed input (constant-time on mismatch).
    static bool verify(const std::string& password, const std::string& stored);
};

}}  // namespace
