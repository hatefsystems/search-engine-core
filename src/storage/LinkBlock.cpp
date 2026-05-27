#include "../../include/search_engine/storage/LinkBlock.h"
#include "../../include/search_engine/storage/ProfileValidator.h"
#include <stdexcept>

namespace search_engine {
namespace storage {

std::string linkPrivacyToString(LinkPrivacy privacy) {
    switch (privacy) {
        case LinkPrivacy::PUBLIC:   return "PUBLIC";
        case LinkPrivacy::HIDDEN:   return "HIDDEN";
        case LinkPrivacy::DISABLED: return "DISABLED";
        default:
            throw std::invalid_argument("Unknown LinkPrivacy value");
    }
}

LinkPrivacy stringToLinkPrivacy(const std::string& str) {
    if (str == "PUBLIC")   return LinkPrivacy::PUBLIC;
    if (str == "HIDDEN")   return LinkPrivacy::HIDDEN;
    if (str == "DISABLED") return LinkPrivacy::DISABLED;
    throw std::invalid_argument("Invalid LinkPrivacy string: " + str);
}

bool LinkBlock::isValid() const {
    // Required: profileId
    if (profileId.empty()) {
        return false;
    }
    
    // Required: url (must be valid http/https)
    if (url.empty() || !ProfileValidator::isValidUrl(url)) {
        return false;
    }
    
    // Required: title (non-empty, max 200 chars)
    if (title.empty() || title.length() > 200) {
        return false;
    }
    
    // Optional: description (max 500 chars if present)
    if (description.has_value() && description.value().length() > 500) {
        return false;
    }
    
    // Optional: iconUrl (must be valid URL if present)
    if (iconUrl.has_value() && !iconUrl.value().empty()) {
        if (!ProfileValidator::isValidUrl(iconUrl.value())) {
            return false;
        }
    }
    
    // sortOrder should be non-negative
    if (sortOrder < 0) {
        return false;
    }
    
    return true;
}

} // namespace storage
} // namespace search_engine
