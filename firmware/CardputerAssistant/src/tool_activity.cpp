#include "tool_activity.h"

#include "sd_storage.h"
#include "tool_catalog.h"

#include <ArduinoJson.h>
#include <SD.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace cardputer {
namespace {

constexpr std::uint32_t kToolActivityVersion = 1;
constexpr std::uint64_t kMaximumJournalBytes = 16384;
constexpr std::size_t kMaximumActivityLineBytes = 512;
constexpr std::size_t kMaximumRecentActivities = 16;

bool journalInitialized = false;
bool activeActivityPresent = false;
std::uint64_t nextSequence = 1;
ToolActivityRecord activeActivity = {};

bool hasExactFields(const JsonObjectConst& object,
                    const char* const* fields,
                    std::size_t fieldCount)
{
    if (object.size() != fieldCount) {
        return false;
    }
    for (std::size_t index = 0; index < fieldCount; ++index) {
        if (!object.containsKey(fields[index])) {
            return false;
        }
    }
    return true;
}

bool targetForTool(const std::string& tool, ToolActivityTarget& target)
{
    const ToolCatalogEntry* entry = toolCatalogEntryForName(tool);
    if (entry == nullptr) {
        return false;
    }
    switch (entry->group) {
        case ToolCapabilityGroup::Web:
            target = ToolActivityTarget::Web;
            return true;
        case ToolCapabilityGroup::Files:
            target = ToolActivityTarget::ProjectFiles;
            return true;
        case ToolCapabilityGroup::Ssh:
            target = ToolActivityTarget::SelectedSsh;
            return true;
        case ToolCapabilityGroup::Python:
        case ToolCapabilityGroup::Count:
            return false;
    }
    return false;
}

bool parseTarget(const char* value, ToolActivityTarget& target)
{
    if (value == nullptr) {
        return false;
    }
    if (std::strcmp(value, "web") == 0) {
        target = ToolActivityTarget::Web;
        return true;
    }
    if (std::strcmp(value, "project_files") == 0) {
        target = ToolActivityTarget::ProjectFiles;
        return true;
    }
    if (std::strcmp(value, "selected_ssh") == 0) {
        target = ToolActivityTarget::SelectedSsh;
        return true;
    }
    return false;
}

bool parseStoredStatus(const char* value, ToolActivityStatus& status)
{
    if (value == nullptr) {
        return false;
    }
    if (std::strcmp(value, "running") == 0) {
        status = ToolActivityStatus::Running;
        return true;
    }
    if (std::strcmp(value, "succeeded") == 0) {
        status = ToolActivityStatus::Succeeded;
        return true;
    }
    if (std::strcmp(value, "failed") == 0) {
        status = ToolActivityStatus::Failed;
        return true;
    }
    if (std::strcmp(value, "canceled") == 0) {
        status = ToolActivityStatus::Canceled;
        return true;
    }
    return false;
}

OperationResult serializeActivity(const ToolActivityRecord& activity,
                                  String& line)
{
    if (activity.sequence == 0 ||
        toolCatalogEntryForName(activity.tool.c_str()) == nullptr ||
        activity.status == ToolActivityStatus::Interrupted) {
        return {false, "Tool activity record is invalid"};
    }
    ToolActivityTarget expectedTarget;
    if (!targetForTool(activity.tool.c_str(), expectedTarget) ||
        expectedTarget != activity.target ||
        (activity.status == ToolActivityStatus::Running &&
         (activity.durationMs != 0 || activity.outputBytes != 0 ||
          activity.exitStatus.present)) ||
        (activity.exitStatus.present &&
         activity.target != ToolActivityTarget::SelectedSsh)) {
        return {false, "Tool activity fields are inconsistent"};
    }
    JsonDocument document;
    document["version"] = kToolActivityVersion;
    document["sequence"] = activity.sequence;
    document["tool"] = activity.tool;
    document["target"] = toolActivityTargetName(activity.target);
    document["status"] = toolActivityStatusName(activity.status);
    document["duration_ms"] = activity.durationMs;
    document["output_bytes"] = activity.outputBytes;
    if (activity.exitStatus.present) {
        document["exit_status"] = activity.exitStatus.value;
    } else {
        document["exit_status"] = nullptr;
    }
    if (measureJson(document) > kMaximumActivityLineBytes) {
        return {false, "Tool activity record exceeds 512 bytes"};
    }
    line = "";
    serializeJson(document, line);
    return line.isEmpty()
        ? OperationResult{false, "Tool activity serialization failed"}
        : OperationResult{true, ""};
}

ToolActivityResult parseActivityLine(const String& line)
{
    if (line.isEmpty() || line.length() > kMaximumActivityLineBytes) {
        return {false, {}, "Tool activity line size is invalid"};
    }
    JsonDocument document;
    const DeserializationError parsed = deserializeJson(document, line);
    if (parsed || !document.is<JsonObject>()) {
        return {false, {}, "Tool activity line is not a JSON object"};
    }
    const JsonObjectConst root = document.as<JsonObjectConst>();
    static constexpr const char* kFields[] = {
        "version", "sequence", "tool", "target", "status",
        "duration_ms", "output_bytes", "exit_status",
    };
    if (!hasExactFields(root, kFields, 8) ||
        !root["version"].is<std::uint32_t>() ||
        root["version"].as<std::uint32_t>() != kToolActivityVersion ||
        !root["sequence"].is<std::uint64_t>() ||
        root["sequence"].as<std::uint64_t>() == 0 ||
        !root["tool"].is<const char*>() ||
        !root["target"].is<const char*>() ||
        !root["status"].is<const char*>() ||
        !root["duration_ms"].is<std::uint32_t>() ||
        !root["output_bytes"].is<std::uint32_t>() ||
        (!root["exit_status"].isNull() &&
         !root["exit_status"].is<std::int32_t>())) {
        return {false, {}, "Tool activity fields or types are invalid"};
    }
    ToolActivityRecord activity = {};
    activity.sequence = root["sequence"].as<std::uint64_t>();
    activity.tool = root["tool"].as<const char*>();
    if (activity.tool.isEmpty() ||
        toolCatalogEntryForName(activity.tool.c_str()) == nullptr ||
        !parseTarget(root["target"].as<const char*>(), activity.target) ||
        !parseStoredStatus(root["status"].as<const char*>(), activity.status)) {
        return {false, {}, "Tool activity catalog fields are invalid"};
    }
    ToolActivityTarget expectedTarget;
    if (!targetForTool(activity.tool.c_str(), expectedTarget) ||
        expectedTarget != activity.target) {
        return {false, {}, "Tool activity target does not match its tool"};
    }
    activity.durationMs = root["duration_ms"].as<std::uint32_t>();
    activity.outputBytes = root["output_bytes"].as<std::uint32_t>();
    activity.exitStatus.present = !root["exit_status"].isNull();
    activity.exitStatus.value = activity.exitStatus.present
        ? root["exit_status"].as<std::int32_t>() : 0;
    if ((activity.status == ToolActivityStatus::Running &&
         (activity.durationMs != 0 || activity.outputBytes != 0 ||
          activity.exitStatus.present)) ||
        (activity.exitStatus.present &&
         activity.target != ToolActivityTarget::SelectedSsh)) {
        return {false, {}, "Tool activity state fields are inconsistent"};
    }
    return {true, std::move(activity), ""};
}

OperationResult retainJournalEvent(
    std::vector<ToolActivityRecord>& events,
    ToolActivityRecord event)
{
    if (!events.empty() && event.sequence < events.back().sequence) {
        return {false, "Tool activity sequence is not monotonic"};
    }
    const bool newSequence = events.empty() ||
        events.back().sequence != event.sequence;
    if (newSequence) {
        std::size_t sequenceCount = 0;
        std::uint64_t lastSequence = 0;
        for (const ToolActivityRecord& retained : events) {
            if (sequenceCount == 0 || retained.sequence != lastSequence) {
                ++sequenceCount;
                lastSequence = retained.sequence;
            }
        }
        if (sequenceCount == kMaximumRecentActivities) {
            const std::uint64_t oldestSequence = events.front().sequence;
            events.erase(
                events.begin(),
                std::find_if(
                    events.begin(), events.end(),
                    [oldestSequence](const ToolActivityRecord& retained) {
                        return retained.sequence != oldestSequence;
                    }));
        }
    }
    events.push_back(std::move(event));
    return {true, ""};
}

OperationResult readJournal(
    const char* path,
    std::vector<ToolActivityRecord>& events,
    std::uint64_t& maximumSequence)
{
    const OperationResult access = requireSdReadAccess();
    if (!access.success) {
        return access;
    }
    if (!SD.exists(path)) {
        return {true, ""};
    }
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return {false, "Failed to open tool activity journal"};
    }
    if (file.size() > kMaximumJournalBytes) {
        file.close();
        return {false, "Tool activity journal exceeds 16384 bytes"};
    }
    String line;
    while (file.available()) {
        const int value = file.read();
        if (value < 0) {
            file.close();
            return {false, "Tool activity journal read stopped before EOF"};
        }
        if (value != '\n') {
            if (line.length() >= kMaximumActivityLineBytes ||
                !line.concat(static_cast<char>(value))) {
                file.close();
                return {false, "Tool activity line exceeds 512 bytes"};
            }
            continue;
        }
        ToolActivityResult parsed = parseActivityLine(line);
        line = "";
        if (!parsed.success) {
            file.close();
            return {false, parsed.error};
        }
        maximumSequence = std::max(maximumSequence, parsed.activity.sequence);
        const OperationResult retained = retainJournalEvent(
            events, std::move(parsed.activity));
        if (!retained.success) {
            file.close();
            return retained;
        }
    }
    file.close();
    return {true, ""};
}

