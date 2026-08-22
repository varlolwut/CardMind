#include "crash_journal.h"

#include <SD.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include <cstring>

namespace cardputer {
namespace {

constexpr const char* kJournalPath = "/assistant/diagnostics.log";
constexpr const char* kPreviousJournalPath = "/assistant/diagnostics.previous.log";
constexpr std::size_t kMaximumJournalBytes = 65536;
constexpr std::size_t kOperationCapacity = 32;
constexpr std::uint32_t kRtcMagic = 0x434D4A31;

RTC_DATA_ATTR std::uint32_t rtcMagic = 0;
RTC_DATA_ATTR char rtcOperation[kOperationCapacity] = {};
String bootPreviousOperation;

OperationResult rotateJournalIfNeeded()
{
    File journal = SD.open(kJournalPath, FILE_READ);
    if (!journal) {
        return {true, ""};
    }
    const std::size_t size = journal.size();
    journal.close();
    if (size < kMaximumJournalBytes) {
        return {true, ""};
    }
    if (SD.exists(kPreviousJournalPath) && !SD.remove(kPreviousJournalPath)) {
        return {false, "Failed to remove the previous crash journal during rotation"};
    }
    if (!SD.rename(kJournalPath, kPreviousJournalPath)) {
        return {false, "Failed to rotate the crash journal on microSD"};
    }
    return {true, ""};
}

}  // namespace

void markOperation(const char* operation)
{
    rtcMagic = kRtcMagic;
    std::strncpy(rtcOperation, operation, kOperationCapacity - 1);
    rtcOperation[kOperationCapacity - 1] = '\0';
}

String previousOperation()
{
    if (!bootPreviousOperation.isEmpty()) {
        return bootPreviousOperation;
    }
    if (rtcMagic != kRtcMagic || rtcOperation[0] == '\0') {
        return "power_on";
    }
    return String(rtcOperation);
}

OperationResult appendBootJournal(const char* firmwareVersion)
{
    const String operation = previousOperation();
    bootPreviousOperation = operation;
    const OperationResult rotation = rotateJournalIfNeeded();
    if (!rotation.success) {
        return rotation;
    }
    File journal = SD.open(kJournalPath, FILE_APPEND);
    if (!journal) {
        return {false, "Failed to open the crash journal on microSD"};
    }
    const String line =
        "boot firmware=" + String(firmwareVersion) +
        " reset_reason=" + String(static_cast<int>(esp_reset_reason())) +
        " previous_operation=" + operation +
        " heap=" + String(ESP.getFreeHeap()) +
        " largest_heap=" + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) +
        " min_heap=" + String(ESP.getMinFreeHeap()) +
        " stack_free=" + String(uxTaskGetStackHighWaterMark(nullptr)) + "\n";
    const std::size_t written = journal.print(line);
    journal.close();
    if (written != line.length()) {
        return {false, "Crash journal write was incomplete"};
    }
    markOperation("idle");
    return {true, ""};
}

}  // namespace cardputer
