#pragma once

#include "app_types.h"

#include <cstdint>
#include <functional>

namespace cardputer {

using VoiceProgressCallback = std::function<void(std::uint32_t elapsedMs, std::uint16_t level)>;

OperationResult initializeVoiceStorage();
VoiceRecordingResult recordVoiceWhileButtonHeld(const VoiceProgressCallback& onProgress);
VoiceRecordingResult probeMicrophone(std::uint32_t durationMs);
OperationResult removeVoiceRecording();
const char* voiceRecordingPath();
std::uint32_t maximumVoiceRecordingMs();

}  // namespace cardputer
