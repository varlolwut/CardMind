#include "provisioning.h"

#include "storage.h"
#include "ui.h"
#include "wifi_networks.h"

#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

#include <vector>

namespace cardputer {
namespace {

constexpr const char* kDefaultModel = "claude-sonnet-4-6";
constexpr const char* kDefaultApiBaseUrl = "https://api.example.com";
constexpr const char* kDefaultSttBaseUrl = "https://api.groq.com/openai/v1";
constexpr const char* kDefaultSttModel = "whisper-large-v3-turbo";
constexpr const char* kDefaultWebSearchBaseUrl = "https://api.exa.ai";
constexpr const char* kDefaultTtsBaseUrl = "https://api.elevenlabs.io";
constexpr const char* kDefaultTtsModel = "eleven_multilingual_v2";
constexpr const char* kDefaultTtsVoice = "JBFqnCBsd6RMkjVDRZzb";
constexpr std::uint32_t kRestartDelayMs = 5000;
WebServer webServer(80);
Settings currentSettings;
WifiScanResult nearbyNetworkScan = {true, {}, ""};
bool restartPending = false;
std::uint32_t restartAt = 0;
String serialCommand;

String htmlEscape(const String& value)
{
    String result;
    result.reserve(value.length() + 16);
    for (std::size_t index = 0; index < value.length(); ++index) {
        const char character = value[index];
        switch (character) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += character; break;
        }
    }
    return result;
}

bool containsNetwork(const std::vector<WifiNetwork>& networks, const String& ssid)
{
    for (const auto& network : networks) {
        if (network.ssid == ssid) {
            return true;
        }
    }
    return false;
}

String networkOptions(const std::vector<WifiNetwork>& networks, const String& selectedSsid)
{
    String options;
    for (const auto& network : networks) {
        options += "<option value='" + htmlEscape(network.ssid) + "'";
        if (network.ssid == selectedSsid) {
            options += " selected";
        }
        options += ">" + htmlEscape(network.ssid) + " (" + String(network.rssi) + " dBm" +
                   (network.secured ? ", locked)</option>" : ", open)</option>");
    }
    const bool manualSelected = selectedSsid.isEmpty() || !containsNetwork(networks, selectedSsid);
    options += String("<option value='' ") + (manualSelected ? "selected" : "") +
               ">Hidden network / manual entry</option>";
    return options;
}

String setupPage(const String& error)
{
    const String model = currentSettings.model.isEmpty() ? String(kDefaultModel) : currentSettings.model;
    const String apiBaseUrl = currentSettings.apiBaseUrl.isEmpty()
        ? String(kDefaultApiBaseUrl) : currentSettings.apiBaseUrl;
    const String sttBaseUrl = currentSettings.sttBaseUrl.isEmpty()
        ? String(kDefaultSttBaseUrl) : currentSettings.sttBaseUrl;
    const String sttModel = currentSettings.sttModel.isEmpty()
        ? String(kDefaultSttModel) : currentSettings.sttModel;
    const bool obsoleteTavilyDefault = currentSettings.webSearchApiKey.isEmpty() &&
        currentSettings.webSearchBaseUrl == "https://api.tavily.com";
    const String webSearchBaseUrl = currentSettings.webSearchBaseUrl.isEmpty() || obsoleteTavilyDefault
        ? String(kDefaultWebSearchBaseUrl) : currentSettings.webSearchBaseUrl;
    const String ttsBaseUrl = currentSettings.ttsBaseUrl.isEmpty()
        ? String(kDefaultTtsBaseUrl) : currentSettings.ttsBaseUrl;
    const String ttsModel = currentSettings.ttsModel.isEmpty()
        ? String(kDefaultTtsModel) : currentSettings.ttsModel;
    const String ttsVoice = currentSettings.ttsVoice.isEmpty()
        ? String(kDefaultTtsVoice) : currentSettings.ttsVoice;
    String page =
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Cardputer Assistant Setup</title><style>"
        "body{font:16px system-ui;background:#101522;color:#eef;max-width:560px;margin:32px auto;padding:16px}"
        "form{background:#1b2436;padding:20px;border-radius:14px}label{display:block;margin:14px 0 5px}"
        "input,select{box-sizing:border-box;width:100%;padding:12px;border:1px solid #52617d;border-radius:8px;background:#0c1220;color:#fff}"
        "button{margin-top:20px;padding:12px 18px;border:0;border-radius:8px;background:#55d6be;color:#08131a;font-weight:700}"
        ".error{background:#642a35;padding:10px;border-radius:8px}.note{color:#aebbd1;font-size:14px}</style></head><body>"
        "<h1>Cardputer Assistant</h1><p class='note'>On iPhone, use Safari at 192.168.4.1 so password managers remain available.</p>";
    if (!error.isEmpty()) {
        page += "<p class='error'>" + htmlEscape(error) + "</p>";
    }
    if (!nearbyNetworkScan.success) {
        page += "<p class='error'>" + htmlEscape(nearbyNetworkScan.error) + "</p>";
    }
    const bool manualSsid = currentSettings.wifiSsid.isEmpty() ||
        !containsNetwork(nearbyNetworkScan.networks, currentSettings.wifiSsid);
    page += "<form method='post' action='/save'>"
            "<label for='ssid'>Wi-Fi network (2.4 GHz)</label><select id='ssid' name='ssid'>" +
            networkOptions(nearbyNetworkScan.networks, currentSettings.wifiSsid) +
            "</select><input id='ssid_manual' name='ssid_manual' maxlength='32' placeholder='Hidden SSID' value='" +
            (manualSsid ? htmlEscape(currentSettings.wifiSsid) : String("")) +
            "' style='margin-top:8px'><label for='wifi_password'>Wi-Fi password</label>"
            "<input id='wifi_password' name='wifi_password' type='password' maxlength='63' autocomplete='new-password'>"
            "<p class='note'>Leave blank to keep the saved password. Open networks may use an empty password.</p>"
            "<label for='api_key'>API key</label>"
            "<input id='api_key' name='api_key' type='password' minlength='8' autocomplete='off' placeholder='Never displayed or logged'>"
            "<p class='note'>Leave blank to keep the saved key. The key is stored only in device NVS.</p>"
            "<label for='api_base_url'>API base URL</label>"
            "<input id='api_base_url' name='api_base_url' type='url' required maxlength='180' value='" +
            htmlEscape(apiBaseUrl) + "'>"
            "<label for='model'>Model id</label><input id='model' name='model' required maxlength='80' value='" +
            htmlEscape(model) + "'>"
            "<h2>Voice input (optional)</h2>"
            "<p class='note'>Use a separate Groq API key. It is stored only in device NVS and is never displayed or logged.</p>"
            "<label for='stt_api_key'>STT API key</label>"
            "<input id='stt_api_key' name='stt_api_key' type='password' minlength='8' autocomplete='off' placeholder='Leave blank to keep saved STT key'>"
            "<label for='stt_base_url'>STT base URL</label>"
            "<input id='stt_base_url' name='stt_base_url' type='url' maxlength='180' value='" +
            htmlEscape(sttBaseUrl) + "'>"
            "<label for='stt_model'>STT model</label>"
            "<input id='stt_model' name='stt_model' maxlength='80' value='" +
            htmlEscape(sttModel) + "'>"
            "<h2>Web search (optional)</h2>"
            "<p class='note'>Exa is the default provider and offers a starter tier without a payment method. Its key is stored only in device NVS and is never displayed or logged.</p>"
            "<label for='search_api_key'>Web search API key</label>"
            "<input id='search_api_key' name='search_api_key' type='password' minlength='8' autocomplete='off' placeholder='Leave blank to keep saved search key'>"
            "<label for='search_base_url'>Web search base URL</label>"
            "<input id='search_base_url' name='search_base_url' type='url' maxlength='180' value='" +
            htmlEscape(webSearchBaseUrl) + "'>"
            "<h2>Speech output (optional)</h2>"
            "<p class='note'>ElevenLabs multilingual TTS. The separate key is stored only in device NVS.</p>"
            "<label for='tts_api_key'>TTS API key</label>"
            "<input id='tts_api_key' name='tts_api_key' type='password' minlength='8' autocomplete='off' placeholder='Leave blank to keep saved TTS key'>"
            "<label for='tts_base_url'>TTS base URL</label>"
            "<input id='tts_base_url' name='tts_base_url' type='url' maxlength='180' value='" +
            htmlEscape(ttsBaseUrl) + "'>"
            "<label for='tts_model'>TTS model</label>"
            "<input id='tts_model' name='tts_model' maxlength='80' value='" + htmlEscape(ttsModel) + "'>"
            "<label for='tts_voice'>TTS voice id</label>"
            "<input id='tts_voice' name='tts_voice' maxlength='80' value='" + htmlEscape(ttsVoice) + "'>"
            "<label><input name='tts_auto' type='checkbox' style='width:auto' " +
            (currentSettings.ttsAutoPlay ? String("checked") : String("")) +
            "> Automatically speak new assistant replies</label>"
            "<button type='submit'>Save and restart</button></form>"
            "<script>const s=document.getElementById('ssid'),m=document.getElementById('ssid_manual');"
            "function u(){m.style.display=s.value?'none':'block';m.required=!s.value}"
            "s.addEventListener('change',u);u()</script></body></html>";
    return page;
}

void sendSetupPage()
{
    webServer.send(200, "text/html; charset=utf-8", setupPage(""));
}

void saveSubmittedSettings()
{
    Settings submitted = currentSettings;
    submitted.wifiSsid = webServer.arg("ssid");
    if (submitted.wifiSsid.isEmpty()) {
        submitted.wifiSsid = webServer.arg("ssid_manual");
    }
    submitted.apiBaseUrl = webServer.arg("api_base_url");
    submitted.apiBaseUrl.trim();
    while (submitted.apiBaseUrl.endsWith("/")) {
        submitted.apiBaseUrl.remove(submitted.apiBaseUrl.length() - 1);
    }
    submitted.model = webServer.arg("model");
    submitted.sttBaseUrl = webServer.arg("stt_base_url");
    submitted.sttBaseUrl.trim();
    while (submitted.sttBaseUrl.endsWith("/")) {
        submitted.sttBaseUrl.remove(submitted.sttBaseUrl.length() - 1);
    }
    submitted.sttModel = webServer.arg("stt_model");
    submitted.webSearchBaseUrl = webServer.arg("search_base_url");
    submitted.webSearchBaseUrl.trim();
    while (submitted.webSearchBaseUrl.endsWith("/")) {
        submitted.webSearchBaseUrl.remove(submitted.webSearchBaseUrl.length() - 1);
    }
    submitted.ttsBaseUrl = webServer.arg("tts_base_url");
    submitted.ttsBaseUrl.trim();
    while (submitted.ttsBaseUrl.endsWith("/")) {
        submitted.ttsBaseUrl.remove(submitted.ttsBaseUrl.length() - 1);
    }
    submitted.ttsModel = webServer.arg("tts_model");
    submitted.ttsVoice = webServer.arg("tts_voice");
    submitted.ttsAutoPlay = webServer.hasArg("tts_auto");
    const String wifiPassword = webServer.arg("wifi_password");
    const String apiKey = webServer.arg("api_key");
    const String sttApiKey = webServer.arg("stt_api_key");
    const String webSearchApiKey = webServer.arg("search_api_key");
    const String ttsApiKey = webServer.arg("tts_api_key");
    if (!wifiPassword.isEmpty() || currentSettings.wifiSsid.isEmpty()) {
        submitted.wifiPassword = wifiPassword;
    }
    if (!apiKey.isEmpty()) {
        submitted.apiKey = apiKey;
    }
    if (!sttApiKey.isEmpty()) {
        submitted.sttApiKey = sttApiKey;
    }
    if (!webSearchApiKey.isEmpty()) {
        submitted.webSearchApiKey = webSearchApiKey;
    }
    if (!ttsApiKey.isEmpty()) {
        submitted.ttsApiKey = ttsApiKey;
    }
    const OperationResult result = saveSettings(submitted);
    if (!result.success) {
        webServer.send(400, "text/html; charset=utf-8", setupPage(result.error));
        return;
    }
    currentSettings = submitted;
    webServer.send(200, "text/html; charset=utf-8",
                   "<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width'>"
                   "<body style='font:18px system-ui;background:#101522;color:#eef;padding:30px'>"
                   "<h1>Saved and verified</h1><p>The Cardputer will restart in five seconds. "
                   "You may close this page after it disconnects.</p></body>");
    Serial.printf("PROVISIONING settings_saved=yes nvs_verified=yes restart_delay_ms=%u\n",
                  static_cast<unsigned int>(kRestartDelayMs));
    restartPending = true;
    restartAt = millis() + kRestartDelayMs;
}

String macSuffix()
{
    const std::uint64_t mac = ESP.getEfuseMac();
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%04X", static_cast<unsigned int>(mac & 0xFFFF));
    return String(suffix);
}

String randomPassword()
{
    char password[13];
    snprintf(password, sizeof(password), "CP-%08lX", static_cast<unsigned long>(esp_random()));
    return String(password);
}

void updateProvisioningSerial()
{
    while (Serial.available() > 0) {
        const char character = static_cast<char>(Serial.read());
        if (character == '\n') {
            serialCommand.trim();
            if (serialCommand == "STATUS") {
                Serial.printf("STATUS board_adv=%s configured=%s search_configured=%s tts_configured=%s wifi=ap portal=ready heap=%u\n",
                              M5.getBoard() == m5::board_t::board_M5CardputerADV ? "yes" : "no",
                              settingsAreComplete(currentSettings) ? "yes" : "no",
                              webSearchSettingsAreComplete(currentSettings) ? "yes" : "no",
                              ttsSettingsAreComplete(currentSettings) ? "yes" : "no",
                              static_cast<unsigned int>(ESP.getFreeHeap()));
            } else if (serialCommand == "APITEST") {
                Serial.println("APITEST result=blocked reason=setup_portal_active");
            } else if (!serialCommand.isEmpty()) {
                Serial.println("ERROR event=serial_command reason=unsupported_command");
            }
            serialCommand = "";
        } else if (character != '\r' && serialCommand.length() < 32) {
            serialCommand += character;
        }
    }
}

}  // namespace

