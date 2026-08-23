#pragma once

#include "api_client.h"
#include "app_types.h"

namespace cardputer {

TranscriptionResult transcribeVoiceRecording(const Settings& settings,
                                              const CancelCallback& isCancelled);
OperationResult probeDefaultSttTls();
OperationResult validateSttCredentials(const Settings& settings);

}  // namespace cardputer
