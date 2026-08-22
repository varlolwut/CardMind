#include "file_workspace.h"

#include "text_utils.h"

#include <ArduinoJson.h>
#include <SD.h>

#include <algorithm>
#include <string>

namespace cardputer {
namespace {

constexpr const char* kWorkspaceDirectory = "/assistant/files";
constexpr std::size_t kCopyBufferBytes = 1024;

OperationResult removeIfPresent(const String& path)
{
    if (!SD.exists(path)) {
        return {true, ""};
    }
    if (!SD.remove(path)) {
        return {false, "Failed to remove workspace file " + path};
    }
    return {true, ""};
}

std::string jsonOutput(const JsonDocument& document)
{
    String output;
    serializeJson(document, output);
    return output.c_str();
}

ToolExecutionResult toolFailure(const String& error)
{
    JsonDocument document;
    document["ok"] = false;
    document["error"] = error;
    return {false, jsonOutput(document), error};
}

OperationResult copyFile(const String& sourcePath, const String& destinationPath)
{
    File source = SD.open(sourcePath, FILE_READ);
    if (!source) {
        return {false, "Failed to open source workspace file for copying"};
    }
    File destination = SD.open(destinationPath, FILE_WRITE);
    if (!destination) {
        source.close();
        return {false, "Failed to create temporary workspace copy"};
    }
    std::uint8_t buffer[kCopyBufferBytes] = {};
    std::size_t copiedBytes = 0;
    while (source.available() > 0) {
        const std::size_t readBytes = source.read(buffer, sizeof(buffer));
        if (readBytes == 0) {
            source.close();
            destination.close();
            return {false, "Workspace copy stopped before reaching end of source file"};
        }
        const std::size_t writtenBytes = destination.write(buffer, readBytes);
        if (writtenBytes != readBytes) {
            source.close();
            destination.close();
            return {false, "Workspace copy could not write a complete block"};
        }
        copiedBytes += writtenBytes;
    }
    const std::size_t expectedBytes = source.size();
    destination.flush();
    source.close();
    destination.close();
    return copiedBytes == expectedBytes
        ? OperationResult{true, ""}
        : OperationResult{false, "Workspace copy size does not match source size"};
}

OperationResult commitTemporaryFile(const String& target,
                                    const String& temporary,
                                    const String& backup)
{
    const bool hadTarget = SD.exists(target);
    if (hadTarget && !SD.rename(target, backup)) {
        removeIfPresent(temporary);
        return {false, "Failed to create a backup before replacing workspace file"};
    }
    if (!SD.rename(temporary, target)) {
        if (hadTarget && !SD.rename(backup, target)) {
            return {false, "Failed to commit workspace file and restore its backup"};
        }
        return {false, "Failed to commit workspace file"};
    }
    return hadTarget ? removeIfPresent(backup) : OperationResult{true, ""};
}

OperationResult prepareTemporaryPaths(const String& temporary, const String& backup)
{
    const OperationResult temporaryResult = removeIfPresent(temporary);
    return temporaryResult.success ? removeIfPresent(backup) : temporaryResult;
}

ToolExecutionResult listFilesTool()
{
    const WorkspaceFilesResult result = listWorkspaceFiles();
    if (!result.success) {
        return toolFailure(result.error);
    }
    JsonDocument document;
    document["ok"] = true;
    JsonArray files = document["files"].to<JsonArray>();
    for (const auto& file : result.files) {
        JsonObject item = files.add<JsonObject>();
        item["name"] = file.name;
        item["bytes"] = file.size;
    }
    return {true, jsonOutput(document), ""};
}

ToolExecutionResult readFileTool(const String& name,
                                 std::size_t offset,
                                 std::size_t maximumBytes)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return toolFailure("Invalid filename; use ASCII letters, digits, ._- and a text extension");
    }
    if (maximumBytes == 0 || maximumBytes > kMaximumWorkspaceToolChunkBytes) {
        return toolFailure("read_file max_bytes must be between 1 and 12288");
    }
    const String path = workspaceFilePath(name);
    File file = SD.open(path, FILE_READ);
    if (!file) {
        return toolFailure("Workspace file does not exist: " + name);
    }
    const std::size_t totalBytes = file.size();
    if (totalBytes > kMaximumWorkspaceFileBytes) {
        file.close();
        return toolFailure("Workspace file exceeds the 491520-byte size limit");
    }
    if (offset > totalBytes || !file.seek(static_cast<std::uint32_t>(offset))) {
        file.close();
        return toolFailure("read_file offset is outside the file or could not be selected");
    }
    const std::size_t requestedBytes = std::min(maximumBytes, totalBytes - offset);
    std::string content(requestedBytes, '\0');
    const std::size_t readBytes = requestedBytes == 0
        ? 0
        : file.read(reinterpret_cast<std::uint8_t*>(&content[0]), requestedBytes);
    file.close();
    if (readBytes != requestedBytes) {
        return toolFailure("microSD read ended before the requested file chunk was complete");
    }
    while (!content.empty() && !isValidUtf8(content)) {
        content.pop_back();
    }
    if (requestedBytes > 0 && content.empty()) {
        return toolFailure("read_file offset does not point to a UTF-8 code-point boundary");
    }
    const std::size_t nextOffset = offset + content.size();
    JsonDocument document;
    document["ok"] = true;
    document["name"] = name;
    document["total_bytes"] = totalBytes;
    document["offset"] = offset;
    document["next_offset"] = nextOffset;
    document["eof"] = nextOffset == totalBytes;
    document["content"] = content;
    return {true, jsonOutput(document), ""};
}

