# Isolated production bridge check

This harness runs the production `IconEngineController` against the real Python service without MongoDB. Build using the repository-pinned uWebSockets/uSockets dependencies from `docker/dependencies.lock.json`, nlohmann/json headers and libcurl. With those installed in `/usr/local`:

```sh
mkdir -p build/icons
g++ -std=c++20 -O1 -I/usr/local/include \
  tests/semantic-icons/bridge-harness.cpp \
  src/controllers/IconEngineController.cpp src/routing/RouteRegistry.cpp \
  src/common/Logger.cpp -L/usr/local/lib -lusockets -lcurl -lssl -lcrypto -lz -pthread \
  -o build/icons/bridge-harness
.venv-icons/bin/python tests/semantic-icons/check-bridge.py build/icons/bridge-harness
```

For a no-SSL uSockets build, use `-DLIBUS_NO_SSL -DUWS_NO_ZLIB`, the actual static uSockets archive and the corresponding headers. The harness binds only test ports selected by its runner, starts a synthetic catalog, checks actual forwarded requests and terminates both child processes. It does not contact production systems.
