#pragma once

#include "app_types.h"

namespace cardputer {

constexpr std::size_t kMaximumWorkspaceFileBytes = 491520;
constexpr std::size_t kMaximumWorkspaceToolChunkBytes = 12288;
constexpr std::size_t kMaximumWorkspaceFiles = 40;

OperationResult initializeFileWorkspace();
WorkspaceFilesResult listWorkspaceFiles();
WorkspaceChunkResult readWorkspaceFileChunk(const String& name,
                                            std::uint32_t offset,
                                            std::size_t maximumBytes);
OperationResult createWorkspaceFile(const String& name);
OperationResult replaceWorkspaceFileRange(const String& name,
                                          std::uint32_t offset,
                                          std::uint32_t originalBytes,
                                          const std::string& replacement);
OperationResult copyWorkspaceFile(const String& sourceName, const String& destinationName);
OperationResult renameWorkspaceFile(const String& sourceName, const String& destinationName);
OperationResult deleteWorkspaceFile(const String& name);
ToolExecutionResult executeWorkspaceTool(const ToolCall& call);
String workspaceFilePath(const String& name);

}  // namespace cardputer
