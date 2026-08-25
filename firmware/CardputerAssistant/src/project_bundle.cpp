#include "project_bundle.h"

#include "project_chat_storage.h"
#include "project_storage.h"
#include "sd_storage.h"
#include "text_utils.h"

#include <ArduinoJson.h>
#include <SD.h>

#include <cstdint>
#include <string>

namespace cardputer {
namespace {

constexpr const char* kSharedRoot = "/assistant/files";

bool isValidProjectBundleFilename(const String& filename)
{
    return filename.endsWith(".cardmind-project.jsonl") &&
        filename.indexOf('/') < 0 && filename.indexOf('\\') < 0 &&
        isValidStorageRelativePath(std::string(filename.c_str()), 180);
}

String projectBundlePath(const String& filename)
{
    return String(kSharedRoot) + "/" + filename;
}

JsonDocument buildProjectHeader(const ProjectDocument& project,
                                std::uint32_t linkCount)
{
    JsonDocument header;
    header["record"] = "project";
    header["format"] = 1;
    header["title"] = project.summary.title;
    header["pinned"] = project.summary.pinned;
    header["archived"] = project.summary.archived;
    header["instructions"] = project.instructions;
    header["active_chat_id"] = project.activeChatId;
    header["model"] = project.model;
    header["api_profile"] = project.apiProfile;
    header["tool_policy"] = project.toolPolicy;
    header["ssh_profile"] = project.sshProfile;
    header["context_byte_budget"] = project.contextByteBudget;
    header["maximum_output_tokens"] = project.maximumOutputTokens;
    header["automatic_compaction"] = project.automaticCompaction;
    header["chat_count"] = project.summary.chatCount;
    header["shared_link_count"] = linkCount;
    return header;
}

OperationResult writeJsonLine(File& output, JsonDocument& document, const String& label)
{
    const std::size_t expectedBytes = measureJson(document);
    if (serializeJson(document, output) != expectedBytes || output.write('\n') != 1) {
        return {false, "Failed to write complete " + label + " record"};
    }
    return {true, ""};
}

struct SharedLinksMeasurement {
    bool success;
    std::uint32_t count;
    std::uint64_t bytes;
    String error;
};

SharedLinksMeasurement measureSharedLinks(const String& projectId)
{
    std::uint32_t offset = 0;
    std::uint32_t count = 0;
    std::uint64_t bytes = 0;
    bool eof = false;
    while (!eof) {
        const SharedFileLinksPageResult page = listProjectSharedLinksPage(
            projectId, offset, kMaximumProjectPageEntries);
        if (!page.success) {
            return {false, 0, 0, page.error};
        }
        for (const SharedFileLink& link : page.links) {
            JsonDocument record;
            record["record"] = "shared_link";
            record["path"] = link.path;
            bytes += measureJson(record) + 1;
            ++count;
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, 0, 0, "Shared-link pagination did not advance"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    return {true, count, bytes, ""};
}

StorageSizeResult measureProjectChats(const ProjectDocument& project)
{
    std::uint32_t offset = 0;
    std::uint32_t count = 0;
    std::uint64_t bytes = 0;
    bool eof = false;
    while (!eof) {
        const ProjectChatsPageResult page = listProjectChatsPage(
            project.summary.id, offset, kMaximumProjectPageEntries);
        if (!page.success) {
            return {false, 0, page.error};
        }
        for (const ChatSummary& chat : page.chats) {
            const StorageSizeResult measured = measureProjectChatBundleRecords(
                project.summary.id, chat.id);
            if (!measured.success) {
                return measured;
            }
            bytes += measured.bytes;
            ++count;
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, 0, "Project chat pagination did not advance"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    return count == project.summary.chatCount
        ? StorageSizeResult{true, bytes, ""}
        : StorageSizeResult{false, 0, "Project chat count changed during bundle measurement"};
}

OperationResult writeSharedLinks(File& output, const String& projectId)
{
    std::uint32_t offset = 0;
    bool eof = false;
    while (!eof) {
        const SharedFileLinksPageResult page = listProjectSharedLinksPage(
            projectId, offset, kMaximumProjectPageEntries);
        if (!page.success) {
            return {false, page.error};
        }
        for (const SharedFileLink& link : page.links) {
            JsonDocument record;
            record["record"] = "shared_link";
            record["path"] = link.path;
            const OperationResult written = writeJsonLine(output, record, "Shared link");
            if (!written.success) {
                return written;
            }
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, "Shared-link pagination did not advance"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    return {true, ""};
}

OperationResult writeProjectChats(File& output, const ProjectDocument& project)
{
    std::uint32_t offset = 0;
    std::uint32_t count = 0;
    bool eof = false;
    while (!eof) {
        const ProjectChatsPageResult page = listProjectChatsPage(
            project.summary.id, offset, kMaximumProjectPageEntries);
        if (!page.success) {
            return {false, page.error};
        }
        for (const ChatSummary& chat : page.chats) {
            const OperationResult written = writeProjectChatBundleRecords(
                output, project.summary.id, chat.id);
            if (!written.success) {
                return written;
            }
            ++count;
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, "Project chat pagination did not advance"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    return count == project.summary.chatCount
        ? OperationResult{true, ""}
        : OperationResult{false, "Project chat count changed during bundle export"};
}

ProjectDocumentResult cleanupFailedProjectImport(const String& projectId, const String& error)
{
    const OperationResult cleanup = deleteProject(projectId);
    return cleanup.success
        ? ProjectDocumentResult{false, {}, error}
        : ProjectDocumentResult{false, {}, error + "; imported project cleanup also failed: " +
                                             cleanup.error};
}

}  // namespace

OperationResult exportProjectBundle(const String& projectId, const String& filename)
{
    if (!isValidProjectBundleFilename(filename)) {
        return {false, "Project bundle filename must end with .cardmind-project.jsonl"};
    }
    const ProjectDocumentResult loaded = loadProject(projectId);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    const SharedLinksMeasurement links = measureSharedLinks(projectId);
    if (!links.success) {
        return {false, links.error};
    }
    const StorageSizeResult chats = measureProjectChats(loaded.project);
    if (!chats.success) {
        return {false, chats.error};
    }
    JsonDocument header = buildProjectHeader(loaded.project, links.count);
    const std::uint64_t expectedBytes = measureJson(header) + 1 + links.bytes + chats.bytes;
    if (expectedBytes > kMaximumProjectBundleBytes) {
        return {false, "Project bundle exceeds the 256 MiB portable bundle limit"};
    }
    OperationResult result = checkSdOperationSpace(expectedBytes, kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    const String target = projectBundlePath(filename);
    result = recoverAtomicSdFile(target);
    if (!result.success) {
        return result;
    }
    const String stagedPath = target + ".tmp";
    File output = SD.open(stagedPath, FILE_WRITE);
    if (!output) {
        return {false, "Failed to create staged project bundle"};
    }
    result = writeJsonLine(output, header, "project header");
    if (result.success) {
        result = writeSharedLinks(output, projectId);
    }
    if (result.success) {
        result = writeProjectChats(output, loaded.project);
    }
    output.flush();
    const std::uint64_t writtenBytes = output.size();
    output.close();
    if (result.success && writtenBytes != expectedBytes) {
        result = {false, "Project bundle changed while it was being exported"};
    }
    if (!result.success) {
        if (SD.exists(stagedPath) && !SD.remove(stagedPath)) {
            return {false, result.error + "; staged project bundle cleanup also failed"};
        }
        return result;
    }
    return commitStagedSdFile(target, stagedPath);
}

ProjectDocumentResult importProjectBundle(const String& filename)
{
    if (!isValidProjectBundleFilename(filename)) {
        return {false, {}, "Project bundle filename must end with .cardmind-project.jsonl"};
    }
    File input = SD.open(projectBundlePath(filename), FILE_READ);
    if (!input) {
        return {false, {}, "Project bundle does not exist in Shared workspace"};
    }
    if (input.size() > kMaximumProjectBundleBytes) {
        input.close();
        return {false, {}, "Project bundle exceeds the 256 MiB portable bundle limit"};
    }
    const OperationResult space = checkSdOperationSpace(
        input.size(), kStorageOperationalFloorBytes);
    if (!space.success) {
        input.close();
        return {false, {}, space.error};
    }
    const String headerLine = input.readStringUntil('\n');
    JsonDocument header;
    const DeserializationError error = deserializeJson(header, headerLine);
    if (error || !header["record"].is<const char*>() ||
        String(header["record"].as<const char*>()) != "project" ||
        !header["format"].is<std::uint32_t>() || header["format"].as<std::uint32_t>() != 1 ||
        !header["title"].is<const char*>() || !header["pinned"].is<bool>() ||
        !header["archived"].is<bool>() || !header["instructions"].is<const char*>() ||
        !header["active_chat_id"].is<const char*>() ||
        !header["model"].is<const char*>() || !header["api_profile"].is<const char*>() ||
        !header["tool_policy"].is<const char*>() || !header["ssh_profile"].is<const char*>() ||
        !header["context_byte_budget"].is<std::uint32_t>() ||
        !header["maximum_output_tokens"].is<std::uint32_t>() ||
        !header["automatic_compaction"].is<bool>() ||
        !header["chat_count"].is<std::uint32_t>() ||
        !header["shared_link_count"].is<std::uint32_t>()) {
        input.close();
        return {false, {}, "Project bundle header is invalid or unsupported"};
    }
    const ProjectDocumentResult created = createProject(header["title"].as<const char*>());
    if (!created.success) {
        input.close();
        return created;
    }
    ProjectDocument imported = created.project;
    imported.summary.pinned = header["pinned"].as<bool>();
    imported.summary.archived = header["archived"].as<bool>();
    imported.instructions = header["instructions"].as<const char*>();
    imported.activeChatId = header["active_chat_id"].as<const char*>();
    imported.model = header["model"].as<const char*>();
    imported.apiProfile = header["api_profile"].as<const char*>();
    imported.toolPolicy = header["tool_policy"].as<const char*>();
    imported.sshProfile = header["ssh_profile"].as<const char*>();
    imported.contextByteBudget = header["context_byte_budget"].as<std::uint32_t>();
    imported.maximumOutputTokens = header["maximum_output_tokens"].as<std::uint32_t>();
    imported.automaticCompaction = header["automatic_compaction"].as<bool>();
    OperationResult result = saveProject(imported);
    const std::uint32_t linkCount = header["shared_link_count"].as<std::uint32_t>();
    for (std::uint32_t index = 0; index < linkCount && result.success; ++index) {
        if (!input.available()) {
            result = {false, "Project bundle ended before all Shared links were read"};
            break;
        }
        const String line = input.readStringUntil('\n');
        JsonDocument record;
        const DeserializationError recordError = deserializeJson(record, line);
        if (recordError || !record["record"].is<const char*>() ||
            String(record["record"].as<const char*>()) != "shared_link" ||
            !record["path"].is<const char*>()) {
            result = {false, "Project bundle contains an invalid Shared-link record"};
            break;
        }
        result = linkSharedFileToProject(imported.summary.id,
                                         record["path"].as<const char*>());
    }
    const std::uint32_t chatCount = header["chat_count"].as<std::uint32_t>();
    for (std::uint32_t index = 0; index < chatCount && result.success; ++index) {
        if (!input.available()) {
            result = {false, "Project bundle ended before all chats were read"};
            break;
        }
        const String line = input.readStringUntil('\n');
        JsonDocument record;
        const DeserializationError recordError = deserializeJson(record, line);
        if (recordError || !record["record"].is<const char*>() ||
            String(record["record"].as<const char*>()) != "chat" ||
            !record["id"].is<const char*>() || !record["title"].is<const char*>() ||
            !record["updated_at"].is<std::uint64_t>() ||
            !record["message_count"].is<std::uint32_t>() ||
            !record["pinned"].is<bool>() || !record["archived"].is<bool>() ||
            !record["instructions"].is<const char*>() || !record["draft"].is<const char*>() ||
            !record["ssh_tools_enabled"].is<bool>() ||
            !record["context_summary"].is<const char*>() ||
            !record["summarized_message_count"].is<std::uint32_t>()) {
            result = {false, "Project bundle contains an invalid chat header"};
            break;
        }
        ChatDocument chat;
        chat.summary.id = record["id"].as<const char*>();
        chat.summary.title = record["title"].as<const char*>();
        chat.summary.updatedAt = record["updated_at"].as<std::uint64_t>();
        chat.summary.messageCount = record["message_count"].as<std::uint32_t>();
        chat.summary.pinned = record["pinned"].as<bool>();
        chat.summary.archived = record["archived"].as<bool>();
        chat.instructions = record["instructions"].as<const char*>();
        chat.draft = record["draft"].as<const char*>();
        chat.sshToolsEnabled = false;
        chat.contextSummary = record["context_summary"].as<const char*>();
        chat.summarizedMessageCount =
            record["summarized_message_count"].as<std::uint32_t>();
        result = importProjectChatBundleRecords(
            input, imported.summary.id, chat, chat.summary.messageCount);
    }
    while (result.success && input.available()) {
        const String trailing = input.readStringUntil('\n');
        if (!trailing.isEmpty()) {
            result = {false, "Project bundle contains unexpected trailing records"};
        }
    }
    input.close();
    if (!result.success) {
        return cleanupFailedProjectImport(imported.summary.id, result.error);
    }
    const ProjectDocumentResult reloaded = loadProject(imported.summary.id);
    if (!reloaded.success || reloaded.project.summary.chatCount != chatCount) {
        return cleanupFailedProjectImport(
            imported.summary.id,
            reloaded.success ? String("Imported project chat count is incomplete")
                             : reloaded.error);
    }
    return reloaded;
}

}  // namespace cardputer
