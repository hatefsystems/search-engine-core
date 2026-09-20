#include <gtest/gtest.h>
#include "search_engine/profile/ProfileEditor.h"
#include "search_engine/profile/ProfileJson.h"
#include "search_engine/profile/PublicProfile.h"
using namespace search_engine;
TEST(ProfileEditor, KeysAreRandomHashedAndProfileSpecific) {
    auto first=profile::newOwnerKey(), second=profile::newOwnerKey();
    EXPECT_EQ(first.size(),64); EXPECT_NE(first,second);
    storage::PersonProfile p; p.ownerTokenHash=profile::keyHash(first);
    EXPECT_NE(*p.ownerTokenHash,first); EXPECT_TRUE(profile::ownsProfile(p,first));
    EXPECT_FALSE(profile::ownsProfile(p,second)); EXPECT_FALSE(profile::ownsProfile(p,""));
    p.id="507f1f77bcf86cd799439011"; EXPECT_FALSE(profile::ownsProfile(p,*p.id));
    p.ownerTokenHash.reset(); p.ownerToken=first; EXPECT_TRUE(profile::ownsProfile(p,first));
}
TEST(ProfileEditor, DraftAndPartialPatchPreserveLegacyData) {
    storage::PersonProfile p; p.isPublic=false; p.englishName="Legacy name"; p.email="private@example.com";
    p.ownerTokenHash="secret-hash";
    EXPECT_NO_THROW(profile::applyEditorPatch(p,{{"name",""}}));
    EXPECT_NO_THROW(profile::applyEditorPatch(p,{{"name","ه"}}));
    EXPECT_EQ(p.name,"ه"); EXPECT_EQ(*p.displayName,"ه"); EXPECT_EQ(*p.englishName,"Legacy name");
    EXPECT_EQ(*p.email,"private@example.com"); EXPECT_EQ(*p.ownerTokenHash,"secret-hash");
    EXPECT_THROW(profile::applyEditorPatch(p,{{"englishName","new"}}),std::invalid_argument);
    EXPECT_THROW(profile::applyEditorPatch(p,{{"ownerToken","new"}}),std::invalid_argument);
}
TEST(ProfileEditor, UnicodeLimitsAndPublication) {
    storage::PersonProfile p; p.isPublic=false;
    EXPECT_THROW(profile::applyEditorPatch(p,{{"isPublic",true}}),std::invalid_argument);
    p.isPublic=false;
    EXPECT_THROW(profile::applyEditorPatch(p,{{"name","Hatef"}}),std::invalid_argument);
    std::string bio; for(int i=0;i<500;++i) bio+="ه";
    EXPECT_NO_THROW(profile::applyEditorPatch(p,{{"bio",bio}}));
    EXPECT_THROW(profile::applyEditorPatch(p,{{"bio",bio+"ه"}}),std::invalid_argument);
    EXPECT_NO_THROW(profile::applyEditorPatch(p,{{"name","هاتف رستمخانی"},{"isPublic",true}}));
    EXPECT_TRUE(p.isPublic); EXPECT_EQ(profile::textLength("هاتف"),4);
    EXPECT_THROW(profile::textLength(std::string("\xc0\xaf",2)),std::invalid_argument);
}
TEST(ProfileEditor, EmptyFieldsAndSkillsAreRepresented) {
    storage::PersonProfile p; p.isPublic=false; p.bio="old"; p.skills={"legacy"}; p.skillsWithLevel={{"C++","EXPERT","OTHER"}};
    profile::applyEditorPatch(p,{{"bio",""},{"skillsWithLevel",nlohmann::json::array()},{"avatarUrl",""}});
    EXPECT_EQ(*p.bio,""); EXPECT_TRUE(p.skills.empty()); EXPECT_TRUE(p.skillsWithLevel.empty()); EXPECT_EQ(*p.avatarUrl,"");
    EXPECT_THROW(profile::applyEditorPatch(p,{{"avatarUrl","javascript:alert(1)"}}),std::invalid_argument);
}
TEST(ProfileEditor, CredentialsNeverSerializeAndCookiesAreScoped) {
    storage::PersonProfile p; p.ownerToken="legacy-secret"; p.ownerTokenHash="secret-hash";
    auto json=profile::personProfileToJson(p).dump();
    EXPECT_EQ(json.find("secret"),std::string::npos);
    auto pub=profile::publicPersonProfile(p); EXPECT_FALSE(pub.ownerToken); EXPECT_FALSE(pub.ownerTokenHash);
    auto cookie=profile::ownerCookie("abc","key",true);
    EXPECT_NE(cookie.find("HttpOnly; SameSite=Strict; Max-Age=2592000; Secure"),std::string::npos);
    EXPECT_NE(cookie.find("Path=/api/profiles/abc"),std::string::npos);
    EXPECT_EQ(profile::readCookie("other=1; profile_owner_abc=key; last=2","abc"),"key");
    EXPECT_EQ(profile::readCookie("profile_owner_abc=key","xyz"),"");
    EXPECT_NE(profile::ownerCookie("abc","",false,true).find("Max-Age=0"),std::string::npos);
}
