#include "storage.h"

#include "text_utils.h"
#include "tool_policy_codec.h"

#include <Preferences.h>

#include <cctype>

namespace cardputer {
namespace {

constexpr const char* kNamespace = "assistant";
constexpr const char* kSdVolumeIdentityKey = "sd_volume_id";
constexpr const char* kToolPolicyKey = "tool_policy";
constexpr std::size_t kSdVolumeIdentityBytes = 16;
constexpr std::size_t kGlobalToolPolicyRecordLength =
    1 + (2 * kEncodedToolPolicyLength);

struct EncodedGlobalToolPoliciesResult {
    bool success;
    String value;
    String error;
};

struct DecodedGlobalToolPoliciesResult {
    bool success;
    ToolPermissionPolicy master;
    ScopedToolPermissionPolicy newChat;
    String error;
};

EncodedGlobalToolPoliciesResult encodeGlobalToolPolicies(
    const ToolPermissionPolicy& master,
    const ScopedToolPermissionPolicy& newChat)
{
    const ToolPolicyEncodeResult encodedMaster =
        encodeToolPermissionPolicy(master);
    if (encodedMaster.error != ToolPolicyCodecError::None) {
        return {
            false,
            "",
            String("Cannot store Master access: ") +
                toolPolicyCodecErrorText(encodedMaster.error),
        };
    }
    const ToolPolicyEncodeResult encodedNewChat =
        encodeScopedToolPermissionPolicy(newChat);
    if (encodedNewChat.error != ToolPolicyCodecError::None) {
        return {
            false,
            "",
            String("Cannot store default policy for new chats: ") +
                toolPolicyCodecErrorText(encodedNewChat.error),
        };
    }
    const String value = String(encodedMaster.encoded.value.data()) +
        "|" + encodedNewChat.encoded.value.data();
    return {true, value, ""};
}

DecodedGlobalToolPoliciesResult decodeGlobalToolPolicies(
    const String& value)
{
    const ToolPermissionPolicy defaultMaster =
        defaultGlobalToolPermissionPolicy();
    const ScopedToolPermissionPolicy defaultNewChat =
        defaultNewChatToolPermissionPolicy();
    if (value.length() != kGlobalToolPolicyRecordLength) {
        return {
            false,
            defaultMaster,
            defaultNewChat,
            "Stored global tool policy has an invalid length",
        };
    }
    if (value[kEncodedToolPolicyLength] != '|') {
        return {
            false,
            defaultMaster,
            defaultNewChat,
            "Stored global tool policy has an invalid separator",
        };
    }
    const ToolPermissionPolicyDecodeResult decodedMaster =
        decodeToolPermissionPolicy(value.c_str(), kEncodedToolPolicyLength);
    if (decodedMaster.error != ToolPolicyCodecError::None) {
        return {
            false,
            defaultMaster,
            defaultNewChat,
            String("Stored Master access is invalid: ") +
                toolPolicyCodecErrorText(decodedMaster.error),
        };
    }
    const ScopedToolPermissionPolicyDecodeResult decodedNewChat =
        decodeScopedToolPermissionPolicy(
            value.c_str() + 1 + kEncodedToolPolicyLength,
            kEncodedToolPolicyLength);
    if (decodedNewChat.error != ToolPolicyCodecError::None) {
        return {
            false,
            defaultMaster,
            defaultNewChat,
            String("Stored default policy for new chats is invalid: ") +
                toolPolicyCodecErrorText(decodedNewChat.error),
        };
    }
    return {true, decodedMaster.policy, decodedNewChat.policy, ""};
}

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

OperationResult verifyStoredLength(std::size_t storedLength, std::size_t expectedLength, const char* field)
{
    if (storedLength == expectedLength) {
        return {true, ""};
    }
    return {false, String("Failed to store ") + field + ": NVS wrote " + storedLength +
                       " bytes, expected " + expectedLength};
}

OperationResult verifyStoredValue(const String& storedValue, const String& expectedValue, const char* field)
{
    if (storedValue == expectedValue) {
        return {true, ""};
    }
    return {false, String("Failed to verify ") + field + " after NVS write"};
}

bool isSafeTtsIdentifier(const String& value)
{
    for (std::size_t index = 0; index < value.length(); ++index) {
        const char character = value[index];
        if (!std::isalnum(static_cast<unsigned char>(character)) &&
            character != '-' && character != '_') {
            return false;
        }
    }
    return true;
}

}  // namespace

bool isValidProjectChatHistoryQuota(std::uint32_t quotaBytes)
{
    return quotaBytes == 0 || quotaBytes >= kMinimumProjectChatHistoryQuotaBytes;
}

OperationResult loadSettings(Settings& settings)
{
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        return {false, "Failed to initialize NVS namespace 'assistant'"};
    }
    Settings loaded = {
        preferences.getString("ssid", ""),
        preferences.getString("wifi_pass", ""),
        preferences.getString("api_key", ""),
        preferences.getString("base_url", ""),
        preferences.getString("model", ""),
        preferences.getString("global_inst", ""),
        preferences.getString("stt_key", ""),
        preferences.getString("stt_url", ""),
        preferences.getString("stt_model", ""),
        preferences.getString("search_key", ""),
        preferences.getString("search_url", ""),
        preferences.getString("tts_key", ""),
        preferences.getString("tts_url", ""),
        preferences.getString("tts_model", ""),
        preferences.getString("tts_voice", ""),
        preferences.getBool("tts_auto", false),
        preferences.getUChar("tts_volume", 192),
        preferences.getUChar("brightness", 192),
        preferences.getUShort("sleep_min", 5),
        preferences.getUShort("key_repeat", 125),
        preferences.getUChar("power", 1),
        preferences.getUInt("chat_quota", 0),
    };
    const PreferenceType policyType = preferences.getType(kToolPolicyKey);
    if (policyType == PT_INVALID) {
        const EncodedGlobalToolPoliciesResult encoded = encodeGlobalToolPolicies(
            loaded.masterToolPolicy, loaded.newChatToolPolicy);
        if (!encoded.success) {
            preferences.end();
            return {false, encoded.error};
        }
        OperationResult migration = verifyStoredLength(
            preferences.putString(kToolPolicyKey, encoded.value),
            encoded.value.length(),
            "global tool policy");
        if (migration.success) {
            migration = verifyStoredValue(
                preferences.getString(kToolPolicyKey, ""),
                encoded.value,
                "global tool policy");
        }
        if (!migration.success) {
            preferences.end();
            return {false, migration.error};
        }
    } else if (policyType != PT_STR) {
        preferences.end();
        return {false, "Stored global tool policy has the wrong NVS type"};
    } else {
        const DecodedGlobalToolPoliciesResult decoded =
            decodeGlobalToolPolicies(preferences.getString(kToolPolicyKey, ""));
        if (!decoded.success) {
            preferences.end();
            return {false, decoded.error};
        }
        loaded.masterToolPolicy = decoded.master;
        loaded.newChatToolPolicy = decoded.newChat;
    }
    preferences.end();
    loaded.apiKey.trim();
    loaded.sttApiKey.trim();
    loaded.webSearchApiKey.trim();
    loaded.ttsApiKey.trim();
    settings = loaded;
    return {true, ""};
}

