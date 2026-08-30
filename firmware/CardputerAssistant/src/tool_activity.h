#pragma once

#include "app_types.h"

#include <cstdint>
#include <vector>

namespace cardputer {

constexpr const char* kToolActivityPath = "/assistant/v2/tool_activity.jsonl";
constexpr const char* kPreviousToolActivityPath =
    "/assistant/v2/tool_activity.previous.jsonl";

enum class ToolActivityTarget : std::uint8_t {
    Web,
    ProjectFiles,
    SelectedSsh,
};

enum class ToolActivityStatus : std::uint8_t {
    Running,
    Succeeded,
    Failed,
    Canceled,
    Interrupted,
};

struct ToolActivityExitStatus {
    bool present;
    std::int32_t value;
};

struct ToolActivityRecord {
    std::uint64_t sequence;
    String tool;
    ToolActivityTarget target;
    ToolActivityStatus status;
    std::uint32_t durationMs;
    std::uint32_t outputBytes;
    ToolActivityExitStatus exitStatus;
};

struct ToolActivityResult {
    bool success;
    ToolActivityRecord activity;
    String error;
};

struct ToolActivitiesResult {
    bool success;
    std::vector<ToolActivityRecord> activities;
    String error;
};

const char* toolActivityTargetName(ToolActivityTarget target);
const char* toolActivityStatusName(ToolActivityStatus status);
OperationResult initializeToolActivityJournal();
ToolActivityResult startToolActivity(const std::string& tool);
OperationResult finishToolActivity(
    const ToolActivityRecord& running,
    ToolActivityStatus status,
    std::uint32_t durationMs,
    std::uint32_t outputBytes,
    ToolActivityExitStatus exitStatus);
ToolActivitiesResult loadRecentToolActivities();

}  // namespace cardputer