ToolExecutionResult writeFileTool(const String& name, const std::string& content)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return toolFailure("Invalid filename; use ASCII letters, digits, ._- and a text extension");
    }
    if (content.size() > kMaximumWorkspaceToolChunkBytes) {
        return toolFailure("write_file content exceeds 12288 bytes; write an initial chunk, then use append_file");
    }
    if (!isValidUtf8(content)) {
        return toolFailure("File content must be valid UTF-8 text");
    }
    const String target = workspaceFilePath(name);
    const String temporary = target + ".tmp";
    const String backup = target + ".bak";
    const OperationResult prepared = prepareTemporaryPaths(temporary, backup);
    if (!prepared.success) {
        return toolFailure(prepared.error);
    }
    File file = SD.open(temporary, FILE_WRITE);
    if (!file) {
        return toolFailure("Failed to create temporary workspace file");
    }
    const std::size_t written = content.empty()
        ? 0
        : file.write(reinterpret_cast<const std::uint8_t*>(content.data()), content.size());
    file.flush();
    file.close();
    if (written != content.size()) {
        removeIfPresent(temporary);
        return toolFailure("Failed to write complete workspace file content");
    }
    const OperationResult committed = commitTemporaryFile(target, temporary, backup);
    if (!committed.success) {
        return toolFailure(committed.error);
    }
    JsonDocument document;
    document["ok"] = true;
    document["name"] = name;
    document["bytes"] = content.size();
    return {true, jsonOutput(document), ""};
}

ToolExecutionResult appendFileTool(const String& name, const std::string& content)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return toolFailure("Invalid filename; use ASCII letters, digits, ._- and a text extension");
    }
    if (content.size() > kMaximumWorkspaceToolChunkBytes) {
        return toolFailure("append_file content exceeds the 12288-byte chunk limit");
    }
    if (!isValidUtf8(content)) {
        return toolFailure("File content must be valid UTF-8 text");
    }
    const String target = workspaceFilePath(name);
    File existing = SD.open(target, FILE_READ);
    if (!existing) {
        return toolFailure("Workspace file does not exist; create it with write_file before appending");
    }
    const std::size_t currentBytes = existing.size();
    existing.close();
    if (currentBytes > kMaximumWorkspaceFileBytes ||
        content.size() > kMaximumWorkspaceFileBytes - currentBytes) {
        return toolFailure("Appending would exceed the 491520-byte file limit");
    }
    const String temporary = target + ".tmp";
    const String backup = target + ".bak";
    OperationResult result = prepareTemporaryPaths(temporary, backup);
    if (!result.success) {
        return toolFailure(result.error);
    }
    result = copyFile(target, temporary);
    if (!result.success) {
        removeIfPresent(temporary);
        return toolFailure(result.error);
    }
    File file = SD.open(temporary, FILE_APPEND);
    if (!file) {
        removeIfPresent(temporary);
        return toolFailure("Failed to open temporary workspace file for appending");
    }
    const std::size_t written = content.empty()
        ? 0
        : file.write(reinterpret_cast<const std::uint8_t*>(content.data()), content.size());
    file.flush();
    file.close();
    if (written != content.size()) {
        removeIfPresent(temporary);
        return toolFailure("Failed to append complete workspace file content");
    }
    result = commitTemporaryFile(target, temporary, backup);
    if (!result.success) {
        return toolFailure(result.error);
    }
    JsonDocument document;
    document["ok"] = true;
    document["name"] = name;
    document["bytes"] = currentBytes + content.size();
    return {true, jsonOutput(document), ""};
}

}  // namespace

