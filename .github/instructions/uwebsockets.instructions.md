---
description: uWebSockets onData + onAborted pattern for POST endpoints
applyTo: '**/*.cpp,src/controllers/**/*.cpp'
---

# uWebSockets Rules

## MANDATORY: onData + onAborted

Never use `res->onData()` without `res->onAborted()` — the server can crash on client disconnect.

```cpp
// ✅ CORRECT
res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
    buffer.append(data.data(), data.length());
    if (last) { /* process */ }
});
res->onAborted([]() { LOG_WARNING("Request aborted by client"); });
```

- Always add `res->onAborted()` after `res->onData()`
- Use `std::move(buffer)` in lambda capture
- Check `if (last)` before processing
- Never access `res` or controller members after client disconnect

## Full POST Template

```cpp
void Controller::safePostEndpoint(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    std::string buffer;
    res->onData([this, res, buffer = std::move(buffer)](std::string_view data, bool last) mutable {
        buffer.append(data.data(), data.length());
        if (last) {
            try {
                auto jsonBody = nlohmann::json::parse(buffer);
                // ... business logic ...
                this->json(res, response);
            } catch (const std::exception& e) {
                LOG_ERROR("Error in endpoint: " + std::string(e.what()));
                serverError(res, "Processing error");
            }
        }
    });
    res->onAborted([]() { LOG_WARNING("Client disconnected during request processing"); });
}
```

Common crash: "Empty reply from server" or segfault in POST → add `onAborted()`.
