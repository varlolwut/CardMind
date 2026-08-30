#include "ssh_tool.h"

#include "ssh_client.h"
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
                                        const String& stage)
{
    if (state == SshCommandTerminalState::UserCancelled) {
        return toolCanceled(String("SSH command canceled ") + stage);
    }
    return toolError(String("SSH command timed out after ") + String(timeoutMs) +
                     " ms " + stage);
}

}  // namespace

bool isSshToolName(const std::string& name)
{
    return name == "ssh_command";
}

bool sshToolIsAvailable()
{
    SshProfile profile;
    const OperationResult loaded = loadSshProfile(profile);
    const bool available = loaded.success && sshProfileIsComplete(profile);
    profile.password = "";
    profile.privateKeyPassphrase = "";
    return available;
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
    const String& command = arguments.command;
    const std::uint32_t timeoutMs = arguments.timeoutMs;
    const std::size_t maximumOutputBytes = arguments.maximumInlineOutputBytes;
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
        return terminalStateResult(terminalState, timeoutMs, "before connection");
    }
    SshClient client;
    result = client.connectControlled(
        profile, timeoutMs, observeTerminalState);
    observeTerminalState();
    if (terminalState != SshCommandTerminalState::None) {
        client.close();
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return terminalStateResult(terminalState, timeoutMs, "during connection");
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
        return terminalStateResult(terminalState, timeoutMs, "during host-key verification");
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
        return terminalStateResult(terminalState, timeoutMs, "during authentication");
    }
    if (!result.success) {
        client.close();
        return toolError(result.error);
    }
    std::string commandOutput;
    int exitStatus = -1;
    result = client.executeCommandControlled(
        command, commandOutput, exitStatus, maximumOutputBytes,
        timeoutMs, observeTerminalState);
    observeTerminalState();
    client.close();
    if (terminalState != SshCommandTerminalState::None) {
        std::string().swap(commandOutput);
        return terminalStateResult(terminalState, timeoutMs, "during execution");
    }
    if (!result.success) {
        std::string().swap(commandOutput);
        return toolError(result.error);
    }
    if (!isValidUtf8(commandOutput)) {
        std::string().swap(commandOutput);
        return toolError("SSH command returned output that is not valid UTF-8");
    }
    JsonDocument document;
    document["ok"] = true;
    document["exit_status"] = exitStatus;
    document["output"] = commandOutput;
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

}  // namespace cardputer
