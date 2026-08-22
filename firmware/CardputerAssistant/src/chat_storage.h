#pragma once

#include "app_types.h"

#include <cstddef>

namespace cardputer {

constexpr std::size_t kMaximumStoredChats = 20;
constexpr std::size_t kMaximumStoredMessages = 64;
constexpr std::size_t kMaximumStoredHistoryBytes = 32768;
constexpr std::size_t kMaximumChatTitleCells = 28;

OperationResult initializeChatStorage();
ChatsResult listChats();
ChatDocumentResult createChat(const String& title);
ChatDocumentResult loadChat(const String& id);
OperationResult saveChat(const ChatDocument& chat);
OperationResult deleteChat(const String& id);

}  // namespace cardputer
