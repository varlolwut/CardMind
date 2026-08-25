#include "project_chat_storage.h"

#include "chat_storage.h"
#include "file_workspace.h"
#include "project_storage.h"
#include "sd_storage.h"
#include "text_utils.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <esp_random.h>

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace cardputer {
namespace {

String projectChatMetadataPath(const String& projectId, const String& chatId)
{
    return projectChatDirectoryPath(projectId, chatId) + "/chat.json";
}

String projectChatHistoryPath(const String& projectId, const String& chatId)
{
    return projectChatDirectoryPath(projectId, chatId) + "/history.jsonl";
}

String projectChatTailPath(const String& projectId, const String& chatId)
{
    return projectChatDirectoryPath(projectId, chatId) + "/tail.jsonl";
}

String projectChatAppendMarkerPath(const String& projectId, const String& chatId)
{
    return projectChatDirectoryPath(projectId, chatId) + "/append.pending.json";
}

constexpr std::size_t kStoredTailMessages = 96;
constexpr std::size_t kStoredTailBytes = 131072;

struct SequencedMessage {
    std::uint32_t sequence;
    Message message;
};

OperationResult validateHistoryFile(const String& path, std::uint32_t expectedMessages);
OperationResult recoverPendingAppend(const String& projectId, const String& chatId);

String generateProjectChatId()
{
    char buffer[25];
    std::snprintf(buffer, sizeof(buffer), "%08lx%08lx",
                  static_cast<unsigned long>(esp_random()),
                  static_cast<unsigned long>(esp_random()));
    return String(buffer);
}

OperationResult validateProjectChatMetadataValues(const ChatDocument& chat)
{
    if (!isValidChatId(chat.projectId.c_str()) ||
        !isValidChatId(chat.summary.id.c_str())) {
        return {false, "Project chat metadata contains an invalid id"};
    }
    if (chat.summary.title.isEmpty() || !isValidUtf8(chat.summary.title.c_str())) {
        return {false, "Project chat title must be non-empty valid UTF-8"};
    }
    if (chat.instructions.size() > kMaximumProjectChatInstructionsBytes ||
        chat.draft.size() > kMaximumProjectChatDraftBytes ||
        chat.contextSummary.size() > kMaximumProjectChatSummaryBytes ||
        !isValidUtf8(chat.instructions) || !isValidUtf8(chat.draft) ||
        !isValidUtf8(chat.contextSummary)) {
        return {false, "Project chat text metadata exceeds its limit or is invalid UTF-8"};
    }
    if (chat.summarizedMessageCount > chat.summary.messageCount) {
        return {false, "Project chat summary covers more messages than the chat contains"};
    }
    return {true, ""};
}

OperationResult writeHistoryMessage(File& file,
                                    std::uint32_t sequence,
                                    const Message& message)
{
    if ((message.role != "user" && message.role != "assistant") ||
        message.content.empty() || !isValidUtf8(message.content)) {
        return {false, "Cannot write invalid raw chat history message"};
    }
    JsonDocument item;
    item["sequence"] = sequence;
    item["role"] = message.role;
    item["content"] = message.content;
    const std::size_t expectedBytes = measureJson(item);
    if (serializeJson(item, file) != expectedBytes || file.write('\n') != 1) {
        return {false, "Failed to write complete raw chat history message"};
    }
    return {true, ""};
}

OperationResult writeTailFile(const String& path,
                              const std::deque<SequencedMessage>& messages)
{
    OperationResult result = recoverAtomicSdFile(path);
    if (!result.success) {
        return result;
    }
    const String stagedPath = path + ".tmp";
    File staged = SD.open(stagedPath, FILE_WRITE);
    if (!staged) {
        return {false, "Failed to create staged project chat tail"};
    }
    for (const SequencedMessage& item : messages) {
        result = writeHistoryMessage(staged, item.sequence, item.message);
        if (!result.success) {
            break;
        }
    }
    staged.flush();
    staged.close();
    if (!result.success) {
        SD.remove(stagedPath);
        return result;
    }
    return commitStagedSdFile(path, stagedPath);
}

OperationResult rebuildTailFile(const String& projectId,
                                const String& chatId,
                                std::uint32_t expectedMessages)
{
    File history = SD.open(projectChatHistoryPath(projectId, chatId), FILE_READ);
    if (!history) {
        return {false, "Project chat raw history does not exist"};
    }
    std::deque<SequencedMessage> tail;
    std::size_t tailBytes = 0;
    std::uint32_t expectedSequence = 1;
    while (history.available()) {
        const String line = history.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument record;
        const DeserializationError error = deserializeJson(record, line);
        if (error || !record["sequence"].is<std::uint32_t>() ||
            record["sequence"].as<std::uint32_t>() != expectedSequence ||
            !record["role"].is<const char*>() || !record["content"].is<const char*>()) {
            history.close();
            return {false, "Project chat raw history is invalid while rebuilding its tail"};
        }
        Message message = {record["role"].as<const char*>(),
                           record["content"].as<const char*>()};
        tailBytes += message.content.size();
        tail.push_back({expectedSequence, std::move(message)});
        while (tail.size() > kStoredTailMessages || tailBytes > kStoredTailBytes) {
            tailBytes -= tail.front().message.content.size();
            tail.pop_front();
        }
        ++expectedSequence;
    }
    history.close();
    if (expectedSequence - 1 != expectedMessages) {
        return {false, "Project chat raw history count changed while rebuilding its tail"};
    }
    return writeTailFile(projectChatTailPath(projectId, chatId), tail);
}

struct LegacyHistoryMeasurement {
    bool success;
    std::uint64_t bytes;
    std::uint32_t messages;
    String error;
};

LegacyHistoryMeasurement measureLegacyHistory(const ChatDocument& legacyChat)
{
    std::uint64_t bytes = 0;
    std::uint32_t sequence = 1;
    std::uint32_t archiveOffset = 0;
    bool archiveEof = false;
    while (!archiveEof) {
        const ArchivedMessagesPageResult page = readArchivedChatMessages(
            legacyChat.summary.id, archiveOffset, 16, 32768);
        if (!page.success) {
            return {false, 0, 0, page.error};
        }
        for (const Message& message : page.messages) {
            JsonDocument item;
            item["sequence"] = sequence;
            item["role"] = message.role;
            item["content"] = message.content;
            bytes += measureJson(item) + 1;
            ++sequence;
        }
        archiveOffset = page.nextOffset;
        archiveEof = page.eof;
    }
    for (const Message& message : legacyChat.messages) {
        JsonDocument item;
        item["sequence"] = sequence;
        item["role"] = message.role;
        item["content"] = message.content;
        bytes += measureJson(item) + 1;
        ++sequence;
    }
    return {true, bytes, sequence - 1, ""};
}

ChatDocumentResult parseProjectChatMetadata(File& file,
                                            const String& projectId,
                                            const String& chatId)
{
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, file);
    if (error || !document["version"].is<std::uint32_t>() ||
        document["version"].as<std::uint32_t>() != kProjectChatFormatVersion ||
        !document["project_id"].is<const char*>() || !document["id"].is<const char*>() ||
        !document["title"].is<const char*>() ||
        !document["updated_at"].is<std::uint64_t>() ||
        !document["message_count"].is<std::uint32_t>() ||
        !document["pinned"].is<bool>() || !document["archived"].is<bool>() ||
        !document["revision"].is<std::uint32_t>() ||
        !document["instructions"].is<const char*>() ||
        !document["draft"].is<const char*>() ||
        !document["ssh_tools_enabled"].is<bool>() ||
        !document["context_summary"].is<const char*>() ||
        !document["summarized_message_count"].is<std::uint32_t>()) {
        return {false, {}, "Project chat metadata is missing required typed fields"};
    }
    ChatDocument chat;
    chat.projectId = document["project_id"].as<const char*>();
    chat.summary.id = document["id"].as<const char*>();
    chat.summary.title = document["title"].as<const char*>();
    chat.summary.updatedAt = document["updated_at"].as<std::uint64_t>();
    chat.summary.messageCount = document["message_count"].as<std::uint32_t>();
    chat.summary.pinned = document["pinned"].as<bool>();
    chat.summary.archived = document["archived"].as<bool>();
    chat.summary.revision = document["revision"].as<std::uint32_t>();
    chat.instructions = document["instructions"].as<const char*>();
    chat.draft = document["draft"].as<const char*>();
    chat.sshToolsEnabled = document["ssh_tools_enabled"].as<bool>();
    chat.contextSummary = document["context_summary"].as<const char*>();
    chat.summarizedMessageCount = document["summarized_message_count"].as<std::uint32_t>();
    if (chat.projectId != projectId || chat.summary.id != chatId ||
        !isValidChatId(chat.projectId.c_str()) || !isValidChatId(chat.summary.id.c_str()) ||
        chat.summary.title.isEmpty() || !isValidUtf8(chat.summary.title.c_str()) ||
        !isValidUtf8(chat.instructions) || !isValidUtf8(chat.draft) ||
        !isValidUtf8(chat.contextSummary) ||
        chat.summarizedMessageCount > chat.summary.messageCount) {
        return {false, {}, "Project chat metadata contains invalid values"};
    }
    return {true, chat, ""};
}

OperationResult writeProjectChatMetadata(const ChatDocument& chat)
{
    const OperationResult validation = validateProjectChatMetadataValues(chat);
    if (!validation.success) {
        return validation;
    }
    JsonDocument document;
    document["version"] = kProjectChatFormatVersion;
    document["project_id"] = chat.projectId;
    document["id"] = chat.summary.id;
    document["title"] = chat.summary.title;
    document["updated_at"] = chat.summary.updatedAt;
    document["message_count"] = chat.summary.messageCount;
    document["pinned"] = chat.summary.pinned;
    document["archived"] = chat.summary.archived;
    document["revision"] = chat.summary.revision;
    document["instructions"] = chat.instructions;
    document["draft"] = chat.draft;
    document["ssh_tools_enabled"] = chat.sshToolsEnabled;
    document["context_summary"] = chat.contextSummary;
    document["summarized_message_count"] = chat.summarizedMessageCount;
    return writeAtomicJsonSdFile(
        projectChatMetadataPath(chat.projectId, chat.summary.id), document);
}

OperationResult validateHistoryFile(const String& path, std::uint32_t expectedMessages)
{
    File history = SD.open(path, FILE_READ);
    if (!history) {
        return {false, "Project chat raw history file does not exist"};
    }
    std::uint32_t expectedSequence = 1;
    while (history.available()) {
        const String line = history.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument item;
        const DeserializationError error = deserializeJson(item, line);
        if (error || !item["sequence"].is<std::uint32_t>() ||
            !item["role"].is<const char*>() || !item["content"].is<const char*>()) {
            history.close();
            return {false, "Project chat raw history contains an invalid typed record"};
        }
        const String role = item["role"].as<const char*>();
        const std::string content = item["content"].as<const char*>();
        if (item["sequence"].as<std::uint32_t>() != expectedSequence ||
            (role != "user" && role != "assistant") || content.empty() ||
            !isValidUtf8(content)) {
            history.close();
            return {false, "Project chat raw history sequence or message is invalid"};
        }
        ++expectedSequence;
    }
    history.close();
    return expectedSequence - 1 == expectedMessages
        ? OperationResult{true, ""}
        : OperationResult{false, "Project chat raw history count does not match metadata"};
}

OperationResult cleanupFailedChatImport(const String& chatDirectory, const String& error)
{
    const OperationResult cleanup = removeSdDirectoryTree(chatDirectory);
    return cleanup.success
        ? OperationResult{false, error}
        : OperationResult{false, error + "; migrated chat cleanup also failed: " +
                                   cleanup.error};
}

OperationResult recoverPendingAppend(const String& projectId, const String& chatId)
{
    const String markerPath = projectChatAppendMarkerPath(projectId, chatId);
    if (!SD.exists(markerPath)) {
        return {true, ""};
    }
    File marker = SD.open(markerPath, FILE_READ);
    if (!marker) {
        return {false, "Pending chat append marker cannot be opened"};
    }
    JsonDocument transaction;
    const DeserializationError markerError = deserializeJson(transaction, marker);
    marker.close();
    if (markerError || !transaction["old_count"].is<std::uint32_t>() ||
        !transaction["new_count"].is<std::uint32_t>() ||
        !transaction["updated_at"].is<std::uint64_t>()) {
        return {false, "Pending chat append marker is invalid"};
    }
    const std::uint32_t oldCount = transaction["old_count"].as<std::uint32_t>();
    const std::uint32_t newCount = transaction["new_count"].as<std::uint32_t>();
    if (newCount <= oldCount) {
        return {false, "Pending chat append marker contains an invalid message range"};
    }
    File metadataFile = SD.open(projectChatMetadataPath(projectId, chatId), FILE_READ);
    if (!metadataFile) {
        return {false, "Pending chat append cannot recover because metadata is missing"};
    }
    ChatDocumentResult metadata = parseProjectChatMetadata(
        metadataFile, projectId, chatId);
    metadataFile.close();
    if (!metadata.success) {
        return {false, "Pending chat append metadata is invalid: " + metadata.error};
    }
    const String historyPath = projectChatHistoryPath(projectId, chatId);
    if (validateHistoryFile(historyPath, oldCount).success) {
        return SD.remove(markerPath)
            ? OperationResult{true, ""}
            : OperationResult{false, "Failed to clear an uncommitted chat append marker"};
    }
    const OperationResult completeHistory = validateHistoryFile(historyPath, newCount);
    if (!completeHistory.success) {
        return {false,
                "Pending chat append raw history is neither the old nor new complete state"};
    }
    OperationResult result = rebuildTailFile(projectId, chatId, newCount);
    if (!result.success) {
        return result;
    }
    metadata.chat.summary.messageCount = newCount;
    metadata.chat.summary.updatedAt = transaction["updated_at"].as<std::uint64_t>();
    ++metadata.chat.summary.revision;
    result = writeProjectChatMetadata(metadata.chat);
    if (result.success) {
        result = upsertProjectChatSummary(projectId, metadata.chat.summary);
    }
    if (!result.success) {
        return result;
    }
    return SD.remove(markerPath)
        ? OperationResult{true, ""}
        : OperationResult{false, "Recovered chat append but failed to clear its marker"};
}

}  // namespace

