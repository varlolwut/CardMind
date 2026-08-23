#pragma once

#include "app_types.h"

#include <functional>

namespace cardputer {

struct FirmwareUpdateInfo {
    bool success;
    bool newerAvailable;
    String version;
    String assetUrl;
    String sha256;
    std::uint32_t assetBytes;
    bool pythonRecoveryReady;
    String error;
};

using FirmwareProgressCallback = std::function<void(std::uint32_t, std::uint32_t)>;
using FirmwareCancelCallback = std::function<bool()>;

bool isNewerFirmwareVersion(const String& candidate, const String& current);
FirmwareUpdateInfo checkLatestFirmwareUpdate(const String& currentVersion);
OperationResult downloadFirmwareUpdate(const FirmwareUpdateInfo& info,
                                       FirmwareProgressCallback onProgress,
                                       FirmwareCancelCallback isCancelled);
OperationResult installDownloadedFirmware(const FirmwareUpdateInfo& info,
                                          FirmwareProgressCallback onProgress,
                                          FirmwareCancelCallback isCancelled);
OperationResult removeDownloadedFirmware();

}  // namespace cardputer
