# 🚀 Profile Database Models

**Duration:** 3 days
**Dependencies:** MongoDB instance setup
**Acceptance Criteria:**
- ✅ Profile base model with common fields
- ✅ Person profile model extending base
- ✅ Business profile model extending base
- ✅ Profile validation rules implemented
- ✅ Database indexes created for performance
- ✅ Unit tests for all models (85%+ coverage)

## 🎯 Task Description

Create the core database models for the profile system. This task establishes the foundation for all profile types (personal and business) with proper MongoDB schema design and validation.

## 📋 Daily Breakdown

### Day 1: Base Profile Model
- Design base profile schema with common fields
- Implement username/slug validation
- Create profile types enum (PERSON, BUSINESS)
- Add basic privacy controls (public/private)
- Set up MongoDB collection with proper naming

### Day 2: Person Profile Extension
- Extend base model for individual profiles
- Add personal info fields (name, bio, skills)
- Implement resume/work experience structure
- Create projects and recommendations arrays
- Add validation for personal profile specific rules

### Day 3: Business Profile Extension
- Extend base model for company/brand profiles
- Add business info fields (company name, category, location)
- Implement products/services structure
- Create reviews and jobs arrays
- Add validation for business profile specific rules
- Create database indexes for query performance

## 🧪 Testing Strategy

### Unit Tests
```cpp
// Test profile creation
TEST(ProfileModelTest, CreateBaseProfile) {
    Profile profile{
        .id = generateId(),
        .slug = "test-profile",
        .type = ProfileType::PERSON,
        .isPublic = true
    };
    EXPECT_TRUE(profile.isValid());
}
```

### Integration Tests
- Test MongoDB document insertion/extraction
- Validate schema constraints
- Test index performance
- Verify data consistency across profile types

## 🎉 Success Criteria
- All models compile without errors
- MongoDB documents can be created and retrieved
- Validation rules prevent invalid data
- Indexes improve query performance by 10x+
- Unit test coverage >85%
