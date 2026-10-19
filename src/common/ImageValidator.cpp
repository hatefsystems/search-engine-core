#include "../../include/search_engine/common/ImageValidator.h"
#include <algorithm>

namespace search_engine {
namespace common {

ImageValidator::ImageInfo ImageValidator::validate(const std::vector<unsigned char>& data, size_t maxSize) {
    ImageInfo info;
    info.size = data.size();
    info.isValid = false;
    
    // Check if data is empty
    if (data.empty()) {
        info.type = ImageType::UNKNOWN;
        info.mimeType = "";
        return info;
    }
    
    // Check size limit
    if (info.size > maxSize) {
        info.type = ImageType::UNKNOWN;
        info.mimeType = "";
        return info;
    }
    
    // Detect image type
    info.type = detectType(data);
    info.mimeType = getMimeType(info.type);
    
    // Validate type is supported
    info.isValid = isSupportedType(info.type);
    
    return info;
}

ImageValidator::ImageType ImageValidator::detectType(const std::vector<unsigned char>& data) {
    if (data.size() < 12) {
        return ImageType::UNKNOWN;
    }
    
    // JPEG magic bytes: FF D8 FF
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return ImageType::JPEG;
    }
    
    // PNG magic bytes: 89 50 4E 47 0D 0A 1A 0A
    if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
        data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A) {
        return ImageType::PNG;
    }
    
    // GIF magic bytes: 47 49 46 38 (GIF8)
    if (data[0] == 0x47 && data[1] == 0x49 && data[2] == 0x46 && data[3] == 0x38) {
        return ImageType::GIF;
    }
    
    // WebP magic bytes: 52 49 46 46 ... 57 45 42 50 (RIFF....WEBP)
    if (data.size() >= 12 &&
        data[0] == 0x52 && data[1] == 0x49 && data[2] == 0x46 && data[3] == 0x46 &&
        data[8] == 0x57 && data[9] == 0x45 && data[10] == 0x42 && data[11] == 0x50) {
        return ImageType::WEBP;
    }
    
    return ImageType::UNKNOWN;
}

std::string ImageValidator::getMimeType(ImageType type) {
    switch (type) {
        case ImageType::JPEG:
            return "image/jpeg";
        case ImageType::PNG:
            return "image/png";
        case ImageType::GIF:
            return "image/gif";
        case ImageType::WEBP:
            return "image/webp";
        default:
            return "";
    }
}

std::string ImageValidator::getExtension(ImageType type) {
    switch (type) {
        case ImageType::JPEG:
            return "jpg";
        case ImageType::PNG:
            return "png";
        case ImageType::GIF:
            return "gif";
        case ImageType::WEBP:
            return "webp";
        default:
            return "";
    }
}

bool ImageValidator::isSupportedType(ImageType type) {
    return type == ImageType::JPEG || 
           type == ImageType::PNG || 
           type == ImageType::GIF || 
           type == ImageType::WEBP;
}

} // namespace common
} // namespace search_engine
