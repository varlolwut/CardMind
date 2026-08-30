#include "project_storage.h"

#include "project_chat_storage.h"
#include "sd_storage.h"
#include "text_utils.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <esp_random.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <utility>

namespace cardputer {
namespace {

constexpr const char* kAssistantDirectory = "/assistant";
constexpr const char* kStorageRoot = "/assistant/v2";
constexpr const char* kProjectsRoot = "/assistant/v2/projects";
constexpr const char* kProjectsIndex = "/assistant/v2/projects/index.jsonl";
constexpr const char* kManifestPath = "/assistant/v2/manifest.json";
constexpr const char* kSharedRoot = "/assistant/files";
constexpr std::size_t kMaximumProjectJsonFieldNameBytes = 64;

bool isProjectJsonWhitespace(int value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

OperationResult restoreIndexEntry(const String& path,
                                  const String& keyField,
                                  const String& keyValue,
                                  const StorageIndexLookupResult& previous);

const char* migrationStateName(ProjectMigrationState state)
{
    switch (state) {
        case ProjectMigrationState::Uninitialized: return "uninitialized";
        case ProjectMigrationState::Staging: return "staging";
        case ProjectMigrationState::Validated: return "validated";
        case ProjectMigrationState::Committed: return "committed";
    }
    return "invalid";
}

bool parseMigrationState(const String& value, ProjectMigrationState& state)
{
    if (value == "uninitialized") {
        state = ProjectMigrationState::Uninitialized;
    } else if (value == "staging") {
        state = ProjectMigrationState::Staging;
    } else if (value == "validated") {
        state = ProjectMigrationState::Validated;
    } else if (value == "committed") {
        state = ProjectMigrationState::Committed;
    } else {
        return false;
    }
    return true;
}

std::uint64_t currentTimestamp()
{
    const std::time_t current = std::time(nullptr);
    return current >= 1700000000 ? static_cast<std::uint64_t>(current) : 0;
}

String generateStorageId()
{
    char buffer[17] = {};
    std::snprintf(buffer, sizeof(buffer), "%08x%08x",
                  static_cast<unsigned int>(esp_random()),
                  static_cast<unsigned int>(esp_random()));
    return String(buffer);
}

String projectMetadataPath(const String& id)
{
    return projectDirectoryPath(id) + "/project.json";
}

String projectChatsIndexPath(const String& id)
{
    return projectChatsDirectoryPath(id) + "/index.jsonl";
}

String projectSharedLinksPath(const String& id)
{
    return projectDirectoryPath(id) + "/shared-links.jsonl";
}

OperationResult validateProjectSummary(const ProjectSummary& summary)
{
    if (!isValidChatId(summary.id.c_str())) {
        return {false, "Project id must contain exactly 16 lowercase hexadecimal characters"};
    }
    if (summary.title.isEmpty() || summary.title.length() > kMaximumProjectTitleBytes ||
        !isValidUtf8(summary.title.c_str())) {
        return {false, "Project title must be valid UTF-8 and contain 1 to 120 bytes"};
    }
    return {true, ""};
}

OperationResult validateOptionalProjectString(const String& value,
                                               const String& field,
                                               std::size_t maximumBytes)
{
    if (value.length() > maximumBytes || !isValidUtf8(value.c_str())) {
        return {false, field + " must be valid UTF-8 up to " + String(maximumBytes) +
                           " bytes"};
    }
    return {true, ""};
}

OperationResult validateProjectDocument(const ProjectDocument& project)
{
    OperationResult result = validateProjectSummary(project.summary);
    if (!result.success) {
        return result;
    }
    if (project.instructions.size() > kMaximumProjectInstructionsBytes ||
        !isValidUtf8(project.instructions)) {
        return {false, "Project instructions must be valid UTF-8 up to 16384 bytes"};
    }
    if (!project.activeChatId.isEmpty() && !isValidChatId(project.activeChatId.c_str())) {
        return {false, "Project active chat id is invalid"};
    }
    if (project.contextByteBudget < 8192 || project.contextByteBudget > 262144) {
        return {false, "Project context budget must be between 8192 and 262144 bytes"};
    }
    if (project.maximumOutputTokens < 128 || project.maximumOutputTokens > 8192) {
        return {false, "Project output budget must be between 128 and 8192 tokens"};
    }
    result = validateOptionalProjectString(project.model, "Project model", 240);
    if (!result.success) {
        return result;
    }
    result = validateOptionalProjectString(project.apiProfile, "Project API profile", 120);
    if (!result.success) {
        return result;
    }
    const ToolPolicyEncodeResult encodedToolPolicy =
        encodeScopedToolPermissionPolicy(project.toolPolicy);
    if (encodedToolPolicy.error != ToolPolicyCodecError::None) {
        return {
            false,
            String("Project tool policy is invalid: ") +
                toolPolicyCodecErrorText(encodedToolPolicy.error),
        };
    }
    return validateOptionalProjectString(project.sshProfile, "Project SSH profile", 120);
}

String serializeProjectSummary(const ProjectSummary& summary)
{
    JsonDocument document;
    document["id"] = summary.id;
    document["title"] = summary.title;
    document["updated_at"] = summary.updatedAt;
    document["chat_count"] = summary.chatCount;
    document["pinned"] = summary.pinned;
    document["archived"] = summary.archived;
    document["revision"] = summary.revision;
    String line;
    serializeJson(document, line);
    return line;
}

String serializeChatSummary(const ChatSummary& summary)
{
    JsonDocument document;
    document["id"] = summary.id;
    document["title"] = summary.title;
    document["updated_at"] = summary.updatedAt;
    document["message_count"] = summary.messageCount;
    document["archived_message_count"] = summary.archivedMessageCount;
    document["pinned"] = summary.pinned;
    document["archived"] = summary.archived;
    document["revision"] = summary.revision;
    String line;
    serializeJson(document, line);
    return line;
}

ProjectDocumentResult parseProjectDocumentCore(File& file)
{
    JsonDocument filter;
    filter["version"] = true;
    filter["id"] = true;
    filter["title"] = true;
    filter["updated_at"] = true;
    filter["chat_count"] = true;
    filter["pinned"] = true;
    filter["archived"] = true;
    filter["revision"] = true;
    filter["active_chat_id"] = true;
    filter["model"] = true;
    filter["api_profile"] = true;
    filter["tool_policy"] = true;
    filter["ssh_profile"] = true;
    filter["context_byte_budget"] = true;
    filter["maximum_output_tokens"] = true;
    filter["automatic_compaction"] = true;
    filter["chat_index_revision"] = true;
    filter["shared_links_revision"] = true;
    JsonDocument document;
    const DeserializationError error = deserializeJson(
        document, file, DeserializationOption::Filter(filter));
    if (error) {
        return {false, {}, "Failed to parse project JSON: " + String(error.c_str())};
    }
    if (!document["version"].is<std::uint32_t>() ||
        document["version"].as<std::uint32_t>() != kProjectStorageFormatVersion ||
        !document["id"].is<const char*>() || !document["title"].is<const char*>() ||
        !document["updated_at"].is<std::uint64_t>() ||
        !document["chat_count"].is<std::uint32_t>() ||
        !document["pinned"].is<bool>() || !document["archived"].is<bool>() ||
        !document["revision"].is<std::uint32_t>() ||
        !document["active_chat_id"].is<const char*>() ||
        !document["model"].is<const char*>() ||
        !document["api_profile"].is<const char*>() ||
        !document["tool_policy"].is<const char*>() ||
        !document["ssh_profile"].is<const char*>() ||
        !document["context_byte_budget"].is<std::uint32_t>() ||
        !document["maximum_output_tokens"].is<std::uint32_t>() ||
        !document["automatic_compaction"].is<bool>() ||
        !document["chat_index_revision"].is<std::uint32_t>() ||
        !document["shared_links_revision"].is<std::uint32_t>()) {
        return {false, {}, "Project JSON is missing required typed fields"};
    }
    ProjectDocument project;
    project.summary.id = document["id"].as<const char*>();
    project.summary.title = document["title"].as<const char*>();
    project.summary.updatedAt = document["updated_at"].as<std::uint64_t>();
    project.summary.chatCount = document["chat_count"].as<std::uint32_t>();
    project.summary.pinned = document["pinned"].as<bool>();
    project.summary.archived = document["archived"].as<bool>();
    project.summary.revision = document["revision"].as<std::uint32_t>();
    project.activeChatId = document["active_chat_id"].as<const char*>();
    project.model = document["model"].as<const char*>();
    project.apiProfile = document["api_profile"].as<const char*>();
    const char* const encodedToolPolicy = document["tool_policy"].as<const char*>();
    const std::size_t encodedToolPolicyLength = std::strlen(encodedToolPolicy);
    if (encodedToolPolicyLength == 0) {
        project.toolPolicy = inheritedToolPermissionPolicy();
    } else {
        const ScopedToolPermissionPolicyDecodeResult decodedToolPolicy =
            decodeScopedToolPermissionPolicy(
                encodedToolPolicy, encodedToolPolicyLength);
        if (decodedToolPolicy.error != ToolPolicyCodecError::None) {
            return {
                false,
                {},
                String("Project tool policy is invalid: ") +
                    toolPolicyCodecErrorText(decodedToolPolicy.error),
            };
        }
        project.toolPolicy = decodedToolPolicy.policy;
    }
    project.sshProfile = document["ssh_profile"].as<const char*>();
    project.contextByteBudget = document["context_byte_budget"].as<std::uint32_t>();
    project.maximumOutputTokens = document["maximum_output_tokens"].as<std::uint32_t>();
    project.automaticCompaction = document["automatic_compaction"].as<bool>();
    project.chatIndexRevision = document["chat_index_revision"].as<std::uint32_t>();
    project.sharedLinksRevision = document["shared_links_revision"].as<std::uint32_t>();
    const OperationResult validation = validateProjectDocument(project);
    return validation.success ? ProjectDocumentResult{true, std::move(project), ""}
                              : ProjectDocumentResult{false, {}, validation.error};
}

ProjectDocumentResult parseProjectDocument(File& file, const String& path)
{
    ProjectDocumentResult result = parseProjectDocumentCore(file);
    if (!result.success) return result;
    JsonStringFieldResult instructions = readJsonStringField(
        path, "instructions", kMaximumProjectInstructionsBytes);
    if (!instructions.success) return {false, {}, instructions.error};
    result.project.instructions = std::move(instructions.value);
    const OperationResult validation = validateProjectDocument(result.project);
    return validation.success ? ProjectDocumentResult{true, std::move(result.project), ""}
                              : ProjectDocumentResult{false, {}, validation.error};
}

OperationResult writeProjectDocumentWithSummary(const ProjectDocument& project,
                                                const ProjectSummary& summary)
{
    const ToolPolicyEncodeResult encodedToolPolicy =
        encodeScopedToolPermissionPolicy(project.toolPolicy);
    if (encodedToolPolicy.error != ToolPolicyCodecError::None) {
        return {
            false,
            String("Project tool policy is invalid: ") +
                toolPolicyCodecErrorText(encodedToolPolicy.error),
        };
    }
    JsonDocument document;
    document["version"] = kProjectStorageFormatVersion;
    document["id"] = summary.id;
    document["title"] = summary.title;
    document["updated_at"] = summary.updatedAt;
    document["chat_count"] = summary.chatCount;
    document["pinned"] = summary.pinned;
    document["archived"] = summary.archived;
    document["revision"] = summary.revision;
    document["instructions"] = project.instructions.c_str();
    document["active_chat_id"] = project.activeChatId.c_str();
    document["model"] = project.model.c_str();
    document["api_profile"] = project.apiProfile.c_str();
    document["tool_policy"] = encodedToolPolicy.encoded.value.data();
    document["ssh_profile"] = project.sshProfile.c_str();
    document["context_byte_budget"] = project.contextByteBudget;
    document["maximum_output_tokens"] = project.maximumOutputTokens;
    document["automatic_compaction"] = project.automaticCompaction;
    document["chat_index_revision"] = project.chatIndexRevision;
    document["shared_links_revision"] = project.sharedLinksRevision;
    return writeAtomicJsonSdFile(projectMetadataPath(summary.id), document);
}

OperationResult writeProjectDocument(const ProjectDocument& project)
{
    return writeProjectDocumentWithSummary(project, project.summary);
}

ProjectDocumentResult loadProjectCore(const String& id)
{
    const String path = projectMetadataPath(id);
    const OperationResult recovered = recoverAtomicSdFile(path);
    if (!recovered.success) return {false, {}, recovered.error};
    File file = SD.open(path, FILE_READ);
    if (!file) return {false, {}, "Project metadata does not exist for id " + id};
    ProjectDocumentResult result = parseProjectDocumentCore(file);
    file.close();
    if (result.success && result.project.summary.id != id) {
        return {false, {}, "Project directory id does not match project metadata"};
    }
    return result;
}

struct ProjectCounterPatch {
    std::uint64_t updatedAt;
    std::uint32_t chatCount;
    std::uint32_t revision;
    std::uint32_t chatIndexRevision;
};

bool projectCounterReplacement(const String& key,
                               const ProjectCounterPatch& patch,
                               String& replacement)
{
    if (key == "updated_at") {
        replacement = String(static_cast<unsigned long long>(patch.updatedAt));
    } else if (key == "chat_count") {
        replacement = String(patch.chatCount);
    } else if (key == "revision") {
        replacement = String(patch.revision);
    } else if (key == "chat_index_revision") {
        replacement = String(patch.chatIndexRevision);
    } else {
        return false;
    }
    return true;
}

OperationResult patchProjectCounters(const String& projectId,
                                     const ProjectCounterPatch& patch)
{
    const String path = projectMetadataPath(projectId);
    OperationResult result = recoverAtomicSdFile(path);
    if (!result.success) return result;
    File input = SD.open(path, FILE_READ);
    if (!input || input.isDirectory()) {
        if (input) input.close();
        return {false, "Project metadata cannot be opened for counter update"};
    }
    result = checkSdOperationSpace(input.size(), kStorageOperationalFloorBytes);
    if (!result.success) {
        input.close();
        return result;
    }
    const String stagedPath = path + ".tmp";
    File output = SD.open(stagedPath, FILE_WRITE);
    if (!output) {
        input.close();
        return {false, "Failed to create staged project counter update"};
    }
    std::uint32_t depth = 0;
    bool inString = false;
    bool escaped = false;
    bool expectingKey = false;
    bool capturingKey = false;
    String key;
    std::uint8_t patchedFields = 0;
    while (input.available() && result.success) {
        const int value = input.read();
        if (value < 0 || output.write(static_cast<std::uint8_t>(value)) != 1) {
            result = {false, "Failed while streaming staged project counter update"};
            break;
        }
        if (inString) {
            if (escaped) {
                escaped = false;
                if (capturingKey) key = "";
            } else if (value == '\\') {
                escaped = true;
            } else if (value == '"') {
                inString = false;
                if (capturingKey) expectingKey = false;
                capturingKey = false;
            } else if (capturingKey && key.length() <= kMaximumProjectJsonFieldNameBytes) {
                key += static_cast<char>(value);
            }
            continue;
        }
        if (value == '"') {
            inString = true;
            capturingKey = depth == 1 && expectingKey;
            if (capturingKey) key = "";
            continue;
        }
        if (value == '{' || value == '[') {
            ++depth;
            if (depth == 1 && value == '{') expectingKey = true;
            continue;
        }
        if (value == '}' || value == ']') {
            if (depth == 0) {
                result = {false, "Project JSON contains mismatched brackets"};
                break;
            }
            --depth;
            continue;
        }
        if (depth == 1 && value == ',') {
            expectingKey = true;
            continue;
        }
        if (depth != 1 || value != ':' || expectingKey) continue;
        String replacement;
        if (!projectCounterReplacement(key, patch, replacement)) continue;
        while (input.available() && isProjectJsonWhitespace(input.peek())) {
            const int whitespace = input.read();
            if (output.write(static_cast<std::uint8_t>(whitespace)) != 1) {
                result = {false, "Failed while copying project counter whitespace"};
                break;
            }
        }
        if (!result.success) break;
        if (!input.available() || input.peek() < '0' || input.peek() > '9') {
            result = {false, "Project counter is not an unsigned JSON number: " + key};
            break;
        }
        while (input.available() && input.peek() >= '0' && input.peek() <= '9') {
            input.read();
        }
        if (output.print(replacement) != replacement.length()) {
            result = {false, "Failed to write updated project counter: " + key};
            break;
        }
        ++patchedFields;
    }
    const std::uint64_t sourceBytes = input.size();
    input.close();
    output.flush();
    output.close();
    if (result.success && (depth != 0 || inString || patchedFields != 4)) {
        result = {false, "Project counter update did not patch four complete fields"};
    }
    if (result.success) {
        File validation = SD.open(stagedPath, FILE_READ);
        if (!validation || validation.size() + 80 < sourceBytes ||
            validation.size() > sourceBytes + 80) {
            if (validation) validation.close();
            result = {false, "Staged project counter update has an invalid size"};
        } else {
            ProjectDocumentResult parsed = parseProjectDocumentCore(validation);
            validation.close();
            if (!parsed.success || parsed.project.summary.updatedAt != patch.updatedAt ||
                parsed.project.summary.chatCount != patch.chatCount ||
                parsed.project.summary.revision != patch.revision ||
                parsed.project.chatIndexRevision != patch.chatIndexRevision) {
                result = {false, parsed.success
                    ? String("Staged project counters do not match requested values")
                    : parsed.error};
            }
        }
    }
    if (!result.success) {
        SD.remove(stagedPath);
        return result;
    }
    return commitStagedSdFile(path, stagedPath);
}

OperationResult persistProjectCounterUpdate(const ProjectDocument& project)
{
    const StorageIndexLookupResult previous = findJsonlSdIndexEntry(
        kProjectsIndex, "id", project.summary.id);
    if (!previous.success || !previous.found) {
        return {false, previous.success
            ? String("Project index entry is missing for id ") + project.summary.id
            : previous.error};
    }
    OperationResult result = mutateJsonlSdIndex(
        kProjectsIndex, "id", project.summary.id,
        serializeProjectSummary(project.summary), false, kStorageOperationalFloorBytes);
    if (!result.success) return result;
    result = patchProjectCounters(project.summary.id,
        {project.summary.updatedAt, project.summary.chatCount,
         project.summary.revision, project.chatIndexRevision});
    if (result.success) return result;
    const OperationResult rollback = restoreIndexEntry(
        kProjectsIndex, "id", project.summary.id, previous);
    return rollback.success
        ? result
        : OperationResult{false, result.error + "; project index rollback also failed: " +
                                   rollback.error};
}

ProjectDocumentResult parseProjectSummaryLine(const String& line)
{
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, line);
    if (error || !document["id"].is<const char*>() ||
        !document["title"].is<const char*>() ||
        !document["updated_at"].is<std::uint64_t>() ||
        !document["chat_count"].is<std::uint32_t>() ||
        !document["pinned"].is<bool>() || !document["archived"].is<bool>() ||
        !document["revision"].is<std::uint32_t>()) {
        return {false, {}, "Project index contains an invalid typed summary"};
    }
    ProjectDocument project;
    project.summary.id = document["id"].as<const char*>();
    project.summary.title = document["title"].as<const char*>();
    project.summary.updatedAt = document["updated_at"].as<std::uint64_t>();
    project.summary.chatCount = document["chat_count"].as<std::uint32_t>();
    project.summary.pinned = document["pinned"].as<bool>();
    project.summary.archived = document["archived"].as<bool>();
    project.summary.revision = document["revision"].as<std::uint32_t>();
    const OperationResult validation = validateProjectSummary(project.summary);
    return validation.success ? ProjectDocumentResult{true, std::move(project), ""}
                              : ProjectDocumentResult{false, {}, validation.error};
}

ChatDocumentResult parseChatSummaryLine(const String& line)
{
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, line);
    if (error || !document["id"].is<const char*>() ||
        !document["title"].is<const char*>() ||
        !document["updated_at"].is<std::uint64_t>() ||
        !document["message_count"].is<std::uint32_t>() ||
        !document["archived_message_count"].is<std::uint32_t>() ||
        !document["pinned"].is<bool>() || !document["archived"].is<bool>() ||
        !document["revision"].is<std::uint32_t>()) {
        return {false, {}, "Project chat index contains an invalid typed summary"};
    }
    ChatDocument chat;
    chat.summary.id = document["id"].as<const char*>();
    chat.summary.title = document["title"].as<const char*>();
    chat.summary.updatedAt = document["updated_at"].as<std::uint64_t>();
    chat.summary.messageCount = document["message_count"].as<std::uint32_t>();
    chat.summary.archivedMessageCount =
        document["archived_message_count"].as<std::uint32_t>();
    chat.summary.pinned = document["pinned"].as<bool>();
    chat.summary.archived = document["archived"].as<bool>();
    chat.summary.revision = document["revision"].as<std::uint32_t>();
    if (!isValidChatId(chat.summary.id.c_str()) || chat.summary.title.isEmpty() ||
        !isValidUtf8(chat.summary.title.c_str())) {
        return {false, {}, "Project chat index summary contains invalid metadata"};
    }
    return {true, std::move(chat), ""};
}

OperationResult createProjectDirectories(const String& id)
{
    OperationResult result = ensureSdDirectory(projectDirectoryPath(id));
    if (!result.success) {
        return result;
    }
    result = ensureSdDirectory(projectChatsDirectoryPath(id));
    if (!result.success) {
        return result;
    }
    const String index = projectChatsIndexPath(id);
    if (!SD.exists(index)) {
        result = writeEmptyAtomicSdFile(index);
        if (!result.success) {
            return result;
        }
    }
    const String links = projectSharedLinksPath(id);
    return SD.exists(links) ? OperationResult{true, ""}
                            : writeEmptyAtomicSdFile(links);
}

ProjectDocumentResult failedProjectCreation(const String& id, const String& error)
{
    if (!SD.exists(projectDirectoryPath(id))) {
        return {false, {}, error};
    }
    const OperationResult cleanup = removeSdDirectoryTree(projectDirectoryPath(id));
    return cleanup.success
        ? ProjectDocumentResult{false, {}, error}
        : ProjectDocumentResult{
            false, {}, error + "; partial project cleanup also failed: " + cleanup.error};
}

OperationResult restoreIndexEntry(const String& path,
                                  const String& keyField,
                                  const String& keyValue,
                                  const StorageIndexLookupResult& previous)
{
    return mutateJsonlSdIndex(path, keyField, keyValue, previous.line,
                              !previous.found, kStorageOperationalFloorBytes);
}

}  // namespace

