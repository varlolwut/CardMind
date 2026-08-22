#pragma once

#include "app_types.h"

#include <functional>

namespace cardputer {

using ChatTextCallback = std::function<void(const std::string&)>;
using ToolExecutor = std::function<ToolExecutionResult(const ToolCall&)>;

OperationResult connectToWifi(const Settings& settings);
OperationResult synchronizeTlsClock();
ModelsResult fetchModels(const Settings& settings);
ChatResult streamChatCompletion(const Settings& settings,
                                const std::vector<Message>& history,
                                const std::string& instructions,
                                const ChatTextCallback& onText);
ChatResult streamChatCompletionWithTools(const Settings& settings,
                                         const std::vector<Message>& history,
                                         const std::string& instructions,
                                         const ChatTextCallback& onText,
                                         const ToolExecutor& executeTool);

}  // namespace cardputer
