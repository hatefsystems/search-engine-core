#include "../../include/search_engine/storage/DataEncryption.h"
#include "../../include/Logger.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <cstring>
#include <vector>
#include <sstream>
#include <iomanip>

namespace search_engine {
namespace storage {

// Base64 encoding/decoding helpers
static std::string base64Encode(const unsigned char* data, size_t len) {
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    result.reserve(((len + 2) / 3) * 4);
    
    for (size_t i = 0; i < len; i += 3) {
        unsigned int val = (data[i] << 16);
        if (i + 1 < len) val |= (data[i + 1] << 8);
        if (i + 2 < len) val |= data[i + 2];
        
        result.push_back(base64_chars[(val >> 18) & 0x3F]);
        result.push_back(base64_chars[(val >> 12) & 0x3F]);
        result.push_back((i + 1 < len) ? base64_chars[(val >> 6) & 0x3F] : '=');
        result.push_back((i + 2 < len) ? base64_chars[val & 0x3F] : '=');
    }
    
    return result;
}

static std::vector<unsigned char> base64Decode(const std::string& encoded) {
    static const int base64_table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    std::vector<unsigned char> result;
    result.reserve((encoded.length() * 3) / 4);
    
    unsigned int val = 0;
    int bits = -8;
    
    for (unsigned char c : encoded) {
        if (base64_table[c] == -1) continue;
        val = (val << 6) | base64_table[c];
        bits += 6;
        
        if (bits >= 0) {
            result.push_back((val >> bits) & 0xFF);
            bits -= 8;
        }
    }
    
    return result;
}

std::string DataEncryption::encrypt(const std::string& plaintext, const std::string& key) {
    if (plaintext.empty()) {
        return "";
    }
    
    if (!isValidKeyLength(key)) {
        throw std::runtime_error("Invalid key length for AES-256. Key must be 32 bytes.");
    }
    
    // Generate random IV
    unsigned char iv[GCM_IV_SIZE];
    if (RAND_bytes(iv, GCM_IV_SIZE) != 1) {
        throw std::runtime_error("Failed to generate random IV");
    }
    
    // Initialize cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    try {
        // Initialize encryption
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                               reinterpret_cast<const unsigned char*>(key.data()), iv) != 1) {
            throw std::runtime_error("Failed to initialize encryption");
        }
        
        // Encrypt plaintext
        std::vector<unsigned char> ciphertext(plaintext.length() + EVP_CIPHER_CTX_block_size(ctx));
        int len = 0;
        
        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                             reinterpret_cast<const unsigned char*>(plaintext.data()),
                             plaintext.length()) != 1) {
            throw std::runtime_error("Encryption failed");
        }
        
        int ciphertext_len = len;
        
        // Finalize encryption
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
            throw std::runtime_error("Failed to finalize encryption");
        }
        
        ciphertext_len += len;
        ciphertext.resize(ciphertext_len);
        
        // Get authentication tag
        unsigned char tag[GCM_TAG_SIZE];
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, tag) != 1) {
            throw std::runtime_error("Failed to get authentication tag");
        }
        
        // Combine IV + ciphertext + tag
        std::vector<unsigned char> combined;
        combined.reserve(GCM_IV_SIZE + ciphertext_len + GCM_TAG_SIZE);
        combined.insert(combined.end(), iv, iv + GCM_IV_SIZE);
        combined.insert(combined.end(), ciphertext.begin(), ciphertext.end());
        combined.insert(combined.end(), tag, tag + GCM_TAG_SIZE);
        
        EVP_CIPHER_CTX_free(ctx);
        
        // Base64 encode the result
        return base64Encode(combined.data(), combined.size());
        
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
}

std::string DataEncryption::decrypt(const std::string& ciphertext, const std::string& key) {
    if (ciphertext.empty()) {
        return "";
    }
    
    if (!isValidKeyLength(key)) {
        throw std::runtime_error("Invalid key length for AES-256. Key must be 32 bytes.");
    }
    
    // Base64 decode
    std::vector<unsigned char> combined = base64Decode(ciphertext);
    
    if (combined.size() < GCM_IV_SIZE + GCM_TAG_SIZE) {
        throw std::runtime_error("Invalid ciphertext: too short");
    }
    
    // Extract IV, ciphertext, and tag
    const unsigned char* iv = combined.data();
    const unsigned char* encrypted_data = combined.data() + GCM_IV_SIZE;
    size_t encrypted_len = combined.size() - GCM_IV_SIZE - GCM_TAG_SIZE;
    const unsigned char* tag = combined.data() + GCM_IV_SIZE + encrypted_len;
    
    // Initialize cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    try {
        // Initialize decryption
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                               reinterpret_cast<const unsigned char*>(key.data()), iv) != 1) {
            throw std::runtime_error("Failed to initialize decryption");
        }
        
        // Decrypt ciphertext
        std::vector<unsigned char> plaintext(encrypted_len + EVP_CIPHER_CTX_block_size(ctx));
        int len = 0;
        
        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, encrypted_data, encrypted_len) != 1) {
            throw std::runtime_error("Decryption failed");
        }
        
        int plaintext_len = len;
        
        // Set expected tag
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE,
                                const_cast<unsigned char*>(tag)) != 1) {
            throw std::runtime_error("Failed to set authentication tag");
        }
        
        // Finalize decryption (verifies authentication tag)
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Decryption failed: authentication tag mismatch");
        }
        
        plaintext_len += len;
        plaintext.resize(plaintext_len);
        
        EVP_CIPHER_CTX_free(ctx);
        
        return std::string(plaintext.begin(), plaintext.end());
        
    } catch (...) {
        EVP_CIPHER_CTX_free(ctx);
        throw;
    }
}

std::string DataEncryption::getEncryptionKey() {
    const char* keyEnv = std::getenv("COMPLIANCE_ENCRYPTION_KEY");
    
    if (!keyEnv) {
        throw std::runtime_error(
            "COMPLIANCE_ENCRYPTION_KEY environment variable not set. "
            "Set a 32-byte (64 hex character) key for AES-256 encryption."
        );
    }
    
    std::string key(keyEnv);
    
    if (!isValidKeyLength(key)) {
        throw std::runtime_error(
            "COMPLIANCE_ENCRYPTION_KEY must be exactly 32 bytes (64 hex characters) for AES-256. "
            "Current length: " + std::to_string(key.length())
        );
    }
    
    return key;
}

bool DataEncryption::isValidKeyLength(const std::string& key) {
    return key.length() == AES_256_KEY_SIZE;
}

void secureMemoryWipe(std::string* str) {
    if (!str || str->empty()) {
        return;
    }
    
    // Overwrite the string's internal buffer with zeros
    // Note: This uses non-const data() which is available in C++17
    volatile char* ptr = const_cast<volatile char*>(str->data());
    size_t len = str->length();
    
    for (size_t i = 0; i < len; ++i) {
        ptr[i] = 0;
    }
    
    // Clear the string (deallocates memory)
    str->clear();
    
    LOG_DEBUG("Securely wiped sensitive data from memory");
}

} // namespace storage
} // namespace search_engine
