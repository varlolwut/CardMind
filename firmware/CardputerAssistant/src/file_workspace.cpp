#include "file_workspace.h"

#include "text_utils.h"
#include "sd_storage.h"
#include "project_storage.h"

#include <ArduinoJson.h>
#include <SD.h>

#include <algorithm>
#include <limits>
#include <string>

namespace cardputer {
namespace {

constexpr const char* kWorkspaceDirectory = "/assistant/files";
constexpr const char* kBookmarksPath = "/assistant/file_bookmarks.json";
constexpr std::size_t kCopyBufferBytes = 4096;
constexpr std::size_t kMaximumSearchBytes = 1024;
constexpr std::size_t kDefaultWorkspaceToolListEntries = 16;
constexpr std::size_t kMaximumWorkspaceToolListEntries = 16;

OperationResult requireWorkspaceTextFile(const String& name)
{
    return isWorkspaceTextFile(std::string(name.c_str()))
        ? OperationResult{true, ""}
        : OperationResult{false,
                          "Workspace text operation requires a safe text, source, or config extension: " +
                              name};
}

struct BookmarkEntry {
    String name;
    std::uint32_t offset;
};

struct BookmarkEntriesResult {
    bool success;
    std::vector<BookmarkEntry> entries;
    String error;
};

struct WorkspacePageCollector {
    std::uint32_t skipped;
    std::size_t maximumEntries;
    std::uint32_t visited;
    bool hasMore;
    std::vector<WorkspaceFile> entries;
    String error;
};

struct WorkspaceRecoveryCollector {
    std::vector<String> targets;
    String error;
};

struct ListFilesArgumentsResult {
    bool success;
    std::uint32_t offset;
    std::size_t maximumEntries;
    String error;
};

ListFilesArgumentsResult parseListFilesArguments(const JsonObjectConst& arguments)
{
    std::uint32_t offset = 0;
    std::size_t maximumEntries = kDefaultWorkspaceToolListEntries;
    for (const JsonPairConst field : arguments) {
        const String name = field.key().c_str();
        if (name == "offset") {
            if (!field.value().is<std::uint32_t>()) {
                return {false, 0, 0,
                        "list_files field 'offset' must be a non-negative 32-bit integer"};
            }
            offset = field.value().as<std::uint32_t>();
        } else if (name == "max_entries") {
            if (!field.value().is<std::uint32_t>()) {
                return {false, 0, 0,
                        "list_files field 'max_entries' must be an integer between 1 and 16"};
            }
            const std::uint32_t requestedEntries = field.value().as<std::uint32_t>();
            if (requestedEntries == 0 ||
                requestedEntries > kMaximumWorkspaceToolListEntries) {
                return {false, 0, 0,
                        "list_files field 'max_entries' must be between 1 and 16"};
            }
            maximumEntries = requestedEntries;
        } else {
            return {false, 0, 0,
                    "list_files accepts only optional fields 'offset' and 'max_entries'"};
        }
    }
    return {true, offset, maximumEntries, ""};
}

String baseName(const String& path)
{
    const int slash = path.lastIndexOf('/');
    return slash >= 0 ? path.substring(slash + 1) : path;
}

void collectWorkspaceEntries(File& directory,
                             const String& prefix,
                             std::size_t depth,
                             WorkspacePageCollector& collector)
{
    if (!collector.error.isEmpty() || collector.hasMore) {
        return;
    }
    if (depth > 16) {
        collector.error = "Workspace directory nesting exceeds 16 levels";
        return;
    }
    File entry = directory.openNextFile();
    while (entry) {
        const String relativePath = prefix.isEmpty()
            ? baseName(entry.name()) : prefix + "/" + baseName(entry.name());
        if (!isValidStorageRelativePath(std::string(relativePath.c_str()), 512)) {
            collector.error = "Workspace contains an invalid path: " + relativePath;
            entry.close();
            return;
        }
        if (entry.isDirectory()) {
            collectWorkspaceEntries(entry, relativePath, depth + 1, collector);
        } else {
            const std::size_t fileBytes = entry.size();
            if (fileBytes > kMaximumWorkspaceFileBytes) {
                collector.error = "Workspace file exceeds the supported 32-bit file range: " +
                    relativePath;
                entry.close();
                return;
            }
            if (collector.visited >= collector.skipped) {
                if (collector.entries.size() >= collector.maximumEntries) {
                    collector.hasMore = true;
                    entry.close();
                    return;
                }
                collector.entries.push_back(
                    {relativePath, static_cast<std::uint32_t>(fileBytes), false});
            }
            ++collector.visited;
        }
        entry.close();
        if (!collector.error.isEmpty() || collector.hasMore) {
            return;
        }
        entry = directory.openNextFile();
    }
}

void collectWorkspaceRecoveryTargets(File& directory,
                                     const String& prefix,
                                     std::size_t depth,
                                     WorkspaceRecoveryCollector& collector)
{
    if (!collector.error.isEmpty() || collector.targets.size() >= 32) {
        return;
    }
    if (depth > 16) {
        collector.error = "Workspace directory nesting exceeds 16 levels";
        return;
    }
    File entry = directory.openNextFile();
    while (entry) {
        const String relativePath = prefix.isEmpty()
            ? baseName(entry.name()) : prefix + "/" + baseName(entry.name());
        if (entry.isDirectory()) {
            collectWorkspaceRecoveryTargets(entry, relativePath, depth + 1, collector);
        } else if (relativePath.endsWith(".tmp") || relativePath.endsWith(".bak")) {
            const String target = relativePath.substring(0, relativePath.length() - 4);
            if (!isValidWorkspaceFilename(target.c_str())) {
                collector.error = "Workspace contains an invalid recovery artifact: " +
                    relativePath;
            } else if (std::find(collector.targets.begin(), collector.targets.end(), target) ==
                       collector.targets.end()) {
                collector.targets.push_back(target);
            }
        }
        entry.close();
        if (!collector.error.isEmpty() || collector.targets.size() >= 32) {
            return;
        }
        entry = directory.openNextFile();
    }
}

OperationResult recoverWorkspaceArtifacts()
{
    while (true) {
        File directory = SD.open(kWorkspaceDirectory);
        if (!directory || !directory.isDirectory()) {
            return {false, "Failed to open workspace while recovering interrupted writes"};
        }
        WorkspaceRecoveryCollector collector;
        collector.targets.reserve(32);
        collectWorkspaceRecoveryTargets(directory, "", 0, collector);
        directory.close();
        if (!collector.error.isEmpty()) {
            return {false, collector.error};
        }
        if (collector.targets.empty()) {
            return {true, ""};
        }
        for (const String& target : collector.targets) {
            const OperationResult recovered = recoverAtomicSdFile(workspaceFilePath(target));
            if (!recovered.success) {
                return recovered;
            }
        }
    }
}

OperationResult ensureWorkspaceParentDirectories(const String& name)
{
    std::size_t separator = std::string(name.c_str()).find('/');
    while (separator != std::string::npos) {
        const String directory = String(kWorkspaceDirectory) + "/" + name.substring(0, separator);
        if (!SD.exists(directory) && !SD.mkdir(directory)) {
            return {false, "Failed to create workspace directory: " + directory};
        }
        separator = std::string(name.c_str()).find('/', separator + 1);
    }
    return {true, ""};
}

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

ToolExecutionResult toolCancelled(const String& error)
{
    JsonDocument document;
    document["ok"] = false;
    document["error"] = error;
    return {
        false,
        jsonOutput(document),
        error,
        ToolExecutionOutcome::Cancelled,
    };
}

OperationResult copyFileControlled(
    const String& sourcePath,
    const String& destinationPath,
    const std::function<bool()>& isCancelled,
    bool& cancelled)
{
    cancelled = false;
    File source = SD.open(sourcePath, FILE_READ);
    if (!source) {
        return {false, "Failed to open source workspace file for copying"};
    }
    const std::size_t expectedBytes = source.size();
    if (expectedBytes > kMaximumWorkspaceFileBytes) {
        source.close();
        return {false, "Workspace source exceeds the supported 32-bit file range"};
    }
    const OperationResult space = checkSdOperationSpace(
        expectedBytes, kStorageOperationalFloorBytes);
    if (!space.success) {
        source.close();
        return space;
    }
    File destination = SD.open(destinationPath, FILE_WRITE);
    if (!destination) {
        source.close();
        return {false, "Failed to create temporary workspace copy"};
    }
    std::uint8_t buffer[kCopyBufferBytes] = {};
    std::uint32_t copiedBytes = 0;
    while (copiedBytes < expectedBytes) {
        if (isCancelled()) {
            cancelled = true;
            source.close();
            destination.close();
            return {false, "Workspace copy canceled by user"};
        }
        const std::size_t blockBytes = std::min<std::size_t>(
            sizeof(buffer), expectedBytes - copiedBytes);
        const std::size_t readBytes = source.read(buffer, blockBytes);
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
        if (writtenBytes > kMaximumWorkspaceFileBytes - copiedBytes) {
            source.close();
            destination.close();
            return {false, "Workspace copy exceeds the supported 32-bit file range"};
        }
        copiedBytes += static_cast<std::uint32_t>(writtenBytes);
    }
    destination.flush();
    source.close();
    destination.close();
    return copiedBytes == expectedBytes
        ? OperationResult{true, ""}
        : OperationResult{false, "Workspace copy size does not match source size"};
}

OperationResult copyFile(const String& sourcePath, const String& destinationPath)
{
    const std::function<bool()> isCancelled = []() { return false; };
    bool cancelled = false;
    return copyFileControlled(sourcePath, destinationPath, isCancelled, cancelled);
}

OperationResult copyExactBytes(File& source,
                               File& destination,
                               std::size_t byteCount,
                               const String& operation)
{
    std::uint8_t buffer[kCopyBufferBytes] = {};
    std::size_t remaining = byteCount;
    while (remaining > 0) {
        const std::size_t blockBytes = std::min(remaining, sizeof(buffer));
        const std::size_t readBytes = source.read(buffer, blockBytes);
        if (readBytes != blockBytes) {
            return {false, operation + " could not read a complete source block"};
        }
        const std::size_t writtenBytes = destination.write(buffer, readBytes);
        if (writtenBytes != readBytes) {
            return {false, operation + " could not write a complete destination block"};
        }
        remaining -= blockBytes;
    }
    return {true, ""};
}

bool isFileUtf8Boundary(File& file, std::size_t offset, std::size_t totalBytes)
{
    if (offset == 0 || offset == totalBytes) {
        return true;
    }
    if (!file.seek(static_cast<std::uint32_t>(offset))) {
        return false;
    }
    const int value = file.read();
    return value >= 0 && (static_cast<std::uint8_t>(value) & 0xC0U) != 0x80U;
}

OperationResult validateWorkspaceFileUtf8Path(const String& path)
{
    File file = SD.open(path, FILE_READ);
    if (!file) {
        return {false, "Workspace file does not exist: " + path};
    }
    const std::size_t fileBytes = file.size();
    if (fileBytes > kMaximumWorkspaceFileBytes) {
        file.close();
        return {false, "Workspace file exceeds the supported 32-bit file range"};
    }
    constexpr std::size_t blockBytes = 4096;
    std::vector<std::uint8_t> block(blockBytes);
    std::string pending;
    while (file.position() < fileBytes) {
        const std::size_t readRequestBytes = std::min<std::size_t>(
            block.size(), fileBytes - file.position());
        const std::size_t readBytes = file.read(block.data(), readRequestBytes);
        if (readBytes == 0) {
            file.close();
            return {false, "microSD read stopped while validating UTF-8"};
        }
        pending.append(reinterpret_cast<const char*>(block.data()), readBytes);
        if (file.position() == fileBytes) {
            break;
        }
        bool prefixValid = false;
        for (std::size_t retained = 0; retained <= 3 && retained <= pending.size(); ++retained) {
            const std::size_t prefixBytes = pending.size() - retained;
            if (isValidUtf8(pending.substr(0, prefixBytes))) {
                pending = pending.substr(prefixBytes);
                prefixValid = true;
                break;
            }
        }
        if (!prefixValid) {
            file.close();
            return {false, "Workspace file contains invalid UTF-8"};
        }
    }
    file.close();
    return isValidUtf8(pending)
        ? OperationResult{true, ""}
        : OperationResult{false, "Workspace file contains incomplete or invalid UTF-8"};
}

OperationResult commitTemporaryFile(const String& target,
                                    const String& temporary,
                                    const String& backup)
{
    const bool hadTarget = SD.exists(target);
    if (hadTarget && !SD.rename(target, backup)) {
        const OperationResult cleaned = removeIfPresent(temporary);
        return cleaned.success
            ? OperationResult{
                false, "Failed to create a backup before replacing workspace file"}
            : OperationResult{
                false,
                "Failed to create a backup before replacing workspace file; " +
                    cleaned.error};
    }
    if (!SD.rename(temporary, target)) {
        if (hadTarget && !SD.rename(backup, target)) {
            return {false, "Failed to commit workspace file and restore its backup"};
        }
        const OperationResult cleaned = removeIfPresent(temporary);
        return cleaned.success
            ? OperationResult{false, "Failed to commit workspace file"}
            : OperationResult{
                false,
                String("Failed to commit workspace file") +
                    (hadTarget ? " after restoring its backup; " : "; ") +
                    cleaned.error};
    }
    return hadTarget ? removeIfPresent(backup) : OperationResult{true, ""};
}

OperationResult prepareWorkspaceTarget(const String& target)
{
    return recoverAtomicSdFile(target);
}

BookmarkEntriesResult loadBookmarkEntries()
{
    const OperationResult recovered = recoverAtomicSdFile(kBookmarksPath);
    if (!recovered.success) {
        return {false, {}, recovered.error};
    }
    if (!SD.exists(kBookmarksPath)) {
        return {true, {}, ""};
    }
    File file = SD.open(kBookmarksPath, FILE_READ);
    if (!file) {
        return {false, {}, "Failed to open workspace bookmark metadata"};
    }
    JsonDocument document;
    const DeserializationError parseError = deserializeJson(document, file);
    file.close();
    if (parseError) {
        return {false, {}, "Workspace bookmark metadata is invalid JSON: " +
                            String(parseError.c_str())};
    }
    if (!document["bookmarks"].is<JsonArray>()) {
        return {false, {}, "Workspace bookmark metadata is missing the bookmarks array"};
    }
    std::vector<BookmarkEntry> entries;
    for (JsonObjectConst item : document["bookmarks"].as<JsonArrayConst>()) {
        if (!item["name"].is<const char*>() || !item["offset"].is<std::uint32_t>()) {
            return {false, {}, "Workspace bookmark entry is missing name or offset"};
        }
        const String name = item["name"].as<const char*>();
        if (!isValidWorkspaceFilename(name.c_str())) {
            return {false, {}, "Workspace bookmark contains an invalid filename"};
        }
        entries.push_back({name, item["offset"].as<std::uint32_t>()});
        if (entries.size() > kMaximumWorkspaceFiles) {
            return {false, {}, "Workspace bookmark metadata contains more than 4096 entries"};
        }
    }
    return {true, entries, ""};
}

OperationResult saveBookmarkEntries(const std::vector<BookmarkEntry>& entries)
{
    const String target = kBookmarksPath;
    const String temporary = target + ".tmp";
    const String backup = target + ".bak";
    OperationResult result = prepareWorkspaceTarget(target);
    if (!result.success) {
        return result;
    }
    JsonDocument document;
    JsonArray bookmarks = document["bookmarks"].to<JsonArray>();
    for (const auto& entry : entries) {
        JsonObject item = bookmarks.add<JsonObject>();
        item["name"] = entry.name;
        item["offset"] = entry.offset;
    }
    File file = SD.open(temporary, FILE_WRITE);
    if (!file) {
        return {false, "Failed to create temporary workspace bookmark metadata"};
    }
    const std::size_t written = serializeJson(document, file);
    file.flush();
    file.close();
    if (written == 0) {
        removeIfPresent(temporary);
        return {false, "Failed to write workspace bookmark metadata"};
    }
    return commitTemporaryFile(target, temporary, backup);
}

ToolExecutionResult listFilesTool(std::uint32_t offset, std::size_t maximumEntries)
{
    const WorkspaceFilesPageResult result = listWorkspaceFilesPage(offset, maximumEntries);
    if (!result.success) {
        return toolFailure(result.error);
    }
    if (!result.eof && result.nextOffset <= offset) {
        return toolFailure("Workspace pagination did not advance");
    }
    JsonDocument document;
    document["ok"] = true;
    JsonArray files = document["files"].to<JsonArray>();
    for (const auto& file : result.files) {
        if (!isWorkspaceTextFile(std::string(file.name.c_str()))) {
            continue;
        }
        JsonObject item = files.add<JsonObject>();
        item["name"] = file.name;
        item["bytes"] = file.size;
    }
    document["next_offset"] = result.nextOffset;
    document["eof"] = result.eof;
    return {true, jsonOutput(document), ""};
}

ToolExecutionResult readFileTool(const String& name,
                                 std::size_t offset,
                                 std::size_t maximumBytes)
{
    if (offset > std::numeric_limits<std::uint32_t>::max()) {
        return toolFailure("read_file offset exceeds the supported 32-bit file range");
    }
    const WorkspaceChunkResult result = readWorkspaceFileChunk(
        name, static_cast<std::uint32_t>(offset), maximumBytes);
    if (!result.success) {
        return toolFailure(result.error);
    }
    JsonDocument document;
    document["ok"] = true;
    document["name"] = name;
    document["total_bytes"] = result.totalBytes;
    document["offset"] = result.offset;
    document["next_offset"] = result.nextOffset;
    document["eof"] = result.eof;
    document["content"] = result.content;
    return {true, jsonOutput(document), ""};
}

ToolExecutionResult writeFileTool(const String& name, const std::string& content)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return toolFailure("Invalid workspace-relative path");
    }
    const OperationResult textFile = requireWorkspaceTextFile(name);
    if (!textFile.success) {
        return toolFailure(textFile.error);
    }
    if (content.size() > kMaximumWorkspaceToolChunkBytes) {
        return toolFailure("write_file content exceeds 12288 bytes; write an initial chunk, then use append_file");
    }
    if (!isValidUtf8(content)) {
        return toolFailure("File content must be valid UTF-8 text");
    }
    const OperationResult space = checkSdOperationSpace(
        content.size(), kStorageOperationalFloorBytes);
    if (!space.success) {
        return toolFailure(space.error);
    }
    const OperationResult parent = ensureWorkspaceParentDirectories(name);
    if (!parent.success) {
        return toolFailure(parent.error);
    }
    const String target = workspaceFilePath(name);
    const String temporary = target + ".tmp";
    const String backup = target + ".bak";
    const OperationResult prepared = prepareWorkspaceTarget(target);
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

ToolExecutionResult appendFileTool(
    const String& name,
    const std::string& content,
    const std::function<bool()>& isCancelled)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return toolFailure("Invalid workspace-relative path");
    }
    const OperationResult textFile = requireWorkspaceTextFile(name);
    if (!textFile.success) {
        return toolFailure(textFile.error);
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
    const std::size_t currentBytesValue = existing.size();
    existing.close();
    if (currentBytesValue > kMaximumWorkspaceFileBytes) {
        return toolFailure("Workspace file exceeds the supported 32-bit file range");
    }
    const std::uint32_t currentBytes = static_cast<std::uint32_t>(currentBytesValue);
    if (content.size() > kMaximumWorkspaceFileBytes - currentBytes) {
        return toolFailure("Appending would exceed the supported 32-bit file range");
    }
    const OperationResult space = checkSdOperationSpace(
        currentBytes + content.size(), kStorageOperationalFloorBytes);
    if (!space.success) {
        return toolFailure(space.error);
    }
    const String temporary = target + ".tmp";
    const String backup = target + ".bak";
    OperationResult result = prepareWorkspaceTarget(target);
    if (!result.success) {
        return toolFailure(result.error);
    }
    bool cancelled = false;
    result = copyFileControlled(target, temporary, isCancelled, cancelled);
    if (cancelled) {
        const OperationResult removed = removeIfPresent(temporary);
        return toolCancelled(removed.success
            ? String("Workspace append canceled by user")
            : String("Workspace append canceled; temporary cleanup failed: ") +
                  removed.error);
    }
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
    if (isCancelled()) {
        const OperationResult removed = removeIfPresent(temporary);
        return toolCancelled(removed.success
            ? String("Workspace append canceled by user")
            : String("Workspace append canceled; temporary cleanup failed: ") +
                  removed.error);
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
    return recoverWorkspaceArtifacts();
}

OperationResult ensureWorkspaceFileParent(const String& name)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, "Invalid workspace-relative path"};
    }
    return ensureWorkspaceParentDirectories(name);
}

