#include <gtest/gtest.h>
#include <inja/inja.hpp>
#include <search_engine/common/ProfileSlug.h>
#include <search_engine/profile/PublicProfile.h>
#include <search_engine/profile/ProfileJson.h>
#include <search_engine/seo/SEOGenerator.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace search_engine;
using nlohmann::json;

namespace {
json fixture() {
    return {
        {"profile", {{"name", "نام اصلی"}, {"slug", "هاتف_رستمخانی"}}},
        {"links", json::array()}, {"jsonld", {{"@type", "Person"}, {"name", "نام اصلی"}}},
        {"openGraph", json::array()}, {"twitterCard", json::array()},
        {"seo", {{"title", "پروفایل"}, {"description", "معرفی"}}},
        {"baseUrl", "http://127.0.0.1"}
    };
}

std::string render(json data, const std::string& file = "profile_person.inja") {
    if (file == "profile_person.inja") profile::addPersonHeaderData(data);
    inja::Environment env(std::string(PROFILE_ROOT) + "/templates/");
    return env.render_file(file, profile::profileTemplateData(std::move(data)));
}

void exportFixture(const char* name, const std::string& html) {
    if (const char* dir = std::getenv("PROFILE_PREVIEW_DIR")) {
        std::filesystem::create_directories(dir);
        std::ofstream(std::filesystem::path(dir) / name) << html;
    }
}
}

TEST(ProfileSlug, CanonicalPersianIdentityAndDots) {
    for (const auto* slug : {"هاتف.رستمخانی", "دکتر.هاتف.رستمخانی", "دانشگاه.تهران", "john.doe", "علی.۱۲۳", "a.b", "a"}) {
        EXPECT_TRUE(common::isValidProfileSlug(slug)) << slug;
        EXPECT_EQ(common::decodeProfileSlug(slug), slug);
        EXPECT_EQ(common::decodeProfileSlug(common::encodeProfileSlug(slug)), slug);
    }
    EXPECT_EQ(common::encodeProfileSlug("john.doe"), "john.doe");
    EXPECT_EQ(common::decodeProfileSlug("john%2Edoe"), "john.doe");
    EXPECT_EQ(common::decodeProfileSlug("john%2Ddoe", true), "john-doe");
    EXPECT_FALSE(common::decodeProfileSlug("john%2Ddoe"));
    EXPECT_EQ(common::canonicalProfileSlug("هاتف_رستمخانی"), "هاتف.رستمخانی");
    EXPECT_EQ(common::canonicalProfileSlug("هاتف-رستمخانی"), "هاتف.رستمخانی");
    std::string persian;
    for (int i = 0; i < 100; ++i) persian += "ه";
    EXPECT_TRUE(common::isValidProfileSlug(persian));
    EXPECT_FALSE(common::isValidProfileSlug(persian + "ه"));
    EXPECT_TRUE(common::isValidProfileSlug(std::string(100, 'a')));
    EXPECT_FALSE(common::isValidProfileSlug(std::string(101, 'a')));
}

TEST(ProfileSlug, RejectsMalformedDotsEncodingAndNonPersianSymbols) {
    for (const auto* slug : {".هاتف", "هاتف.", "هاتف..رستمخانی", "هاتف.-رستمخانی", "هاتف-.رستمخانی",
        "هاتف_.رستمخانی", "هاتف._رستمخانی", "a..b", "..", ".", "a%2E%2Eb", "a%252Eb", "%2Eهاتف",
        "", "%", "%D", "%GG", "a%00b", "a%2Fb", "a%5Cb", "a+b", "a b", "a@b", "%252F",
        "%C0%AF", "%D9", "%FF", "é", "😀", "علی،رضا", "علی؛رضا", "ي", "ك", "علیَ", "علی.١٢٣"}) {
        EXPECT_FALSE(common::decodeProfileSlug(slug)) << slug;
        EXPECT_FALSE(common::decodeProfileSlug(slug, true)) << slug;
    }
    EXPECT_FALSE(common::isValidProfileSlug("هاتف_رستمخانی"));
    EXPECT_FALSE(common::isValidProfileSlug("john-doe"));
    EXPECT_FALSE(common::isValidProfileSlug(std::string("\xD9\x41", 2)));
    EXPECT_FALSE(common::isValidProfileSlug(std::string("\x80", 1)));
}

