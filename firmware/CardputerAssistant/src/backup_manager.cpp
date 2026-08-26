#include "backup_manager.h"

#include "chat_storage.h"
#include "storage.h"
#include "text_utils.h"

#include <ArduinoJson.h>
#include <SD.h>

#include <algorithm>
#include <cstdint>
#include <ctime>

namespace cardputer {
namespace {

constexpr const char* kAssistantPath = "/assistant";
constexpr const char* kBackupsPath = "/assistant/backups";
constexpr const char* kLatestPath = "/assistant/backups/latest";
constexpr const char* kTemporaryPath = "/assistant/backups/latest.tmp";
constexpr const char* kOldPath = "/assistant/backups/latest.old";
constexpr const char* kChatsPath = "/assistant/chats";
constexpr const char* kRestoreChatsPath = "/assistant/chats.restore.tmp";
constexpr const char* kOldChatsPath = "/assistant/chats.restore.old";
constexpr const char* kBookmarksPath = "/assistant/file_bookmarks.json";
constexpr const char* kRestoreBookmarksPath = "/assistant/file_bookmarks.restore.tmp";
constexpr const char* kOldBookmarksPath = "/assistant/file_bookmarks.restore.old";
constexpr std::uint32_t kBackupFormatVersion = 2;
constexpr std::size_t kCopyBufferBytes = 1024;

OperationResult removeFileIfPresent(const String& path)
{
    if (!SD.exists(path)) {
        return {true, ""};
    }
    return SD.remove(path)
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to remove backup file " + path};
}

OperationResult removeFlatDirectory(const String& path)
{
    if (!SD.exists(path)) {
        return {true, ""};
    }
    File directory = SD.open(path);
    if (!directory || !directory.isDirectory()) {
        return {false, "Backup path is not a directory: " + path};
    }
    File entry = directory.openNextFile();
    while (entry) {
        const bool nested = entry.isDirectory();
        String entryPath = entry.path();
        entry.close();
        if (nested) {
            directory.close();
            return {false, "Backup directory unexpectedly contains a nested directory"};
        }
        if (!SD.remove(entryPath)) {
            directory.close();
            return {false, "Failed to remove backup entry " + entryPath};
        }
        entry = directory.openNextFile();
    }
    directory.close();
    return SD.rmdir(path)
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to remove backup directory " + path};
}

OperationResult removeBackupTree(const String& path)
{
    if (!SD.exists(path)) {
        return {true, ""};
    }
    const String chats = path + "/chats";
    OperationResult result = removeFlatDirectory(chats);
    if (!result.success) {
        return result;
    }
    result = removeFileIfPresent(path + "/file_bookmarks.json");
    if (!result.success) {
        return result;
    }
    result = removeFileIfPresent(path + "/manifest.json");
    if (!result.success) {
        return result;
    }
    return SD.rmdir(path)
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to remove backup tree " + path};
}

OperationResult copyFile(const String& sourcePath, const String& destinationPath)
{
    File source = SD.open(sourcePath, FILE_READ);
    if (!source) {
        return {false, "Failed to open backup source " + sourcePath};
    }
    File destination = SD.open(destinationPath, FILE_WRITE);
    if (!destination) {
        source.close();
        return {false, "Failed to create backup destination " + destinationPath};
    }
    std::uint8_t buffer[kCopyBufferBytes] = {};
    std::size_t copied = 0;
    while (source.available()) {
        const std::size_t read = source.read(buffer, sizeof(buffer));
        if (read == 0 || destination.write(buffer, read) != read) {
            source.close();
            destination.close();
            removeFileIfPresent(destinationPath);
            return {false, "Backup copy failed for " + sourcePath};
        }
        copied += read;
    }
    const std::size_t expected = source.size();
    destination.flush();
    source.close();
    destination.close();
    if (copied != expected) {
        removeFileIfPresent(destinationPath);
        return {false, "Backup copy size mismatch for " + sourcePath};
    }
    return {true, ""};
}

OperationResult copyFlatDirectory(const String& sourcePath, const String& destinationPath)
{
    if (!SD.mkdir(destinationPath)) {
        return {false, "Failed to create backup directory " + destinationPath};
    }
    File source = SD.open(sourcePath);
    if (!source || !source.isDirectory()) {
        return {false, "Failed to open backup source directory " + sourcePath};
    }
    File entry = source.openNextFile();
    while (entry) {
        const bool nested = entry.isDirectory();
        String name = entry.name();
        const int slash = name.lastIndexOf('/');
        if (slash >= 0) {
            name = name.substring(slash + 1);
        }
        entry.close();
        if (nested || name.isEmpty() || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) {
            source.close();
            return {false, "Chat storage contains an invalid backup entry"};
        }
        const OperationResult copied = copyFile(sourcePath + "/" + name,
                                                destinationPath + "/" + name);
        if (!copied.success) {
            source.close();
            return copied;
        }
        entry = source.openNextFile();
    }
    source.close();
    return {true, ""};
}

OperationResult writeManifest(const Settings& settings, const String& activeChatId)
{
    JsonDocument document;
    document["format"] = kBackupFormatVersion;
    document["created_at"] = static_cast<std::uint64_t>(std::time(nullptr));
    document["active_chat_id"] = activeChatId;
    JsonObject nonSecret = document["settings"].to<JsonObject>();
    nonSecret["api_base_url"] = settings.apiBaseUrl;
    nonSecret["model"] = settings.model;
    nonSecret["stt_base_url"] = settings.sttBaseUrl;
    nonSecret["stt_model"] = settings.sttModel;
    nonSecret["search_base_url"] = settings.webSearchBaseUrl;
    nonSecret["tts_base_url"] = settings.ttsBaseUrl;
    nonSecret["tts_model"] = settings.ttsModel;
    nonSecret["tts_voice"] = settings.ttsVoice;
    nonSecret["tts_auto"] = settings.ttsAutoPlay;
    nonSecret["tts_volume"] = settings.ttsVolume;
    nonSecret["brightness"] = settings.displayBrightness;
    nonSecret["sleep_minutes"] = settings.screenSleepMinutes;
    nonSecret["keyboard_repeat_ms"] = settings.keyboardRepeatMs;
    nonSecret["power_profile"] = settings.powerProfile;
    nonSecret["project_chat_history_quota_bytes"] =
        settings.projectChatHistoryQuotaBytes;
    File file = SD.open(String(kTemporaryPath) + "/manifest.json", FILE_WRITE);
    if (!file) {
        return {false, "Failed to create backup manifest"};
    }
    const std::size_t expected = measureJson(document);
    const std::size_t written = serializeJson(document, file);
    file.flush();
    file.close();
    return written == expected
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to write complete backup manifest"};
}

OperationResult parseManifest(Settings& restored, String& activeChatId, std::uint64_t& createdAt)
{
    File file = SD.open(String(kLatestPath) + "/manifest.json", FILE_READ);
    if (!file) {
        return {false, "No local backup manifest exists"};
    }
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, file);
    file.close();
    if (error || !document["format"].is<std::uint32_t>() ||
        document["format"].as<std::uint32_t>() < 1 ||
        document["format"].as<std::uint32_t>() > kBackupFormatVersion ||
        !document["created_at"].is<std::uint64_t>() ||
        !document["active_chat_id"].is<const char*>() ||
        !document["settings"].is<JsonObject>()) {
        return {false, "Backup manifest is invalid or unsupported"};
    }
    activeChatId = document["active_chat_id"].as<const char*>();
    if (activeChatId.isEmpty() || !isValidChatId(activeChatId.c_str())) {
        return {false, "Backup manifest contains an invalid active chat id"};
    }
    const std::uint32_t format = document["format"].as<std::uint32_t>();
    JsonObjectConst value = document["settings"].as<JsonObjectConst>();
    if (!value["api_base_url"].is<const char*>() || !value["model"].is<const char*>() ||
        !value["stt_base_url"].is<const char*>() || !value["stt_model"].is<const char*>() ||
        !value["search_base_url"].is<const char*>() ||
        !value["tts_base_url"].is<const char*>() || !value["tts_model"].is<const char*>() ||
        !value["tts_voice"].is<const char*>() || !value["tts_auto"].is<bool>() ||
        !value["tts_volume"].is<std::uint8_t>() ||
        !value["brightness"].is<std::uint8_t>() ||
        !value["sleep_minutes"].is<std::uint16_t>() ||
        !value["keyboard_repeat_ms"].is<std::uint16_t>() ||
        !value["power_profile"].is<std::uint8_t>() ||
        (format >= 2 &&
         !value["project_chat_history_quota_bytes"].is<std::uint32_t>())) {
        return {false, "Backup manifest is missing required non-secret settings"};
    }
    restored.apiBaseUrl = value["api_base_url"].as<const char*>();
    restored.model = value["model"].as<const char*>();
    restored.sttBaseUrl = value["stt_base_url"].as<const char*>();
    restored.sttModel = value["stt_model"].as<const char*>();
    restored.webSearchBaseUrl = value["search_base_url"].as<const char*>();
    restored.ttsBaseUrl = value["tts_base_url"].as<const char*>();
    restored.ttsModel = value["tts_model"].as<const char*>();
    restored.ttsVoice = value["tts_voice"].as<const char*>();
    restored.ttsAutoPlay = value["tts_auto"].as<bool>();
    restored.ttsVolume = value["tts_volume"].as<std::uint8_t>();
    restored.displayBrightness = value["brightness"].as<std::uint8_t>();
    restored.screenSleepMinutes = value["sleep_minutes"].as<std::uint16_t>();
    restored.keyboardRepeatMs = value["keyboard_repeat_ms"].as<std::uint16_t>();
    restored.powerProfile = value["power_profile"].as<std::uint8_t>();
    if (format >= 2) {
        restored.projectChatHistoryQuotaBytes =
            value["project_chat_history_quota_bytes"].as<std::uint32_t>();
        if (!isValidProjectChatHistoryQuota(restored.projectChatHistoryQuotaBytes)) {
            return {false, "Backup manifest contains an invalid chat history quota"};
        }
    }
    createdAt = document["created_at"].as<std::uint64_t>();
    return {true, ""};
}

OperationResult restoreChatsDirectory()
{
    OperationResult result = removeFlatDirectory(kRestoreChatsPath);
    if (!result.success) {
        return result;
    }
    result = removeFlatDirectory(kOldChatsPath);
    if (!result.success) {
        return result;
    }
    result = copyFlatDirectory(String(kLatestPath) + "/chats", kRestoreChatsPath);
    if (!result.success) {
        removeFlatDirectory(kRestoreChatsPath);
        return result;
    }
    if (!SD.rename(kChatsPath, kOldChatsPath)) {
        removeFlatDirectory(kRestoreChatsPath);
        return {false, "Failed to stage current chats for restore rollback"};
    }
    if (!SD.rename(kRestoreChatsPath, kChatsPath)) {
        SD.rename(kOldChatsPath, kChatsPath);
        removeFlatDirectory(kRestoreChatsPath);
        return {false, "Failed to activate restored chats"};
    }
    result = initializeChatStorage();
    const ChatsResult chats = result.success ? listChats()
                                             : ChatsResult{false, {}, result.error};
    if (!result.success || !chats.success || chats.chats.empty()) {
        removeFlatDirectory(kChatsPath);
        if (!SD.rename(kOldChatsPath, kChatsPath)) {
            return {false, "Restored chats were invalid and rollback also failed"};
        }
        return {false, result.success
            ? (chats.success ? String("Backup contains no chats") : chats.error)
            : result.error};
    }
    return {true, ""};
}

OperationResult rollbackChatsDirectory()
{
    OperationResult result = removeFlatDirectory(kChatsPath);
    if (!result.success) {
        return result;
    }
    return SD.rename(kOldChatsPath, kChatsPath)
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to roll back the original chats directory"};
}

}  // namespace