ToolExecutionResult listProjectFilesTool(const String& projectId,
                                         std::uint32_t offset,
                                         std::size_t maximumEntries)
{
    const SharedFileLinksPageResult page = listProjectSharedLinksPage(
        projectId, offset, maximumEntries);
    if (!page.success) {
        return toolFailure(page.error);
    }
    if (!page.eof && page.nextOffset <= offset) {
        return toolFailure("Project Shared-link pagination did not advance");
    }
    JsonDocument document;
    document["ok"] = true;
    JsonArray files = document["files"].to<JsonArray>();
    for (const SharedFileLink& link : page.links) {
        File file = SD.open(workspaceFilePath(link.path), FILE_READ);
        if (!file || file.isDirectory()) {
            if (file) {
                file.close();
            }
            return toolFailure("Project links a missing Shared file: " + link.path);
        }
        if (!isWorkspaceTextFile(std::string(link.path.c_str()))) {
            file.close();
            continue;
        }
        JsonObject item = files.add<JsonObject>();
        item["name"] = link.path;
        item["bytes"] = file.size();
        file.close();
    }
    document["next_offset"] = page.nextOffset;
    document["eof"] = page.eof;
    return {true, jsonOutput(document), ""};
}

WorkspaceFilesPageResult listWorkspaceFilesPage(std::uint32_t offset,
                                                std::size_t maximumEntries)
{
    if (maximumEntries == 0 || maximumEntries > 64) {
        return {false, {}, offset, true,
                "Workspace page size must be between 1 and 64"};
    }
    File directory = SD.open(kWorkspaceDirectory);
    if (!directory || !directory.isDirectory()) {
        return {false, {}, offset, true, "Failed to open /assistant/files directory"};
    }
    WorkspacePageCollector collector = {offset, maximumEntries, 0, false, {}, ""};
    collector.entries.reserve(maximumEntries);
    collectWorkspaceEntries(directory, "", 0, collector);
    directory.close();
    if (!collector.error.isEmpty()) {
        return {false, {}, offset, true, collector.error};
    }
    const std::uint32_t nextOffset = offset +
        static_cast<std::uint32_t>(collector.entries.size());
    return {true, std::move(collector.entries), nextOffset, !collector.hasMore, ""};
}