ChatDocumentResult createProjectChat(const String& projectId, const String& title)
{
    if (!isValidChatId(projectId.c_str()) || title.isEmpty() ||
        !isValidUtf8(title.c_str())) {
        return {false, {}, "Cannot create a project chat with invalid metadata"};
    }
    const ProjectDocumentResult project = loadProject(projectId);
    if (!project.success) {
        return {false, {}, project.error};
    }
    String chatId;
    for (std::uint8_t attempt = 0; attempt < 8; ++attempt) {
        chatId = generateProjectChatId();
        if (!SD.exists(projectChatDirectoryPath(projectId, chatId))) {
            break;
        }
        chatId.clear();
    }
    if (chatId.isEmpty()) {
        return {false, {}, "Failed to generate a unique project chat id after 8 attempts"};
    }
    const String chatDirectory = projectChatDirectoryPath(projectId, chatId);
    OperationResult result = ensureSdDirectory(chatDirectory);
    if (!result.success) {
        return {false, {}, result.error};
    }
    ChatDocument chat;
    chat.projectId = projectId;
    chat.summary = {chatId, title, 0, 0, false, false, 0, 1};
    result = writeEmptyAtomicSdFile(projectChatHistoryPath(projectId, chatId));
    if (result.success) {
        result = writeEmptyAtomicSdFile(projectChatTailPath(projectId, chatId));
    }
    if (result.success) {
        result = writeProjectChatMetadata(chat);
    }
    if (result.success) {
        result = upsertProjectChatSummary(projectId, chat.summary);
    }
    if (!result.success) {
        return {false, {}, cleanupFailedChatImport(chatDirectory, result.error).error};
    }
    return {true, chat, ""};
}

