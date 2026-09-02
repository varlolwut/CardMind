#include "python_mode.h"

#include "file_workspace.h"
#include "text_utils.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <SD.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace cardputer {
namespace {

constexpr const char* kPythonNamespace = "cardmind_py";
constexpr const char* kPythonHandoffPath = "/assistant/.python-open-web";
constexpr const char* kPythonRunRequestPath =
    "/assistant/v2/python_run_request.json";
constexpr const char* kPythonRunRequestTemporaryPath =
    "/assistant/v2/python_run_request.json.tmp";
constexpr const char* kPythonRunResultPath =
    "/assistant/v2/python_run_result.json";
constexpr const char* kPythonRunResultTemporaryPath =
    "/assistant/v2/python_run_result.json.tmp";
constexpr const char* kCardMindPartitionLabel = "cardmind";
constexpr const char* kPythonPartitionLabel = "python";
constexpr std::uint32_t kMinimumCardMindPartitionBytes = 0x330000U;
constexpr std::uint32_t kMinimumPythonPartitionBytes = 0x1a0000U;
constexpr std::uint8_t kEspImageMagic = 0xe9U;
constexpr std::uint8_t kPythonRunVersion = 1;
constexpr std::size_t kPythonRunBlobBytes = 91;
constexpr std::size_t kPythonRunRequestMaximumBytes = 1024;
constexpr std::size_t kPythonRunResultMaximumBytes = 24576;
constexpr std::size_t kPythonRunOutputMaximumBytes = 16384;
constexpr std::size_t kPythonRunMessageMaximumBytes = 18432;
constexpr std::uint64_t kPythonRunSourceMaximumBytes = 65536;

enum class PythonRunState : std::uint8_t {
    Pending = 1,
    Claimed = 2,
    Complete = 3,
};

struct PythonRunBlob {
    PythonRunState state;
    PythonRunReturnSurface returnSurface;
    String pendingId;
    std::array<std::uint8_t, 32> requestSha256;
    std::array<std::uint8_t, 32> resultSha256;
    std::uint64_t auditSequence;
};

const esp_partition_t* findAppPartition(const char* label)
{
    return esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);
}

bool partitionStartsWithEspImage(const esp_partition_t* partition)
{
    if (partition == nullptr) {
        return false;
    }
    std::uint8_t magic = 0;
    return esp_partition_read(partition, 0, &magic, sizeof(magic)) == ESP_OK &&
           magic == kEspImageMagic;
}

bool validSha256(const String& value)
{
    if (value.length() != 64) {
        return false;
    }
    for (std::size_t index = 0; index < value.length(); ++index) {
        const char character = value[index];
        if (!std::isdigit(static_cast<unsigned char>(character)) &&
            !(character >= 'a' && character <= 'f')) {
            return false;
        }
    }
    return true;
}

bool validPendingId(const String& value)
{
    if (value.length() != 16) return false;
    for (std::size_t index = 0; index < value.length(); ++index) {
        const char character = value[index];
        if (!std::isdigit(static_cast<unsigned char>(character)) &&
            (character < 'a' || character > 'f')) {
            return false;
        }
    }
    return true;
}

bool validReturnSurface(PythonRunReturnSurface surface)
{
    return surface == PythonRunReturnSurface::Device ||
           surface == PythonRunReturnSurface::Web;
}

bool sha256Bytes(const std::uint8_t* bytes,
                 std::size_t size,
                 std::array<std::uint8_t, 32>& digest)
{
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    const bool valid = mbedtls_sha256_starts(&context, 0) == 0 &&
        (size == 0 || mbedtls_sha256_update(&context, bytes, size) == 0) &&
        mbedtls_sha256_finish(&context, digest.data()) == 0;
    mbedtls_sha256_free(&context);
    return valid;
}

