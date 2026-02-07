cmake_minimum_required(VERSION 3.15)

# Privacy architecture tests
add_executable(test_encryption test_encryption.cpp)

target_link_libraries(test_encryption
    PRIVATE
        storage
        common
        OpenSSL::SSL
        OpenSSL::Crypto
)

target_include_directories(test_encryption
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Add test to CTest
add_test(NAME EncryptionTests COMMAND test_encryption)