String projectStorageRoot()
{
    return kStorageRoot;
}

String projectDirectoryPath(const String& id)
{
    return String(kProjectsRoot) + "/" + id;
}

String projectChatsDirectoryPath(const String& id)
{
    return projectDirectoryPath(id) + "/chats";
}

String projectChatDirectoryPath(const String& projectId, const String& chatId)
{
    return projectChatsDirectoryPath(projectId) + "/" + chatId;
}

OperationResult initializeProjectStorage()
{
    if (SD.cardType() == CARD_NONE) {
        return {false, "microSD is required for project storage"};
    }
    OperationResult result = ensureSdDirectory(kAssistantDirectory);
    if (!result.success) {
        return result;
    }
    result = ensureSdDirectory(kSharedRoot);
    if (!result.success) {
        return result;
    }
    result = ensureSdDirectory(kStorageRoot);
    if (!result.success) {
        return result;
    }
    result = ensureSdDirectory(kProjectsRoot);
    if (!result.success) {
        return result;
    }
    result = recoverAtomicSdFile(kProjectsIndex);
    if (!result.success) {
        return result;
    }
    if (!SD.exists(kProjectsIndex)) {
        result = writeEmptyAtomicSdFile(kProjectsIndex);
        if (!result.success) {
            return result;
        }
    }
    result = recoverAtomicSdFile(kManifestPath);
    if (!result.success || SD.exists(kManifestPath)) {
        return result;
    }
    return saveProjectStorageManifest({
        kProjectStorageFormatVersion,
        ProjectMigrationState::Uninitialized,
        "",
        kSharedRoot,
        0,
    });
}

