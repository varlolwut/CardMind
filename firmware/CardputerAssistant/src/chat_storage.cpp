#include "chat_storage.h"

#include "file_workspace.h"
#include "text_utils.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>

namespace cardputer {
namespace {

constexpr const char* kAssistantDirectory = "/assistant";
constexpr const char* kChatsDirectory = "/assistant/chats";
constexpr std::uint32_t kFormatVersion = 3;
constexpr std::uint32_t kOldestSupportedFormatVersion = 1;

String chatPath(const String& id, const char* extension)
{
    return String(kChatsDirectory) + "/" + id + extension;
}

String chatArchivePath(const String& id)
{
    return chatPath(id, ".archive.jsonl");
}

std::uint64_t currentTimestamp()
{
    const std::time_t current = std::time(nullptr);
    return current >= 1700000000 ? static_cast<std::uint64_t>(current) : 0;
}

OperationResult validateSummary(const ChatSummary& summary)
{
    if (!isValidChatId(summary.id.c_str())) {
        return {false, "Chat id must contain exactly 16 lowercase hexadecimal characters"};
    }
    if (summary.title.isEmpty() || !isValidUtf8(summary.title.c_str()) || summary.title.length() > 120) {
        return {false, "Chat title must be valid UTF-8 and contain 1 to 120 bytes"};
    }
    if (summary.messageCount > kMaximumStoredMessages) {
        return {false, "Chat message count exceeds the 64-message context limit"};
    }
    return {true, ""};
}

OperationResult validateDocument(const ChatDocument& chat)
{
    ChatSummary summary = chat.summary;
    summary.messageCount = static_cast<std::uint32_t>(chat.messages.size());
    const OperationResult summaryResult = validateSummary(summary);
    if (!summaryResult.success) {
        return summaryResult;
    }
    std::size_t historyBytes = 0;
    for (const auto& message : chat.messages) {
        if (message.role != "user" && message.role != "assistant") {
            return {false, "Stored chat message role must be 'user' or 'assistant'"};
        }
        if (!isValidUtf8(message.content) || message.content.empty()) {
            return {false, "Stored chat message content must be non-empty valid UTF-8"};
        }
        historyBytes += message.content.size();
    }
    if (historyBytes > kMaximumStoredHistoryBytes) {
        return {false, "Stored chat history exceeds the 32768-byte context limit"};
    }
    if (chat.instructions.size() > kMaximumChatInstructionsBytes) {
        return {false, "Chat instructions exceed the 2048-byte limit"};
    }
    if (!isValidUtf8(chat.instructions)) {
        return {false, "Chat instructions must be valid UTF-8"};
    }
    if (chat.draft.size() > kMaximumChatDraftBytes || !isValidUtf8(chat.draft)) {
        return {false, "Chat draft must be valid UTF-8 up to 1200 bytes"};
    }
    return {true, ""};
}

ChatDocumentResult parseChatFile(File& file)
{
    JsonDocument document;
    const DeserializationError jsonError = deserializeJson(document, file);
    if (jsonError) {
        return {false, {}, String("Failed to parse chat JSON: ") + jsonError.c_str()};
    }
    if (!document["version"].is<std::uint32_t>() ||
        !document["id"].is<const char*>() || !document["title"].is<const char*>() ||
        !document["updated_at"].is<std::uint64_t>() || !document["messages"].is<JsonArray>()) {
        return {false, {}, "Chat JSON is missing required typed fields"};
    }
    const std::uint32_t version = document["version"].as<std::uint32_t>();
    if (version < kOldestSupportedFormatVersion || version > kFormatVersion) {
        return {false, {}, "Chat JSON version is not supported"};
    }
    if (version >= 2 && !document["instructions"].is<const char*>()) {
        return {false, {}, "Chat JSON is missing typed instructions"};
    }
    if (version >= 3 &&
        (!document["draft"].is<const char*>() || !document["pinned"].is<bool>() ||
         !document["archived"].is<bool>() ||
         !document["archived_message_count"].is<std::uint32_t>())) {
        return {false, {}, "Chat JSON is missing version-3 metadata"};
    }

    ChatDocument result;
    result.summary.id = document["id"].as<const char*>();
    result.summary.title = document["title"].as<const char*>();
    result.summary.updatedAt = document["updated_at"].as<std::uint64_t>();
    result.instructions = version >= 2 ? document["instructions"].as<const char*>() : "";
    result.draft = version >= 3 ? document["draft"].as<const char*>() : "";
    result.summary.pinned = version >= 3 ? document["pinned"].as<bool>() : false;
    result.summary.archived = version >= 3 ? document["archived"].as<bool>() : false;
    result.summary.archivedMessageCount = version >= 3
        ? document["archived_message_count"].as<std::uint32_t>() : 0;
    const JsonArrayConst messages = document["messages"].as<JsonArrayConst>();
    if (messages.size() > kMaximumStoredMessages) {
        return {false, {}, "Chat JSON contains too many messages"};
    }
    for (const JsonVariantConst item : messages) {
        if (!item.is<JsonObjectConst>()) {
            return {false, {}, "Chat JSON message entry must be an object"};
        }
        const JsonObjectConst object = item.as<JsonObjectConst>();
        if (!object["role"].is<const char*>() || !object["content"].is<const char*>()) {
            return {false, {}, "Chat JSON message is missing role or content"};
        }
        result.messages.push_back({object["role"].as<const char*>(), object["content"].as<const char*>()});
    }
    result.summary.messageCount = static_cast<std::uint32_t>(result.messages.size());
    const OperationResult validation = validateDocument(result);
    if (!validation.success) {
        return {false, {}, validation.error};
    }
    return {true, result, ""};
}

OperationResult removeIfPresent(const String& path)
{
    if (!SD.exists(path)) {
        return {true, ""};
    }
    if (!SD.remove(path)) {
        return {false, "Failed to remove stale chat file " + path};
    }
    return {true, ""};
}

OperationResult writeChatFile(const ChatDocument& chat, const String& path)
{
    JsonDocument document;
    document["version"] = kFormatVersion;
    document["id"] = chat.summary.id;
    document["title"] = chat.summary.title;
    document["updated_at"] = chat.summary.updatedAt;
    document["instructions"] = chat.instructions;
    document["draft"] = chat.draft;
    document["pinned"] = chat.summary.pinned;
    document["archived"] = chat.summary.archived;
    document["archived_message_count"] = chat.summary.archivedMessageCount;
    JsonArray messages = document["messages"].to<JsonArray>();
    for (const auto& message : chat.messages) {
        JsonObject item = messages.add<JsonObject>();
        item["role"] = message.role;
        item["content"] = message.content;
    }

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        return {false, "Failed to create chat file " + path};
    }
    const std::size_t expectedBytes = measureJson(document);
    const std::size_t writtenBytes = serializeJson(document, file);
    file.flush();
    file.close();
    if (writtenBytes != expectedBytes) {
        return {false, "Failed to write complete chat JSON to " + path};
    }
    return {true, ""};
}

String generateChatId()
{
    char buffer[17] = {};
    std::snprintf(buffer, sizeof(buffer), "%08x%08x",
                  static_cast<unsigned int>(esp_random()),
                  static_cast<unsigned int>(esp_random()));
    return String(buffer);
}

}  // namespace