TEST(PublicProfile, FiltersEveryPrivacyCombinationWithoutMutatingOwner) {
    storage::PersonProfile owner;
    owner.type = storage::ProfileType::PERSON;
    owner.name = "Test";
    owner.slug = "test";
    owner.ownerToken = "secret-token";
    owner.ownerId = "secret-owner";
    owner.email = "private@example.test";
    owner.phone = "private-phone";
    owner.location = "private-location";
    owner.availabilityStatus = "AVAILABLE";
    for (int mask = 0; mask < 16; ++mask) {
        owner.privacy = {bool(mask & 1), bool(mask & 2), bool(mask & 4), bool(mask & 8)};
        const auto person = profile::publicPersonProfile(owner);
        EXPECT_EQ(person.email.has_value(), bool(mask & 1));
        EXPECT_EQ(person.phone.has_value(), bool(mask & 2));
        EXPECT_EQ(person.location.has_value(), bool(mask & 4));
        EXPECT_EQ(person.availabilityStatus.has_value(), bool(mask & 8));
        EXPECT_FALSE(person.ownerToken);
        EXPECT_FALSE(person.ownerId);
        const auto publicJson = profile::personProfileToJson(person);
        EXPECT_EQ(publicJson.contains("email"), bool(mask & 1));
        EXPECT_EQ(publicJson.contains("phone"), bool(mask & 2));
        EXPECT_EQ(publicJson.contains("location"), bool(mask & 4));
        EXPECT_EQ(publicJson.contains("availabilityStatus"), bool(mask & 8));
        EXPECT_EQ(publicJson.dump().find("secret-"), std::string::npos);
        auto data = fixture();
        data["profile"] = publicJson;
        data["jsonld"] = seo::SEOGenerator::generatePersonSchema(person, "http://127.0.0.1");
        const auto html = render(data);
        EXPECT_EQ(html.find("secret-"), std::string::npos);
        if (!(mask & 1)) EXPECT_EQ(html.find("private@example.test"), std::string::npos);
        if (!(mask & 2)) EXPECT_EQ(html.find("private-phone"), std::string::npos);
        if (!(mask & 4)) EXPECT_EQ(html.find("private-location"), std::string::npos);
        if (!(mask & 8)) EXPECT_EQ(html.find("آماده همکاری"), std::string::npos);
        // SEO also enforces contact privacy if handed an unfiltered profile.
        const auto schema = seo::SEOGenerator::generatePersonSchema(owner, "http://127.0.0.1");
        EXPECT_EQ(schema.contains("email"), bool(mask & 1));
        EXPECT_EQ(schema.contains("telephone"), bool(mask & 2));
    }
    EXPECT_EQ(owner.ownerToken, "secret-token");
    EXPECT_EQ(owner.location, "private-location");
}

TEST(PublicProfile, SerializesExtendedFieldsForThePublicPage) {
    storage::PersonProfile owner;
    owner.type = storage::ProfileType::PERSON;
    owner.name = "Saved name";
    owner.slug = "saved-profile";
    owner.displayName = "Saved display name";
    owner.englishName = "English name";
    owner.tagline = "Saved tagline";
    owner.professionalSummary = "Saved summary";
    owner.avatarUrl = "/uploads/avatars/test.png";
    owner.coverImageUrl = "/uploads/covers/test.png";
    owner.skillsWithLevel = {{"C++", "EXPERT", "TECHNICAL"}};
    const auto data = profile::personProfileToJson(profile::publicPersonProfile(owner));
    EXPECT_EQ(data["name"], "Saved name");
    EXPECT_EQ(data["displayName"], owner.displayName.value());
    EXPECT_EQ(data["englishName"], owner.englishName.value());
    EXPECT_EQ(data["tagline"], owner.tagline.value());
    EXPECT_EQ(data["professionalSummary"], owner.professionalSummary.value());
    EXPECT_EQ(data["avatarUrl"], owner.avatarUrl.value());
    EXPECT_EQ(data["coverImageUrl"], owner.coverImageUrl.value());
    EXPECT_EQ(data["skillsWithLevel"][0]["name"], "C++");
}

TEST(PublicProfile, RejectsUnsafeImageSchemes) {
    storage::PersonProfile owner;
    owner.type = storage::ProfileType::PERSON;
    owner.avatarUrl = "javascript:alert(1)";
    owner.coverImageUrl = "//example.test/image.png";
    const auto person = profile::publicPersonProfile(owner);
    EXPECT_FALSE(person.avatarUrl);
    EXPECT_FALSE(person.coverImageUrl);
    EXPECT_TRUE(profile::isPublicImageUrl("/uploads/avatars/a.png"));
    EXPECT_TRUE(profile::isPublicImageUrl("https://example.test/image.png"));
}

TEST(ProfileTemplate, MinimalProfileRendersFallbacksAndHidesEmptySections) {
    const auto html = render(fixture());
    EXPECT_NE(html.find("نام اصلی"), std::string::npos);
    EXPECT_NE(html.find("avatar-placeholder"), std::string::npos);
    EXPECT_NE(html.find("lang=\"fa\" dir=\"rtl\""), std::string::npos);
    EXPECT_EQ(html.find("skills-heading"), std::string::npos);
    EXPECT_EQ(html.find("اطلاعات عمومی"), std::string::npos);
    EXPECT_EQ(html.find("<h2>"), std::string::npos);
    exportFixture("minimal.html", html);
}

