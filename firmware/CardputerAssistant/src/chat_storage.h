#pragma once

#include "app_types.h"

#include <cstddef>

namespace cardputer {

constexpr std::size_t kMaximumStoredChats = 20;
constexpr std::size_t kMaximumStoredMessages = 64;
constexpr std::size_t kMaximumStoredHistoryBytes = 32768;
constexpr std::size_t kMaximumChatTitleCells = 28;
constexpr std::size_t kMaximumChatInstructionsBytes = 2048;
constexpr std::size_t kMaximumChatDraftBytes = 1200;
constexpr std::size_t kMaximumArchivedChatBytes = 2097152;

struct HistoryFitResult {
    std::vector<Message> retained;
    std::vector<Message> archived;
};

struct ArchivedMessagesPageResult {
    bool success;
    std::vector<Message> messages;
    std::uint32_t nextOffset;
    bool eof;
    String error;
};

OperationResult initializeChatStorage();
ChatsResult listChats();
ChatDocumentResult createChat(const String& title);
ChatDocumentResult loadChat(const String& id);
OperationResult saveChat(const ChatDocument& chat);
OperationResult deleteChat(const String& id);
OperationResult archiveChatMessages(const String& id,
                                    const std::vector<Message>& messages);
ArchivedMessagesPageResult readArchivedChatMessages(const String& id,
                                                     std::uint32_t offset,
                                                     std::size_t maximumMessages,
                                                     std::size_t maximumBytes);
OperationResult clearChatHistory(const String& id);
OperationResult exportChatToWorkspace(const String& id, const String& filename);
OperationResult exportChatBundleToWorkspace(const String& id, const String& filename);
ChatDocumentResult importChatBundleFromWorkspace(const String& filename);
ChatDocumentResult duplicateChat(const String& id);
HistoryFitResult fitHistoryToActiveContext(const std::vector<Message>& messages);

}  // namespace cardputer