HistoryFitResult fitHistoryToActiveContext(const std::vector<Message>& messages)
{
    std::size_t firstRetained = 0;
    std::size_t retainedBytes = 0;
    for (const auto& message : messages) {
        retainedBytes += message.content.size();
    }
    while (messages.size() - firstRetained > kMaximumStoredMessages ||
           retainedBytes > kMaximumStoredHistoryBytes) {
        const std::size_t remaining = messages.size() - firstRetained;
        if (remaining < 2) {
            firstRetained = messages.size();
            retainedBytes = 0;
            break;
        }
        retainedBytes -= messages[firstRetained].content.size();
        retainedBytes -= messages[firstRetained + 1].content.size();
        firstRetained += 2;
    }
    return {
        std::vector<Message>(messages.begin() + static_cast<std::ptrdiff_t>(firstRetained),
                             messages.end()),
        std::vector<Message>(messages.begin(),
                             messages.begin() + static_cast<std::ptrdiff_t>(firstRetained)),
    };
}

OperationResult initializeChatStorage()
{
    if (SD.cardType() == CARD_NONE) {
        return {false, "microSD is required for persistent chats"};
    }
    if (!SD.exists(kAssistantDirectory) && !SD.mkdir(kAssistantDirectory)) {
        return {false, "Failed to create /assistant on microSD"};
    }
    if (!SD.exists(kChatsDirectory) && !SD.mkdir(kChatsDirectory)) {
        return {false, "Failed to create /assistant/chats on microSD"};
    }
    return {true, ""};
}

