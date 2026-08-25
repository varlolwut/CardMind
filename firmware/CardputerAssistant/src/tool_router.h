#pragma once

#include "api_client.h"
#include "app_types.h"

#include <string>

namespace cardputer {

struct ChatToolPolicy {
    bool workspaceEnabled;
    bool webSearchEnabled;
    bool sshEnabled;
};

ChatToolPolicy resolveChatToolPolicy(const Settings& settings,
                                     const std::string& prompt,
                                     bool chatAllowsSsh,
                                     bool sshAvailable);
bool chatToolPolicyIsEnabled(const ChatToolPolicy& policy);
ToolExecutionResult routeToolCall(const Settings& settings,
                                  const ChatToolPolicy& policy,
                                  const ToolCall& call,
                                  const CancelCallback& isCancelled);
ToolExecutionResult routeProjectToolCall(const Settings& settings,
                                         const ChatToolPolicy& policy,
                                         const String& projectId,
                                         const ToolCall& call,
                                         const CancelCallback& isCancelled);

}  // namespace cardputer
