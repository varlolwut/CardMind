#include "tool_router.h"

#include "file_workspace.h"
#include "sftp_tool.h"
#include "ssh_tool.h"
#include "storage.h"
#include "tool_activity.h"
#include "web_search_client.h"

#include <utility>

namespace cardputer {
namespace {

enum class ToolRouteAuthorization : std::uint8_t {
    Allow = 0,
    Ask = 1,
    Deny = 2,
    Unavailable = 3,
    InvalidPlan = 4,
};

ToolExecutionResult unavailableTool(const std::string& name)
{
    return {
        false,
        "{\"ok\":false,\"error\":\"unsupported tool\"}",
        "API requested unsupported tool '" + String(name.c_str()) + "'",
    };
}

ToolExecutionResult deniedTool(const std::string& name)
{
    return {
        false,
        "{\"ok\":false,\"error\":\"permission denied\"}",
        "Tool '" + String(name.c_str()) + "' is denied for this request",
    };
}

ToolExecutionResult confirmationRequiredTool(const std::string& name)
{
    return {
        false,
        "",
        "Tool '" + String(name.c_str()) + "' requires confirmation",
        ToolExecutionOutcome::AwaitingConfirmation,
    };
}

ToolExecutionResult invalidToolCall(const String& error)
{
    return {
        false,
        "{\"ok\":false,\"error\":\"invalid tool call\"}",
        error,
    };
}

ToolExecutionResult unavailableConfiguredTool(const std::string& name)
{
    return {
        false,
        "{\"ok\":false,\"error\":\"tool unavailable\"}",
        "Tool '" + String(name.c_str()) + "' is unavailable for this request",
    };
}

ToolExecutionResult invalidToolPlan()
{
    return {
        false,
        "{\"ok\":false,\"error\":\"invalid tool permission plan\"}",
        "Tool execution rejected an invalid permission plan",
    };
}

ToolExecutionResult activityUnavailable(const String& error)
{
    return {
        false,
        "{\"ok\":false,\"error\":\"tool activity unavailable\"}",
        "Tool execution did not start because activity journaling failed: " + error,
    };
}

ToolRouteAuthorization toolRouteAuthorization(
    const ToolRequestPlan& plan,
    const ToolCatalogEntry& entry) noexcept
{
    if (!toolRequestPlanIsConsistent(plan)) {
        return ToolRouteAuthorization::InvalidPlan;
    }
    const ToolPermissionDecision decision = toolRequestPlanDecision(
        plan, entry.schema);
    if (!toolRequestPlanIncludesSchema(plan, entry.schema)) {
        return decision == ToolPermissionDecision::Unavailable
            ? ToolRouteAuthorization::Unavailable
            : ToolRouteAuthorization::Deny;
    }
    switch (decision) {
        case ToolPermissionDecision::Allow:
            return ToolRouteAuthorization::Allow;
        case ToolPermissionDecision::Ask:
            return ToolRouteAuthorization::Ask;
        case ToolPermissionDecision::Unavailable:
            return ToolRouteAuthorization::Unavailable;
        case ToolPermissionDecision::Deny:
            return ToolRouteAuthorization::Deny;
    }
    return ToolRouteAuthorization::InvalidPlan;
}

ToolExecutionResult toolAuthorizationFailure(
    ToolRouteAuthorization authorization,
    const std::string& name)
{
    switch (authorization) {
        case ToolRouteAuthorization::Ask:
            return confirmationRequiredTool(name);
        case ToolRouteAuthorization::Unavailable:
            return unavailableConfiguredTool(name);
        case ToolRouteAuthorization::Deny:
            return deniedTool(name);
        case ToolRouteAuthorization::InvalidPlan:
            return invalidToolPlan();
        case ToolRouteAuthorization::Allow:
            break;
    }
    return invalidToolPlan();
}

bool isFilesSchema(ToolSchemaId schema) noexcept
{
    return schema == ToolSchemaId::ListFiles ||
        schema == ToolSchemaId::ReadFile ||
        schema == ToolSchemaId::WriteFile ||
        schema == ToolSchemaId::AppendFile;
}

ToolExecutionResult dispatchToolCall(
    const Settings& settings,
    ToolSchemaId schema,
    const ToolCall& call,
    const CancelCallback& isCancelled)
{
    switch (schema) {
        case ToolSchemaId::WebSearch:
            return executeWebSearchTool(settings, call, isCancelled);
        case ToolSchemaId::WebFetch:
            return executeWebFetchTool(settings, call, isCancelled);
        case ToolSchemaId::ListFiles:
        case ToolSchemaId::ReadFile:
        case ToolSchemaId::WriteFile:
        case ToolSchemaId::AppendFile:
            return executeControlledWorkspaceTool(call, isCancelled);
        case ToolSchemaId::SshCommand:
            return executeSshTool(call, isCancelled);
        case ToolSchemaId::SftpList:
            return executeSftpListTool(call, isCancelled);
        case ToolSchemaId::SftpRead:
            return executeSftpReadTool(call, isCancelled);
        case ToolSchemaId::SftpWrite:
            return executeSftpWriteTool(call, isCancelled);
        case ToolSchemaId::SftpMove:
            return executeSftpMoveTool(call, isCancelled);
        case ToolSchemaId::Count:
            break;
    }
    return unavailableTool(call.name);
}

ToolExecutionResult dispatchProjectToolCall(
    const Settings& settings,
    ToolSchemaId schema,
    const String& projectId,
    const ToolCall& call,
    const CancelCallback& isCancelled)
{
    if (isFilesSchema(schema)) {
        return executeControlledProjectWorkspaceTool(
            projectId, call, isCancelled);
    }
    return dispatchToolCall(settings, schema, call, isCancelled);
}

template <typename Dispatch>
ToolExecutionResult executeAuditedToolCall(
    const ToolCall& call,
    const CancelCallback& isCancelled,
    Dispatch dispatch)
{
    ToolActivityResult started = startToolActivity(call.name);
    if (!started.success) {
        return activityUnavailable(started.error);
    }
    bool cancelled = false;
    const CancelCallback latchedCancellation = [&]() {
        cancelled = cancelled || isCancelled();
        return cancelled;
    };
    const std::uint32_t startedAt = millis();
    ToolExecutionResult result = dispatch(latchedCancellation);
    if (cancelled) {
        result.success = false;
        result.outcome = ToolExecutionOutcome::Cancelled;
    }
    const ToolActivityStatus status =
        result.outcome == ToolExecutionOutcome::Cancelled
        ? ToolActivityStatus::Canceled
        : (result.success ? ToolActivityStatus::Succeeded
                          : ToolActivityStatus::Failed);
    const std::uint32_t outputBytes =
        result.outcome == ToolExecutionOutcome::Cancelled
        ? 0
        : static_cast<std::uint32_t>(result.output.size());
    const OperationResult finished = finishToolActivity(
        started.activity, status,
        static_cast<std::uint32_t>(millis() - startedAt), outputBytes,
        {
            result.hasExitStatus,
            static_cast<std::int32_t>(result.exitStatus),
        });
    if (!finished.success) {
        Serial.printf(
            "ERROR event=tool_activity_finish name=%s error=%s\n",
            call.name.c_str(), finished.error.c_str());
    }
    return result;
}

}  // namespace

ToolPolicyResolutionResult resolveChatToolPermissions(
    const Settings& settings,
    const ProjectDocument& project,
    const ChatDocument& chat,
    const ToolMessageIntent& intent,
    bool filesReadable,
    bool filesWritable,
    bool webStorageWritable,
    bool sshAvailable)
{
    ToolAvailabilitySet availability = {};
    const bool webAvailable = webStorageWritable &&
        webSearchSettingsAreComplete(settings);
    availability[static_cast<std::size_t>(ToolCapability::WebSearch)] =
        webAvailable;
    availability[static_cast<std::size_t>(ToolCapability::WebFetch)] =
        webAvailable;
    availability[static_cast<std::size_t>(ToolCapability::FilesRead)] =
        filesReadable;
    availability[static_cast<std::size_t>(
        ToolCapability::FilesWriteDelete)] = filesWritable;
    availability[static_cast<std::size_t>(ToolCapability::SshRead)] =
        false;
    availability[static_cast<std::size_t>(ToolCapability::SshMutate)] =
        sshAvailable && project.sshProfile.isEmpty();
    availability[static_cast<std::size_t>(ToolCapability::SftpReadWrite)] =
        sshAvailable && project.sshProfile.isEmpty();
    return resolveToolPolicy(
        defaultGlobalToolPermissionPolicy(), settings.masterToolPolicy,
        project.toolPolicy, chat.toolPolicy, intent, availability);
}

ToolRequestPlan resolveChatToolRequestPlan(
    const Settings& settings,
    const ProjectDocument& project,
    const ChatDocument& chat,
    const ToolMessageIntent& intent,
    bool filesReadable,
    bool filesWritable,
    bool webStorageWritable,
    bool sshAvailable)
{
    const ToolPolicyResolutionResult resolution = resolveChatToolPermissions(
        settings, project, chat, intent, filesReadable, filesWritable,
        webStorageWritable, sshAvailable);
    return buildToolRequestPlan(resolution, intent);
}

ToolExecutionResult routeToolCall(const Settings& settings,
                                  const ToolRequestPlan& plan,
                                  const ToolCall& call,
                                  const CancelCallback& isCancelled)
{
    const ToolCatalogEntry* entry = toolCatalogEntryForName(call.name);
    if (entry == nullptr) {
        return unavailableTool(call.name);
    }
    const ToolRouteAuthorization authorization = toolRouteAuthorization(
        plan, *entry);
    if (authorization != ToolRouteAuthorization::Allow) {
        return toolAuthorizationFailure(authorization, call.name);
    }
    if (toolConfirmationReason(
            ToolPermissionDecision::Allow, entry->schema, false) !=
        ToolConfirmationReason::None) {
        return confirmationRequiredTool(call.name);
    }
    if (entry->schema == ToolSchemaId::WriteFile) {
        const WorkspaceWriteTargetResult target = inspectWorkspaceWriteTarget(call);
        if (!target.success) {
            return invalidToolCall(target.error);
        }
        if (toolConfirmationReason(
                ToolPermissionDecision::Allow, entry->schema,
                target.replacesExisting) != ToolConfirmationReason::None) {
            return confirmationRequiredTool(call.name);
        }
    }
    if (entry->schema == ToolSchemaId::SftpWrite ||
        entry->schema == ToolSchemaId::SftpMove) {
        const SftpOverwriteInspection overwrite = inspectSftpOverwrite(
            entry->schema, call);
        if (!overwrite.success) {
            return invalidToolCall(overwrite.error);
        }
        if (toolConfirmationReason(
                ToolPermissionDecision::Allow, entry->schema,
                overwrite.requested) != ToolConfirmationReason::None) {
            return confirmationRequiredTool(call.name);
        }
    }
    return executeAuditedToolCall(
        call, isCancelled,
        [&settings, entry, &call](const CancelCallback& cancellation) {
            return dispatchToolCall(
                settings, entry->schema, call, cancellation);
        });
}

ToolExecutionResult routeProjectToolCall(const Settings& settings,
                                         const ToolRequestPlan& plan,
                                         const String& projectId,
                                         const ToolCall& call,
                                         const CancelCallback& isCancelled)
{
    const ToolCatalogEntry* entry = toolCatalogEntryForName(call.name);
    if (entry == nullptr || !isFilesSchema(entry->schema)) {
        return routeToolCall(settings, plan, call, isCancelled);
    }
    const ToolRouteAuthorization authorization = toolRouteAuthorization(
        plan, *entry);
    if (authorization != ToolRouteAuthorization::Allow) {
        return toolAuthorizationFailure(authorization, call.name);
    }
    if (entry->schema == ToolSchemaId::WriteFile) {
        const WorkspaceWriteTargetResult target =
            inspectProjectWorkspaceWriteTarget(projectId, call);
        if (!target.success) {
            return invalidToolCall(target.error);
        }
        if (toolConfirmationReason(
                ToolPermissionDecision::Allow, entry->schema,
                target.replacesExisting) != ToolConfirmationReason::None) {
            return confirmationRequiredTool(call.name);
        }
    }
    return executeAuditedToolCall(
        call, isCancelled,
        [&settings, entry, &projectId,
         &call](const CancelCallback& cancellation) {
            return dispatchProjectToolCall(
                settings, entry->schema, projectId, call, cancellation);
        });
}

static ToolExecutionResult executeConfirmedProjectToolCall(
    const Settings& settings,
    const String& projectId,
    const ToolCall& call,
    const CancelCallback& isCancelled)
{
    const ToolCatalogEntry* entry = toolCatalogEntryForName(call.name);
    if (entry == nullptr) {
        return unavailableTool(call.name);
    }
    return executeAuditedToolCall(
        call, isCancelled,
        [&settings, entry, &projectId,
         &call](const CancelCallback& cancellation) {
            return dispatchProjectToolCall(
                settings, entry->schema, projectId, call, cancellation);
        });
}

PendingToolDecisionResult approvePendingProjectToolCall(
    const Settings& settings,
    const ToolRequestPlan& currentPlan,
    const String& pendingId,
    const CancelCallback& isCancelled)
{
    PendingToolCallResult claimed = claimPendingToolCallApproval(
        pendingId, currentPlan);
    if (!claimed.success) {
        return {false, {}, {}, claimed.error};
    }
    ToolExecutionResult executed = executeConfirmedProjectToolCall(
        settings, claimed.pending.projectId,
        claimed.pending.continuation.call, isCancelled);
    return {
        true,
        std::move(claimed.pending),
        std::move(executed),
        "",
    };
}

PendingToolDecisionResult denyPendingProjectToolCall(
    const String& pendingId)
{
    PendingToolCallResult denied = denyPendingToolCall(pendingId);
    if (!denied.success) {
        return {false, {}, {}, denied.error};
    }
    const String error = "Tool '" +
        String(denied.pending.continuation.call.name.c_str()) +
        "' was denied by the user";
    return {
        true,
        std::move(denied.pending),
        {
            false,
            "{\"ok\":false,\"error\":\"permission denied by user\"}",
            error,
        },
        "",
    };
}

}  // namespace cardputer