ChatsResult listChats()
{
    File directory = SD.open(kChatsDirectory);
    if (!directory || !directory.isDirectory()) {
        return {false, {}, "Failed to open /assistant/chats directory"};
    }
    std::vector<ChatSummary> chats;
    File file = directory.openNextFile();
    while (file) {
        const String name = file.name();
        if (!file.isDirectory() && name.endsWith(".json")) {
            const ChatDocumentResult parsed = parseChatFile(file);
            if (!parsed.success) {
                file.close();
                directory.close();
                return {false, {}, "Failed to load " + name + ": " + parsed.error};
            }
            chats.push_back(parsed.chat.summary);
            if (chats.size() > kMaximumStoredChats) {
                file.close();
                directory.close();
                return {false, {}, "microSD contains more than 20 chat files"};
            }
        }
        file.close();
        file = directory.openNextFile();
    }
    directory.close();
    std::sort(chats.begin(), chats.end(), [](const ChatSummary& left, const ChatSummary& right) {
        if (left.pinned != right.pinned) {
            return left.pinned;
        }
        if (left.archived != right.archived) {
            return !left.archived;
        }
        if (left.updatedAt != right.updatedAt) {
            return left.updatedAt > right.updatedAt;
        }
        return left.id < right.id;
    });
    return {true, chats, ""};
}

ChatDocumentResult createChat(const String& title)
{
    const ChatsResult existing = listChats();
    if (!existing.success) {
        return {false, {}, existing.error};
    }
    if (existing.chats.size() >= kMaximumStoredChats) {
        return {false, {}, "Chat limit reached; delete a chat before creating another"};
    }
    String id;
    for (std::size_t attempt = 0; attempt < 8; ++attempt) {
        id = generateChatId();
        if (!SD.exists(chatPath(id, ".json"))) {
            break;
        }
        id = "";
    }
    if (id.isEmpty()) {
        return {false, {}, "Failed to generate a unique chat id after 8 attempts"};
    }
    ChatDocument chat = {{id, title, currentTimestamp(), 0}, {}, ""};
    const OperationResult saved = saveChat(chat);
    return saved.success ? ChatDocumentResult{true, chat, ""}
                         : ChatDocumentResult{false, {}, saved.error};
}

ChatDocumentResult loadChat(const String& id)
{
    if (!isValidChatId(id.c_str())) {
        return {false, {}, "Cannot load chat: invalid chat id"};
    }
    File file = SD.open(chatPath(id, ".json"), FILE_READ);
    if (!file) {
        return {false, {}, "Chat file does not exist for id " + id};
    }
    ChatDocumentResult result = parseChatFile(file);
    file.close();
    if (result.success && result.chat.summary.id != id) {
        return {false, {}, "Chat filename id does not match the JSON id"};
    }
    return result;
}