WorkspaceChunkResult readWorkspaceFileChunk(const String& name,
                                            std::uint32_t offset,
                                            std::size_t maximumBytes)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, "", 0, 0, 0, true,
                "Invalid workspace-relative path"};
    }
    const OperationResult textFile = requireWorkspaceTextFile(name);
    if (!textFile.success) {
        return {false, "", 0, 0, 0, true, textFile.error};
    }
    if (maximumBytes == 0 || maximumBytes > kMaximumWorkspaceToolChunkBytes) {
        return {false, "", 0, 0, 0, true,
                "Workspace read size must be between 1 and 12288 bytes"};
    }
    File file = SD.open(workspaceFilePath(name), FILE_READ);
    if (!file) {
        return {false, "", 0, 0, 0, true, "Workspace file does not exist: " + name};
    }
    const std::size_t totalBytesValue = file.size();
    if (totalBytesValue > kMaximumWorkspaceFileBytes) {
        file.close();
        return {false, "", 0, 0, 0, true,
                "Workspace file exceeds the supported 32-bit file range"};
    }
    const std::uint32_t totalBytes = static_cast<std::uint32_t>(totalBytesValue);
    if (offset > totalBytes || !isFileUtf8Boundary(file, offset, totalBytes) ||
        !file.seek(offset)) {
        file.close();
        return {false, "", 0, 0, 0, true,
                "Workspace offset is outside the file or not a UTF-8 boundary"};
    }
    const std::size_t availableBytes = totalBytes - offset;
    const std::size_t requestedBytes = std::min(maximumBytes, availableBytes);
    std::string content(requestedBytes, '\0');
    const std::size_t readBytes = requestedBytes == 0
        ? 0
        : file.read(reinterpret_cast<std::uint8_t*>(&content[0]), requestedBytes);
    file.close();
    if (readBytes != requestedBytes) {
        return {false, "", 0, 0, 0, true,
                "microSD read ended before the requested file chunk was complete"};
    }
    while (!content.empty() && !isValidUtf8(content)) {
        content.pop_back();
    }
    if (requestedBytes > 0 && content.empty()) {
        return {false, "", 0, 0, 0, true,
                "Workspace chunk does not contain a complete UTF-8 code point"};
    }
    const std::uint32_t nextOffset = offset + static_cast<std::uint32_t>(content.size());
    return {true, content, offset, nextOffset, totalBytes,
            nextOffset == totalBytes, ""};
}

