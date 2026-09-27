#include "search_engine/profile/ProfileContent.h"
#include "search_engine/profile/ProfileEditor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>

namespace search_engine::profile {
namespace {
void require(bool ok, const std::string& message) { if (!ok) throw std::invalid_argument(message); }
bool identifier(const std::string& value) {
    return value.size() >= 8 && value.size() <= 64 && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    });
}
void text(const std::string& value, size_t limit) {
    require(textLength(value) <= limit, "متن بیش از حد طولانی است.");
    for (auto c : codepoints(value)) require((c >= 32 || c == 10 || c == 9) && c != 127 &&
        !(c >= 0x202A && c <= 0x202E) && !(c >= 0x2066 && c <= 0x2069), "نویسهٔ کنترلی مجاز نیست.");
}
bool phoneNumber(const std::string& value) {
    const auto chars = codepoints(value);
    if (chars.size() < 3 || chars.size() > 30) return false;
    size_t digits = 0;
    for (size_t i = 0; i < chars.size(); ++i) {
        const auto c = chars[i];
        if ((c >= '0' && c <= '9') || (c >= 0x06F0 && c <= 0x06F9)) ++digits;
        else if (c != ' ' && c != '(' && c != ')' && c != '-' && !(c == '+' && i == 0)) return false;
    }
    return digits >= 3;
}
void fieldsOnly(const Json& value, const Json& defaults) {
    require(value.is_object(), "ساختار اطلاعات نامعتبر است.");
    for (auto it = value.begin(); it != value.end(); ++it) require(defaults.contains(it.key()), "فیلد قابل ویرایش نیست: " + it.key());
}
void date(const Json& value) {
    fieldsOnly(value, Json(PartialDate{}));
    for (const auto* field : {"year", "month", "day"}) if (value.contains(field)) require(value[field].is_number_integer() && value[field].get<double>() >= 0 && value[field].get<double>() <= 9999, "تاریخ نامعتبر است.");
    auto d = value.get<PartialDate>();
    require(d.calendar == "persian" || d.calendar == "gregory", "تقویم نامعتبر است.");
    require(d.year >= 0 && d.year <= 9999 && d.month >= 0 && d.month <= 12 && d.day >= 0 && d.day <= 31, "تاریخ نامعتبر است.");
    require(d.year || (!d.month && !d.day), "سال تاریخ لازم است.");
    require(d.month || !d.day, "ماه تاریخ لازم است.");
    if (!d.day) return;
    if (d.calendar == "gregory") {
        require(std::chrono::year_month_day{std::chrono::year{d.year}, std::chrono::month{unsigned(d.month)}, std::chrono::day{unsigned(d.day)}}.ok(), "روز تاریخ معتبر نیست.");
    } else {
        // Civil Persian 33-year rule; retain the supplied calendar and precision.
        // The 2820-year arithmetic calendar would incorrectly reject 1403/12/30.
        const bool leap = (25 * d.year + 11) % 33 < 8;
        const int days = d.month <= 6 ? 31 : d.month <= 11 ? 30 : leap ? 30 : 29;
        require(d.day <= days, "روز تاریخ معتبر نیست.");
    }
}
DomainItem domain(const std::string& key, const Json& input) {
#define DOMAIN(keyName, Type) if (key == keyName) return input.get<Type>();
    DOMAIN("experiences", Experience) DOMAIN("projects", Project) DOMAIN("skills", AdvancedSkill)
    DOMAIN("education", Education) DOMAIN("certifications", Certification) DOMAIN("publications", Publication)
    DOMAIN("openSource", OpenSourceContribution) DOMAIN("services", Service) DOMAIN("achievements", Achievement)
    DOMAIN("languages", Language) DOMAIN("recommendations", Recommendation) DOMAIN("contacts", Contact)
    DOMAIN("availability", Availability) DOMAIN("about", About)
#undef DOMAIN
    throw std::invalid_argument("بخش نامعتبر است.");
}
const std::map<std::string, std::vector<std::string>>& enums() {
    static const std::map<std::string, std::vector<std::string>> values = {
        {"employmentType", {"FULL_TIME","PART_TIME","CONTRACT","FREELANCE","INTERNSHIP","CONSULTING","SELF_EMPLOYED","VOLUNTEER","OTHER"}},
        {"locationType", {"ONSITE","HYBRID","REMOTE"}},
        {"projectType", {"PROFESSIONAL","PERSONAL","OPEN_SOURCE","ACADEMIC","RESEARCH","STARTUP","CLIENT","COMMUNITY","OTHER"}},
        {"proficiencyLevel", {"BEGINNER","INTERMEDIATE","ADVANCED","EXPERT"}},
        {"proficiency", {"BASIC","CONVERSATIONAL","PROFESSIONAL","FLUENT","NATIVE"}},
        {"pricingMode", {"CONTACT","HOURLY","FIXED","STARTING_FROM","CUSTOM","FREE"}},
        {"deliveryMode", {"ONSITE","REMOTE","HYBRID"}},
        {"kind", {"ACADEMIC","COURSE","BOOTCAMP","WORKSHOP","SELF_LEARNING","RESEARCH","OTHER"}},
        {"status", {"AVAILABLE","BUSY","NOT_AVAILABLE"}}
    };
    return values;
}
std::string referenceSection(const std::string& field) {
    if (field == "skillIds") return "skills";
    if (field == "projectIds") return "projects";
    if (field == "experienceIds") return "experiences";
    if (field == "certificationIds") return "certifications";
    return "";
}
void prune(ProfileContent& content) {
    std::map<std::string, std::set<std::string>> ids;
    for (const auto& [section, items] : content.sections) for (const auto& item : items) ids[section].insert(item.id);
    for (auto& [section, items] : content.sections) for (auto& item : items) {
        auto data = itemJson(item);
        for (auto it = data.begin(); it != data.end(); ++it) {
            const auto target = referenceSection(it.key());
            if (target.empty()) continue;
            Json kept = Json::array();
            for (const auto& id : it.value()) if (ids[target].count(id.get<std::string>())) kept.push_back(id);
            it.value() = kept;
        }
        item.value = domain(section, data);
    }
    std::erase_if(content.featured, [&](const auto& ref) { return !ids[ref.section].count(ref.id); });
}
}
const std::vector<SectionDefinition>& sectionDefinitions() {
    static const std::vector<SectionDefinition> definitions = {
        {"about","دربارهٔ من","title",1,About{}},
        {"projects","پروژه‌ها و نمونه‌کارها","title",50,Project{}},
        {"experiences","تجربهٔ کاری","roleTitle",50,Experience{}},
        {"skills","مهارت‌ها","name",100,AdvancedSkill{}},
        {"education","تحصیلات و یادگیری","institutionName",50,Education{}},
        {"certifications","گواهینامه‌ها","name",50,Certification{}},
        {"publications","آثار و انتشارات","title",50,Publication{}},
        {"openSource","مشارکت متن‌باز","repositoryName",50,OpenSourceContribution{}},
        {"services","خدمات","title",50,Service{}},
        {"achievements","دستاوردها","title",50,Achievement{}},
        {"languages","زبان‌ها","name",20,Language{}},
        {"recommendations","توصیه‌نامه‌های مستند","authorName",50,Recommendation{}},
        {"contacts","راه‌های ارتباطی","label",50,Contact{}},
        {"availability","فرصت‌های همکاری","type",20,Availability{}}
    };
    return definitions;
}
const SectionDefinition& sectionDefinition(const std::string& key) {
    for (const auto& def : sectionDefinitions()) if (def.key == key) return def;
    throw std::invalid_argument("بخش نامعتبر است.");
}
bool safeContentUrl(const std::string& url) {
    if (url.size() > 2048 || url.find_first_of("\\<>\"'\r\n\t ") != std::string::npos) return false;
    static const std::regex pattern(R"(^https?://[^/@?#:]+(?::[0-9]{1,5})?(?:[/?#].*)?$)");
    return std::regex_match(url, pattern);
}
bool validDraftUrl(const std::string& url) {
    if (url.empty() || safeContentUrl(url)) return true;
    if (url.size() > 2048 || url.find_first_of("\\<>\"'\r\n\t ") != std::string::npos) return false;
    return url.find(':') == std::string::npos || std::string("https://").starts_with(url) || std::string("http://").starts_with(url);
}
std::string contentNow() {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm value{}; gmtime_r(&now, &value); std::ostringstream out; out << std::put_time(&value, "%FT%TZ"); return out.str();
}
Json itemJson(const ContentItem& item) {
    Json result = std::visit([](const auto& value) { return Json(value); }, item.value);
    result.update({{"id",item.id},{"visibility",item.visibility},{"displayOrder",item.displayOrder},
        {"createdAt",item.createdAt},{"updatedAt",item.updatedAt},{"evidence",item.evidence}});
    return result;
}
ContentItem parseContentItem(const std::string& section, const Json& input, bool fromStorage) {
    const auto& def = sectionDefinition(section);
    Json defaults = def.defaults;
    defaults.update({{"id",""},{"visibility","HIDDEN"},{"displayOrder",0},{"createdAt",""},{"updatedAt",""},{"evidence",Json::array()}});
    fieldsOnly(input, defaults);
    auto data = defaults; data.update(input);
    require(identifier(data.at("id").get<std::string>()), "شناسهٔ آیتم معتبر نیست.");
    for (auto it = data.begin(); it != data.end(); ++it) {
        const auto& baseline = defaults.at(it.key());
        require(it.value().type() == baseline.type() || (it.value().is_number() && baseline.is_number()), "نوع فیلد نامعتبر است: " + it.key());
        if (it.value().is_string()) {
            const auto value = it.value().get<std::string>();
            size_t limit = (it.key() == "description" || it.key() == "content" || it.key() == "summary" || it.key() == "problem" || it.key() == "solution" || it.key() == "architecture") ? 5000 : 200;
            if (it.key().ends_with("Url") || it.key() == "url" || it.key() == "value") limit = 2048;
            text(value, limit);
            if ((it.key().ends_with("Url") || it.key() == "url") && !value.empty()) require(safeContentUrl(value) || (data["visibility"] == "HIDDEN" && validDraftUrl(value)), "لینک معتبر http یا https وارد کنید.");
            if (auto found = enums().find(it.key()); found != enums().end() && !value.empty())
                require(std::find(found->second.begin(), found->second.end(), value) != found->second.end(), "گزینهٔ انتخاب‌شده معتبر نیست.");
        } else if (it.value().is_array()) {
            const auto field = it.key();
            require(it.value().size() <= ((field == "evidence" || field == "media" || field == "links") ? 10 : 50), "تعداد موارد بیش از حد مجاز است.");
            std::set<std::string> seen;
            for (auto& element : it.value()) {
                if (field == "evidence") {
                    fieldsOnly(element, Evidence{});
                    auto ev = element.get<Evidence>();
                    require(identifier(ev.id) && seen.insert(ev.id).second, "شناسهٔ شاهد تکراری یا نامعتبر است.");
                    require(ev.verificationStatus == "SELF_REPORTED" || (fromStorage && (ev.verificationStatus == "UNVERIFIED" || ev.verificationStatus == "EXTERNALLY_VERIFIED" || ev.verificationStatus == "HATEF_VERIFIED")), "وضعیت تأیید قابل تنظیم نیست.");
                    text(ev.title,200); text(ev.description,1000); text(ev.type,80);
                    require(ev.url.empty() || safeContentUrl(ev.url) || (data["visibility"] == "HIDDEN" && validDraftUrl(ev.url)), "لینک شاهد معتبر نیست.");
                } else if (field == "links") {
                    fieldsOnly(element, ExternalReference{}); auto link = element.get<ExternalReference>();
                    text(link.title,200); require(link.url.empty() || safeContentUrl(link.url) || (data["visibility"] == "HIDDEN" && validDraftUrl(link.url)), "لینک معتبر نیست.");
                } else if (field == "media") {
                    fieldsOnly(element, MediaReference{}); auto media = element.get<MediaReference>();
                    require(identifier(media.id) && seen.insert(media.id).second, "شناسهٔ تصویر معتبر نیست."); text(media.alt,300);
                } else {
                    require(element.is_string(), "فهرست باید متنی باشد."); auto value = element.get<std::string>();
                    text(value,500); require(!value.empty() && seen.insert(value).second, "مقدار خالی یا تکراری مجاز نیست.");
                    if (!referenceSection(field).empty()) require(identifier(value), "شناسهٔ مرتبط معتبر نیست.");
                }
            }
        } else if (it.value().is_object()) date(it.value());
        else if (it.value().is_number()) {
            require(std::isfinite(it.value().get<double>()) && it.value().get<double>() >= 0, "عدد نامعتبر است.");
            if (it.key() == "yearsOfExperience") require(it.value().get<double>() <= 100, "سابقه نامعتبر است.");
            else require(it.value().is_number_integer() && it.value().get<double>() <= 9999, "عدد صحیح معتبر لازم است.");
        }
    }
    require(data["visibility"] == "PUBLIC" || data["visibility"] == "HIDDEN", "وضعیت نمایش نامعتبر است.");
    if (data["visibility"] == "PUBLIC") {
        require(!data[def.titleField].get<std::string>().empty(), "برای نمایش عمومی، عنوان این بخش را کامل کنید.");
        if (section == "recommendations") require(!data["content"].get<std::string>().empty() && safeContentUrl(data["sourceUrl"]), "توصیه‌نامهٔ عمومی به متن و منبع نیاز دارد.");
        if (section == "contacts") require(!data["type"].get<std::string>().empty() && !data["value"].get<std::string>().empty(), "راه ارتباطی را کامل کنید.");
        if (data.contains("links")) for (const auto& link : data["links"]) require(safeContentUrl(link.value("url", "")), "لینک را کامل کنید.");
        for (const auto& ev : data["evidence"]) require(!ev.value("title", "").empty() && safeContentUrl(ev.value("url", "")), "عنوان و لینک شاهد را کامل کنید.");
    }
    if (section == "contacts") {
        const auto type = data["type"].get<std::string>(); const auto value = data["value"].get<std::string>();
        require(type.empty() || type == "EMAIL" || type == "PHONE" || type == "LINK", "نوع تماس معتبر نیست.");
        if (!value.empty() && type == "LINK") require(safeContentUrl(value) || (data["visibility"] == "HIDDEN" && validDraftUrl(value)), "لینک تماس معتبر نیست.");
        if (data["visibility"] == "PUBLIC" && !value.empty() && type == "EMAIL") require(std::regex_match(value, std::regex(R"(^[^\s@]+@[^\s@]+\.[^\s@]+$)")), "ایمیل معتبر نیست.");
        if (data["visibility"] == "PUBLIC" && !value.empty() && type == "PHONE") require(phoneNumber(value), "شماره معتبر نیست.");
    }
    if (data.contains("startDate") && data.contains("endDate")) {
        const auto start = data["startDate"].get<PartialDate>(), end = data["endDate"].get<PartialDate>();
        if (start.year && end.year) {
            require(start.calendar == end.calendar, "تقویم شروع و پایان یکسان باشد.");
            require(end.year > start.year || (end.year == start.year && (!end.month || !start.month || end.month > start.month || (end.month == start.month && (!end.day || !start.day || end.day >= start.day)))), "پایان نمی‌تواند پیش از شروع باشد.");
        }
    }
    ContentItem item;
    item.id = data["id"]; item.visibility = data["visibility"]; item.displayOrder = data["displayOrder"];
    item.createdAt = data["createdAt"]; item.updatedAt = data["updatedAt"]; item.evidence = data["evidence"].get<std::vector<Evidence>>();
    item.value = domain(section,data); return item;
}
Json contentJson(const ProfileContent& content) {
    Json sections = Json::object();
    for (const auto& [name, items] : content.sections) { sections[name] = Json::array(); for (const auto& item : items) sections[name].push_back(itemJson(item)); }
    return {{"schemaVersion",1},{"sections",sections},{"visibility",content.visibility},{"order",content.order},{"featured",content.featured},{"goal",content.goal}};
}
ProfileContent parseContent(const Json& input) {
    ProfileContent content;
    if (input.contains("sections")) for (auto it = input["sections"].begin(); it != input["sections"].end(); ++it) {
        sectionDefinition(it.key()); auto& items = content.sections[it.key()];
        for (const auto& item : it.value()) items.push_back(parseContentItem(it.key(),item,true));
    }
    content.order = input.value("order", std::vector<std::string>{});
    content.visibility = input.value("visibility", std::map<std::string,std::string>{});
    content.featured = input.value("featured", std::vector<FeaturedReference>{});
    content.goal = input.value("goal", "PERSONAL_IDENTITY"); return content;
}
void validateContent(const ProfileContent& content) {
    require(contentJson(content).dump().size() <= 2 * 1024 * 1024, "حجم اطلاعات پروفایل بیش از حد مجاز است.");
    const std::set<std::string> goals = {"PERSONAL_IDENTITY","FIND_JOB","FIND_CLIENTS","PORTFOLIO","RESEARCH_VISIBILITY"};
    require(goals.count(content.goal), "هدف معتبر نیست.");
    std::set<std::string> ids, order;
    std::map<std::string,std::set<std::string>> sectionIds;
    for (const auto& [section, items] : content.sections) {
        require(items.size() <= sectionDefinition(section).limit, "تعداد آیتم‌های بخش بیش از حد مجاز است.");
        for (const auto& item : items) { require(ids.insert(item.id).second, "شناسهٔ تکراری مجاز نیست."); sectionIds[section].insert(item.id); }
    }
    for (const auto& section : content.order) { sectionDefinition(section); require(order.insert(section).second, "ترتیب تکراری معتبر نیست."); }
    for (const auto& [section, visibility] : content.visibility) { sectionDefinition(section); require(visibility == "PUBLIC" || visibility == "HIDDEN", "وضعیت نمایش معتبر نیست."); }
    require(content.featured.size() <= 6, "حداکثر شش مورد برجسته مجاز است.");
    std::set<std::string> featured;
    for (const auto& ref : content.featured) require(sectionIds[ref.section].count(ref.id) && featured.insert(ref.id).second, "مورد برجسته معتبر نیست.");
    for (const auto& [section, items] : content.sections) for (const auto& item : items) {
        auto data = itemJson(item);
        for (auto it = data.begin(); it != data.end(); ++it) {
            auto target = referenceSection(it.key()); if (target.empty()) continue;
            for (const auto& id : it.value()) require(sectionIds[target].count(id.get<std::string>()), "آیتم مرتبط در این پروفایل وجود ندارد.");
        }
    }
}
void removeContentReferences(ProfileContent& content, const std::string&, const std::string&) { prune(content); }
ProfileContent publicContent(const ProfileContent& content) {
    auto result = content;
    std::erase_if(result.sections, [&](const auto& pair) { auto v = result.visibility.find(pair.first); return v != result.visibility.end() && v->second == "HIDDEN"; });
    for (auto& [section, items] : result.sections) {
        std::erase_if(items, [](const auto& item) { return item.visibility != "PUBLIC"; });
        std::stable_sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return std::tie(a.displayOrder,a.id) < std::tie(b.displayOrder,b.id); });
    }
    prune(result); result.goal.clear(); result.visibility.clear(); return result;
}
std::string normalizeProfileTerm(const std::string& input) {
    auto value = input;
    for (const auto& [from,to] : std::vector<std::pair<std::string,std::string>>{{"ي","ی"},{"ك","ک"},{"‌"," "}}) {
        size_t pos = 0; while ((pos = value.find(from,pos)) != std::string::npos) { value.replace(pos,from.size(),to); pos += to.size(); }
    }
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return c < 128 ? std::tolower(c) : c;});
    value = std::regex_replace(value,std::regex("[ \\t\\r\\n]+")," ");
    const auto first = value.find_first_not_of(' '); if (first == std::string::npos) return "";
    return value.substr(first,value.find_last_not_of(' ') - first + 1);
}
std::string profileSearchText(const std::string& text) {
    // MongoDB text tokenization drops punctuation. Encode the meaningful language
    // suffixes in BOTH the indexed text and the query to keep C/C++/C# distinct.
    auto normalized = normalizeProfileTerm(text);
    normalized = std::regex_replace(normalized, std::regex(R"(\bc\+\+(?![a-z0-9+#]))"), "hateflanguagecplusplus");
    return std::regex_replace(normalized, std::regex(R"(\bc#(?![a-z0-9+#]))"), "hateflanguagecsharp");
}
Json contentEditorSchema() {
    Json sections = Json::array();
    for (const auto& def : sectionDefinitions()) sections.push_back({{"key",def.key},{"label",def.label},{"titleField",def.titleField},{"limit",def.limit},{"defaults",def.defaults}});
    return {{"sections",sections},{"enums",enums()}};
}
} // namespace search_engine::profile