OperationResult saveSettings(const Settings& settings)
{
    if (settings.wifiSsid.isEmpty()) {
        return {false, "Wi-Fi SSID must not be empty"};
    }
    if (settings.apiKey.length() < 8) {
        return {false, "API key must contain at least 8 characters"};
    }
    if (!settings.apiBaseUrl.startsWith("https://") || settings.apiBaseUrl.length() < 12 ||
        settings.apiBaseUrl.length() > 180 || settings.apiBaseUrl.indexOf(' ') >= 0 ||
        settings.apiBaseUrl.indexOf('?') >= 0 || settings.apiBaseUrl.indexOf('#') >= 0) {
        return {false, "API base URL must be an https:// URL without spaces, query, or fragment"};
    }
    if (settings.model.isEmpty()) {
        return {false, "Model id must not be empty"};
    }
    if (settings.globalInstructions.length() > 2048 ||
        !isValidUtf8(std::string(settings.globalInstructions.c_str()))) {
        return {false, "Global instructions must be valid UTF-8 and at most 2048 bytes"};
    }
    if (!settings.sttApiKey.isEmpty() && settings.sttApiKey.length() < 8) {
        return {false, "STT API key must contain at least 8 characters"};
    }
    if (!settings.sttBaseUrl.isEmpty() &&
        (!settings.sttBaseUrl.startsWith("https://") || settings.sttBaseUrl.length() < 12 ||
         settings.sttBaseUrl.length() > 180 || settings.sttBaseUrl.indexOf(' ') >= 0 ||
         settings.sttBaseUrl.indexOf('?') >= 0 || settings.sttBaseUrl.indexOf('#') >= 0)) {
        return {false, "STT base URL must be an https:// URL without spaces, query, or fragment"};
    }
    if (settings.sttModel.length() > 80) {
        return {false, "STT model id must not exceed 80 characters"};
    }
    if (!settings.webSearchApiKey.isEmpty() && settings.webSearchApiKey.length() < 8) {
        return {false, "Web search API key must contain at least 8 characters"};
    }
    if (!settings.webSearchBaseUrl.isEmpty() &&
        (!settings.webSearchBaseUrl.startsWith("https://") ||
         settings.webSearchBaseUrl.length() < 12 || settings.webSearchBaseUrl.length() > 180 ||
         settings.webSearchBaseUrl.indexOf(' ') >= 0 ||
         settings.webSearchBaseUrl.indexOf('?') >= 0 ||
         settings.webSearchBaseUrl.indexOf('#') >= 0)) {
        return {false, "Web search base URL must be an https:// URL without spaces, query, or fragment"};
    }
    if (!settings.ttsApiKey.isEmpty() && settings.ttsApiKey.length() < 8) {
        return {false, "TTS API key must contain at least 8 characters"};
    }
    if (!settings.ttsBaseUrl.isEmpty() &&
        (!settings.ttsBaseUrl.startsWith("https://") || settings.ttsBaseUrl.length() < 12 ||
         settings.ttsBaseUrl.length() > 180 || settings.ttsBaseUrl.indexOf(' ') >= 0 ||
         settings.ttsBaseUrl.indexOf('?') >= 0 || settings.ttsBaseUrl.indexOf('#') >= 0)) {
        return {false, "TTS base URL must be an https:// URL without spaces, query, or fragment"};
    }
    if (settings.ttsModel.length() > 80 || settings.ttsVoice.length() > 80) {
        return {false, "TTS model and voice ids must not exceed 80 characters"};
    }
    if ((!settings.ttsModel.isEmpty() && !isSafeTtsIdentifier(settings.ttsModel)) ||
        (!settings.ttsVoice.isEmpty() && !isSafeTtsIdentifier(settings.ttsVoice))) {
        return {false, "TTS model and voice ids may contain only letters, digits, '-' and '_'"};
    }
    if (settings.displayBrightness < 32) {
        return {false, "Display brightness must be at least 32"};
    }
    if (settings.screenSleepMinutes != 0 && settings.screenSleepMinutes != 1 &&
        settings.screenSleepMinutes != 5 && settings.screenSleepMinutes != 10 &&
        settings.screenSleepMinutes != 30) {
        return {false, "Screen sleep must be Off, 1, 5, 10, or 30 minutes"};
    }
    if (settings.keyboardRepeatMs != 0 && settings.keyboardRepeatMs != 75 &&
        settings.keyboardRepeatMs != 125 && settings.keyboardRepeatMs != 200) {
        return {false, "Keyboard repeat must be Off, 75, 125, or 200 ms"};
    }
    if (settings.powerProfile > 2) {
        return {false, "Power profile must be Performance, Balanced, or Saver"};
    }
    if (!isValidProjectChatHistoryQuota(settings.projectChatHistoryQuotaBytes)) {
        return {false, "Chat history quota must be 0 or at least 2 MiB"};
    }
    const EncodedGlobalToolPoliciesResult encodedToolPolicies =
        encodeGlobalToolPolicies(
            settings.masterToolPolicy, settings.newChatToolPolicy);
    if (!encodedToolPolicies.success) {
        return {false, encodedToolPolicies.error};
    }

    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        return {false, "Failed to open NVS namespace 'assistant' for writing"};
    }
    OperationResult result = verifyStoredLength(
        preferences.putString("ssid", settings.wifiSsid), settings.wifiSsid.length(), "Wi-Fi SSID");
    if (result.success) {
        result = verifyStoredLength(preferences.putString("wifi_pass", settings.wifiPassword),
                                    settings.wifiPassword.length(), "Wi-Fi password");
    }
    if (result.success) {
        result = verifyStoredLength(preferences.putString("api_key", settings.apiKey),
                                    settings.apiKey.length(), "API key");
    }
    if (result.success) {
        result = verifyStoredLength(preferences.putString("base_url", settings.apiBaseUrl),
                                    settings.apiBaseUrl.length(), "API base URL");
    }
    if (result.success) {
        result = verifyStoredLength(
            preferences.putString("model", settings.model), settings.model.length(), "model id");
    }
    if (result.success) {
        result = verifyStoredLength(
            preferences.putString("global_inst", settings.globalInstructions),
            settings.globalInstructions.length(), "global instructions");
    }
    if (result.success) {
        result = verifyStoredLength(preferences.putString("stt_key", settings.sttApiKey),
                                    settings.sttApiKey.length(), "STT API key");
    }
    if (result.success) {
        result = verifyStoredLength(preferences.putString("stt_url", settings.sttBaseUrl),
                                    settings.sttBaseUrl.length(), "STT base URL");
    }
    if (result.success) {
        result = verifyStoredLength(preferences.putString("stt_model", settings.sttModel),
                                    settings.sttModel.length(), "STT model id");
    }
    if (result.success) {
        result = verifyStoredLength(preferences.putString("search_key", settings.webSearchApiKey),
                                    settings.webSearchApiKey.length(), "web search API key");
    }
    if (result.success) {
        result = verifyStoredLength(preferences.putString("search_url", settings.webSearchBaseUrl),
                                    settings.webSearchBaseUrl.length(), "web search base URL");
    }
    if (result.success) {
        result = verifyStoredLength(preferences.putString("tts_key", settings.ttsApiKey),
                                    settings.ttsApiKey.length(), "TTS API key");
    }
    if (result.success) {
        result = verifyStoredLength(preferences.putString("tts_url", settings.ttsBaseUrl),
                                    settings.ttsBaseUrl.length(), "TTS base URL");
    }
    if (result.success) {
        result = verifyStoredLength(preferences.putString("tts_model", settings.ttsModel),
                                    settings.ttsModel.length(), "TTS model id");
    }
    if (result.success) {
        result = verifyStoredLength(preferences.putString("tts_voice", settings.ttsVoice),
                                    settings.ttsVoice.length(), "TTS voice id");
    }
    if (result.success && preferences.putBool("tts_auto", settings.ttsAutoPlay) != 1) {
        result = {false, "Failed to store TTS auto-play setting"};
    }
    if (result.success && preferences.putUChar("tts_volume", settings.ttsVolume) != 1) {
        result = {false, "Failed to store TTS playback volume"};
    }
    if (result.success && preferences.putUChar("brightness", settings.displayBrightness) != 1) {
        result = {false, "Failed to store display brightness"};
    }
    if (result.success && preferences.putUShort("sleep_min", settings.screenSleepMinutes) != 2) {
        result = {false, "Failed to store screen sleep timeout"};
    }
    if (result.success && preferences.putUShort("key_repeat", settings.keyboardRepeatMs) != 2) {
        result = {false, "Failed to store keyboard repeat interval"};
    }
    if (result.success && preferences.putUChar("power", settings.powerProfile) != 1) {
        result = {false, "Failed to store power profile"};
    }
    if (result.success &&
        preferences.putUInt("chat_quota", settings.projectChatHistoryQuotaBytes) != 4) {
        result = {false, "Failed to store project chat history quota"};
    }
    if (result.success) {
        result = verifyStoredLength(
            preferences.putString(kToolPolicyKey, encodedToolPolicies.value),
            encodedToolPolicies.value.length(),
            "global tool policy");
    }
    if (result.success) {
        result = verifyStoredValue(preferences.getString("ssid", ""), settings.wifiSsid, "Wi-Fi SSID");
    }
    if (result.success) {
        result = verifyStoredValue(
            preferences.getString("wifi_pass", "__missing__"), settings.wifiPassword, "Wi-Fi password");
    }
    if (result.success) {
        result = verifyStoredValue(preferences.getString("api_key", ""), settings.apiKey, "API key");
    }
    if (result.success) {
        result = verifyStoredValue(
            preferences.getString("base_url", ""), settings.apiBaseUrl, "API base URL");
    }
    if (result.success) {
        result = verifyStoredValue(preferences.getString("model", ""), settings.model, "model id");
    }
    if (result.success) {
        result = verifyStoredValue(preferences.getString("global_inst", ""),
                                   settings.globalInstructions, "global instructions");
    }
    if (result.success) {
        result = verifyStoredValue(
            preferences.getString("stt_key", ""), settings.sttApiKey, "STT API key");
    }
    if (result.success) {
        result = verifyStoredValue(
            preferences.getString("stt_url", ""), settings.sttBaseUrl, "STT base URL");
    }
    if (result.success) {
        result = verifyStoredValue(
            preferences.getString("stt_model", ""), settings.sttModel, "STT model id");
    }
    if (result.success) {
        result = verifyStoredValue(preferences.getString("search_key", ""),
                                   settings.webSearchApiKey, "web search API key");
    }
    if (result.success) {
        result = verifyStoredValue(preferences.getString("search_url", ""),
                                   settings.webSearchBaseUrl, "web search base URL");
    }
    if (result.success) {
        result = verifyStoredValue(preferences.getString("tts_key", ""),
                                   settings.ttsApiKey, "TTS API key");
    }
    if (result.success) {
        result = verifyStoredValue(preferences.getString("tts_url", ""),
                                   settings.ttsBaseUrl, "TTS base URL");
    }
    if (result.success) {
        result = verifyStoredValue(preferences.getString("tts_model", ""),
                                   settings.ttsModel, "TTS model id");
    }
    if (result.success) {
        result = verifyStoredValue(preferences.getString("tts_voice", ""),
                                   settings.ttsVoice, "TTS voice id");
    }
    if (result.success && preferences.getBool("tts_auto", !settings.ttsAutoPlay) != settings.ttsAutoPlay) {
        result = {false, "Failed to verify TTS auto-play setting after NVS write"};
    }
    if (result.success && preferences.getUChar("tts_volume", 0) != settings.ttsVolume) {
        result = {false, "Failed to verify TTS playback volume after NVS write"};
    }
    if (result.success && preferences.getUChar("brightness", 0) != settings.displayBrightness) {
        result = {false, "Failed to verify display brightness after NVS write"};
    }
    if (result.success && preferences.getUShort("sleep_min", 0) != settings.screenSleepMinutes) {
        result = {false, "Failed to verify screen sleep timeout after NVS write"};
    }
    if (result.success && preferences.getUShort("key_repeat", 1) != settings.keyboardRepeatMs) {
        result = {false, "Failed to verify keyboard repeat interval after NVS write"};
    }
    if (result.success && preferences.getUChar("power", 255) != settings.powerProfile) {
        result = {false, "Failed to verify power profile after NVS write"};
    }
    if (result.success &&
        preferences.getUInt("chat_quota", 1) != settings.projectChatHistoryQuotaBytes) {
        result = {false, "Failed to verify project chat history quota after NVS write"};
    }
    if (result.success) {
        result = verifyStoredValue(
            preferences.getString(kToolPolicyKey, ""),
            encodedToolPolicies.value,
            "global tool policy");
    }
    preferences.end();
    return result;
}