OperationResult rotateBeforeAppend(std::size_t lineBytes)
{
    if (lineBytes + 1U > kMaximumJournalBytes) {
        return {false, "Tool activity line exceeds journal capacity"};
    }
    File current = SD.open(kToolActivityPath, FILE_READ);
    std::uint64_t currentBytes = 0;
    bool trailingPartial = false;
    if (current) {
        currentBytes = current.size();
        if (currentBytes != 0) {
            if (!current.seek(currentBytes - 1U)) {
                current.close();
                return {false, "Failed to inspect tool activity journal tail"};
            }
            trailingPartial = current.read() != '\n';
        }
        current.close();
    }
    if (!trailingPartial &&
        currentBytes + lineBytes + 1U <= kMaximumJournalBytes) {
        return {true, ""};
    }
    const OperationResult access = requireSdCleanupAccess();
    if (!access.success) {
        return access;
    }
    if (SD.exists(kPreviousToolActivityPath) &&
        !SD.remove(kPreviousToolActivityPath)) {
        return {false, "Failed to remove previous tool activity journal"};
    }
    if (SD.exists(kToolActivityPath) &&
        !SD.rename(kToolActivityPath, kPreviousToolActivityPath)) {
        return {false, "Failed to rotate tool activity journal"};
    }
    File replacement = SD.open(kToolActivityPath, FILE_WRITE);
    if (!replacement) {
        return {false, "Failed to create tool activity journal after rotation"};
    }
    replacement.flush();
    replacement.close();
    return {true, ""};
}

