#include "../../include/search_engine/storage/DataEncryption.h"
#include "../../include/Logger.h"
#include <iostream>
#include <cassert>
#include <cstdlib>

using namespace search_engine::storage;

void testEncryptionRoundTrip() {
    std::cout << "Test: Encryption round-trip" << std::endl;
    
    // Set a test encryption key (32 bytes for AES-256)
    setenv("COMPLIANCE_ENCRYPTION_KEY", "0123456789abcdef0123456789abcdef", 1);
    
    std::string key = DataEncryption::getEncryptionKey();
    std::string plaintext = "user@example.com";
    
    std::string encrypted = DataEncryption::encrypt(plaintext, key);
    std::cout << "  Original: " << plaintext << std::endl;
    std::cout << "  Encrypted: " << encrypted.substr(0, 40) << "..." << std::endl;
    
    // Encrypted should be different from original
    assert(encrypted != plaintext);
    assert(!encrypted.empty());
    
    std::string decrypted = DataEncryption::decrypt(encrypted, key);
    std::cout << "  Decrypted: " << decrypted << std::endl;
    
    // Decrypted should match original
    assert(decrypted == plaintext);
    
    std::cout << "  ✓ Round-trip successful" << std::endl;
}

void testEmptyString() {
    std::cout << "\nTest: Empty string encryption" << std::endl;
    
    std::string key = DataEncryption::getEncryptionKey();
    std::string plaintext = "";
    
    std::string encrypted = DataEncryption::encrypt(plaintext, key);
    assert(encrypted.empty());
    
    std::string decrypted = DataEncryption::decrypt(encrypted, key);
    assert(decrypted.empty());
    
    std::cout << "  ✓ Empty string handled correctly" << std::endl;
}

void testSecureMemoryWipe() {
    std::cout << "\nTest: Secure memory wipe" << std::endl;
    
    std::string sensitive = "192.168.1.1";
    std::cout << "  Before wipe: " << sensitive << std::endl;
    
    secureMemoryWipe(&sensitive);
    
    std::cout << "  After wipe: '" << sensitive << "'" << std::endl;
    assert(sensitive.empty());
    
    std::cout << "  ✓ Memory wiped successfully" << std::endl;
}

void testMultipleEncryptions() {
    std::cout << "\nTest: Multiple encryptions produce different ciphertexts" << std::endl;
    
    std::string key = DataEncryption::getEncryptionKey();
    std::string plaintext = "test@example.com";
    
    std::string encrypted1 = DataEncryption::encrypt(plaintext, key);
    std::string encrypted2 = DataEncryption::encrypt(plaintext, key);
    
    // Different IVs mean different ciphertexts
    assert(encrypted1 != encrypted2);
    std::cout << "  Encryption 1: " << encrypted1.substr(0, 40) << "..." << std::endl;
    std::cout << "  Encryption 2: " << encrypted2.substr(0, 40) << "..." << std::endl;
    
    // But both decrypt to same plaintext
    std::string decrypted1 = DataEncryption::decrypt(encrypted1, key);
    std::string decrypted2 = DataEncryption::decrypt(encrypted2, key);
    
    assert(decrypted1 == plaintext);
    assert(decrypted2 == plaintext);
    
    std::cout << "  ✓ Different ciphertexts, same plaintext" << std::endl;
}

int main() {
    std::cout << "=== Privacy Architecture Encryption Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        testEncryptionRoundTrip();
        testEmptyString();
        testSecureMemoryWipe();
        testMultipleEncryptions();
        
        std::cout << "\n=== All tests passed! ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
