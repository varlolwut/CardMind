#pragma once

#include "app_types.h"

namespace cardputer {

struct ProjectMigrationResult {
    bool success;
    bool migrated;
    String activeProjectId;
    std::uint32_t migratedChats;
    String error;
};

struct ProjectMigrationDiagnosticResult {
    bool success;
    std::uint32_t legacyChats;
    std::uint32_t matchedChats;
    std::uint32_t matchedMessages;
    std::uint32_t matchedArchivedMessages;
    std::uint32_t historyFnv32;
    std::uint32_t revision;
    String error;
};

struct ProjectMigrationRecoveryDiagnosticResult {
    bool success;
    bool stagingRecovered;
    bool corruptionDetected;
    bool originalRestored;
    String error;
};

ProjectMigrationResult migrateLegacyStorageToProjects();
OperationResult validateCommittedProjectStorage();
OperationResult recoverInterruptedProjectMigrationDiagnostic();
ProjectMigrationDiagnosticResult runProjectMigrationDiagnostic();
ProjectMigrationRecoveryDiagnosticResult runProjectMigrationRecoveryDiagnostic();

}  // namespace cardputer