TEST(ProfileTemplate, FullProfileUsesPreferredNameAndLeveledSkillsOnce) {
    auto data = fixture();
    data["profile"].update({
        {"displayName", "هاتف رستمخانی"}, {"englishName", "Hatef Rostamkhani"},
        {"tagline", "توسعه‌دهنده نرم‌افزار و علاقه‌مند به ساخت ابزارهای کاربردی"},
        {"location", "تهران، ایران"}, {"availabilityStatus", "AVAILABLE"},
        {"avatarUrl", "/fixtures/avatar.svg"}, {"coverImageUrl", "/fixtures/cover.svg"},
        {"skills", {"legacy-only"}}, {"skillsWithLevel", {{{"name", "C++"}, {"level", "EXPERT"}}, {{"name", "طراحی وب"}, {"level", "INTERMEDIATE"}}}},
        {"bio", "به ساخت تجربه‌های ساده و کاربردی علاقه دارم."}, {"title", "مهندس نرم‌افزار"},
        {"company", "Hatef.ir"}, {"education", "مهندسی کامپیوتر"}, {"school", "دانشگاه"}
    });
    data["links"].push_back({{"id", "test-link"}, {"title", "نمونه‌کارها"}, {"url", "https://example.test/portfolio"}});
    const auto html = render(data);
    EXPECT_NE(html.find("<h1 dir=\"auto\">هاتف رستمخانی</h1>"), std::string::npos);
    EXPECT_NE(html.find("آماده همکاری"), std::string::npos);
    EXPECT_NE(html.find("پیشرفته"), std::string::npos);
    EXPECT_NE(html.find("/l/test-link"), std::string::npos);
    EXPECT_EQ(html.find("legacy-only"), std::string::npos);
    EXPECT_EQ(html.find("C++", html.find("C++") + 1), std::string::npos);
    exportFixture("full.html", html);
    data["profile"]["avatarUrl"] = "/missing-avatar.png";
    data["profile"]["coverImageUrl"] = "/missing-cover.png";
    exportFixture("broken.html", render(data));
    data["profile"]["displayName"] = std::string(150, 'A') + " هاتف رستمخانی";
    data["profile"]["skillsWithLevel"][0]["name"] = std::string(180, 'B');
    exportFixture("long.html", render(data));
}

TEST(ProfileTemplate, FallsBackToLegacySkillsAndHidesEmptyValues) {
    auto data = fixture();
    data["profile"].update({{"displayName", ""}, {"location", ""}, {"title", ""},
        {"bio", ""}, {"skills", {"Python"}}, {"skillsWithLevel", json::array()}});
    const auto html = render(data);
    EXPECT_NE(html.find("نام اصلی"), std::string::npos);
    EXPECT_NE(html.find("Python"), std::string::npos);
    EXPECT_EQ(html.find("اطلاعات حرفه‌ای"), std::string::npos);
    EXPECT_EQ(html.find("اطلاعات عمومی"), std::string::npos);
    EXPECT_EQ(html.find("درباره من"), std::string::npos);
}

TEST(ProfileTemplate, EscapesMarkupAndKeepsJsonLdParseable) {
    auto data = fixture();
    const std::string attack = "</script><script>alert('x')</script><img src=x onerror=alert(1)> & \"";
    data["profile"]["name"] = attack;
    data["profile"]["bio"] = attack;
    data["jsonld"]["name"] = attack;
    data["seo"]["description"] = attack;
    const auto html = render(data);
    EXPECT_EQ(html.find("<script>alert"), std::string::npos);
    EXPECT_EQ(html.find("<img src=x"), std::string::npos);
    EXPECT_NE(html.find("&lt;img"), std::string::npos);
    const auto prepared = profile::profileTemplateData(data);
    EXPECT_EQ(json::parse(prepared["jsonld"].get<std::string>())["name"], attack);
    exportFixture("escaped.html", html);
}

TEST(ProfileTemplate, BusinessTemplateStillRenders) {
    auto data = fixture();
    data["profile"]["name"] = "Business & Company";
    data["jsonld"]["@type"] = "Organization";
    const auto html = render(data, "profile_organization.inja");
    EXPECT_NE(html.find("Business &amp; Company"), std::string::npos);
    EXPECT_NE(html.find("Organization"), std::string::npos);
}

TEST(ProfileSeo, LongPersianMetadataRemainsValidUtf8) {
    storage::PersonProfile person;
    person.name = "هاتف";
    person.bio = std::string();
    for (int i = 0; i < 100; ++i) *person.bio += "فارسی";
    for (size_t limit : {0, 1, 2, 3, 4, 159, 160}) {
        const auto description = seo::SEOGenerator::generateMetaDescription(person, limit);
        EXPECT_LE(description.size(), limit);
        EXPECT_NO_THROW(json(description).dump());
    }
}

TEST(ProfileTemplate, PublicPresentationIsInertEscapedJson) {
    auto data = fixture();
    const std::string attack = "</template><script>window.injected=true</script>";
    data["publicPresentation"] = json{{"name",attack},{"sections",json::object()}}.dump();
    const auto html = render(data);
    EXPECT_EQ(html.find("<script>window.injected"),std::string::npos);
    EXPECT_NE(html.find("id=\"public-profile-data\""),std::string::npos);
    EXPECT_NE(html.find("&lt;/template&gt;"),std::string::npos);
}
