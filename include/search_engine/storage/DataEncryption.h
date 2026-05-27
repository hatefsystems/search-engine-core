#pragma once

#include <string>
#include <stdexcept>

namespace search_engine {
namespace storage {

/**
 * @brief AES-256-GCM encryption utility for sensitive data
 * 
 * Provides secure encryption/decryption for profile PII and compliance data.
 * Uses AES-256-GCM for authenticated encryption.
 * 
 * Key management:
 * - Reads encryption key from COMPLIANCE_ENCRYPTION_KEY environment variable
 * - Key must be 32 bytes (64 hex characters) for AES-256
 * - Single key for MVP; key rotation is a future enhancement
 */
class DataEncryption {
public:
    /**
     * @brief Encrypt plaintext using AES-256-GCM
     * @param plaintext The data to encrypt
     * @param key Encryption key (32 bytes for AES-256)
     * @return Base64-encoded ciphertext with IV and auth tag
     * @throws std::runtime_error on encryption failure
     */
    static std::string encrypt(const std::string& plaintext, const std::string& key);

    /**
     * @brief Decrypt ciphertext using AES-256-GCM
     * @param ciphertext Base64-encoded encrypted data with IV and auth tag
     * @param key Decryption key (must match encryption key)
     * @return Decrypted plaintext
     * @throws std::runtime_error on decryption failure or authentication failure
     */
    static std::string decrypt(const std::string& ciphertext, const std::string& key);

    /**
     * @brief Get encryption key from environment
     * @return Encryption key from COMPLIANCE_ENCRYPTION_KEY env var
     * @throws std::runtime_error if key is not set or invalid length
     */
    static std::string getEncryptionKey();

    /**
     * @brief Validate that key is correct length for AES-256
     * @param key Key to validate
     * @return true if key is 32 bytes, false otherwise
     */
    static bool isValidKeyLength(const std::string& key);

private:
    static constexpr size_t AES_256_KEY_SIZE = 32;  // 256 bits
    static constexpr size_t GCM_IV_SIZE = 12;       // 96 bits (recommended for GCM)
    static constexpr size_t GCM_TAG_SIZE = 16;      // 128 bits authentication tag
};

/**
 * @brief Securely wipe sensitive data from memory
 * @param str String containing sensitive data to wipe
 * 
 * Overwrites string buffer with zeros before clearing.
 * Defense-in-depth measure against memory dumps and debugging.
 */
void secureMemoryWipe(std::string* str);

} // namespace storage
} // namespace search_engine
