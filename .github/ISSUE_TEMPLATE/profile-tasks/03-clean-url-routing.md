# 🚀 Clean URL Routing System

**Duration:** 3 days
**Dependencies:** Profile routing & CRUD API
**Acceptance Criteria:**
- ✅ URL patterns: hatef.ir/username, hatef.ir/@handle, hatef.ir/company-name
- ✅ Automatic slug generation from names
- ✅ Unicode-friendly slug creation (Persian/Arabic support)
- ✅ Duplicate slug collision handling
- ✅ SEO-friendly URL redirects
- ✅ URL validation and sanitization
- ✅ Performance tests for URL resolution

## 🎯 Task Description

Implement clean, readable URLs for profiles that work across different languages and character sets. Support both username-style URLs and company name URLs with proper Unicode handling.

## 📋 Daily Breakdown

### Day 1: Slug Generation Engine
- Create SlugGenerator utility class
- Implement Unicode normalization for Persian/Arabic
- Add automatic transliteration for non-ASCII characters
- Handle special characters and punctuation
- Create slug uniqueness checking

### Day 2: URL Routing Implementation
- Extend ProfileController with clean URL routes
- Implement GET /:slug routing pattern
- Add profile type detection from URL pattern
- Create fallback routing for edge cases
- Handle URL conflicts and redirects

### Day 3: Advanced URL Features
- Implement @handle short URLs (hatef.ir/@username)
- Add URL change history and redirects
- Create URL validation middleware
- Implement SEO redirects (301/302)
- Add performance caching for URL resolution

## 🔧 URL Patterns Supported

```cpp
// Supported URL formats
GET /john-doe                    // Person profile
GET /@johndoe                    // Short handle
GET /apple-inc                   // Company profile
GET /شرکت-تست                    // Persian company name
GET /موبایل-فروشان              // Persian business

// API endpoints for URL management
GET  /api/profiles/check-slug    // Check slug availability
POST /api/profiles/change-slug   // Change profile URL
```

## 🧪 Testing Strategy

### URL Generation Tests
```cpp
TEST(SlugGeneratorTest, PersianSlugGeneration) {
    std::string input = "شرکت موبایل فروشان";
    std::string expected = "شرکت-موبایل-فروشان";
    EXPECT_EQ(generateSlug(input), expected);
}

TEST(SlugGeneratorTest, HandleCollisions) {
    // Test duplicate slug resolution
    EXPECT_EQ(resolveSlugConflict("john-doe"), "john-doe-2");
}
```

### Integration Tests
```bash
# Test various URL formats
curl http://localhost:3000/john-doe
curl http://localhost:3000/@johndoe
curl http://localhost:3000/شرکت-تست

# Test URL availability
curl "http://localhost:3000/api/profiles/check-slug?slug=test-slug"
```

## 🔍 Unicode Handling

### Persian/Arabic Characters
- Normalize different forms of same letters
- Remove diacritics and special marks
- Convert Arabic numerals to standard digits
- Handle right-to-left text properly

### Edge Cases
- Empty or whitespace-only names
- Names with only special characters
- Reserved words and system paths
- Maximum length constraints

## 🎉 Success Criteria
- All URL formats resolve correctly
- Persian/Arabic URLs work properly
- Slug generation is deterministic and fast
- URL conflicts resolved automatically
- SEO redirects work for URL changes
- URL resolution < 10ms average response time
