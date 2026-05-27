---
description: Result<T>, BSON, and MongoDB C++ API patterns (zero-error)
applyTo: '**/*.cpp,**/*.h,src/**/*.cpp,include/**/*.h'
---

# C++ API Patterns - Zero-Error Rules

Never guess API usage. Use these patterns to prevent compilation errors.

## Result<T> Interface

```cpp
// ❌ WRONG - These methods DON'T EXIST
if (result.isSuccess()) { }
result.getError(); result.getValue();
Result<T>::success(value); Result<T>::failure(message);

// ✅ CORRECT
if (result.success) { }
result.message
result.value
Result<T>::Success(value, "msg")
Result<T>::Failure("error")
```

## MongoDB BSON String Access

```cpp
// ❌ WRONG
element.get_utf8().value.to_string()
element.key().to_string()

// ✅ CORRECT
std::string(element.get_string().value)
std::string(element.key())
```

## MongoDB Aggregation

```cpp
// ❌ WRONG
auto pipeline = document{} << "pipeline" << array{...} << finalize;
collection.aggregate(pipeline.view())

// ✅ CORRECT
mongocxx::pipeline pipeline;
pipeline.match(document{} << "field" << "value" << finalize);
pipeline.group(document{} << "_id" << "$field" << finalize);
collection.aggregate(pipeline)
```

## Optional / Result Checks

```cpp
// ❌ WRONG
if (result) { }  // run_command
if (!result) { } // find_one

// ✅ CORRECT
if (!result.empty()) { }      // run_command
if (!result.has_value()) { } // find_one (std::optional)
```

## Quick Reference

```cpp
if (result.success) { auto v = result.value; }
std::string str = std::string(element.get_string().value);
mongocxx::pipeline pipe; pipe.match(filter).group(grouping);
if (findResult.has_value()) { }
```

## Code Template Checklist (Before Implementing)

- [ ] Use `Result<T>::Success()` and `Result<T>::Failure()` (capital letters)
- [ ] Include MongoDB singleton: `MongoDBInstance::getInstance()`
- [ ] Use lazy initialization for controller services
- [ ] Pair `res->onData()` with `res->onAborted()` for POST endpoints
- [ ] Use `LOG_DEBUG()` instead of `std::cout`
- [ ] Check BSON strings with `std::string(element.get_string().value)`
- [ ] Use `mongocxx::pipeline` for aggregations
- [ ] Use basic builder with `.extract()` for complex MongoDB updates

## Zero-Error Strategy

1. Copy working patterns — use templates above
2. Build frequently after every major function/class
3. Fix immediately — don't accumulate errors
4. Use static analysis — let IDE catch issues before building
5. Follow established project patterns
