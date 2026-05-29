#include <catch2/catch_test_macros.hpp>
#include "search_engine/auth/User.h"
#include "search_engine/auth/UserStore.h"
#include "search_engine/auth/PasswordHasher.h"
#include "search_engine/auth/Base64Url.h"
#include "search_engine/auth/Jwt.h"
#include "search_engine/crawler/CrawlerManager.h"

#include <thread>
#include <chrono>

using namespace search_engine::auth;
using namespace std::chrono_literals;

// =====================================================================
// Base64Url
// =====================================================================
TEST_CASE("Base64Url roundtrip", "[auth][base64]") {
    std::string original = "Hello, World! This is a test \xff\x00 with binary.";
    auto encoded = Base64Url::encode(original);
    REQUIRE(encoded.find('+') == std::string::npos);
    REQUIRE(encoded.find('/') == std::string::npos);
    REQUIRE(encoded.find('=') == std::string::npos);
    auto decoded = Base64Url::decode(encoded);
    REQUIRE(std::string(decoded.begin(), decoded.end()) == original);
}

TEST_CASE("Base64Url empty input", "[auth][base64]") {
    REQUIRE(Base64Url::encode("").empty());
    REQUIRE(Base64Url::decode("").empty());
}

TEST_CASE("Base64Url rejects bad chars", "[auth][base64]") {
    REQUIRE(Base64Url::decode("ab!!cd").empty());
    REQUIRE(Base64Url::decode("ab+cd").empty());  // '+' not valid in URL-safe alphabet
}

// =====================================================================
// PasswordHasher
// =====================================================================
TEST_CASE("PasswordHasher roundtrip", "[auth][password]") {
    auto stored = PasswordHasher::hash("hunter2-strong-pw", 1000);  // low iters for test speed
    REQUIRE_FALSE(stored.empty());
    REQUIRE(PasswordHasher::verify("hunter2-strong-pw", stored));
    REQUIRE_FALSE(PasswordHasher::verify("wrong", stored));
}

TEST_CASE("PasswordHasher salts unique hashes", "[auth][password]") {
    auto a = PasswordHasher::hash("same-password", 1000);
    auto b = PasswordHasher::hash("same-password", 1000);
    REQUIRE(a != b);                                  // different salt
    REQUIRE(PasswordHasher::verify("same-password", a));
    REQUIRE(PasswordHasher::verify("same-password", b));
}

TEST_CASE("PasswordHasher rejects malformed stored hash", "[auth][password]") {
    REQUIRE_FALSE(PasswordHasher::verify("p", "not-a-real-hash"));
    REQUIRE_FALSE(PasswordHasher::verify("p", ""));
    REQUIRE_FALSE(PasswordHasher::verify("p", "1000$$$"));
}

// =====================================================================
// JWT
// =====================================================================
TEST_CASE("JWT issue and verify roundtrip", "[auth][jwt]") {
    JwtIssuer issuer("test-secret-1234567890");
    User u;
    u.id = "u_123";
    u.username = "alice";
    u.role = Role::USER;
    auto token = issuer.issue(u, std::chrono::seconds(60));
    REQUIRE(token.find('.') != std::string::npos);

    auto claims = issuer.verify(token);
    REQUIRE(claims.has_value());
    REQUIRE(claims->sub == "u_123");
    REQUIRE(claims->username == "alice");
    REQUIRE(claims->role == Role::USER);
    REQUIRE(claims->exp > claims->iat);
}

TEST_CASE("JWT rejects token with bad signature", "[auth][jwt]") {
    JwtIssuer issuer("secret-A");
    User u; u.id = "u"; u.username = "n"; u.role = Role::ADMIN;
    auto token = issuer.issue(u, std::chrono::seconds(60));

    JwtIssuer wrong("secret-B");
    REQUIRE_FALSE(wrong.verify(token).has_value());
}

TEST_CASE("JWT rejects tampered payload", "[auth][jwt]") {
    JwtIssuer issuer("test-secret");
    User u; u.id = "alice"; u.username = "alice"; u.role = Role::USER;
    auto token = issuer.issue(u, std::chrono::seconds(60));

    // Flip a character in the middle of the payload segment.
    auto firstDot = token.find('.');
    REQUIRE(firstDot != std::string::npos);
    auto secondDot = token.find('.', firstDot + 1);
    REQUIRE(secondDot != std::string::npos);
    auto mid = (firstDot + secondDot) / 2;
    std::string tampered = token;
    tampered[mid] = (token[mid] == 'a') ? 'b' : 'a';
    REQUIRE_FALSE(issuer.verify(tampered).has_value());
}

TEST_CASE("JWT rejects expired token", "[auth][jwt]") {
    JwtIssuer issuer("s");
    User u; u.id = "u"; u.username = "n"; u.role = Role::USER;
    // 1-second expiry; wait 2 seconds.
    auto token = issuer.issue(u, std::chrono::seconds(1));
    std::this_thread::sleep_for(1100ms);
    REQUIRE_FALSE(issuer.verify(token).has_value());
}

TEST_CASE("JWT rejects malformed token", "[auth][jwt]") {
    JwtIssuer issuer("s");
    REQUIRE_FALSE(issuer.verify("").has_value());
    REQUIRE_FALSE(issuer.verify("not.a.jwt").has_value());
    REQUIRE_FALSE(issuer.verify("only-one-segment").has_value());
    REQUIRE_FALSE(issuer.verify("only.two").has_value());
}

// =====================================================================
// UserStore
// =====================================================================
TEST_CASE("InMemoryUserStore basic CRUD", "[auth][userstore]") {
    InMemoryUserStore store;
    User u;
    u.username = "bob";
    u.passwordHash = "x";
    REQUIRE(store.create(u));
    REQUIRE_FALSE(u.id.empty());
    REQUIRE(store.size() == 1);

    auto found = store.findByUsername("bob");
    REQUIRE(found.has_value());
    REQUIRE(found->id == u.id);
    REQUIRE(store.findById(u.id).has_value());
}

TEST_CASE("InMemoryUserStore rejects duplicate username", "[auth][userstore]") {
    InMemoryUserStore store;
    User u1; u1.username = "carol"; u1.passwordHash = "x";
    User u2; u2.username = "carol"; u2.passwordHash = "y";
    REQUIRE(store.create(u1));
    REQUIRE_FALSE(store.create(u2));
    REQUIRE(store.size() == 1);
}

// =====================================================================
// CrawlerManager::canAccess (session ownership policy)
// =====================================================================
TEST_CASE("canAccess: legacy session with empty owner is open", "[auth][isolation]") {
    AuthContext anon;  // empty userId, USER role
    REQUIRE(CrawlerManager::canAccess("", anon));
    AuthContext user{"u1", "u", Role::USER};
    REQUIRE(CrawlerManager::canAccess("", user));
}

TEST_CASE("canAccess: owner sees their own session", "[auth][isolation]") {
    AuthContext alice{"u_alice", "alice", Role::USER};
    REQUIRE(CrawlerManager::canAccess("u_alice", alice));
}

TEST_CASE("canAccess: stranger blocked from another user's session", "[auth][isolation]") {
    AuthContext bob{"u_bob", "bob", Role::USER};
    REQUIRE_FALSE(CrawlerManager::canAccess("u_alice", bob));
}

TEST_CASE("canAccess: admin sees everyone's session", "[auth][isolation]") {
    AuthContext admin{"u_admin", "root", Role::ADMIN};
    REQUIRE(CrawlerManager::canAccess("u_alice", admin));
    REQUIRE(CrawlerManager::canAccess("u_bob", admin));
    REQUIRE(CrawlerManager::canAccess("", admin));
}