OperationResult saveModel(const String& model)
{
    if (model.isEmpty()) {
        return {false, "Model id must not be empty"};
    }
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        return {false, "Failed to open NVS namespace 'assistant' for model update"};
    }
    const std::size_t storedLength = preferences.putString("model", model);
    preferences.end();
    return verifyStoredLength(storedLength, model.length(), "model id");
}

OperationResult loadSetupAccessPointPassword(String& password)
{
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        return {false, "Failed to open NVS namespace 'assistant' for setup password"};
    }
    const String loaded = preferences.getString("setup_pass", "");
    preferences.end();
    if (!loaded.isEmpty() && (loaded.length() < 8 || loaded.length() > 63)) {
        return {false, "Stored setup access-point password has an invalid length"};
    }
    password = loaded;
    return {true, ""};
}

OperationResult saveSetupAccessPointPassword(const String& password)
{
    if (password.length() < 8 || password.length() > 63) {
        return {false, "Setup access-point password must contain 8 to 63 characters"};
    }
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        return {false, "Failed to open NVS namespace 'assistant' for setup password write"};
    }
    OperationResult result = verifyStoredLength(
        preferences.putString("setup_pass", password), password.length(), "setup access-point password");
    if (result.success) {
        result = verifyStoredValue(
            preferences.getString("setup_pass", ""), password, "setup access-point password");
    }
    preferences.end();
    return result;
}

