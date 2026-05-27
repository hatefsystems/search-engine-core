---
description: Error handling, file organization, security, performance
applyTo: '**/*.cpp,**/*.h,src/**/*,include/**/*'
---

# Code Style and Practices

## Error Handling

```cpp
try {
    mongocxx::instance& instance = MongoDBInstance::getInstance();
    // ...
} catch (const mongocxx::exception& e) {
    LOG_ERROR("MongoDB error: " + std::string(e.what()));
    serverError(res, "Database error occurred");
}
```

## Includes Order

```cpp
// System headers first
#include <string>
#include <memory>
// Project headers
#include "../../include/Logger.h"
#include "../../include/mongodb.h"
// Third-party headers
#include <mongocxx/client.hpp>
#include <nlohmann/json.hpp>
```

## Security

Validate input; use prepared statements; rate limit APIs; sanitize URLs; validate email/mobile formats.

## Performance

Connection pooling (MongoDB); Redis caching; minimize DB round trips; indexes on hot fields.

## Memory and Threads

Smart pointers; RAII; mutexes for shared state; lock_guard; minimize lock scope.

## Testing

- MongoDB: `docker exec mongodb_test mongosh --username admin --password password123 --eval "db.adminCommand('ping')"`
- API: `curl --location 'http://localhost:3000/api/endpoint' --header 'Content-Type: application/json' --data-raw '{ ... }'`
- Verify data: `docker exec mongodb_test mongosh --username admin --password password123 --eval "use('search-engine'); db.collection.find().pretty()"`
- Logs: `docker logs core --tail 50`, `docker logs mongodb_test`
