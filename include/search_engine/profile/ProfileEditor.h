#pragma once
#include "../storage/Profile.h"
#include "../common/ProfileSlug.h"
#include <nlohmann/json.hpp>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include <set>

namespace search_engine::profile {
inline std::string hexBytes(const unsigned char* bytes, size_t count) {
    constexpr char alphabet[] = "0123456789abcdef";
    std::string result;
    for (size_t i = 0; i < count; ++i) { result += alphabet[bytes[i] >> 4]; result += alphabet[bytes[i] & 15]; }
    return result;
}
inline std::string newOwnerKey() {
    unsigned char bytes[32];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) throw std::runtime_error("Key generation failed");
    return hexBytes(bytes, sizeof(bytes));
}
inline std::string keyHash(const std::string& key) {
    unsigned char bytes[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(key.data()), key.size(), bytes);
    return hexBytes(bytes, sizeof(bytes));
}
inline bool ownsProfile(const storage::Profile& p, const std::string& key) {
    if (key.empty() || key.size() > 256) return false;
    const auto supplied = p.ownerTokenHash ? keyHash(key) : key;
    const auto expected = p.ownerTokenHash ? p.ownerTokenHash : p.ownerToken;
    return expected && supplied.size() == expected->size() &&
        CRYPTO_memcmp(supplied.data(), expected->data(), supplied.size()) == 0;
}
inline std::vector<uint32_t> codepoints(const std::string& text) {
    std::vector<uint32_t> points;
    for (size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i++]);
        uint32_t point = first;
        int tails = 0;
        if (first >= 0xC2 && first <= 0xDF) { point &= 31; tails = 1; }
        else if (first >= 0xE0 && first <= 0xEF) { point &= 15; tails = 2; }
        else if (first >= 0xF0 && first <= 0xF4) { point &= 7; tails = 3; }
        else if (first >= 0x80) throw std::invalid_argument("متن نامعتبر است.");
        for (int j = 0; j < tails; ++j) {
            if (i == text.size()) throw std::invalid_argument("متن نامعتبر است.");
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xC0) != 0x80) throw std::invalid_argument("متن نامعتبر است.");
            point = (point << 6) | (next & 63);
        }
        if ((tails == 1 && point < 0x80) || (tails == 2 && point < 0x800) ||
            (tails == 3 && point < 0x10000) || point > 0x10FFFF ||
            (point >= 0xD800 && point <= 0xDFFF)) throw std::invalid_argument("متن نامعتبر است.");
        points.push_back(point);
    }
    return points;
}
inline size_t textLength(const std::string& text) { return codepoints(text).size(); }
inline bool persianName(const std::string& name) {
    bool letter = false;
    for (auto point : codepoints(name)) {
        if ((point >= 0x0621 && point <= 0x063A) || (point >= 0x0641 && point <= 0x064A) ||
            point == 0x067E || point == 0x0686 || point == 0x0698 || point == 0x06A9 ||
            point == 0x06AF || point == 0x06CC || point == 0x06C0) { letter = true; continue; }
        if (point == ' ' || point == '-' || point == 0x200C || (point >= 0x064B && point <= 0x0655)) continue;
        return false;
    }
    return letter;
}
inline std::string cookieName(const std::string& id) { return "profile_owner_" + id; }
inline std::string ownerCookie(const std::string& id, const std::string& key, bool secure, bool remove = false) {
    return cookieName(id) + "=" + key + "; Path=/api/profiles/" + id +
        "; HttpOnly; SameSite=Strict; Max-Age=" + (remove ? "0" : "2592000") + (secure ? "; Secure" : "");
}
inline std::string readCookie(std::string_view cookies, const std::string& id) {
    const auto prefix = cookieName(id) + "=";
    while (!cookies.empty()) {
        const auto end = cookies.find(';');
        auto part = cookies.substr(0, end);
        while (!part.empty() && part.front() == ' ') part.remove_prefix(1);
        if (part.substr(0, prefix.size()) == prefix) return std::string(part.substr(prefix.size()));
        if (end == std::string_view::npos) break;
        cookies.remove_prefix(end + 1);
    }
    return "";
}
// Editor patches are intentionally narrow; legacy fields and credentials stay intact.
inline void applyEditorPatch(storage::PersonProfile& p, const nlohmann::json& body) {
    if (!body.is_object()) throw std::invalid_argument("اطلاعات نامعتبر است.");
    const std::set<std::string> allowed = {"version", "name", "title", "tagline", "company", "bio", "location",
        "availabilityStatus", "skillsWithLevel", "avatarUrl", "coverImageUrl", "isPublic"};
    for (auto it = body.begin(); it != body.end(); ++it)
        if (!allowed.count(it.key())) throw std::invalid_argument("فیلد قابل ویرایش نیست: " + it.key());
    auto text = [&](const char* key, size_t limit) {
        if (!body.at(key).is_string()) throw std::invalid_argument("مقدار متنی لازم است.");
        auto value = body.at(key).get<std::string>();
        if (textLength(value) > limit) throw std::invalid_argument("متن بیش از حد طولانی است.");
        return value;
    };
    if (body.contains("name")) {
        auto name = text("name", 200);
        if (!name.empty() && !persianName(name)) throw std::invalid_argument("نام را فقط با حروف فارسی وارد کنید.");
        p.name = name; p.displayName = name;
    }
    if (body.contains("title")) p.title = text("title", 200);
    if (body.contains("tagline")) p.tagline = text("tagline", 120);
    if (body.contains("company")) p.company = text("company", 200);
    if (body.contains("bio")) p.bio = text("bio", 500);
    if (body.contains("location")) p.location = text("location", 200);
    if (body.contains("availabilityStatus")) {
        auto value = text("availabilityStatus", 30);
        if (value != "" && value != "AVAILABLE" && value != "BUSY" && value != "NOT_AVAILABLE")
            throw std::invalid_argument("وضعیت همکاری نامعتبر است.");
        p.availabilityStatus = value;
    }
    for (const auto* key : {"avatarUrl", "coverImageUrl"}) {
        if (body.contains(key)) {
            if (text(key, 0) != "") throw std::invalid_argument("از بخش بارگذاری تصویر استفاده کنید.");
            (std::string(key) == "avatarUrl" ? p.avatarUrl : p.coverImageUrl) = "";
        }
    }
    if (body.contains("skillsWithLevel")) {
        const auto& skills = body["skillsWithLevel"];
        if (!skills.is_array() || skills.size() > 50) throw std::invalid_argument("حداکثر ۵۰ مهارت مجاز است.");
        p.skillsWithLevel.clear(); p.skills.clear();
        for (const auto& skill : skills) {
            if (!skill.is_object() || !skill.contains("name") || !skill["name"].is_string())
                throw std::invalid_argument("مهارت نامعتبر است.");
            const auto name = skill["name"].get<std::string>();
            const auto level = skill.value("level", "BEGINNER");
            if (name.empty() || textLength(name) > 80 || (level != "BEGINNER" && level != "INTERMEDIATE" && level != "EXPERT"))
                throw std::invalid_argument("مهارت نامعتبر است.");
            p.skillsWithLevel.push_back({name, level, "OTHER"});
        }
    }
    if (body.contains("isPublic")) {
        if (!body["isPublic"].is_boolean()) throw std::invalid_argument("وضعیت انتشار نامعتبر است.");
        p.isPublic = body["isPublic"].get<bool>();
    }
    if (p.isPublic && !persianName(p.name)) throw std::invalid_argument("برای انتشار، نام فارسی را وارد کنید.");
}
} // namespace search_engine::profile