ProjectStorageManifestResult loadProjectStorageManifest()
{
    File file = SD.open(kManifestPath, FILE_READ);
    if (!file) {
        return {false, {}, "Project storage manifest does not exist"};
    }
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, file);
    file.close();
    if (error || !document["version"].is<std::uint32_t>() ||
        !document["migration_state"].is<const char*>() ||
        !document["active_project_id"].is<const char*>() ||
        !document["shared_root"].is<const char*>() ||
        !document["revision"].is<std::uint32_t>()) {
        return {false, {}, "Project storage manifest is missing required typed fields"};
    }
    ProjectStorageManifest manifest;
    manifest.version = document["version"].as<std::uint32_t>();
    manifest.activeProjectId = document["active_project_id"].as<const char*>();
    manifest.sharedRoot = document["shared_root"].as<const char*>();
    manifest.revision = document["revision"].as<std::uint32_t>();
    if (manifest.version != kProjectStorageFormatVersion ||
        !parseMigrationState(document["migration_state"].as<const char*>(),
                             manifest.migrationState) ||
        manifest.sharedRoot != kSharedRoot ||
        (!manifest.activeProjectId.isEmpty() &&
         !isValidChatId(manifest.activeProjectId.c_str()))) {
        return {false, {}, "Project storage manifest contains unsupported or invalid values"};
    }
    return {true, manifest, ""};
}

