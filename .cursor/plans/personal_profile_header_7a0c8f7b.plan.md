---
name: Personal Profile Header
overview: Implement a comprehensive profile header system with avatar/cover images, personal info fields, skills management, and professional summary sections. This includes extending the Profile data model, adding image upload capabilities, updating templates, and creating new API endpoints.
todos:
  - id: extend-data-model
    content: Extend Profile.h with header fields (displayName, tagline, avatarUrl, coverImageUrl, professionalSummary, location, availabilityStatus, skillsWithLevel, languages)
    status: pending
  - id: update-storage-bson
    content: Update ProfileStorage.cpp BSON conversion methods to serialize/deserialize new header fields using basic builder pattern
    status: pending
  - id: add-validations
    content: Extend ProfileValidator with header field validations (length limits, URL formats, enum values)
    status: pending
  - id: implement-base64-utils
    content: Create Base64 encoder/decoder and ImageValidator utility classes for image upload handling
    status: pending
  - id: add-upload-endpoints
    content: Add avatar and cover image upload endpoints to ProfileController (POST /api/profiles/:id/avatar and /api/profiles/:id/cover)
    status: pending
  - id: update-static-file-serving
    content: Modify StaticFileController to serve uploaded images from uploads/ directory
    status: pending
  - id: create-skills-system
    content: "Implement skills management: SkillsData.h with predefined skills, SkillNormalizer, and autocomplete endpoint"
    status: pending
  - id: add-skills-endpoints
    content: Add skills CRUD endpoints (POST/DELETE /api/profiles/:id/skills, GET /api/skills/autocomplete)
    status: pending
  - id: create-header-template
    content: Create profile_header.inja template component with responsive design for avatar, cover, info, skills, and summary sections
    status: pending
  - id: add-header-styles
    content: Create profile-header.css with mobile-first responsive design, CSS variables, and proper sizing for all header elements
    status: pending
  - id: update-profile-templates
    content: Update profile_person.inja to include new header component and remove old header markup
    status: pending
  - id: add-completeness-endpoint
    content: Implement header completeness scoring endpoint (GET /api/profiles/:id/completeness)
    status: pending
  - id: add-privacy-controls
    content: Implement field-level privacy settings and filtering for header data based on viewer permissions
    status: pending
  - id: update-routes
    content: Register all new routes in main.cpp (upload, skills, completeness endpoints)
    status: pending
  - id: add-mongodb-indexes
    content: Add MongoDB indexes for skills array to enable efficient searching and filtering
    status: pending
  - id: write-documentation
    content: Update API documentation with new endpoints, request/response schemas, and cURL examples
    status: pending
  - id: create-integration-tests
    content: Write integration tests for image upload, header updates, skills management, and completeness scoring
    status: pending
  - id: database-migration
    content: Create and run database migration script to add default values for new header fields on existing profiles
    status: pending
isProject: false
---

# Personal Profile Header Implementation

## Current State Analysis

### Existing Infrastructure ✅

- **Profile System:** Complete CRUD operations with `[src/controllers/ProfileController.cpp](src/controllers/ProfileController.cpp)`
- **Data Models:** `PersonProfile` and `BusinessProfile` in `[include/search_engine/storage/Profile.h](include/search_engine/storage/Profile.h)`
- **Storage Layer:** `[src/storage/ProfileStorage.cpp](src/storage/ProfileStorage.cpp)` with MongoDB integration
- **Templates:** Inja templating with `[templates/profile_person.inja](templates/profile_person.inja)`
- **Authentication:** Token-based ownership via `ownerToken` (Bearer auth)

### Missing Infrastructure ❌

- **Image Upload:** No C++ multipart/form-data handling
- **Image Processing:** No image processing libraries (OpenCV, stb_image, etc.)
- **File Storage:** No persistent upload storage system
- **Header Fields:** Profile model lacks avatar, cover, tagline, location, availability, professional summary

## Implementation Strategy

### Phase 1: Data Model Extension (Day 1)

#### 1.1 Extend Profile Data Structure

Update `[include/search_engine/storage/Profile.h](include/search_engine/storage/Profile.h)`:

