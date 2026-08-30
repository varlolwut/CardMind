#pragma once

#include "app_types.h"
#include "chat_storage.h"

#include <FS.h>

#include <cstddef>

namespace cardputer {

constexpr std::uint32_t kProjectChatFormatVersion = 1;
constexpr std::size_t kMaximumProjectChatInstructionsBytes = 16384;
constexpr std::size_t kMaximumProjectChatDraftBytes = 16384;
constexpr std::size_t kMaximumProjectChatSummaryBytes = 131072;

struct IndexedMessagesPageResult {
    bool success;
    std::vector<Message> messages;
    std::uint32_t nextMessageIndex;
    bool eof;
    String error;
};

struct ProjectChatAppendPlanResult {
    bool success;
    std::uint64_t newHistoryBytes;
    std::uint64_t requiredFreeBytes;
    String error;
};

ProjectChatAppendPlanResult planProjectChatAppend(
    std::uint64_t currentHistoryBytes,
    std::uint64_t appendedHistoryBytes,
    std::uint64_t historyQuotaBytes,
    std::uint64_t markerBytes,
    std::uint64_t stagedTailBytes,
    std::uint64_t stagedMetadataBytes,
    std::uint64_t freeBytes,
    std::uint64_t operationalFloorBytes);

ChatDocumentResult createProjectChat(
    const String& projectId,
    const String& title,
    const ScopedToolPermissionPolicy& toolPolicy);
OperationResult saveProjectChatMetadata(const ChatDocument& chat);
OperationResult appendProjectChatMessages(const String& projectId,
                                           const String& chatId,
                                           const std::vector<Message>& messages,
                                           std::uint64_t updatedAt,
                                           std::uint32_t historyQuotaBytes);
OperationResult clearProjectChatHistory(const String& projectId, const String& chatId);
OperationResult deleteProjectChat(const String& projectId, const String& chatId);
ChatDocumentResult duplicateProjectChat(const String& projectId, const String& chatId);
ArchivedMessagesPageResult readProjectChatMessages(const String& projectId,
                                                   const String& chatId,
                                                   std::uint32_t offset,
                                                   std::size_t maximumMessages,
                                                   std::size_t maximumBytes);
IndexedMessagesPageResult readProjectChatMessagesByIndex(
    const String& projectId,
    const String& chatId,
    std::uint32_t firstMessageIndex,
    std::size_t maximumMessages,
    std::size_t maximumBytes);
OperationResult exportProjectChatMarkdown(const String& projectId,
                                          const String& chatId,
                                          const String& filename);
OperationResult importLegacyChatToProject(const String& projectId,
                                          const ChatDocument& legacyChat);
ChatDocumentResult loadProjectChat(const String& projectId,
                                   const String& chatId,
                                   std::size_t maximumTailMessages,
                                   std::size_t maximumTailBytes);
ChatDocumentResult loadProjectChatMetadata(const String& projectId,
                                           const String& chatId);
OperationResult validateProjectChat(const String& projectId, const String& chatId);
OperationResult cloneProjectChat(const String& sourceProjectId,
                                 const String& destinationProjectId,
                                 const String& chatId);
OperationResult writeProjectChatBundleRecords(File& output,
                                              const String& projectId,
                                              const String& chatId);
StorageSizeResult measureProjectChatBundleRecords(const String& projectId,
                                                  const String& chatId);
OperationResult importProjectChatBundleRecords(File& input,
                                               const String& destinationProjectId,
                                               const ChatDocument& metadata,
                                               std::uint32_t messageCount);

}  // namespace cardputer
