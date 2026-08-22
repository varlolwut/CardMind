#pragma once

#include "app_types.h"

namespace cardputer {

ToolExecutionResult executeWebSearchTool(const Settings& settings, const ToolCall& call);
ToolExecutionResult executeWebFetchTool(const Settings& settings, const ToolCall& call);

}  // namespace cardputer
