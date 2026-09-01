#pragma once
#include "UserProfile.h"
#include "ProfileStore.h"
#include "../safety/audit/AuditLog.h"
#include <string>
#include <chrono>
#include <ctime>

namespace polaris::profile {

struct ProfileUpdateResult {
    bool success = false;
    std::string field;
    TriState previousValue = TriState::UNKNOWN;
    TriState newValue = TriState::UNKNOWN;
    std::string timestamp;
    std::string auditOperation; // "profile.updated" or "profile.update.idempotent" or "profile.update.rejected.unknown_field"
    std::string reason;
};

class ProfileService {
public:
    // Explicit update by TriState value - caller supplies profile reference and path
    static ProfileUpdateResult updateField(UserProfile& profile, const std::string& field, TriState value, const std::string& path = ProfileStore::profilePath()){
        ProfileUpdateResult r;
        r.field = field;
        r.newValue = value;
        r.timestamp = nowISO();

        // Validate field known
        auto known = UserProfile::knownFields();
        bool isKnown = std::find(known.begin(), known.end(), field)!=known.end();
        if(!isKnown){
            r.success = false;
            r.reason = "Unknown profile field: "+field;
            r.auditOperation = "profile.update.rejected.unknown_field";
            r.previousValue = TriState::UNKNOWN;
            // Audit
            safety::AuditEvent ev{ r.timestamp, "PROFILE", r.auditOperation, "test", "", "", "", "", "", "", r.reason, "", "" };
            safety::AuditLog::append(ev);
            throw std::invalid_argument(r.reason);
        }
        // Validate value is valid TriState (always valid if TriState enum)

        TriState prev = profile.getField(field);
        r.previousValue = prev;

        if(prev == value){
            r.success = true;
            r.reason = "Idempotent: field "+field+" already "+toString(value);
            r.auditOperation = "profile.update.idempotent";
            safety::AuditEvent ev{ r.timestamp, "PROFILE", r.auditOperation, "test", "", "", "", "", "", "", "field="+field+" previous="+toString(prev)+" new="+toString(value)+" applied=false", "", "" };
            safety::AuditLog::append(ev);
            return r;
        }

        // Explicit update - no inference
        profile.setField(field, value);

        // Persist atomically
        try {
            ProfileStore::save(profile, path);
        } catch(...){
            // Revert in-memory on failure
            profile.setField(field, prev);
            throw;
        }

        r.success = true;
        r.reason = "Updated "+field+" from "+toString(prev)+" to "+toString(value);
        r.auditOperation = "profile.updated";
        safety::AuditEvent ev{ r.timestamp, "PROFILE", r.auditOperation, "test", "", "", "", "", "", "", "field="+field+" previous="+toString(prev)+" new="+toString(value)+" applied=true", "", "" };
        safety::AuditLog::append(ev);
        return r;
    }

    static ProfileUpdateResult updateField(UserProfile& profile, const std::string& field, const std::string& valueStr, const std::string& path = ProfileStore::profilePath()){
        TriState v = fromString(valueStr);
        return updateField(profile, field, v, path);
    }

    // Convenience: load, update, save in one call (for CLI) - returns updated profile
    static ProfileUpdateResult updateFieldInStore(const std::string& field, const std::string& valueStr, const std::string& path = ProfileStore::profilePath()){
        UserProfile profile;
        try {
            profile = ProfileStore::load(path);
        } catch(...){
            // If malformed, start from empty unknown
            profile = UserProfile{};
        }
        // If file didn't exist, load returns unknown profile without creating file
        // Now update
        return updateField(profile, field, valueStr, path);
    }

    static bool isIdempotent(const UserProfile& profile, const std::string& field, TriState value){
        try {
            return profile.getField(field) == value;
        } catch(...){
            return false;
        }
    }

private:
    static std::string nowISO(){
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        char buf[64]; strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%S%z", localtime(&tt));
        return buf;
    }
};

} // namespace polaris::profile
