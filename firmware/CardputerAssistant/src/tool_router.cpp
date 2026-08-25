#include "tool_router.h"

#include "file_workspace.h"
#include "ssh_tool.h"
#include "storage.h"
#include "text_utils.h"
#include "web_search_client.h"

namespace cardputer {
namespace {

bool isWorkspaceToolName(const std::string& name)
{
    return name == "list_files" || name == "read_file" ||
           name == "write_file" || name == "append_file";
}

ToolExecutionResult unavailableTool(const std::string& name)
{
    return {
        false,
        "{\"ok\":false,\"error\":\"unsupported tool\"}",
        "API requested unsupported tool '" + String(name.c_str()) + "'",
    };
}

ToolExecutionResult deniedTool(const char* capability)
{
    return {
        false,
        "{\"ok\":false,\"error\":\"permission denied\"}",
        String(capability) + " tool access is disabled for this chat",
    };
}

}  // namespace

ChatToolPolicy resolveChatToolPolicy(const Settings& settings,
                                     const std::string& prompt,
                                     bool chatAllowsSsh,
                                     bool sshAvailable)
{
    return {
        requestsWorkspaceAccess(prompt),
        webSearchSettingsAreComplete(settings),
        chatAllowsSsh && sshAvailable,
    };
}

bool chatToolPolicyIsEnabled(const ChatToolPolicy& policy)
{
    return policy.workspaceEnabled || policy.webSearchEnabled || policy.sshEnabled;
}

ToolExecutionResult routeToolCall(const Settings& settings,
                                  const ChatToolPolicy& policy,
                                  const ToolCall& call,
                                  const CancelCallback& isCancelled)
{
    if (isWebSearchToolName(call.name)) {
        return policy.webSearchEnabled
            ? executeWebSearchTool(settings, call, isCancelled)
            : deniedTool("Web search");
    }
    if (isWebFetchToolName(call.name)) {
        return policy.webSearchEnabled
            ? executeWebFetchTool(settings, call, isCancelled)
            : deniedTool("Web fetch");
    }
    if (isWorkspaceToolName(call.name)) {
        return policy.workspaceEnabled
            ? executeWorkspaceTool(call)
            : deniedTool("Workspace");
    }
    if (isSshToolName(call.name)) {
        return policy.sshEnabled
            ? executeSshTool(call, isCancelled)
            : deniedTool("SSH");
    }
    return unavailableTool(call.name);
}

ToolExecutionResult routeProjectToolCall(const Settings& settings,
                                         const ChatToolPolicy& policy,
                                         const String& projectId,
                                         const ToolCall& call,
                                         const CancelCallback& isCancelled)
{
    if (isWorkspaceToolName(call.name)) {
        return policy.workspaceEnabled
            ? executeProjectWorkspaceTool(projectId, call)
            : deniedTool("Workspace");
    }
    return routeToolCall(settings, policy, call, isCancelled);
}

}  // namespace cardputer