OperationResult saveProjectStorageManifest(const ProjectStorageManifest& manifest)
{
    if (manifest.version != kProjectStorageFormatVersion ||
        manifest.sharedRoot != kSharedRoot ||
        (!manifest.activeProjectId.isEmpty() &&
         !isValidChatId(manifest.activeProjectId.c_str())) ||
        String(migrationStateName(manifest.migrationState)) == "invalid") {
        return {false, "Cannot save an invalid project storage manifest"};
    }
    JsonDocument document;
    document["version"] = manifest.version;
    document["migration_state"] = migrationStateName(manifest.migrationState);
    document["active_project_id"] = manifest.activeProjectId;
    document["shared_root"] = manifest.sharedRoot;
    document["revision"] = manifest.revision;
    return writeAtomicJsonSdFile(kManifestPath, document);
}

OperationResult resetUncommittedProjectStorage()
{
    const ProjectStorageManifestResult loaded = loadProjectStorageManifest();
    if (!loaded.success) {
        return {false, loaded.error};
    }
    if (loaded.manifest.migrationState == ProjectMigrationState::Committed) {
        return {false, "Committed project storage cannot be reset by migration"};
    }
    if (SD.exists(kProjectsRoot)) {
        const OperationResult removed = removeSdDirectoryTree(kProjectsRoot);
        if (!removed.success) {
            return removed;
        }
    }
    OperationResult result = ensureSdDirectory(kProjectsRoot);
    if (!result.success) {
        return result;
    }
    result = writeEmptyAtomicSdFile(kProjectsIndex);
    if (!result.success) {
        return result;
    }
    ProjectStorageManifest reset = loaded.manifest;
    reset.migrationState = ProjectMigrationState::Uninitialized;
    reset.activeProjectId = "";
    ++reset.revision;
    return saveProjectStorageManifest(reset);
}

