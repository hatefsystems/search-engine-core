#include "search_engine/profile/ProfileProjection.h"
#include "search_engine/profile/ProfileEditor.h"
#include "search_engine/profile/PublicProfile.h"
#include <algorithm>
#include <set>
#include "search_engine/profile/ProfileContentLabels.h"

namespace search_engine::profile {
namespace {
ContentItem legacy(const storage::PersonProfile& p, const std::string& section, const std::string& key, Json fields, bool visible = true) {
    fields["id"] = "legacy-" + keyHash(p.id.value_or(p.slug) + ":" + section + ":" + key).substr(0,24);
    fields["visibility"] = visible ? "PUBLIC" : "HIDDEN";
    return parseContentItem(section,fields,true);
}
}
ProfileContent effectiveContent(const storage::PersonProfile& p) {
    auto content = p.content;
    if (!content.sections.count("skills")) {
        auto& items = content.sections["skills"]; std::set<std::string> seen;
        if (!p.skillsWithLevel.empty()) for (const auto& skill : p.skillsWithLevel) {
            if (skill.name.empty() || !seen.insert(normalizeProfileTerm(skill.name)).second) continue;
            items.push_back(legacy(p,"skills",normalizeProfileTerm(skill.name),{{"name",skill.name},{"proficiencyLevel",skill.level},{"category",skill.category}}));
        } else for (const auto& name : p.skills) if (!name.empty() && seen.insert(normalizeProfileTerm(name)).second)
            items.push_back(legacy(p,"skills",normalizeProfileTerm(name),{{"name",name}}));
    }
    if (!content.sections.count("education") && (!p.education.value_or("").empty() || !p.school.value_or("").empty()))
        content.sections["education"] = {legacy(p,"education","education",{{"kind","ACADEMIC"},{"institutionName",p.school.value_or(p.education.value_or(""))},{"degree",p.education.value_or("")}})};
    if (!content.sections.count("languages")) for (const auto& name : p.languages) if (!name.empty())
        content.sections["languages"].push_back(legacy(p,"languages",normalizeProfileTerm(name),{{"name",name}}));
    return content;
}
void initializeContentSection(storage::PersonProfile& p, const std::string& section) {
    if (!p.content.sections.count(section)) p.content.sections[section] = effectiveContent(p).sections[section];
}
void prunePersonReferences(storage::PersonProfile& p) {
    auto effective = effectiveContent(p); removeContentReferences(effective, "", "");
    for (auto& [section,items] : p.content.sections) items = effective.sections[section];
    p.content.featured = effective.featured;
}
void bridgeLegacyPatch(storage::PersonProfile& p, const Json& patch) {
    if (!patch.contains("skillsWithLevel")) return;
    if (!p.content.sections.count("skills")) { prunePersonReferences(p); return; }
    auto old = p.content.sections["skills"]; std::vector<ContentItem> updated;
    for (const auto& skill : p.skillsWithLevel) {
        auto found = std::find_if(old.begin(),old.end(),[&](const auto& item){return normalizeProfileTerm(std::get<AdvancedSkill>(item.value).name) == normalizeProfileTerm(skill.name);});
        auto item = found != old.end() ? *found : legacy(p,"skills",normalizeProfileTerm(skill.name),{{"name",skill.name}});
        auto& data = std::get<AdvancedSkill>(item.value); data.name = skill.name; data.proficiencyLevel = skill.level;
        item.updatedAt = contentNow(); updated.push_back(item);
    }
    p.content.sections["skills"] = updated; prunePersonReferences(p);
}
void bridgeLegacySkillsAddition(storage::PersonProfile& p) {
    if (!p.content.sections.count("skills")) return;
    auto& items=p.content.sections["skills"];
    for (const auto& skill:p.skillsWithLevel) {
        const auto normalized=normalizeProfileTerm(skill.name);
        if (std::none_of(items.begin(),items.end(),[&](const auto& item){return normalizeProfileTerm(std::get<AdvancedSkill>(item.value).name)==normalized;})) {
            auto value=legacy(p,"skills",normalized,{{"name",skill.name},{"proficiencyLevel",skill.level},{"category",skill.category}});
            value.displayOrder=items.size();items.push_back(value);
        }
    }
    validateContent(effectiveContent(p));
}
bool removePersonSkill(storage::PersonProfile& p, const std::string& name) {
    const auto normalized=normalizeProfileTerm(name);size_t removed=0;
    removed+=std::erase_if(p.skills,[&](const auto& value){return normalizeProfileTerm(value)==normalized;});
    removed+=std::erase_if(p.skillsWithLevel,[&](const auto& value){return normalizeProfileTerm(value.name)==normalized;});
    if(p.content.sections.count("skills")) removed+=std::erase_if(p.content.sections["skills"],[&](const auto& value){return normalizeProfileTerm(std::get<AdvancedSkill>(value.value).name)==normalized;});
    prunePersonReferences(p);return removed>0;
}
Json searchableProfile(const storage::PersonProfile& original) {
    if (!original.isPublic || original.deletedAt) return Json::object();
    auto p = publicPersonProfile(original);
    Json out = {{"name",normalizeProfileTerm(p.displayName.value_or(p.name))}, {"title",normalizeProfileTerm(p.title.value_or(""))},
        {"location",normalizeProfileTerm(p.location.value_or(""))},{"skills",Json::array()},{"availability",Json::array()},{"text",normalizeProfileTerm(p.bio.value_or(""))}};
    auto content = p.content;
    std::string text = out["text"];
    for (const auto& [section, items] : content.sections) {
        // Contact details, URLs and testimonials are never indexed.
        if (section == "contacts" || section == "recommendations") continue;
        for (const auto& item : items) {
            auto data = itemJson(item);
            if (section == "skills") out["skills"].push_back(normalizeProfileTerm(data.value("name","")));
            if (section == "availability" && data.value("status","") == "AVAILABLE") out["availability"].push_back(data.value("type",""));
            for (auto key : {"title","name","roleTitle","organizationName","institutionName","fieldOfStudy","repositoryName","description","summary","problem","solution","role"})
                if (data.contains(key)) text += " " + data[key].get<std::string>();
            for (auto key : {"topics","technologies","outcomes","achievements"}) if (data.contains(key)) for (const auto& v : data[key]) text += " " + v.get<std::string>();
        }
    }
    if (out["availability"].empty() && p.availabilityStatus.value_or("") == "AVAILABLE") out["availability"].push_back("COLLABORATION");
    for (const auto* field : {"skills", "availability"}) {
        std::set<std::string> unique;
        for (const auto& value : out[field]) unique.insert(value.get<std::string>());
        out[field] = unique;
    }
    out["text"] = profileSearchText(text); out["name"] = profileSearchText(out["name"]); out["title"] = profileSearchText(out["title"]); return out;
}
Json profileCompletion(const storage::PersonProfile& p) {
    const auto c = effectiveContent(p); const auto visible = publicContent(c);
    // Identity, proof of work, skills, evidence and reachable presence. Not a reputation score.
    const std::map<std::string,std::vector<int>> weights = {{"PERSONAL_IDENTITY",{25,30,15,20,10}},
        {"FIND_JOB",{20,35,20,15,10}},{"FIND_CLIENTS",{20,40,15,15,10}},
        {"PORTFOLIO",{15,45,15,20,5}},{"RESEARCH_VISIBILITY",{20,35,15,25,5}}};
    const auto w = weights.at(c.goal);
    auto count = [&](const std::string& name) { auto it=visible.sections.find(name);return it==visible.sections.end()?size_t(0):it->second.size();};
    double identity = (!p.name.empty() ? .4 : 0) + (!p.title.value_or("").empty() ? .3 : 0) + (!p.bio.value_or("").empty() || count("about") ? .3 : 0);
    std::vector<double> work; int proof = 0;
    const std::set<std::string> workSections = c.goal == "RESEARCH_VISIBILITY" ? std::set<std::string>{"publications","projects","education"} :
        c.goal == "FIND_CLIENTS" ? std::set<std::string>{"services","projects","experiences"} : std::set<std::string>{"projects","experiences","publications","openSource","education","achievements"};
    for (const auto& [section,items] : visible.sections) for (const auto& item : items) {
        if (!item.evidence.empty()) ++proof;
        if (!workSections.count(section)) continue;
        auto d = itemJson(item); double value = .3;
        for (auto key : {"description","summary","problem","solution","architecture","role"}) if (d.contains(key) && !d[key].get<std::string>().empty()) { value += .4; break; }
        for (auto key : {"outcomes","achievements","responsibilities","evidence"}) if (d.contains(key) && !d[key].empty()) { value += .3; break; }
        work.push_back(value);
    }
    std::sort(work.rbegin(),work.rend()); double workValue = work.empty()?0:work[0];
    // One substantial project is sufficient; a student is not penalized for no employment history.
    double skills = std::min(1.,count("skills") / 3.);
    double evidence = std::min(1.,proof / 2.);
    double reachable = count("contacts") || !p.portfolioUrl.value_or("").empty() || !p.githubUrl.value_or("").empty() || !p.linkedinUrl.value_or("").empty() ? 1 : 0;
    Json recommendations = Json::array();
    if (workValue < 1) recommendations.push_back({{"section",c.goal == "RESEARCH_VISIBILITY"?"publications":"projects"},{"message","یک نمونه از کاری که انجام داده‌اید، همراه با نقش و نتیجهٔ آن اضافه کنید."}});
    if (skills < 1) recommendations.push_back({{"section","skills"},{"message","مهارت‌های اصلی خود را معرفی کنید؛ سه مهارت مرتبط کافی است."}});
    if (auto it=visible.sections.find("skills");it!=visible.sections.end()) for (const auto& item:it->second) {
        const auto& skill=std::get<AdvancedSkill>(item.value);
        if ((skill.proficiencyLevel=="ADVANCED" || skill.proficiencyLevel=="EXPERT") && skill.projectIds.empty() && skill.experienceIds.empty() && item.evidence.empty()) {
            recommendations.push_back({{"section","skills"},{"itemId",item.id},{"message","برای مهارت «"+skill.name+"» یک پروژه یا شاهد مرتبط معرفی کنید."}}); break;
        }
    }
    if (c.goal=="FIND_CLIENTS" && !count("services")) recommendations.push_back({{"section","services"},{"message","خدماتی را که می‌توانید ارائه کنید توضیح دهید."}});
    return {{"goal",c.goal},{"score",int(std::round(w[0]*identity+w[1]*workValue+w[2]*skills+w[3]*evidence+w[4]*reachable))},
        {"recommendations",recommendations},{"description","راهنمای تکمیل اطلاعات؛ این امتیاز تأیید اعتبار یا رتبهٔ جست‌وجو نیست."}};
}
Json publicSectionCards(const ProfileContent& content, const std::string& profileId) {
    Json sections=Json::array(); auto order=content.order;
    std::map<std::string,std::string> titles;
    for(const auto& [section,items]:content.sections)for(const auto& item:items)titles[item.id]=itemJson(item).value(sectionDefinition(section).titleField,"");
    auto card=[&](const std::string& section,const ContentItem& item) {
        auto data=itemJson(item);Json fields=Json::array();
        const auto titleField=sectionDefinition(section).titleField;
        std::string heading=data.value(titleField,"");if(contentEnumLabels().contains(heading))heading=contentEnumLabels()[heading];
        std::string summary=data.value("summary",data.value("description",data.value("subtitle",data.value("content",""))));
        for(auto it=data.begin();it!=data.end();++it) {
            const auto key=it.key();const auto& value=it.value();
            if(!contentLabels().contains(key) || key==titleField || key=="visibility" || key=="media" || key=="evidence" || key=="links")continue;
            std::string display;
            if(value.is_string()) {
                display=value.get<std::string>();if(display==summary)continue;
                if(contentEnumLabels().contains(display))display=contentEnumLabels()[display];
            }else if(value.is_array()) {
                for(const auto& v:value)if(v.is_string()) {
                    auto text=v.get<std::string>();if(key.ends_with("Ids")){if(!titles.count(text))continue;text=titles[text];}
                    if(!display.empty())display+="، ";display+=text;
                }
            }else if(value.is_boolean()) {if(value.get<bool>())display="بله";}
            else if(value.is_number()) {if(value.get<double>()!=0)display=value.dump();}
            else if(value.is_object() && value.value("year",0)) {
                display=std::to_string(value["year"].get<int>());
                if(value.value("month",0))display+="/"+std::to_string(value["month"].get<int>());
                if(value.value("day",0))display+="/"+std::to_string(value["day"].get<int>());
                display+=value.value("calendar","")=="gregory"?" میلادی":" شمسی";
            }
            if(!display.empty())fields.push_back({{"label",contentLabels()[key]},{"value",display},{"url",(key=="url"||key.ends_with("Url"))&&safeContentUrl(display)?display:""}});
        }
        Json links=data.value("links",Json::array());for(const auto& ev:item.evidence)if(safeContentUrl(ev.url))links.push_back({{"title",ev.title+" · خوداظهاری"},{"url",ev.url}});
        Json media=Json::array();if(data.contains("media"))for(const auto& m:data["media"])media.push_back({{"alt",m.value("alt",heading)},{"url","/api/profiles/"+profileId+"/media/"+m.at("id").get<std::string>()}});
        return Json{{"id",item.id},{"heading",heading},{"summary",summary},{"fields",fields},{"links",links},{"media",media},{"isRecommendation",section=="recommendations"}};
    };
    Json featured=Json::array();
    for(const auto& ref:content.featured)if(content.sections.count(ref.section))for(const auto& item:content.sections.at(ref.section))if(item.id==ref.id)featured.push_back(card(ref.section,item));
    if(!featured.empty())sections.push_back({{"key","featured"},{"label","برگزیده‌ها"},{"items",featured},{"total",featured.size()},{"hasMore",false}});
    for(const auto& d:sectionDefinitions())if(std::find(order.begin(),order.end(),d.key)==order.end())order.push_back(d.key);
    for(const auto& key:order) {
        auto it=content.sections.find(key);if(it==content.sections.end() || it->second.empty())continue;
        Json items=Json::array();for(const auto& item:it->second){if(items.size()==3)break;items.push_back(card(key,item));}
        sections.push_back({{"key",key},{"label",sectionDefinition(key).label},{"items",items},{"total",it->second.size()},{"hasMore",it->second.size()>3}});
    }
    return sections;
}
}
