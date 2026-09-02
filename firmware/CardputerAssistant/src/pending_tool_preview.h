#pragma once

#include "app_types.h"
#include "json_string_reader.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace cardputer {

constexpr std::size_t kMaximumPendingToolPreviewBodyBytes = 2048;
constexpr std::size_t kMaximumPendingToolPreviewLinesPerSide = 4;
constexpr std::size_t kMaximumPendingToolPreviewLineBytes = 192;
constexpr std::size_t kMaximumPendingFilePreviewSourceBytes = 12288;

struct PendingToolPreviewBodyResult {
    bool success;
    std::string body;
    bool truncated;
    String error;
};

json_reader::JsonStringValueResult readCanonicalStringArgument(
    const std::string& arguments,
    const char* field,
    std::size_t maximumBytes);
PendingToolPreviewBodyResult buildPendingFileReplacementPreview(
    const std::string& currentPrefix,
    std::uint32_t currentBytes,
    bool currentComplete,
    const std::string& proposed);
PendingToolPreviewBodyResult buildPendingSshCommandPreview(
    std::string command);
PendingToolPreviewBodyResult buildPendingPythonSourcePreview(
    const String& name,
    std::uint32_t sourceBytes,
    const std::string& sha256,
    std::string sourcePrefix,
    bool sourceComplete);

}  // namespace cardputer
