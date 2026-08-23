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
WorkspaceFindResult findWorkspaceText(const String& name,
                                      const std::string& query,
                                      std::uint32_t startOffset);
WorkspaceBookmarkResult loadWorkspaceBookmark(const String& name);
OperationResult saveWorkspaceBookmark(const String& name, std::uint32_t offset);
OperationResult clearWorkspaceBookmark(const String& name);
OperationResult createWorkspaceFile(const String& name);
OperationResult validateWorkspaceFileUtf8(const String& name);
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
