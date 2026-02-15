#include "include/search_engine/common/SlugGenerator.h"
#include <iostream>
#include <string>

int main() {
    std::cout << "Testing SlugGenerator..." << std::endl;

    // Test basic functionality
    std::string slug1 = search_engine::common::SlugGenerator::generateSlug("John Doe");
    std::cout << "John Doe -> " << slug1 << std::endl;

    std::string slug2 = search_engine::common::SlugGenerator::generateSlug("علی رضایی");
    std::cout << "علی رضایی -> " << slug2 << std::endl;

    std::string slug3 = search_engine::common::SlugGenerator::generateSlug("Test@User.com");
    std::cout << "Test@User.com -> " << slug3 << std::endl;

    // Test collision resolution
    auto existsFunc = [](const std::string& s) { return s == "test-slug"; };
    std::string resolved = search_engine::common::SlugGenerator::resolveSlugConflict("test-slug", existsFunc);
    std::cout << "test-slug conflict -> " << resolved << std::endl;

    // Test reserved words
    bool isReserved = search_engine::common::SlugGenerator::isReservedSlug("api");
    std::cout << "Is 'api' reserved? " << (isReserved ? "Yes" : "No") << std::endl;

    bool isReserved2 = search_engine::common::SlugGenerator::isReservedSlug("john-doe");
    std::cout << "Is 'john-doe' reserved? " << (isReserved2 ? "Yes" : "No") << std::endl;

    std::cout << "SlugGenerator tests completed!" << std::endl;
    return 0;
}
