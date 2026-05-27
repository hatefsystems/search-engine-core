# 🚀 Profile Database Models - MVP

**Duration:** 1 day
**Dependencies:** MongoDB instance setup
**Acceptance Criteria:**
- ✅ Basic Profile struct with minimal fields (id, slug, name, type, bio, isPublic, createdAt)
- ✅ Simple MongoDB collection setup
- ✅ Basic validation (slug uniqueness, required fields)
- ❌ NO encryption, NO IP tracking, NO three-tier architecture
- ✅ Simple unit tests
- ✅ Can create and retrieve profiles

## 🎯 Task Description

Create a basic profile database model for MVP. This is a simplified version that focuses on getting core profile functionality working without complex privacy architecture, encryption, or IP tracking. Perfect for quick prototyping and getting the system working end-to-end.

## 📋 What We Build (Minimal)

### Basic Profile Structure
```cpp
struct Profile {
    std::string id;           // Unique identifier
    std::string slug;         // URL-friendly name (e.g., "john-doe")
    std::string name;         // Display name
    std::string type;         // "PERSON" or "BUSINESS"
    std::string bio;          // Optional short description
    bool isPublic = true;     // Public by default for MVP
    Date createdAt;           // Creation timestamp
};
```

### MongoDB Collection Setup
- Collection name: `profiles`
- Basic indexes on `slug` and `createdAt`
- Simple schema validation

### Basic Validation
- `slug` must be unique
- `slug` must match pattern: lowercase, alphanumeric, hyphens only
- `type` must be "PERSON" or "BUSINESS"
- `name` and `slug` are required
- `bio` optional, max 500 characters

## 📋 Daily Breakdown

### Day 1: Basic Profile Model + MongoDB Setup
- Create basic Profile struct with essential fields
- Set up MongoDB collection with basic schema
- Implement basic validation rules
- Create simple unit tests
- Test profile creation and retrieval

## 🧪 Testing Strategy

### Unit Tests
```cpp
TEST(ProfileModelTest, CreateBasicProfile) {
    Profile profile{
        .id = generateId(),
        .slug = "test-user",
        .name = "Test User",
        .type = "PERSON",
        .bio = "Test bio",
        .isPublic = true
    };
    EXPECT_TRUE(profile.isValid());
    EXPECT_EQ(profile.slug, "test-user");
}

TEST(ProfileModelTest, SlugValidation) {
    // Test valid slugs
    EXPECT_TRUE(isValidSlug("john-doe"));
    EXPECT_TRUE(isValidSlug("test123"));
    EXPECT_TRUE(isValidSlug("user-name"));

    // Test invalid slugs
    EXPECT_FALSE(isValidSlug("John Doe"));  // spaces
    EXPECT_FALSE(isValidSlug("john@doe"));  // special chars
    EXPECT_FALSE(isValidSlug(""));         // empty
}
```

### Integration Tests
- Test MongoDB document insertion
- Test profile retrieval by slug
- Test basic CRUD operations
- Verify schema constraints

## 🎉 Success Criteria

### Functionality
- ✅ Profile struct compiles without errors
- ✅ MongoDB documents can be created and retrieved
- ✅ Basic validation prevents invalid data
- ✅ Slug uniqueness enforced
- ✅ Simple unit tests pass (70%+ coverage)

### MVP Readiness
- ✅ Can create profiles via code
- ✅ Can retrieve profiles by slug
- ✅ Basic error handling works
- ✅ Ready for API integration

### Exclusions (By Design)
- ❌ **NO encryption** - sensitive data stored as plain text for MVP
- ❌ **NO IP tracking** - no geo data or visitor analytics
- ❌ **NO three-tier architecture** - single simple database
- ❌ **NO complex privacy controls** - all profiles public by default
- ❌ **NO audit logging** - basic logging only

## 🔄 Next Steps

After this MVP task, continue with:
- **01b-database-personal-business.md** - Extend with Person/Business specific fields
- **01c-privacy-architecture.md** - Add encryption and privacy controls
- **01d-database-indexes-validation.md** - Add performance indexes and advanced validation

This MVP focuses on **speed over security** to get the core system working quickly.
