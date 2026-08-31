#pragma once

#include "app_types.h"
#include "chat_storage.h"
#include "ssh_client.h"
#include "tool_policy.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

namespace cardputer {

struct WebConsoleRuntimeState {
    const String& status;
    const String& firmwareVersion;
    bool sshTerminalOpen;
};

void buildWebConsoleStatusState(const WebConsoleRuntimeState& runtime,
                                bool diagnosticsEnabled,
                                JsonDocument& document);
void buildWebConsoleChatsState(const std::vector<ChatSummary>& chats,
                               std::uint32_t revision,
                               JsonDocument& document);
OperationResult buildWebConsoleChatState(
    const Settings& settings,
    const ProjectDocument& activeProject,
    const ChatDocument& activeChat,
    const ToolPolicyResolutionResult& toolPermissions,
    std::uint64_t availableSshProfileId,
    std::size_t maximumContextBytes,
    std::uint32_t revision,
    JsonDocument& document);
void buildWebConsoleFilesState(const std::vector<WorkspaceFile>& files,
                               std::uint64_t totalBytes,
                               std::uint64_t usedBytes,
                               std::uint32_t revision,
                               JsonDocument& document);
void buildWebConsoleSshState(const std::vector<SshProfile>& profiles,
                             std::size_t selected,
                             bool privateKeyInstalled,
                             const WebConsoleRuntimeState& runtime,
                             std::uint32_t revision,
                             JsonDocument& document);
OperationResult buildWebConsoleSettingsState(
    const Settings& settings,
    const WebConsoleRuntimeState& runtime,
    std::uint32_t revision,
    JsonDocument& document);

}  // namespace cardputer