OperationResult saveChat(const ChatDocument& chat)
{
    ChatDocument stored = chat;
    stored.summary.messageCount = static_cast<std::uint32_t>(stored.messages.size());
    if (stored.summary.updatedAt == 0) {
        stored.summary.updatedAt = currentTimestamp();
    }
    const OperationResult validation = validateDocument(stored);
    if (!validation.success) {
        return validation;
    }
    const String target = chatPath(stored.summary.id, ".json");
    const String temporary = chatPath(stored.summary.id, ".tmp");
    const String backup = chatPath(stored.summary.id, ".bak");
    OperationResult result = removeIfPresent(temporary);
    if (!result.success) {
        return result;
    }
    result = removeIfPresent(backup);
    if (!result.success) {
        return result;
    }
    result = writeChatFile(stored, temporary);
    if (!result.success) {
        removeIfPresent(temporary);
        return result;
    }
    const bool hadTarget = SD.exists(target);
    if (hadTarget && !SD.rename(target, backup)) {
        removeIfPresent(temporary);
        return {false, "Failed to create a backup before replacing chat " + stored.summary.id};
    }
    if (!SD.rename(temporary, target)) {
        if (hadTarget && !SD.rename(backup, target)) {
            return {false, "Failed to commit chat and failed to restore its backup"};
        }
        return {false, "Failed to commit chat file " + stored.summary.id};
    }
    if (hadTarget) {
        result = removeIfPresent(backup);
        if (!result.success) {
            return result;
        }
    }
    return {true, ""};
}

OperationResult deleteChat(const String& id)
{
    if (!isValidChatId(id.c_str())) {
        return {false, "Cannot delete chat: invalid chat id"};
    }
    const String target = chatPath(id, ".json");
    if (!SD.exists(target)) {
        return {false, "Cannot delete chat: file does not exist"};
    }
    if (!SD.remove(target)) {
        return {false, "Failed to delete chat file " + id};
    }
    const String archive = chatArchivePath(id);
    if (SD.exists(archive) && !SD.remove(archive)) {
        return {false, "Chat was deleted, but its archived turns could not be removed"};
    }
    return {true, ""};
}

OperationResult archiveChatMessages(const String& id,
                                    const std::vector<Message>& messages)
{
    if (!isValidChatId(id.c_str())) {
        return {false, "Cannot archive messages: invalid chat id"};
    }
    if (messages.empty()) {
        return {true, ""};
    }
    const String path = chatArchivePath(id);
    File existing = SD.open(path, FILE_READ);
    const std::size_t existingBytes = existing ? existing.size() : 0;
    if (existing) {
        existing.close();
    }
    std::size_t additionalBytes = 0;
    for (const auto& message : messages) {
        if ((message.role != "user" && message.role != "assistant") ||
            message.content.empty() || !isValidUtf8(message.content)) {
            return {false, "Cannot archive an invalid chat message"};
        }
        JsonDocument document;
        document["role"] = message.role;
        document["content"] = message.content;
        additionalBytes += measureJson(document) + 1;
    }
    if (existingBytes > kMaximumArchivedChatBytes ||
        additionalBytes > kMaximumArchivedChatBytes - existingBytes) {
        return {false, "Archived chat history reached the 2 MiB safety limit; export it first"};
    }
    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        return {false, "Failed to open archived chat history on microSD"};
    }
    for (const auto& message : messages) {
        JsonDocument document;
        document["role"] = message.role;
        document["content"] = message.content;
        const std::size_t expected = measureJson(document);
        if (serializeJson(document, file) != expected || file.write('\n') != 1) {
            file.close();
            return {false, "Failed while appending archived chat history"};
        }
    }
    file.flush();
    file.close();
    return {true, ""};
}

