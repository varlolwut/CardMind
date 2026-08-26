#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef ARDUINO
#include <Arduino.h>
#else
using String = std::string;
#endif

namespace cardputer {

enum class KeyboardLayout {
    English,
    Russian,
};

struct Settings {
    String wifiSsid;
    String wifiPassword;
    String apiKey;
    String apiBaseUrl;
    String model;
    String globalInstructions;
    String sttApiKey;
    String sttBaseUrl;
    String sttModel;
    String webSearchApiKey;
    String webSearchBaseUrl;
    String ttsApiKey;
    String ttsBaseUrl;
    String ttsModel;
    String ttsVoice;
    bool ttsAutoPlay;
    std::uint8_t ttsVolume;
    std::uint8_t displayBrightness;
    std::uint16_t screenSleepMinutes;
    std::uint16_t keyboardRepeatMs;
    std::uint8_t powerProfile;
    std::uint32_t projectChatHistoryQuotaBytes;
};

struct Message {
    String role;
    std::string content;
};

struct OperationResult {
    bool success;
    String error;
};

struct ModelsResult {
    bool success;
    std::vector<String> models;
    String error;
};

struct ChatResult {
    bool success;
    std::string response;
    String error;
};

struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;
};

struct ToolExecutionResult {
    bool success;
    std::string output;
    String error;
};

struct WorkspaceFile {
    String name;
    std::uint32_t size;
    bool directory = false;
};

struct WorkspaceChunkResult {
    bool success;
    std::string content;
    std::uint32_t offset;
    std::uint32_t nextOffset;
    std::uint32_t totalBytes;
    bool eof;
    String error;
};

struct WorkspaceFindResult {
    bool success;
    bool found;
    std::uint32_t offset;
    String error;
};

struct WorkspaceBookmarkResult {
    bool success;
    bool found;
    std::uint32_t offset;
    String error;
};

struct VoiceRecordingResult {
    bool success;
    std::uint32_t sampleCount;
    std::uint16_t peakLevel;
    std::uint16_t meanLevel;
    String error;
};

struct ChatSummary {
    String id;
    String title;
    std::uint64_t updatedAt;
    std::uint32_t messageCount;
    bool pinned = false;
    bool archived = false;
    std::uint32_t archivedMessageCount = 0;
    std::uint32_t revision = 0;
};

struct ChatDocument {
    ChatSummary summary;
    std::vector<Message> messages;
    std::string instructions;
    std::string draft;
    bool sshToolsEnabled = false;
    String projectId;
    std::string contextSummary;
    std::uint32_t summarizedMessageCount = 0;
};

struct WorkspaceFilesPageResult {
    bool success;
    std::vector<WorkspaceFile> files;
    std::uint32_t nextOffset;
    bool eof;
    String error;
};

struct ChatsResult {
    bool success;
    std::vector<ChatSummary> chats;
    String error;
};

struct ChatDocumentResult {
    bool success;
    ChatDocument chat;
    String error;
};

struct ProjectSummary {
    String id;
    String title;
    std::uint64_t updatedAt;
    std::uint32_t chatCount;
    bool pinned = false;
    bool archived = false;
    std::uint32_t revision = 0;
};

struct ProjectDocument {
    ProjectSummary summary;
    std::string instructions;
    String activeChatId;
    String model;
    String apiProfile;
    String toolPolicy;
    String sshProfile;
    std::uint32_t contextByteBudget = 32768;
    std::uint32_t maximumOutputTokens = 1024;
    bool automaticCompaction = true;
    std::uint32_t chatIndexRevision = 0;
    std::uint32_t sharedLinksRevision = 0;
};

struct ProjectDocumentResult {
    bool success;
    ProjectDocument project;
    String error;
};

struct ProjectsPageResult {
    bool success;
    std::vector<ProjectSummary> projects;
    std::uint32_t nextOffset;
    bool eof;
    String error;
};

struct ProjectChatsPageResult {
    bool success;
    std::vector<ChatSummary> chats;
    std::uint32_t nextOffset;
    bool eof;
    String error;
};

struct SharedFileLink {
    String path;
};

struct SharedFileLinksPageResult {
    bool success;
    std::vector<SharedFileLink> links;
    std::uint32_t nextOffset;
    bool eof;
    String error;
};

struct SharedFileLinkResult {
    bool success;
    bool linked;
    String error;
};

enum class ProjectMigrationState {
    Uninitialized,
    Staging,
    Validated,
    Committed,
};

struct ProjectStorageManifest {
    std::uint32_t version;
    ProjectMigrationState migrationState;
    String activeProjectId;
    String sharedRoot;
    std::uint32_t revision;
};

struct ProjectStorageManifestResult {
    bool success;
    ProjectStorageManifest manifest;
    String error;
};

struct StorageSizeResult {
    bool success;
    std::uint64_t bytes;
    String error;
};

struct ContextWindowResult {
    std::vector<Message> retained;
    std::uint32_t droppedMessages;
    std::size_t retainedBytes;
};

struct TranscriptionResult {
    bool success;
    std::string text;
    String error;
};

}  // namespace cardputer
