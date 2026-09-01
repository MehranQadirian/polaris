cmake_minimum_required(VERSION 3.28)
project(polaris VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-error=deprecated-declarations)

# Core library (no Qt)
add_library(polaris_core STATIC
    core/engines/diagnostics/DiagnosticsEngine.cpp
    core/safety/transaction/TransactionManager.cpp
    core/safety/transaction/TransactionValidator.cpp
    core/safety/transaction/TransactionStore.cpp
    core/safety/backup/BackupEngine.cpp
    core/safety/audit/AuditLog.cpp
    core/providers/real/RealGpuProvider.cpp
    core/providers/real/RealSystemdProvider.cpp
    core/providers/real/RealJournalProvider.cpp
    core/engines/perf/BaselineEngine.cpp
    core/engines/bottleneck/BottleneckEngine.cpp
    core/engines/benchmark/BenchmarkEngine.cpp
    core/engines/recommend/RecommendationEngine.cpp
    core/engines/comparison/ComparisonEngine.cpp
    core/profile/UserProfile.cpp
    core/profile/ProfileStore.cpp
    core/profile/ProfileService.cpp
    core/profile/ProfileAdvisor.cpp
    core/ipc/IpcProtocol.cpp
    core/ipc/IpcAuth.cpp
    core/ipc/IpcServer.cpp
    core/ipc/IpcClient.cpp
    core/safety/lock/TransactionLock.cpp
    core/safety/recovery/RecoveryDetector.cpp
    core/explainability/Explanation.cpp
    core/explainability/ExplanationEngine.cpp
    core/capabilities/CapabilityRegistrySetup.cpp
)
target_link_libraries(polaris_core PUBLIC crypto)
target_include_directories(polaris_core PUBLIC core)
# No Qt dependency for core - header-only nlohmann/json via FetchContent optional

# CLI (thin API client, headless capable) - mock scaffold
add_executable(polaris cli/main.cpp)
target_link_libraries(polaris PRIVATE polaris_core)

# Real read-only scan binary (P2, no sudo, no writes)
add_executable(polaris_real cli/real_scan.cpp)
target_link_libraries(polaris_real PRIVATE polaris_core)

# Tests
enable_testing()
add_executable(polaris_tests tests/unit/test_health.cpp)
target_link_libraries(polaris_tests PRIVATE polaris_core)
add_executable(test_real_providers tests/unit/test_real_providers.cpp)
target_link_libraries(test_real_providers PRIVATE polaris_core)
add_test(NAME unit COMMAND polaris_tests)
add_test(NAME real_providers COMMAND test_real_providers)
add_executable(test_parsers tests/unit/test_parsers.cpp)
target_link_libraries(test_parsers PRIVATE polaris_core)
add_test(NAME parsers COMMAND test_parsers)
add_executable(test_readonly tests/integration/test_readonly.cpp)
target_link_libraries(test_readonly PRIVATE polaris_core)
add_test(NAME readonly COMMAND test_readonly)

# Packaging helper
install(TARGETS polaris DESTINATION bin)
install(FILES README.md ARCHITECTURE.md API.md SECURITY.md DESTINATION share/doc/polaris)

# P3 CLI
add_executable(polaris_p3 cli/p3_cli.cpp)
target_link_libraries(polaris_p3 PRIVATE polaris_core)

# Tests for P3
add_executable(test_baseline tests/unit/test_parsers.cpp) # reuse parsers already

# P4 CLI
add_executable(polaris_p4 cli/p4_cli.cpp)
target_link_libraries(polaris_p4 PRIVATE polaris_core crypto)
add_executable(test_p4_security tests/security/test_p4_security.cpp)
target_link_libraries(test_p4_security PRIVATE polaris_core crypto)
add_test(NAME p4_security COMMAND test_p4_security)

# P5 Pilot
add_executable(polaris_p5 cli/p5_pilot.cpp)
target_link_libraries(polaris_p5 PRIVATE polaris_core crypto)

