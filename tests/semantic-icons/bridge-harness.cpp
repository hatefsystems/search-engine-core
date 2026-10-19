// Test the production bridge without MongoDB or profile fixture dependencies.
#include "../../src/controllers/IconEngineController.h"
#include <cstdlib>
int main() {
    uWS::App app;
    routing::RouteRegistry::getInstance().applyRoutes(app);
    app.get("/assets/icons/*", [](auto* res, auto* req) { IconEngineController::asset(res,req); });
    bool bound=false;
    app.listen(std::atoi(std::getenv("ICON_BRIDGE_PORT")),[&](auto* socket){bound=socket!=nullptr;});
    if (!bound) return 1;
    app.run();
}