OperationResult exportChatToWorkspace(const String& id, const String& filename)
{
    const ChatDocumentResult loaded = loadChat(id);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    const OperationResult created = createWorkspaceFile(filename);
    if (!created.success) {
        return created;
    }
    const String target = workspaceFilePath(filename);
    File output = SD.open(target, FILE_APPEND);
    if (!output) {
        deleteWorkspaceFile(filename);
        return {false, "Failed to open chat export file"};
    }
    std::size_t writtenBytes = 0;
    const String heading = "# " + loaded.chat.summary.title + "\n\n";
    const std::size_t headingBytes = output.print(heading);
    if (headingBytes != heading.length()) {
        output.close();
        deleteWorkspaceFile(filename);
        return {false, "Failed while writing the chat export heading"};
    }
    writtenBytes += headingBytes;
    String exportError;
    auto writeMessage = [&output, &writtenBytes, &exportError](const Message& message) -> bool {
        const String headingText = message.role == "user" ? "## You\n\n" : "## Assistant\n\n";
        const std::size_t required = headingText.length() + message.content.size() + 2;
        if (required > kMaximumWorkspaceFileBytes - writtenBytes) {
            exportError = "Chat export exceeds the 491520-byte workspace file limit";
            return false;
        }
        const std::size_t headingWritten = output.print(headingText);
        const std::size_t contentWritten = output.write(
            reinterpret_cast<const std::uint8_t*>(message.content.data()),
            message.content.size());
        const std::size_t separatorWritten = output.print("\n\n");
        if (headingWritten != headingText.length() ||
            contentWritten != message.content.size() || separatorWritten != 2) {
            exportError = "Failed while writing a chat export message";
            return false;
        }
        writtenBytes += headingWritten + contentWritten + separatorWritten;
        return true;
    };
    File archive = SD.open(chatArchivePath(id), FILE_READ);
    while (archive && archive.available()) {
        const String line = archive.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument item;
        const DeserializationError error = deserializeJson(item, line);
        if (error || !item["role"].is<const char*>() || !item["content"].is<const char*>()) {
            archive.close();
            output.close();
            deleteWorkspaceFile(filename);
            return {false, "Archived chat contains an invalid JSON line"};
        }
        if (!writeMessage({item["role"].as<const char*>(), item["content"].as<const char*>()})) {
            archive.close();
            output.close();
            deleteWorkspaceFile(filename);
            return {false, exportError};
        }
    }
    if (archive) {
        archive.close();
    }
    for (const auto& message : loaded.chat.messages) {
        if (!writeMessage(message)) {
            output.close();
            deleteWorkspaceFile(filename);
            return {false, exportError};
        }
    }
    output.flush();
    output.close();
    return {true, ""};
}

OperationResult exportChatBundleToWorkspace(const String& id, const String& filename)
{
    const ChatDocumentResult loaded = loadChat(id);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    const OperationResult created = createWorkspaceFile(filename);
    if (!created.success) {
        return created;
    }
    File output = SD.open(workspaceFilePath(filename), FILE_APPEND);
    if (!output) {
        deleteWorkspaceFile(filename);
        return {false, "Failed to open portable chat bundle"};
    }
    std::size_t writtenBytes = 0;
    auto writeJsonLine = [&output, &writtenBytes](JsonDocument& document) -> OperationResult {
        const std::size_t required = measureJson(document) + 1;
        if (required > kMaximumWorkspaceFileBytes - writtenBytes) {
            return {false, "Portable chat bundle exceeds the 491520-byte workspace file limit"};
        }
        if (serializeJson(document, output) != required - 1 || output.write('\n') != 1) {
            return {false, "Failed while writing portable chat bundle"};
        }
        writtenBytes += required;
        return {true, ""};
    };

    JsonDocument header;
    header["type"] = "cardmind_chat";
    header["format"] = 1;
    header["title"] = loaded.chat.summary.title;
    header["instructions"] = loaded.chat.instructions;
    header["archived_message_count"] = loaded.chat.summary.archivedMessageCount;
    OperationResult result = writeJsonLine(header);
    File archive;
    if (result.success) {
        archive = SD.open(chatArchivePath(id), FILE_READ);
    }
    while (result.success && archive && archive.available()) {
        const String line = archive.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument message;
        const DeserializationError error = deserializeJson(message, line);
        if (error || !message["role"].is<const char*>() ||
            !message["content"].is<const char*>()) {
            result = {false, "Archived chat contains an invalid JSON line"};
        } else {
            result = writeJsonLine(message);
        }
    }
    if (archive) {
        archive.close();
    }
    for (const auto& message : loaded.chat.messages) {
        if (!result.success) {
            break;
        }
        JsonDocument item;
        item["role"] = message.role;
        item["content"] = message.content;
        result = writeJsonLine(item);
    }
    output.flush();
    output.close();
    if (!result.success) {
        deleteWorkspaceFile(filename);
    }
    return result;
}

