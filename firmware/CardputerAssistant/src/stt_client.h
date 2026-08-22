#pragma once

#include "app_types.h"

namespace cardputer {

TranscriptionResult transcribeVoiceRecording(const Settings& settings);
OperationResult probeDefaultSttTls();
OperationResult validateSttCredentials(const Settings& settings);

}  // namespace cardputer
