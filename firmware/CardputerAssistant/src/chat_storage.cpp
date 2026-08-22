#include "chat_storage.h"

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
constexpr std::uint32_t kFormatVersion = 1;

String chatPath(const String& id, const char* extension)
{
    return String(kChatsDirectory) + "/" + id + extension;
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
        document["version"].as<std::uint32_t>() != kFormatVersion ||
        !document["id"].is<const char*>() || !document["title"].is<const char*>() ||
        !document["updated_at"].is<std::uint64_t>() || !document["messages"].is<JsonArray>()) {
        return {false, {}, "Chat JSON is missing required typed fields"};
    }

    ChatDocument result;
    result.summary.id = document["id"].as<const char*>();
    result.summary.title = document["title"].as<const char*>();
    result.summary.updatedAt = document["updated_at"].as<std::uint64_t>();
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
    ChatDocument chat = {{id, title, currentTimestamp(), 0}, {}};
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
    return {true, ""};
}

}  // namespace cardputer