WorkspaceFindResult findWorkspaceText(const String& name,
                                      const std::string& query,
                                      std::uint32_t startOffset)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, false, 0, "Invalid workspace filename"};
    }
    const OperationResult textFile = requireWorkspaceTextFile(name);
    if (!textFile.success) {
        return {false, false, 0, textFile.error};
    }
    if (query.empty() || query.size() > kMaximumSearchBytes || !isValidUtf8(query)) {
        return {false, false, 0, "Search text must be valid UTF-8 between 1 and 1024 bytes"};
    }
    File file = SD.open(workspaceFilePath(name), FILE_READ);
    if (!file) {
        return {false, false, 0, "Workspace file does not exist: " + name};
    }
    const std::size_t totalBytesValue = file.size();
    if (totalBytesValue > kMaximumWorkspaceFileBytes) {
        file.close();
        return {false, false, 0,
                "Workspace file exceeds the supported 32-bit file range"};
    }
    const std::uint32_t totalBytes = static_cast<std::uint32_t>(totalBytesValue);
    if (startOffset > totalBytes ||
        !isFileUtf8Boundary(file, startOffset, totalBytes) || !file.seek(startOffset)) {
        file.close();
        return {false, false, 0,
                "Search offset is outside the file or not a UTF-8 boundary"};
    }
    std::string window;
    window.reserve(kCopyBufferBytes + query.size());
    std::uint32_t windowOffset = startOffset;
    std::uint8_t buffer[kCopyBufferBytes] = {};
    while (file.position() < totalBytes) {
        const std::size_t blockBytes = std::min<std::size_t>(
            sizeof(buffer), totalBytes - file.position());
        const std::size_t readBytes = file.read(buffer, blockBytes);
        if (readBytes == 0) {
            file.close();
            return {false, false, 0, "Workspace search stopped before end of file"};
        }
        window.append(reinterpret_cast<const char*>(buffer), readBytes);
        const std::size_t match = window.find(query);
        if (match != std::string::npos) {
            file.close();
            if (match > kMaximumWorkspaceFileBytes - windowOffset) {
                file.close();
                return {false, false, 0,
                        "Workspace search result exceeds the supported 32-bit file range"};
            }
            return {true, true, windowOffset + static_cast<std::uint32_t>(match), ""};
        }
        const std::size_t overlap = std::min(query.size() - 1, window.size());
        const std::size_t consumed = window.size() - overlap;
        if (consumed > kMaximumWorkspaceFileBytes - windowOffset) {
            file.close();
            return {false, false, 0,
                    "Workspace search offset exceeds the supported 32-bit file range"};
        }
        windowOffset += static_cast<std::uint32_t>(consumed);
        window.erase(0, consumed);
    }
    file.close();
    return {true, false, 0, ""};
}