bool parseSha256(const std::string& value,
                 std::array<std::uint8_t, 32>& digest)
{
    if (value.size() != 64) return false;
    for (std::size_t index = 0; index < digest.size(); ++index) {
        const auto nibble = [](char character) -> int {
            if (character >= '0' && character <= '9') return character - '0';
            if (character >= 'a' && character <= 'f') {
                return character - 'a' + 10;
            }
            return -1;
        };
        const int high = nibble(value[index * 2]);
        const int low = nibble(value[index * 2 + 1]);
        if (high < 0 || low < 0) return false;
        digest[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return true;
}

std::array<std::uint8_t, kPythonRunBlobBytes> encodeRunBlob(
    const PythonRunBlob& blob)
{
    std::array<std::uint8_t, kPythonRunBlobBytes> encoded = {};
    encoded[0] = kPythonRunVersion;
    encoded[1] = static_cast<std::uint8_t>(blob.state);
    encoded[2] = static_cast<std::uint8_t>(blob.returnSurface);
    std::memcpy(encoded.data() + 3, blob.pendingId.c_str(), 16);
    std::copy(blob.requestSha256.begin(), blob.requestSha256.end(),
              encoded.begin() + 19);
    std::copy(blob.resultSha256.begin(), blob.resultSha256.end(),
              encoded.begin() + 51);
    for (std::size_t index = 0; index < 8; ++index) {
        encoded[83 + index] = static_cast<std::uint8_t>(
            (blob.auditSequence >> (index * 8)) & 0xffU);
    }
    return encoded;
}

OperationResult decodeRunBlob(
    const std::array<std::uint8_t, kPythonRunBlobBytes>& encoded,
    PythonRunBlob& blob)
{
    if (encoded[0] != kPythonRunVersion) {
        return {false, "Python run state version is invalid"};
    }
    blob.state = static_cast<PythonRunState>(encoded[1]);
    blob.returnSurface = static_cast<PythonRunReturnSurface>(encoded[2]);
    blob.pendingId = String(
        reinterpret_cast<const char*>(encoded.data() + 3), 16);
    std::copy(encoded.begin() + 19, encoded.begin() + 51,
              blob.requestSha256.begin());
    std::copy(encoded.begin() + 51, encoded.begin() + 83,
              blob.resultSha256.begin());
    blob.auditSequence = 0;
    for (std::size_t index = 0; index < 8; ++index) {
        blob.auditSequence |= static_cast<std::uint64_t>(encoded[83 + index])
            << (index * 8);
    }
    const bool resultHashZero = std::all_of(
        blob.resultSha256.begin(), blob.resultSha256.end(),
        [](std::uint8_t value) { return value == 0; });
    if ((blob.state != PythonRunState::Pending &&
         blob.state != PythonRunState::Claimed &&
         blob.state != PythonRunState::Complete) ||
        !validReturnSurface(blob.returnSurface) ||
        !validPendingId(blob.pendingId) || blob.auditSequence == 0 ||
        std::all_of(blob.requestSha256.begin(), blob.requestSha256.end(),
                    [](std::uint8_t value) { return value == 0; }) ||
        ((blob.state == PythonRunState::Complete) == resultHashZero)) {
        return {false, "Python run state fields are invalid"};
    }
    return {true, ""};
}

OperationResult readRunBlob(bool& found, PythonRunBlob& blob)
{
    found = false;
    Preferences preferences;
    if (!preferences.begin(kPythonNamespace, true)) {
        return {false, "Failed to open Python run state in NVS"};
    }
    if (!preferences.isKey("run")) {
        preferences.end();
        return {true, ""};
    }
    found = true;
    if (preferences.getBytesLength("run") != kPythonRunBlobBytes) {
        preferences.end();
        return {false, "Python run state does not contain exactly 91 bytes"};
    }
    std::array<std::uint8_t, kPythonRunBlobBytes> encoded = {};
    const std::size_t read = preferences.getBytes(
        "run", encoded.data(), encoded.size());
    preferences.end();
    if (read != encoded.size()) {
        return {false, "Failed to read the complete Python run state"};
    }
    return decodeRunBlob(encoded, blob);
}

OperationResult writeRunBlob(const PythonRunBlob& blob)
{
    const auto encoded = encodeRunBlob(blob);
    Preferences preferences;
    if (!preferences.begin(kPythonNamespace, false)) {
        return {false, "Failed to open Python run state in NVS"};
    }
    const std::size_t stored = preferences.putBytes(
        "run", encoded.data(), encoded.size());
    std::array<std::uint8_t, kPythonRunBlobBytes> verified = {};
    const std::size_t read = preferences.getBytes(
        "run", verified.data(), verified.size());
    preferences.end();
    return stored == encoded.size() && read == encoded.size() &&
            verified == encoded
        ? OperationResult{true, ""}
        : OperationResult{false, "Failed to persist and verify Python run state"};
}

OperationResult removeOwnedFile(const char* path)
{
    if (!SD.exists(path)) return {true, ""};
    return SD.remove(path)
        ? OperationResult{true, ""}
        : OperationResult{false, String("Failed to remove Python run artifact: ") + path};
}

OperationResult readBoundedFile(const char* path,
                                std::size_t maximumBytes,
                                std::string& output)
{
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        return {false, String("Failed to open Python run file: ") + path};
    }
    const std::size_t size = file.size();
    if (size > maximumBytes) {
        file.close();
        return {false, String("Python run file exceeds its byte limit: ") + path};
    }
    output.assign(size, '\0');
    std::size_t offset = 0;
    while (offset < size) {
        const std::size_t read = file.read(
            reinterpret_cast<std::uint8_t*>(&output[offset]), size - offset);
        if (read == 0) {
            file.close();
            return {false, String("Python run file ended early: ") + path};
        }
        offset += read;
    }
    file.close();
    return {true, ""};
}

struct DecodedPythonRunRequest {
    String pendingId;
    String path;
    std::uint64_t sourceBytes;
    std::string sourceSha256;
    std::uint64_t auditSequence;
    PythonRunReturnSurface returnSurface;
};

OperationResult decodePythonRunRequest(
    const std::string& bytes,
    DecodedPythonRunRequest& decoded)
{
    JsonDocument document;
    const DeserializationError parsed = deserializeJson(document, bytes);
    const JsonObjectConst request = document.as<JsonObjectConst>();
    static constexpr const char* kFields[] = {
        "version", "pending_id", "path", "size", "sha256",
        "audit_sequence", "surface",
    };
    bool exactFields = !parsed && request.size() == 7;
    for (const char* field : kFields) {
        exactFields = exactFields && request.containsKey(field);
    }
    if (!exactFields ||
        !request["version"].is<std::uint8_t>() ||
        request["version"].as<std::uint8_t>() != kPythonRunVersion ||
        !request["pending_id"].is<const char*>() ||
        !request["path"].is<const char*>() ||
        !request["size"].is<std::uint64_t>() ||
        !request["sha256"].is<const char*>() ||
        !request["audit_sequence"].is<std::uint64_t>() ||
        !request["surface"].is<const char*>()) {
        return {false, "Python run request fields or types are invalid"};
    }
    decoded.pendingId = request["pending_id"].as<const char*>();
    decoded.path = request["path"].as<const char*>();
    decoded.sourceBytes = request["size"].as<std::uint64_t>();
    decoded.sourceSha256 = request["sha256"].as<const char*>();
    decoded.auditSequence = request["audit_sequence"].as<std::uint64_t>();
    const char* surface = request["surface"].as<const char*>();
    if (!validPendingId(decoded.pendingId) ||
        !isValidWorkspaceFilename(decoded.path.c_str()) ||
        !decoded.path.endsWith(".py") ||
        decoded.sourceBytes > kPythonRunSourceMaximumBytes ||
        !validSha256(decoded.sourceSha256.c_str()) ||
        decoded.auditSequence == 0 ||
        (std::strcmp(surface, "device") != 0 &&
         std::strcmp(surface, "web") != 0)) {
        return {false, "Python run request identity is invalid"};
    }
    decoded.returnSurface = std::strcmp(surface, "web") == 0
        ? PythonRunReturnSurface::Web : PythonRunReturnSurface::Device;
    return {true, ""};
}

OperationResult writeRequestFile(const std::string& request)
{
    File file = SD.open(kPythonRunRequestTemporaryPath, FILE_WRITE);
    if (!file) {
        return {false, "Failed to create temporary Python run request"};
    }
    const std::size_t written = file.write(
        reinterpret_cast<const std::uint8_t*>(request.data()), request.size());
    file.flush();
    file.close();
    if (written != request.size()) {
        removeOwnedFile(kPythonRunRequestTemporaryPath);
        return {false, "Python run request write was incomplete"};
    }
    std::string verified;
    OperationResult result = readBoundedFile(
        kPythonRunRequestTemporaryPath, kPythonRunRequestMaximumBytes, verified);
    if (!result.success || verified != request) {
        removeOwnedFile(kPythonRunRequestTemporaryPath);
        return {false, result.success
            ? String("Python run request readback does not match")
            : result.error};
    }
    if (SD.exists(kPythonRunRequestPath) ||
        !SD.rename(kPythonRunRequestTemporaryPath, kPythonRunRequestPath)) {
        removeOwnedFile(kPythonRunRequestTemporaryPath);
        return {false, "Failed to commit Python run request"};
    }
    return {true, ""};
}

OperationResult verifyPythonSource(const PythonRunStageRequest& request)
{
    const std::string name(request.fileName.c_str());
    if (!validPendingId(request.pendingId) ||
        !isValidWorkspaceFilename(name) || name.size() < 3 ||
        name.compare(name.size() - 3, 3, ".py") != 0 ||
        request.sourceBytes > kPythonRunSourceMaximumBytes ||
        request.auditSequence == 0 ||
        !validReturnSurface(request.returnSurface)) {
        return {false, "Python run staging identity is invalid"};
    }
    std::array<std::uint8_t, 32> expected = {};
    if (!parseSha256(request.sourceSha256, expected)) {
        return {false, "Python run source SHA-256 is invalid"};
    }
    const String path = workspaceFilePath(request.fileName);
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        return {false, "Failed to reopen approved Python source"};
    }
    const std::size_t size = file.size();
    if (size != request.sourceBytes || size > kPythonRunSourceMaximumBytes) {
        file.close();
        return {false, "Approved Python source size changed before staging"};
    }
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    if (mbedtls_sha256_starts(&context, 0) != 0) {
        mbedtls_sha256_free(&context);
        file.close();
        return {false, "Failed to initialize approved Python source SHA-256"};
    }
    std::array<std::uint8_t, 1024> buffer = {};
    PythonUtf8StreamState utf8 = {0, 0, 0, true};
    std::size_t offset = 0;
    while (offset < size) {
        const std::size_t remaining = size - offset;
        const std::size_t read = file.read(
            buffer.data(), std::min(remaining, buffer.size()));
        if (read == 0 || read > remaining) {
            mbedtls_sha256_free(&context);
            file.close();
            return {false, "Approved Python source ended before its exact size"};
        }
        utf8 = consumePythonUtf8Chunk(utf8, buffer.data(), read);
        if (!utf8.valid) {
            mbedtls_sha256_free(&context);
            file.close();
            return {false, "Approved Python source is not valid UTF-8"};
        }
        if (mbedtls_sha256_update(&context, buffer.data(), read) != 0) {
            mbedtls_sha256_free(&context);
            file.close();
            return {false, "Failed to hash approved Python source completely"};
        }
        offset += read;
        delay(0);
    }
    file.close();
    if (!pythonUtf8StreamComplete(utf8)) {
        mbedtls_sha256_free(&context);
        return {false, "Approved Python source ends with incomplete UTF-8"};
    }
    std::array<std::uint8_t, 32> actual = {};
    if (mbedtls_sha256_finish(&context, actual.data()) != 0) {
        mbedtls_sha256_free(&context);
        return {false, "Failed to finish approved Python source SHA-256"};
    }
    mbedtls_sha256_free(&context);
    if (actual != expected) {
        return {false, "Approved Python source SHA-256 changed before staging"};
    }
    return {true, ""};
}

