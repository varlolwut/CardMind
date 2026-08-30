#pragma once

#include "app_types.h"

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

PendingToolPreviewBodyResult buildPendingFileReplacementPreview(
    const std::string& currentPrefix,
    std::uint32_t currentBytes,
    bool currentComplete,
    const std::string& proposed);
PendingToolPreviewBodyResult buildPendingSshCommandPreview(
    std::string command);

}  // namespace cardputer
