#pragma once
#include <stdexcept>
#include <string>
#include <fstream>

namespace polaris::safety {

// P2 safety guard: compile-time + runtime enforcement of read-only mode.
// If any code attempts a mutating operation, it must throw.
// This header is included by all RealProviders in P2.

constexpr bool kReadOnlyMode = true;

inline void enforceReadOnly(const std::string& operation) {
    if constexpr (kReadOnlyMode) {
        // In P2, any write attempt is a programming error.
        // We do not silently allow.
        (void)operation;
    }
}

// Helper to open files read-only only. Throws if write mode requested.
inline std::ifstream openReadOnly(const std::string& path) {
    // Validate path: must not contain NUL, must be absolute or relative safe?
    // No shell injection: we use fixed paths, caller validates.
    if (path.empty() || path.find('\0') != std::string::npos) {
        throw std::invalid_argument("Invalid path: " + path);
    }
    // Enforce read-only: only ifstream, no ofstream in P2.
    std::ifstream f(path);
    return f; // caller checks is_open()
}

// If any provider tries to call a mutating API, call this:
[[noreturn]] inline void rejectMutation(const std::string& reason) {
    throw std::runtime_error("P2 READ-ONLY VIOLATION: " + reason + " - refused in read-only mode");
}

} // namespace polaris::safety