WorkspaceBookmarkResult loadWorkspaceBookmark(const String& name)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, false, 0, "Invalid workspace filename"};
    }
    const BookmarkEntriesResult loaded = loadBookmarkEntries();
    if (!loaded.success) {
        return {false, false, 0, loaded.error};
    }
    for (const auto& entry : loaded.entries) {
        if (entry.name == name) {
            File file = SD.open(workspaceFilePath(name), FILE_READ);
            if (!file) {
                return {false, false, 0, "Bookmarked workspace file does not exist"};
            }
            const std::size_t totalBytes = file.size();
            const bool valid = entry.offset <= totalBytes &&
                isFileUtf8Boundary(file, entry.offset, totalBytes);
            file.close();
            return valid
                ? WorkspaceBookmarkResult{true, true, entry.offset, ""}
                : WorkspaceBookmarkResult{false, false, 0,
                                          "Stored bookmark is outside the file"};
        }
    }
    return {true, false, 0, ""};
}

OperationResult saveWorkspaceBookmark(const String& name, std::uint32_t offset)
{
    File file = SD.open(workspaceFilePath(name), FILE_READ);
    if (!file) {
        return {false, "Workspace file does not exist: " + name};
    }
    const std::size_t totalBytes = file.size();
    const bool valid = offset <= totalBytes && isFileUtf8Boundary(file, offset, totalBytes);
    file.close();
    if (!valid) {
        return {false, "Bookmark offset is outside the file or not a UTF-8 boundary"};
    }
    BookmarkEntriesResult loaded = loadBookmarkEntries();
    if (!loaded.success) {
        return {false, loaded.error};
    }
    for (auto& entry : loaded.entries) {
        if (entry.name == name) {
            entry.offset = offset;
            return saveBookmarkEntries(loaded.entries);
        }
    }
    if (loaded.entries.size() >= kMaximumWorkspaceFiles) {
        return {false, "Workspace bookmark limit of 4096 entries reached"};
    }
    loaded.entries.push_back({name, offset});
    return saveBookmarkEntries(loaded.entries);
}

