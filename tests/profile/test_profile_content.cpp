#include <gtest/gtest.h>
#include "search_engine/profile/ProfileContent.h"
#include "search_engine/profile/ProfileProjection.h"
#include "search_engine/profile/ProfileJson.h"
#include "search_engine/profile/PublicProfile.h"
#include "search_engine/profile/ProfileContentLabels.h"
using namespace search_engine;
using namespace search_engine::profile;
namespace {
ContentItem item(std::string section,std::string id,Json values={}){values["id"]=id;return parseContentItem(section,values);}
storage::PersonProfile person(){storage::PersonProfile p;p.id="507f1f77bcf86cd799439011";p.name="هاتف آزمایشی";p.slug="هاتف.آزمایشی";p.isPublic=true;return p;}
}
TEST(ProfileContent, AllSectionsAreTypedAndHavePersianLabels) {
    EXPECT_EQ(sectionDefinitions().size(),14);
    for(const auto& def:sectionDefinitions()) {
        auto value=item(def.key,"sample-item");EXPECT_NO_THROW(parseContentItem(def.key,itemJson(value)));
        for(auto it=def.defaults.begin();it!=def.defaults.end();++it)EXPECT_TRUE(contentLabels().contains(it.key()))<<it.key();
        auto invalid=itemJson(value);invalid["ownerToken"]="forged";EXPECT_THROW(parseContentItem(def.key,invalid),std::invalid_argument);
    }
    EXPECT_THROW(sectionDefinition("unknown"),std::invalid_argument);
}
TEST(ProfileContent, IncompleteHiddenAndOneCharacterDraftsAreAllowed) {
    EXPECT_NO_THROW(item("projects","sample-item",{{"title","ه"}}));
    EXPECT_NO_THROW(item("contacts","sample-item",{{"type","EMAIL"},{"value","h"}}));
    EXPECT_THROW(item("projects","sample-item",{{"visibility","PUBLIC"}}),std::invalid_argument);
    EXPECT_NO_THROW(item("projects","sample-item",{{"title","ه"},{"visibility","PUBLIC"}}));
    EXPECT_THROW(item("projects","sample-item",{{"title",false}}),std::invalid_argument);
}
TEST(ProfileContent, TextLimitsCountUnicodeAndRejectMalformedText) {
    std::string s;for(int i=0;i<200;++i)s+="ه";
    EXPECT_NO_THROW(item("projects","sample-item",{{"title",s}}));
    EXPECT_THROW(item("projects","sample-item",{{"title",s+"ه"}}),std::invalid_argument);
    EXPECT_THROW(item("projects","sample-item",{{"title",std::string("\xff")}}),std::invalid_argument);
    EXPECT_THROW(item("projects","sample-item",{{"title",std::string("x\0y",3)}}),std::invalid_argument);
}
TEST(ProfileContent, DatesKeepPrecisionAndCalendar) {
    auto value=item("experiences","sample-item",{{"startDate",{{"year",1403}}},{"endDate",{{"year",1403},{"month",4}}}});
    auto data=itemJson(value);EXPECT_EQ(data["startDate"]["day"],0);EXPECT_EQ(data["startDate"]["calendar"],"persian");
    EXPECT_NO_THROW(item("experiences","sample-item",{{"startDate",{{"year",1403},{"month",4}}},{"endDate",{{"year",1403}}}}));
    EXPECT_NO_THROW(item("experiences","sample-item",{{"startDate",{{"year",1403},{"month",12},{"day",30}}}}));
    EXPECT_THROW(item("experiences","sample-item",{{"startDate",{{"year",1404},{"month",12},{"day",30}}}}),std::invalid_argument);
    EXPECT_THROW(item("experiences","sample-item",{{"startDate",{{"year",4294967297LL}}}}),std::invalid_argument);
    EXPECT_THROW(item("projects","sample-item",{{"startDate",{{"year",2025},{"month",2},{"day",30},{"calendar","gregory"}}}}),std::invalid_argument);
    EXPECT_THROW(item("projects","sample-item",{{"startDate",{{"month",2}}}}),std::invalid_argument);
    EXPECT_THROW(item("projects","sample-item",{{"startDate",{{"year",1404},{"month",8},{"day",31}}}}),std::invalid_argument);
}
TEST(ProfileContent, PhoneValidationUsesUnicodeDigitsRatherThanUtf8ByteRanges) {
    for (const auto& number : {"+۹۸ ۹۱۲ ۳۴۵۶۷۸۹", "+98 (912) 345-6789"})
        EXPECT_NO_THROW(item("contacts","phone-item",{{"label","تماس"},{"type","PHONE"},{"value",number},{"visibility","PUBLIC"}}));
    for (const auto& number : {"تلفن", "---", "۱۲+۳", "۱۲a۳"})
        EXPECT_THROW(item("contacts","phone-item",{{"label","تماس"},{"type","PHONE"},{"value",number},{"visibility","PUBLIC"}}),std::invalid_argument);
}
TEST(ProfileContent, OwnersCannotClaimVerificationAndRecommendationsNeedSource) {
    Json evidence={{"id","evidence-id"},{"title","اثر"},{"url","https://example.com"},{"verificationStatus","HATEF_VERIFIED"}};
    EXPECT_THROW(item("projects","sample-item",{{"evidence",Json::array({evidence})}}),std::invalid_argument);
    EXPECT_THROW(item("recommendations","sample-item",{{"authorName","علی"},{"content","توصیه"},{"visibility","PUBLIC"}}),std::invalid_argument);
    EXPECT_NO_THROW(item("recommendations","sample-item",{{"authorName","علی"},{"content","توصیه"},{"sourceUrl","https://example.com/source"},{"visibility","PUBLIC"}}));
}
TEST(ProfileContent, ExternalLinksNeverAllowExecutableProtocols) {
    for(const auto& url:{"javascript:alert(1)","data:text/html,test","//evil.test","https://user:secret@example.com","https://example.com/\nheader"})EXPECT_FALSE(safeContentUrl(url));
    EXPECT_TRUE(safeContentUrl("https://example.com/مدرک?q=1"));
    EXPECT_THROW(item("projects","sample-item",{{"links",Json::array({{{"url","javascript:alert(1)"}}})}}),std::invalid_argument);
}
TEST(ProfileContent, HiddenReferencesAndFeaturedAreRemovedTogether) {
    auto p=person();p.content.sections["skills"]={item("skills","secret-skill",{{"name","secret"}})};
    p.content.sections["projects"]={item("projects","public-project",{{"title","نمونه"},{"visibility","PUBLIC"},{"skillIds",{"secret-skill"}}})};
    p.content.featured={{"skills","secret-skill"},{"projects","public-project"}};EXPECT_NO_THROW(validateContent(p.content));
    auto pub=publicPersonProfile(p);auto data=personProfileToJson(pub).dump();EXPECT_EQ(data.find("secret"),std::string::npos);
    EXPECT_EQ(pub.content.featured.size(),1);EXPECT_TRUE(std::get<Project>(pub.content.sections["projects"][0].value).skillIds.empty());
    p.content.visibility["projects"]="HIDDEN";EXPECT_TRUE(publicPersonProfile(p).content.featured.empty());
}
TEST(ProfileContent, DeletionPrunesReferencesAndFeatured) {
    ProfileContent c;c.sections["skills"]={item("skills","skill-one",{{"name","C++"}})};
    c.sections["projects"]={item("projects","project-one",{{"skillIds",{"skill-one"}}})};c.featured={{"skills","skill-one"}};
    c.sections["skills"].clear();removeContentReferences(c,"skills","skill-one");EXPECT_NO_THROW(validateContent(c));
    EXPECT_TRUE(c.featured.empty());EXPECT_TRUE(std::get<Project>(c.sections["projects"][0].value).skillIds.empty());
}
TEST(ProfileContent, SectionLimitsDuplicateIdsAndForeignReferencesAreRejected) {
    ProfileContent c;for(int i=0;i<21;++i)c.sections["languages"].push_back(item("languages","language-"+std::to_string(i)));
    EXPECT_THROW(validateContent(c),std::invalid_argument);c.sections.clear();
    c.sections["projects"]={item("projects","project-one",{{"skillIds",{"foreign-id"}}})};EXPECT_THROW(validateContent(c),std::invalid_argument);
    c.sections["projects"]={item("projects","project-one"),item("projects","project-one")};EXPECT_THROW(validateContent(c),std::invalid_argument);
}
TEST(ProfileContent, LegacyFallbackIsStableNonMutatingAndEmptyDoesNotResurrect) {
    auto p=person();p.skills={"C++"};p.school="دانشگاه";auto first=effectiveContent(p),second=effectiveContent(p);
    EXPECT_EQ(first.sections["skills"][0].id,second.sections["skills"][0].id);EXPECT_TRUE(p.content.sections.empty());
    p.content.sections["skills"]={};EXPECT_TRUE(effectiveContent(p).sections["skills"].empty());
    p.content.sections["education"]={};auto pub=publicPersonProfile(p);EXPECT_FALSE(pub.school);EXPECT_FALSE(pub.education);
}
TEST(ProfileContent, LegacyEditsPreserveEvidenceAndAdvancedFields) {
    auto p=person();p.content.sections["skills"]={item("skills","skill-one",{{"name","C++"},{"description","تجربهٔ قدیمی"},{"evidence",Json::array({{{"id","evidence-one"},{"url","https://example.com"}}})}})};
    p.skillsWithLevel={{"C++","EXPERT",""}};bridgeLegacyPatch(p,{{"skillsWithLevel",Json::array()}});
    const auto& skill=p.content.sections["skills"][0];EXPECT_EQ(skill.evidence.size(),1);EXPECT_EQ(std::get<AdvancedSkill>(skill.value).description,"تجربهٔ قدیمی");EXPECT_EQ(skill.id,"skill-one");
}
TEST(ProfileContent, ProjectionExcludesSecretsContactsAndPrivateItemsFromSearch) {
    auto p=person();p.ownerToken="key-secret";p.ownerTokenHash="hash-secret";p.ownerId="owner-secret";p.email="mail-secret@example.com";p.location="city-secret";p.privacy.showLocation=false;
    p.content.sections["contacts"]={item("contacts","contact-one",{{"type","EMAIL"},{"label","mail-secret"},{"value","mail-secret@example.com"},{"visibility","PUBLIC"}})};
    p.content.sections["projects"]={item("projects","project-one",{{"title","project-secret"}})};
    EXPECT_EQ(personProfileToJson(publicPersonProfile(p)).dump().find("secret"),std::string::npos);
    EXPECT_EQ(searchableProfile(p).dump().find("secret"),std::string::npos);
    p.isPublic=false;EXPECT_TRUE(searchableProfile(p).empty());
}
TEST(ProfileContent, PersianNormalizationKeepsProgrammingLanguagesDistinct) {
    EXPECT_EQ(normalizeProfileTerm("  علي كريمي  "),"علی کریمی");EXPECT_NE(normalizeProfileTerm("C++"),normalizeProfileTerm("C"));EXPECT_NE(normalizeProfileTerm("C#"),normalizeProfileTerm("C++"));
}
TEST(ProfileContent, CompletionSupportsStudentsAndDoesNotRewardItemSpam) {
    auto p=person();p.content.goal="FIND_JOB";p.content.sections["projects"]={item("projects","project-one",{{"title","پروژهٔ دانشجویی"},{"description","شرح"},{"outcomes",{"نتیجه"}},{"visibility","PUBLIC"}})};
    const auto score=profileCompletion(p)["score"];
    for(int i=0;i<20;++i)p.content.sections["projects"].push_back(item("projects","project-more-"+std::to_string(i),{{"title","ناقص"},{"visibility","PUBLIC"}}));
    EXPECT_EQ(profileCompletion(p)["score"],score);EXPECT_GT(score.get<int>(),30);
    EXPECT_FALSE(personProfileToJson(publicPersonProfile(p)).contains("completion"));
}
TEST(ProfileContent, PublicCardsStartWithThreeAndEscapeAtTemplateBoundary) {
    auto p=person();for(int i=0;i<5;++i)p.content.sections["projects"].push_back(item("projects","project-"+std::to_string(i),{{"title","<script>alert(1)</script>"},{"visibility","PUBLIC"}}));
    auto cards=publicSectionCards(publicPersonProfile(p).content,*p.id);ASSERT_FALSE(cards.empty());EXPECT_EQ(cards[0]["items"].size(),3);
    auto escaped=escapeTemplateStrings(cards).dump();EXPECT_EQ(escaped.find("<script>"),std::string::npos);
}
TEST(ProfileContent, OldSkillsEndpointsKeepAdvancedItemsAndCanRemoveThem) {
    auto p=person();p.content.sections["skills"]={item("skills","advanced-one",{{"name","C++"},{"description","detail"},{"proficiencyLevel","ADVANCED"}})};
    p.skillsWithLevel={{"Go","BEGINNER",""}};bridgeLegacySkillsAddition(p);
    ASSERT_EQ(p.content.sections["skills"].size(),2);EXPECT_EQ(std::get<AdvancedSkill>(p.content.sections["skills"][0].value).proficiencyLevel,"ADVANCED");
    EXPECT_TRUE(removePersonSkill(p,"C++"));EXPECT_EQ(p.content.sections["skills"].size(),1);EXPECT_FALSE(removePersonSkill(p,"C#"));
}

TEST(ProfileContent, ExperienceAndProjectMediaRoundTripWithPrivacyAndLimits) {
    for(const auto& section:{"experiences","projects"}) {
        auto p=person();auto values=Json{{"roleTitle","نقش"}};
        if(std::string(section)=="projects")values={{"title","پروژه"}};
        values["visibility"]="PUBLIC";values["media"]=Json::array({{{"id","image-one"},{"alt","نمودار مرتبط"}}});
        p.content.sections[section]={item(section,"media-item",values)};
        EXPECT_EQ(itemJson(publicPersonProfile(p).content.sections[section][0])["media"],values["media"]);
        p.content.visibility[section]="HIDDEN";EXPECT_TRUE(publicPersonProfile(p).content.sections[section].empty());
        values["media"].push_back(values["media"][0]);EXPECT_THROW(item(section,"media-item",values),std::invalid_argument);
        values["media"]=Json::array();for(int i=0;i<11;++i)values["media"].push_back({{"id","image-"+std::to_string(i)}});
        EXPECT_THROW(item(section,"media-item",values),std::invalid_argument);
    }
}