OperationResult appendActivity(const ToolActivityRecord& activity)
{
    String line;
    OperationResult result = serializeActivity(activity, line);
    if (!result.success) {
        return result;
    }
    result = requireSdWriteAccess(line.length() + 1U, kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    result = rotateBeforeAppend(line.length());
    if (!result.success) {
        return result;
    }
    File file = SD.open(kToolActivityPath, FILE_APPEND);
    if (!file) {
        return {false, "Failed to open tool activity journal for append"};
    }
    const std::size_t written = file.print(line);
    const std::size_t newline = file.write('\n');
    file.flush();
    file.close();
    if (written != line.length() || newline != 1U) {
        return {false, "Tool activity journal append was incomplete"};
    }
    return {true, ""};
}

ToolActivitiesResult foldRecentActivities(
    std::vector<ToolActivityRecord> events)
{
    std::vector<ToolActivityRecord> activities;
    for (ToolActivityRecord& event : events) {
        auto existing = std::find_if(
            activities.begin(), activities.end(),
            [&event](const ToolActivityRecord& item) {
                return item.sequence == event.sequence;
            });
        if (existing == activities.end()) {
            activities.push_back(std::move(event));
            continue;
        }
        if (existing->tool != event.tool || existing->target != event.target ||
            existing->status != ToolActivityStatus::Running ||
            event.status == ToolActivityStatus::Running) {
            return {false, {}, "Tool activity sequence contains conflicting events"};
        }
        *existing = std::move(event);
    }
    for (ToolActivityRecord& activity : activities) {
        if (activity.status == ToolActivityStatus::Running &&
            (!activeActivityPresent ||
             activeActivity.sequence != activity.sequence)) {
            activity.status = ToolActivityStatus::Interrupted;
        }
    }
    std::sort(
        activities.begin(), activities.end(),
        [](const ToolActivityRecord& left, const ToolActivityRecord& right) {
            return left.sequence > right.sequence;
        });
    if (activities.size() > kMaximumRecentActivities) {
        activities.resize(kMaximumRecentActivities);
    }
    return {true, std::move(activities), ""};
}

}  // namespace

