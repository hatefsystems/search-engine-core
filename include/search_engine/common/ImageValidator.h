#pragma once

#include <string>
#include <vector>

namespace search_engine {
namespace common {

/**
 * @brief Image validation utilities
 * 
 * Provides functions to validate image types, sizes, and basic properties
 */
class ImageValidator {
public:
    enum class ImageType {
        JPEG,
        PNG,
        GIF,
        WEBP,
        UNKNOWN
    };
    
    struct ImageInfo {
        ImageType type;
        size_t size;
        bool isValid;
        std::string mimeType;
    };
    
    /**
     * @brief Validate image data
     * @param data Image binary data
     * @param maxSize Maximum allowed size in bytes
     * @return ImageInfo with validation results
     */
    static ImageInfo validate(const std::vector<unsigned char>& data, size_t maxSize);
    
    /**
     * @brief Detect image type from magic bytes
     * @param data Image binary data (at least first 12 bytes)
     * @return Detected image type
     */
    static ImageType detectType(const std::vector<unsigned char>& data);
    
    /**
     * @brief Get MIME type for image type
     * @param type Image type
     * @return MIME type string
     */
    static std::string getMimeType(ImageType type);
    
    /**
     * @brief Get file extension for image type
     * @param type Image type
     * @return File extension (without dot)
     */
    static std::string getExtension(ImageType type);
    
    /**
     * @brief Check if image type is supported
     * @param type Image type
     * @return true if supported, false otherwise
     */
    static bool isSupportedType(ImageType type);
    
    // Size limits
    static constexpr size_t MAX_AVATAR_SIZE = 5 * 1024 * 1024;    // 5 MB
    static constexpr size_t MAX_COVER_SIZE = 10 * 1024 * 1024;    // 10 MB
};

} // namespace common
} // namespace search_engine
