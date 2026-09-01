#include "sftp_tool.h"

#include "ssh_client.h"
#include "text_utils.h"

#include <ArduinoJson.h>

#include <cstring>
#include <functional>
#include <string>
#include <utility>

namespace cardputer {
namespace {

enum class SftpTerminalState : std::uint8_t {
    None,
    UserCancelled,
    TimedOut,
};

struct SftpListArguments {
    String path;
    std::uint32_t offset;
    std::size_t maximumEntries;
};

struct SftpReadArguments {
    String path;
    std::uint64_t offset;
    std::size_t maximumBytes;
};

struct SftpWriteArguments {
    String path;
    std::string content;
    bool overwrite;
};

struct SftpMoveArguments {
    String sourcePath;
    String destinationPath;
    bool overwrite;
};

template <typename Value>
struct ParsedSftpArguments {
    bool success;
    Value value;
    String error;
};

bool hasExactFields(const JsonObjectConst& input,
                    const char* const* fields,
                    std::size_t count)
{
    if (input.size() != count) {
        return false;
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (!input.containsKey(fields[index])) {
            return false;
        }
    }
    return true;
}

bool readRemotePath(JsonVariantConst input, String& path)
{
    if (!input.is<const char*>()) {
        return false;
    }
    const JsonString value = input.as<JsonString>();
    if (value.isNull() || !isValidModelSftpPath(value.c_str(), value.size())) {
        return false;
    }
    path = String(value.c_str());
    return true;
}

ParsedSftpArguments<SftpListArguments> parseListArguments(
    const std::string& arguments)
{
    JsonDocument document;
    const DeserializationError parsed = deserializeJson(document, arguments);
    if (parsed || !document.is<JsonObject>()) {
        return {false, {}, "sftp_list arguments must be one JSON object"};
    }
    const JsonObjectConst input = document.as<JsonObjectConst>();
    static constexpr const char* kFields[] = {
        "path", "offset", "max_entries",
    };
    SftpListArguments value = {};
    if (!hasExactFields(input, kFields, 3) ||
        !readRemotePath(input["path"], value.path) ||
        !input["offset"].is<std::uint32_t>() ||
        !input["max_entries"].is<std::size_t>()) {
        return {false, {}, "sftp_list requires exact path, offset and max_entries fields"};
    }
    value.offset = input["offset"].as<std::uint32_t>();
    value.maximumEntries = input["max_entries"].as<std::size_t>();
    if (value.maximumEntries == 0 ||
        value.maximumEntries > kMaximumModelSftpPageEntries) {
        return {false, {}, "sftp_list max_entries must be between 1 and 16"};
    }
    return {true, std::move(value), ""};
}

ParsedSftpArguments<SftpReadArguments> parseReadArguments(
    const std::string& arguments)
{
    JsonDocument document;
    const DeserializationError parsed = deserializeJson(document, arguments);
    if (parsed || !document.is<JsonObject>()) {
        return {false, {}, "sftp_read arguments must be one JSON object"};
    }
    const JsonObjectConst input = document.as<JsonObjectConst>();
    static constexpr const char* kFields[] = {
        "path", "offset", "max_bytes",
    };
    SftpReadArguments value = {};
    if (!hasExactFields(input, kFields, 3) ||
        !readRemotePath(input["path"], value.path) ||
        !input["offset"].is<std::uint64_t>() ||
        !input["max_bytes"].is<std::size_t>()) {
        return {false, {}, "sftp_read requires exact path, offset and max_bytes fields"};
    }
    value.offset = input["offset"].as<std::uint64_t>();
    value.maximumBytes = input["max_bytes"].as<std::size_t>();
    if (value.maximumBytes < kMinimumModelSftpReadBytes ||
        value.maximumBytes > kMaximumModelSftpChunkBytes) {
        return {false, {}, "sftp_read max_bytes must be between 4 and 12288"};
    }
    return {true, std::move(value), ""};
}

ParsedSftpArguments<SftpWriteArguments> parseWriteArguments(
    const std::string& arguments)
{
    JsonDocument document;
    const DeserializationError parsed = deserializeJson(document, arguments);
    if (parsed || !document.is<JsonObject>()) {
        return {false, {}, "sftp_write arguments must be one JSON object"};
    }
    const JsonObjectConst input = document.as<JsonObjectConst>();
    const bool hasOverwrite = input.containsKey("overwrite");
    static constexpr const char* kRequiredFields[] = {"path", "content"};
    static constexpr const char* kOverwriteFields[] = {
        "path", "content", "overwrite",
    };
    SftpWriteArguments value = {};
    if (!(hasOverwrite
              ? hasExactFields(input, kOverwriteFields, 3)
              : hasExactFields(input, kRequiredFields, 2)) ||
        !readRemotePath(input["path"], value.path) ||
        !input["content"].is<const char*>() ||
        (hasOverwrite && !input["overwrite"].is<bool>())) {
        return {false, {}, "sftp_write requires path, content and optional boolean overwrite"};
    }
    const JsonString content = input["content"].as<JsonString>();
    if (content.isNull() || content.size() > kMaximumModelSftpChunkBytes ||
        !isValidModelSftpText(content.c_str(), content.size())) {
        return {false, {}, "sftp_write content must be JSON-safe UTF-8 text up to 12288 bytes"};
    }
    value.content.assign(content.c_str(), content.size());
    value.overwrite = hasOverwrite && input["overwrite"].as<bool>();
    return {true, std::move(value), ""};
}

ParsedSftpArguments<SftpMoveArguments> parseMoveArguments(
    const std::string& arguments)
{
    JsonDocument document;
    const DeserializationError parsed = deserializeJson(document, arguments);
    if (parsed || !document.is<JsonObject>()) {
        return {false, {}, "sftp_move arguments must be one JSON object"};
    }
    const JsonObjectConst input = document.as<JsonObjectConst>();
    const bool hasOverwrite = input.containsKey("overwrite");
    static constexpr const char* kRequiredFields[] = {
        "source_path", "destination_path",
    };
    static constexpr const char* kOverwriteFields[] = {
        "source_path", "destination_path", "overwrite",
    };
    SftpMoveArguments value = {};
    if (!(hasOverwrite
              ? hasExactFields(input, kOverwriteFields, 3)
              : hasExactFields(input, kRequiredFields, 2)) ||
        !readRemotePath(input["source_path"], value.sourcePath) ||
        !readRemotePath(input["destination_path"], value.destinationPath) ||
        (hasOverwrite && !input["overwrite"].is<bool>()) ||
        value.sourcePath == value.destinationPath) {
        return {false, {}, "sftp_move requires distinct source/destination paths and optional boolean overwrite"};
    }
    value.overwrite = hasOverwrite && input["overwrite"].as<bool>();
    return {true, std::move(value), ""};
}

ToolExecutionResult toolError(const String& error)
{
    JsonDocument document;
    document["ok"] = false;
    document["error"] = error;
    std::string output;
    serializeJson(document, output);
    return {false, output, error};
}

ToolExecutionResult toolCancelled(const String& error)
{
    ToolExecutionResult result = toolError(error);
    result.outcome = ToolExecutionOutcome::Cancelled;
    return result;
}

SftpTerminalState observeTerminalState(SftpTerminalState current,
                                       bool cancelled,
                                       std::uint32_t startedAt,
                                       std::uint32_t now)
{
    if (current != SftpTerminalState::None) {
        return current;
    }
    if (cancelled) {
        return SftpTerminalState::UserCancelled;
    }
    return static_cast<std::uint32_t>(now - startedAt) >= kModelSftpTimeoutMs
        ? SftpTerminalState::TimedOut : SftpTerminalState::None;
}

std::uint32_t remainingBudget(std::uint32_t startedAt)
{
    const std::uint32_t elapsed = static_cast<std::uint32_t>(millis() - startedAt);
    return elapsed >= kModelSftpTimeoutMs ? 0 : kModelSftpTimeoutMs - elapsed;
}

ToolExecutionResult terminalResult(SftpTerminalState state,
                                   const String& stage,
                                   const String& detail)
{
    if (state == SftpTerminalState::UserCancelled) {
        return toolCancelled("SFTP operation canceled " + stage + detail);
    }
    return toolError("SFTP operation timed out after 60000 ms " + stage + detail);
}

template <typename Operation>
ToolExecutionResult withSelectedSftp(const CancelCallback& isCancelled,
                                     Operation operation)
{
    if (!isCancelled) {
        return toolError("SFTP operation requires a cancellation callback");
    }
    SftpTerminalState terminalState = SftpTerminalState::None;
    const std::uint32_t startedAt = millis();
    const CancelCallback observe = [&]() {
        terminalState = observeTerminalState(
            terminalState, isCancelled(), startedAt, millis());
        return terminalState != SftpTerminalState::None;
    };
    if (observe()) {
        return terminalResult(terminalState, "before connection", "");
    }
    SshProfile profile;
    OperationResult result = loadSshProfile(profile);
    if (!result.success || !sshProfileIsComplete(profile)) {
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return toolError(result.success
            ? String("Selected SSH profile is incomplete") : result.error);
    }
    SshClient client;
    std::uint32_t budget = remainingBudget(startedAt);
    if (budget < 1000) {
        terminalState = SftpTerminalState::TimedOut;
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return terminalResult(terminalState, "before connection", "");
    }
    result = client.connectControlled(profile, budget, observe);
    observe();
    if (terminalState != SftpTerminalState::None) {
        client.close();
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return terminalResult(terminalState, "during connection", "");
    }
    if (!result.success) {
        client.close();
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return toolError(result.error);
    }
    const SshTrustResult trust = checkTrustedSshHost(
        profile.host, profile.port, client.fingerprint());
    observe();
    if (terminalState != SftpTerminalState::None) {
        client.close();
        profile.password = "";
        profile.privateKeyPassphrase = "";
        return terminalResult(terminalState, "during host-key verification", "");
    }
    if (!trust.success || !trust.found || !trust.matches) {
        client.close();
        profile.password = "";
        profile.privateKeyPassphrase = "";
        if (trust.success && trust.found && !trust.matches) {
            return toolError("Selected SSH host key mismatch; SFTP connection blocked");
        }
        return toolError(trust.success
            ? String("Selected SSH host key is not trusted; connect manually first")
            : trust.error);
    }
    budget = remainingBudget(startedAt);
    if (budget < 1000) {
        client.close();
        profile.password = "";
        profile.privateKeyPassphrase = "";
        terminalState = SftpTerminalState::TimedOut;
        return terminalResult(terminalState, "before authentication", "");
    }
    result = client.authenticateControlled(profile, budget, observe);
    profile.password = "";
    profile.privateKeyPassphrase = "";
    observe();
    if (terminalState != SftpTerminalState::None) {
        client.close();
        return terminalResult(terminalState, "during authentication", "");
    }
    if (!result.success) {
        client.close();
        return toolError(result.error);
    }
    budget = remainingBudget(startedAt);
    if (budget == 0) {
        client.close();
        terminalState = SftpTerminalState::TimedOut;
        return terminalResult(terminalState, "before SFTP initialization", "");
    }
    result = client.openSftpControlled(budget, observe);
    observe();
    if (terminalState != SftpTerminalState::None) {
        client.close();
        return terminalResult(terminalState, "during SFTP initialization", "");
    }
    if (!result.success) {
        client.close();
        return toolError(result.error);
    }
    ToolExecutionResult executed = operation(
        client, observe, startedAt, terminalState);
    client.close();
    return executed;
}

}  // namespace

bool isModelSftpSchema(ToolSchemaId schema) noexcept
{
    return schema == ToolSchemaId::SftpList ||
        schema == ToolSchemaId::SftpRead ||
        schema == ToolSchemaId::SftpWrite ||
        schema == ToolSchemaId::SftpMove;
}

bool isValidModelSftpText(const char* value, std::size_t bytes) noexcept
{
    if (bytes == 0) return true;
    if (value == nullptr || std::strlen(value) != bytes ||
        !isValidUtf8(value, bytes)) {
        return false;
    }
    for (std::size_t index = 0; index < bytes; ++index) {
        const unsigned char byte = static_cast<unsigned char>(value[index]);
        if ((byte < 0x20 && byte != '\t' && byte != '\r' && byte != '\n') ||
            byte == 0x7F) {
            return false;
        }
    }
    return true;
}

bool isValidModelSftpPath(const char* value, std::size_t bytes) noexcept
{
    return value != nullptr && bytes > 0 &&
        bytes <= kMaximumModelSftpPathBytes && value[0] == '/' &&
        std::strlen(value) == bytes &&
        std::memchr(value, '\r', bytes) == nullptr &&
        std::memchr(value, '\n', bytes) == nullptr &&
        isValidUtf8(value, bytes);
}

SftpOverwriteInspection inspectSftpOverwrite(
    ToolSchemaId schema,
    const ToolCall& call)
{
    if (schema == ToolSchemaId::SftpWrite) {
        const auto parsed = parseWriteArguments(call.arguments);
        return {parsed.success, parsed.success && parsed.value.overwrite,
                parsed.error};
    }
    if (schema == ToolSchemaId::SftpMove) {
        const auto parsed = parseMoveArguments(call.arguments);
        return {parsed.success, parsed.success && parsed.value.overwrite,
                parsed.error};
    }
    return {false, false, "Overwrite inspection requires sftp_write or sftp_move"};
}

ToolExecutionResult executeSftpListTool(
    const ToolCall& call,
    const CancelCallback& isCancelled)
{
    if (call.name != "sftp_list") {
        return toolError("Unsupported SFTP list tool name");
    }
    const auto arguments = parseListArguments(call.arguments);
    if (!arguments.success) {
        return toolError(arguments.error);
    }
    return withSelectedSftp(
        isCancelled,
        [&arguments](SshClient& client, const CancelCallback& observe,
                     std::uint32_t startedAt, SftpTerminalState& state) {
            const std::uint32_t budget = remainingBudget(startedAt);
            if (budget == 0) {
                state = SftpTerminalState::TimedOut;
                return terminalResult(state, "before directory listing", "");
            }
            const SftpPageResult page = client.listSftpDirectoryPageControlled(
                arguments.value.path, arguments.value.offset,
                arguments.value.maximumEntries, budget, observe);
            observe();
            if (state != SftpTerminalState::None) {
                return terminalResult(state, "during directory listing", "");
            }
            if (!page.success) {
                return toolError(page.error);
            }
            JsonDocument document;
            document["ok"] = true;
            document["offset"] = arguments.value.offset;
            document["next_offset"] = page.nextOffset;
            document["eof"] = page.eof;
            JsonArray entries = document["entries"].to<JsonArray>();
            for (const SftpEntry& entry : page.entries) {
                if (!isValidModelSftpText(
                        entry.name.c_str(), entry.name.length())) {
                    return toolError(
                        "SFTP directory contains a name that is not model-safe text");
                }
                JsonObject item = entries.add<JsonObject>();
                item["name"] = entry.name;
                item["type"] = entry.directory ? "directory" : "file";
                item["size"] = entry.size;
            }
            std::string output;
            serializeJson(document, output);
            return ToolExecutionResult{true, output, ""};
        });
}

ToolExecutionResult executeSftpReadTool(
    const ToolCall& call,
    const CancelCallback& isCancelled)
{
    if (call.name != "sftp_read") {
        return toolError("Unsupported SFTP read tool name");
    }
    const auto arguments = parseReadArguments(call.arguments);
    if (!arguments.success) {
        return toolError(arguments.error);
    }
    return withSelectedSftp(
        isCancelled,
        [&arguments](SshClient& client, const CancelCallback& observe,
                     std::uint32_t startedAt, SftpTerminalState& state) {
            const std::uint32_t budget = remainingBudget(startedAt);
            if (budget == 0) {
                state = SftpTerminalState::TimedOut;
                return terminalResult(state, "before file read", "");
            }
            const SftpReadResult read = client.readSftpFileChunkControlled(
                arguments.value.path, arguments.value.offset,
                arguments.value.maximumBytes, budget, observe);
            observe();
            if (state != SftpTerminalState::None) {
                return terminalResult(state, "during file read", "");
            }
            if (!read.success) {
                return toolError(read.error);
            }
            if (!isValidModelSftpText(
                    read.content.data(), read.content.size())) {
                return toolError(
                    "SFTP file chunk contains text controls that cannot fit the model result envelope");
            }
            JsonDocument document;
            document["ok"] = true;
            document["offset"] = arguments.value.offset;
            document["next_offset"] = read.nextOffset;
            document["total_bytes"] = read.totalBytes;
            document["eof"] = read.eof;
            document["content"] = read.content;
            std::string output;
            serializeJson(document, output);
            return ToolExecutionResult{true, output, ""};
        });
}

ToolExecutionResult executeSftpWriteTool(
    const ToolCall& call,
    const CancelCallback& isCancelled)
{
    if (call.name != "sftp_write") {
        return toolError("Unsupported SFTP write tool name");
    }
    const auto arguments = parseWriteArguments(call.arguments);
    if (!arguments.success) {
        return toolError(arguments.error);
    }
    return withSelectedSftp(
        isCancelled,
        [&arguments](SshClient& client, const CancelCallback& observe,
                     std::uint32_t startedAt, SftpTerminalState& state) {
            const std::uint32_t budget = remainingBudget(startedAt);
            if (budget == 0) {
                state = SftpTerminalState::TimedOut;
                return terminalResult(state, "before file write", "");
            }
            const SftpMutationResult written = client.writeSftpTextFileControlled(
                arguments.value.path, arguments.value.content,
                arguments.value.overwrite, budget, observe);
            observe();
            if (!written.success) {
                if (state != SftpTerminalState::None) {
                    const String detail = written.error.isEmpty()
                        ? String() : String("; ") + written.error;
                    return terminalResult(state, "during file write", detail);
                }
                return toolError(written.error);
            }
            JsonDocument document;
            document["ok"] = true;
            document["destination"] = arguments.value.path;
            document["bytes"] = arguments.value.content.size();
            document["overwrite"] = arguments.value.overwrite;
            std::string output;
            serializeJson(document, output);
            return ToolExecutionResult{true, output, ""};
        });
}

ToolExecutionResult executeSftpMoveTool(
    const ToolCall& call,
    const CancelCallback& isCancelled)
{
    if (call.name != "sftp_move") {
        return toolError("Unsupported SFTP move tool name");
    }
    const auto arguments = parseMoveArguments(call.arguments);
    if (!arguments.success) {
        return toolError(arguments.error);
    }
    return withSelectedSftp(
        isCancelled,
        [&arguments](SshClient& client, const CancelCallback& observe,
                     std::uint32_t startedAt, SftpTerminalState& state) {
            const std::uint32_t budget = remainingBudget(startedAt);
            if (budget == 0) {
                state = SftpTerminalState::TimedOut;
                return terminalResult(state, "before move", "");
            }
            const SftpMutationResult moved = client.moveSftpPathControlled(
                arguments.value.sourcePath, arguments.value.destinationPath,
                arguments.value.overwrite, budget, observe);
            observe();
            if (!moved.success) {
                if (state != SftpTerminalState::None) {
                    const String detail = moved.error.isEmpty()
                        ? String() : String("; ") + moved.error;
                    return terminalResult(state, "during move", detail);
                }
                return toolError(moved.error);
            }
            JsonDocument document;
            document["ok"] = true;
            document["source"] = arguments.value.sourcePath;
            document["destination"] = arguments.value.destinationPath;
            document["overwrite"] = arguments.value.overwrite;
            std::string output;
            serializeJson(document, output);
            return ToolExecutionResult{true, output, ""};
        });
}

}  // namespace cardputer
