# 🚀 Personal Profile Header & Basic Info

**Duration:** 3 days
**Dependencies:** Profile database models, Clean URL routing
**Acceptance Criteria:**
- ✅ Profile header with avatar and cover image
- ✅ Name fields (Unicode/English) with proper display
- ✅ Tagline/bio section with character limits
- ✅ Location and contact information
- ✅ Skills and expertise tags
- ✅ Professional summary section
- ✅ Avatar upload and image processing
- ✅ Responsive header design

## 🎯 Task Description

Create the header section for personal profiles with all the essential information that visitors see first. This includes photos, names, professional summary, and key contact details.

## 📋 Daily Breakdown

### Day 1: Header Layout & Basic Fields
- Design profile header HTML template
- Implement avatar and cover image display
- Add name fields with Unicode/English support
- Create tagline/bio input with validation
- Add location and basic contact fields
- Implement responsive design for mobile

### Day 2: Skills & Expertise System
- Create skills tag system with autocomplete
- Implement skill levels (Beginner/Intermediate/Expert)
- Add skill categories (Technical, Business, Creative)
- Create skill validation and normalization
- Add skill search and filtering capabilities

### Day 3: Professional Summary & CTA
- Implement professional summary section
- Add call-to-action buttons (contact, hire, etc.)
- Create availability status (available, busy, etc.)
- Add professional headline generation
- Implement header preview and editing
- Add header completeness scoring

## 🔧 Header Data Structure

```cpp
struct PersonalHeader {
    std::string profileId;
    std::string displayName;
    std::string englishName;
    std::string tagline;
    std::string professionalSummary;
    std::string avatarUrl;
    std::string coverImageUrl;
    std::string location;
    std::string email;
    std::string phone;
    AvailabilityStatus availability;
    std::vector<Skill> skills;
    std::vector<std::string> languages;
};
```

## 🎨 Design Requirements

### Visual Hierarchy
- Large avatar (120x120px) with rounded corners
- Cover image (1200x300px) with overlay gradient
- Name in large, readable font
- Tagline in secondary color
- Skills displayed as badges/chips
- CTA buttons prominently placed

### Responsive Design
- Mobile: Stacked layout, smaller images
- Tablet: Compact header with side-by-side elements
- Desktop: Full-width header with rich layout

## 🧪 Testing Strategy

### UI Component Tests
```cpp
TEST(PersonalHeaderTest, RenderCompleteHeader) {
    auto header = createCompleteHeader();
    auto html = renderHeader(header);
    EXPECT_TRUE(html.find("avatar") != std::string::npos);
    EXPECT_TRUE(html.find("John Doe") != std::string::npos);
}
```

### Data Validation Tests
```cpp
TEST(PersonalHeaderTest, ValidateHeaderFields) {
    PersonalHeader header;
    header.tagline = std::string(500, 'a'); // Too long
    EXPECT_FALSE(header.isValid());
}
```

### Integration Tests
```bash
# Test header rendering
curl http://localhost:3000/profiles/john-doe | grep "header"

# Test avatar upload
curl -X POST http://localhost:3000/api/profiles/avatar \
  -F "image=@avatar.jpg" \
  -H "Authorization: Bearer token"
```

## 🔒 Privacy Controls

### Header Privacy Settings
- Public: Full header visible to everyone
- Limited: Basic info only, contact hidden
- Private: Header hidden, profile not searchable
- Custom: Granular control per field

### Contact Information
- Email visibility controls
- Phone number privacy settings
- Social media link visibility
- Professional contact preferences

## 🎉 Success Criteria
- Header renders correctly across devices
- Avatar images upload and process within 2 seconds
- All text fields support Unicode characters
- Skills autocomplete works with 1000+ skills
- Header completeness score calculates accurately
- Privacy controls work as expected
- Page load time < 1 second for header section
