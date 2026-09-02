#pragma once

#include "app_types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

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

enum class PythonRunReturnSurface : std::uint8_t {
    Device = 1,
    Web = 2,
};

enum class PythonRunArtifactState : std::uint8_t {
    Present,
    ProvenAbsent,
    AccessError,
};

struct PythonRunArtifactsInspection {
    bool success;
    PythonRunArtifactState request;
    PythonRunArtifactState requestTemporary;
    PythonRunArtifactState result;
    PythonRunArtifactState resultTemporary;
    String error;
};

struct PythonUtf8StreamState {
    std::uint32_t codePoint;
    std::uint32_t minimumCodePoint;
    std::uint8_t remainingBytes;
    bool valid;
};

inline PythonUtf8StreamState consumePythonUtf8Chunk(
    PythonUtf8StreamState state,
    const std::uint8_t* bytes,
    std::size_t size)
{
    if (!state.valid || (bytes == nullptr && size != 0)) {
        state.valid = false;
        return state;
    }
    for (std::size_t index = 0; index < size && state.valid; ++index) {
        const std::uint8_t byte = bytes[index];
        if (state.remainingBytes == 0) {
            if (byte <= 0x7fU) continue;
            if (byte >= 0xc2U && byte <= 0xdfU) {
                state.codePoint = byte & 0x1fU;
                state.minimumCodePoint = 0x80U;
                state.remainingBytes = 1;
            } else if (byte >= 0xe0U && byte <= 0xefU) {
                state.codePoint = byte & 0x0fU;
                state.minimumCodePoint = 0x800U;
                state.remainingBytes = 2;
            } else if (byte >= 0xf0U && byte <= 0xf4U) {
                state.codePoint = byte & 0x07U;
                state.minimumCodePoint = 0x10000U;
                state.remainingBytes = 3;
            } else {
                state.valid = false;
            }
            continue;
        }
        if ((byte & 0xc0U) != 0x80U) {
            state.valid = false;
            continue;
        }
        state.codePoint = (state.codePoint << 6U) | (byte & 0x3fU);
        --state.remainingBytes;
        if (state.remainingBytes == 0 &&
            (state.codePoint < state.minimumCodePoint ||
             state.codePoint > 0x10ffffU ||
             (state.codePoint >= 0xd800U && state.codePoint <= 0xdfffU))) {
            state.valid = false;
        }
    }
    return state;
}

inline bool pythonUtf8StreamComplete(const PythonUtf8StreamState& state)
{
    return state.valid && state.remainingBytes == 0;
}

struct PythonRunStageRequest {
    String pendingId;
    String fileName;
    std::uint64_t sourceBytes;
    std::string sourceSha256;
    std::uint64_t auditSequence;
    PythonRunReturnSurface returnSurface;
};

struct PythonRunStageResult {
    bool success;
    bool restartRequired;
    bool startupRecoveryRequired;
    String error;
};

inline bool pythonRunStageFailureAllowsContinuation(
    const PythonRunStageResult& result)
{
    return !result.success && !result.startupRecoveryRequired;
}

struct PythonRunRecoveryResult {
    bool success;
    bool found;
    bool attachmentAllowed;
    bool executionSucceeded;
    bool hasExitStatus;
    std::int32_t exitStatus;
    std::uint32_t outputBytes;
    String pendingId;
    std::uint64_t auditSequence;
    PythonRunReturnSurface returnSurface;
    std::string assistantMessage;
    String error;
};

PythonModeStatus inspectPythonMode();
bool pythonOneShotAvailable();
OperationResult synchronizePythonModeSettings(const Settings& settings,
                                              const String& consolePassword,
                                              const String& handoffToken);
OperationResult activatePythonMode();
OperationResult stageCardMindUpdateForPython(std::uint32_t firmwareBytes,
                                             const String& sha256);
OperationResult clearCardMindUpdateRequest();
PythonHandoffRequest consumePythonHandoffRequest();
PythonHandoffRequest consumePythonSdHandoffRequest();
OperationResult acknowledgePythonHandoffRequest();
OperationResult preparePythonRunStaging();
PythonRunArtifactsInspection inspectPythonRunArtifacts();
PythonRunStageResult stagePythonRun(
    const PythonRunStageRequest& request,
    const std::function<bool()>& isCancelled);
PythonRunRecoveryResult loadPythonRunRecovery();
PythonRunRecoveryResult loadDetachedPythonRunRecovery(
    const String& expectedPendingId,
    const String& expectedFileName,
    std::uint64_t expectedSourceBytes,
    const std::string& expectedSourceSha256);
OperationResult validateDetachedPythonRunRequest(
    const String& expectedPendingId,
    const String& expectedFileName,
    std::uint64_t expectedSourceBytes,
    const std::string& expectedSourceSha256);
OperationResult finalizePythonRunHandoff(PythonRunReturnSurface returnSurface);
OperationResult discardPythonRunState();
OperationResult cleanupPythonRunArtifacts();

}  // namespace cardputer
