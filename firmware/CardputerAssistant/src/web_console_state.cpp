#include "web_console_state.h"

#include "python_mode.h"
#include "sd_storage.h"
#include "text_utils.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

namespace cardputer {
namespace {

constexpr std::size_t kMaximumStateHistoryBytes = 12000;
constexpr const char* kCapabilityIds[kToolCapabilityCount] = {
    "ws", "wf", "fr", "fw", "sr", "sm", "sf", "py",
};

const char* scopedPermissionName(ScopedToolPermission permission)
{
    switch (permission) {
        case ScopedToolPermission::Inherit: return "inherit";
        case ScopedToolPermission::Off: return "off";
        case ScopedToolPermission::Ask: return "ask";
        case ScopedToolPermission::Allow: return "allow";
        case ScopedToolPermission::Count: break;
    }
    return nullptr;
}

const char* permissionDecisionName(ToolPermissionDecision decision)
{
    switch (decision) {
        case ToolPermissionDecision::Deny: return "deny";
        case ToolPermissionDecision::Ask: return "ask";
        case ToolPermissionDecision::Allow: return "allow";
        case ToolPermissionDecision::Unavailable: return "unavailable";
    }
    return nullptr;
}

const char* permissionSourceName(ToolPermissionSource source)
{
    switch (source) {
        case ToolPermissionSource::None: return "built_in";
        case ToolPermissionSource::BuiltIn: return "built_in";
        case ToolPermissionSource::Global: return "global";
        case ToolPermissionSource::Project: return "project";
        case ToolPermissionSource::Chat: return "chat";
        case ToolPermissionSource::Message: return "message";
        case ToolPermissionSource::Availability: return "availability";
    }
    return nullptr;
}

void appendChatMessages(const ChatDocument& chat, JsonDocument& document)
{
    std::size_t firstMessage = chat.messages.size();
    std::size_t includedBytes = 0;
    while (firstMessage > 0) {
        const std::size_t messageBytes = chat.messages[firstMessage - 1].content.size();
        if (includedBytes + messageBytes > kMaximumStateHistoryBytes) {
            break;
        }
        includedBytes += messageBytes;
        --firstMessage;
    }
    JsonArray messages = document["messages"].to<JsonArray>();
    for (std::size_t index = firstMessage; index < chat.messages.size(); ++index) {
        JsonObject item = messages.add<JsonObject>();
        item["role"] = chat.messages[index].role;
        item["content"] = chat.messages[index].content;
    }
}

}  // namespace

void buildWebConsoleStatusState(const WebConsoleRuntimeState& runtime,
                                bool diagnosticsEnabled,
                                JsonDocument& document)
{
    const SdStorageStatus storage = inspectSdStorage();
    const std::uint64_t freeBytes = storage.totalBytes >= storage.usedBytes
        ? storage.totalBytes - storage.usedBytes
        : 0;
    document["ok"] = true;
    document["ip"] = WiFi.localIP().toString();
    document["battery"] = M5Cardputer.Power.getBatteryLevel();
    document["free_heap"] = ESP.getFreeHeap();
    document["largest_heap"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    document["wifi_rssi"] = WiFi.RSSI();
    document["status"] = runtime.status;
    document["firmware_version"] = runtime.firmwareVersion;
    document["uptime_ms"] = millis();
    document["minimum_heap"] = ESP.getMinFreeHeap();
    document["stack_free"] = uxTaskGetStackHighWaterMark(nullptr);
    document["cpu_mhz"] = getCpuFrequencyMhz();
    document["reset_reason"] = static_cast<int>(esp_reset_reason());
    document["diagnostic_metrics_enabled"] = diagnosticsEnabled;
    document["sd_state"] = sdStorageStateName(storage.state);
    document["sd_error_code"] = sdStorageErrorCode(storage.state);
    document["sd_error"] = storage.error;
    document["sd_readable"] = storage.state == SdStorageState::Ready ||
                              storage.state == SdStorageState::Full;
    document["sd_writable"] = storage.state == SdStorageState::Ready;
    document["sd_total_bytes"] = storage.totalBytes;
    document["sd_used_bytes"] = storage.usedBytes;
    document["sd_free_bytes"] = freeBytes;
}

void buildWebConsoleChatsState(const std::vector<ChatSummary>& chats,
                               std::uint32_t revision,
                               JsonDocument& document)
{
    document["ok"] = true;
    document["chats_revision"] = revision;
    JsonArray items = document["chats"].to<JsonArray>();
    for (const auto& chat : chats) {
        JsonObject item = items.add<JsonObject>();
        item["id"] = chat.id;
        item["title"] = chat.title;
        item["pinned"] = chat.pinned;
        item["archived"] = chat.archived;
        item["total_messages"] = chat.messageCount + chat.archivedMessageCount;
    }
}

OperationResult buildWebConsoleChatState(
    const Settings& settings,
    const ProjectDocument& activeProject,
    const ChatDocument& activeChat,
    const ToolPolicyResolutionResult& toolPermissions,
    std::uint64_t availableSshProfileId,
    std::size_t maximumContextBytes,
    std::uint32_t revision,
    JsonDocument& document)
{
    const ToolPolicyEncodeResult projectPolicy =
        encodeScopedToolPermissionPolicy(activeProject.toolPolicy);
    const ToolPolicyEncodeResult chatPolicy =
        encodeScopedToolPermissionPolicy(activeChat.toolPolicy);
    if (projectPolicy.error != ToolPolicyCodecError::None ||
        chatPolicy.error != ToolPolicyCodecError::None ||
        toolPermissions.error != ToolPolicyContractError::None) {
        return {false, "Tool permission policy could not be encoded"};
    }
    const EncodedSshProfileId encodedAvailableProfile =
        encodeSshProfileId(availableSshProfileId);
    if (availableSshProfileId != 0 &&
        encodedAvailableProfile.error != SshProfileIdCodecError::None) {
        return {false, "Available SSH profile ID could not be encoded"};
    }
    for (std::size_t index = 0; index < kToolCapabilityCount; ++index) {
        if (scopedPermissionName(activeProject.toolPolicy[index]) == nullptr ||
            scopedPermissionName(activeChat.toolPolicy[index]) == nullptr ||
            permissionDecisionName(toolPermissions.permissions[index].decision) == nullptr ||
            permissionSourceName(toolPermissions.permissions[index].source) == nullptr) {
            return {false, "Tool permission state contains an invalid value"};
        }
    }
    document["ok"] = true;
    document["chat_revision"] = revision;
    document["model"] = settings.model;
    document["active_chat_id"] = activeChat.summary.id;
    document["active_chat_title"] = activeChat.summary.title;
    const ContextUsage contextUsage = resolveContextUsage(
        activeChat, maximumContextBytes);
    document["active_context_messages"] = contextUsage.retainedMessages;
    document["dropped_context_messages"] = contextUsage.droppedMessages;
    document["archived_messages"] = activeChat.summary.archivedMessageCount;
    document["active_context_bytes"] = contextUsage.retainedBytes;
    document["maximum_context_messages"] = 0;
    document["maximum_context_bytes"] = maximumContextBytes;
    document["instructions"] = activeChat.instructions;
    document["project_tool_policy"] = JsonString(
        projectPolicy.encoded.value.data(), kEncodedToolPolicyLength,
        JsonString::Copied);
    document["chat_tool_policy"] = JsonString(
        chatPolicy.encoded.value.data(), kEncodedToolPolicyLength,
        JsonString::Copied);
    document["project_ssh_profile"] = activeProject.sshProfile;
    document["chat_ssh_profile"] = activeChat.sshProfile;
    document["project_ssh_profile_matches"] = sshProfileCeilingsAllowSelected(
        availableSshProfileId,
        activeProject.sshProfile.c_str(), activeProject.sshProfile.length(),
        "", 0);
    document["chat_ssh_profile_matches"] = sshProfileCeilingsAllowSelected(
        availableSshProfileId,
        activeProject.sshProfile.c_str(), activeProject.sshProfile.length(),
        activeChat.sshProfile.c_str(), activeChat.sshProfile.length());
    document["ssh_available_profile_id"] = "";
    if (availableSshProfileId != 0) {
        document["ssh_available_profile_id"] = JsonString(
            encodedAvailableProfile.value.data(), encodedAvailableProfile.length,
            JsonString::Copied);
    }
    document["chat_model"] = activeChat.model;
    document["ssh_tools_enabled"] = legacySshToolsEnabled(
        activeChat.toolPolicy);
    JsonArray capabilities = document["capabilities"].to<JsonArray>();
    for (std::size_t index = 0; index < kToolCapabilityCount; ++index) {
        JsonObject capability = capabilities.add<JsonObject>();
        capability["id"] = kCapabilityIds[index];
        capability["raw_project"] = scopedPermissionName(
            activeProject.toolPolicy[index]);
        capability["raw_chat"] = scopedPermissionName(
            activeChat.toolPolicy[index]);
        capability["effective"] = permissionDecisionName(
            toolPermissions.permissions[index].decision);
        capability["source"] = permissionSourceName(
            toolPermissions.permissions[index].source);
    }
    appendChatMessages(activeChat, document);
    return {true, ""};
}

void buildWebConsoleFilesState(const std::vector<WorkspaceFile>& files,
                               std::uint64_t totalBytes,
                               std::uint64_t usedBytes,
                               std::uint32_t revision,
                               JsonDocument& document)
{
    document["ok"] = true;
    document["files_revision"] = revision;
    document["sd_total_bytes"] = totalBytes;
    document["sd_used_bytes"] = usedBytes;
    JsonArray items = document["files"].to<JsonArray>();
    for (const auto& file : files) {
        JsonObject item = items.add<JsonObject>();
        item["name"] = file.name;
        item["size"] = file.size;
        item["editable"] = isWorkspaceTextFile(std::string(file.name.c_str()));
    }
}

OperationResult buildWebConsoleSshState(
    const std::vector<SshProfileSummary>& profiles,
    std::size_t selected,
    bool selectedConfigured,
    bool privateKeyInstalled,
    const WebConsoleRuntimeState& runtime,
    std::uint32_t revision,
    JsonDocument& document)
{
    if (!profiles.empty() && selected >= profiles.size()) {
        return {false, "Selected SSH profile index is invalid"};
    }
    const SshProfileSummary profile = profiles.empty()
        ? SshProfileSummary{0, "", "", 22, "", SshAuthMode::Password}
        : profiles[selected];
    document["ok"] = true;
    document["ssh_revision"] = revision;
    document["ssh_name"] = profile.name;
    document["ssh_host"] = profile.host;
    document["ssh_port"] = profile.port;
    document["ssh_username"] = profile.username;
    document["ssh_auth_mode"] =
        profile.authMode == SshAuthMode::PrivateKey ? "key" : "password";
    document["ssh_selected"] = selected;
    document["ssh_terminal_open"] = runtime.sshTerminalOpen;
    JsonArray items = document["ssh_profiles"].to<JsonArray>();
    for (const auto& item : profiles) {
        const EncodedSshProfileId encoded = encodeSshProfileId(item.id);
        if (encoded.error != SshProfileIdCodecError::None) {
            return {false, "SSH profile ID could not be encoded"};
        }
        JsonObject profileItem = items.add<JsonObject>();
        profileItem["id"] = JsonString(
            encoded.value.data(), encoded.length, JsonString::Copied);
        profileItem["name"] = item.name;
        profileItem["host"] = item.host;
        profileItem["port"] = item.port;
        profileItem["username"] = item.username;
        profileItem["auth_mode"] =
            item.authMode == SshAuthMode::PrivateKey ? "key" : "password";
    }
    document["ssh_key_installed"] = privateKeyInstalled;
    document["ssh_configured"] = selectedConfigured;
    return {true, ""};
}

OperationResult buildWebConsoleSettingsState(
    const Settings& settings,
    const WebConsoleRuntimeState& runtime,
    std::uint32_t revision,
    JsonDocument& document)
{
    const ToolPolicyEncodeResult masterPolicy =
        encodeToolPermissionPolicy(settings.masterToolPolicy);
    const ToolPolicyEncodeResult newChatPolicy =
        encodeScopedToolPermissionPolicy(settings.newChatToolPolicy);
    if (masterPolicy.error != ToolPolicyCodecError::None ||
        newChatPolicy.error != ToolPolicyCodecError::None) {
        return {false, "Tool permission policy could not be encoded"};
    }
    document["ok"] = true;
    document["settings_revision"] = revision;
    document["firmware_version"] = runtime.firmwareVersion;
    document["wifi_ssid"] = settings.wifiSsid;
    document["model"] = settings.model;
    document["global_instructions"] = settings.globalInstructions;
    document["master_tool_policy"] = JsonString(
        masterPolicy.encoded.value.data(), kEncodedToolPolicyLength,
        JsonString::Copied);
    document["new_chat_tool_policy"] = JsonString(
        newChatPolicy.encoded.value.data(), kEncodedToolPolicyLength,
        JsonString::Copied);
    document["project_chat_history_quota_bytes"] =
        settings.projectChatHistoryQuotaBytes;
    document["api_base_url"] = settings.apiBaseUrl;
    document["api_key_configured"] = settings.apiKey.length() >= 8;
    document["stt_base_url"] = settings.sttBaseUrl;
    document["stt_model"] = settings.sttModel;
    document["stt_key_configured"] = settings.sttApiKey.length() >= 8;
    document["search_base_url"] = settings.webSearchBaseUrl;
    document["search_key_configured"] = settings.webSearchApiKey.length() >= 8;
    document["tts_base_url"] = settings.ttsBaseUrl;
    document["tts_model"] = settings.ttsModel;
    document["tts_voice"] = settings.ttsVoice;
    document["tts_key_configured"] = settings.ttsApiKey.length() >= 8;
    document["tts_auto_play"] = settings.ttsAutoPlay;
    document["tts_volume"] = settings.ttsVolume;
    document["display_brightness"] = settings.displayBrightness;
    document["screen_sleep_minutes"] = settings.screenSleepMinutes;
    document["keyboard_repeat_ms"] = settings.keyboardRepeatMs;
    document["power_profile"] = settings.powerProfile;
    const PythonModeStatus python = inspectPythonMode();
    document["python_layout_ready"] = python.partitionLayoutReady;
    document["python_image_ready"] = python.pythonImageReady;
    document["python_error"] = python.error;
    document["python_runtime_error"] = python.lastRuntimeError;
    return {true, ""};
}

}  // namespace cardputer