ProjectDocumentResult createProject(const String& title)
{
    if (title.isEmpty() || title.length() > kMaximumProjectTitleBytes ||
        !isValidUtf8(title.c_str())) {
        return {false, {}, "Project title must be valid UTF-8 and contain 1 to 120 bytes"};
    }
    String id;
    for (std::size_t attempt = 0; attempt < 8; ++attempt) {
        id = generateStorageId();
        if (!SD.exists(projectDirectoryPath(id))) {
            break;
        }
        id = "";
    }
    if (id.isEmpty()) {
        return {false, {}, "Failed to generate a unique project id after 8 attempts"};
    }
    return createProjectWithId(title, id);
}

ProjectDocumentResult createProjectWithId(const String& title, const String& id)
{
    if (title.isEmpty() || title.length() > kMaximumProjectTitleBytes ||
        !isValidUtf8(title.c_str())) {
        return {false, {}, "Project title must be valid UTF-8 and contain 1 to 120 bytes"};
    }
    if (!isValidChatId(id.c_str())) {
        return {false, {}, "Cannot create a project with an invalid id"};
    }
    if (SD.exists(projectDirectoryPath(id))) {
        return {false, {}, "Project id already exists: " + id};
    }
    ProjectDocument project;
    project.summary = {id, title, currentTimestamp(), 0, false, false, 1};
    OperationResult result = createProjectDirectories(id);
    if (!result.success) {
        return failedProjectCreation(id, result.error);
    }
    result = writeProjectDocument(project);
    if (!result.success) {
        return failedProjectCreation(id, result.error);
    }
    result = mutateJsonlSdIndex(kProjectsIndex, "id", id,
                                serializeProjectSummary(project.summary), false,
                                kStorageOperationalFloorBytes);
    if (!result.success) {
        return failedProjectCreation(id, result.error);
    }
    return {true, std::move(project), ""};
}