OperationResult clearWorkspaceBookmark(const String& name)
{
    BookmarkEntriesResult loaded = loadBookmarkEntries();
    if (!loaded.success) {
        return {false, loaded.error};
    }
    const auto retained = std::remove_if(
        loaded.entries.begin(), loaded.entries.end(),
        [&name](const BookmarkEntry& entry) { return entry.name == name; });
    if (retained == loaded.entries.end()) {
        return {true, ""};
    }
    loaded.entries.erase(retained, loaded.entries.end());
    return saveBookmarkEntries(loaded.entries);
}

OperationResult createWorkspaceFile(const String& name)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, "Invalid workspace-relative path"};
    }
    const String path = workspaceFilePath(name);
    if (SD.exists(path)) {
        return {false, "Workspace file already exists: " + name};
    }
    const OperationResult parent = ensureWorkspaceParentDirectories(name);
    if (!parent.success) {
        return parent;
    }
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        return {false, "Failed to create workspace file: " + name};
    }
    file.close();
    return {true, ""};
}

OperationResult validateWorkspaceFileUtf8(const String& name)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, "Invalid workspace filename"};
    }
    const OperationResult textFile = requireWorkspaceTextFile(name);
    if (!textFile.success) {
        return textFile;
    }
    return validateWorkspaceFileUtf8Path(workspaceFilePath(name));
}

OperationResult replaceWorkspaceFileRange(const String& name,
                                          std::uint32_t offset,
                                          std::uint32_t originalBytes,
                                          const std::string& replacement)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, "Invalid workspace filename"};
    }
    const OperationResult textFile = requireWorkspaceTextFile(name);
    if (!textFile.success) {
        return textFile;
    }
    if (!isValidUtf8(replacement)) {
        return {false, "Replacement content must be valid UTF-8 text"};
    }
    const String target = workspaceFilePath(name);
    OperationResult result = prepareWorkspaceTarget(target);
    if (!result.success) {
        return result;
    }
    File source = SD.open(target, FILE_READ);
    if (!source) {
        return {false, "Workspace file does not exist: " + name};
    }
    const std::size_t totalBytesValue = source.size();
    if (totalBytesValue > kMaximumWorkspaceFileBytes) {
        source.close();
        return {false, "Workspace file exceeds the supported 32-bit file range"};
    }
    const std::uint32_t totalBytes = static_cast<std::uint32_t>(totalBytesValue);
    if (offset > totalBytes || originalBytes > totalBytes - offset) {
        source.close();
        return {false, "Edit range is outside the file"};
    }
    const std::uint32_t rangeEnd = offset + originalBytes;
    if (!isFileUtf8Boundary(source, offset, totalBytes) ||
        !isFileUtf8Boundary(source, rangeEnd, totalBytes)) {
        source.close();
        return {false, "Edit range is outside the file or splits a UTF-8 code point"};
    }
    const std::uint32_t retainedBytes = totalBytes - originalBytes;
    if (replacement.size() > kMaximumWorkspaceFileBytes - retainedBytes) {
        source.close();
        return {false, "Edited file would exceed the supported 32-bit file range"};
    }
    const std::uint32_t resultBytes = retainedBytes +
        static_cast<std::uint32_t>(replacement.size());
    result = checkSdOperationSpace(
        resultBytes, kStorageOperationalFloorBytes);
    if (!result.success) {
        source.close();
        return result;
    }
    const String temporary = target + ".tmp";
    const String backup = target + ".bak";
    File destination = SD.open(temporary, FILE_WRITE);
    if (!destination) {
        source.close();
        return {false, "Failed to create temporary file for atomic edit"};
    }
    source.seek(0);
    result = copyExactBytes(source, destination, offset, "Workspace edit prefix copy");
    if (result.success && !source.seek(rangeEnd)) {
        result = {false, "Workspace edit could not select the suffix"};
    }
    if (result.success && !replacement.empty()) {
        const std::size_t written = destination.write(
            reinterpret_cast<const std::uint8_t*>(replacement.data()), replacement.size());
        if (written != replacement.size()) {
            result = {false, "Workspace edit could not write the complete replacement"};
        }
    }
    if (result.success) {
        result = copyExactBytes(source, destination, totalBytes - rangeEnd,
                                "Workspace edit suffix copy");
    }
    destination.flush();
    source.close();
    destination.close();
    if (!result.success) {
        removeIfPresent(temporary);
        return result;
    }
    return commitTemporaryFile(target, temporary, backup);
}

OperationResult replaceWorkspaceFileWithTemporary(const String& name,
                                                   const String& temporaryName)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, "Invalid workspace filename"};
    }
    if (temporaryName != name + ".tmp") {
        return {false, "Workspace replacement must use the target .tmp path"};
    }
    const OperationResult textFile = requireWorkspaceTextFile(name);
    if (!textFile.success) {
        return textFile;
    }
    if (name == temporaryName) {
        return {false, "Replacement file must differ from the destination"};
    }
    const String target = workspaceFilePath(name);
    const String temporary = workspaceFilePath(temporaryName);
    if (!SD.exists(target)) {
        return {false, "Workspace file does not exist: " + name};
    }
    if (!SD.exists(temporary)) {
        return {false, "Temporary replacement file does not exist"};
    }
    const OperationResult valid = validateWorkspaceFileUtf8Path(temporary);
    if (!valid.success) {
        return valid;
    }
    const String backup = target + ".bak";
    if (SD.exists(backup)) {
        return {false, "Workspace replacement has an unresolved recovery file"};
    }
    const OperationResult committed = commitTemporaryFile(target, temporary, backup);
    if (!committed.success) {
        return committed;
    }
    const OperationResult bookmark = clearWorkspaceBookmark(name);
    return bookmark.success
        ? OperationResult{true, ""}
        : OperationResult{false, "File was replaced, but its old bookmark could not be cleared: " +
                                    bookmark.error};
}

