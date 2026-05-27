---
description: Environment variables via std::getenv and Docker Compose
---

# Environment Variable Configuration

Use `std::getenv()` for configuration (works with Docker Compose and .env).

```cpp
const char* host = std::getenv("SMTP_HOST");
config.smtpHost = host ? host : "smtp.gmail.com";
// Boolean: compare to "true", "1", "yes" (case-insensitive)
// Integer: std::stoi with try/catch and default
```

- Do not create custom .env parser classes.
- Containers: `core`, `mongodb_test`, `redis`, `browserless`.
- Critical: `LOG_LEVEL`, `MONGODB_URI`, `REDIS_URL`, `BROWSERLESS_URL`.
- Never commit `.env` with credentials; use Docker secrets in production.

## Configuration Priority

1. Docker environment (highest)
2. `.env` file (Docker Compose fallback)
3. Default values in code

## Docker Compose Pattern

```yaml
environment:
  - SMTP_HOST=${SMTP_HOST:-smtp.gmail.com}
  - SMTP_PORT=${SMTP_PORT:-587}
  - SMTP_USE_TLS=${SMTP_USE_TLS:-true}
```

## Boolean/Integer Parsing

```cpp
// Boolean: compare to "true", "1", "yes" (case-insensitive)
// Integer: std::stoi with try/catch and default
const char* port = std::getenv("SMTP_PORT");
try {
    config.smtpPort = port ? std::stoi(port) : 587;
} catch (const std::exception& e) {
    LOG_WARNING("Invalid SMTP_PORT, using default: 587");
    config.smtpPort = 587;
}
```
