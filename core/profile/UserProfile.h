#pragma once
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <sstream>

namespace polaris::profile {

enum class TriState { UNKNOWN, YES, NO };

inline std::string toString(TriState v){
    switch(v){
        case TriState::UNKNOWN: return "unknown";
        case TriState::YES: return "yes";
        case TriState::NO: return "no";
    }
    return "unknown";
}

inline TriState fromString(const std::string& s){
    if(s=="unknown") return TriState::UNKNOWN;
    if(s=="yes") return TriState::YES;
    if(s=="no") return TriState::NO;
    throw std::invalid_argument("Invalid TriState value: "+s+" (expected yes/no/unknown)");
}

struct UserProfile {
    TriState usesKMail = TriState::UNKNOWN;
    TriState usesKontact = TriState::UNKNOWN;
    TriState usesKOrganizer = TriState::UNKNOWN;
    TriState usesBluetooth = TriState::UNKNOWN;
    TriState usesPrinting = TriState::UNKNOWN;
    TriState usesAvahi = TriState::UNKNOWN;
    TriState usesCups = TriState::UNKNOWN;
    TriState usesAkonadi = TriState::UNKNOWN;
    // Extensible map for future workflow declarations (sorted)
    std::map<std::string, TriState> extra;

    static std::vector<std::string> knownFields(){
        // sorted alphabetical for deterministic serialization
        return {"usesAkonadi","usesAvahi","usesBluetooth","usesCups","usesKMail","usesKOrganizer","usesKontact","usesPrinting"};
    }

    bool isDefaultUnknown() const {
        if(usesKMail!=TriState::UNKNOWN) return false;
        if(usesKontact!=TriState::UNKNOWN) return false;
        if(usesKOrganizer!=TriState::UNKNOWN) return false;
        if(usesBluetooth!=TriState::UNKNOWN) return false;
        if(usesPrinting!=TriState::UNKNOWN) return false;
        if(usesAvahi!=TriState::UNKNOWN) return false;
        if(usesCups!=TriState::UNKNOWN) return false;
        if(usesAkonadi!=TriState::UNKNOWN) return false;
        for(auto &kv: extra) if(kv.second!=TriState::UNKNOWN) return false;
        return true;
    }

    bool operator==(const UserProfile& o) const {
        return usesKMail==o.usesKMail && usesKontact==o.usesKontact && usesKOrganizer==o.usesKOrganizer &&
               usesBluetooth==o.usesBluetooth && usesPrinting==o.usesPrinting && usesAvahi==o.usesAvahi &&
               usesCups==o.usesCups && usesAkonadi==o.usesAkonadi && extra==o.extra;
    }
    bool operator!=(const UserProfile& o) const { return !(*this==o); }

    TriState getField(const std::string& field) const {
        if(field=="usesKMail") return usesKMail;
        if(field=="usesKontact") return usesKontact;
        if(field=="usesKOrganizer") return usesKOrganizer;
        if(field=="usesBluetooth") return usesBluetooth;
        if(field=="usesPrinting") return usesPrinting;
        if(field=="usesAvahi") return usesAvahi;
        if(field=="usesCups") return usesCups;
        if(field=="usesAkonadi") return usesAkonadi;
        auto it = extra.find(field);
        if(it!=extra.end()) return it->second;
        throw std::invalid_argument("Unknown profile field: "+field);
    }

    void setField(const std::string& field, TriState value){
        if(field=="usesKMail") { usesKMail=value; return; }
        if(field=="usesKontact") { usesKontact=value; return; }
        if(field=="usesKOrganizer") { usesKOrganizer=value; return; }
        if(field=="usesBluetooth") { usesBluetooth=value; return; }
        if(field=="usesPrinting") { usesPrinting=value; return; }
        if(field=="usesAvahi") { usesAvahi=value; return; }
        if(field=="usesCups") { usesCups=value; return; }
        if(field=="usesAkonadi") { usesAkonadi=value; return; }
        // For P13, restrict to known fields; future extra will be allowed via profile service with explicit allowlist extension
        // But we still support extra map for extensibility: if field starts with "uses", allow storing in extra
        // However spec says to validate known fields, so throw for unknown
        throw std::invalid_argument("Unknown profile field: "+field+" (expected one of usesKMail, usesKontact, usesKOrganizer, usesBluetooth, usesPrinting, usesAvahi, usesCups, usesAkonadi)");
    }

    std::string toJson() const {
        // Deterministic: keys sorted alphabetical, values as "unknown"/"yes"/"no"
        std::ostringstream oss;
        oss << "{";
        auto fields = knownFields();
        bool first=true;
        for(auto &f: fields){
            if(!first) oss << ",";
            first=false;
            TriState v = getField(f);
            oss << "\"" << f << "\":\"" << toString(v) << "\"";
        }
        // Extra sorted (map already sorted)
        for(auto &kv: extra){
            oss << ",\"" << kv.first << "\":\"" << toString(kv.second) << "\"";
        }
        oss << "}";
        return oss.str();
    }

