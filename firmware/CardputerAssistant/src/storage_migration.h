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

ProjectMigrationResult migrateLegacyStorageToProjects();
OperationResult validateCommittedProjectStorage();

}  // namespace cardputer
