#pragma once

#include "api_client.h"
#include "tool_catalog.h"

#include <cstddef>
#include <cstdint>

namespace cardputer {

constexpr std::uint32_t kModelSftpTimeoutMs = 60000;
constexpr std::size_t kMaximumModelSftpPageEntries = 16;
constexpr std::size_t kMinimumModelSftpReadBytes = 4;
constexpr std::size_t kMaximumModelSftpChunkBytes = 12288;
constexpr std::size_t kMaximumModelSftpPathBytes = 511;

struct SftpOverwriteInspection {
    bool success;
    bool requested;
    String error;
};

bool isModelSftpSchema(ToolSchemaId schema) noexcept;
bool isValidModelSftpText(const char* value, std::size_t bytes) noexcept;
bool isValidModelSftpPath(const char* value, std::size_t bytes) noexcept;
SftpOverwriteInspection inspectSftpOverwrite(
    ToolSchemaId schema,
    const ToolCall& call);
ToolExecutionResult executeSftpListTool(
    const ToolCall& call,
    const CancelCallback& isCancelled);
ToolExecutionResult executeSftpReadTool(
    const ToolCall& call,
    const CancelCallback& isCancelled);
ToolExecutionResult executeSftpWriteTool(
    const ToolCall& call,
    const CancelCallback& isCancelled);
ToolExecutionResult executeSftpMoveTool(
    const ToolCall& call,
    const CancelCallback& isCancelled);

}  // namespace cardputer