OperationResult createLocalBackup(const Settings& settings, const String& activeChatId)
{
    if (SD.cardType() == CARD_NONE) {
        return {false, "microSD is required for local backup"};
    }
    if (!SD.exists(kAssistantPath) || !SD.exists(kChatsPath)) {
        return {false, "CardMind storage is not initialized"};
    }
    if (!SD.exists(kBackupsPath) && !SD.mkdir(kBackupsPath)) {
        return {false, "Failed to create /assistant/backups"};
    }
    OperationResult result = removeBackupTree(kTemporaryPath);
    if (!result.success) {
        return result;
    }
    if (!SD.mkdir(kTemporaryPath)) {
        return {false, "Failed to create temporary backup directory"};
    }
    result = copyFlatDirectory(kChatsPath, String(kTemporaryPath) + "/chats");
    if (result.success && SD.exists(kBookmarksPath)) {
        result = copyFile(kBookmarksPath, String(kTemporaryPath) + "/file_bookmarks.json");
    }
    if (result.success) {
        result = writeManifest(settings, activeChatId);
    }
    if (!result.success) {
        removeBackupTree(kTemporaryPath);
        return result;
    }
    result = removeBackupTree(kOldPath);
    if (!result.success) {
        return result;
    }
    const bool hadLatest = SD.exists(kLatestPath);
    if (hadLatest && !SD.rename(kLatestPath, kOldPath)) {
        return {false, "Failed to stage previous backup"};
    }
    if (!SD.rename(kTemporaryPath, kLatestPath)) {
        if (hadLatest) {
            SD.rename(kOldPath, kLatestPath);
        }
        return {false, "Failed to activate new backup"};
    }
    return hadLatest ? removeBackupTree(kOldPath) : OperationResult{true, ""};
}

