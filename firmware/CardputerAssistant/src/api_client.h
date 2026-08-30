#pragma once

#include "app_types.h"
#include "pending_tool_call.h"
#include "text_utils.h"
#include "tool_catalog.h"

#include <functional>

namespace cardputer {

using ChatTextCallback = std::function<void(const std::string&)>;
using CancelCallback = std::function<bool()>;
using ToolExecutor = std::function<ToolExecutionResult(const ToolCall&)>;
using PendingToolSaver = std::function<OperationResult(
    const PendingToolContinuation&)>;

struct ChatRequestSerializationValidation {
    bool success;
    bool precedence;
    bool inheritance;
    bool model;
    bool outputTokens;
    bool noTools;
    bool tools;
    String error;
};

OperationResult connectToWifi(const Settings& settings);
OperationResult synchronizeTlsClock();
ModelsResult fetchModels(const Settings& settings);
ChatRequestSerializationValidation validateChatRequestSerialization(
    const Settings& settings,
    const ResolvedProjectRequestPolicy& policy,
    const std::string& globalInstructions,
    const std::string& projectInstructions,
    const std::string& chatInstructions,
    const std::string& requestInstructions,
    const std::string& contextSummary);
ChatResult streamChatCompletion(const Settings& settings,
                                const std::vector<Message>& history,
                                const std::string& instructions,
                                const ChatTextCallback& onText,
                                const CancelCallback& isCancelled);
ChatResult streamChatCompletionWithBudget(const Settings& settings,
                                          const std::vector<Message>& history,
                                          const std::string& instructions,
                                          std::uint32_t maximumOutputTokens,
                                          const ChatTextCallback& onText,
                                          const CancelCallback& isCancelled);
ChatResult streamChatCompletionWithTools(const Settings& settings,
                                         const std::vector<Message>& history,
                                         const std::string& instructions,
                                         const ToolRequestPlan& toolPlan,
                                         const ChatTextCallback& onText,
                                         const ToolExecutor& executeTool,
                                         const PendingToolSaver& savePendingTool,
                                         const CancelCallback& isCancelled);
ChatResult streamChatCompletionWithToolsAndBudget(
    const Settings& settings,
    const std::vector<Message>& history,
    const std::string& instructions,
    const ToolRequestPlan& toolPlan,
    std::uint32_t maximumOutputTokens,
    const ChatTextCallback& onText,
    const ToolExecutor& executeTool,
    const PendingToolSaver& savePendingTool,
    const CancelCallback& isCancelled);
ChatResult continueChatCompletionAfterPendingToolResult(
    const Settings& settings,
    const std::vector<Message>& history,
    const std::string& instructions,
    const ToolRequestPlan& toolPlan,
    std::uint32_t maximumOutputTokens,
    PendingToolContinuation continuation,
    ToolExecutionResult toolResult,
    const ChatTextCallback& onText,
    const ToolExecutor& executeTool,
    const PendingToolSaver& savePendingTool,
    const CancelCallback& isCancelled);

}  // namespace cardputer