bool hasExactResultFields(const JsonObjectConst& object)
{
    static constexpr const char* kFields[] = {
        "version", "pending_id", "audit_sequence", "exit_status",
        "stdout_b64", "stderr_b64", "stdout_truncated", "stderr_truncated",
    };
    if (object.size() != 8) return false;
    for (const char* field : kFields) {
        if (!object.containsKey(field)) return false;
    }
    return true;
}

OperationResult decodeBase64Utf8(const char* encoded, std::string& decoded)
{
    if (encoded == nullptr) return {false, "Python result base64 field is missing"};
    const std::size_t encodedBytes = std::strlen(encoded);
    if (encodedBytes == 0) {
        decoded.clear();
        return {true, ""};
    }
    std::vector<std::uint8_t> buffer(encodedBytes * 3U / 4U + 3U);
    std::size_t decodedBytes = 0;
    const int result = mbedtls_base64_decode(
        buffer.data(), buffer.size(), &decodedBytes,
        reinterpret_cast<const std::uint8_t*>(encoded), encodedBytes);
    if (result != 0) {
        return {false, "Python result contains invalid base64 output"};
    }
    decoded.assign(reinterpret_cast<const char*>(buffer.data()), decodedBytes);
    return isValidUtf8(decoded)
        ? OperationResult{true, ""}
        : OperationResult{false, "Python result output is not valid UTF-8"};
}

