#pragma once
#include "../storage/Profile.h"
namespace search_engine::profile {
ProfileContent effectiveContent(const storage::PersonProfile& person);
void initializeContentSection(storage::PersonProfile& person, const std::string& section);
void prunePersonReferences(storage::PersonProfile& person);
Json searchableProfile(const storage::PersonProfile& person);
Json profileCompletion(const storage::PersonProfile& person);
void bridgeLegacyPatch(storage::PersonProfile& person, const Json& patch);
void bridgeLegacySkillsAddition(storage::PersonProfile& person);
bool removePersonSkill(storage::PersonProfile& person, const std::string& name);
Json publicSectionCards(const ProfileContent& content, const std::string& profileId);
}