OperationResult loadActiveChatId(String& id)
{
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        return {false, "Failed to open NVS namespace 'assistant' for active chat"};
    }
    const String loaded = preferences.getString("active_chat", "");
    preferences.end();
    if (!loaded.isEmpty() && !isValidChatId(loaded.c_str())) {
        return {false, "Stored active chat id is invalid"};
    }
    id = loaded;
    return {true, ""};
}

OperationResult saveActiveChatId(const String& id)
{
    if (!isValidChatId(id.c_str())) {
        return {false, "Cannot store an invalid active chat id"};
    }
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        return {false, "Failed to open NVS namespace 'assistant' for active chat write"};
    }
    OperationResult result = verifyStoredLength(
        preferences.putString("active_chat", id), id.length(), "active chat id");
    if (result.success) {
        result = verifyStoredValue(
            preferences.getString("active_chat", ""), id, "active chat id");
    }
    preferences.end();
    return result;
}

OperationResult loadSdVolumeIdentity(String& identity)
{
    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) {
        return {false, "Failed to open NVS namespace 'assistant' for microSD identity"};
    }
    const String loaded = preferences.getString(kSdVolumeIdentityKey, "");
    preferences.end();
    if (!loaded.isEmpty() && !isValidSdVolumeIdentity(loaded)) {
        return {false, "Stored microSD identity is invalid"};
    }
    identity = loaded;
    return {true, ""};
}