PythonRunRecoveryResult decodeCompleteRecovery(
    const PythonRunBlob& blob,
    bool attachmentAllowed)
{
    PythonRunRecoveryResult recovery = {};
    recovery.found = true;
    recovery.attachmentAllowed = attachmentAllowed;
    recovery.pendingId = blob.pendingId;
    recovery.auditSequence = blob.auditSequence;
    recovery.returnSurface = blob.returnSurface;
    std::string serialized;
    OperationResult result = readBoundedFile(
        kPythonRunResultPath, kPythonRunResultMaximumBytes, serialized);
    if (!result.success) {
        recovery.error = result.error;
        return recovery;
    }
    const auto invalidResult = [&recovery]() -> PythonRunRecoveryResult {
        recovery.success = true;
        recovery.executionSucceeded = false;
        recovery.hasExitStatus = false;
        recovery.exitStatus = 0;
        recovery.outputBytes = 0;
        recovery.assistantMessage =
            "Python run result was invalid; effects are unknown and the run was not replayed.";
        recovery.error = "";
        return recovery;
    };
    std::array<std::uint8_t, 32> actualDigest = {};
    if (!sha256Bytes(
            reinterpret_cast<const std::uint8_t*>(serialized.data()),
            serialized.size(), actualDigest) || actualDigest != blob.resultSha256) {
        return invalidResult();
    }
    JsonDocument document;
    const DeserializationError parsed = deserializeJson(document, serialized);
    const JsonObjectConst root = document.as<JsonObjectConst>();
    if (parsed || !hasExactResultFields(root) ||
        !root["version"].is<std::uint8_t>() ||
        root["version"].as<std::uint8_t>() != kPythonRunVersion ||
        !root["pending_id"].is<const char*>() ||
        !root["audit_sequence"].is<std::uint64_t>() ||
        !root["exit_status"].is<std::int32_t>() ||
        !root["stdout_b64"].is<const char*>() ||
        !root["stderr_b64"].is<const char*>() ||
        !root["stdout_truncated"].is<bool>() ||
        !root["stderr_truncated"].is<bool>() ||
        String(root["pending_id"].as<const char*>()) != blob.pendingId ||
        root["audit_sequence"].as<std::uint64_t>() != blob.auditSequence) {
        return invalidResult();
    }
    std::string standardOutput;
    std::string standardError;
    result = decodeBase64Utf8(
        root["stdout_b64"].as<const char*>(), standardOutput);
    if (result.success) {
        result = decodeBase64Utf8(
            root["stderr_b64"].as<const char*>(), standardError);
    }
    if (!result.success ||
        standardOutput.size() + standardError.size() >
            kPythonRunOutputMaximumBytes) {
        return invalidResult();
    }
    recovery.exitStatus = root["exit_status"].as<std::int32_t>();
    recovery.hasExitStatus = true;
    recovery.executionSucceeded = recovery.exitStatus == 0;
    recovery.outputBytes = static_cast<std::uint32_t>(
        standardOutput.size() + standardError.size());
    recovery.assistantMessage =
        "Python run finished.\nExit status: " +
        std::to_string(recovery.exitStatus) + "\nstdout:\n" +
        (standardOutput.empty() ? std::string("<empty>") : standardOutput);
    if (root["stdout_truncated"].as<bool>()) {
        recovery.assistantMessage += "\n[stdout truncated]";
    }
    recovery.assistantMessage += "\nstderr:\n" +
        (standardError.empty() ? std::string("<empty>") : standardError);
    if (root["stderr_truncated"].as<bool>()) {
        recovery.assistantMessage += "\n[stderr truncated]";
    }
    if (recovery.assistantMessage.size() > kPythonRunMessageMaximumBytes ||
        !isValidUtf8(recovery.assistantMessage)) {
        return invalidResult();
    }
    recovery.success = true;
    return recovery;
}