OperationResult commitWorkspaceBinaryTemporary(const String& name,
                                                const String& temporaryName)
{
    if (!isValidWorkspaceFilename(name.c_str()) ||
        temporaryName != name + ".tmp") {
        return {false, "Workspace binary replacement paths are invalid"};
    }
    const String target = workspaceFilePath(name);
    const String temporary = workspaceFilePath(temporaryName);
    File file = SD.open(temporary, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        return {false, "Workspace binary replacement file could not be opened"};
    }
    const std::size_t replacementBytes = file.size();
    file.close();
    if (replacementBytes > kMaximumWorkspaceFileBytes) {
        return {false, "Workspace binary replacement exceeds the supported 32-bit file range"};
    }
    const String backup = target + ".bak";
    if (SD.exists(backup)) {
        return {false, "Workspace binary replacement has an unresolved recovery file"};
    }
    return commitTemporaryFile(target, temporary, backup);
}

OperationResult copyWorkspaceFile(const String& sourceName, const String& destinationName)
{
    if (!isValidWorkspaceFilename(sourceName.c_str()) ||
        !isValidWorkspaceFilename(destinationName.c_str())) {
        return {false, "Invalid source or destination workspace filename"};
    }
    const String source = workspaceFilePath(sourceName);
    const String destination = workspaceFilePath(destinationName);
    if (!SD.exists(source)) {
        return {false, "Workspace file does not exist: " + sourceName};
    }
    if (SD.exists(destination)) {
        return {false, "Workspace file already exists: " + destinationName};
    }
    OperationResult result = ensureWorkspaceParentDirectories(destinationName);
    if (!result.success) {
        return result;
    }
    const String temporary = destination + ".tmp";
    result = removeIfPresent(temporary);
    if (result.success) {
        result = copyFile(source, temporary);
    }
    if (!result.success) {
        removeIfPresent(temporary);
        return result;
    }
    if (!SD.rename(temporary, destination)) {
        removeIfPresent(temporary);
        return {false, "Failed to commit copied workspace file"};
    }
    const WorkspaceBookmarkResult bookmark = loadWorkspaceBookmark(sourceName);
    if (!bookmark.success) {
        return {false, "File was copied, but its bookmark could not be read: " + bookmark.error};
    }
    if (bookmark.found) {
        result = saveWorkspaceBookmark(destinationName, bookmark.offset);
        if (!result.success) {
            return {false, "File was copied, but its bookmark could not be copied: " + result.error};
        }
    }
    return {true, ""};
}

OperationResult renameWorkspaceFile(const String& sourceName, const String& destinationName)
{
    if (!isValidWorkspaceFilename(sourceName.c_str()) ||
        !isValidWorkspaceFilename(destinationName.c_str())) {
        return {false, "Invalid source or destination workspace filename"};
    }
    const String source = workspaceFilePath(sourceName);
    const String destination = workspaceFilePath(destinationName);
    if (!SD.exists(source)) {
        return {false, "Workspace file does not exist: " + sourceName};
    }
    const SharedFileLinkResult linked = sharedFileHasAnyProjectLink(sourceName);
    if (!linked.success) {
        return {false, linked.error};
    }
    if (linked.linked) {
        return {false, "Unlink the Shared file from every project before renaming it"};
    }
    if (SD.exists(destination)) {
        return {false, "Workspace file already exists: " + destinationName};
    }
    const OperationResult parent = ensureWorkspaceParentDirectories(destinationName);
    if (!parent.success) {
        return parent;
    }
    const WorkspaceBookmarkResult bookmark = loadWorkspaceBookmark(sourceName);
    if (!bookmark.success) {
        return {false, bookmark.error};
    }
    if (!SD.rename(source, destination)) {
        return {false, "Failed to rename workspace file"};
    }
    if (bookmark.found) {
        OperationResult result = saveWorkspaceBookmark(destinationName, bookmark.offset);
        if (!result.success) {
            return {false, "File was renamed, but its bookmark could not be moved: " + result.error};
        }
        result = clearWorkspaceBookmark(sourceName);
        if (!result.success) {
            return {false, "File was renamed, but its old bookmark could not be removed: " + result.error};
        }
    }
    return {true, ""};
}

OperationResult deleteWorkspaceFile(const String& name)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, "Invalid workspace filename"};
    }
    const String path = workspaceFilePath(name);
    if (!SD.exists(path)) {
        return {false, "Workspace file does not exist: " + name};
    }
    const SharedFileLinkResult linked = sharedFileHasAnyProjectLink(name);
    if (!linked.success) {
        return {false, linked.error};
    }
    if (linked.linked) {
        return {false, "Unlink the Shared file from every project before deleting it"};
    }
    OperationResult result = clearWorkspaceBookmark(name);
    if (!result.success) {
        return result;
    }
    return SD.remove(path)
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to delete workspace file: " + name};
}

static ToolExecutionResult executeWorkspaceToolControlled(
    const ToolCall& call,
    const std::function<bool()>& isCancelled)
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
        const ListFilesArgumentsResult parsed = parseListFilesArguments(
            arguments.as<JsonObjectConst>());
        return parsed.success
            ? listFilesTool(parsed.offset, parsed.maximumEntries)
            : toolFailure(parsed.error);
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
                                         : appendFileTool(name, content, isCancelled);
    }
    return toolFailure(String("Unsupported workspace tool: ") + call.name.c_str());
}

