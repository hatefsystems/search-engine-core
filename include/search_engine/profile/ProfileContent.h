#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <stdexcept>

namespace search_engine::profile {
using Json = nlohmann::json;
struct PartialDate { std::string calendar = "persian"; int year = 0, month = 0, day = 0; };
inline void to_json(Json& json, const PartialDate& value) {
    json = Json{{"calendar", value.calendar}, {"year", value.year}, {"month", value.month}, {"day", value.day}};
}
inline void from_json(const Json& json, PartialDate& value) {
    value = PartialDate{};
    if (json.contains("calendar")) json.at("calendar").get_to(value.calendar);
    if (json.contains("year")) json.at("year").get_to(value.year);
    if (json.contains("month")) json.at("month").get_to(value.month);
    if (json.contains("day")) json.at("day").get_to(value.day);
}
struct ExternalReference { std::string title, url; };
inline void to_json(Json& json, const ExternalReference& value) {
    json = Json{{"title", value.title}, {"url", value.url}};
}
inline void from_json(const Json& json, ExternalReference& value) {
    value = ExternalReference{};
    if (json.contains("title")) json.at("title").get_to(value.title);
    if (json.contains("url")) json.at("url").get_to(value.url);
}
struct MediaReference { std::string id, alt; };
inline void to_json(Json& json, const MediaReference& value) {
    json = Json{{"id", value.id}, {"alt", value.alt}};
}
inline void from_json(const Json& json, MediaReference& value) {
    value = MediaReference{};
    if (json.contains("id")) json.at("id").get_to(value.id);
    if (json.contains("alt")) json.at("alt").get_to(value.alt);
}
struct Evidence { std::string id, type, title, url, description; std::string verificationStatus = "SELF_REPORTED"; };
inline void to_json(Json& json, const Evidence& value) {
    json = Json{{"id", value.id}, {"type", value.type}, {"title", value.title}, {"url", value.url}, {"description", value.description}, {"verificationStatus", value.verificationStatus}};
}
inline void from_json(const Json& json, Evidence& value) {
    value = Evidence{};
    if (json.contains("id")) json.at("id").get_to(value.id);
    if (json.contains("type")) json.at("type").get_to(value.type);
    if (json.contains("title")) json.at("title").get_to(value.title);
    if (json.contains("url")) json.at("url").get_to(value.url);
    if (json.contains("description")) json.at("description").get_to(value.description);
    if (json.contains("verificationStatus")) json.at("verificationStatus").get_to(value.verificationStatus);
}
struct Experience {
    std::string roleTitle;
    std::string organizationName;
    std::string organizationProfileId;
    std::string employmentType;
    std::string location;
    std::string locationType;
    PartialDate startDate;
    PartialDate endDate;
    bool isCurrent = false;
    std::string summary;
    std::vector<std::string> responsibilities;
    std::vector<std::string> achievements;
    std::vector<std::string> technologies;
    std::vector<std::string> skillIds;
    std::vector<std::string> projectIds;
    std::vector<MediaReference> media;
};
inline void to_json(Json& json, const Experience& value) {
    json = Json{{"roleTitle", value.roleTitle}, {"organizationName", value.organizationName}, {"organizationProfileId", value.organizationProfileId}, {"employmentType", value.employmentType}, {"location", value.location}, {"locationType", value.locationType}, {"startDate", value.startDate}, {"endDate", value.endDate}, {"isCurrent", value.isCurrent}, {"summary", value.summary}, {"responsibilities", value.responsibilities}, {"achievements", value.achievements}, {"technologies", value.technologies}, {"skillIds", value.skillIds}, {"projectIds", value.projectIds}};
    json["media"] = value.media;
}
inline void from_json(const Json& json, Experience& value) {
    value = Experience{};
    if (json.contains("roleTitle")) json.at("roleTitle").get_to(value.roleTitle);
    if (json.contains("organizationName")) json.at("organizationName").get_to(value.organizationName);
    if (json.contains("organizationProfileId")) json.at("organizationProfileId").get_to(value.organizationProfileId);
    if (json.contains("employmentType")) json.at("employmentType").get_to(value.employmentType);
    if (json.contains("location")) json.at("location").get_to(value.location);
    if (json.contains("locationType")) json.at("locationType").get_to(value.locationType);
    if (json.contains("startDate")) json.at("startDate").get_to(value.startDate);
    if (json.contains("endDate")) json.at("endDate").get_to(value.endDate);
    if (json.contains("isCurrent")) json.at("isCurrent").get_to(value.isCurrent);
    if (json.contains("summary")) json.at("summary").get_to(value.summary);
    if (json.contains("responsibilities")) json.at("responsibilities").get_to(value.responsibilities);
    if (json.contains("achievements")) json.at("achievements").get_to(value.achievements);
    if (json.contains("technologies")) json.at("technologies").get_to(value.technologies);
    if (json.contains("skillIds")) json.at("skillIds").get_to(value.skillIds);
    if (json.contains("projectIds")) json.at("projectIds").get_to(value.projectIds);
    if (json.contains("media")) json.at("media").get_to(value.media);
}
struct Project {
    std::string title;
    std::string subtitle;
    std::string description;
    std::string role;
    std::string organization;
    PartialDate startDate;
    PartialDate endDate;
    bool isOngoing = false;
    std::string projectType;
    std::string problem;
    std::string solution;
    std::string architecture;
    std::vector<std::string> responsibilities;
    std::vector<std::string> challenges;
    std::vector<std::string> outcomes;
    std::vector<std::string> technologies;
    std::vector<std::string> skillIds;
    std::vector<std::string> experienceIds;
    std::vector<std::string> collaborators;
    std::vector<ExternalReference> links;
    std::vector<MediaReference> media;
};
inline void to_json(Json& json, const Project& value) {
    json = Json{{"title", value.title}, {"subtitle", value.subtitle}, {"description", value.description}, {"role", value.role}, {"organization", value.organization}, {"startDate", value.startDate}, {"endDate", value.endDate}, {"isOngoing", value.isOngoing}, {"projectType", value.projectType}, {"problem", value.problem}, {"solution", value.solution}, {"architecture", value.architecture}, {"responsibilities", value.responsibilities}, {"challenges", value.challenges}, {"outcomes", value.outcomes}, {"technologies", value.technologies}, {"skillIds", value.skillIds}, {"experienceIds", value.experienceIds}, {"collaborators", value.collaborators}, {"links", value.links}, {"media", value.media}};
}
inline void from_json(const Json& json, Project& value) {
    value = Project{};
    if (json.contains("title")) json.at("title").get_to(value.title);
    if (json.contains("subtitle")) json.at("subtitle").get_to(value.subtitle);
    if (json.contains("description")) json.at("description").get_to(value.description);
    if (json.contains("role")) json.at("role").get_to(value.role);
    if (json.contains("organization")) json.at("organization").get_to(value.organization);
    if (json.contains("startDate")) json.at("startDate").get_to(value.startDate);
    if (json.contains("endDate")) json.at("endDate").get_to(value.endDate);
    if (json.contains("isOngoing")) json.at("isOngoing").get_to(value.isOngoing);
    if (json.contains("projectType")) json.at("projectType").get_to(value.projectType);
    if (json.contains("problem")) json.at("problem").get_to(value.problem);
    if (json.contains("solution")) json.at("solution").get_to(value.solution);
    if (json.contains("architecture")) json.at("architecture").get_to(value.architecture);
    if (json.contains("responsibilities")) json.at("responsibilities").get_to(value.responsibilities);
    if (json.contains("challenges")) json.at("challenges").get_to(value.challenges);
    if (json.contains("outcomes")) json.at("outcomes").get_to(value.outcomes);
    if (json.contains("technologies")) json.at("technologies").get_to(value.technologies);
    if (json.contains("skillIds")) json.at("skillIds").get_to(value.skillIds);
    if (json.contains("experienceIds")) json.at("experienceIds").get_to(value.experienceIds);
    if (json.contains("collaborators")) json.at("collaborators").get_to(value.collaborators);
    if (json.contains("links")) json.at("links").get_to(value.links);
    if (json.contains("media")) json.at("media").get_to(value.media);
}
struct AdvancedSkill {
    std::string name;
    std::string category;
    std::string proficiencyLevel;
    double yearsOfExperience = 0;
    int firstUsedYear = 0;
    int lastUsedYear = 0;
    bool isCurrentlyUsing = false;
    std::string description;
    std::vector<std::string> projectIds;
    std::vector<std::string> experienceIds;
    std::vector<std::string> certificationIds;
    std::vector<MediaReference> media;
};
inline void to_json(Json& json, const AdvancedSkill& value) {
    json = Json{{"certificationIds", value.certificationIds}, {"name", value.name}, {"category", value.category}, {"proficiencyLevel", value.proficiencyLevel}, {"yearsOfExperience", value.yearsOfExperience}, {"firstUsedYear", value.firstUsedYear}, {"lastUsedYear", value.lastUsedYear}, {"isCurrentlyUsing", value.isCurrentlyUsing}, {"description", value.description}, {"projectIds", value.projectIds}, {"experienceIds", value.experienceIds}};
    json["media"] = value.media;
}
inline void from_json(const Json& json, AdvancedSkill& value) {
    value = AdvancedSkill{};
    if (json.contains("certificationIds")) json.at("certificationIds").get_to(value.certificationIds);
    if (json.contains("name")) json.at("name").get_to(value.name);
    if (json.contains("category")) json.at("category").get_to(value.category);
    if (json.contains("proficiencyLevel")) json.at("proficiencyLevel").get_to(value.proficiencyLevel);
    if (json.contains("yearsOfExperience")) json.at("yearsOfExperience").get_to(value.yearsOfExperience);
    if (json.contains("firstUsedYear")) json.at("firstUsedYear").get_to(value.firstUsedYear);
    if (json.contains("lastUsedYear")) json.at("lastUsedYear").get_to(value.lastUsedYear);
    if (json.contains("isCurrentlyUsing")) json.at("isCurrentlyUsing").get_to(value.isCurrentlyUsing);
    if (json.contains("description")) json.at("description").get_to(value.description);
    if (json.contains("projectIds")) json.at("projectIds").get_to(value.projectIds);
    if (json.contains("experienceIds")) json.at("experienceIds").get_to(value.experienceIds);
    if (json.contains("media")) json.at("media").get_to(value.media);
}
struct Education {
    std::string kind;
    std::string institutionName;
    std::string institutionProfileId;
    std::string degree;
    std::string fieldOfStudy;
    PartialDate startDate;
    PartialDate endDate;
    bool isCurrent = false;
    std::string grade;
    std::string description;
    std::vector<std::string> activities;
    std::vector<std::string> achievements;
};
inline void to_json(Json& json, const Education& value) {
    json = Json{{"kind", value.kind}, {"institutionName", value.institutionName}, {"institutionProfileId", value.institutionProfileId}, {"degree", value.degree}, {"fieldOfStudy", value.fieldOfStudy}, {"startDate", value.startDate}, {"endDate", value.endDate}, {"isCurrent", value.isCurrent}, {"grade", value.grade}, {"description", value.description}, {"activities", value.activities}, {"achievements", value.achievements}};
}
inline void from_json(const Json& json, Education& value) {
    value = Education{};
    if (json.contains("kind")) json.at("kind").get_to(value.kind);
    if (json.contains("institutionName")) json.at("institutionName").get_to(value.institutionName);
    if (json.contains("institutionProfileId")) json.at("institutionProfileId").get_to(value.institutionProfileId);
    if (json.contains("degree")) json.at("degree").get_to(value.degree);
    if (json.contains("fieldOfStudy")) json.at("fieldOfStudy").get_to(value.fieldOfStudy);
    if (json.contains("startDate")) json.at("startDate").get_to(value.startDate);
    if (json.contains("endDate")) json.at("endDate").get_to(value.endDate);
    if (json.contains("isCurrent")) json.at("isCurrent").get_to(value.isCurrent);
    if (json.contains("grade")) json.at("grade").get_to(value.grade);
    if (json.contains("description")) json.at("description").get_to(value.description);
    if (json.contains("activities")) json.at("activities").get_to(value.activities);
    if (json.contains("achievements")) json.at("achievements").get_to(value.achievements);
}
struct Certification {
    std::string name;
    std::string issuingOrganization;
    PartialDate issueDate;
    PartialDate expirationDate;
    std::string credentialId;
    std::string credentialUrl;
    std::vector<std::string> skillIds;
};
inline void to_json(Json& json, const Certification& value) {
    json = Json{{"name", value.name}, {"issuingOrganization", value.issuingOrganization}, {"issueDate", value.issueDate}, {"expirationDate", value.expirationDate}, {"credentialId", value.credentialId}, {"credentialUrl", value.credentialUrl}, {"skillIds", value.skillIds}};
}
inline void from_json(const Json& json, Certification& value) {
    value = Certification{};
    if (json.contains("name")) json.at("name").get_to(value.name);
    if (json.contains("issuingOrganization")) json.at("issuingOrganization").get_to(value.issuingOrganization);
    if (json.contains("issueDate")) json.at("issueDate").get_to(value.issueDate);
    if (json.contains("expirationDate")) json.at("expirationDate").get_to(value.expirationDate);
    if (json.contains("credentialId")) json.at("credentialId").get_to(value.credentialId);
    if (json.contains("credentialUrl")) json.at("credentialUrl").get_to(value.credentialUrl);
    if (json.contains("skillIds")) json.at("skillIds").get_to(value.skillIds);
}
struct Publication {
    std::string type;
    std::string title;
    std::string description;
    std::string publisher;
    PartialDate publicationDate;
    std::vector<std::string> authors;
    std::string url;
    std::string doi;
    std::string isbn;
    std::vector<std::string> skillIds;
    std::vector<std::string> topics;
};
inline void to_json(Json& json, const Publication& value) {
    json = Json{{"type", value.type}, {"title", value.title}, {"description", value.description}, {"publisher", value.publisher}, {"publicationDate", value.publicationDate}, {"authors", value.authors}, {"url", value.url}, {"doi", value.doi}, {"isbn", value.isbn}, {"skillIds", value.skillIds}, {"topics", value.topics}};
}
inline void from_json(const Json& json, Publication& value) {
    value = Publication{};
    if (json.contains("type")) json.at("type").get_to(value.type);
    if (json.contains("title")) json.at("title").get_to(value.title);
    if (json.contains("description")) json.at("description").get_to(value.description);
    if (json.contains("publisher")) json.at("publisher").get_to(value.publisher);
    if (json.contains("publicationDate")) json.at("publicationDate").get_to(value.publicationDate);
    if (json.contains("authors")) json.at("authors").get_to(value.authors);
    if (json.contains("url")) json.at("url").get_to(value.url);
    if (json.contains("doi")) json.at("doi").get_to(value.doi);
    if (json.contains("isbn")) json.at("isbn").get_to(value.isbn);
    if (json.contains("skillIds")) json.at("skillIds").get_to(value.skillIds);
    if (json.contains("topics")) json.at("topics").get_to(value.topics);
}
struct OpenSourceContribution {
    std::string platform;
    std::string repositoryName;
    std::string repositoryUrl;
    std::string contributionType;
    std::string role;
    std::string description;
    std::vector<std::string> technologies;
    std::vector<std::string> skillIds;
    PartialDate startDate;
    PartialDate endDate;
    std::vector<std::string> collaborators;
};
inline void to_json(Json& json, const OpenSourceContribution& value) {
    json = Json{{"collaborators", value.collaborators}, {"platform", value.platform}, {"repositoryName", value.repositoryName}, {"repositoryUrl", value.repositoryUrl}, {"contributionType", value.contributionType}, {"role", value.role}, {"description", value.description}, {"technologies", value.technologies}, {"skillIds", value.skillIds}, {"startDate", value.startDate}, {"endDate", value.endDate}};
}
inline void from_json(const Json& json, OpenSourceContribution& value) {
    value = OpenSourceContribution{};
    if (json.contains("collaborators")) json.at("collaborators").get_to(value.collaborators);
    if (json.contains("platform")) json.at("platform").get_to(value.platform);
    if (json.contains("repositoryName")) json.at("repositoryName").get_to(value.repositoryName);
    if (json.contains("repositoryUrl")) json.at("repositoryUrl").get_to(value.repositoryUrl);
    if (json.contains("contributionType")) json.at("contributionType").get_to(value.contributionType);
    if (json.contains("role")) json.at("role").get_to(value.role);
    if (json.contains("description")) json.at("description").get_to(value.description);
    if (json.contains("technologies")) json.at("technologies").get_to(value.technologies);
    if (json.contains("skillIds")) json.at("skillIds").get_to(value.skillIds);
    if (json.contains("startDate")) json.at("startDate").get_to(value.startDate);
    if (json.contains("endDate")) json.at("endDate").get_to(value.endDate);
}
struct Service {
    std::string title;
    std::string description;
    std::string category;
    std::string deliveryMode;
    std::string location;
    std::string pricingMode;
    std::string price;
    std::string currency;
    std::string availability;
    std::string contactMethod;
    std::vector<std::string> projectIds;
    std::vector<std::string> skillIds;
    std::vector<MediaReference> media;
};
inline void to_json(Json& json, const Service& value) {
    json = Json{{"title", value.title}, {"description", value.description}, {"category", value.category}, {"deliveryMode", value.deliveryMode}, {"location", value.location}, {"pricingMode", value.pricingMode}, {"price", value.price}, {"currency", value.currency}, {"availability", value.availability}, {"contactMethod", value.contactMethod}, {"projectIds", value.projectIds}, {"skillIds", value.skillIds}};
    json["media"] = value.media;
}
inline void from_json(const Json& json, Service& value) {
    value = Service{};
    if (json.contains("title")) json.at("title").get_to(value.title);
    if (json.contains("description")) json.at("description").get_to(value.description);
    if (json.contains("category")) json.at("category").get_to(value.category);
    if (json.contains("deliveryMode")) json.at("deliveryMode").get_to(value.deliveryMode);
    if (json.contains("location")) json.at("location").get_to(value.location);
    if (json.contains("pricingMode")) json.at("pricingMode").get_to(value.pricingMode);
    if (json.contains("price")) json.at("price").get_to(value.price);
    if (json.contains("currency")) json.at("currency").get_to(value.currency);
    if (json.contains("availability")) json.at("availability").get_to(value.availability);
    if (json.contains("contactMethod")) json.at("contactMethod").get_to(value.contactMethod);
    if (json.contains("projectIds")) json.at("projectIds").get_to(value.projectIds);
    if (json.contains("skillIds")) json.at("skillIds").get_to(value.skillIds);
    if (json.contains("media")) json.at("media").get_to(value.media);
}
struct Achievement {
    std::string type;
    std::string title;
    std::string issuer;
    PartialDate date;
    std::string description;
    std::vector<MediaReference> media;
};
inline void to_json(Json& json, const Achievement& value) {
    json = Json{{"type", value.type}, {"title", value.title}, {"issuer", value.issuer}, {"date", value.date}, {"description", value.description}};
    json["media"] = value.media;
}
inline void from_json(const Json& json, Achievement& value) {
    value = Achievement{};
    if (json.contains("type")) json.at("type").get_to(value.type);
    if (json.contains("title")) json.at("title").get_to(value.title);
    if (json.contains("issuer")) json.at("issuer").get_to(value.issuer);
    if (json.contains("date")) json.at("date").get_to(value.date);
    if (json.contains("description")) json.at("description").get_to(value.description);
    if (json.contains("media")) json.at("media").get_to(value.media);
}
struct Language {
    std::string name;
    std::string proficiency;
};
inline void to_json(Json& json, const Language& value) {
    json = Json{{"name", value.name}, {"proficiency", value.proficiency}};
}
inline void from_json(const Json& json, Language& value) {
    value = Language{};
    if (json.contains("name")) json.at("name").get_to(value.name);
    if (json.contains("proficiency")) json.at("proficiency").get_to(value.proficiency);
}
struct Recommendation {
    std::string authorName;
    std::string authorTitle;
    std::string relationship;
    std::string content;
    std::string sourceUrl;
};
inline void to_json(Json& json, const Recommendation& value) {
    json = Json{{"authorName", value.authorName}, {"authorTitle", value.authorTitle}, {"relationship", value.relationship}, {"content", value.content}, {"sourceUrl", value.sourceUrl}};
}
inline void from_json(const Json& json, Recommendation& value) {
    value = Recommendation{};
    if (json.contains("authorName")) json.at("authorName").get_to(value.authorName);
    if (json.contains("authorTitle")) json.at("authorTitle").get_to(value.authorTitle);
    if (json.contains("relationship")) json.at("relationship").get_to(value.relationship);
    if (json.contains("content")) json.at("content").get_to(value.content);
    if (json.contains("sourceUrl")) json.at("sourceUrl").get_to(value.sourceUrl);
}
struct Contact {
    std::string type;
    std::string label;
    std::string value;
};
inline void to_json(Json& json, const Contact& value) {
    json = Json{{"type", value.type}, {"label", value.label}, {"value", value.value}};
}
inline void from_json(const Json& json, Contact& value) {
    value = Contact{};
    if (json.contains("type")) json.at("type").get_to(value.type);
    if (json.contains("label")) json.at("label").get_to(value.label);
    if (json.contains("value")) json.at("value").get_to(value.value);
}
struct Availability {
    std::string type;
    std::string status;
    std::string description;
};
inline void to_json(Json& json, const Availability& value) {
    json = Json{{"type", value.type}, {"status", value.status}, {"description", value.description}};
}
inline void from_json(const Json& json, Availability& value) {
    value = Availability{};
    if (json.contains("type")) json.at("type").get_to(value.type);
    if (json.contains("status")) json.at("status").get_to(value.status);
    if (json.contains("description")) json.at("description").get_to(value.description);
}
struct About {
    std::string title;
    std::string description;
};
inline void to_json(Json& json, const About& value) {
    json = Json{{"title", value.title}, {"description", value.description}};
}
inline void from_json(const Json& json, About& value) {
    value = About{};
    if (json.contains("title")) json.at("title").get_to(value.title);
    if (json.contains("description")) json.at("description").get_to(value.description);
}
using DomainItem = std::variant<Experience, Project, AdvancedSkill, Education, Certification, Publication, OpenSourceContribution, Service, Achievement, Language, Recommendation, Contact, Availability, About>;
struct ContentItem {
    std::string id, createdAt, updatedAt;
    std::string iconMode = "none", iconId, iconMediaId;
    std::string visibility = "HIDDEN";
    int displayOrder = 0;
    std::vector<Evidence> evidence;
    DomainItem value;
};
inline std::vector<MediaReference>& itemMedia(ContentItem& item) {
    return std::visit([](auto& value) -> std::vector<MediaReference>& {
        if constexpr (requires { value.media; }) return value.media;
        else throw std::invalid_argument("This section has no media");
    }, item.value);
}
inline const std::vector<MediaReference>& itemMedia(const ContentItem& item) {
    return std::visit([](const auto& value) -> const std::vector<MediaReference>& {
        if constexpr (requires { value.media; }) return value.media;
        else throw std::invalid_argument("This section has no media");
    }, item.value);
}
struct FeaturedReference { std::string section, id; };
inline void to_json(Json& json, const FeaturedReference& value) {
    json = Json{{"section", value.section}, {"id", value.id}};
}
inline void from_json(const Json& json, FeaturedReference& value) {
    value = FeaturedReference{};
    if (json.contains("section")) json.at("section").get_to(value.section);
    if (json.contains("id")) json.at("id").get_to(value.id);
}
struct ProfileContent {
    // An absent section uses legacy fallback; an initialized empty section does not.
    std::map<std::string, std::vector<ContentItem>> sections;
    std::map<std::string, std::string> visibility;
    std::vector<std::string> order;
    std::vector<FeaturedReference> featured;
    std::string goal = "PERSONAL_IDENTITY";
};
struct SectionDefinition { std::string key, label, titleField; size_t limit; Json defaults; };
const std::vector<SectionDefinition>& sectionDefinitions();
const SectionDefinition& sectionDefinition(const std::string& section);
Json itemJson(const ContentItem& item);
ContentItem parseContentItem(const std::string& section, const Json& input, bool fromStorage = false);
Json contentJson(const ProfileContent& content);
ProfileContent parseContent(const Json& input);
void validateContent(const ProfileContent& content);
void removeContentReferences(ProfileContent& content, const std::string& section, const std::string& id);
ProfileContent publicContent(const ProfileContent& content);
bool safeContentUrl(const std::string& url);
bool validDraftUrl(const std::string& url);
std::string contentNow();
std::string normalizeProfileTerm(const std::string& text);
std::string profileSearchText(const std::string& text);
Json contentEditorSchema();
} // namespace search_engine::profile
