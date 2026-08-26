#include "sd_storage.h"

#include "storage.h"
#include "text_utils.h"

#include <SD.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <string>

namespace cardputer {
namespace {

constexpr std::uint32_t kFnvOffsetBasis = 2166136261U;
constexpr std::uint32_t kFnvPrime = 16777619U;
constexpr std::size_t kMaximumJsonFieldNameBytes = 64;
constexpr std::size_t kMaximumJsonNestingDepth = 16;
constexpr const char* kAssistantDirectory = "/assistant";
constexpr const char* kSdVolumeIdentityPath = "/assistant/.cardmind-volume-id";
constexpr const char* kSdVolumeIdentityStagedPath = "/assistant/.cardmind-volume-id.tmp";
constexpr std::size_t kSdVolumeIdentityBytes = 16;

bool sdWasReady = false;
bool sdRestartRequired = false;
bool sdFaultOverrideEnabled = false;
SdStorageState sdFaultOverrideState = SdStorageState::Ready;
bool expectedSdVolumeIdentityLoaded = false;
String expectedSdVolumeIdentity;
String expectedSdVolumeIdentityError;

struct SdVolumeIdentityResult {
    bool success;
    bool found;
    String identity;
    String error;
};

bool isValidSdVolumeIdentity(const String& identity)
{
    if (identity.length() != kSdVolumeIdentityBytes) {
        return false;
    }
    for (std::size_t index = 0; index < identity.length(); ++index) {
        const char character = identity[index];
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

String generateSdVolumeIdentity()
{
    char value[kSdVolumeIdentityBytes + 1] = {};
    snprintf(value, sizeof(value), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));
    return String(value);
}

OperationResult loadExpectedSdVolumeIdentity()
{
    if (expectedSdVolumeIdentityLoaded) {
        return expectedSdVolumeIdentityError.isEmpty()
            ? OperationResult{true, ""}
            : OperationResult{false, expectedSdVolumeIdentityError};
    }
    String loaded;
    const OperationResult result = loadSdVolumeIdentity(loaded);
    expectedSdVolumeIdentityLoaded = true;
    expectedSdVolumeIdentity = result.success ? loaded : String();
    expectedSdVolumeIdentityError = result.success ? String() : result.error;
    return result;
}

SdVolumeIdentityResult readSdVolumeIdentity(const String& path)
{
    if (!SD.exists(path)) {
        return {true, false, "", ""};
    }
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        return {false, false, "", "Failed to open the microSD identity marker"};
    }
    if (file.size() != kSdVolumeIdentityBytes) {
        file.close();
        return {false, true, "", "The microSD identity marker has an invalid size"};
    }
    char value[kSdVolumeIdentityBytes + 1] = {};
    const std::size_t readBytes = file.read(
        reinterpret_cast<std::uint8_t*>(value), kSdVolumeIdentityBytes);
    const bool eof = file.position() == file.size();
    file.close();
    const String identity(value);
    if (readBytes != kSdVolumeIdentityBytes || !eof ||
        !isValidSdVolumeIdentity(identity)) {
        return {false, true, "", "The microSD identity marker is invalid"};
    }
    return {true, true, identity, ""};
}

OperationResult ensureIdentityDirectory()
{
    if (SD.exists(kAssistantDirectory)) {
        File directory = SD.open(kAssistantDirectory, FILE_READ);
        const bool valid = directory && directory.isDirectory();
        if (directory) directory.close();
        return valid
            ? OperationResult{true, ""}
            : OperationResult{false, "/assistant exists but is not a directory"};
    }
    return SD.mkdir(kAssistantDirectory)
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to create /assistant for the microSD identity marker"};
}

OperationResult writeSdVolumeIdentityMarker(const String& identity)
{
    if (!isValidSdVolumeIdentity(identity)) {
        return {false, "Cannot write an invalid microSD identity marker"};
    }
    const OperationResult directory = ensureIdentityDirectory();
    if (!directory.success) return directory;

    if (SD.exists(kSdVolumeIdentityStagedPath)) {
        const SdVolumeIdentityResult staged = readSdVolumeIdentity(
            kSdVolumeIdentityStagedPath);
        if (!staged.success || !staged.found || staged.identity != identity) {
            return {false, "A conflicting staged microSD identity marker already exists"};
        }
        if (!SD.rename(kSdVolumeIdentityStagedPath, kSdVolumeIdentityPath)) {
            return {false, "Failed to recover the staged microSD identity marker"};
        }
    } else {
        File staged = SD.open(kSdVolumeIdentityStagedPath, FILE_WRITE);
        if (!staged) {
            return {false, "Failed to create the staged microSD identity marker"};
        }
        const std::size_t written = staged.write(
            reinterpret_cast<const std::uint8_t*>(identity.c_str()), identity.length());
        staged.flush();
        staged.close();
        if (written != identity.length()) {
            return {false, "Failed to write the complete staged microSD identity marker"};
        }
        const SdVolumeIdentityResult verified = readSdVolumeIdentity(
            kSdVolumeIdentityStagedPath);
        if (!verified.success || !verified.found || verified.identity != identity) {
            return {false, verified.success
                ? String("Staged microSD identity verification failed")
                : verified.error};
        }
        if (!SD.rename(kSdVolumeIdentityStagedPath, kSdVolumeIdentityPath)) {
            return {false, "Failed to commit the microSD identity marker"};
        }
    }
    const SdVolumeIdentityResult committed = readSdVolumeIdentity(kSdVolumeIdentityPath);
    return committed.success && committed.found && committed.identity == identity
        ? OperationResult{true, ""}
        : OperationResult{false, committed.success
            ? String("Committed microSD identity verification failed")
            : committed.error};
}

SdStorageStatus faultStatus(SdStorageState state)
{
    switch (state) {
        case SdStorageState::Missing:
            return {state, 0, 0, "microSD required: no card is present"};
        case SdStorageState::Full:
            return {state, SD.totalBytes(), SD.usedBytes(),
                    "microSD is full or below the operational free-space floor"};
        case SdStorageState::Removed:
            return {state, 0, 0, "microSD was removed; reinsert the expected card"};
        case SdStorageState::Replaced:
            return {state, SD.totalBytes(), SD.usedBytes(),
                    "microSD was unexpectedly replaced; confirm this workspace before writing"};
        case SdStorageState::Ready:
            return {state, SD.totalBytes(), SD.usedBytes(), ""};
    }
    return {SdStorageState::Removed, 0, 0, "microSD state is invalid"};
}

OperationResult requireExistingSdVolumeAccess()
{
    if (sdFaultOverrideEnabled) {
        const SdStorageStatus status = faultStatus(sdFaultOverrideState);
        return status.state == SdStorageState::Ready || status.state == SdStorageState::Full
            ? OperationResult{true, ""}
            : OperationResult{false, status.error};
    }
    if (sdRestartRequired) {
        return {false,
                "microSD replacement confirmed; restart CardMind to initialize the workspace"};
    }
    if (SD.cardType() == CARD_NONE) {
        return {false, faultStatus(sdWasReady ? SdStorageState::Removed
                                              : SdStorageState::Missing).error};
    }
    File root = SD.open("/", FILE_READ);
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return {false, faultStatus(sdWasReady ? SdStorageState::Removed
                                              : SdStorageState::Missing).error};
    }
    root.close();
    const OperationResult expected = loadExpectedSdVolumeIdentity();
    if (!expected.success) return expected;
    const SdVolumeIdentityResult marker = readSdVolumeIdentity(kSdVolumeIdentityPath);
    if (!marker.success) {
        return {false, marker.error + "; confirm this workspace before writing"};
    }
    if (!marker.found || expectedSdVolumeIdentity.isEmpty() ||
        marker.identity != expectedSdVolumeIdentity) {
        return {false, faultStatus(SdStorageState::Replaced).error};
    }
    sdWasReady = true;
    return {true, ""};
}

bool isJsonWhitespace(int value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

void skipJsonWhitespace(File& file)
{
    while (file.available() && isJsonWhitespace(file.peek())) {
        file.read();
    }
}

int hexadecimalValue(int value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

OperationResult appendJsonCodePoint(std::string& output,
                                    std::uint32_t codePoint,
                                    std::size_t maximumBytes)
{
    if (codePoint == 0 || codePoint > 0x10FFFFU ||
        (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
        return {false, "JSON string contains an unsupported Unicode code point"};
    }
    std::uint8_t encoded[4] = {};
    std::size_t encodedBytes = 0;
    if (codePoint <= 0x7FU) {
        encoded[0] = static_cast<std::uint8_t>(codePoint);
        encodedBytes = 1;
    } else if (codePoint <= 0x7FFU) {
        encoded[0] = static_cast<std::uint8_t>(0xC0U | (codePoint >> 6));
        encoded[1] = static_cast<std::uint8_t>(0x80U | (codePoint & 0x3FU));
        encodedBytes = 2;
    } else if (codePoint <= 0xFFFFU) {
        encoded[0] = static_cast<std::uint8_t>(0xE0U | (codePoint >> 12));
        encoded[1] = static_cast<std::uint8_t>(0x80U | ((codePoint >> 6) & 0x3FU));
        encoded[2] = static_cast<std::uint8_t>(0x80U | (codePoint & 0x3FU));
        encodedBytes = 3;
    } else {
        encoded[0] = static_cast<std::uint8_t>(0xF0U | (codePoint >> 18));
        encoded[1] = static_cast<std::uint8_t>(0x80U | ((codePoint >> 12) & 0x3FU));
        encoded[2] = static_cast<std::uint8_t>(0x80U | ((codePoint >> 6) & 0x3FU));
        encoded[3] = static_cast<std::uint8_t>(0x80U | (codePoint & 0x3FU));
        encodedBytes = 4;
    }
    if (encodedBytes > maximumBytes - std::min(maximumBytes, output.size())) {
        return {false, "JSON string field exceeds its byte limit"};
    }
    output.append(reinterpret_cast<const char*>(encoded), encodedBytes);
    return {true, ""};
}

struct JsonCodeUnitResult {
    bool success;
    std::uint16_t value;
    String error;
};

struct JsonStringLengthResult {
    bool success;
    std::size_t bytes;
    String error;
};

JsonCodeUnitResult readJsonCodeUnit(File& file)
{
    std::uint16_t value = 0;
    for (std::size_t index = 0; index < 4; ++index) {
        if (!file.available()) {
            return {false, 0, "JSON Unicode escape ended before four digits"};
        }
        const int digit = hexadecimalValue(file.read());
        if (digit < 0) {
            return {false, 0, "JSON Unicode escape contains a non-hexadecimal digit"};
        }
        value = static_cast<std::uint16_t>((value << 4) | digit);
    }
    return {true, value, ""};
}

std::size_t utf8BytesForJsonCodePoint(std::uint32_t codePoint)
{
    if (codePoint == 0 || codePoint > 0x10FFFFU ||
        (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
        return 0;
    }
    if (codePoint <= 0x7FU) return 1;
    if (codePoint <= 0x7FFU) return 2;
    if (codePoint <= 0xFFFFU) return 3;
    return 4;
}

JsonStringLengthResult measureDecodedJsonString(File& file,
                                                std::size_t maximumBytes)
{
    if (!file.available() || file.read() != '"') {
        return {false, 0, "Expected a JSON string"};
    }
    std::size_t bytes = 0;
    while (file.available()) {
        const int value = file.read();
        if (value == '"') {
            return {true, bytes, ""};
        }
        if (value >= 0 && value < 0x20) {
            return {false, 0, "JSON string contains an unescaped control character"};
        }
        std::size_t decodedBytes = 1;
        if (value == '\\') {
            if (!file.available()) {
                return {false, 0, "JSON string ends after an escape marker"};
            }
            const int escaped = file.read();
            if (escaped == 'u') {
                const JsonCodeUnitResult first = readJsonCodeUnit(file);
                if (!first.success) return {false, 0, first.error};
                std::uint32_t codePoint = first.value;
                if (first.value >= 0xD800U && first.value <= 0xDBFFU) {
                    if (!file.available() || file.read() != '\\' ||
                        !file.available() || file.read() != 'u') {
                        return {false, 0, "JSON high surrogate is missing its low surrogate"};
                    }
                    const JsonCodeUnitResult second = readJsonCodeUnit(file);
                    if (!second.success) return {false, 0, second.error};
                    if (second.value < 0xDC00U || second.value > 0xDFFFU) {
                        return {false, 0,
                                "JSON high surrogate is followed by an invalid low surrogate"};
                    }
                    codePoint = 0x10000U +
                        ((static_cast<std::uint32_t>(first.value) - 0xD800U) << 10) +
                        (static_cast<std::uint32_t>(second.value) - 0xDC00U);
                }
                decodedBytes = utf8BytesForJsonCodePoint(codePoint);
                if (decodedBytes == 0) {
                    return {false, 0, "JSON string contains an unsupported Unicode code point"};
                }
            } else if (escaped != '"' && escaped != '\\' && escaped != '/' &&
                       escaped != 'b' && escaped != 'f' && escaped != 'n' &&
                       escaped != 'r' && escaped != 't') {
                return {false, 0, "JSON string contains an invalid escape sequence"};
            }
        }
        if (decodedBytes > maximumBytes - std::min(maximumBytes, bytes)) {
            return {false, 0, "JSON string field exceeds its byte limit"};
        }
        bytes += decodedBytes;
    }
    return {false, 0, "JSON string ended before its closing quote"};
}

OperationResult readDecodedJsonString(File& file,
                                      std::string& output,
                                      std::size_t maximumBytes)
{
    if (!file.available() || file.read() != '"') {
        return {false, "Expected a JSON string"};
    }
    while (file.available()) {
        const int value = file.read();
        if (value == '"') {
            return isValidUtf8(output)
                ? OperationResult{true, ""}
                : OperationResult{false, "JSON string contains invalid UTF-8"};
        }
        if (value >= 0 && value < 0x20) {
            return {false, "JSON string contains an unescaped control character"};
        }
        if (value != '\\') {
            if (output.size() >= maximumBytes) {
                return {false, "JSON string field exceeds its byte limit"};
            }
            output.push_back(static_cast<char>(value));
            continue;
        }
        if (!file.available()) {
            return {false, "JSON string ends after an escape marker"};
        }
        const int escaped = file.read();
        char decoded = 0;
        switch (escaped) {
            case '"': decoded = '"'; break;
            case '\\': decoded = '\\'; break;
            case '/': decoded = '/'; break;
            case 'b': decoded = '\b'; break;
            case 'f': decoded = '\f'; break;
            case 'n': decoded = '\n'; break;
            case 'r': decoded = '\r'; break;
            case 't': decoded = '\t'; break;
            case 'u': {
                const JsonCodeUnitResult first = readJsonCodeUnit(file);
                if (!first.success) return {false, first.error};
                std::uint32_t codePoint = first.value;
                if (first.value >= 0xD800U && first.value <= 0xDBFFU) {
                    if (!file.available() || file.read() != '\\' ||
                        !file.available() || file.read() != 'u') {
                        return {false, "JSON high surrogate is missing its low surrogate"};
                    }
                    const JsonCodeUnitResult second = readJsonCodeUnit(file);
                    if (!second.success) return {false, second.error};
                    if (second.value < 0xDC00U || second.value > 0xDFFFU) {
                        return {false, "JSON high surrogate is followed by an invalid low surrogate"};
                    }
                    codePoint = 0x10000U +
                        ((static_cast<std::uint32_t>(first.value) - 0xD800U) << 10) +
                        (static_cast<std::uint32_t>(second.value) - 0xDC00U);
                }
                const OperationResult appended = appendJsonCodePoint(
                    output, codePoint, maximumBytes);
                if (!appended.success) return appended;
                continue;
            }
            default:
                return {false, "JSON string contains an invalid escape sequence"};
        }
        if (output.size() >= maximumBytes) {
            return {false, "JSON string field exceeds its byte limit"};
        }
        output.push_back(decoded);
    }
    return {false, "JSON string ended before its closing quote"};
}

OperationResult skipJsonString(File& file)
{
    if (!file.available() || file.read() != '"') {
        return {false, "Expected a JSON string while skipping a value"};
    }
    bool escaped = false;
    while (file.available()) {
        const int value = file.read();
        if (escaped) {
            escaped = false;
        } else if (value == '\\') {
            escaped = true;
        } else if (value == '"') {
            return {true, ""};
        } else if (value >= 0 && value < 0x20) {
            return {false, "JSON string contains an unescaped control character"};
        }
    }
    return {false, "JSON string ended before its closing quote"};
}

OperationResult skipJsonValue(File& file)
{
    skipJsonWhitespace(file);
    if (!file.available()) return {false, "JSON value is missing"};
    if (file.peek() == '"') return skipJsonString(file);
    if (file.peek() != '{' && file.peek() != '[') {
        bool consumed = false;
        while (file.available()) {
            const int value = file.peek();
            if (value == ',' || value == '}' || value == ']' || isJsonWhitespace(value)) break;
            file.read();
            consumed = true;
        }
        return consumed ? OperationResult{true, ""}
                        : OperationResult{false, "JSON primitive value is missing"};
    }
    char closing[kMaximumJsonNestingDepth] = {};
    std::size_t depth = 0;
    while (file.available()) {
        const int value = file.read();
        if (value == '"') {
            if (!file.seek(file.position() - 1)) {
                return {false, "Failed to rewind while skipping a JSON string"};
            }
            const OperationResult skipped = skipJsonString(file);
            if (!skipped.success) return skipped;
            continue;
        }
        if (value == '{' || value == '[') {
            if (depth >= kMaximumJsonNestingDepth) {
                return {false, "JSON value exceeds the supported nesting depth"};
            }
            closing[depth++] = value == '{' ? '}' : ']';
        } else if (value == '}' || value == ']') {
            if (depth == 0 || closing[depth - 1] != value) {
                return {false, "JSON value contains mismatched brackets"};
            }
            --depth;
            if (depth == 0) return {true, ""};
        }
    }
    return {false, "JSON container ended before its closing bracket"};
}

std::uint32_t updateFnv1a(std::uint32_t hash,
                          const std::uint8_t* data,
                          std::size_t size)
{
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= data[index];
        hash *= kFnvPrime;
    }
    return hash;
}

class HashingFilePrint : public Print {
public:
    explicit HashingFilePrint(File& file) : file_(file), hash_(kFnvOffsetBasis) {}

    std::size_t write(std::uint8_t value) override
    {
        const std::size_t written = file_.write(value);
        if (written == 1) {
            hash_ = updateFnv1a(hash_, &value, 1);
        }
        return written;
    }

    std::size_t write(const std::uint8_t* buffer, std::size_t size) override
    {
        const std::size_t written = file_.write(buffer, size);
        hash_ = updateFnv1a(hash_, buffer, written);
        return written;
    }

    std::uint32_t hash() const
    {
        return hash_;
    }

private:
    File& file_;
    std::uint32_t hash_;
};

OperationResult removeSdFileIfPresent(const String& path)
{
    const OperationResult access = requireSdCleanupAccess();
    if (!access.success) return access;
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
    OperationResult access = requireSdCleanupAccess();
    if (!access.success) return access;
    const String recovery = target + ".bak";
    OperationResult result = removeSdFileIfPresent(recovery);
    if (!result.success) {
        const OperationResult cleaned = removeSdFileIfPresent(temporary);
        return cleaned.success
            ? result
            : OperationResult{false, result.error + "; " + cleaned.error};
    }
    const bool hadTarget = SD.exists(target);
    if (hadTarget) {
        access = requireSdCleanupAccess();
        if (!access.success) return access;
        if (!SD.rename(target, recovery)) {
            const OperationResult cleaned = removeSdFileIfPresent(temporary);
            const String error =
                "Failed to stage existing storage file for replacement: " + target;
            return cleaned.success
                ? OperationResult{false, error}
                : OperationResult{false, error + "; " + cleaned.error};
        }
    }
    access = requireSdCleanupAccess();
    if (!access.success) return access;
    if (!SD.rename(temporary, target)) {
        const OperationResult restoreAccess = requireSdCleanupAccess();
        if (!restoreAccess.success) return restoreAccess;
        if (hadTarget && !SD.rename(recovery, target)) {
            return {false, "Failed to commit storage file and restore its recovery copy: " +
                               target};
        }
        result = removeSdFileIfPresent(temporary);
        return result.success
            ? OperationResult{false, "Failed to commit storage file: " + target}
            : OperationResult{
                false,
                "Failed to commit storage file: " + target +
                    (hadTarget ? " after restoring its recovery copy; " : "; ") +
                    result.error};
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

OperationResult initializeSdStorageIdentity()
{
    if (SD.cardType() == CARD_NONE) {
        return {false, "microSD required: no card is present"};
    }
    File root = SD.open("/", FILE_READ);
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return {false, "microSD is mounted but its root directory is unavailable"};
    }
    root.close();

    const OperationResult expected = loadExpectedSdVolumeIdentity();
    if (!expected.success) return expected;
    const SdVolumeIdentityResult marker = readSdVolumeIdentity(kSdVolumeIdentityPath);
    if (!marker.success) {
        return {false, marker.error +
            "; confirm this workspace before writing to the card"};
    }
    if (marker.found) {
        if (!expectedSdVolumeIdentity.isEmpty() &&
            marker.identity != expectedSdVolumeIdentity) {
            return {false,
                    "microSD was unexpectedly replaced; confirm this workspace before writing"};
        }
        if (expectedSdVolumeIdentity.isEmpty()) {
            const OperationResult saved = saveSdVolumeIdentity(marker.identity);
            if (!saved.success) return saved;
            expectedSdVolumeIdentity = marker.identity;
        }
        sdWasReady = true;
        return {true, ""};
    }
    if (!expectedSdVolumeIdentity.isEmpty()) {
        return {false,
                "microSD was unexpectedly replaced and has no CardMind identity marker; "
                "confirm this workspace before writing"};
    }

    const std::uint64_t totalBytes = SD.totalBytes();
    const std::uint64_t usedBytes = SD.usedBytes();
    if (totalBytes == 0 || usedBytes > totalBytes) {
        return {false, "microSD capacity could not be read before identity initialization"};
    }
    if (totalBytes - usedBytes <= kStorageOperationalFloorBytes) {
        return {false,
                "microSD is full or below the operational free-space floor; "
                "free space is required to create its CardMind identity"};
    }
    const SdVolumeIdentityResult staged = readSdVolumeIdentity(
        kSdVolumeIdentityStagedPath);
    if (!staged.success) return {false, staged.error};
    const String identity = staged.found
        ? staged.identity
        : generateSdVolumeIdentity();
    OperationResult result = writeSdVolumeIdentityMarker(identity);
    if (result.success) {
        result = saveSdVolumeIdentity(identity);
    }
    if (!result.success) return result;
    expectedSdVolumeIdentity = identity;
    expectedSdVolumeIdentityError = "";
    expectedSdVolumeIdentityLoaded = true;
    sdWasReady = true;
    return {true, ""};
}

OperationResult confirmSdStorageReplacement()
{
    if (sdFaultOverrideEnabled) {
        return {false, "Cannot confirm a microSD replacement while a diagnostic fault is active"};
    }
    if (SD.cardType() == CARD_NONE) {
        return {false, "microSD required: insert the card to confirm"};
    }
    File root = SD.open("/", FILE_READ);
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return {false, "Cannot confirm microSD replacement because the card is unavailable"};
    }
    root.close();

    SdVolumeIdentityResult marker = readSdVolumeIdentity(kSdVolumeIdentityPath);
    if (!marker.success) {
        return {false, marker.error};
    }
    String identity = marker.identity;
    if (!marker.found) {
        const std::uint64_t totalBytes = SD.totalBytes();
        const std::uint64_t usedBytes = SD.usedBytes();
        if (totalBytes == 0 || usedBytes > totalBytes ||
            totalBytes - usedBytes <= kStorageOperationalFloorBytes) {
            return {false,
                    "microSD replacement needs free space for its CardMind identity marker"};
        }
        const SdVolumeIdentityResult staged = readSdVolumeIdentity(
            kSdVolumeIdentityStagedPath);
        if (!staged.success) return {false, staged.error};
        identity = staged.found ? staged.identity : generateSdVolumeIdentity();
        const OperationResult written = writeSdVolumeIdentityMarker(identity);
        if (!written.success) return written;
    }
    const OperationResult saved = saveSdVolumeIdentity(identity);
    if (!saved.success) return saved;
    expectedSdVolumeIdentity = identity;
    expectedSdVolumeIdentityError = "";
    expectedSdVolumeIdentityLoaded = true;
    sdWasReady = true;
    sdRestartRequired = true;
    return {true, "microSD replacement confirmed; restart CardMind to initialize the workspace"};
}

SdStorageStatus inspectSdStorage()
{
    if (sdFaultOverrideEnabled) {
        return faultStatus(sdFaultOverrideState);
    }
    if (sdRestartRequired) {
        return {SdStorageState::Replaced, SD.totalBytes(), SD.usedBytes(),
                "microSD replacement confirmed; restart CardMind to initialize the workspace"};
    }
    if (SD.cardType() == CARD_NONE) {
        return faultStatus(sdWasReady ? SdStorageState::Removed
                                      : SdStorageState::Missing);
    }
    File root = SD.open("/", FILE_READ);
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return faultStatus(sdWasReady ? SdStorageState::Removed
                                      : SdStorageState::Missing);
    }
    root.close();

    const OperationResult expected = loadExpectedSdVolumeIdentity();
    if (!expected.success) {
        return {SdStorageState::Replaced, SD.totalBytes(), SD.usedBytes(), expected.error};
    }
    const SdVolumeIdentityResult marker = readSdVolumeIdentity(kSdVolumeIdentityPath);
    if (!marker.success) {
        return {SdStorageState::Replaced, SD.totalBytes(), SD.usedBytes(),
                marker.error + "; confirm this workspace before writing"};
    }
    const std::uint64_t totalBytes = SD.totalBytes();
    const std::uint64_t usedBytes = SD.usedBytes();
    if (!marker.found && expectedSdVolumeIdentity.isEmpty() && totalBytes != 0 &&
        usedBytes <= totalBytes &&
        totalBytes - usedBytes <= kStorageOperationalFloorBytes) {
        return {SdStorageState::Full, totalBytes, usedBytes,
                "microSD is full or below the operational free-space floor; "
                "free space is required to create its CardMind identity"};
    }
    if (!marker.found || expectedSdVolumeIdentity.isEmpty() ||
        marker.identity != expectedSdVolumeIdentity) {
        return faultStatus(SdStorageState::Replaced);
    }

    if (totalBytes == 0 || usedBytes > totalBytes) {
        return {SdStorageState::Removed, totalBytes, usedBytes,
                "microSD capacity became unavailable; reinsert the expected card"};
    }
    sdWasReady = true;
    if (totalBytes - usedBytes <= kStorageOperationalFloorBytes) {
        return {SdStorageState::Full, totalBytes, usedBytes,
                "microSD is full or below the operational free-space floor"};
    }
    return {SdStorageState::Ready, totalBytes, usedBytes, ""};
}

const char* sdStorageStateName(SdStorageState state)
{
    switch (state) {
        case SdStorageState::Ready: return "ready";
        case SdStorageState::Missing: return "missing";
        case SdStorageState::Full: return "full";
        case SdStorageState::Removed: return "removed";
        case SdStorageState::Replaced: return "replaced";
    }
    return "removed";
}

const char* sdStorageErrorCode(SdStorageState state)
{
    switch (state) {
        case SdStorageState::Ready: return "none";
        case SdStorageState::Missing: return "micro_sd_required";
        case SdStorageState::Full: return "micro_sd_full";
        case SdStorageState::Removed: return "micro_sd_removed";
        case SdStorageState::Replaced: return "micro_sd_replaced";
    }
    return "micro_sd_removed";
}

OperationResult requireSdReadAccess()
{
    return requireExistingSdVolumeAccess();
}

OperationResult requireSdCleanupAccess()
{
    return requireExistingSdVolumeAccess();
}

OperationResult requireSdWriteAccess(std::uint64_t requiredBytes,
                                     std::uint64_t operationalFloorBytes)
{
    const SdStorageStatus status = inspectSdStorage();
    if (status.state != SdStorageState::Ready) {
        return {false, status.error};
    }
    if (requiredBytes > std::numeric_limits<std::uint64_t>::max() -
            operationalFloorBytes) {
        return {false, "microSD write size plus operational floor overflows its byte range"};
    }
    const std::uint64_t freeBytes = status.totalBytes - status.usedBytes;
    const std::uint64_t requiredWithFloor = requiredBytes + operationalFloorBytes;
    if (requiredWithFloor > freeBytes) {
        return {false, "microSD is full: operation needs " +
                           String(static_cast<unsigned long long>(requiredBytes)) +
                           " bytes plus " +
                           String(static_cast<unsigned long long>(operationalFloorBytes)) +
                           " bytes operational floor, but only " +
                           String(static_cast<unsigned long long>(freeBytes)) +
                           " bytes are free"};
    }
    return {true, ""};
}

void setSdStorageFaultOverrideForDiagnostics(SdStorageState state)
{
    sdFaultOverrideState = state;
    sdFaultOverrideEnabled = state != SdStorageState::Ready;
}

void clearSdStorageFaultOverrideForDiagnostics()
{
    sdFaultOverrideEnabled = false;
    sdFaultOverrideState = SdStorageState::Ready;
}

OperationResult ensureSdDirectory(const String& path)
{
    if (path.isEmpty() || !path.startsWith("/")) {
        return {false, "SD directory path must be absolute"};
    }
    const OperationResult access = requireSdWriteAccess(0, kStorageOperationalFloorBytes);
    if (!access.success) return access;
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
        const int separator = path.lastIndexOf('/');
        const String parentPath = separator > 0 ? path.substring(0, separator) : String("/");
        File parent = SD.open(parentPath, FILE_READ);
        const bool parentExists = parent && parent.isDirectory();
        if (parent) {
            parent.close();
        }
        return {
            false,
            "Failed to create SD directory: " + path +
                "; parent=" + (parentExists ? String("ready") : String("unavailable")) +
                "; total_bytes=" +
                String(static_cast<unsigned long long>(SD.totalBytes())) +
                "; used_bytes=" +
                String(static_cast<unsigned long long>(SD.usedBytes())),
        };
    }
    return {true, ""};
}

OperationResult recoverAtomicSdFile(const String& target)
{
    const OperationResult access = requireSdCleanupAccess();
    if (!access.success) return access;
    const String temporary = target + ".tmp";
    const String recovery = target + ".bak";
    if (SD.exists(target)) {
        OperationResult result = removeSdFileIfPresent(temporary);
        return result.success ? removeSdFileIfPresent(recovery) : result;
    }
    if (SD.exists(recovery)) {
        const OperationResult renameAccess = requireSdCleanupAccess();
        if (!renameAccess.success) return renameAccess;
        if (!SD.rename(recovery, target)) {
            return {false, "Failed to recover interrupted storage file: " + target};
        }
    }
    return removeSdFileIfPresent(temporary);
}

OperationResult checkSdOperationSpace(std::uint64_t requiredBytes,
                                      std::uint64_t operationalFloorBytes)
{
    return requireSdWriteAccess(requiredBytes, operationalFloorBytes);
}

StorageLineResult readBoundedSdLine(File& file,
                                    std::size_t maximumBytes,
                                    const String& label)
{
    if (!file || maximumBytes == 0) {
        return {false, false, "", label + " reader arguments are invalid"};
    }
    const std::size_t totalBytes = file.size();
    if (file.position() >= totalBytes) {
        return {true, true, "", ""};
    }
    String line;
    while (file.position() < totalBytes) {
        const int value = file.read();
        if (value < 0) {
            return {false, false, "", label + " read stopped before newline"};
        }
        if (value == '\n') {
            return {true, false, line, ""};
        }
        if (line.length() >= maximumBytes) {
            return {false, false, "", label + " exceeds " + String(maximumBytes) +
                                      " bytes"};
        }
        if (!line.concat(static_cast<char>(value))) {
            return {false, false, "", label + " could not allocate its bounded line buffer"};
        }
    }
    return {false, false, "", label + " ended before newline"};
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
    HashingFilePrint output(file);
    const std::size_t writtenBytes = serializeJson(document, output);
    const std::uint32_t expectedHash = output.hash();
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
    if (validation.size() != expectedBytes) {
        validation.close();
        removeSdFileIfPresent(temporary);
        return {false, "Staged JSON file size changed after flush: " + temporary};
    }
    std::uint8_t buffer[1024];
    std::size_t validatedBytes = 0;
    std::uint32_t validatedHash = kFnvOffsetBasis;
    while (validation.available()) {
        const std::size_t readBytes = validation.read(buffer, sizeof(buffer));
        if (readBytes == 0) {
            validation.close();
            removeSdFileIfPresent(temporary);
            return {false, "Staged JSON validation stopped before EOF: " + temporary};
        }
        validatedHash = updateFnv1a(validatedHash, buffer, readBytes);
        validatedBytes += readBytes;
    }
    validation.close();
    if (validatedBytes != expectedBytes || validatedHash != expectedHash) {
        removeSdFileIfPresent(temporary);
        return {false, "Staged JSON checksum validation failed: " + temporary};
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
    const OperationResult access = requireSdCleanupAccess();
    if (!access.success) return access;
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
    const OperationResult access = requireSdReadAccess();
    if (!access.success) return access;
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

StorageIndexMutationPlanResult planJsonlSdIndexMutation(
    const String& path,
    const String& keyField,
    const String& keyValue,
    const String& replacementLine,
    bool removeEntry)
{
    if (keyField.isEmpty() || keyValue.isEmpty()) {
        return {false, 0, false,
                "Storage index mutation requires a key field and value"};
    }
    if (!removeEntry && (replacementLine.isEmpty() ||
                         replacementLine.length() > kMaximumStorageIndexLineBytes)) {
        return {false, 0, false,
                "Storage index replacement must contain 1 to 1024 bytes"};
    }
    File source = SD.open(path, FILE_READ);
    std::uint64_t stagedBytes = 0;
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
            return {false, 0, false,
                    "Storage index line exceeds 1024 bytes at line " +
                        String(lineNumber)};
        }
        JsonDocument item;
        const DeserializationError error = deserializeJson(item, line);
        if (error || !item[keyField].is<const char*>()) {
            source.close();
            return {false, 0, false,
                    "Storage index contains an invalid typed entry at line " +
                        String(lineNumber)};
        }
        const bool matches = String(item[keyField].as<const char*>()) == keyValue;
        if (matches) {
            if (found) {
                source.close();
                return {false, 0, false,
                        "Storage index contains duplicate key " + keyValue};
            }
            found = true;
            if (removeEntry) {
                continue;
            }
            if (stagedBytes > std::numeric_limits<std::uint64_t>::max() -
                    replacementLine.length() - 1U) {
                source.close();
                return {false, 0, false, "Storage index staged size overflows"};
            }
            stagedBytes += replacementLine.length() + 1U;
            continue;
        }
        if (stagedBytes > std::numeric_limits<std::uint64_t>::max() -
                line.length() - 1U) {
            source.close();
            return {false, 0, false, "Storage index staged size overflows"};
        }
        stagedBytes += line.length() + 1U;
    }
    if (source) {
        source.close();
    }
    if (!found && !removeEntry) {
        if (stagedBytes > std::numeric_limits<std::uint64_t>::max() -
                replacementLine.length() - 1U) {
            return {false, 0, false, "Storage index staged size overflows"};
        }
        stagedBytes += replacementLine.length() + 1U;
    }
    return {true, stagedBytes, found, ""};
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
    if (existingBytes > std::numeric_limits<std::uint64_t>::max() -
            replacementLine.length() - 1U) {
        return {false, "Storage index preflight size overflows"};
    }
    OperationResult result = checkSdOperationSpace(
        existingBytes + replacementLine.length() + 1U,
        operationalFloorBytes);
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
    const OperationResult access = requireSdReadAccess();
    if (!access.success) {
        return {false, {}, offset, false, access.error};
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
    const OperationResult access = requireSdReadAccess();
    if (!access.success) {
        return {false, false, "", access.error};
    }
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

JsonStringFieldResult readJsonStringField(const String& path,
                                          const String& field,
                                          std::size_t maximumBytes)
{
    if (field.isEmpty() || field.length() > kMaximumJsonFieldNameBytes) {
        return {false, {}, "JSON field name must contain 1 to 64 bytes"};
    }
    const OperationResult access = requireSdReadAccess();
    if (!access.success) {
        return {false, {}, access.error};
    }
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        return {false, {}, "JSON file does not exist: " + path};
    }
    skipJsonWhitespace(file);
    if (!file.available() || file.read() != '{') {
        file.close();
        return {false, {}, "JSON document must contain a top-level object: " + path};
    }
    bool found = false;
    bool closedObject = false;
    std::string selected;
    while (file.available()) {
        skipJsonWhitespace(file);
        if (file.peek() == '}') {
            file.read();
            closedObject = true;
            break;
        }
        std::string key;
        OperationResult parsed = readDecodedJsonString(
            file, key, kMaximumJsonFieldNameBytes);
        if (!parsed.success) {
            file.close();
            return {false, {}, "Failed to read JSON field name: " + parsed.error};
        }
        skipJsonWhitespace(file);
        if (!file.available() || file.read() != ':') {
            file.close();
            return {false, {}, "JSON field is missing its colon: " + String(key.c_str())};
        }
        skipJsonWhitespace(file);
        if (key == field.c_str()) {
            if (found) {
                file.close();
                return {false, {}, "JSON document contains duplicate field: " + field};
            }
            const std::size_t valuePosition = file.position();
            const JsonStringLengthResult measured = measureDecodedJsonString(
                file, maximumBytes);
            if (!measured.success) {
                file.close();
                return {false, {}, "Failed to measure JSON field " + field +
                                           ": " + measured.error};
            }
            if (!file.seek(valuePosition)) {
                file.close();
                return {false, {}, "Failed to rewind JSON field before decoding: " + field};
            }
            selected.reserve(measured.bytes);
            parsed = readDecodedJsonString(file, selected, maximumBytes);
            found = parsed.success;
        } else {
            parsed = skipJsonValue(file);
        }
        if (!parsed.success) {
            file.close();
            return {false, {}, "Failed to read JSON field " + String(key.c_str()) +
                                       ": " + parsed.error};
        }
        skipJsonWhitespace(file);
        if (!file.available()) {
            file.close();
            return {false, {}, "JSON object ended before its closing brace: " + path};
        }
        const int delimiter = file.read();
        if (delimiter == '}') {
            closedObject = true;
            break;
        }
        if (delimiter != ',') {
            file.close();
            return {false, {}, "JSON field is followed by an invalid delimiter: " + path};
        }
    }
    skipJsonWhitespace(file);
    const bool trailingData = file.available();
    file.close();
    if (!closedObject) {
        return {false, {}, "JSON object ended before its closing brace: " + path};
    }
    if (trailingData) {
        return {false, {}, "JSON document contains trailing data: " + path};
    }
    return found ? JsonStringFieldResult{true, std::move(selected), ""}
                 : JsonStringFieldResult{false, {}, "JSON string field is missing: " + field};
}

OperationResult removeSdDirectoryTree(const String& path)
{
    const OperationResult access = requireSdCleanupAccess();
    if (!access.success) return access;
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
        const OperationResult mutationAccess = requireSdCleanupAccess();
        if (!mutationAccess.success) {
            directory.close();
            return mutationAccess;
        }
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