ToolExecutionResult executeWorkspaceTool(const ToolCall& call)
{
    const std::function<bool()> isCancelled = []() { return false; };
    return executeWorkspaceToolControlled(call, isCancelled);
}

ToolExecutionResult executeControlledWorkspaceTool(
    const ToolCall& call,
    const std::function<bool()>& isCancelled)
{
    bool cancelled = false;
    const std::function<bool()> latchedCancellation = [&]() {
        cancelled = cancelled || isCancelled();
        return cancelled;
    };
    if (latchedCancellation()) {
        return toolCancelled("Workspace tool canceled before execution");
    }
    return executeWorkspaceToolControlled(call, latchedCancellation);
}

WorkspaceWriteTargetResult inspectWorkspaceWriteTarget(const ToolCall& call)
{
    if (call.name != "write_file") {
        return {false, false, "", "Write target inspection requires write_file"};
    }
    JsonDocument arguments;
    const DeserializationError jsonError = deserializeJson(arguments, call.arguments);
    if (jsonError || !arguments.is<JsonObject>() ||
        arguments.as<JsonObjectConst>().size() != 2 ||
        !arguments["name"].is<const char*>() ||
        !arguments["content"].is<const char*>()) {
        return {false, false, "",
                "write_file requires exactly string fields 'name' and 'content'"};
    }
    const String name = arguments["name"].as<const char*>();
    const std::string content = arguments["content"].as<const char*>();
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, false, "", "Workspace tool path is invalid"};
    }
    const OperationResult textFile = requireWorkspaceTextFile(name);
    if (!textFile.success) {
        return {false, false, "", textFile.error};
    }
    if (content.size() > kMaximumWorkspaceToolChunkBytes) {
        return {false, false, "", "write_file content exceeds 12288 bytes"};
    }
    if (!isValidUtf8(content)) {
        return {false, false, "", "File content must be valid UTF-8 text"};
    }
    return {true, SD.exists(workspaceFilePath(name)), name, ""};
}

WorkspaceWriteTargetResult inspectProjectWorkspaceWriteTarget(
    const String& projectId,
    const ToolCall& call)
{
    if (!isValidChatId(projectId.c_str())) {
        return {false, false, "", "Workspace tool requires an active project"};
    }
    const WorkspaceWriteTargetResult target = inspectWorkspaceWriteTarget(call);
    if (!target.success) {
        return target;
    }
    const SharedFileLinkResult linked = projectHasSharedFileLink(
        projectId, target.name);
    if (!linked.success) {
        return {false, false, "", linked.error};
    }
    if (target.replacesExisting && !linked.linked) {
        return {false, false, "",
                "Existing Shared file is not linked to the active project: " +
                    target.name};
    }
    return {true, target.replacesExisting && linked.linked, target.name, ""};
}

static ToolExecutionResult executeProjectWorkspaceToolControlled(
    const String& projectId,
    const ToolCall& call,
    const std::function<bool()>& isCancelled)
{
    if (!isValidChatId(projectId.c_str())) {
        return toolFailure("Workspace tool requires an active project");
    }
    JsonDocument arguments;
    const DeserializationError jsonError = deserializeJson(arguments, call.arguments);
    if (jsonError) {
        return toolFailure(String("Tool arguments must be a JSON object: ") +
                           jsonError.c_str());
    }
    if (!arguments.is<JsonObject>()) {
        return toolFailure("Tool arguments must be a JSON object");
    }
    if (call.name == "list_files") {
        const ListFilesArgumentsResult parsed = parseListFilesArguments(
            arguments.as<JsonObjectConst>());
        return parsed.success
            ? listProjectFilesTool(projectId, parsed.offset, parsed.maximumEntries)
            : toolFailure(parsed.error);
    }
    if (!arguments["name"].is<const char*>()) {
        return toolFailure("Tool arguments are missing required string field 'name'");
    }
    const String name = arguments["name"].as<const char*>();
    if (!isValidWorkspaceFilename(name.c_str())) {
        return toolFailure("Workspace tool path is invalid");
    }
    const SharedFileLinkResult linked = projectHasSharedFileLink(projectId, name);
    if (!linked.success) {
        return toolFailure(linked.error);
    }
    const bool exists = SD.exists(workspaceFilePath(name));
    if (call.name != "write_file" && (!linked.linked || !exists)) {
        return toolFailure("Shared file is not linked to the active project: " + name);
    }
    if (call.name == "write_file" && exists && !linked.linked) {
        return toolFailure("Existing Shared file is not linked to the active project: " + name);
    }
    const ToolExecutionResult executed = executeWorkspaceToolControlled(
        call, isCancelled);
    if (!executed.success || call.name != "write_file" || linked.linked) {
        return executed;
    }
    const OperationResult linkResult = linkSharedFileToProject(projectId, name);
    if (linkResult.success) {
        return executed;
    }
    const OperationResult rollback = deleteWorkspaceFile(name);
    return toolFailure(rollback.success
        ? "File was created but project linking failed: " + linkResult.error
        : "File linking failed and created-file rollback also failed: " +
              linkResult.error + "; " + rollback.error);
}

ToolExecutionResult executeProjectWorkspaceTool(const String& projectId,
                                                const ToolCall& call)
{
    const std::function<bool()> isCancelled = []() { return false; };
    return executeProjectWorkspaceToolControlled(projectId, call, isCancelled);
}

ToolExecutionResult executeControlledProjectWorkspaceTool(
    const String& projectId,
    const ToolCall& call,
    const std::function<bool()>& isCancelled)
{
    bool cancelled = false;
    const std::function<bool()> latchedCancellation = [&]() {
        cancelled = cancelled || isCancelled();
        return cancelled;
    };
    if (latchedCancellation()) {
        return toolCancelled("Workspace tool canceled before execution");
    }
    return executeProjectWorkspaceToolControlled(
        projectId, call, latchedCancellation);
}

String workspaceFilePath(const String& name)
{
    return String(kWorkspaceDirectory) + "/" + name;
}

}  // namespace cardputer
