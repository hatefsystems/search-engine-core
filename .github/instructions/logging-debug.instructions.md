---
description: LOG_DEBUG and LOG_LEVEL instead of std::cout
applyTo: '**'
---

# Logging and Debug Output

- Use `LOG_DEBUG()` instead of `std::cout` for debug messages.
- Configure level via `LOG_LEVEL` (trace, debug, info, warning, error, none).
- Use `LOG_INFO()`, `LOG_ERROR()`, `LOG_WARNING()` for other levels.

```cpp
// ❌ WRONG
std::cout << "[DEBUG] Processing " << requestId << std::endl;

// ✅ CORRECT
LOG_DEBUG("Processing request: " + std::to_string(requestId));
```

Test with `LOG_LEVEL=debug` and `LOG_LEVEL=info` to ensure debug is configurable.

## LOG_LEVEL Values

| Level | Use Case | What Gets Logged |
|-------|----------|------------------|
| `trace` | Deep debugging | Everything including execution flow |
| `debug` | Development | WebSocket, crawler, API calls, performance |
| `info` | Production | Standard operations, system status |
| `warning` | High-performance | Non-critical issues, performance warnings |
| `error` | Critical monitoring | System failures, database errors |
| `none` | Performance testing | No logging output |

## Usage

```bash
LOG_LEVEL=debug docker-compose up

LOG_LEVEL=info ./server
```
