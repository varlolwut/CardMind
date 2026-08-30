#pragma once

#include "app_types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cardputer {

std::string removeLastUtf8CodePoint(const std::string& value);
std::size_t previousUtf8Boundary(const std::string& value, std::size_t index);
std::size_t nextUtf8Boundary(const std::string& value, std::size_t index);
std::string insertUtf8At(const std::string& value,
                         std::size_t index,
                         const std::string& insertion);
std::string eraseUtf8Before(const std::string& value, std::size_t index);
std::string mapKeyToRussian(char key);
std::vector<std::string> wrapUtf8Text(const std::string& value, std::size_t maxCells);
bool extractSseData(const std::string& line, std::string& data);
bool isValidUtf8(const char* value, std::size_t size);
bool isValidUtf8(const std::string& value);
std::string buildVersionedApiUrl(const std::string& baseUrl, const std::string& versionedPath);
std::string ellipsizeUtf8(const std::string& value, std::size_t maxCells);
std::string makeChatTitle(const std::string& prompt, std::size_t maxCells);
bool isValidChatId(const std::string& value);
bool isValidWorkspaceFilename(const std::string& value);
bool isWorkspaceTextFile(const std::string& name);
bool isValidStorageRelativePath(const std::string& path, std::size_t maximumBytes);
ContextWindowResult fitMessagesToByteBudget(const std::vector<Message>& messages,
                                            std::size_t maximumBytes);
ContextWindowResult fitOwnedMessagesToByteBudget(std::vector<Message> messages,
                                                 std::size_t maximumBytes);
std::vector<Message> takeMessagesDroppedToByteBudget(
    std::vector<Message> messages, std::size_t maximumBytes);
struct RetryRequestResult {
    bool success;
    std::string prompt;
    std::vector<Message> messages;
    std::string error;
};
RetryRequestResult prepareRetryRequest(const std::vector<Message>& messages,
                                       std::size_t maximumBytes);
std::vector<Message> unsummarizedChatTail(const ChatDocument& chat);
std::vector<Message> takeUnsummarizedChatTail(ChatDocument chat);
struct ContextSummaryPromptResult {
    bool success;
    std::string prompt;
    std::uint32_t includedMessages;
    std::string error;
};
struct ResolvedProjectRequestPolicy {
    String model;
    std::uint32_t contextByteBudget;
    std::uint32_t maximumOutputTokens;
    bool automaticCompaction;
};
struct ContextUsage {
    std::size_t retainedBytes;
    std::uint32_t retainedMessages;
    std::uint32_t droppedMessages;
    std::uint32_t summarizedMessages;
    std::uint32_t totalMessages;
};
ContextSummaryPromptResult buildContextSummaryPrompt(
    const std::string& previousSummary,
    const std::vector<Message>& messages,
    std::size_t maximumBytes);
std::uint32_t resolveRequestOutputTokens(std::uint32_t projectTokens,
                                         std::uint32_t requestOverrideTokens);
ResolvedProjectRequestPolicy resolveProjectRequestPolicy(
    const Settings& settings,
    const ProjectDocument& project,
    const ChatDocument& chat,
    std::uint32_t requestOutputTokens);
bool shouldAutomaticallyCompactRequest(
    const ResolvedProjectRequestPolicy& policy,
    std::uint32_t droppedMessages);
ContextUsage resolveContextUsage(const ChatDocument& chat,
                                 std::size_t maximumBytes);
bool requestsWorkspaceAccess(const std::string& prompt);
bool requestsWorkspaceWrite(const std::string& prompt);
bool requestsWebSearch(const std::string& prompt);
bool isWebSearchToolName(const std::string& name);
bool isWebFetchToolName(const std::string& name);

}  // namespace cardputer
