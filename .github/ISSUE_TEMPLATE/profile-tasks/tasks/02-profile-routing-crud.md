# 🚀 Profile Routing & CRUD API

**Duration:** 4 days
**Dependencies:** Profile database models
**Acceptance Criteria:**
- ✅ ProfileController created with lazy initialization
- ✅ RESTful routes for profile CRUD operations
- ✅ URL slug routing (hatef.ir/username)
- ✅ Basic authentication middleware
- ✅ Error handling with proper HTTP status codes
- ✅ API documentation and examples
- ✅ Integration tests for all endpoints

## 🎯 Task Description

Implement the basic CRUD API endpoints for profile management. This includes creating, reading, updating, and deleting profiles with proper routing and error handling.

## 📋 Daily Breakdown

### Day 1: Controller Setup
- Create ProfileController class with lazy initialization
- Set up controller registration in RouteRegistry
- Implement basic constructor with service dependencies
- Add controller to CMakeLists.txt

### Day 2: Create & Read Operations
- Implement POST /api/profiles (create profile)
- Implement GET /api/profiles/:id (get profile)
- Implement GET /profiles/:slug (public profile view)
- Add input validation and sanitization
- Handle profile not found errors

### Day 3: Update & Delete Operations
- Implement PUT /api/profiles/:id (update profile)
- Implement DELETE /api/profiles/:id (delete profile)
- Add ownership validation (users can only edit their profiles)
- Implement soft delete with recovery option
- Add update timestamp tracking

### Day 4: Error Handling & Testing
- Implement comprehensive error responses
- Add rate limiting for API endpoints
- Create integration tests for all CRUD operations
- Test edge cases (invalid slugs, duplicate usernames)
- Document API endpoints with examples

## 🔧 API Endpoints

```cpp
// Profile CRUD endpoints
ROUTE_CONTROLLER(ProfileController) {
    using namespace routing;
    REGISTER_ROUTE(HttpMethod::POST, "/api/profiles", createProfile, ProfileController);
    REGISTER_ROUTE(HttpMethod::GET, "/api/profiles/:id", getProfile, ProfileController);
    REGISTER_ROUTE(HttpMethod::PUT, "/api/profiles/:id", updateProfile, ProfileController);
    REGISTER_ROUTE(HttpMethod::DELETE, "/api/profiles/:id", deleteProfile, ProfileController);

    // Public profile viewing
    REGISTER_ROUTE(HttpMethod::GET, "/profiles/:slug", getPublicProfile, ProfileController);
}
```

## 🧪 Testing Strategy

### API Tests
```bash
# Test profile creation
curl -X POST http://localhost:3000/api/profiles \
  -H "Content-Type: application/json" \
  -d '{"slug":"test-user","type":"PERSON","name":"Test User"}'

# Test public profile access
curl http://localhost:3000/profiles/test-user
```

### Integration Tests
- Test full CRUD cycle for both profile types
- Verify proper error responses for invalid requests
- Test concurrent profile creation (race conditions)
- Validate URL slug uniqueness constraints

## 🎉 Success Criteria
- All CRUD operations work via HTTP API
- Profile URLs resolve correctly (hatef.ir/username)
- Proper error handling for edge cases
- API responds within 100ms for simple operations
- All integration tests pass
