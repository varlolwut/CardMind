#include "pending_tool_call.h"

#include "ssh_command_options.h"

#include "file_workspace.h"
#include "json_string_reader.h"
#include "pending_tool_preview.h"
#include "project_chat_storage.h"
#include "project_storage.h"
#include "sd_storage.h"
#include "sftp_tool.h"
#include "ssh_client.h"
#include "text_utils.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

namespace cardputer {
namespace {

constexpr std::uint32_t kPendingToolCallVersion = 2;
constexpr std::size_t kMaximumPendingToolCallBytes = 36864;
constexpr std::uint8_t kMaximumCompletedToolRounds = 4;
constexpr std::uint32_t kMaximumPriorToolOutputBytes = 32768;
String resumablePendingIdThisBoot;
static_assert(kMaximumPendingFilePreviewSourceBytes ==
              kMaximumWorkspaceToolChunkBytes);

struct CanonicalToolArgumentsResult {
    bool success;
    ToolSchemaId schema;
    std::string canonical;
    String fileName;
    bool overwrite;
    String error;
};

class CanonicalArgumentsReader {
public:
    explicit CanonicalArgumentsReader(const std::string& value)
        : value_(value), position_(0) {}

    int available() const
    {
        return position_ < value_.size() ? 1 : 0;
    }

    int read()
    {
        return available()
            ? static_cast<unsigned char>(value_[position_++]) : -1;
    }

    int peek() const
    {
        return available()
            ? static_cast<unsigned char>(value_[position_]) : -1;
    }

    std::size_t position() const
    {
        return position_;
    }

