#pragma once

#include "app_types.h"

#include <functional>

namespace cardputer {

enum class SpeechPlaybackCommand {
    Continue,
    Pause,
    Stop,
};

using SpeechPlaybackControl = std::function<SpeechPlaybackCommand()>;

OperationResult synthesizeAndPlaySpeech(const Settings& settings, const std::string& text);
OperationResult synthesizeAndPlaySpeechControlled(const Settings& settings,
                                                  const std::string& text,
                                                  const SpeechPlaybackControl& control);
OperationResult validateTtsCredentials(const Settings& settings);
OperationResult probeDefaultTtsTls();
OperationResult playTtsHardwareTest(std::uint8_t volume);
OperationResult playTtsHardwareTestControlled(std::uint8_t volume,
                                              const SpeechPlaybackControl& control);

}  // namespace cardputer
