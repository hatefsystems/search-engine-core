#pragma once
#include "../../include/routing/Controller.h"
#include "../../include/search_engine/auth/UserStore.h"
#include "../../include/search_engine/auth/Jwt.h"
#include <memory>
#include <mutex>

/**
 * @brief Authentication endpoints (issue #13).
 *
 * POST /api/auth/register  { username, password, role? }
 * POST /api/auth/login     { username, password }   -> { token }
 * GET  /api/auth/me        Authorization: Bearer <token>
 *
 * Also exposes a static helper `extractAuth()` used by other controllers
 * (e.g. SearchController) to read the Bearer token off a request and
 * resolve it into an AuthContext.
 */
class AuthController : public routing::Controller {
public:
    AuthController();

    void registerUser(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void login(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);
    void me(uWS::HttpResponse<false>* res, uWS::HttpRequest* req);

    // Resolve a request's Bearer token (if any) to an AuthContext. Returns
    // an empty optional if missing or invalid.
    static std::optional<search_engine::auth::AuthContext> extractAuth(uWS::HttpRequest* req);

    // Accessors for the singletons (used by SearchController etc.)
    static search_engine::auth::IUserStore* userStore();
    static search_engine::auth::JwtIssuer* jwtIssuer();
};

ROUTE_CONTROLLER(AuthController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::POST, "/api/auth/register", registerUser, AuthController);
    REGISTER_ROUTE(HttpMethod::POST, "/api/auth/login", login, AuthController);
    REGISTER_ROUTE(HttpMethod::GET,  "/api/auth/me", me, AuthController);
}
