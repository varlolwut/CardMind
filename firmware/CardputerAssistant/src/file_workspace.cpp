#include "file_workspace.h"

#include "text_utils.h"

#include <ArduinoJson.h>
#include <SD.h>

#include <algorithm>
#include <limits>
#include <string>

namespace cardputer {
namespace {

constexpr const char* kWorkspaceDirectory = "/assistant/files";
constexpr const char* kBookmarksPath = "/assistant/file_bookmarks.json";
constexpr std::size_t kCopyBufferBytes = 1024;
constexpr std::size_t kMaximumSearchBytes = 128;

struct BookmarkEntry {
    String name;
    std::uint32_t offset;
};

struct BookmarkEntriesResult {
    bool success;
    std::vector<BookmarkEntry> entries;
    String error;
};

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

BookmarkEntriesResult loadBookmarkEntries()
{
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
            return {false, {}, "Workspace bookmark metadata contains more than 40 entries"};
        }
    }
    return {true, entries, ""};
}

OperationResult saveBookmarkEntries(const std::vector<BookmarkEntry>& entries)
{
    const String target = kBookmarksPath;
    const String temporary = target + ".tmp";
    const String backup = target + ".bak";
    OperationResult result = prepareTemporaryPaths(temporary, backup);
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

WorkspaceChunkResult readWorkspaceFileChunk(const String& name,
                                            std::uint32_t offset,
                                            std::size_t maximumBytes)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, "", 0, 0, 0, true,
                "Invalid filename; use ASCII letters, digits, ._- and a text extension"};
    }
    if (maximumBytes == 0 || maximumBytes > kMaximumWorkspaceToolChunkBytes) {
        return {false, "", 0, 0, 0, true,
                "Workspace read size must be between 1 and 12288 bytes"};
    }
    File file = SD.open(workspaceFilePath(name), FILE_READ);
    if (!file) {
        return {false, "", 0, 0, 0, true, "Workspace file does not exist: " + name};
    }
    const std::size_t totalBytes = file.size();
    if (totalBytes > kMaximumWorkspaceFileBytes) {
        file.close();
        return {false, "", 0, 0, 0, true,
                "Workspace file exceeds the 491520-byte size limit"};
    }
    if (offset > totalBytes || !isFileUtf8Boundary(file, offset, totalBytes) ||
        !file.seek(offset)) {
        file.close();
        return {false, "", 0, 0, 0, true,
                "Workspace offset is outside the file or not a UTF-8 boundary"};
    }
    const std::size_t availableBytes = totalBytes - static_cast<std::size_t>(offset);
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
    return {true, content, offset, nextOffset, static_cast<std::uint32_t>(totalBytes),
            nextOffset == totalBytes, ""};
}

WorkspaceFindResult findWorkspaceText(const String& name,
                                      const std::string& query,
                                      std::uint32_t startOffset)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, false, 0, "Invalid workspace filename"};
    }
    if (query.empty() || query.size() > kMaximumSearchBytes || !isValidUtf8(query)) {
        return {false, false, 0, "Search text must be valid UTF-8 between 1 and 128 bytes"};
    }
    File file = SD.open(workspaceFilePath(name), FILE_READ);
    if (!file) {
        return {false, false, 0, "Workspace file does not exist: " + name};
    }
    const std::size_t totalBytes = file.size();
    if (totalBytes > kMaximumWorkspaceFileBytes || startOffset > totalBytes ||
        !isFileUtf8Boundary(file, startOffset, totalBytes) || !file.seek(startOffset)) {
        file.close();
        return {false, false, 0,
                "Search offset is outside the file or not a UTF-8 boundary"};
    }
    std::string window;
    window.reserve(kCopyBufferBytes + query.size());
    std::uint32_t windowOffset = startOffset;
    std::uint8_t buffer[kCopyBufferBytes] = {};
    while (file.available() > 0) {
        const std::size_t readBytes = file.read(buffer, sizeof(buffer));
        if (readBytes == 0) {
            file.close();
            return {false, false, 0, "Workspace search stopped before end of file"};
        }
        window.append(reinterpret_cast<const char*>(buffer), readBytes);
        const std::size_t match = window.find(query);
        if (match != std::string::npos) {
            file.close();
            return {true, true, windowOffset + static_cast<std::uint32_t>(match), ""};
        }
        const std::size_t overlap = std::min(query.size() - 1, window.size());
        const std::size_t consumed = window.size() - overlap;
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
        return {false, "Workspace bookmark limit of 40 entries reached"};
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
        return {false, "Invalid filename; use ASCII letters, digits, ._- and a text extension"};
    }
    const String path = workspaceFilePath(name);
    if (SD.exists(path)) {
        return {false, "Workspace file already exists: " + name};
    }
    const WorkspaceFilesResult existing = listWorkspaceFiles();
    if (!existing.success) {
        return {false, existing.error};
    }
    if (existing.files.size() >= kMaximumWorkspaceFiles) {
        return {false, "Workspace already contains the maximum of 40 files"};
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
    File file = SD.open(workspaceFilePath(name), FILE_READ);
    if (!file) {
        return {false, "Workspace file does not exist: " + name};
    }
    if (file.size() > kMaximumWorkspaceFileBytes) {
        file.close();
        return {false, "Workspace file exceeds the 491520-byte size limit"};
    }
    constexpr std::size_t blockBytes = 4096;
    std::vector<std::uint8_t> block(blockBytes);
    std::string pending;
    while (file.available()) {
        const std::size_t readBytes = file.read(block.data(), block.size());
        if (readBytes == 0) {
            file.close();
            return {false, "microSD read stopped while validating UTF-8"};
        }
        pending.append(reinterpret_cast<const char*>(block.data()), readBytes);
        if (!file.available()) {
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

OperationResult replaceWorkspaceFileRange(const String& name,
                                          std::uint32_t offset,
                                          std::uint32_t originalBytes,
                                          const std::string& replacement)
{
    if (!isValidWorkspaceFilename(name.c_str())) {
        return {false, "Invalid workspace filename"};
    }
    if (!isValidUtf8(replacement)) {
        return {false, "Replacement content must be valid UTF-8 text"};
    }
    const String target = workspaceFilePath(name);
    File source = SD.open(target, FILE_READ);
    if (!source) {
        return {false, "Workspace file does not exist: " + name};
    }
    const std::size_t totalBytes = source.size();
    const std::size_t rangeEnd = static_cast<std::size_t>(offset) + originalBytes;
    if (offset > totalBytes || rangeEnd > totalBytes ||
        !isFileUtf8Boundary(source, offset, totalBytes) ||
        !isFileUtf8Boundary(source, rangeEnd, totalBytes)) {
        source.close();
        return {false, "Edit range is outside the file or splits a UTF-8 code point"};
    }
    const std::size_t resultBytes = totalBytes - originalBytes + replacement.size();
    if (resultBytes > kMaximumWorkspaceFileBytes) {
        source.close();
        return {false, "Edited file would exceed the 491520-byte size limit"};
    }
    const String temporary = target + ".tmp";
    const String backup = target + ".bak";
    OperationResult result = prepareTemporaryPaths(temporary, backup);
    if (!result.success) {
        source.close();
        return result;
    }
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
    const String temporary = destination + ".tmp";
    OperationResult result = removeIfPresent(temporary);
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
    if (SD.exists(destination)) {
        return {false, "Workspace file already exists: " + destinationName};
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
    OperationResult result = clearWorkspaceBookmark(name);
    if (!result.success) {
        return result;
    }
    return SD.remove(path)
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to delete workspace file: " + name};
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