```cpp
struct PersonProfile : public Profile {
    // Existing fields...
    
    // NEW: Header Information
    std::optional<std::string> displayName;           // Preferred display name
    std::optional<std::string> englishName;           // English name variant
    std::optional<std::string> tagline;               // Professional headline (max 120 chars)
    std::optional<std::string> professionalSummary;   // Detailed bio (max 2000 chars)
    std::optional<std::string> location;              // City, Country
    std::vector<std::string> languages;               // Spoken languages
    
    // NEW: Images
    std::optional<std::string> avatarUrl;             // Avatar image path/URL
    std::optional<std::string> coverImageUrl;         // Cover image path/URL
    
    // NEW: Availability
    std::optional<std::string> availabilityStatus;    // AVAILABLE, BUSY, NOT_AVAILABLE
    
    // Skills already exists, but add metadata
    std::vector<SkillWithLevel> skillsWithLevel;      // Skills with proficiency
};

struct SkillWithLevel {
    std::string name;
    std::string level;        // BEGINNER, INTERMEDIATE, EXPERT
    std::string category;     // TECHNICAL, BUSINESS, CREATIVE
};
```

#### 1.2 Update ProfileStorage BSON Conversion

Modify `[src/storage/ProfileStorage.cpp](src/storage/ProfileStorage.cpp)`:

- Add BSON serialization for new header fields in `personProfileToBson()`
- Add BSON deserialization in `bsonToPersonProfile()`
- Use basic builder pattern with `.extract()` for nested documents

#### 1.3 Update ProfileValidator

Extend `[include/search_engine/storage/ProfileValidator.h](include/search_engine/storage/ProfileValidator.h)`:

- Add tagline length validation (max 120 chars)
- Add professional summary length validation (max 2000 chars)
- Add avatar/cover URL format validation
- Add availability status enum validation
- Add skill level enum validation

### Phase 2: Image Upload System (Day 1-2)

#### 2.1 File Storage Strategy

**Decision:** Use local filesystem storage initially (cloud storage can be added later)

- Create `uploads/avatars/` and `uploads/covers/` directories
- Store files with secure naming: `{profileId}_{timestamp}_{hash}.{ext}`
- Serve via StaticFileController at `/uploads/*` route

#### 2.2 Image Upload Endpoint (Base64 Approach)

**Note:** Since no C++ multipart library is available, use JSON + Base64 encoding initially

Add to `[src/controllers/ProfileController.cpp](src/controllers/ProfileController.cpp)`:

```cpp
// POST /api/profiles/:id/avatar
void uploadAvatar(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // 1. Authenticate via ownerToken
    // 2. Parse JSON with base64 image data
    // 3. Validate image type (jpg, png, gif - max 5MB)
    // 4. Decode base64 to binary
    // 5. Generate secure filename
    // 6. Write to uploads/avatars/
    // 7. Update profile.avatarUrl in MongoDB
    // 8. Return new avatar URL
}

// POST /api/profiles/:id/cover
void uploadCoverImage(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Similar to uploadAvatar but for cover images (max 10MB)
}
```

**Image Validation:**

- Check file size limits (avatars: 5MB, covers: 10MB)
- Validate MIME types via magic bytes
- Basic image dimension checks (optional for MVP)

#### 2.3 Update StaticFileController

Modify `[src/controllers/StaticFileController.cpp](src/controllers/StaticFileController.cpp)`:

- Add route mapping for `/uploads/*` → `uploads/` directory
- Add proper cache headers for uploaded images
- Add security checks to prevent directory traversal

### Phase 3: Header API Endpoints (Day 2)

#### 3.1 Update Profile Update Endpoint

Extend `PUT /api/profiles/:id` in ProfileController:

- Accept new header fields (displayName, englishName, tagline, etc.)
- Validate all new fields using ProfileValidator
- Use lazy initialization for storage services
- Return updated profile data

#### 3.2 Skills Management Endpoints

```cpp
// POST /api/profiles/:id/skills
void addSkill(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Add skill with level and category
    // Normalize skill name (lowercase, trim)
    // Prevent duplicates
}

// DELETE /api/profiles/:id/skills/:skillName
void removeSkill(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Remove skill by name
}

// GET /api/skills/autocomplete?q=python
void skillAutocomplete(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Return matching skills from predefined list or existing skills
    // Can use Redis for caching popular skills
}
```

#### 3.3 Header Completeness Endpoint

```cpp
// GET /api/profiles/:id/completeness
void getHeaderCompleteness(uWS::HttpResponse<false>* res, uWS::HttpRequest* req) {
    // Calculate completeness score (0-100%)
    // Return missing fields and suggestions
}
```

**Scoring Logic:**

- Avatar: 15%
- Cover Image: 10%
- Display Name: 10%
- Tagline: 10%
- Professional Summary: 15%
- Location: 5%
- Skills (at least 3): 15%
- Languages: 5%
- Contact Info (email/phone): 10%
- Availability Status: 5%

### Phase 4: Template Updates (Day 2-3)

#### 4.1 Create Profile Header Component

