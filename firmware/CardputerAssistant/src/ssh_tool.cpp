#include "ssh_tool.h"

#include "ssh_client.h"
#include "ssh_command_output.h"
#include "ssh_command_options.h"
#include "text_utils.h"

#include <ArduinoJson.h>

#include <cstring>

namespace cardputer {
namespace {

ToolExecutionResult toolError(const String& error)
{
    JsonDocument document;
    document["ok"] = false;
    document["error"] = error;
    std::string output;
    serializeJson(document, output);
    return {false, output, error};
}

ToolExecutionResult toolCanceled(const String& error)
{
    JsonDocument document;
    document["ok"] = false;
    document["error"] = error;
    std::string output;
    serializeJson(document, output);
    return {
        false,
        output,
        error,
        ToolExecutionOutcome::Cancelled,
    };
}

ToolExecutionResult terminalStateResult(SshCommandTerminalState state,
                                        std::uint32_t timeoutMs,
                                        const String& stage,
                                        const String& storageDetail)
{
    if (state == SshCommandTerminalState::UserCancelled) {
        return toolCanceled(
            String("SSH command canceled ") + stage + storageDetail);
    }
    return toolError(String("SSH command timed out after ") + String(timeoutMs) +
                     " ms " + stage + storageDetail);
}

String outputStorageDetail(const SshCommandOutputCapture& capture,
                           const OperationResult& storage)
{
    String detail;
    if (capture.hasLog()) {
        detail = String(" Retained SSH output log name: ") + capture.logName() + ".";
        if (storage.success && capture.isComplete()) {
            detail += String(" Download: ") + capture.downloadPath() + ".";
        }
    }
    if (!storage.success) {
        detail += String(" Output storage failed: ") + storage.error + ".";
    }
    return detail;
}

String commandFailureWithStorage(const String& primary,
                                 const OperationResult& storage,
                                 const String& detail)
{
    if (!storage.success && primary == storage.error) {
        return primary + (detail.startsWith(" Retained") ? detail.substring(
            0, detail.indexOf(" Output storage failed:")) : String());
    }
    return primary + detail;
}

}  // namespace

bool isSshToolName(const std::string& name)
{
    return name == "ssh_command";
}

std::uint64_t sshToolAvailableProfileId()
{
    SshProfile profile;
    std::uint64_t profileId = 0;
    const OperationResult loaded = loadSshProfileWithId(profile, profileId);
    const bool available = loaded.success && sshProfileIsComplete(profile);
    profile.password = "";
    profile.privateKeyPassphrase = "";
    return available ? profileId : 0;
}

bool sshToolIsAvailable()
{
    return sshToolAvailableProfileId() != 0;
}

SshCommandArgumentsResult parseSshCommandArguments(
    const std::string& argumentsJson)
{
    JsonDocument arguments;
    const DeserializationError parsed = deserializeJson(arguments, argumentsJson);
    if (parsed || !arguments.is<JsonObject>()) {
        return {false, "", 0, 0, "SSH tool arguments must be a JSON object"};
    }
    const JsonObjectConst input = arguments.as<JsonObjectConst>();
    if (input.size() < 1 || input.size() > 3 ||
        !input["command"].is<const char*>()) {
        return {
            false, "", 0, 0,
            "SSH tool arguments require command and only its optional timeout/output fields",
        };
    }
    for (JsonPairConst field : input) {
        const char* name = field.key().c_str();
        if (std::strcmp(name, "command") != 0 &&
            std::strcmp(name, "timeout_ms") != 0 &&
            std::strcmp(name, "max_inline_output_bytes") != 0) {
            return {false, "", 0, 0, "SSH tool arguments contain an unknown field"};
        }
    }
    const JsonString commandValue = input["command"].as<JsonString>();
    if (commandValue.isNull() || commandValue.size() == 0 ||
        commandValue.size() > 1024 ||
        std::strlen(commandValue.c_str()) != commandValue.size() ||
        !isValidUtf8(commandValue.c_str())) {
        return {
            false, "", 0, 0,
            "SSH command must be valid UTF-8 between 1 and 1024 bytes without NUL characters",
        };
    }
    const String command(commandValue.c_str());
    const bool hasTimeout = input.containsKey("timeout_ms");
    const bool hasOutputLimit = input.containsKey("max_inline_output_bytes");
    if ((hasTimeout && !input["timeout_ms"].is<std::uint32_t>()) ||
        (hasOutputLimit &&
         !input["max_inline_output_bytes"].is<std::size_t>())) {
        return {false, "", 0, 0, "SSH command timeout/output options must be integers"};
    }
    const std::uint32_t timeoutMs = hasTimeout
        ? input["timeout_ms"].as<std::uint32_t>()
        : kDefaultSshCommandTimeoutMs;
    const std::size_t maximumOutputBytes = hasOutputLimit
        ? input["max_inline_output_bytes"].as<std::size_t>()
        : kDefaultSshCommandInlineOutputBytes;
    if (!isValidSshCommandTimeout(timeoutMs) ||
        !isValidSshCommandInlineOutputLimit(maximumOutputBytes)) {
        return {
            false, "", 0, 0,
            "SSH command timeout/output options are outside current limits",
        };
    }
    return {true, command, timeoutMs, maximumOutputBytes, ""};
}

SshSafeActionArgumentsResult parseSshSafeActionArguments(
    const std::string& argumentsJson)
{
    JsonDocument arguments;
    const DeserializationError parsed = deserializeJson(arguments, argumentsJson);
    if (parsed || !arguments.is<JsonObject>()) {
        return {false, "", "", 0, 0,
                "SSH Safe Action arguments must be a JSON object"};
    }
    const JsonObjectConst input = arguments.as<JsonObjectConst>();
    if (input.size() < 1 || input.size() > 3 ||
        !input["action"].is<const char*>()) {
        return {
            false, "", "", 0, 0,
            "SSH Safe Action requires action and only its optional timeout/output fields",
        };
    }
    for (JsonPairConst field : input) {
        const char* name = field.key().c_str();
        if (std::strcmp(name, "action") != 0 &&
            std::strcmp(name, "timeout_ms") != 0 &&
            std::strcmp(name, "max_inline_output_bytes") != 0) {
            return {false, "", "", 0, 0,
                    "SSH Safe Action contains an unknown field"};
        }
    }
    const JsonString actionValue = input["action"].as<JsonString>();
    if (actionValue.isNull() ||
        std::strlen(actionValue.c_str()) != actionValue.size()) {
        return {false, "", "", 0, 0,
                "SSH Safe Action id is invalid"};
    }
    const std::string actionId(
        actionValue.c_str(), actionValue.size());
    const SshSafeActionEntry* action = sshSafeActionEntryForId(actionId);
    if (action == nullptr) {
        return {false, "", "", 0, 0,
                "SSH Safe Action id is not in the fixed reviewed set"};
    }
    const bool hasTimeout = input.containsKey("timeout_ms");
    const bool hasOutputLimit =
        input.containsKey("max_inline_output_bytes");
    if ((hasTimeout && !input["timeout_ms"].is<std::uint32_t>()) ||
        (hasOutputLimit &&
         !input["max_inline_output_bytes"].is<std::size_t>())) {
        return {false, "", "", 0, 0,
                "SSH Safe Action timeout/output options must be integers"};
    }
    const std::uint32_t timeoutMs = hasTimeout
        ? input["timeout_ms"].as<std::uint32_t>()
        : kDefaultSshCommandTimeoutMs;
    const std::size_t maximumOutputBytes = hasOutputLimit
        ? input["max_inline_output_bytes"].as<std::size_t>()
        : kDefaultSshCommandInlineOutputBytes;
    if (!isValidSshCommandTimeout(timeoutMs) ||
        !isValidSshCommandInlineOutputLimit(maximumOutputBytes)) {
        return {false, "", "", 0, 0,
                "SSH Safe Action timeout/output options are outside current limits"};
    }
    return {
        true,
        action->id,
        action->command,
        timeoutMs,
        maximumOutputBytes,
        "",
    };
}

static ToolExecutionResult executeSshCommand(
    const String& command,
    std::uint32_t timeoutMs,
    std::size_t maximumOutputBytes,
    const CancelCallback& isCancelled)
{
    if (isCancelled()) {
        return toolCanceled("SSH command canceled before connection");
    }
    SshProfile profile;
    OperationResult result = loadSshProfile(profile);
    if (!result.success || !sshProfileIsComplete(profile)) {
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return toolError(result.success ? String("Selected SSH profile is incomplete")
                                        : result.error);
    }
    SshCommandTerminalState terminalState = SshCommandTerminalState::None;
    const std::uint32_t startedAt = millis();
    const CancelCallback observeTerminalState = [&]() {
        terminalState = observeSshCommandTerminalState(
            terminalState, isCancelled(), startedAt, millis(), timeoutMs);
        return terminalState != SshCommandTerminalState::None;
    };
    if (observeTerminalState()) {
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return terminalStateResult(terminalState, timeoutMs, "before connection", "");
    }
    SshClient client;
    result = client.connectControlled(
        profile, timeoutMs, observeTerminalState);
    observeTerminalState();
    if (terminalState != SshCommandTerminalState::None) {
        client.close();
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return terminalStateResult(terminalState, timeoutMs, "during connection", "");
    }
    if (!result.success) {
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return toolError(result.error);
    }
    const SshTrustResult trust = checkTrustedSshHost(
        profile.host, profile.port, client.fingerprint());
    observeTerminalState();
    if (terminalState != SshCommandTerminalState::None) {
        client.close();
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return terminalStateResult(
            terminalState, timeoutMs, "during host-key verification", "");
    }
    if (!trust.success || !trust.found || !trust.matches) {
        client.close();
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return toolError(trust.success
            ? String("Selected SSH host key is not trusted; connect manually first")
            : trust.error);
    }
    result = client.authenticateControlled(
        profile, timeoutMs, observeTerminalState);
    observeTerminalState();
    profile.password = "";
    profile.privateKeyPassphrase = "";
    if (terminalState != SshCommandTerminalState::None) {
        client.close();
        return terminalStateResult(
            terminalState, timeoutMs, "during authentication", "");
    }
    if (!result.success) {
        client.close();
        return toolError(result.error);
    }
    SshCommandOutputCapture capture(maximumOutputBytes);
    int exitStatus = -1;
    const SshCommandOutputCallback onOutput =
        [&capture](const std::uint8_t* data, std::size_t bytes) {
            return capture.append(data, bytes);
        };
    result = client.executeCommandStreamingControlled(
        command, exitStatus, timeoutMs, observeTerminalState, onOutput);
    observeTerminalState();
    client.close();

    const bool invalidInline =
        result.success && !capture.hasLog() &&
        !isValidUtf8(capture.inlineOutput());
    OperationResult storage = {true, ""};
    if (capture.hasOutput() &&
        (capture.hasLog() ||
         terminalState != SshCommandTerminalState::None ||
         !result.success || invalidInline)) {
        storage = capture.promoteToLog();
    }
    const OperationResult finalized = capture.finalize();
    if (storage.success && !finalized.success) {
        storage = finalized;
    }
    const String storageDetail = outputStorageDetail(capture, storage);

    if (terminalState != SshCommandTerminalState::None) {
        return terminalStateResult(
            terminalState, timeoutMs, "during execution", storageDetail);
    }
    if (!result.success) {
        return toolError(commandFailureWithStorage(
            result.error, storage, storageDetail));
    }
    if (invalidInline) {
        return toolError(commandFailureWithStorage(
            "SSH command returned non-UTF-8 output", storage, storageDetail));
    }
    if (!storage.success) {
        return toolError(storage.error + storageDetail);
    }

    JsonDocument document;
    document["ok"] = true;
    document["exit_status"] = exitStatus;
    if (capture.hasLog()) {
        if (!capture.isComplete()) {
            return toolError(
                "SSH command output log could not be verified" + storageDetail);
        }
        document["output_bytes"] = capture.verifiedOutputBytes();
        document["summary"] =
            "SSH command output stored in a downloadable microSD log";
        JsonObject outputLog = document["output_log"].to<JsonObject>();
        outputLog["name"] = capture.logName();
        outputLog["download_path"] = capture.downloadPath();
    } else {
        document["output"] = capture.inlineOutput();
    }
    std::string output;
    serializeJson(document, output);
    return {
        true,
        output,
        "",
        ToolExecutionOutcome::Finished,
        true,
        exitStatus,
    };
}

ToolExecutionResult executeSshTool(const ToolCall& call,
                                   const CancelCallback& isCancelled)
{
    if (!isSshToolName(call.name)) {
        return toolError("Unsupported SSH tool name");
    }
    if (isCancelled()) {
        return toolCanceled("SSH command canceled before connection");
    }
    const SshCommandArgumentsResult arguments =
        parseSshCommandArguments(call.arguments);
    if (!arguments.success) {
        return toolError(arguments.error);
    }
    return executeSshCommand(
        arguments.command, arguments.timeoutMs,
        arguments.maximumInlineOutputBytes, isCancelled);
}

ToolExecutionResult executeSshSafeActionTool(
    const ToolCall& call,
    const CancelCallback& isCancelled)
{
    if (call.name != "ssh_safe_action") {
        return toolError("Unsupported SSH Safe Action tool name");
    }
    if (isCancelled()) {
        return toolCanceled("SSH command canceled before connection");
    }
    const SshSafeActionArgumentsResult arguments =
        parseSshSafeActionArguments(call.arguments);
    if (!arguments.success) {
        return toolError(arguments.error);
    }
    return executeSshCommand(
        arguments.command, arguments.timeoutMs,
        arguments.maximumInlineOutputBytes, isCancelled);
}

}  // namespace cardputer