OperationResult writeBlob(Preferences& preferences, const char* key,
                          const String& value, std::size_t maximumBytes)
{
    if (value.length() + 1 > maximumBytes) {
        return {false, String("Python mode value exceeds its limit: ") + key};
    }
    const std::size_t expected = value.length() + 1;
    const std::size_t stored = preferences.putBytes(
        key, value.c_str(), expected);
    if (stored != expected) {
        return {false, String("Failed to synchronize Python mode value: ") + key};
    }
    std::array<char, 193> verified = {};
    if (expected > verified.size() ||
        preferences.getBytes(key, verified.data(), expected) != expected ||
        String(verified.data()) != value) {
        return {false, String("Failed to verify Python mode value: ") + key};
    }
    return {true, ""};
}

OperationResult removeKeyIfPresent(Preferences& preferences, const char* key)
{
    if (!preferences.isKey(key)) {
        return {true, ""};
    }
    return preferences.remove(key)
        ? OperationResult{true, ""}
        : OperationResult{false, String("Failed to clear Python update key: ") + key};
}

String readBlobIfPresent(const char* key, std::size_t maximumBytes)
{
    Preferences preferences;
    if (!preferences.begin(kPythonNamespace, false)) {
        return "Failed to read Python mode status from NVS";
    }
    if (!preferences.isKey(key)) {
        preferences.end();
        return "";
    }
    const std::size_t length = preferences.getBytesLength(key);
    if (length == 0 || length > maximumBytes) {
        preferences.end();
        return "Stored Python mode status has an invalid length";
    }
    std::array<char, 193> value = {};
    const std::size_t read = preferences.getBytes(key, value.data(), length);
    preferences.end();
    if (read != length || value[length - 1] != '\0') {
        return "Stored Python mode status could not be read";
    }
    return String(value.data());
}

}  // namespace

PythonModeStatus inspectPythonMode()
{
    const esp_partition_t* cardMind = findAppPartition(kCardMindPartitionLabel);
    const esp_partition_t* python = findAppPartition(kPythonPartitionLabel);
    const bool layoutReady = cardMind != nullptr && python != nullptr &&
        cardMind->size >= kMinimumCardMindPartitionBytes &&
        python->size >= kMinimumPythonPartitionBytes;
    if (!layoutReady) {
        return {false, false,
                cardMind == nullptr ? 0U : cardMind->size,
                python == nullptr ? 0U : python->size,
                readBlobIfPresent("mode_error", 193),
                "CardMind/Python partition layout is not installed"};
    }
    const bool imageReady = partitionStartsWithEspImage(python);
    return {true, imageReady, cardMind->size, python->size,
            readBlobIfPresent("mode_error", 193),
            imageReady ? String("") : String("MicroPython app image is not installed")};
}

bool pythonOneShotAvailable()
{
    const PythonModeStatus status = inspectPythonMode();
    return status.partitionLayoutReady && status.pythonImageReady;
}

OperationResult synchronizePythonModeSettings(const Settings& settings,
                                              const String& consolePassword,
                                              const String& handoffToken)
{
    if (settings.wifiSsid.isEmpty()) {
        return {false, "Python mode requires a configured Wi-Fi SSID"};
    }
    if (consolePassword.length() < 8) {
        return {false, "Python mode requires an installation password of at least 8 characters"};
    }
    if (!handoffToken.isEmpty() && handoffToken.length() != 32) {
        return {false, "Python mode handoff token must contain 32 characters"};
    }
    Preferences preferences;
    if (!preferences.begin(kPythonNamespace, false)) {
        return {false, "Failed to initialize NVS namespace 'cardmind_py'"};
    }
    OperationResult result = writeBlob(preferences, "ssid", settings.wifiSsid, 65);
    if (result.success) {
        result = writeBlob(preferences, "wifi_pass", settings.wifiPassword, 193);
    }
    if (result.success) {
        result = writeBlob(preferences, "console_pass", consolePassword, 97);
    }
    if (result.success) {
        result = handoffToken.isEmpty()
            ? removeKeyIfPresent(preferences, "handoff")
            : writeBlob(preferences, "handoff", handoffToken, 33);
    }
    if (result.success) {
        result = removeKeyIfPresent(preferences, "mode_error");
    }
    preferences.end();
    return result;
}

