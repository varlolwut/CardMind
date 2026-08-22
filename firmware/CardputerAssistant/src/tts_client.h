#pragma once

#include "app_types.h"

namespace cardputer {

OperationResult synthesizeAndPlaySpeech(const Settings& settings, const std::string& text);
OperationResult validateTtsCredentials(const Settings& settings);
OperationResult probeDefaultTtsTls();
OperationResult playTtsHardwareTest(std::uint8_t volume);

}  // namespace cardputer