OperationResult restoreLocalBackup(Settings& settings, String& activeChatId)
{
    Settings restored = settings;
    String restoredActiveChatId;
    std::uint64_t createdAt = 0;
    OperationResult result = parseManifest(restored, restoredActiveChatId, createdAt);
    if (!result.success) {
        return result;
    }
    const Settings originalSettings = settings;
    result = saveSettings(restored);
    if (!result.success) {
        return result;
    }
    result = restoreChatsDirectory();
    if (!result.success) {
        saveSettings(originalSettings);
        return result;
    }
    removeFileIfPresent(kRestoreBookmarksPath);
    removeFileIfPresent(kOldBookmarksPath);
    const String backupBookmarks = String(kLatestPath) + "/file_bookmarks.json";
    const bool hadBookmarks = SD.exists(kBookmarksPath);
    if (hadBookmarks && !SD.rename(kBookmarksPath, kOldBookmarksPath)) {
        result = {false, "Failed to stage workspace bookmarks for restore"};
    }
    if (result.success && SD.exists(backupBookmarks)) {
        result = copyFile(backupBookmarks, kRestoreBookmarksPath);
        if (result.success && !SD.rename(kRestoreBookmarksPath, kBookmarksPath)) {
            if (hadBookmarks) {
                SD.rename(kOldBookmarksPath, kBookmarksPath);
            }
            result = {false, "Failed to activate restored workspace bookmarks"};
        }
    }
    if (result.success) {
        result = saveActiveChatId(restoredActiveChatId);
    }
    if (!result.success) {
        if (SD.exists(kBookmarksPath)) {
            SD.remove(kBookmarksPath);
        }
        if (hadBookmarks && SD.exists(kOldBookmarksPath)) {
            SD.rename(kOldBookmarksPath, kBookmarksPath);
        }
        const OperationResult chatRollback = rollbackChatsDirectory();
        const OperationResult settingsRollback = saveSettings(originalSettings);
        if (!chatRollback.success) {
            return chatRollback;
        }
        if (!settingsRollback.success) {
            return settingsRollback;
        }
        return result;
    }
    result = removeFlatDirectory(kOldChatsPath);
    if (result.success) {
        result = removeFileIfPresent(kOldBookmarksPath);
    }
    if (!result.success) {
        return result;
    }
    settings = restored;
    activeChatId = restoredActiveChatId;
    return {true, ""};
}

OperationResult localBackupSummary(String& summary)
{
    Settings settings;
    String activeChatId;
    std::uint64_t createdAt = 0;
    const OperationResult result = parseManifest(settings, activeChatId, createdAt);
    if (!result.success) {
        return result;
    }
    summary = "Latest backup: " + String(static_cast<unsigned long long>(createdAt)) +
        " · active chat " + activeChatId;
    return {true, ""};
}

}  // namespace cardputer
