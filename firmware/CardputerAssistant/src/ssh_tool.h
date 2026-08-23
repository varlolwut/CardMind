#pragma once

#include "api_client.h"

namespace cardputer {

bool isSshToolName(const std::string& name);
bool sshToolIsAvailable();
ToolExecutionResult executeSshTool(const ToolCall& call,
                                   const CancelCallback& isCancelled);

}  // namespace cardputer