OperationResult activatePythonMode()
{
    bool runFound = false;
    PythonRunBlob run = {};
    const OperationResult runState = readRunBlob(runFound, run);
    if (!runState.success) {
        return runState;
    }
    if (runFound) {
        return {false,
                "Manual Python workspace is unavailable until the one-shot run is consumed"};
    }
    const PythonModeStatus status = inspectPythonMode();
    if (!status.partitionLayoutReady || !status.pythonImageReady) {
        return {false, status.error};
    }
    const esp_partition_t* python = findAppPartition(kPythonPartitionLabel);
    const esp_err_t result = esp_ota_set_boot_partition(python);
    return result == ESP_OK
        ? OperationResult{true, ""}
        : OperationResult{false, String("Failed to select MicroPython partition: ESP error ") +
                                 static_cast<int>(result)};
}

PythonHandoffRequest consumePythonHandoffRequest()
{
    Preferences preferences;
    if (!preferences.begin(kPythonNamespace, true)) {
        return {false, false, "Failed to open Python handoff state in NVS"};
    }
    const bool requested = preferences.getInt("open_web", 0) == 1;
    preferences.end();
    return {true, requested, ""};
}

PythonHandoffRequest consumePythonSdHandoffRequest()
{
    if (!SD.exists(kPythonHandoffPath)) {
        return {true, false, ""};
    }
    return {true, true, ""};
}

OperationResult acknowledgePythonHandoffRequest()
{
    Preferences preferences;
    if (!preferences.begin(kPythonNamespace, false)) {
        return {false, "Failed to open Python Web Console handoff state"};
    }
    OperationResult result = removeKeyIfPresent(preferences, "open_web");
    preferences.end();
    if (!result.success) return result;
    if (SD.exists(kPythonHandoffPath) && !SD.remove(kPythonHandoffPath)) {
        return {false, "Failed to acknowledge Python Web Console microSD marker"};
    }
    return {true, ""};
}

OperationResult cleanupPythonRunArtifacts()
{
    const char* paths[] = {
        kPythonRunRequestTemporaryPath,
        kPythonRunRequestPath,
        kPythonRunResultTemporaryPath,
        kPythonRunResultPath,
    };
    for (const char* path : paths) {
        const OperationResult removed = removeOwnedFile(path);
        if (!removed.success) return removed;
    }
    return {true, ""};
}

OperationResult preparePythonRunStaging()
{
    bool found = false;
    PythonRunBlob blob = {};
    const OperationResult state = readRunBlob(found, blob);
    if (!state.success) return state;
    if (found) {
        return {false, "Another Python one-shot run still requires consumption"};
    }
    return cleanupPythonRunArtifacts();
}

PythonRunStageResult stagePythonRun(
    const PythonRunStageRequest& request,
    const std::function<bool()>& isCancelled)
{
    OperationResult result = verifyPythonSource(request);
    if (!result.success) return {false, false, result.error};

    JsonDocument document;
    document["version"] = kPythonRunVersion;
    document["pending_id"] = request.pendingId;
    document["path"] = request.fileName;
    document["size"] = request.sourceBytes;
    document["sha256"] = request.sourceSha256;
    document["audit_sequence"] = request.auditSequence;
    document["surface"] = request.returnSurface == PythonRunReturnSurface::Web
        ? "web" : "device";
    std::string serialized;
    serialized.reserve(measureJson(document));
    serializeJson(document, serialized);
    if (serialized.empty() || serialized.size() > kPythonRunRequestMaximumBytes) {
        return {false, false, "Python run request exceeds 1024 bytes"};
    }
    std::array<std::uint8_t, 32> requestDigest = {};
    if (!sha256Bytes(
            reinterpret_cast<const std::uint8_t*>(serialized.data()),
            serialized.size(), requestDigest)) {
        return {false, false, "Failed to hash Python run request"};
    }
    result = writeRequestFile(serialized);
    if (!result.success) return {false, false, result.error};

    PythonRunBlob blob = {
        PythonRunState::Pending,
        request.returnSurface,
        request.pendingId,
        requestDigest,
        {},
        request.auditSequence,
    };
    if (isCancelled()) {
        String error = "Tool execution canceled by user";
        const OperationResult temporaryCleanup = removeOwnedFile(
            kPythonRunRequestTemporaryPath);
        const OperationResult requestCleanup = removeOwnedFile(
            kPythonRunRequestPath);
        if (!temporaryCleanup.success) {
            error += "; temporary request cleanup failed: " +
                temporaryCleanup.error;
        }
        if (!requestCleanup.success) {
            error += "; request cleanup failed: " + requestCleanup.error;
        }
        return {false, false, error};
    }
    result = writeRunBlob(blob);
    if (!result.success) {
        return {false, false,
                result.error +
                    "; request and run state retained for startup recovery"};
    }
    const esp_partition_t* python = findAppPartition(kPythonPartitionLabel);
    const esp_err_t selected = python == nullptr
        ? ESP_ERR_NOT_FOUND : esp_ota_set_boot_partition(python);
    if (selected != ESP_OK) {
        const OperationResult discarded = discardPythonRunState();
        const OperationResult cleaned = discarded.success
            ? cleanupPythonRunArtifacts() : discarded;
        String error = String("Failed to select MicroPython for approved run: ESP error ") +
            static_cast<int>(selected);
        if (!cleaned.success) error += "; cleanup failed: " + cleaned.error;
        return {false, false, error};
    }
    return {true, true, ""};
}