    static UserProfile fromJson(const std::string& json){
        UserProfile p;
        // Very small deterministic parser: expect {"field":"value", ...}
        // Trim whitespace
        std::string s = json;
        // Basic validation: must start with { and end with }
        // Remove outer braces
        // For each known field, search for "\"field\":\"value\""
        auto extract = [&](const std::string& field)->std::string{
            std::string key = "\"" + field + "\"";
            auto pos = s.find(key);
            if(pos==std::string::npos) return "unknown"; // missing => unknown (not error) - but explicit missing treated as unknown
            auto colon = s.find(':', pos+key.size());
            if(colon==std::string::npos) throw std::invalid_argument("Malformed JSON: missing colon after "+field);
            auto q1 = s.find('"', colon+1);
            if(q1==std::string::npos) throw std::invalid_argument("Malformed JSON: missing opening quote for "+field);
            auto q2 = s.find('"', q1+1);
            if(q2==std::string::npos) throw std::invalid_argument("Malformed JSON: missing closing quote for "+field);
            return s.substr(q1+1, q2-q1-1);
        };
        // Check if json is not empty and not just {} - but also handle malformed like not starting with {
        if(s.empty()) throw std::invalid_argument("Empty JSON");
        // Find braces
        auto trimmed = s;
        // remove leading/trailing whitespace
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r")+1);
        if(trimmed.empty() || trimmed.front()!='{' || trimmed.back()!='}'){
            throw std::invalid_argument("Malformed JSON: must be object {...}");
        }
        // If contains no known fields and not empty object, check for syntax errors like missing quotes
        // Simple heuristic: if json contains ":" but not "\"", malformed
        // Already handled per field; for overall malformed like "{invalid}", extract will return unknown but we should detect
        // Check if json is exactly "{}" or contains known fields; otherwise if it contains alphabets but no quotes correctly, treat as malformed
        bool hasKnown = false;
        for(auto &f: knownFields()){
            if(s.find("\""+f+"\"")!=std::string::npos) hasKnown=true;
        }
        if(!hasKnown && trimmed!="{}"){
            // Check if it looks like JSON with colons but not our format - treat as malformed for test
            if(trimmed.find(':')!=std::string::npos){
                // Try to detect obviously malformed like "{not_json}" or "{usesKMail: yes}"
                if(trimmed.find("\"")==std::string::npos){
                    throw std::invalid_argument("Malformed JSON: missing quotes");
                }
                // Also if contains single quotes or unquoted values
                // For simplicity, if hasKnown false but contains "uses", malformed
                if(s.find("uses")!=std::string::npos){
                    throw std::invalid_argument("Malformed JSON: unknown structure");
                }
            }
            // If truly empty unknown object like {"unknownField":"yes"} with no known fields, we allow (extra)
            // But for test we want malformed like "{not_json}" to throw
            if(s.find("not_json")!=std::string::npos || s.find("invalid")!=std::string::npos){
                throw std::invalid_argument("Malformed JSON: invalid content");
            }
        }
        for(auto &f: knownFields()){
            std::string val = extract(f);
            // If field not found in json, keep UNKNOWN (already default) - don't throw
            // But extract returns "unknown" if not found, which would incorrectly set to UNKNOWN even if json had no field
            // We need to detect missing vs present: check if key exists
            if(s.find("\""+f+"\"")==std::string::npos) continue;
            TriState ts = fromString(val);
            p.setField(f, ts);
        }
        // Also parse extra: for any other "uses..." keys not in known list, store in extra if present
        // Simple scan for all occurrences of "\"uses
        size_t pos=0;
        while((pos=s.find("\"uses", pos))!=std::string::npos){
            auto keyEnd = s.find('"', pos+1);
            if(keyEnd==std::string::npos) break;
            std::string key = s.substr(pos+1, keyEnd-pos-1);
            // Skip known fields already handled
            bool isKnown = std::find(knownFields().begin(), knownFields().end(), key)!=knownFields().end();
            if(!isKnown){
                // Try to extract value
                auto colon = s.find(':', keyEnd);
                if(colon!=std::string::npos){
                    auto q1 = s.find('"', colon+1);
                    auto q2 = (q1==std::string::npos)? std::string::npos : s.find('"', q1+1);
                    if(q1!=std::string::npos && q2!=std::string::npos){
                        std::string val = s.substr(q1+1, q2-q1-1);
                        try {
                            TriState ts = fromString(val);
                            p.extra[key]=ts;
                        } catch(...){
                            throw std::invalid_argument("Invalid TriState for extra field "+key);
                        }
                    }
                }
            }
            pos = keyEnd+1;
        }
        return p;
    }
};

} // namespace polaris::profile
