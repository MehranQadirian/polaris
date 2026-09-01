#include "../../core/capabilities/OptimizationRegistry.h"
#include "../../core/capabilities/CapabilityRegistrySetup.h"
#include "../../core/capabilities/FlatpakUnusedCapability.h"
#include "../../core/capabilities/JournalVacuumCapability.h"
#include <cassert>
#include <iostream>
#include <algorithm>

using namespace polaris::capabilities;

void test_registry_registration(){
    auto &reg = OptimizationRegistry::instance();
    reg.clear();
    assert(reg.size()==0);
    bool ok = reg.registerCapability(std::make_unique<FlatpakUnusedCapability>());
    assert(ok);
    assert(reg.size()==1);
    ok = reg.registerCapability(std::make_unique<JournalVacuumCapability>());
    assert(ok);
    assert(reg.size()==2);
    // Lookup
    auto cap = reg.lookup("flatpak-unused");
    assert(cap!=nullptr);
    assert(cap->id()=="flatpak-unused");
    cap = reg.lookup("journal-vacuum");
    assert(cap!=nullptr);
    assert(cap->category()=="Storage");
    cap = reg.lookup("nonexistent");
    assert(cap==nullptr);
    std::cout << "registry registration PASS\n";
}

void test_duplicate_rejection(){
    auto &reg = OptimizationRegistry::instance();
    reg.clear();
    reg.registerCapability(std::make_unique<FlatpakUnusedCapability>());
    bool threw=false;
    try{
        reg.registerCapability(std::make_unique<FlatpakUnusedCapability>());
    } catch(const std::runtime_error& e){
        threw=true;
        assert(std::string(e.what()).find("duplicate")!=std::string::npos);
    }
    assert(threw);
    assert(reg.size()==1);
    std::cout << "duplicate rejection PASS\n";
}

void test_deterministic_ordering(){
    auto &reg = OptimizationRegistry::instance();
    reg.clear();
    // Register in reverse id order
    reg.registerCapability(std::make_unique<JournalVacuumCapability>()); // journal-vacuum
    reg.registerCapability(std::make_unique<FlatpakUnusedCapability>()); // flatpak-unused
    assert(reg.size()==2);
    auto caps = reg.capabilities();
    assert(caps.size()==2);
    // Sorted deterministic: flatpak-unused < journal-vacuum
    assert(caps[0]->id()=="flatpak-unused");
    assert(caps[1]->id()=="journal-vacuum");
    assert(reg.isDeterministic());
    // Second call same order
    auto caps2 = reg.capabilities();
    assert(caps2[0]->id()=="flatpak-unused");
    std::cout << "deterministic ordering PASS\n";
}

void test_ensure_registered(){
    auto &reg = OptimizationRegistry::instance();
    reg.clear();
    ensureCapabilitiesRegistered();
    assert(reg.size()==2);
    // Second call should not duplicate
    ensureCapabilitiesRegistered();
    assert(reg.size()==2);
    auto caps = reg.capabilities();
    assert(caps[0]->id()=="flatpak-unused");
    assert(caps[1]->id()=="journal-vacuum");
    std::cout << "ensure registered PASS\n";
}

void test_capability_ids(){
    auto &reg = OptimizationRegistry::instance();
    reg.clear();
    ensureCapabilitiesRegistered();
    std::vector<std::string> ids;
    for(auto c: reg.capabilities()) ids.push_back(c->id());
    std::sort(ids.begin(), ids.end());
    assert(ids.size()==2);
    assert(ids[0]=="flatpak-unused");
    assert(ids[1]=="journal-vacuum");
    std::cout << "capability ids PASS\n";
}

void test_risk_classification(){
    auto &reg = OptimizationRegistry::instance();
    reg.clear();
    ensureCapabilitiesRegistered();
    auto f = reg.lookup("flatpak-unused");
    auto j = reg.lookup("journal-vacuum");
    assert(f->risk()=="R1");
    assert(j->risk()=="R1");
    assert(f->requiresReboot()==false);
    assert(j->requiresReboot()==false);
    assert(f->requiresAuth()==false);
    assert(j->requiresAuth()==true);
    assert(f->reversibility().find("flatpak install")!=std::string::npos);
    assert(j->reversibility().find("logs")!=std::string::npos || j->reversibility().find("Limited")!=std::string::npos);
    std::cout << "risk classification PASS\n";
}

int main(){
    test_registry_registration();
    test_duplicate_rejection();
    test_deterministic_ordering();
    test_ensure_registered();
    test_capability_ids();
    test_risk_classification();
    // Leave registry populated for next tests (ensure)
    auto &reg = OptimizationRegistry::instance();
    reg.clear();
    ensureCapabilitiesRegistered();
    std::cout << "All P19 registry tests PASS (6 categories)\n";
    return 0;
}
