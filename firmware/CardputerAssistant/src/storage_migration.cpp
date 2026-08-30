#include "storage_migration.h"

#include "chat_storage.h"
#include "file_workspace.h"
#include "project_chat_storage.h"
#include "project_storage.h"
#include "sd_storage.h"

#include <SD.h>

#include <new>
#include <utility>

namespace cardputer {
namespace {

constexpr const char* kMigrationDiagnosticMarker =
    "/assistant/.p2-migration-diagnostic";
constexpr const char* kMigrationDiagnosticBackup =
    "/assistant/v2.p2-migration-backup";

bool projectSummariesMatch(const ProjectSummary& indexed,
                           const ProjectSummary& stored)
{
    return indexed.id == stored.id && indexed.title == stored.title &&
        indexed.updatedAt == stored.updatedAt && indexed.chatCount == stored.chatCount &&
        indexed.pinned == stored.pinned && indexed.archived == stored.archived &&
        indexed.revision == stored.revision;
}

bool chatSummariesMatch(const ChatSummary& indexed,
                        const ChatSummary& stored)
{
    return indexed.id == stored.id && indexed.title == stored.title &&
        indexed.updatedAt == stored.updatedAt &&
        indexed.messageCount == stored.messageCount &&
        indexed.archivedMessageCount == stored.archivedMessageCount &&
        indexed.pinned == stored.pinned && indexed.archived == stored.archived &&
        indexed.revision == stored.revision;
}

OperationResult validateProjectChats(const ProjectDocument& project)
{
    std::uint32_t offset = 0;
    std::uint32_t countedChats = 0;
    bool activeChatFound = project.activeChatId.isEmpty();
    bool eof = false;
    while (!eof) {
        const ProjectChatsPageResult page = listProjectChatsPage(
            project.summary.id, offset, kMaximumProjectPageEntries);
        if (!page.success) {
            return {false, page.error};
        }
        for (const ChatSummary& chat : page.chats) {
            const ChatDocumentResult stored = loadProjectChatMetadata(
                project.summary.id, chat.id);
            if (!stored.success) {
                return {false, "Failed to load indexed chat " + chat.id + ": " +
                                   stored.error};
            }
            if (!chatSummariesMatch(chat, stored.chat.summary)) {
                return {false, "Project chat index summary does not match metadata for chat " +
                                   chat.id};
            }
            const OperationResult validation = validateProjectChat(project.summary.id, chat.id);
            if (!validation.success) {
                return {false, "Failed to validate project chat " + chat.id + ": " +
                                   validation.error};
            }
            activeChatFound = activeChatFound || chat.id == project.activeChatId;
            ++countedChats;
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, "Project chat index pagination did not advance"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    if (countedChats != project.summary.chatCount) {
        return {false, "Project chat index count does not match project metadata"};
    }
    return activeChatFound
        ? OperationResult{true, ""}
        : OperationResult{false, "Project active chat is missing from the chat index"};
}

OperationResult validateProjectSharedLinks(const ProjectDocument& project)
{
    std::uint32_t offset = 0;
    bool eof = false;
    while (!eof) {
        const SharedFileLinksPageResult page = listProjectSharedLinksPage(
            project.summary.id, offset, kMaximumProjectPageEntries);
        if (!page.success) {
            return {false, page.error};
        }
        for (const SharedFileLink& link : page.links) {
            if (!SD.exists(workspaceFilePath(link.path))) {
                return {false, "Project Shared link points to a missing file: " + link.path};
            }
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, "Project Shared-link index pagination did not advance"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    return {true, ""};
}

void updateMigrationHistoryHash(std::uint32_t& hash,
                                const std::uint8_t* data,
                                std::size_t bytes)
{
    for (std::size_t index = 0; index < bytes; ++index) {
        hash ^= data[index];
        hash *= 16777619U;
    }
}

void updateMigrationHistoryHash(std::uint32_t& hash, std::uint32_t value)
{
    const std::uint8_t bytes[] = {
        static_cast<std::uint8_t>(value & 0xffU),
        static_cast<std::uint8_t>((value >> 8U) & 0xffU),
        static_cast<std::uint8_t>((value >> 16U) & 0xffU),
        static_cast<std::uint8_t>((value >> 24U) & 0xffU),
    };
    updateMigrationHistoryHash(hash, bytes, sizeof(bytes));
}

void updateMigrationHistoryHash(std::uint32_t& hash, const Message& message)
{
    updateMigrationHistoryHash(hash, static_cast<std::uint32_t>(message.role.length()));
    updateMigrationHistoryHash(
        hash, reinterpret_cast<const std::uint8_t*>(message.role.c_str()),
        message.role.length());
    updateMigrationHistoryHash(hash, static_cast<std::uint32_t>(message.content.size()));
    updateMigrationHistoryHash(
        hash, reinterpret_cast<const std::uint8_t*>(message.content.data()),
        message.content.size());
}

OperationResult compareLegacyChatHistory(const String& projectId,
                                         const ChatDocument& legacyChat,
                                         std::uint32_t& matchedMessages,
                                         std::uint32_t& matchedArchivedMessages,
                                         std::uint32_t& historyFnv32)
{
    constexpr std::size_t kPageMessages = 16;
    constexpr std::size_t kPageBytes = 32768;
    std::uint32_t archiveOffset = 0;
    std::uint32_t migratedOffset = 0;
    std::size_t activeIndex = 0;
    bool archiveEof = false;
    while (!archiveEof || activeIndex < legacyChat.messages.size()) {
        ArchivedMessagesPageResult source = archiveEof
            ? ArchivedMessagesPageResult{true, {}, archiveOffset, true, ""}
            : readArchivedChatMessages(
                  legacyChat.summary.id, archiveOffset, kPageMessages, kPageBytes);
        if (!source.success) {
            return {false, source.error};
        }
        if (!source.eof && source.nextOffset <= archiveOffset) {
            return {false, "Legacy chat archive pagination did not advance"};
        }
        archiveOffset = source.nextOffset;
        archiveEof = source.eof;
        const std::size_t archivedPageMessages = source.messages.size();
        std::size_t sourceBytes = 0;
        for (const Message& message : source.messages) {
            sourceBytes += message.content.size();
        }
        try {
            while (archiveEof && source.messages.size() < kPageMessages &&
                   activeIndex < legacyChat.messages.size()) {
                const Message& message = legacyChat.messages[activeIndex];
                if (!source.messages.empty() &&
                    (sourceBytes > kPageBytes ||
                     message.content.size() > kPageBytes - sourceBytes)) {
                    break;
                }
                sourceBytes += message.content.size();
                source.messages.push_back(message);
                ++activeIndex;
            }
        } catch (const std::bad_alloc&) {
            return {false, "Failed to allocate a bounded migration comparison page"};
        }
        const ArchivedMessagesPageResult migrated = readProjectChatMessages(
            projectId, legacyChat.summary.id, migratedOffset,
            kPageMessages, kPageBytes);
        if (!migrated.success) {
            return {false, migrated.error};
        }
        if (migrated.messages.size() != source.messages.size()) {
            return {false, "Migrated chat history page count does not match legacy history"};
        }
        for (std::size_t index = 0; index < source.messages.size(); ++index) {
            const Message& expected = source.messages[index];
            const Message& actual = migrated.messages[index];
            if (actual.role != expected.role || actual.content != expected.content) {
                return {false, "Migrated chat message role or content does not match legacy history"};
            }
            if (matchedMessages == UINT32_MAX) {
                return {false, "Migration comparison message count overflows"};
            }
            updateMigrationHistoryHash(historyFnv32, expected);
            ++matchedMessages;
            if (index < archivedPageMessages) {
                if (matchedArchivedMessages == UINT32_MAX) {
                    return {false, "Migration archived message count overflows"};
                }
                ++matchedArchivedMessages;
            }
        }
        if (!migrated.eof && migrated.nextOffset <= migratedOffset) {
            return {false, "Migrated chat history pagination did not advance"};
        }
        migratedOffset = migrated.nextOffset;
        const bool sourceEof = archiveEof && activeIndex == legacyChat.messages.size();
        if (migrated.eof != sourceEof) {
            return {false, "Migrated chat history length does not match legacy history"};
        }
    }
    return {true, ""};
}

OperationResult compareLegacyMigration(const String& projectId,
                                       const ChatsResult& legacy,
                                       std::uint32_t& matchedChats,
                                       std::uint32_t& matchedMessages,
                                       std::uint32_t& matchedArchivedMessages,
                                       std::uint32_t& historyFnv32)
{
    matchedChats = 0;
    matchedMessages = 0;
    matchedArchivedMessages = 0;
    historyFnv32 = 2166136261U;
    const ProjectDocumentResult project = loadProject(projectId);
    if (!project.success) {
        return {false, project.error};
    }
    if (project.project.summary.chatCount != legacy.chats.size()) {
        return {false, "Migrated project chat count does not match legacy storage"};
    }
    for (const ChatSummary& summary : legacy.chats) {
        const ChatDocumentResult source = loadChat(summary.id);
        if (!source.success) {
            return {false, source.error};
        }
        const ChatDocumentResult migrated = loadProjectChatMetadata(projectId, summary.id);
        if (!migrated.success) {
            return {false, "Migrated project is missing legacy chat " + summary.id + ": " +
                               migrated.error};
        }
        if (source.chat.messages.size() >
            UINT32_MAX - source.chat.summary.archivedMessageCount) {
            return {false, "Legacy chat message count overflows"};
        }
        const std::uint32_t sourceMessages = source.chat.summary.archivedMessageCount +
            static_cast<std::uint32_t>(source.chat.messages.size());
        if (migrated.chat.summary.id != source.chat.summary.id ||
            migrated.chat.projectId != projectId ||
            migrated.chat.summary.title != source.chat.summary.title ||
            migrated.chat.summary.updatedAt != source.chat.summary.updatedAt ||
            migrated.chat.summary.messageCount != sourceMessages ||
            migrated.chat.summary.archivedMessageCount != 0 ||
            migrated.chat.summary.pinned != source.chat.summary.pinned ||
            migrated.chat.summary.archived != source.chat.summary.archived ||
            migrated.chat.summary.revision != 1 ||
            migrated.chat.instructions != source.chat.instructions ||
            migrated.chat.draft != source.chat.draft ||
            migrated.chat.toolPolicy != source.chat.toolPolicy ||
            !migrated.chat.contextSummary.empty() ||
            migrated.chat.summarizedMessageCount != 0) {
            return {false, "Migrated chat metadata does not match legacy chat " + summary.id};
        }
        const OperationResult history = compareLegacyChatHistory(
            projectId, source.chat, matchedMessages,
            matchedArchivedMessages, historyFnv32);
        if (!history.success) {
            return {false, "Migrated chat history does not match legacy chat " +
                               summary.id + ": " + history.error};
        }
        ++matchedChats;
    }
    return {true, ""};
}

OperationResult restoreProjectMigrationDiagnosticBackup()
{
    const String storageRoot = projectStorageRoot();
    if (SD.exists(storageRoot)) {
        const OperationResult removed = removeSdDirectoryTree(storageRoot);
        if (!removed.success) {
            return removed;
        }
    }
    if (!SD.exists(kMigrationDiagnosticBackup) ||
        !SD.rename(kMigrationDiagnosticBackup, storageRoot)) {
        return {false, "Failed to restore project storage diagnostic backup"};
    }
    if (!SD.remove(kMigrationDiagnosticMarker)) {
        return {false, "Project storage was restored but diagnostic marker removal failed"};
    }
    return {true, ""};
}

OperationResult stageProjectMigrationDiagnosticBackup()
{
    OperationResult result = recoverInterruptedProjectMigrationDiagnostic();
    if (!result.success) {
        return result;
    }
    result = validateCommittedProjectStorage();
    if (!result.success) {
        return result;
    }
    result = writeEmptyAtomicSdFile(kMigrationDiagnosticMarker);
    if (!result.success) {
        return result;
    }
    if (!SD.rename(projectStorageRoot(), kMigrationDiagnosticBackup)) {
        SD.remove(kMigrationDiagnosticMarker);
        return {false, "Failed to stage current project storage for migration diagnostic"};
    }
    return {true, ""};
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

OperationResult recoverInterruptedProjectMigrationDiagnostic()
{
    const bool markerExists = SD.exists(kMigrationDiagnosticMarker);
    const bool backupExists = SD.exists(kMigrationDiagnosticBackup);
    if (!markerExists && !backupExists) {
        return {true, ""};
    }
    if (!markerExists || !backupExists) {
        return {false, "Project migration diagnostic recovery artifacts are incomplete"};
    }
    return restoreProjectMigrationDiagnosticBackup();
}

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
    std::uint32_t offset = 0;
    bool eof = false;
    bool activeProjectFound = false;
    std::uint32_t projectCount = 0;
    while (!eof) {
        const ProjectsPageResult page = listProjectsPage(
            offset, kMaximumProjectPageEntries);
        if (!page.success) {
            return {false, page.error};
        }
        for (const ProjectSummary& summary : page.projects) {
            const ProjectDocumentResult project = loadProject(summary.id);
            if (!project.success) {
                return {false, "Failed to load indexed project " + summary.id + ": " +
                                   project.error};
            }
            if (!projectSummariesMatch(summary, project.project.summary)) {
                return {false, "Project index summary does not match metadata for project " +
                                   summary.id};
            }
            OperationResult validation = validateProjectChats(project.project);
            if (!validation.success) {
                return {false, "Failed to validate project " + summary.id + ": " +
                                   validation.error};
            }
            validation = validateProjectSharedLinks(project.project);
            if (!validation.success) {
                return {false, "Failed to validate project " + summary.id + ": " +
                                   validation.error};
            }
            activeProjectFound = activeProjectFound ||
                summary.id == loaded.manifest.activeProjectId;
            ++projectCount;
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, "Project index pagination did not advance"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    if (projectCount == 0) {
        return {false, "Committed project storage contains no projects"};
    }
    return activeProjectFound
        ? OperationResult{true, ""}
        : OperationResult{false, "Active project is missing from the project index"};
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

ProjectMigrationDiagnosticResult runProjectMigrationDiagnostic()
{
    OperationResult result = stageProjectMigrationDiagnosticBackup();
    if (!result.success) {
        return {false, 0, 0, 0, 0, 0, 0, result.error};
    }
    const ChatsResult legacy = listChats();
    if (!legacy.success) {
        const OperationResult restored = restoreProjectMigrationDiagnosticBackup();
        return {false, 0, 0, 0, 0, 0, 0, restored.success
            ? legacy.error
            : legacy.error + "; original storage restore also failed: " + restored.error};
    }
    std::uint32_t matchedChats = 0;
    std::uint32_t matchedMessages = 0;
    std::uint32_t matchedArchivedMessages = 0;
    std::uint32_t historyFnv32 = 2166136261U;
    std::uint32_t diagnosticRevision = 0;
    const ProjectMigrationResult migrated = migrateLegacyStorageToProjects();
    if (!migrated.success || !migrated.migrated) {
        result = {false, migrated.success
            ? String("Fresh diagnostic storage did not run the legacy migration")
            : migrated.error};
    }
    if (result.success) {
        result = compareLegacyMigration(
            migrated.activeProjectId, legacy, matchedChats,
            matchedMessages, matchedArchivedMessages, historyFnv32);
    }
    ProjectStorageManifestResult beforeRepeat = result.success
        ? loadProjectStorageManifest()
        : ProjectStorageManifestResult{false, {}, result.error};
    const ProjectMigrationResult repeated = beforeRepeat.success
        ? migrateLegacyStorageToProjects()
        : ProjectMigrationResult{false, false, "", 0, beforeRepeat.error};
    const ProjectStorageManifestResult afterRepeat = repeated.success
        ? loadProjectStorageManifest()
        : ProjectStorageManifestResult{false, {}, repeated.error};
    if (result.success && (!repeated.success || repeated.migrated || !afterRepeat.success ||
                           afterRepeat.manifest.activeProjectId !=
                               beforeRepeat.manifest.activeProjectId ||
                           afterRepeat.manifest.revision != beforeRepeat.manifest.revision)) {
        result = {false, repeated.success
            ? String("Repeated diagnostic migration changed committed storage")
            : repeated.error};
    }
    if (afterRepeat.success) {
        diagnosticRevision = afterRepeat.manifest.revision;
    }
    const OperationResult restored = restoreProjectMigrationDiagnosticBackup();
    if (!restored.success) {
        return {false, static_cast<std::uint32_t>(legacy.chats.size()), matchedChats,
                matchedMessages, matchedArchivedMessages, historyFnv32,
                diagnosticRevision, result.success
                    ? restored.error
                    : result.error + "; original storage restore also failed: " +
                          restored.error};
    }
    const OperationResult restoredValidation = validateCommittedProjectStorage();
    if (!restoredValidation.success) {
        return {false, static_cast<std::uint32_t>(legacy.chats.size()), matchedChats,
                matchedMessages, matchedArchivedMessages, historyFnv32,
                diagnosticRevision,
                "Restored project storage validation failed: " +
                                        restoredValidation.error};
    }
    return result.success
        ? ProjectMigrationDiagnosticResult{
              true, static_cast<std::uint32_t>(legacy.chats.size()), matchedChats,
              matchedMessages, matchedArchivedMessages, historyFnv32,
              diagnosticRevision, ""}
        : ProjectMigrationDiagnosticResult{
              false, static_cast<std::uint32_t>(legacy.chats.size()), matchedChats,
              matchedMessages, matchedArchivedMessages, historyFnv32,
              diagnosticRevision, result.error};
}

ProjectMigrationRecoveryDiagnosticResult runProjectMigrationRecoveryDiagnostic()
{
    OperationResult result = stageProjectMigrationDiagnosticBackup();
    if (!result.success) {
        return {false, false, false, false, result.error};
    }
    bool stagingRecovered = false;
    bool corruptionDetected = false;
    result = initializeProjectStorage();
    ProjectStorageManifestResult manifest = result.success
        ? loadProjectStorageManifest()
        : ProjectStorageManifestResult{false, {}, result.error};
    if (manifest.success) {
        manifest.manifest.migrationState = ProjectMigrationState::Staging;
        ++manifest.manifest.revision;
        result = saveProjectStorageManifest(manifest.manifest);
    } else if (result.success) {
        result = {false, manifest.error};
    }
    const ProjectDocumentResult partial = result.success
        ? createProject("Interrupted migration")
        : ProjectDocumentResult{false, {}, result.error};
    if (result.success && !partial.success) {
        result = {false, partial.error};
    }
    const ProjectMigrationResult recovered = result.success
        ? migrateLegacyStorageToProjects()
        : ProjectMigrationResult{false, false, "", 0, result.error};
    if (result.success && (!recovered.success || !recovered.migrated ||
                           recovered.activeProjectId == partial.project.summary.id)) {
        result = {false, recovered.success
            ? String("Interrupted staging tree was not rebuilt")
            : recovered.error};
    }
    if (result.success) {
        const OperationResult validation = validateCommittedProjectStorage();
        if (!validation.success) {
            result = validation;
        } else {
            stagingRecovered = true;
        }
    }
    if (result.success) {
        const String indexPath = projectStorageRoot() + "/projects/index.jsonl";
        File index = SD.open(indexPath, FILE_APPEND);
        const String invalidLine = "{\"id\":123}\n";
        if (!index || index.print(invalidLine) != invalidLine.length()) {
            result = {false, "Failed to create corrupted diagnostic project index"};
        }
        if (index) {
            index.flush();
            index.close();
        }
    }
    if (result.success) {
        const OperationResult validation = validateCommittedProjectStorage();
        if (validation.success) {
            result = {false, "Corrupted project index was accepted"};
        } else {
            corruptionDetected = true;
        }
    }
    const OperationResult restored = recoverInterruptedProjectMigrationDiagnostic();
    bool originalRestored = false;
    if (restored.success) {
        const OperationResult validation = validateCommittedProjectStorage();
        originalRestored = validation.success;
        if (result.success && !validation.success) {
            result = {false, "Restored project storage validation failed: " +
                               validation.error};
        }
    } else {
        result = result.success
            ? restored
            : OperationResult{false, result.error +
                                       "; original storage restore also failed: " +
                                       restored.error};
    }
    return {result.success && stagingRecovered && corruptionDetected && originalRestored,
            stagingRecovered, corruptionDetected, originalRestored,
            result.success ? String() : result.error};
}

}  // namespace cardputer