Create new template: `[templates/components/profile_header.inja](templates/components/profile_header.inja)`

```html
<div class="profile-header">
    <!-- Cover Image with Gradient Overlay -->
    <div class="cover-image" style="background-image: url('{{ coverImageUrl }}')">
        <div class="cover-overlay"></div>
    </div>
    
    <!-- Header Content -->
    <div class="header-content">
        <!-- Avatar -->
        <img class="avatar" src="{{ avatarUrl }}" alt="{{ displayName }}">
        
        <!-- Name and Info -->
        <div class="header-info">
            <h1 class="display-name">{{ displayName }}</h1>
            {% if englishName %}
            <p class="english-name">{{ englishName }}</p>
            {% endif %}
            <p class="tagline">{{ tagline }}</p>
            <div class="meta-info">
                <span class="location">📍 {{ location }}</span>
                <span class="availability status-{{ availability }}">{{ availabilityText }}</span>
            </div>
        </div>
        
        <!-- CTA Buttons -->
        <div class="header-actions">
            <button class="btn-primary">Contact</button>
            <button class="btn-secondary">Hire Me</button>
        </div>
    </div>
    
    <!-- Skills -->
    <div class="skills-section">
        {% for skill in skills %}
        <span class="skill-badge skill-{{ skill.level }}">
            {{ skill.name }}
            <span class="skill-level">{{ skill.levelIcon }}</span>
        </span>
        {% endfor %}
    </div>
    
    <!-- Professional Summary -->
    {% if professionalSummary %}
    <div class="professional-summary">
        <h2>About</h2>
        <p>{{ professionalSummary }}</p>
    </div>
    {% endif %}
</div>
```

#### 4.2 Update Existing Profile Templates

Modify `[templates/profile_person.inja](templates/profile_person.inja)`:

- Include the new header component
- Remove old header markup
- Update CSS imports

#### 4.3 Add Responsive CSS

Create `[public/assets/css/profile-header.css](public/assets/css/profile-header.css)`:

- Mobile-first responsive design
- CSS variables for theming (match existing `[public/assets/css/main.css](public/assets/css/main.css)`)
- Avatar: 120x120px desktop, 80x80px mobile
- Cover: 1200x300px desktop, full-width on mobile
- Flexbox/Grid layout for header elements

### Phase 5: Skills System (Day 3)

#### 5.1 Skills Database

Create skills collection or use in-memory list:

```cpp
// include/search_engine/skills/SkillsData.h
namespace skills {
    const std::vector<std::string> TECHNICAL_SKILLS = {
        "C++", "Python", "JavaScript", "React", "Node.js", 
        "MongoDB", "Redis", "Docker", "Kubernetes", ...
    };
    
    const std::vector<std::string> BUSINESS_SKILLS = {
        "Project Management", "Leadership", "Marketing", ...
    };
    
    const std::vector<std::string> CREATIVE_SKILLS = {
        "Graphic Design", "UI/UX Design", "Video Editing", ...
    };
}
```

#### 5.2 Skill Normalization

```cpp
// include/search_engine/common/SkillNormalizer.h
class SkillNormalizer {
public:
    static std::string normalize(const std::string& skill);
    static std::string getCategory(const std::string& skill);
    static std::vector<std::string> autocomplete(const std::string& query, int limit = 10);
};
```

#### 5.3 Skills Search Index

- Add MongoDB index on `skills` array in profiles collection
- Enable text search on skills for profile discovery

### Phase 6: Privacy Controls (Day 3)

#### 6.1 Field-Level Privacy

Extend Profile model:

```cpp
struct HeaderPrivacy {
    bool showEmail = false;
    bool showPhone = false;
    bool showLocation = true;
    bool showAvailability = true;
};
```

#### 6.2 Privacy Filtering

Add method to ProfileController:

```cpp
// Filter profile data based on privacy settings and viewer
PersonProfile filterByPrivacy(const PersonProfile& profile, bool isOwner);
```

### Phase 7: Testing & Documentation (Day 3)

#### 7.1 API Documentation

Update `[docs/api/profile_endpoint.md](docs/api/profile_endpoint.md)`:

- Document new header fields
- Document image upload endpoints
- Add cURL examples
- Add response schemas

#### 7.2 Integration Tests

Create test scenarios:

