#pragma once

#include "app_types.h"

namespace cardputer {

OperationResult loadSettings(Settings& settings);
OperationResult saveSettings(const Settings& settings);
OperationResult saveModel(const String& model);
OperationResult loadSetupAccessPointPassword(String& password);
OperationResult saveSetupAccessPointPassword(const String& password);
OperationResult loadActiveChatId(String& id);
OperationResult saveActiveChatId(const String& id);
bool settingsAreComplete(const Settings& settings);
bool voiceSettingsAreComplete(const Settings& settings);
bool webSearchSettingsAreComplete(const Settings& settings);
bool ttsSettingsAreComplete(const Settings& settings);

}  // namespace cardputer