OperationResult saveProjectChatMetadata(const ChatDocument& chat)
{
    const ChatDocumentResult current = loadProjectChat(
        chat.projectId, chat.summary.id, 1, 1);
    if (!current.success) {
        return {false, current.error};
    }
    if (chat.summary.messageCount != current.chat.summary.messageCount) {
        return {false, "Project chat message count can only change through history operations"};
    }
    ChatDocument updated = chat;
    updated.messages.clear();
    updated.summary.revision = current.chat.summary.revision + 1;
    OperationResult result = writeProjectChatMetadata(updated);
    if (result.success) {
        result = upsertProjectChatSummary(updated.projectId, updated.summary);
    }
    return result;
}

OperationResult appendProjectChatMessages(const String& projectId,
                                          const String& chatId,
                                          const std::vector<Message>& messages,
                                          std::uint64_t updatedAt)
{
    if (messages.empty()) {
        return {false, "Project chat append requires at least one message"};
    }
    const ChatDocumentResult current = loadProjectChat(
        projectId, chatId, kStoredTailMessages, kStoredTailBytes);
    if (!current.success) {
        return {false, current.error};
    }
    std::uint64_t appendedBytes = 0;
    for (std::size_t index = 0; index < messages.size(); ++index) {
        const Message& message = messages[index];
        if ((message.role != "user" && message.role != "assistant") ||
            message.content.empty() || !isValidUtf8(message.content)) {
            return {false, "Cannot append an invalid project chat message"};
        }
        JsonDocument record;
        record["sequence"] = current.chat.summary.messageCount + index + 1;
        record["role"] = message.role;
        record["content"] = message.content;
        appendedBytes += measureJson(record) + 1;
    }
    OperationResult result = checkSdOperationSpace(
        appendedBytes + kStoredTailBytes, kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    JsonDocument transaction;
    transaction["old_count"] = current.chat.summary.messageCount;
    transaction["new_count"] = current.chat.summary.messageCount +
        static_cast<std::uint32_t>(messages.size());
    transaction["updated_at"] = updatedAt;
    result = writeAtomicJsonSdFile(
        projectChatAppendMarkerPath(projectId, chatId), transaction);
    if (!result.success) {
        return {false, "Failed to stage project chat append: " + result.error};
    }
    const String historyPath = projectChatHistoryPath(projectId, chatId);
    File historyFile = SD.open(historyPath, FILE_APPEND);
    if (!historyFile) {
        return {false, "Failed to open project chat raw history for append"};
    }
    for (std::size_t index = 0; index < messages.size() && result.success; ++index) {
        result = writeHistoryMessage(
            historyFile, current.chat.summary.messageCount + index + 1, messages[index]);
    }
    historyFile.flush();
    historyFile.close();
    if (!result.success) {
        return {false, result.error +
                       "; pending append recovery will validate the raw history"};
    }
    std::deque<SequencedMessage> tail;
    std::size_t tailBytes = 0;
    const std::uint32_t firstSequence = current.chat.summary.messageCount -
        static_cast<std::uint32_t>(current.chat.messages.size()) + 1;
    for (std::size_t index = 0; index < current.chat.messages.size(); ++index) {
        tailBytes += current.chat.messages[index].content.size();
        tail.push_back({firstSequence + static_cast<std::uint32_t>(index),
                        current.chat.messages[index]});
    }
    for (std::size_t index = 0; index < messages.size(); ++index) {
        tailBytes += messages[index].content.size();
        tail.push_back({current.chat.summary.messageCount +
                            static_cast<std::uint32_t>(index) + 1,
                        messages[index]});
        while (tail.size() > kStoredTailMessages || tailBytes > kStoredTailBytes) {
            tailBytes -= tail.front().message.content.size();
            tail.pop_front();
        }
    }
    result = writeTailFile(projectChatTailPath(projectId, chatId), tail);
    if (!result.success) {
        return result;
    }
    ChatDocument updated = current.chat;
    updated.messages.clear();
    updated.summary.messageCount += static_cast<std::uint32_t>(messages.size());
    updated.summary.updatedAt = updatedAt;
    ++updated.summary.revision;
    result = writeProjectChatMetadata(updated);
    if (result.success) {
        result = upsertProjectChatSummary(projectId, updated.summary);
    }
    if (!result.success) {
        return {false, result.error + "; pending append recovery is required"};
    }
    return SD.remove(projectChatAppendMarkerPath(projectId, chatId))
        ? OperationResult{true, ""}
        : OperationResult{false, "Chat append committed but its recovery marker remains"};
}

OperationResult clearProjectChatHistory(const String& projectId, const String& chatId)
{
    const ChatDocumentResult current = loadProjectChat(projectId, chatId, 1, 1);
    if (!current.success) {
        return {false, current.error};
    }
    OperationResult result = writeEmptyAtomicSdFile(projectChatHistoryPath(projectId, chatId));
    if (result.success) {
        result = writeEmptyAtomicSdFile(projectChatTailPath(projectId, chatId));
    }
    ChatDocument updated = current.chat;
    updated.messages.clear();
    updated.summary.messageCount = 0;
    updated.summary.archivedMessageCount = 0;
    updated.contextSummary.clear();
    updated.summarizedMessageCount = 0;
    ++updated.summary.revision;
    if (result.success) {
        result = writeProjectChatMetadata(updated);
    }
    if (result.success) {
        result = upsertProjectChatSummary(projectId, updated.summary);
    }
    return result;
}

OperationResult deleteProjectChat(const String& projectId, const String& chatId)
{
    const ChatDocumentResult current = loadProjectChat(projectId, chatId, 1, 1);
    if (!current.success) {
        return {false, current.error};
    }
    OperationResult result = removeProjectChatSummary(projectId, chatId);
    if (!result.success) {
        return result;
    }
    result = removeSdDirectoryTree(projectChatDirectoryPath(projectId, chatId));
    if (!result.success) {
        const OperationResult rollback = upsertProjectChatSummary(projectId, current.chat.summary);
        return rollback.success
            ? result
            : OperationResult{false, result.error + "; chat index rollback also failed: " +
                                       rollback.error};
    }
    return {true, ""};
}

ChatDocumentResult duplicateProjectChat(const String& projectId, const String& chatId)
{
    const ChatDocumentResult source = loadProjectChat(projectId, chatId, 1, 1);
    if (!source.success) {
        return source;
    }
    String duplicateId;
    for (std::uint8_t attempt = 0; attempt < 8; ++attempt) {
        duplicateId = generateProjectChatId();
        if (!SD.exists(projectChatDirectoryPath(projectId, duplicateId))) {
            break;
        }
        duplicateId.clear();
    }
    if (duplicateId.isEmpty()) {
        return {false, {}, "Failed to generate a unique duplicate chat id after 8 attempts"};
    }
    const String directory = projectChatDirectoryPath(projectId, duplicateId);
    OperationResult result = ensureSdDirectory(directory);
    if (result.success) {
        result = copySdFileAtomically(
            projectChatHistoryPath(projectId, chatId),
            projectChatHistoryPath(projectId, duplicateId),
            kStorageOperationalFloorBytes);
    }
    if (result.success) {
        result = copySdFileAtomically(
            projectChatTailPath(projectId, chatId),
            projectChatTailPath(projectId, duplicateId),
            kStorageOperationalFloorBytes);
    }
    ChatDocument duplicate = source.chat;
    duplicate.summary.id = duplicateId;
    duplicate.summary.title += " copy";
    duplicate.summary.revision = 1;
    duplicate.messages.clear();
    if (result.success) {
        result = writeProjectChatMetadata(duplicate);
    }
    if (result.success) {
        result = upsertProjectChatSummary(projectId, duplicate.summary);
    }
    if (!result.success) {
        return {false, {}, cleanupFailedChatImport(directory, result.error).error};
    }
    return loadProjectChat(projectId, duplicateId, 64, 65536);
}

ArchivedMessagesPageResult readProjectChatMessages(const String& projectId,
                                                   const String& chatId,
                                                   std::uint32_t offset,
                                                   std::size_t maximumMessages,
                                                   std::size_t maximumBytes)
{
    if (!isValidChatId(projectId.c_str()) || !isValidChatId(chatId.c_str()) ||
        maximumMessages == 0 || maximumBytes == 0) {
        return {false, {}, offset, true, "Project chat page arguments are invalid"};
    }
    File history = SD.open(projectChatHistoryPath(projectId, chatId), FILE_READ);
    if (!history) {
        return {false, {}, offset, true, "Project chat raw history does not exist"};
    }
    if (offset > history.size() || !history.seek(offset)) {
        history.close();
        return {false, {}, offset, true, "Project chat page offset is outside raw history"};
    }
    std::vector<Message> messages;
    messages.reserve(maximumMessages);
    std::size_t bytes = 0;
    while (history.available() && messages.size() < maximumMessages) {
        const std::uint32_t lineOffset = history.position();
        const String line = history.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument record;
        const DeserializationError error = deserializeJson(record, line);
        if (error || !record["role"].is<const char*>() ||
            !record["content"].is<const char*>()) {
            history.close();
            return {false, {}, offset, true, "Project chat page contains an invalid record"};
        }
        Message message = {record["role"].as<const char*>(),
                           record["content"].as<const char*>()};
        if (bytes + message.content.size() > maximumBytes && !messages.empty()) {
            history.seek(lineOffset);
            break;
        }
        bytes += message.content.size();
        messages.push_back(std::move(message));
    }
    const std::uint32_t nextOffset = history.position();
    const bool eof = !history.available();
    history.close();
    return {true, std::move(messages), nextOffset, eof, ""};
}

OperationResult exportProjectChatMarkdown(const String& projectId,
                                          const String& chatId,
                                          const String& filename)
{
    if (!isValidWorkspaceFilename(filename.c_str())) {
        return {false, "Project chat export filename is invalid"};
    }
    const ChatDocumentResult chat = loadProjectChat(projectId, chatId, 1, 1);
    if (!chat.success) {
        return {false, chat.error};
    }
    File history = SD.open(projectChatHistoryPath(projectId, chatId), FILE_READ);
    if (!history) {
        return {false, "Project chat raw history does not exist"};
    }
    const String target = workspaceFilePath(filename);
    OperationResult result = recoverAtomicSdFile(target);
    const String stagedPath = target + ".tmp";
    File output;
    if (result.success) {
        result = checkSdOperationSpace(history.size(), kStorageOperationalFloorBytes);
    }
    if (result.success) {
        output = SD.open(stagedPath, FILE_WRITE);
        if (!output) {
            result = {false, "Failed to create staged Markdown chat export"};
        }
    }
    if (result.success) {
        output.print("# ");
        output.println(chat.chat.summary.title);
        output.println();
    }
    while (history.available() && result.success) {
        const String line = history.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument record;
        const DeserializationError error = deserializeJson(record, line);
        if (error || !record["role"].is<const char*>() ||
            !record["content"].is<const char*>()) {
            result = {false, "Project chat history is invalid during Markdown export"};
            break;
        }
        output.print("## ");
        output.println(String(record["role"].as<const char*>()) == "user" ? "You" : "AI");
        output.println();
        output.println(record["content"].as<const char*>());
        output.println();
    }
    history.close();
    if (output) {
        output.flush();
        output.close();
    }
    if (!result.success) {
        SD.remove(stagedPath);
        return result;
    }
    return commitStagedSdFile(target, stagedPath);
}

OperationResult importLegacyChatToProject(const String& projectId,
                                          const ChatDocument& legacyChat)
{
    if (!isValidChatId(projectId.c_str()) || !isValidChatId(legacyChat.summary.id.c_str())) {
        return {false, "Cannot migrate an invalid project or chat id"};
    }
    const String chatDirectory = projectChatDirectoryPath(projectId, legacyChat.summary.id);
    if (SD.exists(chatDirectory)) {
        return {false, "Project chat directory already exists during migration"};
    }
    OperationResult result = ensureSdDirectory(chatDirectory);
    if (!result.success) {
        return result;
    }
    const String historyPath = projectChatHistoryPath(projectId, legacyChat.summary.id);
    const String stagedHistoryPath = historyPath + ".tmp";
    const LegacyHistoryMeasurement measurement = measureLegacyHistory(legacyChat);
    if (!measurement.success) {
        return cleanupFailedChatImport(chatDirectory, measurement.error);
    }
    const std::uint32_t expectedMessages = legacyChat.summary.archivedMessageCount +
        static_cast<std::uint32_t>(legacyChat.messages.size());
    if (measurement.messages != expectedMessages) {
        return cleanupFailedChatImport(
            chatDirectory, "Legacy archived message count does not match raw history");
    }
    result = checkSdOperationSpace(measurement.bytes, kStorageOperationalFloorBytes);
    if (!result.success) {
        return cleanupFailedChatImport(chatDirectory, result.error);
    }
    File staged = SD.open(stagedHistoryPath, FILE_WRITE);
    if (!staged) {
        return cleanupFailedChatImport(chatDirectory,
                                       "Failed to create staged raw chat history");
    }
    std::uint32_t sequence = 1;
    std::uint32_t archiveOffset = 0;
    bool archiveEof = false;
    while (!archiveEof && result.success) {
        const ArchivedMessagesPageResult page = readArchivedChatMessages(
            legacyChat.summary.id, archiveOffset, 16, 32768);
        if (!page.success) {
            result = {false, page.error};
            break;
        }
        for (const Message& message : page.messages) {
            result = writeHistoryMessage(staged, sequence, message);
            if (!result.success) {
                break;
            }
            ++sequence;
        }
        archiveOffset = page.nextOffset;
        archiveEof = page.eof;
    }
    for (const Message& message : legacyChat.messages) {
        if (!result.success) {
            break;
        }
        result = writeHistoryMessage(staged, sequence, message);
        if (result.success) {
            ++sequence;
        }
    }
    staged.flush();
    staged.close();
    const std::uint32_t totalMessages = sequence - 1;
    if (result.success && totalMessages != expectedMessages) {
        result = {false, "Legacy archived message count does not match raw history"};
    }
    if (result.success) {
        result = validateHistoryFile(stagedHistoryPath, totalMessages);
    }
    if (result.success) {
        result = commitStagedSdFile(historyPath, stagedHistoryPath);
    }
    ChatDocument migrated = legacyChat;
    migrated.projectId = projectId;
    migrated.messages.clear();
    migrated.summary.messageCount = totalMessages;
    migrated.summary.archivedMessageCount = 0;
    migrated.summary.revision = 1;
    migrated.contextSummary.clear();
    migrated.summarizedMessageCount = 0;
    if (result.success) {
        result = rebuildTailFile(projectId, legacyChat.summary.id, totalMessages);
    }
    if (result.success) {
        result = writeProjectChatMetadata(migrated);
    }
    if (result.success) {
        result = upsertProjectChatSummary(projectId, migrated.summary);
    }
    if (!result.success) {
        return cleanupFailedChatImport(chatDirectory, result.error);
    }
    return {true, ""};
}

ChatDocumentResult loadProjectChat(const String& projectId,
                                   const String& chatId,
                                   std::size_t maximumTailMessages,
                                   std::size_t maximumTailBytes)
{
    if (!isValidChatId(projectId.c_str()) || !isValidChatId(chatId.c_str())) {
        return {false, {}, "Cannot load project chat with an invalid id"};
    }
    if (maximumTailMessages == 0 || maximumTailBytes == 0) {
        return {false, {}, "Project chat tail limits must be greater than zero"};
    }
    const OperationResult recovered = recoverPendingAppend(projectId, chatId);
    if (!recovered.success) {
        return {false, {}, recovered.error};
    }
    File metadata = SD.open(projectChatMetadataPath(projectId, chatId), FILE_READ);
    if (!metadata) {
        return {false, {}, "Project chat metadata does not exist"};
    }
    ChatDocumentResult result = parseProjectChatMetadata(metadata, projectId, chatId);
    metadata.close();
    if (!result.success) {
        return result;
    }
    if (!SD.exists(projectChatTailPath(projectId, chatId))) {
        const OperationResult rebuilt = rebuildTailFile(
            projectId, chatId, result.chat.summary.messageCount);
        if (!rebuilt.success) {
            return {false, {}, rebuilt.error};
        }
    }
    File history = SD.open(projectChatTailPath(projectId, chatId), FILE_READ);
    if (!history) {
        return {false, {}, "Project chat context tail does not exist"};
    }
    std::deque<Message> tail;
    std::size_t tailBytes = 0;
    std::uint32_t lastSequence = 0;
    while (history.available()) {
        const String line = history.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument item;
        const DeserializationError error = deserializeJson(item, line);
        if (error || !item["sequence"].is<std::uint32_t>() ||
            !item["role"].is<const char*>() || !item["content"].is<const char*>()) {
            history.close();
            return {false, {}, "Project chat raw history contains an invalid typed record"};
        }
        Message message = {item["role"].as<const char*>(), item["content"].as<const char*>()};
        const std::uint32_t sequence = item["sequence"].as<std::uint32_t>();
        if ((lastSequence != 0 && sequence != lastSequence + 1) ||
            sequence == 0 || sequence > result.chat.summary.messageCount ||
            (message.role != "user" && message.role != "assistant") ||
            message.content.empty() || !isValidUtf8(message.content)) {
            history.close();
            return {false, {}, "Project chat raw history sequence or message is invalid"};
        }
        lastSequence = sequence;
        tailBytes += message.content.size();
        tail.push_back(std::move(message));
        while (tail.size() > maximumTailMessages || tailBytes > maximumTailBytes) {
            tailBytes -= tail.front().content.size();
            tail.pop_front();
        }
    }
    history.close();
    if (result.chat.summary.messageCount > 0 &&
        lastSequence != result.chat.summary.messageCount) {
        return {false, {}, "Project chat context tail does not end at metadata count"};
    }
    result.chat.messages.assign(tail.begin(), tail.end());
    return result;
}

OperationResult validateProjectChat(const String& projectId, const String& chatId)
{
    const ChatDocumentResult loaded = loadProjectChat(projectId, chatId, 1, 1);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    return validateHistoryFile(projectChatHistoryPath(projectId, chatId),
                               loaded.chat.summary.messageCount);
}

OperationResult cloneProjectChat(const String& sourceProjectId,
                                 const String& destinationProjectId,
                                 const String& chatId)
{
    if (!isValidChatId(sourceProjectId.c_str()) ||
        !isValidChatId(destinationProjectId.c_str()) || !isValidChatId(chatId.c_str()) ||
        sourceProjectId == destinationProjectId) {
        return {false, "Cannot clone chat with invalid or identical project ids"};
    }
    const ChatDocumentResult source = loadProjectChat(
        sourceProjectId, chatId, 1, 1);
    if (!source.success) {
        return {false, source.error};
    }
    const String destinationDirectory = projectChatDirectoryPath(destinationProjectId, chatId);
    if (SD.exists(destinationDirectory)) {
        return {false, "Destination project already contains this chat id"};
    }
    OperationResult result = ensureSdDirectory(destinationDirectory);
    if (!result.success) {
        return result;
    }
    result = copySdFileAtomically(
        projectChatHistoryPath(sourceProjectId, chatId),
        projectChatHistoryPath(destinationProjectId, chatId),
        kStorageOperationalFloorBytes);
    if (result.success) {
        result = copySdFileAtomically(
            projectChatTailPath(sourceProjectId, chatId),
            projectChatTailPath(destinationProjectId, chatId),
            kStorageOperationalFloorBytes);
    }
    ChatDocument cloned = source.chat;
    cloned.projectId = destinationProjectId;
    cloned.messages.clear();
    cloned.summary.revision = 1;
    if (result.success) {
        result = writeProjectChatMetadata(cloned);
    }
    if (result.success) {
        result = upsertProjectChatSummary(destinationProjectId, cloned.summary);
    }
    return result.success ? OperationResult{true, ""}
                          : cleanupFailedChatImport(destinationDirectory, result.error);
}

OperationResult writeProjectChatBundleRecords(File& output,
                                              const String& projectId,
                                              const String& chatId)
{
    if (!output) {
        return {false, "Project bundle output is not open"};
    }
    const ChatDocumentResult loaded = loadProjectChat(projectId, chatId, 1, 1);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    JsonDocument header;
    header["record"] = "chat";
    header["id"] = loaded.chat.summary.id;
    header["title"] = loaded.chat.summary.title;
    header["updated_at"] = loaded.chat.summary.updatedAt;
    header["message_count"] = loaded.chat.summary.messageCount;
    header["pinned"] = loaded.chat.summary.pinned;
    header["archived"] = loaded.chat.summary.archived;
    header["instructions"] = loaded.chat.instructions;
    header["draft"] = loaded.chat.draft;
    header["ssh_tools_enabled"] = loaded.chat.sshToolsEnabled;
    header["context_summary"] = loaded.chat.contextSummary;
    header["summarized_message_count"] = loaded.chat.summarizedMessageCount;
    const std::size_t headerBytes = measureJson(header);
    if (serializeJson(header, output) != headerBytes || output.write('\n') != 1) {
        return {false, "Failed to write project bundle chat header"};
    }
    File history = SD.open(projectChatHistoryPath(projectId, chatId), FILE_READ);
    if (!history) {
        return {false, "Project chat raw history does not exist"};
    }
    std::uint32_t writtenMessages = 0;
    while (history.available()) {
        const String line = history.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument source;
        const DeserializationError error = deserializeJson(source, line);
        if (error || !source["sequence"].is<std::uint32_t>() ||
            !source["role"].is<const char*>() || !source["content"].is<const char*>()) {
            history.close();
            return {false, "Project chat raw history is invalid during export"};
        }
        JsonDocument record;
        record["record"] = "message";
        record["chat_id"] = chatId;
        record["sequence"] = source["sequence"].as<std::uint32_t>();
        record["role"] = source["role"].as<const char*>();
        record["content"] = source["content"].as<const char*>();
        const std::size_t recordBytes = measureJson(record);
        if (serializeJson(record, output) != recordBytes || output.write('\n') != 1) {
            history.close();
            return {false, "Failed to write project bundle message"};
        }
        ++writtenMessages;
    }
    history.close();
    return writtenMessages == loaded.chat.summary.messageCount
        ? OperationResult{true, ""}
        : OperationResult{false, "Project bundle message count does not match chat metadata"};
}

StorageSizeResult measureProjectChatBundleRecords(const String& projectId,
                                                  const String& chatId)
{
    const ChatDocumentResult loaded = loadProjectChat(projectId, chatId, 1, 1);
    if (!loaded.success) {
        return {false, 0, loaded.error};
    }
    JsonDocument header;
    header["record"] = "chat";
    header["id"] = loaded.chat.summary.id;
    header["title"] = loaded.chat.summary.title;
    header["updated_at"] = loaded.chat.summary.updatedAt;
    header["message_count"] = loaded.chat.summary.messageCount;
    header["pinned"] = loaded.chat.summary.pinned;
    header["archived"] = loaded.chat.summary.archived;
    header["instructions"] = loaded.chat.instructions;
    header["draft"] = loaded.chat.draft;
    header["ssh_tools_enabled"] = loaded.chat.sshToolsEnabled;
    header["context_summary"] = loaded.chat.contextSummary;
    header["summarized_message_count"] = loaded.chat.summarizedMessageCount;
    std::uint64_t bytes = measureJson(header) + 1;
    File history = SD.open(projectChatHistoryPath(projectId, chatId), FILE_READ);
    if (!history) {
        return {false, 0, "Project chat raw history does not exist"};
    }
    std::uint32_t measuredMessages = 0;
    while (history.available()) {
        const String line = history.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument source;
        const DeserializationError error = deserializeJson(source, line);
        if (error || !source["sequence"].is<std::uint32_t>() ||
            !source["role"].is<const char*>() || !source["content"].is<const char*>()) {
            history.close();
            return {false, 0, "Project chat raw history is invalid during measurement"};
        }
        JsonDocument record;
        record["record"] = "message";
        record["chat_id"] = chatId;
        record["sequence"] = source["sequence"].as<std::uint32_t>();
        record["role"] = source["role"].as<const char*>();
        record["content"] = source["content"].as<const char*>();
        bytes += measureJson(record) + 1;
        ++measuredMessages;
    }
    history.close();
    return measuredMessages == loaded.chat.summary.messageCount
        ? StorageSizeResult{true, bytes, ""}
        : StorageSizeResult{false, 0,
                            "Project chat message count changed during bundle measurement"};
}

OperationResult importProjectChatBundleRecords(File& input,
                                               const String& destinationProjectId,
                                               const ChatDocument& metadata,
                                               std::uint32_t messageCount)
{
    if (!input || !isValidChatId(destinationProjectId.c_str()) ||
        !isValidChatId(metadata.summary.id.c_str()) ||
        metadata.summary.messageCount != messageCount || metadata.summary.title.isEmpty() ||
        !isValidUtf8(metadata.summary.title.c_str()) ||
        metadata.instructions.size() > kMaximumProjectChatInstructionsBytes ||
        metadata.draft.size() > kMaximumProjectChatDraftBytes ||
        metadata.contextSummary.size() > kMaximumProjectChatSummaryBytes ||
        !isValidUtf8(metadata.instructions) || !isValidUtf8(metadata.draft) ||
        !isValidUtf8(metadata.contextSummary) ||
        metadata.summarizedMessageCount > messageCount) {
        return {false, "Project bundle chat import arguments are invalid"};
    }
    const String chatDirectory = projectChatDirectoryPath(
        destinationProjectId, metadata.summary.id);
    if (SD.exists(chatDirectory)) {
        return {false, "Imported project contains a duplicate chat id"};
    }
    OperationResult result = ensureSdDirectory(chatDirectory);
    if (!result.success) {
        return result;
    }
    const String historyPath = projectChatHistoryPath(
        destinationProjectId, metadata.summary.id);
    const String stagedHistoryPath = historyPath + ".tmp";
    File staged = SD.open(stagedHistoryPath, FILE_WRITE);
    if (!staged) {
        return cleanupFailedChatImport(chatDirectory,
                                       "Failed to stage imported project chat history");
    }
    for (std::uint32_t expectedSequence = 1;
         expectedSequence <= messageCount && result.success; ++expectedSequence) {
        if (!input.available()) {
            result = {false, "Project bundle ended before all chat messages were read"};
            break;
        }
        const String line = input.readStringUntil('\n');
        JsonDocument record;
        const DeserializationError error = deserializeJson(record, line);
        if (error || !record["record"].is<const char*>() ||
            String(record["record"].as<const char*>()) != "message" ||
            !record["chat_id"].is<const char*>() ||
            String(record["chat_id"].as<const char*>()) != metadata.summary.id ||
            !record["sequence"].is<std::uint32_t>() ||
            record["sequence"].as<std::uint32_t>() != expectedSequence ||
            !record["role"].is<const char*>() || !record["content"].is<const char*>()) {
            result = {false, "Project bundle contains an invalid chat message record"};
            break;
        }
        result = writeHistoryMessage(
            staged, expectedSequence,
            {record["role"].as<const char*>(), record["content"].as<const char*>()});
    }
    staged.flush();
    staged.close();
    if (result.success) {
        result = validateHistoryFile(stagedHistoryPath, messageCount);
    }
    if (result.success) {
        result = commitStagedSdFile(historyPath, stagedHistoryPath);
    }
    ChatDocument imported = metadata;
    imported.projectId = destinationProjectId;
    imported.messages.clear();
    imported.summary.revision = 1;
    if (result.success) {
        result = rebuildTailFile(destinationProjectId, metadata.summary.id, messageCount);
    }
    if (result.success) {
        result = writeProjectChatMetadata(imported);
    }
    if (result.success) {
        result = upsertProjectChatSummary(destinationProjectId, imported.summary);
    }
    return result.success ? OperationResult{true, ""}
                          : cleanupFailedChatImport(chatDirectory, result.error);
}

}  // namespace cardputer
