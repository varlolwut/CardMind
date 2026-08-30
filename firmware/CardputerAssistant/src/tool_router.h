#pragma once

#include "api_client.h"
#include "app_types.h"
#include "tool_catalog.h"

#include <string>

namespace cardputer {

struct PendingToolDecisionResult {
    bool success;
    PendingToolCall pending;
    ToolExecutionResult toolResult;
    String error;
};

ToolPolicyResolutionResult resolveChatToolPermissions(
    const Settings& settings,
    const ProjectDocument& project,
    const ChatDocument& chat,
    const ToolMessageIntent& intent,
    bool filesReadable,
    bool filesWritable,
    bool webStorageWritable,
    bool sshAvailable);
ToolRequestPlan resolveChatToolRequestPlan(
    const Settings& settings,
    const ProjectDocument& project,
    const ChatDocument& chat,
    const ToolMessageIntent& intent,
    bool filesReadable,
    bool filesWritable,
    bool webStorageWritable,
    bool sshAvailable);
ToolExecutionResult routeToolCall(const Settings& settings,
                                  const ToolRequestPlan& plan,
                                  const ToolCall& call,
                                  const CancelCallback& isCancelled);
ToolExecutionResult routeProjectToolCall(const Settings& settings,
                                         const ToolRequestPlan& plan,
                                         const String& projectId,
                                         const ToolCall& call,
                                         const CancelCallback& isCancelled);
PendingToolDecisionResult approvePendingProjectToolCall(
    const Settings& settings,
    const ToolRequestPlan& currentPlan,
    const String& pendingId,
    const CancelCallback& isCancelled);
PendingToolDecisionResult denyPendingProjectToolCall(
    const String& pendingId);

}  // namespace cardputer
