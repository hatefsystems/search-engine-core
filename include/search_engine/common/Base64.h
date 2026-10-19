#pragma once

#include <string>
#include <vector>

namespace search_engine {
namespace common {

/**
 * @brief Base64 encoding and decoding utilities
 * 
 * Provides functions to encode binary data to Base64 and decode Base64 strings to binary
 */
class Base64 {
public:
    /**
     * @brief Encode binary data to Base64 string
     * @param data Binary data to encode
     * @return Base64 encoded string
     */
    static std::string encode(const std::vector<unsigned char>& data);
    
    /**
     * @brief Encode binary data to Base64 string
     * @param data Pointer to binary data
     * @param length Length of binary data
     * @return Base64 encoded string
     */
    static std::string encode(const unsigned char* data, size_t length);
    
    /**
     * @brief Decode Base64 string to binary data
     * @param base64String Base64 encoded string
     * @return Decoded binary data
     */
    static std::vector<unsigned char> decode(const std::string& base64String);
    
    /**
     * @brief Check if a string is valid Base64
     * @param str String to check
     * @return true if valid Base64, false otherwise
     */
    static bool isValidBase64(const std::string& str);

private:
    static const std::string BASE64_CHARS;
    static inline bool isBase64(unsigned char c);
};

} // namespace common
} // namespace search_engine