ChatDocumentResult importChatBundleFromWorkspace(const String& filename)
{
    if (!filename.endsWith(".chat.jsonl")) {
        return {false, {}, "Portable chat filename must end with .chat.jsonl"};
    }
    File input = SD.open(workspaceFilePath(filename), FILE_READ);
    if (!input) {
        return {false, {}, "Portable chat bundle was not found in the workspace"};
    }
    const String headerLine = input.readStringUntil('\n');
    JsonDocument header;
    const DeserializationError headerError = deserializeJson(header, headerLine);
    if (headerError || !header["type"].is<const char*>() ||
        String(header["type"].as<const char*>()) != "cardmind_chat" ||
        !header["format"].is<std::uint32_t>() ||
        header["format"].as<std::uint32_t>() != 1 ||
        !header["title"].is<const char*>() || !header["instructions"].is<const char*>() ||
        !header["archived_message_count"].is<std::uint32_t>()) {
        input.close();
        return {false, {}, "Portable chat bundle header is invalid or unsupported"};
    }
    const String title = header["title"].as<const char*>();
    const std::string instructions = header["instructions"].as<const char*>();
    const std::uint32_t declaredArchivedMessages =
        header["archived_message_count"].as<std::uint32_t>();
    if (title.isEmpty() || title.length() > 120 || !isValidUtf8(title.c_str()) ||
        instructions.size() > kMaximumChatInstructionsBytes || !isValidUtf8(instructions)) {
        input.close();
        return {false, {}, "Portable chat bundle metadata exceeds the supported limits"};
    }

    const ChatDocumentResult created = createChat(title);
    if (!created.success) {
        input.close();
        return created;
    }
    ChatDocument imported = created.chat;
    imported.instructions = instructions;
    std::size_t activeBytes = 0;
    std::uint32_t messageIndex = 0;
    OperationResult result = {true, ""};
    while (input.available() && result.success) {
        const String line = input.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        JsonDocument item;
        const DeserializationError itemError = deserializeJson(item, line);
        if (itemError || !item["role"].is<const char*>() ||
            !item["content"].is<const char*>()) {
            result = {false, "Portable chat bundle contains an invalid message line"};
            break;
        }
        Message message = {item["role"].as<const char*>(), item["content"].as<const char*>()};
        if ((message.role != "user" && message.role != "assistant") ||
            message.content.empty() || !isValidUtf8(message.content) ||
            message.content.size() > kMaximumStoredHistoryBytes) {
            result = {false, "Portable chat bundle contains an unsupported message"};
            break;
        }
        if (messageIndex < declaredArchivedMessages) {
            result = archiveChatMessages(imported.summary.id, {message});
            if (result.success) {
                ++imported.summary.archivedMessageCount;
            }
            ++messageIndex;
            continue;
        }
        ++messageIndex;
        imported.messages.push_back(std::move(message));
        activeBytes += imported.messages.back().content.size();
        while (imported.messages.size() > kMaximumStoredMessages ||
               activeBytes > kMaximumStoredHistoryBytes) {
            const Message archived = imported.messages.front();
            result = archiveChatMessages(imported.summary.id, {archived});
            if (!result.success) {
                break;
            }
            activeBytes -= archived.content.size();
            imported.messages.erase(imported.messages.begin());
            ++imported.summary.archivedMessageCount;
        }
    }
    input.close();
    if (result.success && messageIndex < declaredArchivedMessages) {
        result = {false, "Portable chat bundle declares more archived messages than it contains"};
    }
    if (result.success) {
        result = saveChat(imported);
    }
    if (!result.success) {
        deleteChat(imported.summary.id);
        return {false, {}, result.error};
    }
    imported.summary.messageCount = static_cast<std::uint32_t>(imported.messages.size());
    return {true, imported, ""};
}

ChatDocumentResult duplicateChat(const String& id)
{
    const ChatDocumentResult source = loadChat(id);
    if (!source.success) {
        return source;
    }
    const ChatDocumentResult created = createChat(source.chat.summary.title + " copy");
    if (!created.success) {
        return created;
    }
    ChatDocument duplicate = created.chat;
    duplicate.messages = source.chat.messages;
    duplicate.instructions = source.chat.instructions;
    duplicate.draft = source.chat.draft;
    const OperationResult saved = saveChat(duplicate);
    if (!saved.success) {
        deleteChat(duplicate.summary.id);
        return {false, {}, saved.error};
    }
    return {true, duplicate, ""};
}

}  // namespace cardputer