ProjectDocumentResult loadProject(const String& id)
{
    if (!isValidChatId(id.c_str())) {
        return {false, {}, "Cannot load project: invalid project id"};
    }
    const String path = projectMetadataPath(id);
    const OperationResult recovered = recoverAtomicSdFile(path);
    if (!recovered.success) {
        return {false, {}, recovered.error};
    }
    File file = SD.open(path, FILE_READ);
    if (!file) {
        return {false, {}, "Project metadata does not exist for id " + id};
    }
    ProjectDocumentResult result = parseProjectDocument(file, path);
    file.close();
    if (result.success && result.project.summary.id != id) {
        return {false, {}, "Project directory id does not match project metadata"};
    }
    return result;
}

OperationResult saveProject(const ProjectDocument& project)
{
    const OperationResult validation = validateProjectDocument(project);
    if (!validation.success) {
        return validation;
    }
    const ProjectDocumentResult previousDocument = loadProject(project.summary.id);
    if (!previousDocument.success) {
        return {false, previousDocument.error};
    }
    const StorageIndexLookupResult previousIndex = findJsonlSdIndexEntry(
        kProjectsIndex, "id", project.summary.id);
    if (!previousIndex.success || !previousIndex.found) {
        return {false, previousIndex.success
            ? String("Project index entry is missing for id ") + project.summary.id
            : previousIndex.error};
    }
    ProjectSummary updatedSummary = project.summary;
    updatedSummary.updatedAt = currentTimestamp();
    ++updatedSummary.revision;
    OperationResult result = writeProjectDocumentWithSummary(project, updatedSummary);
    if (!result.success) {
        return result;
    }
    result = mutateJsonlSdIndex(kProjectsIndex, "id", updatedSummary.id,
                                serializeProjectSummary(updatedSummary), false,
                                kStorageOperationalFloorBytes);
    if (!result.success) {
        const OperationResult rollback = writeProjectDocument(previousDocument.project);
        return rollback.success
            ? result
            : OperationResult{false, result.error + "; project metadata rollback also failed: " +
                                       rollback.error};
    }
    return {true, ""};
}