OperationResult saveSdVolumeIdentity(const String& identity)
{
    if (!isValidSdVolumeIdentity(identity)) {
        return {false, "Cannot store an invalid microSD identity"};
    }
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        return {false, "Failed to open NVS namespace 'assistant' for microSD identity write"};
    }
    OperationResult result = verifyStoredLength(
        preferences.putString(kSdVolumeIdentityKey, identity),
        identity.length(), "microSD identity");
    if (result.success) {
        result = verifyStoredValue(
            preferences.getString(kSdVolumeIdentityKey, ""), identity,
            "microSD identity");
    }
    preferences.end();
    return result;
}

bool settingsAreComplete(const Settings& settings)
{
    return !settings.wifiSsid.isEmpty() && settings.apiKey.length() >= 8 &&
           settings.apiBaseUrl.startsWith("https://") && !settings.model.isEmpty();
}

bool voiceSettingsAreComplete(const Settings& settings)
{
    return settings.sttApiKey.length() >= 8 && settings.sttBaseUrl.startsWith("https://") &&
           !settings.sttModel.isEmpty();
}

bool webSearchSettingsAreComplete(const Settings& settings)
{
    return settings.webSearchApiKey.length() >= 8 &&
           settings.webSearchBaseUrl.startsWith("https://");
}

bool ttsSettingsAreComplete(const Settings& settings)
{
    return settings.ttsApiKey.length() >= 8 && settings.ttsBaseUrl.startsWith("https://") &&
           !settings.ttsModel.isEmpty() && !settings.ttsVoice.isEmpty();
}

}  // namespace cardputer