[[noreturn]] void runProvisioningPortal(const Settings& existingSettings)
{
    currentSettings = existingSettings;
    const String accessPointName = "Cardputer-" + macSuffix();
    String accessPointPassword;
    const OperationResult passwordLoadResult = loadSetupAccessPointPassword(accessPointPassword);
    if (!passwordLoadResult.success) {
        showFatalError(passwordLoadResult.error);
        Serial.println("FATAL event=setup_password_load result=failed");
        while (true) {
            delay(1000);
        }
    }
    if (accessPointPassword.isEmpty()) {
        accessPointPassword = randomPassword();
        const OperationResult passwordSaveResult = saveSetupAccessPointPassword(accessPointPassword);
        if (!passwordSaveResult.success) {
            showFatalError(passwordSaveResult.error);
            Serial.println("FATAL event=setup_password_save result=failed");
            while (true) {
                delay(1000);
            }
        }
    }
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(accessPointName.c_str(), accessPointPassword.c_str())) {
        showFatalError("Failed to start protected setup access point");
        while (true) {
            delay(1000);
        }
    }
    nearbyNetworkScan = scanWifiNetworks();
    if (nearbyNetworkScan.success) {
        Serial.printf("INFO event=wifi_scan result=ok count=%u\n",
                      static_cast<unsigned int>(nearbyNetworkScan.networks.size()));
    } else {
        Serial.println("WARN event=wifi_scan result=failed");
    }
    webServer.on("/", HTTP_GET, sendSetupPage);
    webServer.on("/save", HTTP_POST, saveSubmittedSettings);
    webServer.onNotFound(sendSetupPage);
    webServer.begin();
    Serial.println("PROVISIONING result=started security=wpa2 ip=192.168.4.1");
    showProvisioning(accessPointName, accessPointPassword);
    while (true) {
        webServer.handleClient();
        updateProvisioningSerial();
        if (restartPending && static_cast<std::int32_t>(millis() - restartAt) >= 0) {
            ESP.restart();
        }
        delay(2);
    }
}

}  // namespace cardputer