OperationResult deleteProject(const String& id)
{
    const StorageIndexLookupResult previous = findJsonlSdIndexEntry(kProjectsIndex, "id", id);
    if (!previous.success || !previous.found) {
        return {false, previous.success ? String("Project index entry is missing") : previous.error};
    }
    OperationResult result = mutateJsonlSdIndex(kProjectsIndex, "id", id, "", true,
                                                kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    result = removeSdDirectoryTree(projectDirectoryPath(id));
    if (!result.success) {
        const OperationResult rollback = restoreIndexEntry(kProjectsIndex, "id", id, previous);
        return rollback.success
            ? result
            : OperationResult{false, result.error + "; project index rollback also failed: " +
                                       rollback.error};
    }
    return {true, ""};
}

OperationResult renameProject(const String& id, const String& title)
{
    if (title.isEmpty() || title.length() > kMaximumProjectTitleBytes ||
        !isValidUtf8(title.c_str())) {
        return {false, "Project title must be valid UTF-8 and contain 1 to 120 bytes"};
    }
    ProjectDocumentResult loaded = loadProject(id);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    loaded.project.summary.title = title;
    return saveProject(loaded.project);
}

OperationResult setProjectArchived(const String& id, bool archived)
{
    ProjectDocumentResult loaded = loadProject(id);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    loaded.project.summary.archived = archived;
    return saveProject(loaded.project);
}

ProjectDocumentResult duplicateProject(const String& id, const String& title)
{
    const ProjectDocumentResult source = loadProject(id);
    if (!source.success) {
        return source;
    }
    const ProjectDocumentResult created = createProject(title);
    if (!created.success) {
        return created;
    }
    ProjectDocument destination = created.project;
    destination.instructions = source.project.instructions;
    destination.activeChatId = source.project.activeChatId;
    destination.model = source.project.model;
    destination.apiProfile = source.project.apiProfile;
    destination.toolPolicy = source.project.toolPolicy;
    destination.sshProfile = source.project.sshProfile;
    destination.contextByteBudget = source.project.contextByteBudget;
    destination.maximumOutputTokens = source.project.maximumOutputTokens;
    destination.automaticCompaction = source.project.automaticCompaction;
    OperationResult result = saveProject(destination);
    std::uint32_t chatOffset = 0;
    bool chatsEof = false;
    while (result.success && !chatsEof) {
        const ProjectChatsPageResult page = listProjectChatsPage(
            id, chatOffset, kMaximumProjectPageEntries);
        if (!page.success) {
            result = {false, page.error};
            break;
        }
        for (const ChatSummary& chat : page.chats) {
            result = cloneProjectChat(id, destination.summary.id, chat.id);
            if (!result.success) {
                break;
            }
        }
        if (!page.eof && page.nextOffset <= chatOffset) {
            result = {false, "Source project chat pagination did not advance"};
            break;
        }
        chatOffset = page.nextOffset;
        chatsEof = page.eof;
    }
    std::uint32_t linkOffset = 0;
    bool linksEof = false;
    while (result.success && !linksEof) {
        const SharedFileLinksPageResult page = listProjectSharedLinksPage(
            id, linkOffset, kMaximumProjectPageEntries);
        if (!page.success) {
            result = {false, page.error};
            break;
        }
        for (const SharedFileLink& link : page.links) {
            result = linkSharedFileToProject(destination.summary.id, link.path);
            if (!result.success) {
                break;
            }
        }
        if (!page.eof && page.nextOffset <= linkOffset) {
            result = {false, "Source project Shared-link pagination did not advance"};
            break;
        }
        linkOffset = page.nextOffset;
        linksEof = page.eof;
    }
    if (!result.success) {
        const OperationResult cleanup = deleteProject(destination.summary.id);
        return cleanup.success
            ? ProjectDocumentResult{false, {}, result.error}
            : ProjectDocumentResult{
                false, {}, result.error + "; duplicate project cleanup also failed: " +
                                     cleanup.error};
    }
    return loadProject(destination.summary.id);
}

ProjectsPageResult listProjectsPage(std::uint32_t offset, std::size_t maximumEntries)
{
    if (maximumEntries == 0 || maximumEntries > kMaximumProjectPageEntries) {
        return {false, {}, offset, false, "Project page size must be between 1 and 32"};
    }
    const StorageLinesPageResult page = readJsonlSdIndexPage(
        kProjectsIndex, offset, maximumEntries);
    if (!page.success) {
        return {false, {}, offset, false, page.error};
    }
    std::vector<ProjectSummary> projects;
    projects.reserve(page.lines.size());
    for (const String& line : page.lines) {
        const ProjectDocumentResult parsed = parseProjectSummaryLine(line);
        if (!parsed.success) {
            return {false, {}, offset, false, parsed.error};
        }
        projects.push_back(parsed.project.summary);
    }
    return {true, std::move(projects), page.nextOffset, page.eof, ""};
}

OperationResult upsertProjectChatSummary(const String& projectId,
                                         const ChatSummary& summary)
{
    if (!isValidChatId(projectId.c_str()) || !isValidChatId(summary.id.c_str()) ||
        summary.title.isEmpty() || !isValidUtf8(summary.title.c_str())) {
        return {false, "Cannot index invalid project or chat metadata"};
    }
    ProjectDocumentResult project = loadProjectCore(projectId);
    if (!project.success) {
        return {false, project.error};
    }
    const String indexPath = projectChatsIndexPath(projectId);
    const StorageIndexLookupResult previous = findJsonlSdIndexEntry(indexPath, "id", summary.id);
    if (!previous.success) {
        return {false, previous.error};
    }
    OperationResult result = mutateJsonlSdIndex(indexPath, "id", summary.id,
                                                serializeChatSummary(summary), false,
                                                kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    if (!previous.found) {
        ++project.project.summary.chatCount;
    }
    ++project.project.chatIndexRevision;
    project.project.summary.updatedAt = currentTimestamp();
    ++project.project.summary.revision;
    result = persistProjectCounterUpdate(project.project);
    if (!result.success) {
        const OperationResult rollback = restoreIndexEntry(indexPath, "id", summary.id, previous);
        return rollback.success
            ? result
            : OperationResult{false, result.error + "; chat index rollback also failed: " +
                                       rollback.error};
    }
    return {true, ""};
}

OperationResult removeProjectChatSummary(const String& projectId, const String& chatId)
{
    if (!isValidChatId(projectId.c_str()) || !isValidChatId(chatId.c_str())) {
        return {false, "Cannot remove invalid project or chat id from index"};
    }
    ProjectDocumentResult project = loadProjectCore(projectId);
    if (!project.success) {
        return {false, project.error};
    }
    const String indexPath = projectChatsIndexPath(projectId);
    const StorageIndexLookupResult previous = findJsonlSdIndexEntry(indexPath, "id", chatId);
    if (!previous.success || !previous.found) {
        return {false, previous.success ? String("Project chat index entry is missing")
                                       : previous.error};
    }
    OperationResult result = mutateJsonlSdIndex(indexPath, "id", chatId, "", true,
                                                kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    if (project.project.summary.chatCount > 0) {
        --project.project.summary.chatCount;
    }
    ++project.project.chatIndexRevision;
    project.project.summary.updatedAt = currentTimestamp();
    ++project.project.summary.revision;
    result = persistProjectCounterUpdate(project.project);
    if (!result.success) {
        const OperationResult rollback = restoreIndexEntry(indexPath, "id", chatId, previous);
        return rollback.success
            ? result
            : OperationResult{false, result.error + "; chat index rollback also failed: " +
                                       rollback.error};
    }
    return {true, ""};
}

ProjectChatsPageResult listProjectChatsPage(const String& projectId,
                                            std::uint32_t offset,
                                            std::size_t maximumEntries)
{
    if (!isValidChatId(projectId.c_str())) {
        return {false, {}, offset, false, "Cannot list chats for an invalid project id"};
    }
    if (maximumEntries == 0 || maximumEntries > kMaximumProjectPageEntries) {
        return {false, {}, offset, false, "Project chat page size must be between 1 and 32"};
    }
    const StorageLinesPageResult page = readJsonlSdIndexPage(
        projectChatsIndexPath(projectId), offset, maximumEntries);
    if (!page.success) {
        return {false, {}, offset, false, page.error};
    }
    std::vector<ChatSummary> chats;
    chats.reserve(page.lines.size());
    for (const String& line : page.lines) {
        const ChatDocumentResult parsed = parseChatSummaryLine(line);
        if (!parsed.success) {
            return {false, {}, offset, false, parsed.error};
        }
        chats.push_back(parsed.chat.summary);
    }
    return {true, std::move(chats), page.nextOffset, page.eof, ""};
}

OperationResult linkSharedFileToProject(const String& projectId, const String& path)
{
    if (!isValidChatId(projectId.c_str()) ||
        !isValidStorageRelativePath(std::string(path.c_str()), 512)) {
        return {false, "Cannot link an invalid project id or Shared path"};
    }
    ProjectDocumentResult project = loadProject(projectId);
    if (!project.success) {
        return {false, project.error};
    }
    JsonDocument item;
    item["path"] = path;
    String line;
    serializeJson(item, line);
    const String indexPath = projectSharedLinksPath(projectId);
    const StorageIndexLookupResult previous = findJsonlSdIndexEntry(indexPath, "path", path);
    if (!previous.success) {
        return {false, previous.error};
    }
    if (previous.found) {
        return {true, ""};
    }
    OperationResult result = mutateJsonlSdIndex(indexPath, "path", path, line, false,
                                                kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    ++project.project.sharedLinksRevision;
    result = saveProject(project.project);
    if (!result.success) {
        const OperationResult rollback = restoreIndexEntry(indexPath, "path", path, previous);
        return rollback.success
            ? result
            : OperationResult{false, result.error + "; Shared link rollback also failed: " +
                                       rollback.error};
    }
    return {true, ""};
}

OperationResult unlinkSharedFileFromProject(const String& projectId, const String& path)
{
    if (!isValidChatId(projectId.c_str()) ||
        !isValidStorageRelativePath(std::string(path.c_str()), 512)) {
        return {false, "Cannot unlink an invalid project id or Shared path"};
    }
    ProjectDocumentResult project = loadProject(projectId);
    if (!project.success) {
        return {false, project.error};
    }
    const String indexPath = projectSharedLinksPath(projectId);
    const StorageIndexLookupResult previous = findJsonlSdIndexEntry(indexPath, "path", path);
    if (!previous.success || !previous.found) {
        return {false, previous.success ? String("Shared file is not linked to this project")
                                       : previous.error};
    }
    OperationResult result = mutateJsonlSdIndex(indexPath, "path", path, "", true,
                                                kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    ++project.project.sharedLinksRevision;
    result = saveProject(project.project);
    if (!result.success) {
        const OperationResult rollback = restoreIndexEntry(indexPath, "path", path, previous);
        return rollback.success
            ? result
            : OperationResult{false, result.error + "; Shared unlink rollback also failed: " +
                                       rollback.error};
    }
    return {true, ""};
}

SharedFileLinksPageResult listProjectSharedLinksPage(const String& projectId,
                                                     std::uint32_t offset,
                                                     std::size_t maximumEntries)
{
    if (!isValidChatId(projectId.c_str())) {
        return {false, {}, offset, false, "Cannot list Shared links for an invalid project id"};
    }
    if (maximumEntries == 0 || maximumEntries > kMaximumProjectPageEntries) {
        return {false, {}, offset, false, "Shared link page size must be between 1 and 32"};
    }
    const StorageLinesPageResult page = readJsonlSdIndexPage(
        projectSharedLinksPath(projectId), offset, maximumEntries);
    if (!page.success) {
        return {false, {}, offset, false, page.error};
    }
    std::vector<SharedFileLink> links;
    links.reserve(page.lines.size());
    for (const String& line : page.lines) {
        JsonDocument document;
        const DeserializationError error = deserializeJson(document, line);
        if (error || !document["path"].is<const char*>()) {
            return {false, {}, offset, false,
                    "Shared link index contains an invalid typed entry"};
        }
        const String path = document["path"].as<const char*>();
        if (!isValidStorageRelativePath(std::string(path.c_str()), 512)) {
            return {false, {}, offset, false, "Shared link index contains an invalid path"};
        }
        links.push_back({path});
    }
    return {true, std::move(links), page.nextOffset, page.eof, ""};
}

SharedFileLinkResult projectHasSharedFileLink(const String& projectId, const String& path)
{
    if (!isValidChatId(projectId.c_str()) ||
        !isValidStorageRelativePath(std::string(path.c_str()), 512)) {
        return {false, false, "Cannot query an invalid project id or Shared path"};
    }
    const StorageIndexLookupResult result = findJsonlSdIndexEntry(
        projectSharedLinksPath(projectId), "path", path);
    return result.success ? SharedFileLinkResult{true, result.found, ""}
                          : SharedFileLinkResult{false, false, result.error};
}

SharedFileLinkResult sharedFileHasAnyProjectLink(const String& path)
{
    if (!isValidStorageRelativePath(std::string(path.c_str()), 512)) {
        return {false, false, "Cannot query an invalid Shared path"};
    }
    std::uint32_t offset = 0;
    bool eof = false;
    while (!eof) {
        const ProjectsPageResult page = listProjectsPage(
            offset, kMaximumProjectPageEntries);
        if (!page.success) {
            return {false, false, page.error};
        }
        for (const ProjectSummary& project : page.projects) {
            const SharedFileLinkResult linked = projectHasSharedFileLink(project.id, path);
            if (!linked.success || linked.linked) {
                return linked;
            }
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, false,
                    "Project pagination did not advance while checking Shared links"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    return {true, false, ""};
}

}  // namespace cardputer
