#include "CapabilityRegistrySetup.h"
#include "OptimizationRegistry.h"
#include "FlatpakUnusedCapability.h"
#include "JournalVacuumCapability.h"

namespace polaris::capabilities {

void ensureCapabilitiesRegistered() {
    auto &reg = OptimizationRegistry::instance();
    if(reg.size()!=0) return;
    // Register reference capabilities deterministically by id order
    // Register in id order to keep deterministic: flatpak-unused < journal-vacuum
    try { reg.registerCapability(std::make_unique<FlatpakUnusedCapability>()); } catch(...){}
    try { reg.registerCapability(std::make_unique<JournalVacuumCapability>()); } catch(...){}
}

} // namespace polaris::capabilities