    bool seek(std::size_t position)
    {
        if (position > value_.size()) return false;
        position_ = position;
        return true;
    }

private:
    const std::string& value_;
    std::size_t position_;
};

json_reader::JsonStringValueResult readCanonicalStringArgument(
    const std::string& arguments,
    const char* field,
    std::size_t maximumBytes)
{
    CanonicalArgumentsReader reader(arguments);
    return json_reader::readObjectStringField(
        reader, field, 16, maximumBytes);
}

bool hasExactFields(const JsonObjectConst& object,
                    const char* const* fields,
                    std::size_t fieldCount)
{
    if (object.size() != fieldCount) {
        return false;
    }
    for (std::size_t index = 0; index < fieldCount; ++index) {
        if (!object.containsKey(fields[index])) {
            return false;
        }
    }
    return true;
}

bool isLowerHex(const std::string& value, std::size_t expectedBytes)
{
    if (value.size() != expectedBytes) {
        return false;
    }
    for (const char character : value) {
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

std::string sha256Hex(const std::uint8_t* digest)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string result(64, '0');
    for (std::size_t index = 0; index < 32; ++index) {
        result[index * 2] = kHex[digest[index] >> 4];
        result[index * 2 + 1] = kHex[digest[index] & 0x0F];
    }
    return result;
}

OperationResult hashOpenFile(File& file, std::string& hash)
{
    const std::uint64_t expectedBytes = file.size();
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    if (mbedtls_sha256_starts(&context, 0) != 0) {
        mbedtls_sha256_free(&context);
        return {false, "Failed to initialize pending file SHA-256"};
    }
    std::uint8_t buffer[1024] = {};
    std::uint64_t totalBytes = 0;
    std::size_t bytesSinceYield = 0;
    while (totalBytes < expectedBytes) {
        const std::size_t remaining = static_cast<std::size_t>(expectedBytes - totalBytes);
        const std::size_t readBytes = file.read(
            buffer, remaining < sizeof(buffer) ? remaining : sizeof(buffer));
        if (readBytes == 0 || readBytes > expectedBytes - totalBytes ||
            mbedtls_sha256_update(&context, buffer, readBytes) != 0) {
            mbedtls_sha256_free(&context);
            return {false, "Failed to hash the pending file target completely"};
        }
        totalBytes += readBytes;
        bytesSinceYield += readBytes;
        if (bytesSinceYield >= 64U * 1024U) {
            delay(0);
            bytesSinceYield = 0;
        }
    }
    std::uint8_t digest[32] = {};
    if (mbedtls_sha256_finish(&context, digest) != 0) {
        mbedtls_sha256_free(&context);
        return {false, "Failed to finish pending file SHA-256"};
    }
    mbedtls_sha256_free(&context);
    hash = sha256Hex(digest);
    return {true, ""};
}

bool updateLengthPrefixed(mbedtls_sha256_context& context,
                          const std::string& value)
{
    if (value.size() > UINT32_MAX) {
        return false;
    }
    const std::uint32_t length = static_cast<std::uint32_t>(value.size());
    const std::uint8_t prefix[4] = {
        static_cast<std::uint8_t>(length >> 24),
        static_cast<std::uint8_t>(length >> 16),
        static_cast<std::uint8_t>(length >> 8),
        static_cast<std::uint8_t>(length),
    };
    return mbedtls_sha256_update(&context, prefix, sizeof(prefix)) == 0 &&
        (value.empty() ||
         mbedtls_sha256_update(
             &context,
             reinterpret_cast<const std::uint8_t*>(value.data()),
             value.size()) == 0);
}

OperationResult selectedSshAuthorityIdentity(PendingToolTargetIdentity& target)
{
    SshAuthoritySummary authority = {};
    const OperationResult loaded = loadSelectedSshAuthority(authority);
    if (!loaded.success) {
        return loaded;
    }
    String trustedFingerprint;
    bool trusted = false;
    const OperationResult trustLoaded = loadTrustedSshFingerprint(
        authority.host, authority.port, trustedFingerprint, trusted);
    if (!trustLoaded.success || !trusted) {
        return {false, trustLoaded.success
            ? String("Selected SSH host key is not trusted")
            : trustLoaded.error};
    }
    const std::array<std::string, 8> fields = {{
        std::to_string(authority.profileId),
        authority.name.c_str(),
        authority.host.c_str(),
        std::to_string(authority.port),
        authority.username.c_str(),
        authority.authMode == SshAuthMode::Password ? "password" : "private_key",
        std::to_string(authority.privateKeyId),
        trustedFingerprint.c_str(),
    }};
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    bool valid = mbedtls_sha256_starts(&context, 0) == 0;
    for (const std::string& field : fields) {
        valid = valid && updateLengthPrefixed(context, field);
    }
    std::uint8_t digest[32] = {};
    valid = valid && mbedtls_sha256_finish(&context, digest) == 0;
    mbedtls_sha256_free(&context);
    if (!valid) {
        return {false, "Failed to hash selected SSH authority"};
    }
    target.kind = PendingToolTargetKind::Ssh;
    target.sha256 = sha256Hex(digest);
    return {true, ""};
}

CanonicalToolArgumentsResult failArguments(const String& error)
{
    return {false, ToolSchemaId::Count, "", "", false, error};
}

CanonicalToolArgumentsResult canonicalizeParsedToolArguments(
    const std::string& callId,
    const std::string& toolName,
    const JsonObjectConst& input)
{
    if (callId.empty() || callId.size() > kMaximumPendingToolCallIdBytes ||
        !isValidUtf8(callId)) {
        return failArguments("Pending tool call id must be valid UTF-8 between 1 and 256 bytes");
    }
    const ToolCatalogEntry* entry = toolCatalogEntryForName(toolName);
    if (entry == nullptr) {
        return failArguments("Pending tool name is not in the exact current catalog");
    }
    JsonDocument normalized;
    (void)normalized.to<JsonObject>();
    String fileName;
    bool overwrite = false;
    switch (entry->schema) {
        case ToolSchemaId::WebSearch: {
            static constexpr const char* kFields[] = {"query"};
            if (!hasExactFields(input, kFields, 1) ||
                !input["query"].is<const char*>()) {
                return failArguments("web_search requires exactly one string field: query");
            }
            const std::string query = input["query"].as<const char*>();
            if (query.empty() || query.size() > 400 || !isValidUtf8(query)) {
                return failArguments(
                    "web_search query must be valid UTF-8 between 1 and 400 bytes");
            }
            normalized["query"] = query;
            break;
        }
        case ToolSchemaId::WebFetch: {
            static constexpr const char* kFields[] = {"url"};
            if (!hasExactFields(input, kFields, 1) ||
                !input["url"].is<const char*>()) {
                return failArguments("web_fetch requires exactly one string field: url");
            }
            const std::string url = input["url"].as<const char*>();
            if (url.empty() || url.size() > 1200 ||
                url.compare(0, 8, "https://") != 0 || !isValidUtf8(url)) {
                return failArguments(
                    "web_fetch url must be valid UTF-8 HTTPS text of at most 1200 bytes");
            }
            normalized["url"] = url;
            break;
        }
        case ToolSchemaId::ListFiles: {
            static constexpr const char* kFields[] = {"offset"};
            if (input.size() > 1 ||
                (input.size() == 1 &&
                 (!hasExactFields(input, kFields, 1) ||
                  !input["offset"].is<std::uint32_t>()))) {
                return failArguments("list_files accepts only optional uint32 field 'offset'");
            }
            if (input.size() == 1) {
                normalized["offset"] = input["offset"].as<std::uint32_t>();
            }
            break;
        }
        case ToolSchemaId::ReadFile: {
            static constexpr const char* kFields[] = {"name", "offset", "max_bytes"};
            if (!hasExactFields(input, kFields, 3) ||
                !input["name"].is<const char*>() ||
                !input["offset"].is<std::uint32_t>() ||
                !input["max_bytes"].is<std::uint32_t>()) {
                return failArguments(
                    "read_file requires exact fields name, offset, and max_bytes");
            }
            fileName = input["name"].as<const char*>();
            const std::uint32_t maximumBytes = input["max_bytes"].as<std::uint32_t>();
            if (!isValidWorkspaceFilename(fileName.c_str()) ||
                !isWorkspaceTextFile(fileName.c_str()) || maximumBytes == 0 ||
                maximumBytes > kMaximumWorkspaceToolChunkBytes) {
                return failArguments("read_file arguments are outside the current tool limits");
            }
            normalized["name"] = fileName;
            normalized["offset"] = input["offset"].as<std::uint32_t>();
            normalized["max_bytes"] = maximumBytes;
            break;
        }
        case ToolSchemaId::WriteFile:
        case ToolSchemaId::AppendFile: {
            static constexpr const char* kFields[] = {"name", "content"};
            if (!hasExactFields(input, kFields, 2) ||
                !input["name"].is<const char*>() ||
                !input["content"].is<const char*>()) {
                return failArguments(
                    "File mutation requires exactly string fields name and content");
            }
            fileName = input["name"].as<const char*>();
            const JsonString content = input["content"].as<JsonString>();
            if (!isValidWorkspaceFilename(fileName.c_str()) ||
                !isWorkspaceTextFile(fileName.c_str()) ||
                content.size() > kMaximumWorkspaceToolChunkBytes ||
                !isValidUtf8(content.c_str(), content.size())) {
                return failArguments("File mutation arguments are outside the current tool limits");
            }
            normalized["name"] = fileName;
            normalized["content"] = JsonString(
                content.c_str(), content.size(), JsonString::Linked);
            break;
        }
        case ToolSchemaId::SshCommand: {
            if (input.size() < 1 || input.size() > 3 ||
                !input["command"].is<const char*>()) {
                return failArguments(
                    "ssh_command requires command and only its optional timeout/output fields");
            }
            for (JsonPairConst field : input) {
                const char* name = field.key().c_str();
                if (std::strcmp(name, "command") != 0 &&
                    std::strcmp(name, "timeout_ms") != 0 &&
                    std::strcmp(name, "max_inline_output_bytes") != 0) {
                    return failArguments("ssh_command contains an unknown field");
                }
            }
            const std::string command = input["command"].as<const char*>();
            if (command.empty() || command.size() > 1024 || !isValidUtf8(command)) {
                return failArguments(
                    "SSH command must be valid UTF-8 between 1 and 1024 bytes");
            }
            const bool hasTimeout = input.containsKey("timeout_ms");
            const bool hasOutputLimit = input.containsKey("max_inline_output_bytes");
            if ((hasTimeout && !input["timeout_ms"].is<std::uint32_t>()) ||
                (hasOutputLimit &&
                 !input["max_inline_output_bytes"].is<std::size_t>())) {
                return failArguments("SSH command timeout/output options must be integers");
            }
            const std::uint32_t timeoutMs = hasTimeout
                ? input["timeout_ms"].as<std::uint32_t>()
                : kDefaultSshCommandTimeoutMs;
            const std::size_t maximumOutputBytes = hasOutputLimit
                ? input["max_inline_output_bytes"].as<std::size_t>()
                : kDefaultSshCommandInlineOutputBytes;
            if (!isValidSshCommandTimeout(timeoutMs) ||
                !isValidSshCommandInlineOutputLimit(maximumOutputBytes)) {
                return failArguments("SSH command timeout/output options are outside current limits");
            }
            normalized["command"] = command;
            normalized["timeout_ms"] = timeoutMs;
            normalized["max_inline_output_bytes"] = maximumOutputBytes;
            break;
        }
        case ToolSchemaId::SftpList: {
            static constexpr const char* kFields[] = {
                "path", "offset", "max_entries",
            };
            if (!hasExactFields(input, kFields, 3) ||
                !input["path"].is<const char*>() ||
                !input["offset"].is<std::uint32_t>() ||
                !input["max_entries"].is<std::size_t>()) {
                return failArguments(
                    "sftp_list requires exact path, offset and max_entries fields");
            }
            const JsonString path = input["path"].as<JsonString>();
            const std::size_t maximumEntries =
                input["max_entries"].as<std::size_t>();
            if (path.isNull() ||
                !isValidModelSftpPath(path.c_str(), path.size()) ||
                maximumEntries == 0 ||
                maximumEntries > kMaximumModelSftpPageEntries) {
                return failArguments("sftp_list arguments are outside current limits");
            }
            fileName = path.c_str();
            normalized["path"] = fileName;
            normalized["offset"] = input["offset"].as<std::uint32_t>();
            normalized["max_entries"] = maximumEntries;
            break;
        }
        case ToolSchemaId::SftpRead: {
            static constexpr const char* kFields[] = {
                "path", "offset", "max_bytes",
            };
            if (!hasExactFields(input, kFields, 3) ||
                !input["path"].is<const char*>() ||
                !input["offset"].is<std::uint64_t>() ||
                !input["max_bytes"].is<std::size_t>()) {
                return failArguments(
                    "sftp_read requires exact path, offset and max_bytes fields");
            }
            const JsonString path = input["path"].as<JsonString>();
            const std::size_t maximumBytes =
                input["max_bytes"].as<std::size_t>();
            if (path.isNull() ||
                !isValidModelSftpPath(path.c_str(), path.size()) ||
                maximumBytes < kMinimumModelSftpReadBytes ||
                maximumBytes > kMaximumModelSftpChunkBytes) {
                return failArguments("sftp_read arguments are outside current limits");
            }
            fileName = path.c_str();
            normalized["path"] = fileName;
            normalized["offset"] = input["offset"].as<std::uint64_t>();
            normalized["max_bytes"] = maximumBytes;
            break;
        }
        case ToolSchemaId::SftpWrite: {
            const bool hasOverwrite = input.containsKey("overwrite");
            static constexpr const char* kRequired[] = {"path", "content"};
            static constexpr const char* kWithOverwrite[] = {
                "path", "content", "overwrite",
            };
            if (!(hasOverwrite
                      ? hasExactFields(input, kWithOverwrite, 3)
                      : hasExactFields(input, kRequired, 2)) ||
                !input["path"].is<const char*>() ||
                !input["content"].is<const char*>() ||
                (hasOverwrite && !input["overwrite"].is<bool>())) {
                return failArguments(
                    "sftp_write requires path, content and optional boolean overwrite");
            }
            const JsonString path = input["path"].as<JsonString>();
            const JsonString content = input["content"].as<JsonString>();
            if (path.isNull() || content.isNull() ||
                !isValidModelSftpPath(path.c_str(), path.size()) ||
                content.size() > kMaximumModelSftpChunkBytes ||
                !isValidModelSftpText(content.c_str(), content.size())) {
                return failArguments("sftp_write arguments are outside current limits");
            }
            fileName = path.c_str();
            overwrite = hasOverwrite && input["overwrite"].as<bool>();
            normalized["path"] = fileName;
            normalized["content"] = JsonString(
                content.c_str(), content.size(), JsonString::Linked);
            normalized["overwrite"] = overwrite;
            break;
        }
        case ToolSchemaId::SftpMove: {
            const bool hasOverwrite = input.containsKey("overwrite");
            static constexpr const char* kRequired[] = {
                "source_path", "destination_path",
            };
            static constexpr const char* kWithOverwrite[] = {
                "source_path", "destination_path", "overwrite",
            };
            if (!(hasOverwrite
                      ? hasExactFields(input, kWithOverwrite, 3)
                      : hasExactFields(input, kRequired, 2)) ||
                !input["source_path"].is<const char*>() ||
                !input["destination_path"].is<const char*>() ||
                (hasOverwrite && !input["overwrite"].is<bool>())) {
                return failArguments(
                    "sftp_move requires source/destination and optional boolean overwrite");
            }
            const JsonString source = input["source_path"].as<JsonString>();
            const JsonString destination =
                input["destination_path"].as<JsonString>();
            if (source.isNull() || destination.isNull() ||
                !isValidModelSftpPath(source.c_str(), source.size()) ||
                !isValidModelSftpPath(
                    destination.c_str(), destination.size()) ||
                source.size() == destination.size() &&
                    std::memcmp(source.c_str(), destination.c_str(),
                                source.size()) == 0) {
                return failArguments("sftp_move paths are invalid or identical");
            }
            fileName = destination.c_str();
            overwrite = hasOverwrite && input["overwrite"].as<bool>();
            normalized["source_path"] = JsonString(
                source.c_str(), source.size(), JsonString::Linked);
            normalized["destination_path"] = fileName;
            normalized["overwrite"] = overwrite;
            break;
        }
        case ToolSchemaId::Count:
            return failArguments("Pending tool schema is invalid");
    }
    if (normalized.overflowed()) {
        return failArguments("Pending tool argument normalization exceeded available memory");
    }
    std::string canonical;
    canonical.reserve(measureJson(normalized));
    serializeJson(normalized, canonical);
    if (canonical.empty() || canonical.size() > kMaximumPendingToolArgumentsBytes) {
        return failArguments("Canonical pending tool arguments exceed 32768 bytes");
    }
    return {
        true, entry->schema, std::move(canonical), fileName, overwrite, "",
    };
}

CanonicalToolArgumentsResult canonicalizeToolArguments(const ToolCall& call)
{
    if (call.arguments.empty() ||
        call.arguments.size() > kMaximumPendingToolArgumentsBytes ||
        !isValidUtf8(call.arguments)) {
        return failArguments(
            "Pending tool arguments must be a valid UTF-8 JSON object of at most 32768 bytes");
    }
    JsonDocument source;
    const DeserializationError parsed = deserializeJson(source, call.arguments);
    if (parsed || !source.is<JsonObject>()) {
        return failArguments(String("Pending tool arguments must be a JSON object: ") +
                             parsed.c_str());
    }
    return canonicalizeParsedToolArguments(
        call.id, call.name, source.as<JsonObjectConst>());
}

OperationResult validateProjectTarget(const String& projectId,
                                      ToolSchemaId schema,
                                      const String& fileName,
                                      PendingToolTargetIdentity& target)
{
    if (schema != ToolSchemaId::ReadFile && schema != ToolSchemaId::WriteFile &&
        schema != ToolSchemaId::AppendFile) {
        return {true, ""};
    }
    const SharedFileLinkResult link = projectHasSharedFileLink(projectId, fileName);
    if (!link.success) {
        return {false, link.error};
    }
    const String path = workspaceFilePath(fileName);
    const bool exists = SD.exists(path);
    if (schema != ToolSchemaId::WriteFile && (!link.linked || !exists)) {
        return {false, "Shared file is not linked to the active project: " + fileName};
    }
    if (schema == ToolSchemaId::WriteFile && exists && !link.linked) {
        return {false, "Existing Shared file is not linked to the active project: " + fileName};
    }
    if (schema == ToolSchemaId::ReadFile) {
        return {true, ""};
    }
    target.kind = PendingToolTargetKind::File;
    target.name = fileName;
    target.exists = exists;
    if (!exists) {
        return {true, ""};
    }
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return {false, "Failed to open pending file target: " + fileName};
    }
    target.size = file.size();
    OperationResult result = hashOpenFile(file, target.sha256);
    file.close();
    return result;
}

String generatePendingId()
{
    char value[17] = {};
    std::snprintf(value, sizeof(value), "%08lx%08lx",
                  static_cast<unsigned long>(esp_random()),
                  static_cast<unsigned long>(esp_random()));
    return String(value);
}

const char* reasonName(PendingToolConfirmationReason reason)
{
    return reason == PendingToolConfirmationReason::PolicyAsk
        ? "policy_ask" : "mandatory";
}

const char* stateName(PendingToolCallState state)
{
    switch (state) {
        case PendingToolCallState::Awaiting: return "awaiting";
        case PendingToolCallState::ClaimedApprove: return "claimed_approve";
        case PendingToolCallState::Denied: return "denied";
    }
    return "invalid";
}

const char* targetKindName(PendingToolTargetKind kind)
{
    switch (kind) {
        case PendingToolTargetKind::None: return "none";
        case PendingToolTargetKind::File: return "file";
        case PendingToolTargetKind::Ssh: return "ssh";
    }
    return "invalid";
}

PendingToolCallResult corrupted(const String& error)
{
    return {false, true, {}, "Pending tool call is corrupt: " + error};
}

void serializePendingToolCall(const PendingToolCall& pending, JsonDocument& document)
{
    const EncodedToolMessageIntent encoded = encodeToolMessageIntent(
        pending.continuation.intent);
    document["version"] = kPendingToolCallVersion;
    document["pending_id"] = pending.pendingId;
    document["project_id"] = pending.projectId;
    document["chat_id"] = pending.chatId;
    document["project_revision"] = pending.projectRevision;
    document["chat_revision"] = pending.chatRevision;
    document["chat_message_count"] = pending.chatMessageCount;
    document["state"] = stateName(pending.state);
    document["intent"] = std::string(encoded.value.data(), encoded.length);
    document["reason"] = reasonName(pending.reason);
    JsonObject call = document["call"].to<JsonObject>();
    call["id"] = pending.continuation.call.id;
    call["name"] = pending.continuation.call.name;
    call["arguments"] = serialized(
        pending.continuation.call.arguments.data(),
        pending.continuation.call.arguments.size());
    document["remaining_required_groups"] =
        pending.continuation.remainingRequiredGroupsAfterCall;
    document["completed_tool_rounds_before_call"] =
        pending.continuation.completedToolRoundsBeforeCall;
    document["tool_output_bytes_before_call"] =
        pending.continuation.toolOutputBytesBeforeCall;
    document["completed_workspace_write_before_call"] =
        pending.continuation.completedWorkspaceWriteBeforeCall;
    JsonObject target = document["target"].to<JsonObject>();
    target["kind"] = targetKindName(pending.target.kind);
    if (pending.target.kind == PendingToolTargetKind::File) {
        target["name"] = pending.target.name;
        target["exists"] = pending.target.exists;
        if (pending.target.exists) {
            target["size"] = pending.target.size;
            target["sha256"] = pending.target.sha256;
        }
    } else if (pending.target.kind == PendingToolTargetKind::Ssh) {
        target["authority_sha256"] = pending.target.sha256;
        if (!pending.target.name.isEmpty()) {
            target["name"] = pending.target.name;
        }
    }
}

PendingToolCallResult storePendingToolCall(PendingToolCall pending)
{
    const String expectedPendingId = pending.pendingId;
    const PendingToolCallState expectedState = pending.state;
    OperationResult result = {false, "Pending tool call was not written"};
    {
        JsonDocument document;
        serializePendingToolCall(pending, document);
        if (document.overflowed()) {
            return {false, false, {},
                    "Pending tool call serialization exceeded available memory"};
        }
        if (measureJson(document) > kMaximumPendingToolCallBytes) {
            return {false, false, {},
                    "Serialized pending tool call exceeds 36864 bytes"};
        }
        result = writeAtomicJsonSdFile(kPendingToolCallPath, document);
    }
    if (!result.success) {
        return {false, false, {}, result.error};
    }
    {
        std::string releasedArguments;
        pending.continuation.call.arguments.swap(releasedArguments);
    }
    PendingToolCallResult verified = loadPendingToolCall();
    if (!verified.success || !verified.found ||
        verified.pending.pendingId != expectedPendingId ||
        verified.pending.state != expectedState) {
        return {false, verified.found, {}, verified.success
            ? String("Stored pending tool call identity did not round-trip")
            : verified.error};
    }
    return verified;
}

OperationResult validateCurrentProjectChatIdentity(const PendingToolCall& pending)
{
    const ProjectDocumentResult project = loadProject(pending.projectId);
    if (!project.success) {
        return {false, project.error};
    }
    const ChatDocumentResult chat = loadProjectChatMetadata(
        pending.projectId, pending.chatId);
    if (!chat.success) {
        return {false, chat.error};
    }
    if (chat.chat.projectId != pending.projectId ||
        project.project.summary.revision != pending.projectRevision ||
        chat.chat.summary.revision != pending.chatRevision ||
        chat.chat.summary.messageCount != pending.chatMessageCount) {
        return {false, "Pending tool call project or chat identity is stale"};
    }
    return {true, ""};
}

bool sameTargetIdentity(const PendingToolTargetIdentity& left,
                        const PendingToolTargetIdentity& right)
{
    return left.kind == right.kind && left.name == right.name &&
        left.exists == right.exists && left.size == right.size &&
        left.sha256 == right.sha256;
}

struct CurrentPendingIdentityResult {
    bool success;
    PendingToolTargetIdentity target;
    String error;
};

CurrentPendingIdentityResult validateCurrentPendingIdentity(
    const PendingToolCall& pending,
    ToolSchemaId schema,
    const String& fileName)
{
    OperationResult identity = validateCurrentProjectChatIdentity(pending);
    if (!identity.success) {
        return {false, {}, identity.error};
    }
    PendingToolTargetIdentity currentTarget;
    identity = validateProjectTarget(
        pending.projectId, schema, fileName, currentTarget);
    if (identity.success &&
        (schema == ToolSchemaId::SshCommand || isModelSftpSchema(schema))) {
        identity = selectedSshAuthorityIdentity(currentTarget);
        if (identity.success && isModelSftpSchema(schema)) {
            currentTarget.name = fileName;
        }
    }
    if (!identity.success) {
        return {false, {}, identity.error};
    }
    if (!sameTargetIdentity(pending.target, currentTarget)) {
        return {false, {}, "Pending tool target identity is stale"};
    }
    return {true, std::move(currentTarget), ""};
}

struct PendingToolCallBuildResult {
    bool success;
    PendingToolCall pending;
    String error;
};

PendingToolCallBuildResult buildPendingToolCall(
    const ToolRequestPlan& plan,
    const String& projectId,
    const String& chatId,
    const PendingToolContinuation& continuation)
{
    if (!toolRequestPlanIsConsistent(plan) ||
        continuation.intent.mode != plan.intent.mode ||
        continuation.intent.requiredGroups != plan.intent.requiredGroups) {
        return {false, {}, "Pending tool call requires the exact consistent request plan"};
    }
    if ((continuation.remainingRequiredGroupsAfterCall &
         static_cast<std::uint8_t>(~continuation.intent.requiredGroups)) != 0 ||
        continuation.completedToolRoundsBeforeCall >= kMaximumCompletedToolRounds ||
        continuation.toolOutputBytesBeforeCall > kMaximumPriorToolOutputBytes) {
        return {false, {}, "Pending tool continuation counters are outside request limits"};
    }
    CanonicalToolArgumentsResult arguments =
        canonicalizeToolArguments(continuation.call);
    if (!arguments.success) {
        return {false, {}, arguments.error};
    }
    if (!toolRequestPlanIncludesSchema(plan, arguments.schema)) {
        return {false, {}, "Pending tool schema is absent from the exact request plan"};
    }
    const std::uint8_t groupBit = static_cast<std::uint8_t>(
        1U << static_cast<std::uint8_t>(
            toolCatalog()[static_cast<std::size_t>(arguments.schema)].group));
    if ((continuation.remainingRequiredGroupsAfterCall & groupBit) != 0) {
        return {false, {}, "Pending continuation did not observe its exact required group"};
    }
    PendingToolTargetIdentity target;
    OperationResult result = validateProjectTarget(
        projectId, arguments.schema, arguments.fileName, target);
    if (!result.success) {
        return {false, {}, result.error};
    }
    if (arguments.schema == ToolSchemaId::SshCommand ||
        isModelSftpSchema(arguments.schema)) {
        result = selectedSshAuthorityIdentity(target);
        if (!result.success) {
            return {false, {}, result.error};
        }
        if (isModelSftpSchema(arguments.schema)) {
            target.name = arguments.fileName;
        }
    }
    const ToolPermissionDecision decision = toolRequestPlanDecision(plan, arguments.schema);
    const bool mandatory = arguments.schema == ToolSchemaId::SshCommand ||
        (arguments.schema == ToolSchemaId::WriteFile && target.exists) ||
        ((arguments.schema == ToolSchemaId::SftpWrite ||
          arguments.schema == ToolSchemaId::SftpMove) && arguments.overwrite);
    if (decision != ToolPermissionDecision::Ask &&
        !(decision == ToolPermissionDecision::Allow && mandatory)) {
        return {false, {},
                "Tool call does not require confirmation under the exact request plan"};
    }
    const ProjectDocumentResult project = loadProject(projectId);
    if (!project.success) {
        return {false, {}, project.error};
    }
    const ChatDocumentResult chat = loadProjectChatMetadata(projectId, chatId);
    if (!chat.success) {
        return {false, {}, chat.error};
    }
    if (chat.chat.projectId != projectId) {
        return {false, {}, "Pending chat does not belong to the active project"};
    }
    const EncodedToolMessageIntent encoded = encodeToolMessageIntent(continuation.intent);
    if (encoded.error != ToolMessageIntentCodecError::None) {
        return {false, {}, "Pending tool intent is invalid"};
    }
    PendingToolContinuation storedContinuation = {
        {continuation.call.id, continuation.call.name, std::move(arguments.canonical)},
        continuation.intent,
        continuation.remainingRequiredGroupsAfterCall,
        continuation.completedToolRoundsBeforeCall,
        continuation.toolOutputBytesBeforeCall,
        continuation.completedWorkspaceWriteBeforeCall,
    };
    PendingToolCall pending = {
        generatePendingId(),
        projectId,
        chatId,
        project.project.summary.revision,
        chat.chat.summary.revision,
        chat.chat.summary.messageCount,
        PendingToolCallState::Awaiting,
        mandatory
            ? PendingToolConfirmationReason::Mandatory
            : PendingToolConfirmationReason::PolicyAsk,
        std::move(storedContinuation),
        target,
    };
    return {true, std::move(pending), ""};
}

}  // namespace

bool pendingToolCallIsResumableThisBoot(const String& pendingId)
{
    return !pendingId.isEmpty() && pendingId == resumablePendingIdThisBoot;
}

OperationResult savePendingToolCall(
    const ToolRequestPlan& plan,
    const String& projectId,
    const String& chatId,
    const PendingToolContinuation& continuation)
{
    const PendingToolCallResult existing = loadPendingToolCall();
    if (!existing.success) {
        return {false, existing.error};
    }
    if (existing.found) {
        return {false, "Another pending tool call already exists"};
    }
    PendingToolCallBuildResult built = buildPendingToolCall(
        plan, projectId, chatId, continuation);
    if (!built.success) {
        return {false, built.error};
    }
    const PendingToolCallResult stored = storePendingToolCall(std::move(built.pending));
    if (stored.success) {
        resumablePendingIdThisBoot = stored.pending.pendingId;
    }
    return {stored.success, stored.error};
}

OperationResult replaceTerminalPendingToolCall(
    const String& expectedPendingId,
    PendingToolCallState expectedState,
    const ToolRequestPlan& plan,
    const String& projectId,
    const String& chatId,
    const PendingToolContinuation& continuation)
{
    if (expectedState == PendingToolCallState::Awaiting) {
        return {false, "Pending replacement requires an exact terminal state"};
    }
    if (!pendingToolCallIsResumableThisBoot(expectedPendingId)) {
        return {false, "Pending tool request was interrupted by reboot and cannot continue"};
    }
    const PendingToolCallResult existing = loadPendingToolCall();
    if (!existing.success) {
        return {false, existing.error};
    }
    if (!existing.found || existing.pending.pendingId != expectedPendingId ||
        existing.pending.state != expectedState ||
        existing.pending.projectId != projectId ||
        existing.pending.chatId != chatId) {
        return {false, "Pending replacement owner does not match"};
    }
    PendingToolCallBuildResult built = buildPendingToolCall(
        plan, projectId, chatId, continuation);
    if (!built.success) {
        return {false, built.error};
    }
    const PendingToolCallResult stored = storePendingToolCall(std::move(built.pending));
    if (stored.success) {
        resumablePendingIdThisBoot = stored.pending.pendingId;
    }
    return {stored.success, stored.error};
}

PendingToolCallResult loadPendingToolCall()
{
    const OperationResult recovered = recoverAtomicSdFile(kPendingToolCallPath);
    if (!recovered.success) {
        return {false, false, {}, recovered.error};
    }
    if (!SD.exists(kPendingToolCallPath)) {
        return {true, false, {}, ""};
    }
    const OperationResult access = requireSdReadAccess();
    if (!access.success) {
        return {false, true, {}, access.error};
    }
    File file = SD.open(kPendingToolCallPath, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return corrupted("file cannot be opened");
    }
    if (file.size() == 0 || file.size() > kMaximumPendingToolCallBytes) {
        file.close();
        return corrupted("file size is outside the fixed bound");
    }
    JsonDocument document;
    const DeserializationError parsed = deserializeJson(document, file);
    const bool exactEnd = file.position() == file.size();
    file.close();
    if (parsed || !exactEnd || !document.is<JsonObject>()) {
        return corrupted(String("file is not one complete JSON object: ") + parsed.c_str());
    }
    const JsonObjectConst root = document.as<JsonObjectConst>();
    static constexpr const char* kRootFields[] = {
        "version", "pending_id", "project_id", "chat_id", "project_revision",
        "chat_revision", "chat_message_count", "state", "intent", "reason", "call",
        "remaining_required_groups", "completed_tool_rounds_before_call",
        "tool_output_bytes_before_call", "completed_workspace_write_before_call", "target",
    };
    if (!hasExactFields(root, kRootFields, 16) ||
        !root["version"].is<std::uint32_t>() ||
        root["version"].as<std::uint32_t>() != kPendingToolCallVersion ||
        !root["pending_id"].is<const char*>() ||
        !root["project_id"].is<const char*>() || !root["chat_id"].is<const char*>() ||
        !root["project_revision"].is<std::uint32_t>() ||
        !root["chat_revision"].is<std::uint32_t>() ||
        !root["chat_message_count"].is<std::uint32_t>() || !root["state"].is<const char*>() ||
        !root["intent"].is<const char*>() || !root["reason"].is<const char*>() ||
        !root["remaining_required_groups"].is<std::uint8_t>() ||
        !root["completed_tool_rounds_before_call"].is<std::uint8_t>() ||
        !root["tool_output_bytes_before_call"].is<std::uint32_t>() ||
        !root["completed_workspace_write_before_call"].is<bool>() ||
        !root["call"].is<JsonObjectConst>() || !root["target"].is<JsonObjectConst>()) {
        return corrupted("top-level fields or types are invalid");
    }
    PendingToolCall pending = {};
    pending.pendingId = root["pending_id"].as<const char*>();
    pending.projectId = root["project_id"].as<const char*>();
    pending.chatId = root["chat_id"].as<const char*>();
    if (!isLowerHex(pending.pendingId.c_str(), 16) ||
        !isValidChatId(pending.projectId.c_str()) ||
        !isValidChatId(pending.chatId.c_str())) {
        return corrupted("identifiers are invalid");
    }
    pending.projectRevision = root["project_revision"].as<std::uint32_t>();
    pending.chatRevision = root["chat_revision"].as<std::uint32_t>();
    pending.chatMessageCount = root["chat_message_count"].as<std::uint32_t>();
    const String state = root["state"].as<const char*>();
    if (state == "awaiting") {
        pending.state = PendingToolCallState::Awaiting;
    } else if (state == "claimed_approve") {
        pending.state = PendingToolCallState::ClaimedApprove;
    } else if (state == "denied") {
        pending.state = PendingToolCallState::Denied;
    } else {
        return corrupted("state is invalid");
    }
    const char* intentText = root["intent"].as<const char*>();
    const DecodedToolMessageIntent decoded = decodeToolMessageIntent(
        intentText, std::strlen(intentText));
    if (decoded.error != ToolMessageIntentCodecError::None) {
        return corrupted("intent is invalid");
    }
    pending.continuation.intent = decoded.intent;
    const String reason = root["reason"].as<const char*>();
    if (reason == "policy_ask") {
        pending.reason = PendingToolConfirmationReason::PolicyAsk;
    } else if (reason == "mandatory") {
        pending.reason = PendingToolConfirmationReason::Mandatory;
    } else {
        return corrupted("confirmation reason is invalid");
    }
    const JsonObjectConst call = root["call"].as<JsonObjectConst>();
    static constexpr const char* kCallFields[] = {"id", "name", "arguments"};
    if (!hasExactFields(call, kCallFields, 3) || !call["id"].is<const char*>() ||
        !call["name"].is<const char*>() || !call["arguments"].is<JsonObjectConst>()) {
        return corrupted("call fields are invalid");
    }
    pending.continuation.call.id = call["id"].as<const char*>();
    pending.continuation.call.name = call["name"].as<const char*>();
    const JsonObjectConst storedArguments = call["arguments"].as<JsonObjectConst>();
    CanonicalToolArgumentsResult arguments = canonicalizeParsedToolArguments(
        pending.continuation.call.id,
        pending.continuation.call.name,
        storedArguments);
    if (!arguments.success) {
        return corrupted(arguments.error);
    }
    {
        std::string storedCanonical;
        storedCanonical.reserve(measureJson(storedArguments));
        serializeJson(storedArguments, storedCanonical);
        if (arguments.canonical != storedCanonical) {
            return corrupted("call arguments are not canonical");
        }
    }
    pending.continuation.call.arguments = std::move(arguments.canonical);
    pending.continuation.remainingRequiredGroupsAfterCall =
        root["remaining_required_groups"].as<std::uint8_t>();
    pending.continuation.completedToolRoundsBeforeCall =
        root["completed_tool_rounds_before_call"].as<std::uint8_t>();
    pending.continuation.toolOutputBytesBeforeCall =
        root["tool_output_bytes_before_call"].as<std::uint32_t>();
    pending.continuation.completedWorkspaceWriteBeforeCall =
        root["completed_workspace_write_before_call"].as<bool>();
    const std::uint8_t groupBit = static_cast<std::uint8_t>(
        1U << static_cast<std::uint8_t>(
            toolCatalog()[static_cast<std::size_t>(arguments.schema)].group));
    if ((pending.continuation.remainingRequiredGroupsAfterCall &
         static_cast<std::uint8_t>(~pending.continuation.intent.requiredGroups)) != 0 ||
        (pending.continuation.remainingRequiredGroupsAfterCall & groupBit) != 0 ||
        pending.continuation.completedToolRoundsBeforeCall >= kMaximumCompletedToolRounds ||
        pending.continuation.toolOutputBytesBeforeCall > kMaximumPriorToolOutputBytes) {
        return corrupted("continuation counters are invalid");
    }
    const JsonObjectConst target = root["target"].as<JsonObjectConst>();
    const char* kind = target["kind"].as<const char*>();
    if (kind == nullptr) {
        return corrupted("target kind is missing");
    }
    if (std::strcmp(kind, "none") == 0) {
        static constexpr const char* kFields[] = {"kind"};
        if (!hasExactFields(target, kFields, 1)) {
            return corrupted("none target fields are invalid");
        }
    } else if (std::strcmp(kind, "file") == 0) {
        pending.target.kind = PendingToolTargetKind::File;
        static constexpr const char* kAbsentFields[] = {"kind", "name", "exists"};
        static constexpr const char* kPresentFields[] = {
            "kind", "name", "exists", "size", "sha256"};
        if (!target["name"].is<const char*>() || !target["exists"].is<bool>()) {
            return corrupted("file target fields are invalid");
        }
        pending.target.name = target["name"].as<const char*>();
        pending.target.exists = target["exists"].as<bool>();
        if (pending.target.exists) {
            if (!hasExactFields(target, kPresentFields, 5) ||
                !target["size"].is<std::uint64_t>() ||
                !target["sha256"].is<const char*>()) {
                return corrupted("present file target fields are invalid");
            }
            pending.target.size = target["size"].as<std::uint64_t>();
            pending.target.sha256 = target["sha256"].as<const char*>();
            if (!isLowerHex(pending.target.sha256, 64)) {
                return corrupted("file target SHA-256 is invalid");
            }
        } else if (!hasExactFields(target, kAbsentFields, 3)) {
            return corrupted("absent file target fields are invalid");
        }
    } else if (std::strcmp(kind, "ssh") == 0) {
        static constexpr const char* kSshFields[] = {
            "kind", "authority_sha256",
        };
        static constexpr const char* kSftpFields[] = {
            "kind", "authority_sha256", "name",
        };
        const bool sftpTarget = isModelSftpSchema(arguments.schema);
        if (!(sftpTarget
                  ? hasExactFields(target, kSftpFields, 3)
                  : hasExactFields(target, kSshFields, 2)) ||
            !target["authority_sha256"].is<const char*>()) {
            return corrupted("SSH target fields are invalid");
        }
        pending.target.kind = PendingToolTargetKind::Ssh;
        pending.target.sha256 = target["authority_sha256"].as<const char*>();
        if (sftpTarget) {
            if (!target["name"].is<const char*>()) {
                return corrupted("SFTP target name is invalid");
            }
            pending.target.name = target["name"].as<const char*>();
        }
        if (!isLowerHex(pending.target.sha256, 64)) {
            return corrupted("SSH authority SHA-256 is invalid");
        }
    } else {
        return corrupted("target kind is invalid");
    }
    const bool fileMutation = arguments.schema == ToolSchemaId::WriteFile ||
        arguments.schema == ToolSchemaId::AppendFile;
    const bool mandatory = arguments.schema == ToolSchemaId::SshCommand ||
        (arguments.schema == ToolSchemaId::WriteFile && pending.target.exists) ||
        ((arguments.schema == ToolSchemaId::SftpWrite ||
          arguments.schema == ToolSchemaId::SftpMove) && arguments.overwrite);
    const bool sftpTarget = isModelSftpSchema(arguments.schema);
    if ((fileMutation &&
         (pending.target.kind != PendingToolTargetKind::File ||
          pending.target.name != arguments.fileName)) ||
        (!fileMutation && arguments.schema != ToolSchemaId::SshCommand &&
         !sftpTarget &&
         pending.target.kind != PendingToolTargetKind::None) ||
        (arguments.schema == ToolSchemaId::AppendFile && !pending.target.exists) ||
        (arguments.schema == ToolSchemaId::SshCommand &&
         pending.target.kind != PendingToolTargetKind::Ssh) ||
        (sftpTarget &&
         (pending.target.kind != PendingToolTargetKind::Ssh ||
          pending.target.name != arguments.fileName)) ||
        ((pending.reason == PendingToolConfirmationReason::Mandatory) != mandatory)) {
        return corrupted("target identity does not match the exact call and reason");
    }
    return {true, true, std::move(pending), ""};
}

PendingToolPreviewResult loadPendingToolPreview(const String& pendingId)
{
    PendingToolCallResult loaded = loadPendingToolCall();
    if (!loaded.success) {
        return {false, {}, loaded.error};
    }
    if (!loaded.found) {
        return {false, {}, "Pending tool call does not exist"};
    }
    PendingToolCall& pending = loaded.pending;
    if (pending.pendingId != pendingId) {
        return {false, {}, "Pending tool call id does not match"};
    }
    if (pending.state != PendingToolCallState::Awaiting) {
        return {false, {}, "Pending tool call is not awaiting a decision"};
    }
    const ToolCatalogEntry* entry = toolCatalogEntryForName(
        pending.continuation.call.name);
    if (entry == nullptr) {
        return {false, {}, "Pending tool name is not in the exact current catalog"};
    }
    String fileName;
    if (pending.target.kind == PendingToolTargetKind::File) {
        fileName = pending.target.name;
    } else if (isModelSftpSchema(entry->schema)) {
        fileName = pending.target.name;
    } else if (entry->schema == ToolSchemaId::ReadFile) {
        const json_reader::JsonStringValueResult decodedName =
            readCanonicalStringArgument(
                pending.continuation.call.arguments, "name", 512);
        if (!decodedName.success ||
            !isValidWorkspaceFilename(decodedName.value)) {
            return {false, {}, decodedName.success
                ? String("Pending read target name is invalid")
                : decodedName.error};
        }
        fileName = decodedName.value.c_str();
    }
    const CurrentPendingIdentityResult identity =
        validateCurrentPendingIdentity(pending, entry->schema, fileName);
    if (!identity.success) {
        return {false, {}, identity.error};
    }

    PendingToolPreview preview = {
        pending.pendingId,
        entry->schema,
        pending.reason,
        PendingToolPreviewKind::Generic,
        String(pending.continuation.call.name.c_str()),
        (pending.target.kind == PendingToolTargetKind::File ||
         isModelSftpSchema(entry->schema))
            ? pending.target.name : String(),
        0,
        0,
        "",
        false,
    };
    if (entry->schema == ToolSchemaId::WriteFile &&
        identity.target.exists) {
        const json_reader::JsonStringValueResult proposed =
            readCanonicalStringArgument(
                pending.continuation.call.arguments, "content",
                kMaximumWorkspaceToolChunkBytes);
        if (!proposed.success) {
            return {false, {}, proposed.error};
        }
        std::string().swap(pending.continuation.call.arguments);
        const WorkspaceChunkResult current = readWorkspaceFileChunk(
            identity.target.name, 0, kMaximumWorkspaceToolChunkBytes);
        if (!current.success) {
            return {false, {}, current.error};
        }
        if (current.offset != 0 ||
            current.totalBytes != identity.target.size) {
            return {false, {},
                    "Pending file changed while its preview was being read"};
        }
        PendingToolPreviewBodyResult body =
            buildPendingFileReplacementPreview(
                current.content, current.totalBytes, current.eof,
                proposed.value);
        if (!body.success) {
            return {false, {}, body.error};
        }
        preview.kind = PendingToolPreviewKind::FileReplacement;
        preview.currentBytes = current.totalBytes;
        preview.proposedBytes = static_cast<std::uint32_t>(
            proposed.value.size());
        preview.body = std::move(body.body);
        preview.truncated = body.truncated;
    } else if (entry->schema == ToolSchemaId::SshCommand) {
        json_reader::JsonStringValueResult command =
            readCanonicalStringArgument(
                pending.continuation.call.arguments, "command", 1024);
        if (!command.success) {
            return {false, {}, command.error};
        }
        std::string().swap(pending.continuation.call.arguments);
        PendingToolPreviewBodyResult body =
            buildPendingSshCommandPreview(std::move(command.value));
        if (!body.success) {
            return {false, {}, body.error};
        }
        preview.kind = PendingToolPreviewKind::SshCommand;
        preview.proposedBytes = static_cast<std::uint32_t>(body.body.size());
        preview.body = std::move(body.body);
    } else if (entry->schema == ToolSchemaId::SftpWrite ||
               entry->schema == ToolSchemaId::SftpMove) {
        JsonDocument argumentsDocument;
        const DeserializationError parsed = deserializeJson(
            argumentsDocument, pending.continuation.call.arguments);
        if (parsed || !argumentsDocument.is<JsonObject>()) {
            return {false, {}, "Pending SFTP preview arguments are invalid"};
        }
        const JsonObjectConst arguments =
            argumentsDocument.as<JsonObjectConst>();
        preview.body = arguments["overwrite"].as<bool>()
            ? "Overwrite existing destination: yes"
            : "Overwrite existing destination: no";
        if (entry->schema == ToolSchemaId::SftpMove) {
            preview.body = std::string("Source: ") +
                arguments["source_path"].as<const char*>() + "\n" + preview.body;
        } else {
            const JsonString content = arguments["content"].as<JsonString>();
            preview.proposedBytes = static_cast<std::uint32_t>(content.size());
        }
    }
    return {true, std::move(preview), ""};
}

PendingToolCallResult claimPendingToolCallApproval(
    const String& pendingId,
    const ToolRequestPlan& currentPlan)
{
    PendingToolCallResult loaded = loadPendingToolCall();
    if (!loaded.success) {
        return loaded;
    }
    if (!loaded.found) {
        return {false, false, {}, "Pending tool call does not exist"};
    }
    PendingToolCall& pending = loaded.pending;
    if (pending.pendingId != pendingId) {
        return {false, true, {}, "Pending tool call id does not match"};
    }
    if (pending.state != PendingToolCallState::Awaiting) {
        return {false, true, {}, "Pending tool call is not awaiting approval"};
    }
    if (!pendingToolCallIsResumableThisBoot(pendingId)) {
        return {false, true, {},
                "Pending tool request was interrupted by reboot and cannot be approved"};
    }
    if (!toolRequestPlanIsConsistent(currentPlan) ||
        currentPlan.missingRequiredGroups != 0 ||
        currentPlan.intent.mode != pending.continuation.intent.mode ||
        currentPlan.intent.requiredGroups != pending.continuation.intent.requiredGroups) {
        return {false, true, {}, "Current tool request plan does not match the pending request"};
    }
    const CanonicalToolArgumentsResult arguments =
        canonicalizeToolArguments(pending.continuation.call);
    if (!arguments.success ||
        arguments.canonical != pending.continuation.call.arguments ||
        !toolRequestPlanIncludesSchema(currentPlan, arguments.schema)) {
        return {false, true, {}, arguments.success
            ? String("Current tool request plan excludes the pending call")
            : arguments.error};
    }
    const ToolPermissionDecision decision = toolRequestPlanDecision(
        currentPlan, arguments.schema);
    if (decision != ToolPermissionDecision::Ask &&
        decision != ToolPermissionDecision::Allow) {
        return {false, true, {}, "Current tool policy denies the pending call"};
    }
    const CurrentPendingIdentityResult identity =
        validateCurrentPendingIdentity(
            pending, arguments.schema, arguments.fileName);
    if (!identity.success) {
        return {false, true, {}, identity.error};
    }
    const bool mutatesExistingTarget =
        (arguments.schema == ToolSchemaId::SftpWrite ||
         arguments.schema == ToolSchemaId::SftpMove)
        ? arguments.overwrite : identity.target.exists;
    const ToolConfirmationReason reason = toolConfirmationReason(
        decision, arguments.schema, mutatesExistingTarget);
    if ((pending.reason == PendingToolConfirmationReason::Mandatory &&
         reason != ToolConfirmationReason::Mandatory) ||
        (pending.reason == PendingToolConfirmationReason::PolicyAsk &&
         reason != ToolConfirmationReason::PolicyAsk &&
         decision != ToolPermissionDecision::Allow)) {
        return {false, true, {}, "Current confirmation decision does not match the pending call"};
    }
    pending.state = PendingToolCallState::ClaimedApprove;
    return storePendingToolCall(std::move(pending));
}

PendingToolCallResult denyPendingToolCall(const String& pendingId)
{
    PendingToolCallResult loaded = loadPendingToolCall();
    if (!loaded.success) {
        return loaded;
    }
    if (!loaded.found) {
        return {false, false, {}, "Pending tool call does not exist"};
    }
    PendingToolCall& pending = loaded.pending;
    if (pending.pendingId != pendingId) {
        return {false, true, {}, "Pending tool call id does not match"};
    }
    if (pending.state != PendingToolCallState::Awaiting) {
        return {false, true, {}, "Pending tool call is not awaiting a decision"};
    }
    const OperationResult identity = validateCurrentProjectChatIdentity(pending);
    if (!identity.success) {
        return {false, true, {}, identity.error};
    }
    pending.state = PendingToolCallState::Denied;
    return storePendingToolCall(std::move(pending));
}

OperationResult clearPendingToolCall(
    const String& pendingId,
    PendingToolCallState state)
{
    const PendingToolCallResult loaded = loadPendingToolCall();
    if (!loaded.success) {
        return {false, loaded.error};
    }
    if (!loaded.found || loaded.pending.pendingId != pendingId ||
        loaded.pending.state != state) {
        return {false, "Pending tool call id or state does not match"};
    }
    const OperationResult access = requireSdCleanupAccess();
    if (!access.success) {
        return access;
    }
    if (!SD.remove(kPendingToolCallPath) || SD.exists(kPendingToolCallPath)) {
        return {false, "Failed to remove the exact pending tool call"};
    }
    if (resumablePendingIdThisBoot == pendingId) {
        resumablePendingIdThisBoot.clear();
    }
    return {true, ""};
}

}  // namespace cardputer
