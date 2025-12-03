# 🚀 Profile Database Models - Person & Business Extensions

**Duration:** 1 day
**Dependencies:** 01a-database-mvp.md (basic profile model)
**Acceptance Criteria:**
- ✅ Person profile model extending base (skills, experience, education fields)
- ✅ Business profile model extending base (company info, category, location fields)
- ✅ Profile type enum and validation
- ✅ Basic inheritance/composition pattern
- ✅ Both profile types work end-to-end
- ✅ Database schema supports both types
- ✅ Unit tests for both profile types

## 🎯 Task Description

Extend the basic profile model to support both personal and business profiles with their specific fields. This builds on the MVP foundation to add the essential fields needed for different profile types while maintaining a clean, extensible architecture.

## 👤 Person Profile Extensions

### Additional Fields for Personal Profiles
```cpp
struct PersonProfile : public Profile {
    // Professional Info
    std::string title;              // Job title (e.g., "Software Engineer")
    std::string company;            // Current company
    std::vector<std::string> skills; // Skills list
    std::string experienceLevel;    // "Entry", "Mid", "Senior", "Executive"

    // Education
    std::string education;          // Degree/field of study
    std::string school;             // University/school name

    // Social Links
    std::string linkedinUrl;        // LinkedIn profile
    std::string githubUrl;          // GitHub profile
    std::string portfolioUrl;       // Personal website/portfolio

    // Contact (will be encrypted in later tasks)
    std::string email;              // Contact email
    std::string phone;              // Phone number
};
```

### Person Profile Use Cases
- Job seekers showcasing skills and experience
- Professionals building personal brand
- Consultants and freelancers
- Students and recent graduates

## 🏢 Business Profile Extensions

### Additional Fields for Business Profiles
```cpp
struct BusinessProfile : public Profile {
    // Company Info
    std::string companyName;        // Official company name
    std::string industry;           // Industry category
    std::string companySize;        // "1-10", "11-50", "51-200", "201-1000", "1000+"
    int foundedYear;                // Year founded

    // Location
    std::string address;            // Business address
    std::string city;               // City
    std::string country;            // Country

    // Business Details
    std::string website;            // Company website
    std::string description;        // Company description
    std::vector<std::string> services; // Services offered

    // Contact (will be encrypted in later tasks)
    std::string businessEmail;      // Business email
    std::string businessPhone;      // Business phone
};
```

### Business Profile Use Cases
- Local businesses attracting customers
- Startups seeking talent/partners
- Consulting firms and agencies
- Service providers (restaurants, shops, etc.)

## 🔧 Profile Type System

### Type Enum
```cpp
enum class ProfileType {
    PERSON,
    BUSINESS
};

// String conversion for API/database
std::string profileTypeToString(ProfileType type) {
    switch (type) {
        case ProfileType::PERSON: return "PERSON";
        case ProfileType::BUSINESS: return "BUSINESS";
        default: return "UNKNOWN";
    }
}

ProfileType stringToProfileType(const std::string& str) {
    if (str == "PERSON") return ProfileType::PERSON;
    if (str == "BUSINESS") return ProfileType::BUSINESS;
    throw std::invalid_argument("Invalid profile type: " + str);
}
```

### Database Schema Design
```cpp
// MongoDB document structure
{
    "_id": ObjectId("..."),
    "slug": "john-doe",
    "name": "John Doe",
    "type": "PERSON",           // or "BUSINESS"
    "bio": "Software Engineer...",
    "isPublic": true,
    "createdAt": ISODate("..."),

    // Person-specific fields (only if type == "PERSON")
    "title": "Software Engineer",
    "company": "Tech Corp",
    "skills": ["C++", "Python", "MongoDB"],
    "experienceLevel": "Senior",
    "education": "Computer Science",
    "school": "MIT",
    "linkedinUrl": "https://linkedin.com/in/johndoe",
    "githubUrl": "https://github.com/johndoe",
    "email": "john@example.com",
    "phone": "+1234567890",

    // Business-specific fields (only if type == "BUSINESS")
    "companyName": "Tech Corp",
    "industry": "Technology",
    "companySize": "51-200",
    "foundedYear": 2015,
    "address": "123 Main St",
    "city": "San Francisco",
    "country": "USA",
    "website": "https://techcorp.com",
    "description": "Leading tech company...",
    "services": ["Software Development", "Consulting"],
    "businessEmail": "contact@techcorp.com",
    "businessPhone": "+1987654321"
}
```

