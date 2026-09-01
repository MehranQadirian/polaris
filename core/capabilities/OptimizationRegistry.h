#pragma once
#include "IOptimizationCapability.h"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <optional>
#include <algorithm>
#include <stdexcept>

namespace polaris::capabilities {

class OptimizationRegistry {
public:
    static OptimizationRegistry& instance() {
        static OptimizationRegistry reg;
        return reg;
    }

    // Register capability; reject duplicate id deterministically. Returns true if registered, throws on duplicate.
    bool registerCapability(std::unique_ptr<IOptimizationCapability> cap) {
        if(!cap) throw std::invalid_argument("null capability");
        std::string cid = cap->id();
        if(cid.empty()) throw std::invalid_argument("empty capability id");
        if(cid.find('\0')!=std::string::npos) throw std::invalid_argument("NUL in capability id");
        if(cid.find("..")!=std::string::npos) throw std::invalid_argument("traversal in capability id");
        if(cid.size()>256) throw std::invalid_argument("capability id too long");
        for(auto &c: caps_) if(c->id()==cid) throw std::runtime_error("duplicate capability id: "+cid);
        caps_.push_back(std::move(cap));
        // Keep deterministic ordering by id
        std::sort(caps_.begin(), caps_.end(), [](const std::unique_ptr<IOptimizationCapability>& a, const std::unique_ptr<IOptimizationCapability>& b){
            return a->id() < b->id();
        });
        return true;
    }

    std::vector<const IOptimizationCapability*> capabilities() const {
        std::vector<const IOptimizationCapability*> out;
        out.reserve(caps_.size());
        for(auto &c: caps_) out.push_back(c.get());
        return out;
    }

    const IOptimizationCapability* lookup(const std::string& cid) const {
        for(auto &c: caps_) if(c->id()==cid) return c.get();
        return nullptr;
    }

    size_t size() const { return caps_.size(); }

    // For tests: clear all (only allowed for isolated test roots, not real host)
    void clear() { caps_.clear(); }

    // Deterministic ordering check: ids sorted
    bool isDeterministic() const {
        for(size_t i=1;i<caps_.size();i++) if(caps_[i-1]->id() > caps_[i]->id()) return false;
        return true;
    }

    // Prevent copy
    OptimizationRegistry(const OptimizationRegistry&) = delete;
    OptimizationRegistry& operator=(const OptimizationRegistry&) = delete;

private:
    OptimizationRegistry() = default;
    std::vector<std::unique_ptr<IOptimizationCapability>> caps_;
};

} // namespace polaris::capabilities
