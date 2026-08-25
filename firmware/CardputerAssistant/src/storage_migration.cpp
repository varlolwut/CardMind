#include "storage_migration.h"

#include "chat_storage.h"
#include "file_workspace.h"
#include "project_chat_storage.h"
#include "project_storage.h"

namespace cardputer {
namespace {

OperationResult validateProjectChats(const ProjectDocument& project)
{
    std::uint32_t offset = 0;
    std::uint32_t countedChats = 0;
    bool eof = false;
    while (!eof) {
        const ProjectChatsPageResult page = listProjectChatsPage(
            project.summary.id, offset, kMaximumProjectPageEntries);
        if (!page.success) {
            return {false, page.error};
        }
        for (const ChatSummary& chat : page.chats) {
            const OperationResult validation = validateProjectChat(project.summary.id, chat.id);
            if (!validation.success) {
                return {false, "Failed to validate migrated chat " + chat.id + ": " +
                                   validation.error};
            }
            ++countedChats;
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, "Project chat index pagination did not advance"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    return countedChats == project.summary.chatCount
        ? OperationResult{true, ""}
        : OperationResult{false, "Project chat index count does not match project metadata"};
}

OperationResult resetInterruptedMigration(ProjectStorageManifest& manifest)
{
    if (manifest.migrationState == ProjectMigrationState::Uninitialized) {
        return {true, ""};
    }
    const OperationResult reset = resetUncommittedProjectStorage();
    if (!reset.success) {
        return reset;
    }
    const ProjectStorageManifestResult reloaded = loadProjectStorageManifest();
    if (!reloaded.success) {
        return {false, reloaded.error};
    }
    manifest = reloaded.manifest;
    return {true, ""};
}

}  // namespace

OperationResult validateCommittedProjectStorage()
{
    const ProjectStorageManifestResult loaded = loadProjectStorageManifest();
    if (!loaded.success) {
        return {false, loaded.error};
    }
    if (loaded.manifest.migrationState != ProjectMigrationState::Committed) {
        return {false, "Project storage migration is not committed"};
    }
    if (loaded.manifest.activeProjectId.isEmpty()) {
        return {false, "Committed project storage has no active project"};
    }
    const ProjectDocumentResult project = loadProject(loaded.manifest.activeProjectId);
    if (!project.success) {
        return {false, project.error};
    }
    return validateProjectChats(project.project);
}

ProjectMigrationResult migrateLegacyStorageToProjects()
{
    OperationResult result = initializeChatStorage();
    if (!result.success) {
        return {false, false, "", 0, result.error};
    }
    result = initializeFileWorkspace();
    if (!result.success) {
        return {false, false, "", 0, result.error};
    }
    result = initializeProjectStorage();
    if (!result.success) {
        return {false, false, "", 0, result.error};
    }
    ProjectStorageManifestResult loadedManifest = loadProjectStorageManifest();
    if (!loadedManifest.success) {
        return {false, false, "", 0, loadedManifest.error};
    }
    if (loadedManifest.manifest.migrationState == ProjectMigrationState::Committed) {
        result = validateCommittedProjectStorage();
        return result.success
            ? ProjectMigrationResult{true, false, loadedManifest.manifest.activeProjectId, 0, ""}
            : ProjectMigrationResult{false, false, "", 0, result.error};
    }
    ProjectStorageManifest manifest = loadedManifest.manifest;
    result = resetInterruptedMigration(manifest);
    if (!result.success) {
        return {false, false, "", 0, result.error};
    }
    manifest.migrationState = ProjectMigrationState::Staging;
    manifest.activeProjectId = "";
    ++manifest.revision;
    result = saveProjectStorageManifest(manifest);
    if (!result.success) {
        return {false, false, "", 0, result.error};
    }
    const ProjectDocumentResult defaultProject = createProject("Default");
    if (!defaultProject.success) {
        return {false, false, "", 0, defaultProject.error};
    }
    const ChatsResult legacyChats = listChats();
    if (!legacyChats.success) {
        return {false, false, "", 0, legacyChats.error};
    }
    std::uint32_t migratedChats = 0;
    for (const ChatSummary& summary : legacyChats.chats) {
        const ChatDocumentResult legacyChat = loadChat(summary.id);
        if (!legacyChat.success) {
            return {false, false, "", migratedChats, legacyChat.error};
        }
        result = importLegacyChatToProject(defaultProject.project.summary.id, legacyChat.chat);
        if (!result.success) {
            return {false, false, "", migratedChats,
                    "Failed to migrate chat " + summary.id + ": " + result.error};
        }
        ++migratedChats;
    }
    const ProjectDocumentResult stagedProject = loadProject(defaultProject.project.summary.id);
    if (!stagedProject.success) {
        return {false, false, "", migratedChats, stagedProject.error};
    }
    result = validateProjectChats(stagedProject.project);
    if (!result.success || stagedProject.project.summary.chatCount != migratedChats) {
        return {false, false, "", migratedChats,
                result.success ? String("Migrated project chat count is incomplete") : result.error};
    }
    manifest.activeProjectId = stagedProject.project.summary.id;
    manifest.migrationState = ProjectMigrationState::Validated;
    ++manifest.revision;
    result = saveProjectStorageManifest(manifest);
    if (!result.success) {
        return {false, false, "", migratedChats, result.error};
    }
    const ProjectStorageManifestResult validatedManifest = loadProjectStorageManifest();
    if (!validatedManifest.success ||
        validatedManifest.manifest.migrationState != ProjectMigrationState::Validated ||
        validatedManifest.manifest.activeProjectId != stagedProject.project.summary.id) {
        return {false, false, "", migratedChats,
                validatedManifest.success
                    ? String("Validated migration manifest could not be reopened consistently")
                    : validatedManifest.error};
    }
    manifest = validatedManifest.manifest;
    manifest.migrationState = ProjectMigrationState::Committed;
    ++manifest.revision;
    result = saveProjectStorageManifest(manifest);
    if (!result.success) {
        return {false, false, "", migratedChats, result.error};
    }
    result = validateCommittedProjectStorage();
    return result.success
        ? ProjectMigrationResult{true, true, manifest.activeProjectId, migratedChats, ""}
        : ProjectMigrationResult{false, false, "", migratedChats, result.error};
}

}  // namespace cardputer