OperationResult initializeFileWorkspace()
{
    if (SD.cardType() == CARD_NONE) {
        return {false, "microSD is required for the model file workspace"};
    }
    if (!SD.exists("/assistant") && !SD.mkdir("/assistant")) {
        return {false, "Failed to create /assistant on microSD"};
    }
    if (!SD.exists(kWorkspaceDirectory) && !SD.mkdir(kWorkspaceDirectory)) {
        return {false, "Failed to create /assistant/files on microSD"};
    }
    return {true, ""};
}

WorkspaceFilesResult listWorkspaceFiles()
{
    File directory = SD.open(kWorkspaceDirectory);
    if (!directory || !directory.isDirectory()) {
        return {false, {}, "Failed to open /assistant/files directory"};
    }
    std::vector<WorkspaceFile> files;
    File file = directory.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            const int slash = name.lastIndexOf('/');
            if (slash >= 0) {
                name = name.substring(slash + 1);
            }
            if (!isValidWorkspaceFilename(name.c_str())) {
                file.close();
                directory.close();
                return {false, {}, "Workspace contains an invalid filename: " + name};
            }
            if (file.size() > kMaximumWorkspaceFileBytes) {
                file.close();
                directory.close();
                return {false, {}, "Workspace file exceeds the 491520-byte limit: " + name};
            }
            files.push_back({name, static_cast<std::uint32_t>(file.size())});
            if (files.size() > kMaximumWorkspaceFiles) {
                file.close();
                directory.close();
                return {false, {}, "Workspace contains more than 40 files"};
            }
        }
        file.close();
        file = directory.openNextFile();
    }
    directory.close();
    std::sort(files.begin(), files.end(), [](const WorkspaceFile& left, const WorkspaceFile& right) {
        return left.name < right.name;
    });
    return {true, files, ""};
}

ToolExecutionResult executeWorkspaceTool(const ToolCall& call)
{
    JsonDocument arguments;
    const DeserializationError jsonError = deserializeJson(arguments, call.arguments);
    if (jsonError) {
        return toolFailure(String("Tool arguments must be a JSON object: ") + jsonError.c_str());
    }
    if (!arguments.is<JsonObject>()) {
        return toolFailure("Tool arguments must be a JSON object");
    }
    if (call.name == "list_files") {
        return listFilesTool();
    }
    if (!arguments["name"].is<const char*>()) {
        return toolFailure("Tool arguments are missing required string field 'name'");
    }
    const String name = arguments["name"].as<const char*>();
    if (call.name == "read_file") {
        if (!arguments["offset"].is<std::size_t>() || !arguments["max_bytes"].is<std::size_t>()) {
            return toolFailure("read_file requires numeric fields 'offset' and 'max_bytes'");
        }
        return readFileTool(name, arguments["offset"].as<std::size_t>(),
                            arguments["max_bytes"].as<std::size_t>());
    }
    if (call.name == "write_file" || call.name == "append_file") {
        if (!arguments["content"].is<const char*>()) {
            return toolFailure(String(call.name.c_str()) +
                               " arguments are missing required string field 'content'");
        }
        const std::string content = arguments["content"].as<const char*>();
        return call.name == "write_file" ? writeFileTool(name, content)
                                         : appendFileTool(name, content);
    }
    return toolFailure(String("Unsupported workspace tool: ") + call.name.c_str());
}

String workspaceFilePath(const String& name)
{
    return String(kWorkspaceDirectory) + "/" + name;
}

}  // namespace cardputer
