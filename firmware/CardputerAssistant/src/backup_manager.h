#pragma once

#include "app_types.h"

namespace cardputer {

OperationResult createLocalBackup(const Settings& settings, const String& activeChatId);
OperationResult restoreLocalBackup(Settings& settings, String& activeChatId);
OperationResult localBackupSummary(String& summary);

}  // namespace cardputer
