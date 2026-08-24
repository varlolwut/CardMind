#pragma once

#include "app_types.h"
#include "chat_storage.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

namespace cardputer {

struct WebConsoleRuntimeState {
    const String& status;
    const String& firmwareVersion;
    bool sshTerminalOpen;
};

OperationResult buildWebConsoleState(const Settings& settings,
                                     const ChatDocument& activeChat,
                                     const std::vector<ChatSummary>& chats,
                                     const WebConsoleRuntimeState& runtime,
                                     JsonDocument& document);

}  // namespace cardputer
