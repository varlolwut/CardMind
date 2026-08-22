#pragma once

#include <Arduino.h>

#include <cstdint>
#include <string>
#include <vector>

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
};

struct WorkspaceFilesResult {
    bool success;
    std::vector<WorkspaceFile> files;
    String error;
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
};

struct ChatDocument {
    ChatSummary summary;
    std::vector<Message> messages;
    std::string instructions;
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

struct TranscriptionResult {
    bool success;
    std::string text;
    String error;
};

}  // namespace cardputer
