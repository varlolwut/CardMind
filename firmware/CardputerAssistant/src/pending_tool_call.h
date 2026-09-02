#pragma once

#include "app_types.h"
#include "tool_catalog.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace cardputer {

constexpr const char* kPendingToolCallPath = "/assistant/v2/pending_tool.json";
constexpr std::size_t kMaximumPendingToolCallIdBytes = 256;
constexpr std::size_t kMaximumPendingToolArgumentsBytes = 32768;

enum class PendingToolConfirmationReason : std::uint8_t {
    PolicyAsk,
    Mandatory,
};

enum class PendingToolCallState : std::uint8_t {
    Awaiting,
    ClaimedApprove,
    Denied,
};

enum class PendingToolTargetKind : std::uint8_t {
    None,
    File,
    Ssh,
};

enum class PendingToolPreviewKind : std::uint8_t {
    Generic,
    FileReplacement,
    SshCommand,
    PythonSource,
};

struct PendingToolTargetIdentity {
    PendingToolTargetKind kind = PendingToolTargetKind::None;
    String name;
    bool exists = false;
    std::uint64_t size = 0;
    std::string sha256;
};

struct PendingToolContinuation {
    ToolCall call;
    ToolMessageIntent intent;
    std::uint8_t remainingRequiredGroupsAfterCall;
    std::uint8_t completedToolRoundsBeforeCall;
    std::uint32_t toolOutputBytesBeforeCall;
    bool completedWorkspaceWriteBeforeCall;
};

inline bool pendingToolContinuationAllowsSchema(
    ToolSchemaId schema,
    std::uint8_t remainingRequiredGroupsAfterCall)
{
    return schema != ToolSchemaId::PythonRun ||
        remainingRequiredGroupsAfterCall == 0;
}

struct PendingToolCall {
    String pendingId;
    String projectId;
    String chatId;
    std::uint32_t projectRevision;
    std::uint32_t chatRevision;
    std::uint32_t chatMessageCount;
    PendingToolCallState state;
    PendingToolConfirmationReason reason;
    PendingToolContinuation continuation;
    PendingToolTargetIdentity target;
};

struct PendingToolCallResult {
    bool success;
    bool found;
    PendingToolCall pending;
    String error;
};

struct PendingToolPreview {
    String pendingId;
    ToolSchemaId schema;
    PendingToolConfirmationReason reason;
    PendingToolPreviewKind kind;
    String toolName;
    String targetName;
    std::uint32_t currentBytes;
    std::uint32_t proposedBytes;
    std::string body;
    bool truncated;
};

struct PendingToolPreviewResult {
    bool success;
    PendingToolPreview preview;
    String error;
};

OperationResult savePendingToolCall(
    const ToolRequestPlan& plan,
    const String& projectId,
    const String& chatId,
    const PendingToolContinuation& continuation);
OperationResult replaceTerminalPendingToolCall(
    const String& expectedPendingId,
    PendingToolCallState expectedState,
    const ToolRequestPlan& plan,
    const String& projectId,
    const String& chatId,
    const PendingToolContinuation& continuation);
PendingToolCallResult loadPendingToolCall();
PendingToolPreviewResult loadPendingToolPreview(const String& pendingId);
bool pendingToolCallIsResumableThisBoot(const String& pendingId);
PendingToolCallResult claimPendingToolCallApproval(
    const String& pendingId,
    const ToolRequestPlan& currentPlan);
OperationResult revalidateClaimedPendingToolCall(
    const PendingToolCall& pending);
PendingToolCallResult denyPendingToolCall(const String& pendingId);
OperationResult clearPendingToolCall(
    const String& pendingId,
    PendingToolCallState state);

}  // namespace cardputer
