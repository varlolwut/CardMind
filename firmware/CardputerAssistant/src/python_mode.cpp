#include "python_mode.h"

#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include <array>
#include <cctype>

namespace cardputer {
namespace {

constexpr const char* kPythonNamespace = "cardmind_py";
constexpr const char* kCardMindPartitionLabel = "cardmind";
constexpr const char* kPythonPartitionLabel = "python";
constexpr std::uint32_t kMinimumCardMindPartitionBytes = 0x330000U;
constexpr std::uint32_t kMinimumPythonPartitionBytes = 0x1a0000U;
constexpr std::uint8_t kEspImageMagic = 0xe9U;

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

OperationResult synchronizePythonModeSettings(const Settings& settings,
                                              const String& consolePassword)
{
    if (settings.wifiSsid.isEmpty()) {
        return {false, "Python mode requires a configured Wi-Fi SSID"};
    }
    if (consolePassword.length() < 8) {
        return {false, "Python mode requires an installation password of at least 8 characters"};
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
        result = removeKeyIfPresent(preferences, "mode_error");
    }
    preferences.end();
    return result;
}

OperationResult activatePythonMode()
{
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