# P11 Tests
add_executable(test_comparison tests/unit/test_comparison.cpp)
target_link_libraries(test_comparison PRIVATE polaris_core crypto)
add_test(NAME comparison COMMAND test_comparison)
add_executable(test_post_change tests/integration/test_post_change.cpp)
target_link_libraries(test_post_change PRIVATE polaris_core crypto)
add_test(NAME post_change COMMAND test_post_change)
add_executable(test_regression tests/integration/test_regression.cpp)
target_link_libraries(test_regression PRIVATE polaris_core crypto)
add_test(NAME regression COMMAND test_regression)
add_executable(test_observed_benefit tests/integration/test_observed_benefit.cpp)
target_link_libraries(test_observed_benefit PRIVATE polaris_core crypto)
add_test(NAME observed_benefit COMMAND test_observed_benefit)

# P12 Tests - Stale-Preview + Idempotency + StateMachine + Audit/Compat
add_executable(test_p12_stale tests/security/test_p12_stale.cpp)
target_link_libraries(test_p12_stale PRIVATE polaris_core crypto)
add_test(NAME p12_stale COMMAND test_p12_stale)
add_executable(test_p12_idempotency tests/security/test_p12_idempotency.cpp)
target_link_libraries(test_p12_idempotency PRIVATE polaris_core crypto)
add_test(NAME p12_idempotency COMMAND test_p12_idempotency)
add_executable(test_p12_statemachine tests/security/test_p12_statemachine.cpp)
target_link_libraries(test_p12_statemachine PRIVATE polaris_core crypto)
add_test(NAME p12_statemachine COMMAND test_p12_statemachine)
add_executable(test_p12_transaction_model tests/unit/test_p12_transaction_model.cpp)
target_link_libraries(test_p12_transaction_model PRIVATE polaris_core crypto)
add_test(NAME p12_transaction_model COMMAND test_p12_transaction_model)

# P13 Tests - User Workflow / Profile Engine
add_executable(test_p13_profile_model tests/unit/test_p13_profile_model.cpp)
target_link_libraries(test_p13_profile_model PRIVATE polaris_core crypto)
add_test(NAME p13_profile_model COMMAND test_p13_profile_model)
add_executable(test_p13_profile_store tests/unit/test_p13_profile_store.cpp)
target_link_libraries(test_p13_profile_store PRIVATE polaris_core crypto)
add_test(NAME p13_profile_store COMMAND test_p13_profile_store)
add_executable(test_p13_profile_service tests/security/test_p13_profile_service.cpp)
target_link_libraries(test_p13_profile_service PRIVATE polaris_core crypto)
add_test(NAME p13_profile_service COMMAND test_p13_profile_service)
add_executable(test_p13_profile_advisor tests/unit/test_p13_profile_advisor.cpp)
target_link_libraries(test_p13_profile_advisor PRIVATE polaris_core crypto)
add_test(NAME p13_profile_advisor COMMAND test_p13_profile_advisor)

# P14 Tests - IPC / Helper / Lock / Recovery
add_executable(test_p14_ipc_protocol tests/unit/test_p14_ipc_protocol.cpp)
target_link_libraries(test_p14_ipc_protocol PRIVATE polaris_core crypto)
add_test(NAME p14_ipc_protocol COMMAND test_p14_ipc_protocol)
add_executable(test_p14_ipc_auth tests/security/test_p14_ipc_auth.cpp)
target_link_libraries(test_p14_ipc_auth PRIVATE polaris_core crypto)
add_test(NAME p14_ipc_auth COMMAND test_p14_ipc_auth)
add_executable(test_p14_socket_security tests/unit/test_p14_socket_security.cpp)
target_link_libraries(test_p14_socket_security PRIVATE polaris_core crypto)
add_test(NAME p14_socket_security COMMAND test_p14_socket_security)
add_executable(test_p14_ipc_server tests/unit/test_p14_ipc_server.cpp)
target_link_libraries(test_p14_ipc_server PRIVATE polaris_core crypto)
add_test(NAME p14_ipc_server COMMAND test_p14_ipc_server)
add_executable(test_p14_lock tests/unit/test_p14_lock.cpp)
target_link_libraries(test_p14_lock PRIVATE polaris_core crypto)
add_test(NAME p14_lock COMMAND test_p14_lock)
add_executable(test_p14_ipc_security tests/security/test_p14_ipc_security.cpp)
target_link_libraries(test_p14_ipc_security PRIVATE polaris_core crypto)
add_test(NAME p14_ipc_security COMMAND test_p14_ipc_security)
add_executable(test_p14_recovery tests/unit/test_p14_recovery.cpp)
target_link_libraries(test_p14_recovery PRIVATE polaris_core crypto)
add_test(NAME p14_recovery COMMAND test_p14_recovery)

