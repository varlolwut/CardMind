#pragma once

#include "api_client.h"
#include "ssh_command_options.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace cardputer {

struct SshCommandArgumentsResult {
    bool success;
    String command;
    std::uint32_t timeoutMs;
    std::size_t maximumInlineOutputBytes;
    String error;
};

bool isSshToolName(const std::string& name);
bool sshToolIsAvailable();
SshCommandArgumentsResult parseSshCommandArguments(
    const std::string& argumentsJson);
ToolExecutionResult executeSshTool(const ToolCall& call,
                                   const CancelCallback& isCancelled);

}  // namespace cardputer
