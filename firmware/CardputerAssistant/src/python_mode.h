#pragma once

#include "app_types.h"

namespace cardputer {

struct PythonModeStatus {
    bool partitionLayoutReady;
    bool pythonImageReady;
    std::uint32_t cardMindPartitionBytes;
    std::uint32_t pythonPartitionBytes;
    String lastRuntimeError;
    String error;
};

struct PythonHandoffRequest {
    bool success;
    bool openWebConsole;
    String error;
};

PythonModeStatus inspectPythonMode();
OperationResult synchronizePythonModeSettings(const Settings& settings,
                                              const String& consolePassword,
                                              const String& handoffToken);
OperationResult activatePythonMode();
OperationResult stageCardMindUpdateForPython(std::uint32_t firmwareBytes,
                                             const String& sha256);
OperationResult clearCardMindUpdateRequest();
PythonHandoffRequest consumePythonHandoffRequest();
PythonHandoffRequest consumePythonSdHandoffRequest();

}  // namespace cardputer
