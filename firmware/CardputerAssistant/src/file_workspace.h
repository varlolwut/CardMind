#pragma once

#include "app_types.h"

#include <limits>

namespace cardputer {

constexpr std::uint32_t kMaximumWorkspaceFileBytes =
    std::numeric_limits<std::uint32_t>::max();
constexpr std::size_t kMaximumWorkspaceToolChunkBytes = 12288;
constexpr std::size_t kMaximumWorkspaceFiles = 4096;

OperationResult initializeFileWorkspace();
OperationResult ensureWorkspaceFileParent(const String& name);
WorkspaceFilesPageResult listWorkspaceFilesPage(std::uint32_t offset,
                                                std::size_t maximumEntries);
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
OperationResult replaceWorkspaceFileWithTemporary(const String& name,
                                                   const String& temporaryName);
OperationResult commitWorkspaceBinaryTemporary(const String& name,
                                               const String& temporaryName);
OperationResult copyWorkspaceFile(const String& sourceName, const String& destinationName);
OperationResult renameWorkspaceFile(const String& sourceName, const String& destinationName);
OperationResult deleteWorkspaceFile(const String& name);
ToolExecutionResult executeWorkspaceTool(const ToolCall& call);
ToolExecutionResult executeProjectWorkspaceTool(const String& projectId,
                                                const ToolCall& call);
String workspaceFilePath(const String& name);

}  // namespace cardputer
