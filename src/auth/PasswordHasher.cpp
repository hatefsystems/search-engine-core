#include "search_engine/auth/PasswordHasher.h"
#include "search_engine/auth/Base64Url.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

namespace search_engine { namespace auth {

std::string PasswordHasher::hash(const std::string& password, int iterations) {
    if (iterations <= 0) iterations = DEFAULT_ITERATIONS;

    unsigned char salt[SALT_BYTES];
    if (RAND_bytes(salt, SALT_BYTES) != 1) {
        throw std::runtime_error("PasswordHasher: RAND_bytes failed");
    }

    unsigned char out[HASH_BYTES];
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          salt, SALT_BYTES,
                          iterations,
                          EVP_sha256(),
                          HASH_BYTES, out) != 1) {
        throw std::runtime_error("PasswordHasher: PKCS5_PBKDF2_HMAC failed");
    }

    std::ostringstream ss;
    ss << iterations << "$"
       << Base64Url::encode(salt, SALT_BYTES) << "$"
       << Base64Url::encode(out, HASH_BYTES);
    return ss.str();
}

bool PasswordHasher::verify(const std::string& password, const std::string& stored) {
    // Parse "iter$saltB64$hashB64".
    auto firstDollar = stored.find('$');
    if (firstDollar == std::string::npos) return false;
    auto secondDollar = stored.find('$', firstDollar + 1);
    if (secondDollar == std::string::npos) return false;

    int iterations = 0;
    try {
        iterations = std::stoi(stored.substr(0, firstDollar));
    } catch (...) {
        return false;
    }
    if (iterations <= 0) return false;

    auto saltStr = stored.substr(firstDollar + 1, secondDollar - firstDollar - 1);
    auto hashStr = stored.substr(secondDollar + 1);

    auto salt = Base64Url::decode(saltStr);
    auto expected = Base64Url::decode(hashStr);
    if (salt.empty() || expected.empty()) return false;
    if (expected.size() != HASH_BYTES) return false;

    std::vector<unsigned char> candidate(HASH_BYTES);
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                          salt.data(), static_cast<int>(salt.size()),
                          iterations,
                          EVP_sha256(),
                          HASH_BYTES, candidate.data()) != 1) {
        return false;
    }

    // Constant-time comparison.
    unsigned char diff = 0;
    for (size_t i = 0; i < HASH_BYTES; ++i) {
        diff |= candidate[i] ^ expected[i];
    }
    return diff == 0;
}

}}  // namespace
