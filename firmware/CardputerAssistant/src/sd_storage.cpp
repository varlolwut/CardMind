#include "sd_storage.h"

#include "text_utils.h"

#include <SD.h>

#include <algorithm>
#include <string>

namespace cardputer {
namespace {

OperationResult removeSdFileIfPresent(const String& path)
{
    if (!SD.exists(path)) {
        return {true, ""};
    }
    if (!SD.remove(path)) {
        return {false, "Failed to remove stale storage file " + path};
    }
    return {true, ""};
}

OperationResult commitAtomicSdFile(const String& target, const String& temporary)
{
    const String recovery = target + ".bak";
    OperationResult result = removeSdFileIfPresent(recovery);
    if (!result.success) {
        return result;
    }
    const bool hadTarget = SD.exists(target);
    if (hadTarget && !SD.rename(target, recovery)) {
        return {false, "Failed to stage existing storage file for replacement: " + target};
    }
    if (!SD.rename(temporary, target)) {
        if (hadTarget && !SD.rename(recovery, target)) {
            return {false, "Failed to commit storage file and restore its recovery copy: " +
                               target};
        }
        return {false, "Failed to commit storage file: " + target};
    }
    if (hadTarget) {
        result = removeSdFileIfPresent(recovery);
        if (!result.success) {
            return result;
        }
    }
    return {true, ""};
}

OperationResult validateJsonlFile(const String& path, const String& keyField)
{
    File file = SD.open(path, FILE_READ);
    if (!file) {
        return {false, "Failed to reopen staged index for validation: " + path};
    }
    std::uint32_t lineNumber = 0;
    while (file.available()) {
        ++lineNumber;
        const String line = file.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        if (line.length() > kMaximumStorageIndexLineBytes) {
            file.close();
            return {false, "Storage index line exceeds 1024 bytes at line " +
                               String(lineNumber)};
        }
        JsonDocument document;
        const DeserializationError error = deserializeJson(document, line);
        if (error || !document[keyField].is<const char*>()) {
            file.close();
            return {false, "Storage index contains an invalid typed entry at line " +
                               String(lineNumber)};
        }
    }
    file.close();
    return {true, ""};
}

OperationResult validatePageOffset(File& file, std::uint32_t offset)
{
    const std::uint32_t totalBytes = file.size();
    if (offset > totalBytes) {
        return {false, "Storage index offset is outside the file"};
    }
    if (offset == 0 || offset == totalBytes) {
        return {true, ""};
    }
    if (!file.seek(offset - 1)) {
        return {false, "Failed to validate storage index offset"};
    }
    if (file.read() != '\n') {
        return {false, "Storage index offset does not point to an entry boundary"};
    }
    return file.seek(offset)
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to select storage index page"};
}

}  // namespace

OperationResult ensureSdDirectory(const String& path)
{
    if (path.isEmpty() || !path.startsWith("/")) {
        return {false, "SD directory path must be absolute"};
    }
    if (SD.exists(path)) {
        File directory = SD.open(path, FILE_READ);
        const bool valid = directory && directory.isDirectory();
        if (directory) {
            directory.close();
        }
        return valid ? OperationResult{true, ""}
                     : OperationResult{false, "SD path exists but is not a directory: " + path};
    }
    if (!SD.mkdir(path)) {
        return {false, "Failed to create SD directory: " + path};
    }
    return {true, ""};
}

OperationResult recoverAtomicSdFile(const String& target)
{
    const String temporary = target + ".tmp";
    const String recovery = target + ".bak";
    if (SD.exists(target)) {
        OperationResult result = removeSdFileIfPresent(temporary);
        return result.success ? removeSdFileIfPresent(recovery) : result;
    }
    if (SD.exists(recovery)) {
        if (!SD.rename(recovery, target)) {
            return {false, "Failed to recover interrupted storage file: " + target};
        }
    }
    return removeSdFileIfPresent(temporary);
}

OperationResult checkSdOperationSpace(std::uint64_t requiredBytes,
                                      std::uint64_t operationalFloorBytes)
{
    const std::uint64_t totalBytes = SD.totalBytes();
    const std::uint64_t usedBytes = SD.usedBytes();
    if (totalBytes == 0 || usedBytes > totalBytes) {
        return {false, "microSD capacity is unavailable or inconsistent"};
    }
    const std::uint64_t freeBytes = totalBytes - usedBytes;
    if (requiredBytes > freeBytes || operationalFloorBytes > freeBytes - requiredBytes) {
        return {false, "microSD has " + String(static_cast<unsigned long long>(freeBytes)) +
                           " free bytes; this operation requires " +
                           String(static_cast<unsigned long long>(requiredBytes)) +
                           " bytes plus a " +
                           String(static_cast<unsigned long long>(operationalFloorBytes)) +
                           "-byte recovery floor"};
    }
    return {true, ""};
}

OperationResult writeAtomicJsonSdFile(const String& target, JsonDocument& document)
{
    const std::size_t expectedBytes = measureJson(document);
    OperationResult result = checkSdOperationSpace(expectedBytes, 1048576);
    if (!result.success) {
        return result;
    }
    result = recoverAtomicSdFile(target);
    if (!result.success) {
        return result;
    }
    const String temporary = target + ".tmp";
    File file = SD.open(temporary, FILE_WRITE);
    if (!file) {
        return {false, "Failed to create staged JSON file: " + temporary};
    }
    const std::size_t writtenBytes = serializeJson(document, file);
    file.flush();
    file.close();
    if (writtenBytes != expectedBytes) {
        removeSdFileIfPresent(temporary);
        return {false, "Failed to write complete staged JSON file: " + temporary};
    }
    File validation = SD.open(temporary, FILE_READ);
    if (!validation) {
        removeSdFileIfPresent(temporary);
        return {false, "Failed to reopen staged JSON file: " + temporary};
    }
    JsonDocument reopened;
    const DeserializationError error = deserializeJson(reopened, validation);
    validation.close();
    if (error) {
        removeSdFileIfPresent(temporary);
        return {false, "Staged JSON file failed validation: " + String(error.c_str())};
    }
    return commitAtomicSdFile(target, temporary);
}

OperationResult writeEmptyAtomicSdFile(const String& target)
{
    OperationResult result = checkSdOperationSpace(0, 1048576);
    if (!result.success) {
        return result;
    }
    result = recoverAtomicSdFile(target);
    if (!result.success) {
        return result;
    }
    const String temporary = target + ".tmp";
    File file = SD.open(temporary, FILE_WRITE);
    if (!file) {
        return {false, "Failed to create staged empty file: " + temporary};
    }
    file.flush();
    file.close();
    return commitAtomicSdFile(target, temporary);
}

OperationResult commitStagedSdFile(const String& target, const String& staged)
{
    if (!SD.exists(staged)) {
        return {false, "Staged SD file does not exist: " + staged};
    }
    const String expectedTemporary = target + ".tmp";
    if (staged != expectedTemporary) {
        return {false, "Staged SD file must use the target .tmp path"};
    }
    return commitAtomicSdFile(target, staged);
}

OperationResult copySdFileAtomically(const String& source,
                                     const String& target,
                                     std::uint64_t operationalFloorBytes)
{
    File input = SD.open(source, FILE_READ);
    if (!input || input.isDirectory()) {
        if (input) {
            input.close();
        }
        return {false, "Source SD file does not exist: " + source};
    }
    OperationResult result = checkSdOperationSpace(input.size(), operationalFloorBytes);
    if (!result.success) {
        input.close();
        return result;
    }
    result = recoverAtomicSdFile(target);
    if (!result.success) {
        input.close();
        return result;
    }
    const String staged = target + ".tmp";
    File output = SD.open(staged, FILE_WRITE);
    if (!output) {
        input.close();
        return {false, "Failed to create staged SD copy: " + staged};
    }
    std::uint8_t buffer[4096];
    std::uint64_t copiedBytes = 0;
    while (input.available()) {
        const std::size_t readBytes = input.read(buffer, sizeof(buffer));
        if (readBytes == 0) {
            result = {false, "SD source read stopped before EOF: " + source};
            break;
        }
        if (output.write(buffer, readBytes) != readBytes) {
            result = {false, "SD destination write stopped before the complete copy: " + target};
            break;
        }
        copiedBytes += readBytes;
    }
    const std::uint64_t expectedBytes = input.size();
    output.flush();
    input.close();
    output.close();
    if (result.success && copiedBytes != expectedBytes) {
        result = {false, "SD copy byte count does not match the source"};
    }
    if (!result.success) {
        removeSdFileIfPresent(staged);
        return result;
    }
    return commitAtomicSdFile(target, staged);
}

OperationResult mutateJsonlSdIndex(const String& path,
                                   const String& keyField,
                                   const String& keyValue,
                                   const String& replacementLine,
                                   bool removeEntry,
                                   std::uint64_t operationalFloorBytes)
{
    if (keyField.isEmpty() || keyValue.isEmpty()) {
        return {false, "Storage index mutation requires a key field and value"};
    }
    if (!removeEntry && (replacementLine.isEmpty() ||
                         replacementLine.length() > kMaximumStorageIndexLineBytes)) {
        return {false, "Storage index replacement must contain 1 to 1024 bytes"};
    }
    std::uint64_t existingBytes = 0;
    File existing = SD.open(path, FILE_READ);
    if (existing) {
        existingBytes = existing.size();
        existing.close();
    }
    OperationResult result = checkSdOperationSpace(
        existingBytes + replacementLine.length() + 1, operationalFloorBytes);
    if (!result.success) {
        return result;
    }
    result = recoverAtomicSdFile(path);
    if (!result.success) {
        return result;
    }
    const String temporary = path + ".tmp";
    File source = SD.open(path, FILE_READ);
    File destination = SD.open(temporary, FILE_WRITE);
    if (!destination) {
        if (source) {
            source.close();
        }
        return {false, "Failed to create staged storage index: " + temporary};
    }
    bool found = false;
    std::uint32_t lineNumber = 0;
    while (source && source.available()) {
        ++lineNumber;
        const String line = source.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        if (line.length() > kMaximumStorageIndexLineBytes) {
            source.close();
            destination.close();
            removeSdFileIfPresent(temporary);
            return {false, "Storage index line exceeds 1024 bytes at line " +
                               String(lineNumber)};
        }
        JsonDocument item;
        const DeserializationError error = deserializeJson(item, line);
        if (error || !item[keyField].is<const char*>()) {
            source.close();
            destination.close();
            removeSdFileIfPresent(temporary);
            return {false, "Storage index contains an invalid typed entry at line " +
                               String(lineNumber)};
        }
        const bool matches = String(item[keyField].as<const char*>()) == keyValue;
        if (matches) {
            if (found) {
                source.close();
                destination.close();
                removeSdFileIfPresent(temporary);
                return {false, "Storage index contains duplicate key " + keyValue};
            }
            found = true;
            if (!removeEntry &&
                (destination.print(replacementLine) != replacementLine.length() ||
                 destination.write('\n') != 1)) {
                source.close();
                destination.close();
                removeSdFileIfPresent(temporary);
                return {false, "Failed to replace storage index entry " + keyValue};
            }
            continue;
        }
        if (destination.print(line) != line.length() || destination.write('\n') != 1) {
            source.close();
            destination.close();
            removeSdFileIfPresent(temporary);
            return {false, "Failed while copying storage index"};
        }
    }
    if (source) {
        source.close();
    }
    if (!found && !removeEntry &&
        (destination.print(replacementLine) != replacementLine.length() ||
         destination.write('\n') != 1)) {
        destination.close();
        removeSdFileIfPresent(temporary);
        return {false, "Failed to append storage index entry " + keyValue};
    }
    destination.flush();
    destination.close();
    result = validateJsonlFile(temporary, keyField);
    if (!result.success) {
        removeSdFileIfPresent(temporary);
        return result;
    }
    return commitAtomicSdFile(path, temporary);
}

StorageLinesPageResult readJsonlSdIndexPage(const String& path,
                                            std::uint32_t offset,
                                            std::size_t maximumEntries)
{
    if (maximumEntries == 0) {
        return {false, {}, offset, false, "Storage index page size must be greater than zero"};
    }
    File file = SD.open(path, FILE_READ);
    if (!file) {
        return {false, {}, offset, false, "Failed to open storage index: " + path};
    }
    OperationResult result = validatePageOffset(file, offset);
    if (!result.success) {
        file.close();
        return {false, {}, offset, false, result.error};
    }
    if (!file.seek(offset)) {
        file.close();
        return {false, {}, offset, false, "Failed to select storage index offset"};
    }
    std::vector<String> lines;
    lines.reserve(maximumEntries);
    while (file.available() && lines.size() < maximumEntries) {
        const String line = file.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        if (line.length() > kMaximumStorageIndexLineBytes) {
            file.close();
            return {false, {}, offset, false, "Storage index line exceeds 1024 bytes"};
        }
        lines.push_back(line);
    }
    const std::uint32_t nextOffset = file.position();
    const bool eof = !file.available();
    file.close();
    return {true, std::move(lines), nextOffset, eof, ""};
}

StorageIndexLookupResult findJsonlSdIndexEntry(const String& path,
                                               const String& keyField,
                                               const String& keyValue)
{
    File file = SD.open(path, FILE_READ);
    if (!file) {
        return {false, false, "", "Failed to open storage index: " + path};
    }
    std::uint32_t lineNumber = 0;
    while (file.available()) {
        ++lineNumber;
        const String line = file.readStringUntil('\n');
        if (line.isEmpty()) {
            continue;
        }
        if (line.length() > kMaximumStorageIndexLineBytes) {
            file.close();
            return {false, false, "", "Storage index line exceeds 1024 bytes at line " +
                                         String(lineNumber)};
        }
        JsonDocument item;
        const DeserializationError error = deserializeJson(item, line);
        if (error || !item[keyField].is<const char*>()) {
            file.close();
            return {false, false, "", "Storage index contains an invalid typed entry at line " +
                                         String(lineNumber)};
        }
        if (String(item[keyField].as<const char*>()) == keyValue) {
            file.close();
            return {true, true, line, ""};
        }
    }
    file.close();
    return {true, false, "", ""};
}

OperationResult removeSdDirectoryTree(const String& path)
{
    File directory = SD.open(path, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) {
            directory.close();
        }
        return {false, "SD directory does not exist: " + path};
    }
    File entry = directory.openNextFile();
    while (entry) {
        const String entryPath = entry.path();
        const bool isDirectory = entry.isDirectory();
        entry.close();
        const OperationResult result = isDirectory
            ? removeSdDirectoryTree(entryPath)
            : (SD.remove(entryPath)
                   ? OperationResult{true, ""}
                   : OperationResult{false, "Failed to remove SD file: " + entryPath});
        if (!result.success) {
            directory.close();
            return result;
        }
        entry = directory.openNextFile();
    }
    directory.close();
    return SD.rmdir(path)
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to remove SD directory: " + path};
}

}  // namespace cardputer