```bash
# Upload avatar
curl -X POST http://localhost:3000/api/profiles/{id}/avatar \
  -H "Authorization: Bearer {token}" \
  -H "Content-Type: application/json" \
  -d '{"image": "data:image/png;base64,...", "filename": "avatar.png"}'

# Update header info
curl -X PUT http://localhost:3000/api/profiles/{id} \
  -H "Authorization: Bearer {token}" \
  -H "Content-Type: application/json" \
  -d '{
    "tagline": "Senior Software Engineer",
    "professionalSummary": "10+ years of experience...",
    "location": "Tehran, Iran",
    "availabilityStatus": "AVAILABLE"
  }'

# Add skills
curl -X POST http://localhost:3000/api/profiles/{id}/skills \
  -H "Authorization: Bearer {token}" \
  -H "Content-Type: application/json" \
  -d '{
    "skills": [
      {"name": "C++", "level": "EXPERT", "category": "TECHNICAL"},
      {"name": "Python", "level": "INTERMEDIATE", "category": "TECHNICAL"}
    ]
  }'
```

## Technical Considerations

### MongoDB Pattern Compliance

- ✅ Use `MongoDBInstance::getInstance()` before creating clients
- ✅ Use basic builder with `.extract()` for nested documents
- ✅ Use lazy initialization in controllers
- ✅ Pair `onData` with `onAborted` for POST endpoints

### Security Checklist

- ✅ Validate file sizes and types
- ✅ Use secure filename generation (prevent path traversal)
- ✅ Authenticate all mutating endpoints with `ownerToken`
- ✅ Sanitize all text inputs
- ✅ Rate limit image upload endpoints
- ✅ Validate image dimensions (prevent zip bombs)

### Performance Optimization

- Add MongoDB index on `skills` array
- Cache skill autocomplete results in Redis
- Optimize image serving with CDN headers
- Use lazy loading for cover images
- Compress images on upload (future enhancement)

### Logging

- Use `LOG_DEBUG()` for image processing steps
- Use `LOG_INFO()` for profile updates
- Use `LOG_ERROR()` for upload failures

## Alternative Approaches Considered

### Image Upload Alternative: External Service

Instead of implementing multipart/form-data in C++:

1. Use Node.js microservice for image uploads
2. Service handles multipart, validation, processing
3. Returns upload URLs to C++ backend
4. C++ backend updates MongoDB with URLs

**Pros:** Faster to implement, leverage existing Node.js ecosystem
**Cons:** Additional service dependency, network overhead

**Recommendation:** Start with Base64 in JSON for MVP, migrate to dedicated service if needed

## Database Migration

```javascript
// migration-add-header-fields.js
db.profiles.updateMany(
    { type: "PERSON" },
    {
        $set: {
            displayName: "$name",
            availabilityStatus: "AVAILABLE",
            languages: [],
            skillsWithLevel: []
        }
    }
);
```

## Success Validation

After implementation, verify:

- ✅ Header renders on all device sizes (mobile, tablet, desktop)
- ✅ Avatar/cover images upload and display correctly
- ✅ Skills autocomplete responds in < 100ms
- ✅ Profile completeness score calculates accurately
- ✅ Privacy controls hide/show fields correctly
- ✅ All API endpoints return proper errors for invalid data
- ✅ No memory leaks in image upload flow
- ✅ Template rendering time < 50ms

## Files to Create

1. `include/search_engine/storage/Skill.h` - Skill data structures
2. `include/search_engine/skills/SkillsData.h` - Predefined skills list
3. `include/search_engine/common/SkillNormalizer.h` - Skill utilities
4. `include/search_engine/common/Base64.h` - Base64 encoding/decoding
5. `include/search_engine/common/ImageValidator.h` - Image validation utilities
6. `templates/components/profile_header.inja` - Header template component
7. `public/assets/css/profile-header.css` - Header styles
8. `docs/api/profile-header-endpoints.md` - API documentation

## Files to Modify

1. `[include/search_engine/storage/Profile.h](include/search_engine/storage/Profile.h)` - Add header fields
2. `[src/storage/ProfileStorage.cpp](src/storage/ProfileStorage.cpp)` - BSON conversion
3. `[src/controllers/ProfileController.cpp](src/controllers/ProfileController.cpp)` - New endpoints
4. `[include/search_engine/storage/ProfileValidator.h](include/search_engine/storage/ProfileValidator.h)` - Add validations
5. `[templates/profile_person.inja](templates/profile_person.inja)` - Include header component
6. `[src/controllers/StaticFileController.cpp](src/controllers/StaticFileController.cpp)` - Serve uploads
7. `[src/main.cpp](src/main.cpp)` - Register new routes
8. `[CMakeLists.txt](CMakeLists.txt)` - Add new source files if needed
9. `[docs/api/profile_endpoint.md](docs/api/profile_endpoint.md)` - Update documentation