## 📋 Implementation Plan

### Day 1: Person Profile Model
- Create PersonProfile struct extending base Profile
- Add person-specific fields and validation
- Implement person profile creation and serialization
- Add unit tests for person profiles
- Test person profile CRUD operations

### Day 1 Continued: Business Profile Model
- Create BusinessProfile struct extending base Profile
- Add business-specific fields and validation
- Implement business profile creation and serialization
- Add unit tests for business profiles
- Test business profile CRUD operations

## 🧪 Testing Strategy

### Unit Tests
```cpp
TEST(PersonProfileTest, CreateValidPersonProfile) {
    PersonProfile profile;
    profile.slug = "john-doe";
    profile.name = "John Doe";
    profile.title = "Software Engineer";
    profile.company = "Tech Corp";
    profile.skills = {"C++", "Python"};
    profile.email = "john@example.com";

    EXPECT_TRUE(profile.isValid());
    EXPECT_EQ(profile.type, ProfileType::PERSON);
    EXPECT_EQ(profile.skills.size(), 2);
}

TEST(BusinessProfileTest, CreateValidBusinessProfile) {
    BusinessProfile profile;
    profile.slug = "tech-corp";
    profile.name = "Tech Corp";
    profile.companyName = "Tech Corporation Inc.";
    profile.industry = "Technology";
    profile.companySize = "51-200";
    profile.businessEmail = "contact@techcorp.com";

    EXPECT_TRUE(profile.isValid());
    EXPECT_EQ(profile.type, ProfileType::BUSINESS);
    EXPECT_EQ(profile.industry, "Technology");
}

TEST(ProfileTypeTest, TypeConversion) {
    EXPECT_EQ(profileTypeToString(ProfileType::PERSON), "PERSON");
    EXPECT_EQ(profileTypeToString(ProfileType::BUSINESS), "BUSINESS");

    EXPECT_EQ(stringToProfileType("PERSON"), ProfileType::PERSON);
    EXPECT_EQ(stringToProfileType("BUSINESS"), ProfileType::BUSINESS);
}
```

### Integration Tests
- Test person profile creation, update, retrieval
- Test business profile creation, update, retrieval
- Test profile type validation and conversion
- Test MongoDB document structure for both types
- Verify field validation and constraints

## 🎉 Success Criteria

### Functionality
- ✅ PersonProfile and BusinessProfile structs compile
- ✅ Both profile types can be created and validated
- ✅ Profile type enum works correctly
- ✅ MongoDB documents store both profile types properly
- ✅ Inheritance/composition pattern works
- ✅ Unit tests pass for both profile types (80%+ coverage)

### Data Integrity
- ✅ Person-specific fields only appear on person profiles
- ✅ Business-specific fields only appear on business profiles
- ✅ No field conflicts between profile types
- ✅ Type validation prevents invalid combinations

### Extensibility
- ✅ Easy to add new fields to either profile type
- ✅ Clean separation of concerns
- ✅ Ready for API layer integration
- ✅ Foundation for profile-specific features

## 🔗 Dependencies & Next Steps

### Depends On
- **01a-database-mvp.md** - Basic profile model and MongoDB setup

### Enables
- **01c-privacy-architecture.md** - Can add encryption to sensitive fields (email, phone)
- **01d-database-indexes-validation.md** - Can add type-specific indexes
- **Personal profile tasks (06-09)** - Person profiles ready for UI/features
- **Business profile tasks (10-13)** - Business profiles ready for UI/features

## 💡 Design Decisions

### Inheritance vs Composition
**Decision:** Used inheritance (PersonProfile : Profile) for simplicity and clarity
- ✅ Clear type hierarchy
- ✅ Easy to understand relationships
- ✅ Type-safe field access
- ✅ Polymorphic behavior possible

### Single Collection vs Multiple Collections
**Decision:** Single `profiles` collection with discriminated fields
- ✅ Simpler queries and indexing
- ✅ Easier to maintain single schema
- ✅ Better for unified profile search
- ✅ Follows MongoDB best practices

### Optional Fields Strategy
**Decision:** All extended fields are optional
- ✅ Backward compatibility
- ✅ Progressive profile completion
- ✅ Users can start with minimal info
- ✅ Easy migration and updates
