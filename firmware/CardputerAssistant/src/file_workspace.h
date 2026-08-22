#pragma once

#include "app_types.h"

namespace cardputer {

constexpr std::size_t kMaximumWorkspaceFileBytes = 491520;
constexpr std::size_t kMaximumWorkspaceToolChunkBytes = 12288;
constexpr std::size_t kMaximumWorkspaceFiles = 40;

OperationResult initializeFileWorkspace();
WorkspaceFilesResult listWorkspaceFiles();
ToolExecutionResult executeWorkspaceTool(const ToolCall& call);
String workspaceFilePath(const String& name);

}  // namespace cardputer
