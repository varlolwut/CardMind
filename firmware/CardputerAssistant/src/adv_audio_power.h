#pragma once

#include "app_types.h"

namespace cardputer {

OperationResult initializeCardputerAdvAudioPowerControl();
OperationResult powerDownCardputerAdvAudio();
OperationResult verifyCardputerAdvAudioPoweredDown();

}  // namespace cardputer