PythonRunRecoveryResult loadPythonRunRecovery()
{
    PythonRunRecoveryResult recovery = {};
    recovery.returnSurface = PythonRunReturnSurface::Device;
    PythonRunBlob blob = {};
    bool found = false;
    OperationResult result = readRunBlob(found, blob);
    recovery.found = found;
    recovery.attachmentAllowed = found;
    if (!result.success) {
        recovery.error = result.error;
        return recovery;
    }
    if (!found) {
        recovery.success = true;
        return recovery;
    }
    recovery.pendingId = blob.pendingId;
    recovery.auditSequence = blob.auditSequence;
    recovery.returnSurface = blob.returnSurface;
    if (blob.state == PythonRunState::Pending) {
        recovery.success = true;
        recovery.assistantMessage =
            "Python run was interrupted before execution; no code executed.";
        return recovery;
    }
    if (blob.state == PythonRunState::Claimed) {
        recovery.success = true;
        recovery.assistantMessage =
            "Python run was interrupted after launch; effects are unknown and the run was not replayed.";
        return recovery;
    }

    return decodeCompleteRecovery(blob, true);
}

PythonRunRecoveryResult loadDetachedPythonRunRecovery(
    const String& expectedPendingId,
    const String& expectedFileName,
    std::uint64_t expectedSourceBytes,
    const std::string& expectedSourceSha256)
{
    PythonRunRecoveryResult recovery = {};
    recovery.returnSurface = PythonRunReturnSurface::Device;
    const bool requestPresent = SD.exists(kPythonRunRequestPath);
    const bool resultPresent = SD.exists(kPythonRunResultPath);
    recovery.found = requestPresent || resultPresent;
    if (!recovery.found) {
        recovery.success = true;
        return recovery;
    }
    if (!requestPresent || !resultPresent ||
        SD.exists(kPythonRunRequestTemporaryPath) ||
        SD.exists(kPythonRunResultTemporaryPath) ||
        !validPendingId(expectedPendingId)) {
        recovery.error =
            "Detached Python recovery requires both exact request and result files";
        return recovery;
    }
    std::string requestBytes;
    OperationResult result = readBoundedFile(
        kPythonRunRequestPath, kPythonRunRequestMaximumBytes, requestBytes);
    if (!result.success) {
        recovery.error = result.error;
        return recovery;
    }
    DecodedPythonRunRequest request = {};
    result = decodePythonRunRequest(requestBytes, request);
    if (!result.success || request.pendingId != expectedPendingId ||
        request.path != expectedFileName ||
        request.sourceBytes != expectedSourceBytes ||
        request.sourceSha256 != expectedSourceSha256) {
        recovery.error = result.success
            ? String("Detached Python request does not match the frozen pending target")
            : result.error;
        return recovery;
    }
    std::string resultBytes;
    result = readBoundedFile(
        kPythonRunResultPath, kPythonRunResultMaximumBytes, resultBytes);
    if (!result.success) {
        recovery.error = result.error;
        return recovery;
    }
    PythonRunBlob blob = {
        PythonRunState::Complete,
        request.returnSurface,
        request.pendingId,
        {},
        {},
        request.auditSequence,
    };
    if (!sha256Bytes(
            reinterpret_cast<const std::uint8_t*>(requestBytes.data()),
            requestBytes.size(), blob.requestSha256) ||
        !sha256Bytes(
            reinterpret_cast<const std::uint8_t*>(resultBytes.data()),
            resultBytes.size(), blob.resultSha256)) {
        recovery.error = "Failed to hash detached Python recovery files";
        return recovery;
    }
    return decodeCompleteRecovery(blob, false);
}

