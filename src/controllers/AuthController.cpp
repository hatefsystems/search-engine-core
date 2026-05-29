#include "AuthController.h"
#include "../../include/Logger.h"
#include "../../include/search_engine/auth/PasswordHasher.h"
#include <nlohmann/json.hpp>
#include <mutex>

using namespace search_engine::auth;

namespace {
    std::once_flag g_authInitFlag;
    std::unique_ptr<IUserStore> g_userStore;
    std::unique_ptr<JwtIssuer> g_jwtIssuer;
    bool g_devSecretWarned = false;

    void ensureAuthInitialized() {
        std::call_once(g_authInitFlag, []() {
            g_userStore = std::make_unique<InMemoryUserStore>();
            auto secret = JwtIssuer::secretFromEnvOrDev();
            if (std::getenv("JWT_SECRET") == nullptr) {
                LOG_WARNING("JWT_SECRET env var not set — using built-in dev secret. "
                            "DO NOT run in production without setting JWT_SECRET.");
                g_devSecretWarned = true;
            }
            g_jwtIssuer = std::make_unique<JwtIssuer>(secret);
            LOG_INFO("Auth subsystem initialized");
        });
    }
}

AuthController::AuthController() { ensureAuthInitialized(); }

IUserStore* AuthController::userStore() {
    ensureAuthInitialized();
    return g_userStore.get();
}

JwtIssuer* AuthController::jwtIssuer() {
    ensureAuthInitialized();
    return g_jwtIssuer.get();
}

std::optional<AuthContext> AuthController::extractAuth(uWS::HttpRequest* req) {
    ensureAuthInitialized();
    if (!req) return std::nullopt;
    auto authHeader = req->getHeader("authorization");
    if (authHeader.empty()) return std::nullopt;
    const std::string prefix = "Bearer ";
    if (authHeader.size() <= prefix.size() ||
        std::string(authHeader.substr(0, prefix.size())) != prefix) {
        return std::nullopt;
    }
    std::string token(authHeader.substr(prefix.size()));
    auto claims = g_jwtIssuer->verify(token);
    if (!claims.has_value()) return std::nullopt;

    AuthContext ctx;
    ctx.userId = claims->sub;
    ctx.username = claims->username;
    ctx.role = claims->role;
    return ctx;
}

namespace {
    // Read JSON body asynchronously, then run `cb`. Bounds enforced to avoid
    // unbounded buffering.
    template <typename Cb>
    void readJsonBody(uWS::HttpResponse<false>* res, Cb cb) {
        auto buffer = std::make_shared<std::string>();
        res->onData([res, buffer, cb = std::move(cb)](std::string_view chunk, bool last) mutable {
            buffer->append(chunk.data(), chunk.size());
            if (buffer->size() > 64 * 1024) {
                res->writeStatus("413 Payload Too Large")->end("Body too large");
                return;
            }
            if (!last) return;
            try {
                auto j = nlohmann::json::parse(*buffer);
                cb(j);
            } catch (const std::exception& e) {
                res->writeStatus("400 Bad Request")
                   ->writeHeader("Content-Type", "application/json")
                   ->end(std::string("{\"error\":\"Invalid JSON: ") + e.what() + "\"}");
            }
        });
        res->onAborted([]() {});
    }
}

void AuthController::registerUser(uWS::HttpResponse<false>* res, uWS::HttpRequest* /*req*/) {
    ensureAuthInitialized();
    readJsonBody(res, [res](const nlohmann::json& body) {
        std::string username = body.value("username", "");
        std::string password = body.value("password", "");
        std::string roleStr = body.value("role", "user");
        if (username.empty() || password.size() < 8) {
            badRequest(res, "username required and password must be >= 8 chars");
            return;
        }
        if (g_userStore->findByUsername(username).has_value()) {
            res->writeStatus("409 Conflict")
               ->writeHeader("Content-Type", "application/json")
               ->end("{\"error\":\"username already exists\"}");
            return;
        }
        User u;
        u.username = username;
        u.passwordHash = PasswordHasher::hash(password);
        u.role = roleFromString(roleStr);
        if (!g_userStore->create(u)) {
            serverError(res, "Failed to create user");
            return;
        }
        nlohmann::json response = {
            {"id", u.id},
            {"username", u.username},
            {"role", roleToString(u.role)}
        };
        json(res, response, "201 Created");
    });
}

void AuthController::login(uWS::HttpResponse<false>* res, uWS::HttpRequest* /*req*/) {
    ensureAuthInitialized();
    readJsonBody(res, [res](const nlohmann::json& body) {
        std::string username = body.value("username", "");
        std::string password = body.value("password", "");
        if (username.empty() || password.empty()) {
            badRequest(res, "username and password required");
            return;
        }
        auto u = g_userStore->findByUsername(username);
        if (!u.has_value() || !PasswordHasher::verify(password, u->passwordHash)) {
            // Same response for unknown user and wrong password to avoid
            // user-enumeration via timing/error difference.
            res->writeStatus("401 Unauthorized")
               ->writeHeader("Content-Type", "application/json")
               ->end("{\"error\":\"invalid credentials\"}");
            return;
        }
        auto token = g_jwtIssuer->issue(*u);
        nlohmann::json response = {
            {"token", token},
            {"tokenType", "Bearer"},
            {"user", {
                {"id", u->id},
                {"username", u->username},
                {"role", roleToString(u->role)}
            }}
        };
        json(res, response);
    });
}

void AuthController::me(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    ensureAuthInitialized();
    auto ctx = extractAuth(req);
    if (!ctx.has_value()) {
        res->writeStatus("401 Unauthorized")
           ->writeHeader("Content-Type", "application/json")
           ->end("{\"error\":\"missing or invalid token\"}");
        return;
    }
    nlohmann::json response = {
        {"id", ctx->userId},
        {"username", ctx->username},
        {"role", roleToString(ctx->role)}
    };
    json(res, response);
}