# P15 Tests - Test/CI/Fixture Expansion (table-driven, deterministic, isolated)
add_executable(test_p15_lifecycle tests/unit/test_p15_lifecycle.cpp)
target_link_libraries(test_p15_lifecycle PRIVATE polaris_core crypto)
add_test(NAME p15_lifecycle COMMAND test_p15_lifecycle)
add_executable(test_p15_stale_matrix tests/unit/test_p15_stale_matrix.cpp)
target_link_libraries(test_p15_stale_matrix PRIVATE polaris_core crypto)
add_test(NAME p15_stale_matrix COMMAND test_p15_stale_matrix)
add_executable(test_p15_toctou_idempotency tests/unit/test_p15_toctou_idempotency.cpp)
target_link_libraries(test_p15_toctou_idempotency PRIVATE polaris_core crypto)
add_test(NAME p15_toctou_idempotency COMMAND test_p15_toctou_idempotency)
add_executable(test_p15_lock_recovery tests/unit/test_p15_lock_recovery.cpp)
target_link_libraries(test_p15_lock_recovery PRIVATE polaris_core crypto)
add_test(NAME p15_lock_recovery COMMAND test_p15_lock_recovery)
add_executable(test_p15_regression_audit tests/unit/test_p15_regression_audit.cpp)
target_link_libraries(test_p15_regression_audit PRIVATE polaris_core crypto)
add_test(NAME p15_regression_audit COMMAND test_p15_regression_audit)

# P16 Tests - Explainability
add_executable(test_p16_explanation_model tests/unit/test_p16_explanation_model.cpp)
target_link_libraries(test_p16_explanation_model PRIVATE polaris_core crypto)
add_test(NAME p16_explanation_model COMMAND test_p16_explanation_model)
add_executable(test_p16_explain_candidate tests/unit/test_p16_explain_candidate.cpp)
target_link_libraries(test_p16_explain_candidate PRIVATE polaris_core crypto)
add_test(NAME p16_explain_candidate COMMAND test_p16_explain_candidate)
add_executable(test_p16_explain_transaction tests/unit/test_p16_explain_transaction.cpp)
target_link_libraries(test_p16_explain_transaction PRIVATE polaris_core crypto)
add_test(NAME p16_explain_transaction COMMAND test_p16_explain_transaction)
add_executable(test_p16_verbose_redaction tests/security/test_p16_verbose_redaction.cpp)
target_link_libraries(test_p16_verbose_redaction PRIVATE polaris_core crypto)
add_test(NAME p16_verbose_redaction COMMAND test_p16_verbose_redaction)

# P19 Tests - Optimization Capability Framework
add_executable(test_p19_registry tests/unit/test_p19_registry.cpp)
target_link_libraries(test_p19_registry PRIVATE polaris_core crypto)
add_test(NAME p19_registry COMMAND test_p19_registry)
add_executable(test_p19_flatpak_capability tests/unit/test_p19_flatpak_capability.cpp)
target_link_libraries(test_p19_flatpak_capability PRIVATE polaris_core crypto)
add_test(NAME p19_flatpak_capability COMMAND test_p19_flatpak_capability)
add_executable(test_p19_journal_capability tests/unit/test_p19_journal_capability.cpp)
target_link_libraries(test_p19_journal_capability PRIVATE polaris_core crypto)
add_test(NAME p19_journal_capability COMMAND test_p19_journal_capability)
add_executable(test_p19_lifecycle tests/unit/test_p19_lifecycle.cpp)
target_link_libraries(test_p19_lifecycle PRIVATE polaris_core crypto)
add_test(NAME p19_lifecycle COMMAND test_p19_lifecycle)
add_executable(test_p19_capability_security tests/security/test_p19_capability_security.cpp)
target_link_libraries(test_p19_capability_security PRIVATE polaris_core crypto)
add_test(NAME p19_capability_security COMMAND test_p19_capability_security)