OperationResult validateDetachedPythonRunRequest(
    const String& expectedPendingId,
    const String& expectedFileName,
    std::uint64_t expectedSourceBytes,
    const std::string& expectedSourceSha256)
{
    if (!SD.exists(kPythonRunRequestPath) ||
        SD.exists(kPythonRunRequestTemporaryPath) ||
        SD.exists(kPythonRunResultPath) ||
        SD.exists(kPythonRunResultTemporaryPath)) {
        return {false, "Detached Python request is not the exact request-only state"};
    }
    std::string requestBytes;
    OperationResult result = readBoundedFile(
        kPythonRunRequestPath, kPythonRunRequestMaximumBytes, requestBytes);
    if (!result.success) return result;
    DecodedPythonRunRequest request = {};
    result = decodePythonRunRequest(requestBytes, request);
    if (!result.success) return result;
    if (request.pendingId != expectedPendingId ||
        request.path != expectedFileName ||
        request.sourceBytes != expectedSourceBytes ||
        request.sourceSha256 != expectedSourceSha256) {
        return {false, "Detached Python request does not match the frozen pending target"};
    }
    if (!SD.exists(kPythonRunRequestPath) ||
        SD.exists(kPythonRunRequestTemporaryPath) ||
        SD.exists(kPythonRunResultPath) ||
        SD.exists(kPythonRunResultTemporaryPath)) {
        return {false, "Detached Python request artifacts changed during validation"};
    }
    return {true, ""};
}

OperationResult finalizePythonRunHandoff(
    PythonRunReturnSurface returnSurface)
{
    if (!validReturnSurface(returnSurface)) {
        return {false, "Python run return surface is invalid"};
    }
    Preferences preferences;
    if (!preferences.begin(kPythonNamespace, false)) {
        return {false, "Failed to open Python run cleanup state"};
    }
    OperationResult result = {true, ""};
    if (returnSurface == PythonRunReturnSurface::Web &&
        (preferences.putInt("open_web", 1) != sizeof(std::int32_t) ||
         preferences.getInt("open_web", 0) != 1)) {
        result = {false, "Failed to persist Python Web return marker"};
    }
    if (result.success) {
        result = removeKeyIfPresent(preferences, "run");
    }
    preferences.end();
    return result;
}

OperationResult discardPythonRunState()
{
    Preferences preferences;
    if (!preferences.begin(kPythonNamespace, false)) {
        return {false, "Failed to open Python run state for cleanup"};
    }
    const OperationResult removed = removeKeyIfPresent(preferences, "run");
    preferences.end();
    bool found = false;
    PythonRunBlob blob = {};
    const OperationResult verified = readRunBlob(found, blob);
    if (!removed.success) {
        String error = removed.error +
            "; Python run-state erase durability is unconfirmed";
        if (!verified.success) error += "; " + verified.error;
        else if (found) error += "; Python run state remains";
        return {false, error};
    }
    if (!verified.success) {
        return {false,
                "Failed to verify Python run-state absence after cleanup: " +
                    verified.error};
    }
    if (found) {
        return {false, removed.success
            ? String("Python run state remains after cleanup")
            : removed.error};
    }
    return {true, ""};
}

OperationResult stageCardMindUpdateForPython(std::uint32_t firmwareBytes,
                                             const String& sha256)
{
    const PythonModeStatus status = inspectPythonMode();
    if (!status.partitionLayoutReady || !status.pythonImageReady) {
        return {false, status.error};
    }
    if (firmwareBytes == 0 || firmwareBytes > status.cardMindPartitionBytes) {
        return {false, "CardMind update size does not fit its app partition"};
    }
    String normalizedSha = sha256;
    normalizedSha.toLowerCase();
    if (!validSha256(normalizedSha)) {
        return {false, "CardMind update SHA-256 is invalid"};
    }
    Preferences preferences;
    if (!preferences.begin(kPythonNamespace, false)) {
        return {false, "Failed to initialize NVS namespace 'cardmind_py'"};
    }
    OperationResult result = writeBlob(preferences, "upd_sha", normalizedSha, 65);
    if (result.success &&
        preferences.putInt("upd_size", static_cast<std::int32_t>(firmwareBytes)) !=
            sizeof(std::int32_t)) {
        result = {false, "Failed to store the pending CardMind update size"};
    }
    if (result.success && preferences.putInt("upd_pending", 1) != sizeof(std::int32_t)) {
        result = {false, "Failed to store the pending CardMind update marker"};
    }
    if (result.success &&
        (preferences.getInt("upd_size", -1) != static_cast<std::int32_t>(firmwareBytes) ||
         preferences.getInt("upd_pending", 0) != 1)) {
        result = {false, "Failed to verify the pending CardMind update metadata"};
    }
    if (result.success) {
        result = removeKeyIfPresent(preferences, "upd_error");
    }
    preferences.end();
    return result;
}

OperationResult clearCardMindUpdateRequest()
{
    Preferences preferences;
    if (!preferences.begin(kPythonNamespace, false)) {
        return {false, "Failed to initialize NVS namespace 'cardmind_py'"};
    }
    OperationResult result = removeKeyIfPresent(preferences, "upd_pending");
    if (result.success) {
        result = removeKeyIfPresent(preferences, "upd_size");
    }
    if (result.success) {
        result = removeKeyIfPresent(preferences, "upd_sha");
    }
    preferences.end();
    return result;
}

}  // namespace cardputer
