#include "ssh_tool.h"

#include "ssh_client.h"
#include "text_utils.h"

#include <ArduinoJson.h>

namespace cardputer {
namespace {

constexpr std::size_t kMaximumSshCommandOutputBytes = 16384;
constexpr std::uint32_t kSshToolTimeoutMs = 60000;

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

ToolExecutionResult executeSshTool(const ToolCall& call,
                                   const CancelCallback& isCancelled)
{
    if (!isSshToolName(call.name)) {
        return toolError("Unsupported SSH tool name");
    }
    bool cancelled = false;
    const CancelCallback latchedCancellation = [&]() {
        cancelled = cancelled || isCancelled();
        return cancelled;
    };
    if (latchedCancellation()) {
        return toolCanceled("SSH command canceled before connection");
    }
    JsonDocument arguments;
    const DeserializationError parsed = deserializeJson(arguments, call.arguments);
    if (parsed || !arguments.is<JsonObject>() ||
        !arguments["command"].is<const char*>() || arguments.as<JsonObject>().size() != 1) {
        return toolError("SSH tool arguments require exactly one string field: command");
    }
    const String command = arguments["command"].as<const char*>();
    if (command.isEmpty() || command.length() > 1024 ||
        !isValidUtf8(command.c_str())) {
        return toolError("SSH command must be valid UTF-8 between 1 and 1024 bytes");
    }
    SshProfile profile;
    OperationResult result = loadSshProfile(profile);
    if (!result.success || !sshProfileIsComplete(profile)) {
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return toolError(result.success ? String("Selected SSH profile is incomplete")
                                        : result.error);
    }
    SshClient client;
    result = client.connectControlled(
        profile, kSshToolTimeoutMs, latchedCancellation);
    if (!result.success) {
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return cancelled ? toolCanceled(result.error) : toolError(result.error);
    }
    const SshTrustResult trust = checkTrustedSshHost(
        profile.host, profile.port, client.fingerprint());
    if (!trust.success || !trust.found || !trust.matches) {
        client.close();
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return toolError(trust.success
            ? String("Selected SSH host key is not trusted; connect manually first")
            : trust.error);
    }
    result = client.authenticateControlled(
        profile, kSshToolTimeoutMs, latchedCancellation);
    profile.password = "";
    profile.privateKeyPassphrase = "";
    if (!result.success) {
        client.close();
        return cancelled ? toolCanceled(result.error) : toolError(result.error);
    }
    if (latchedCancellation()) {
        client.close();
        return toolCanceled("SSH command canceled before execution");
    }
    std::string commandOutput;
    int exitStatus = -1;
    result = client.executeCommandControlled(
        command, commandOutput, exitStatus, kMaximumSshCommandOutputBytes,
        kSshToolTimeoutMs, latchedCancellation);
    client.close();
    if (!result.success) {
        return cancelled ? toolCanceled(result.error) : toolError(result.error);
    }
    if (!isValidUtf8(commandOutput)) {
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
