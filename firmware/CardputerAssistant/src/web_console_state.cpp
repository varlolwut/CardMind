#include "web_console_state.h"

#include "python_mode.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

namespace cardputer {
namespace {

constexpr std::size_t kMaximumStateHistoryBytes = 12000;

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

void buildWebConsoleChatState(const Settings& settings,
                              const ChatDocument& activeChat,
                              std::uint32_t revision,
                              JsonDocument& document)
{
    document["ok"] = true;
    document["chat_revision"] = revision;
    document["model"] = settings.model;
    document["active_chat_id"] = activeChat.summary.id;
    document["active_chat_title"] = activeChat.summary.title;
    document["active_context_messages"] = activeChat.summary.messageCount;
    document["archived_messages"] = activeChat.summary.archivedMessageCount;
    std::size_t activeContextBytes = 0;
    for (const auto& message : activeChat.messages) {
        activeContextBytes += message.content.size();
    }
    document["active_context_bytes"] = activeContextBytes;
    document["maximum_context_messages"] = kMaximumStoredMessages;
    document["maximum_context_bytes"] = kMaximumStoredHistoryBytes;
    document["instructions"] = activeChat.instructions;
    document["ssh_tools_enabled"] = activeChat.sshToolsEnabled;
    appendChatMessages(activeChat, document);
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
    }
}

void buildWebConsoleSshState(const std::vector<SshProfile>& profiles,
                             std::size_t selected,
                             bool privateKeyInstalled,
                             const WebConsoleRuntimeState& runtime,
                             std::uint32_t revision,
                             JsonDocument& document)
{
    const SshProfile profile = profiles.empty()
        ? SshProfile{"", "", 22, "", "", SshAuthMode::Password, ""}
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
        JsonObject profileItem = items.add<JsonObject>();
        profileItem["name"] = item.name;
        profileItem["host"] = item.host;
        profileItem["port"] = item.port;
        profileItem["username"] = item.username;
        profileItem["auth_mode"] =
            item.authMode == SshAuthMode::PrivateKey ? "key" : "password";
    }
    document["ssh_key_installed"] = privateKeyInstalled;
    document["ssh_configured"] = sshProfileIsComplete(profile);
}

void buildWebConsoleSettingsState(const Settings& settings,
                                  const WebConsoleRuntimeState& runtime,
                                  std::uint32_t revision,
                                  JsonDocument& document)
{
    document["ok"] = true;
    document["settings_revision"] = revision;
    document["firmware_version"] = runtime.firmwareVersion;
    document["wifi_ssid"] = settings.wifiSsid;
    document["model"] = settings.model;
    document["global_instructions"] = settings.globalInstructions;
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
}

}  // namespace cardputer