const char* toolActivityTargetName(ToolActivityTarget target)
{
    switch (target) {
        case ToolActivityTarget::Web: return "web";
        case ToolActivityTarget::ProjectFiles: return "project_files";
        case ToolActivityTarget::SelectedSsh: return "selected_ssh";
    }
    return "invalid";
}

const char* toolActivityStatusName(ToolActivityStatus status)
{
    switch (status) {
        case ToolActivityStatus::Running: return "running";
        case ToolActivityStatus::Succeeded: return "succeeded";
        case ToolActivityStatus::Failed: return "failed";
        case ToolActivityStatus::Canceled: return "canceled";
        case ToolActivityStatus::Interrupted: return "interrupted";
    }
    return "invalid";
}

OperationResult initializeToolActivityJournal()
{
    OperationResult result = requireSdWriteAccess(0, kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    result = ensureSdDirectory("/assistant/v2");
    if (!result.success) {
        return result;
    }
    std::vector<ToolActivityRecord> events;
    std::uint64_t maximumSequence = 0;
    result = readJournal(
        kPreviousToolActivityPath, events, maximumSequence);
    if (!result.success) {
        return result;
    }
    result = readJournal(kToolActivityPath, events, maximumSequence);
    if (!result.success) {
        return result;
    }
    if (maximumSequence == std::numeric_limits<std::uint64_t>::max()) {
        return {false, "Tool activity sequence is exhausted"};
    }
    if (!SD.exists(kToolActivityPath)) {
        File journal = SD.open(kToolActivityPath, FILE_WRITE);
        if (!journal) {
            return {false, "Failed to create tool activity journal"};
        }
        journal.flush();
        journal.close();
    }
    nextSequence = maximumSequence + 1U;
    journalInitialized = true;
    return {true, ""};
}

ToolActivityResult startToolActivity(const std::string& tool)
{
    if (!journalInitialized) {
        const OperationResult initialized = initializeToolActivityJournal();
        if (!initialized.success) {
            return {false, {}, initialized.error};
        }
    }
    if (activeActivityPresent) {
        return {false, {}, "Another foreground tool activity is already running"};
    }
    ToolActivityTarget target;
    if (!targetForTool(tool, target)) {
        return {false, {}, "Tool activity requires a canonical current catalog tool"};
    }
    if (nextSequence == 0) {
        return {false, {}, "Tool activity sequence is exhausted"};
    }
    ToolActivityRecord running = {
        nextSequence,
        tool.c_str(),
        target,
        ToolActivityStatus::Running,
        0,
        0,
        {false, 0},
    };
    const OperationResult appended = appendActivity(running);
    if (!appended.success) {
        return {false, {}, appended.error};
    }
    activeActivity = running;
    activeActivityPresent = true;
    ++nextSequence;
    return {true, std::move(running), ""};
}

OperationResult finishToolActivity(
    const ToolActivityRecord& running,
    ToolActivityStatus status,
    std::uint32_t durationMs,
    std::uint32_t outputBytes,
    ToolActivityExitStatus exitStatus)
{
    if (!activeActivityPresent ||
        running.status != ToolActivityStatus::Running ||
        running.sequence != activeActivity.sequence ||
        running.tool != activeActivity.tool ||
        running.target != activeActivity.target) {
        return {false, "Tool activity finish does not match the active record"};
    }
    if (status == ToolActivityStatus::Running ||
        status == ToolActivityStatus::Interrupted) {
        return {false, "Tool activity finish requires a terminal status"};
    }
    ToolActivityRecord terminal = {
        running.sequence,
        running.tool,
        running.target,
        status,
        durationMs,
        outputBytes,
        exitStatus,
    };
    const OperationResult appended = appendActivity(terminal);
    activeActivityPresent = false;
    activeActivity = {};
    if (!appended.success) {
        return appended;
    }
    return {true, ""};
}

ToolActivitiesResult loadRecentToolActivities()
{
    std::vector<ToolActivityRecord> events;
    std::uint64_t maximumSequence = 0;
    OperationResult result = readJournal(
        kPreviousToolActivityPath, events, maximumSequence);
    if (!result.success) {
        return {false, {}, result.error};
    }
    result = readJournal(kToolActivityPath, events, maximumSequence);
    if (!result.success) {
        return {false, {}, result.error};
    }
    return foldRecentActivities(std::move(events));
}

}  // namespace cardputer
