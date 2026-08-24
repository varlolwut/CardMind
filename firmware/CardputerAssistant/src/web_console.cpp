#include "web_console.h"
#include "web_console_asset.h"

#include "api_client.h"
#include "chat_storage.h"
#include "crash_journal.h"
#include "file_workspace.h"
#include "python_mode.h"
#include "storage.h"
#include "ssh_client.h"
#include "ssh_tool.h"
#include "text_utils.h"
#include "tool_router.h"
#include "ui.h"
#include "web_search_client.h"

#include <ArduinoJson.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace cardputer {
namespace {

constexpr std::uint32_t kSessionIdleMs = 15U * 60U * 1000U;
constexpr std::uint32_t kLoginLockMs = 30U * 1000U;
constexpr std::size_t kMaximumLoginFailures = 5;
constexpr std::size_t kMaximumPromptBytes = 1200;
constexpr std::size_t kMaximumStateHistoryBytes = 12000;
constexpr std::size_t kMaximumWebFileChunkBytes = 12288;

WebServer server(80);
Settings consoleSettings;
ChatDocument activeChat;
std::vector<ChatSummary> consoleChats;
String accessPassword;
String sessionToken;
String csrfToken;
String consoleStatus;
String consoleSerialInput;
std::string activeResponse;
std::uint32_t sessionLastActivityAt = 0;
std::uint32_t loginLockedUntil = 0;
std::size_t loginFailures = 0;
bool exitRequested = false;
bool routesConfigured = false;
bool serverStarted = false;
File uploadFile;
String uploadName;
String uploadStorageName;
String uploadError;
std::size_t uploadBytes = 0;
bool uploadCreated = false;
bool uploadReplacing = false;
constexpr const char* kWebSaveTemporaryName = "_cardmind-web-save.txt";
File sshKeyUploadFile;
String sshKeyUploadError;
std::size_t sshKeyUploadBytes = 0;
constexpr const char* kSshKeyUploadPath = "/assistant/ssh/upload.tmp";
SshClient webSshClient;
SshProfile webSshProfile = {"", "", 22, "", "", SshAuthMode::Password, ""};
bool webSshAwaitingTrust = false;
bool webSshTerminalOpen = false;
bool consoleEscapeConsumed = false;
String consoleQrPayload;
String firmwareVersion;
bool pythonRestartRequested = false;

bool consoleEscapePressed()
{
    const Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
    return keys.esc || std::find(keys.word.begin(), keys.word.end(), '`') != keys.word.end();
}

bool parseUnsignedArgument(const String& value, std::uint32_t& result)
{
    if (value.isEmpty()) {
        return false;
    }
    std::uint64_t parsed = 0;
    for (std::size_t index = 0; index < value.length(); ++index) {
        const char character = value[index];
        if (character < '0' || character > '9') {
            return false;
        }
        parsed = parsed * 10U + static_cast<std::uint64_t>(character - '0');
        if (parsed > UINT32_MAX) {
            return false;
        }
    }
    result = static_cast<std::uint32_t>(parsed);
    return true;
}

String normalizedBaseUrl(String value)
{
    value.trim();
    while (value.endsWith("/")) {
        value.remove(value.length() - 1);
    }
    return value;
}

void clearConsoleSecrets()
{
    webSshClient.close();
    webSshProfile.password = "";
    webSshProfile.privateKeyPassphrase = "";
    webSshAwaitingTrust = false;
    webSshTerminalOpen = false;
    if (sshKeyUploadFile) {
        sshKeyUploadFile.close();
    }
    SD.remove(kSshKeyUploadPath);
    sshKeyUploadError = "";
    sshKeyUploadBytes = 0;
    accessPassword = "";
    sessionToken = "";
    csrfToken = "";
    consoleSettings.apiKey = "";
    consoleSettings.wifiPassword = "";
    consoleSettings.sttApiKey = "";
    consoleSettings.webSearchApiKey = "";
    consoleSettings.ttsApiKey = "";
    consoleQrPayload = "";
    firmwareVersion = "";
}

void failUpload(const String& error)
{
    if (uploadFile) {
        uploadFile.close();
    }
    if (uploadCreated && !uploadStorageName.isEmpty()) {
        SD.remove(workspaceFilePath(uploadStorageName));
    }
    uploadCreated = false;
    uploadError = error;
}

String htmlEscape(const String& value)
{
    String result;
    result.reserve(value.length() + 16);
    for (std::size_t index = 0; index < value.length(); ++index) {
        switch (value[index]) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += value[index]; break;
        }
    }
    return result;
}

String randomHexToken()
{
    char token[33] = {};
    for (std::size_t index = 0; index < 4; ++index) {
        std::snprintf(token + index * 8, 9, "%08lX",
                      static_cast<unsigned long>(esp_random()));
    }
    return String(token);
}

bool constantTimeEquals(const String& left, const String& right)
{
    const std::size_t maximum = std::max(left.length(), right.length());
    std::uint8_t difference = static_cast<std::uint8_t>(left.length() ^ right.length());
    for (std::size_t index = 0; index < maximum; ++index) {
        const std::uint8_t leftByte = index < left.length()
            ? static_cast<std::uint8_t>(left[index])
            : 0;
        const std::uint8_t rightByte = index < right.length()
            ? static_cast<std::uint8_t>(right[index])
            : 0;
        difference |= leftByte ^ rightByte;
    }
    return difference == 0;
}

bool sessionIsActive()
{
    if (sessionToken.isEmpty() ||
        static_cast<std::uint32_t>(millis() - sessionLastActivityAt) > kSessionIdleMs) {
        sessionToken = "";
        csrfToken = "";
        return false;
    }
    const String cookie = server.header("Cookie");
    const bool authenticated = cookie.indexOf("cm_session=" + sessionToken) >= 0;
    if (authenticated) {
        sessionLastActivityAt = millis();
    }
    return authenticated;
}

bool requestHasValidCsrf()
{
    return sessionIsActive() && !csrfToken.isEmpty() &&
           constantTimeEquals(server.header("X-CardMind-CSRF"), csrfToken);
}

void sendJson(int statusCode, const JsonDocument& document)
{
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(measureJson(document));
    server.send(statusCode, "application/json; charset=utf-8", "");
    serializeJson(document, server.client());
}

void sendJsonError(int statusCode, const String& error)
{
    JsonDocument document;
    document["ok"] = false;
    document["error"] = error;
    sendJson(statusCode, document);
}

std::uint64_t currentTimestamp()
{
    const std::time_t current = std::time(nullptr);
    return current >= 1700000000 ? static_cast<std::uint64_t>(current) : 0;
}

OperationResult refreshChats()
{
    const ChatsResult result = listChats();
    if (!result.success) {
        return {false, result.error};
    }
    consoleChats = result.chats;
    return {true, ""};
}

OperationResult loadActiveChat(const String& id)
{
    const ChatDocumentResult loaded = loadChat(id);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    activeChat = loaded.chat;
    activeResponse.clear();
    return {true, ""};
}

void renderConsoleChat()
{
    const String state = consoleStatus.isEmpty()
        ? String("Web console: ") + WiFi.localIP().toString() + "  ESC exits"
        : consoleStatus;
    showChat(activeChat.messages, activeResponse, "", KeyboardLayout::English,
             activeChat.summary.title, state, 0, WiFi.status() == WL_CONNECTED,
             M5Cardputer.Power.getBatteryLevel(),
             M5Cardputer.Power.isCharging() == m5::Power_Class::is_charging);
}

String loginPage(const String& error)
{
    String page =
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>CardMind Login</title><style>"
        ":root{color-scheme:dark}*{box-sizing:border-box}body{margin:0;font:15px system-ui;background:radial-gradient(circle at 50% 0,#143550,#050b12 38rem);color:#f3f7fb;display:grid;place-items:center;min-height:100vh;padding:18px}"
        ".card{width:min(420px,100%);background:linear-gradient(145deg,#101e2d,#0a1521);border:1px solid #294158;padding:26px;border-radius:20px;box-shadow:0 24px 80px #0009}.brand{display:flex;align-items:center;gap:12px;margin-bottom:26px}.mark{width:44px;height:44px;display:grid;place-items:center;border:1px solid #65f2cc66;border-radius:14px;background:#15343d;color:#65f2cc;font-weight:900}.brand b{letter-spacing:.12em}.badge{margin-left:auto;padding:6px 9px;border:1px solid #294158;border-radius:999px;color:#91a9be;font-size:11px}"
        "h1{font-size:24px;margin:0 0 7px}p{color:#9eb1c4;line-height:1.5;margin:0 0 18px}label{display:block;color:#c8d5e2;font-weight:700;font-size:12px}input,button{box-sizing:border-box;width:100%;padding:12px;margin-top:8px;border-radius:10px;font:inherit}input{background:#07111c;color:#fff;border:1px solid #39536e;outline:none}input:focus{border-color:#65f2cc;box-shadow:0 0 0 3px #65f2cc18}button{border:0;background:#65f2cc;color:#052019;font-weight:850;cursor:pointer;margin-top:14px}.hint{margin:17px 0 0;padding-top:15px;border-top:1px solid #203247;color:#8197aa;font-size:12px}.error{padding:10px;border:1px solid #713544;border-radius:10px;background:#351722;color:#ffc6ce}</style></head><body><form class='card' method='post' action='/login'>"
        "<div class='brand'><div class='mark'>CM</div><div><b>CARDMIND</b><br><small>Device console</small></div><span class='badge'>LOCAL</span></div><h1>Connect to CardMind</h1><p>Use the installation password displayed on your Cardputer.</p>";
    if (!error.isEmpty()) {
        page += "<p class='error'>" + htmlEscape(error) + "</p>";
    }
    page += "<label for='password'>Installation password</label><input id='password' name='password' type='password' required autocomplete='current-password' autofocus>"
            "<button type='submit'>Open device console</button><p class='hint'>Keep this page on the same trusted Wi-Fi network as the Cardputer.</p></form></body></html>";
    return page;
}

void sendConsolePage()
{
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Content-Encoding", "gzip");
    server.setContentLength(kWebConsolePageGzipSize);
    server.send(200, "text/html; charset=utf-8", "");
    server.sendContent_P(reinterpret_cast<PGM_P>(kWebConsolePageGzip),
                         kWebConsolePageGzipSize);
}

void sendLoginPage()
{
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html; charset=utf-8", loginPage(""));
}

void sendRoot()
{
    if (!sessionIsActive()) {
        sendLoginPage();
        return;
    }
    server.sendHeader("Cache-Control", "no-store");
    sendConsolePage();
}

void handleSession()
{
    if (!sessionIsActive()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["csrf"] = csrfToken;
    sendJson(200, document);
}

void handleLogin()
{
    if (static_cast<std::int32_t>(millis() - loginLockedUntil) < 0) {
        server.send(429, "text/html; charset=utf-8",
                    loginPage("Too many attempts; wait 30 seconds"));
        return;
    }
    if (!constantTimeEquals(server.arg("password"), accessPassword)) {
        ++loginFailures;
        if (loginFailures >= kMaximumLoginFailures) {
            loginFailures = 0;
            loginLockedUntil = millis() + kLoginLockMs;
        }
        delay(250);
        server.send(401, "text/html; charset=utf-8", loginPage("Invalid password"));
        return;
    }
    loginFailures = 0;
    sessionToken = randomHexToken();
    csrfToken = randomHexToken();
    sessionLastActivityAt = millis();
    server.sendHeader("Set-Cookie", "cm_session=" + sessionToken +
                      "; HttpOnly; SameSite=Strict; Path=/; Max-Age=900");
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Authenticated");
    renderConsoleChat();
}

void handleLogout()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    sessionToken = "";
    csrfToken = "";
    server.sendHeader("Set-Cookie", "cm_session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0");
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleCloseConsole()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["message"] = "Web Console is closing";
    sendJson(200, document);
    exitRequested = true;
}

void handleState()
{
    if (!sessionIsActive()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["ip"] = WiFi.localIP().toString();
    document["battery"] = M5Cardputer.Power.getBatteryLevel();
    document["free_heap"] = ESP.getFreeHeap();
    document["largest_heap"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    document["wifi_rssi"] = WiFi.RSSI();
    document["sd_total_bytes"] = SD.totalBytes();
    document["sd_used_bytes"] = SD.usedBytes();
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
    document["status"] = consoleStatus;
    document["firmware_version"] = firmwareVersion;
    document["uptime_ms"] = millis();
    document["minimum_heap"] = ESP.getMinFreeHeap();
    document["stack_free"] = uxTaskGetStackHighWaterMark(nullptr);
    document["cpu_mhz"] = getCpuFrequencyMhz();
    document["reset_reason"] = static_cast<int>(esp_reset_reason());
    document["wifi_ssid"] = consoleSettings.wifiSsid;
    document["model"] = consoleSettings.model;
    document["api_base_url"] = consoleSettings.apiBaseUrl;
    document["api_key_configured"] = consoleSettings.apiKey.length() >= 8;
    document["stt_base_url"] = consoleSettings.sttBaseUrl;
    document["stt_model"] = consoleSettings.sttModel;
    document["stt_key_configured"] = consoleSettings.sttApiKey.length() >= 8;
    document["search_base_url"] = consoleSettings.webSearchBaseUrl;
    document["search_key_configured"] = consoleSettings.webSearchApiKey.length() >= 8;
    document["tts_base_url"] = consoleSettings.ttsBaseUrl;
    document["tts_model"] = consoleSettings.ttsModel;
    document["tts_voice"] = consoleSettings.ttsVoice;
    document["tts_key_configured"] = consoleSettings.ttsApiKey.length() >= 8;
    document["tts_auto_play"] = consoleSettings.ttsAutoPlay;
    document["tts_volume"] = consoleSettings.ttsVolume;
    document["display_brightness"] = consoleSettings.displayBrightness;
    document["screen_sleep_minutes"] = consoleSettings.screenSleepMinutes;
    document["keyboard_repeat_ms"] = consoleSettings.keyboardRepeatMs;
    document["power_profile"] = consoleSettings.powerProfile;
    const PythonModeStatus python = inspectPythonMode();
    document["python_layout_ready"] = python.partitionLayoutReady;
    document["python_image_ready"] = python.pythonImageReady;
    document["python_error"] = python.error;
    document["python_runtime_error"] = python.lastRuntimeError;
    std::vector<SshProfile> sshProfiles;
    std::size_t sshSelected = 0;
    const OperationResult sshProfileResult = loadSshProfiles(sshProfiles, sshSelected);
    if (!sshProfileResult.success) {
        sendJsonError(500, sshProfileResult.error);
        return;
    }
    const SshProfile sshProfile = sshProfiles.empty()
        ? SshProfile{"", "", 22, "", "", SshAuthMode::Password, ""}
        : sshProfiles[sshSelected];
    document["ssh_name"] = sshProfile.name;
    document["ssh_host"] = sshProfile.host;
    document["ssh_port"] = sshProfile.port;
    document["ssh_username"] = sshProfile.username;
    document["ssh_auth_mode"] = sshProfile.authMode == SshAuthMode::PrivateKey ? "key" : "password";
    document["ssh_selected"] = sshSelected;
    document["ssh_terminal_open"] = webSshTerminalOpen && webSshClient.isOpen();
    JsonArray sshProfileItems = document["ssh_profiles"].to<JsonArray>();
    for (const auto& item : sshProfiles) {
        JsonObject profileItem = sshProfileItems.add<JsonObject>();
        profileItem["name"] = item.name;
        profileItem["host"] = item.host;
        profileItem["port"] = item.port;
        profileItem["username"] = item.username;
        profileItem["auth_mode"] = item.authMode == SshAuthMode::PrivateKey ? "key" : "password";
    }
    document["ssh_key_installed"] = sshPrivateKeyIsInstalled();
    document["ssh_configured"] = sshProfileIsComplete(sshProfile);
    JsonArray chats = document["chats"].to<JsonArray>();
    for (const auto& chat : consoleChats) {
        JsonObject item = chats.add<JsonObject>();
        item["id"] = chat.id;
        item["title"] = chat.title;
        item["pinned"] = chat.pinned;
        item["archived"] = chat.archived;
        item["total_messages"] = chat.messageCount + chat.archivedMessageCount;
    }
    std::size_t firstMessage = activeChat.messages.size();
    std::size_t includedBytes = 0;
    while (firstMessage > 0) {
        const std::size_t messageBytes = activeChat.messages[firstMessage - 1].content.size();
        if (includedBytes + messageBytes > kMaximumStateHistoryBytes) {
            break;
        }
        includedBytes += messageBytes;
        --firstMessage;
    }
    JsonArray messages = document["messages"].to<JsonArray>();
    for (std::size_t index = firstMessage; index < activeChat.messages.size(); ++index) {
        JsonObject item = messages.add<JsonObject>();
        item["role"] = activeChat.messages[index].role;
        item["content"] = activeChat.messages[index].content;
    }
    const WorkspaceFilesResult workspace = listWorkspaceFiles();
    if (!workspace.success) {
        sendJsonError(500, workspace.error);
        return;
    }
    JsonArray files = document["files"].to<JsonArray>();
    for (const auto& file : workspace.files) {
        JsonObject item = files.add<JsonObject>();
        item["name"] = file.name;
        item["size"] = file.size;
    }
    sendJson(200, document);
}

void sendSse(const char* type, const std::string& delta, const String& error)
{
    JsonDocument document;
    document["type"] = type;
    if (!delta.empty()) {
        document["delta"] = delta;
    }
    if (!error.isEmpty()) {
        document["error"] = error;
    }
    String output;
    serializeJson(document, output);
    server.sendContent("data:" + output + "\n\n");
}

void handlePrompt()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const String promptValue = server.arg("prompt");
    const std::string prompt = promptValue.c_str();
    if (prompt.empty() || prompt.size() > kMaximumPromptBytes || !isValidUtf8(prompt)) {
        sendJsonError(400, "Prompt must be valid UTF-8 between 1 and 1200 bytes");
        return;
    }
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/event-stream; charset=utf-8", "");
    std::vector<Message> pending = activeChat.messages;
    pending.push_back({"user", prompt});
    HistoryFitResult pendingFit = fitHistoryToActiveContext(pending);
    if (!pendingFit.archived.empty()) {
        const OperationResult archived = archiveChatMessages(
            activeChat.summary.id, pendingFit.archived);
        if (!archived.success) {
            sendSse("error", "", archived.error);
            return;
        }
        activeChat.summary.archivedMessageCount += pendingFit.archived.size();
    }
    activeChat.messages = std::move(pendingFit.retained);
    activeChat.draft.clear();
    if (activeChat.summary.messageCount == 0) {
        activeChat.summary.title = makeChatTitle(prompt, kMaximumChatTitleCells).c_str();
    }
    activeChat.summary.messageCount = activeChat.messages.size();
    activeChat.summary.updatedAt = currentTimestamp();
    OperationResult saved = saveChat(activeChat);
    if (!saved.success) {
        sendSse("error", "", saved.error);
        return;
    }
    activeResponse.clear();
    consoleStatus = "Streaming from web console...";
    renderConsoleChat();
    std::uint32_t lastRenderAt = 0;
    const ChatTextCallback onText = [&lastRenderAt](const std::string& text) {
        activeResponse += text;
        sendSse("delta", text, "");
        const std::uint32_t now = millis();
        if (lastRenderAt == 0 || now - lastRenderAt >= 120) {
            renderConsoleChat();
            lastRenderAt = now;
        }
    };
    const ChatToolPolicy toolPolicy = resolveChatToolPolicy(
        consoleSettings, prompt, activeChat.sshToolsEnabled, sshToolIsAvailable());
    const CancelCallback isCancelled = []() {
        M5Cardputer.update();
        if (consoleEscapePressed()) {
            consoleEscapeConsumed = true;
            return true;
        }
        return !server.client().connected();
    };
    markOperation(chatToolPolicyIsEnabled(toolPolicy)
        ? "web_console_tools" : "web_console_chat");
    const ChatResult result = chatToolPolicyIsEnabled(toolPolicy)
        ? streamChatCompletionWithTools(consoleSettings, activeChat.messages,
                                        activeChat.instructions, toolPolicy.sshEnabled,
                                        onText,
                                        [&toolPolicy, &isCancelled](const ToolCall& call) {
                                            return routeToolCall(
                                                consoleSettings, toolPolicy, call,
                                                isCancelled);
                                        },
                                        isCancelled)
        : streamChatCompletion(consoleSettings, activeChat.messages,
                               activeChat.instructions, onText, isCancelled);
    markOperation("idle");
    if (!result.success) {
        consoleStatus = result.error;
        activeResponse = result.response;
        sendSse("error", "", result.error);
        renderConsoleChat();
        return;
    }
    activeChat.messages.push_back({"assistant", result.response});
    HistoryFitResult finalFit = fitHistoryToActiveContext(activeChat.messages);
    if (!finalFit.archived.empty()) {
        const OperationResult archived = archiveChatMessages(
            activeChat.summary.id, finalFit.archived);
        if (!archived.success) {
            activeResponse.clear();
            consoleStatus = archived.error;
            sendSse("error", "", archived.error);
            renderConsoleChat();
            return;
        }
        activeChat.summary.archivedMessageCount += finalFit.archived.size();
    }
    activeChat.messages = std::move(finalFit.retained);
    activeChat.summary.messageCount = activeChat.messages.size();
    activeChat.summary.updatedAt = currentTimestamp();
    saved = saveChat(activeChat);
    activeResponse.clear();
    consoleStatus = saved.success ? String("Saved") : saved.error;
    if (saved.success) {
        refreshChats();
        sendSse("done", "", "");
    } else {
        sendSse("error", "", saved.error);
    }
    renderConsoleChat();
}

void handleSelectChat()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const String id = server.arg("id");
    const OperationResult result = loadActiveChat(id);
    if (!result.success) {
        sendJsonError(400, result.error);
        return;
    }
    consoleStatus = "Chat selected";
    renderConsoleChat();
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleNewChat()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const ChatDocumentResult created = createChat("New chat");
    if (!created.success) {
        sendJsonError(400, created.error);
        return;
    }
    activeChat = created.chat;
    OperationResult result = refreshChats();
    if (!result.success) {
        sendJsonError(500, result.error);
        return;
    }
    consoleStatus = "New chat created";
    renderConsoleChat();
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleInstructions()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const String value = server.arg("instructions");
    const std::string instructions = value.c_str();
    if (instructions.size() > kMaximumChatInstructionsBytes || !isValidUtf8(instructions)) {
        sendJsonError(400, "Instructions must be valid UTF-8 up to 2048 bytes");
        return;
    }
    activeChat.instructions = instructions;
    activeChat.summary.updatedAt = currentTimestamp();
    const OperationResult saved = saveChat(activeChat);
    if (!saved.success) {
        sendJsonError(500, saved.error);
        return;
    }
    consoleStatus = instructions.empty() ? String("Instructions disabled")
                                         : String("Instructions saved");
    renderConsoleChat();
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleChatPermissions()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const String requested = server.arg("ssh_tools_enabled");
    if (requested != "0" && requested != "1") {
        sendJsonError(400, "SSH tool permission must be either 0 or 1");
        return;
    }
    if (requested == "1") {
        SshProfile profile;
        const OperationResult loaded = loadSshProfile(profile);
        const bool complete = loaded.success && sshProfileIsComplete(profile);
        profile.password = "";
        profile.privateKeyPassphrase = "";
        if (!complete) {
            sendJsonError(409, loaded.success
                ? String("A complete selected SSH profile is required") : loaded.error);
            return;
        }
    }
    activeChat.sshToolsEnabled = requested == "1";
    activeChat.summary.updatedAt = currentTimestamp();
    const OperationResult saved = saveChat(activeChat);
    if (!saved.success) {
        sendJsonError(500, saved.error);
        return;
    }
    consoleStatus = activeChat.sshToolsEnabled
        ? String("Model SSH access enabled for this chat")
        : String("Model SSH access disabled for this chat");
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleRenameChat()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const std::string requestedTitle = server.arg("title").c_str();
    if (requestedTitle.empty() || requestedTitle.size() > 256 || !isValidUtf8(requestedTitle)) {
        sendJsonError(400, "Chat title must be valid UTF-8 between 1 and 256 bytes");
        return;
    }
    activeChat.summary.title = makeChatTitle(requestedTitle, kMaximumChatTitleCells).c_str();
    activeChat.summary.updatedAt = currentTimestamp();
    OperationResult result = saveChat(activeChat);
    if (result.success) {
        result = refreshChats();
    }
    if (!result.success) {
        sendJsonError(500, result.error);
        return;
    }
    consoleStatus = "Chat renamed";
    renderConsoleChat();
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handlePinChat()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    activeChat.summary.pinned = !activeChat.summary.pinned;
    if (activeChat.summary.pinned) {
        activeChat.summary.archived = false;
    }
    OperationResult result = saveChat(activeChat);
    if (result.success) {
        result = refreshChats();
    }
    if (!result.success) {
        sendJsonError(500, result.error);
        return;
    }
    consoleStatus = activeChat.summary.pinned ? String("Chat pinned") : String("Chat unpinned");
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleArchiveChat()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    activeChat.summary.archived = !activeChat.summary.archived;
    if (activeChat.summary.archived) {
        activeChat.summary.pinned = false;
    }
    OperationResult result = saveChat(activeChat);
    if (result.success) {
        result = refreshChats();
    }
    if (!result.success) {
        sendJsonError(500, result.error);
        return;
    }
    consoleStatus = activeChat.summary.archived ? String("Chat archived")
                                                : String("Chat restored");
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleDuplicateChat()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const ChatDocumentResult duplicated = duplicateChat(activeChat.summary.id);
    if (!duplicated.success) {
        sendJsonError(500, duplicated.error);
        return;
    }
    activeChat = duplicated.chat;
    const OperationResult refreshed = refreshChats();
    if (!refreshed.success) {
        sendJsonError(500, refreshed.error);
        return;
    }
    consoleStatus = "Chat duplicated";
    renderConsoleChat();
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleExportChat()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const String filename = "chat_" + activeChat.summary.id + ".md";
    const OperationResult exported = exportChatToWorkspace(
        activeChat.summary.id, filename);
    if (!exported.success) {
        sendJsonError(400, exported.error);
        return;
    }
    consoleStatus = "Chat exported as " + filename;
    JsonDocument document;
    document["ok"] = true;
    document["filename"] = filename;
    sendJson(200, document);
}

void handleExportChatBundle()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const String filename = "chat_" + activeChat.summary.id + ".chat.jsonl";
    const OperationResult exported = exportChatBundleToWorkspace(
        activeChat.summary.id, filename);
    if (!exported.success) {
        sendJsonError(400, exported.error);
        return;
    }
    consoleStatus = "Chat bundle exported as " + filename;
    JsonDocument document;
    document["ok"] = true;
    document["filename"] = filename;
    sendJson(200, document);
}

void handleImportChatBundle()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const ChatDocumentResult imported = importChatBundleFromWorkspace(server.arg("name"));
    if (!imported.success) {
        sendJsonError(400, imported.error);
        return;
    }
    activeChat = imported.chat;
    const OperationResult refreshed = refreshChats();
    if (!refreshed.success) {
        sendJsonError(500, refreshed.error);
        return;
    }
    consoleStatus = "Chat imported";
    renderConsoleChat();
    JsonDocument document;
    document["ok"] = true;
    document["chat_id"] = activeChat.summary.id;
    sendJson(200, document);
}

void handleDeleteChat()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    OperationResult result = deleteChat(activeChat.summary.id);
    if (result.success) {
        result = refreshChats();
    }
    if (!result.success) {
        sendJsonError(500, result.error);
        return;
    }
    if (consoleChats.empty()) {
        const ChatDocumentResult created = createChat("New chat");
        if (!created.success) {
            sendJsonError(500, created.error);
            return;
        }
        activeChat = created.chat;
        result = refreshChats();
    } else {
        result = loadActiveChat(consoleChats.front().id);
    }
    if (!result.success) {
        sendJsonError(500, result.error);
        return;
    }
    consoleStatus = "Chat deleted";
    renderConsoleChat();
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleSettings()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    Settings updated = consoleSettings;
    const String wifiSsid = server.arg("wifi_ssid");
    const String wifiPassword = server.arg("wifi_password");
    if (wifiSsid.isEmpty() || wifiSsid.length() > 32 || wifiPassword.length() > 63) {
        sendJsonError(400, "Wi-Fi SSID must contain 1-32 bytes and password at most 63 bytes");
        return;
    }
    if (wifiSsid != updated.wifiSsid) {
        updated.wifiSsid = wifiSsid;
        updated.wifiPassword = wifiPassword;
    } else if (!wifiPassword.isEmpty()) {
        updated.wifiPassword = wifiPassword;
    }
    String apiKey = server.arg("api_key");
    apiKey.trim();
    if (!apiKey.isEmpty()) {
        updated.apiKey = apiKey;
    }
    updated.apiBaseUrl = normalizedBaseUrl(server.arg("api_base_url"));
    updated.model = server.arg("model");
    updated.model.trim();
    if (updated.model.length() > 120) {
        sendJsonError(400, "Model id must not exceed 120 characters");
        return;
    }
    String sttKey = server.arg("stt_api_key");
    sttKey.trim();
    if (server.arg("clear_stt_key") == "1") {
        updated.sttApiKey = "";
    } else if (!sttKey.isEmpty()) {
        updated.sttApiKey = sttKey;
    }
    updated.sttBaseUrl = normalizedBaseUrl(server.arg("stt_base_url"));
    updated.sttModel = server.arg("stt_model");
    String searchKey = server.arg("search_api_key");
    searchKey.trim();
    if (server.arg("clear_search_key") == "1") {
        updated.webSearchApiKey = "";
    } else if (!searchKey.isEmpty()) {
        updated.webSearchApiKey = searchKey;
    }
    updated.webSearchBaseUrl = normalizedBaseUrl(server.arg("search_base_url"));
    String ttsKey = server.arg("tts_api_key");
    ttsKey.trim();
    if (server.arg("clear_tts_key") == "1") {
        updated.ttsApiKey = "";
    } else if (!ttsKey.isEmpty()) {
        updated.ttsApiKey = ttsKey;
    }
    updated.ttsBaseUrl = normalizedBaseUrl(server.arg("tts_base_url"));
    updated.ttsModel = server.arg("tts_model");
    updated.ttsVoice = server.arg("tts_voice");
    updated.ttsAutoPlay = server.arg("tts_auto_play") == "1";
    std::uint32_t volume = 0;
    std::uint32_t brightness = 0;
    std::uint32_t sleepMinutes = 0;
    std::uint32_t repeatMs = 0;
    std::uint32_t powerProfile = 0;
    if (!parseUnsignedArgument(server.arg("tts_volume"), volume) || volume > 255 ||
        !parseUnsignedArgument(server.arg("display_brightness"), brightness) ||
        brightness < 32 || brightness > 255 ||
        !parseUnsignedArgument(server.arg("screen_sleep_minutes"), sleepMinutes) ||
        sleepMinutes > UINT16_MAX ||
        !parseUnsignedArgument(server.arg("keyboard_repeat_ms"), repeatMs) ||
        repeatMs > UINT16_MAX ||
        !parseUnsignedArgument(server.arg("power_profile"), powerProfile) ||
        powerProfile > 2) {
        sendJsonError(400, "Device preference values are outside their supported ranges");
        return;
    }
    updated.ttsVolume = static_cast<std::uint8_t>(volume);
    updated.displayBrightness = static_cast<std::uint8_t>(brightness);
    updated.screenSleepMinutes = static_cast<std::uint16_t>(sleepMinutes);
    updated.keyboardRepeatMs = static_cast<std::uint16_t>(repeatMs);
    updated.powerProfile = static_cast<std::uint8_t>(powerProfile);
    const OperationResult result = saveSettings(updated);
    if (!result.success) {
        sendJsonError(400, result.error);
        return;
    }
    consoleSettings = updated;
    consoleStatus = "Settings saved; Wi-Fi changes apply after closing the console";
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleClearChat()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    OperationResult result = clearChatHistory(activeChat.summary.id);
    if (result.success) {
        result = loadActiveChat(activeChat.summary.id);
    }
    if (result.success) {
        result = refreshChats();
    }
    if (!result.success) {
        sendJsonError(500, result.error);
        return;
    }
    consoleStatus = "Chat messages cleared";
    renderConsoleChat();
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleArchivedMessages()
{
    if (!sessionIsActive()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    std::uint32_t offset = 0;
    if (!parseUnsignedArgument(server.arg("offset"), offset)) {
        sendJsonError(400, "Archive offset must be an unsigned integer");
        return;
    }
    const ArchivedMessagesPageResult result = readArchivedChatMessages(
        activeChat.summary.id, offset, 8, 12000);
    if (!result.success) {
        sendJsonError(500, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["next_offset"] = result.nextOffset;
    document["eof"] = result.eof;
    JsonArray messages = document["messages"].to<JsonArray>();
    for (const auto& message : result.messages) {
        JsonObject item = messages.add<JsonObject>();
        item["role"] = message.role;
        item["content"] = message.content;
    }
    sendJson(200, document);
}

void handleModels()
{
    if (!sessionIsActive()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const ModelsResult result = fetchModels(consoleSettings);
    if (!result.success) {
        sendJsonError(502, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    JsonArray models = document["models"].to<JsonArray>();
    for (const auto& model : result.models) {
        models.add(model);
    }
    sendJson(200, document);
}

void handleDiagnosticsDownload()
{
    if (!sessionIsActive()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    String report;
    report.reserve(640);
    report += "CardMind diagnostics\n";
    report += "firmware=" + firmwareVersion + "\n";
    report += "uptime_ms=" + String(millis()) + "\n";
    report += "reset_reason=" + String(static_cast<int>(esp_reset_reason())) + "\n";
    report += "cpu_mhz=" + String(getCpuFrequencyMhz()) + "\n";
    report += "free_heap=" + String(ESP.getFreeHeap()) + "\n";
    report += "minimum_heap=" + String(ESP.getMinFreeHeap()) + "\n";
    report += "largest_heap=" + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) + "\n";
    report += "stack_free=" + String(uxTaskGetStackHighWaterMark(nullptr)) + "\n";
    report += "wifi_rssi=" + String(WiFi.RSSI()) + "\n";
    report += "sd_total_bytes=" + String(static_cast<unsigned long long>(SD.totalBytes())) + "\n";
    report += "sd_used_bytes=" + String(static_cast<unsigned long long>(SD.usedBytes())) + "\n";
    report += "chat_configured=" + String(settingsAreComplete(consoleSettings) ? "yes" : "no") + "\n";
    report += "stt_configured=" + String(voiceSettingsAreComplete(consoleSettings) ? "yes" : "no") + "\n";
    report += "search_configured=" + String(webSearchSettingsAreComplete(consoleSettings) ? "yes" : "no") + "\n";
    report += "tts_configured=" + String(ttsSettingsAreComplete(consoleSettings) ? "yes" : "no") + "\n";
    const PythonModeStatus python = inspectPythonMode();
    report += "python_ready=" + String(
        python.partitionLayoutReady && python.pythonImageReady ? "yes" : "no") + "\n";
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Content-Disposition", "attachment; filename=cardmind-diagnostics.txt");
    server.send(200, "text/plain; charset=utf-8", report);
}

void handlePythonStart()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const PythonModeStatus status = inspectPythonMode();
    if (!status.partitionLayoutReady || !status.pythonImageReady) {
        sendJsonError(409, status.error);
        return;
    }
    String password;
    String handoffToken = randomHexToken();
    OperationResult result = loadSetupAccessPointPassword(password);
    if (result.success && password.isEmpty()) {
        result = {false, "Installation password is missing; run device setup first"};
    }
    if (result.success) {
        result = synchronizePythonModeSettings(consoleSettings, password, handoffToken);
    }
    password = "";
    if (result.success) {
        result = activatePythonMode();
    }
    if (!result.success) {
        sendJsonError(409, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["restarting"] = true;
    document["address"] = "http://" + WiFi.localIP().toString() + "/";
    document["handoff_token"] = handoffToken;
    sendJson(200, document);
    handoffToken = "";
    pythonRestartRequested = true;
}

void handleSshSettings()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    std::uint32_t port = 0;
    if (!parseUnsignedArgument(server.arg("port"), port) || port == 0 || port > 65535) {
        sendJsonError(400, "SSH port must be between 1 and 65535");
        return;
    }
    const bool create = server.arg("create") == "1";
    SshProfile profile = {"", "", 22, "", "", SshAuthMode::Password, ""};
    const OperationResult loaded = create ? OperationResult{true, ""} : loadSshProfile(profile);
    if (!loaded.success) {
        sendJsonError(500, loaded.error);
        return;
    }
    profile.name = server.arg("name");
    profile.host = server.arg("host");
    profile.port = static_cast<std::uint16_t>(port);
    profile.username = server.arg("username");
    const String authMode = server.arg("auth_mode");
    if (authMode == "password") {
        profile.authMode = SshAuthMode::Password;
    } else if (authMode == "key") {
        profile.authMode = SshAuthMode::PrivateKey;
    } else {
        sendJsonError(400, "SSH auth mode must be 'password' or 'key'");
        return;
    }
    if (server.arg("replace_password") == "1") {
        profile.password = server.arg("password");
    }
    if (server.arg("replace_key_passphrase") == "1") {
        profile.privateKeyPassphrase = server.arg("key_passphrase");
    }
    OperationResult saved;
    if (create) {
        std::vector<SshProfile> profiles;
        std::size_t selectedIndex = 0;
        saved = loadSshProfiles(profiles, selectedIndex);
        if (saved.success) saved = saveSshProfileAt(profile, profiles.size());
    } else {
        saved = saveSshProfile(profile);
    }
    profile.password = "";
    profile.privateKeyPassphrase = "";
    if (!saved.success) {
        sendJsonError(400, saved.error);
        return;
    }
    consoleStatus = "SSH profile saved";
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleSshSelect()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    std::uint32_t index = 0;
    if (!parseUnsignedArgument(server.arg("index"), index)) {
        sendJsonError(400, "SSH profile index must be an unsigned integer");
        return;
    }
    const OperationResult result = selectSshProfile(index);
    if (!result.success) {
        sendJsonError(400, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleSshDelete()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    std::uint32_t index = 0;
    if (!parseUnsignedArgument(server.arg("index"), index)) {
        sendJsonError(400, "SSH profile index must be an unsigned integer");
        return;
    }
    const OperationResult result = deleteSshProfile(index);
    if (!result.success) {
        sendJsonError(400, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

OperationResult finishWebSshStart()
{
    OperationResult result = webSshClient.authenticate(webSshProfile, 60000);
    if (result.success) result = webSshClient.openTerminal(120, 36, 30000);
    if (!result.success) {
        webSshClient.close();
        webSshProfile.password = "";
        webSshProfile.privateKeyPassphrase = "";
        webSshTerminalOpen = false;
        return result;
    }
    webSshAwaitingTrust = false;
    webSshTerminalOpen = true;
    return {true, ""};
}

void handleSshStart()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    webSshClient.close();
    webSshTerminalOpen = false;
    webSshAwaitingTrust = false;
    const OperationResult loaded = loadSshProfile(webSshProfile);
    if (!loaded.success || !sshProfileIsComplete(webSshProfile)) {
        sendJsonError(400, loaded.success ? String("Selected SSH profile is incomplete") : loaded.error);
        return;
    }
    OperationResult result = webSshClient.connect(webSshProfile, 60000);
    if (!result.success) {
        sendJsonError(502, result.error);
        return;
    }
    const SshTrustResult trust = checkTrustedSshHost(
        webSshProfile.host, webSshProfile.port, webSshClient.fingerprint());
    if (!trust.success) {
        webSshClient.close();
        sendJsonError(500, trust.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    if (!trust.found || !trust.matches) {
        webSshAwaitingTrust = true;
        document["trust_required"] = true;
        document["changed"] = trust.found && !trust.matches;
        document["host"] = webSshProfile.host;
        document["port"] = webSshProfile.port;
        document["key_type"] = webSshClient.hostKeyType();
        document["fingerprint"] = webSshClient.fingerprint();
        document["open"] = false;
        sendJson(200, document);
        return;
    }
    result = finishWebSshStart();
    if (!result.success) {
        sendJsonError(502, result.error);
        return;
    }
    document["open"] = true;
    sendJson(200, document);
}

void handleSshTrust()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    if (!webSshAwaitingTrust || server.arg("fingerprint") != webSshClient.fingerprint()) {
        sendJsonError(409, "SSH trust request does not match the pending connection");
        return;
    }
    OperationResult result = trustSshHost(webSshProfile.host, webSshProfile.port,
                                          webSshClient.fingerprint());
    if (result.success) result = finishWebSshStart();
    if (!result.success) {
        sendJsonError(502, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["open"] = true;
    sendJson(200, document);
}

void handleSshInput()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const String data = server.arg("data");
    if (!webSshTerminalOpen || !webSshClient.isOpen() || data.isEmpty() || data.length() > 512) {
        sendJsonError(409, "SSH terminal is closed or input is outside the 1-512 byte limit");
        return;
    }
    const OperationResult result = webSshClient.write(
        reinterpret_cast<const std::uint8_t*>(data.c_str()), data.length(), 5000);
    if (!result.success) {
        sendJsonError(502, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleSshOutput()
{
    if (!sessionIsActive()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    std::uint8_t buffer[2048] = {};
    const int readBytes = webSshTerminalOpen ? webSshClient.read(buffer, sizeof(buffer)) : 0;
    if (readBytes < 0) {
        webSshClient.close();
        webSshTerminalOpen = false;
        sendJsonError(502, "SSH terminal read failed with code " + String(readBytes));
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    const bool open = webSshTerminalOpen && webSshClient.isOpen();
    document["open"] = open;
    if (readBytes > 0) {
        document["output"] = String(reinterpret_cast<const char*>(buffer), readBytes);
    }
    if (!open) {
        webSshClient.close();
        webSshTerminalOpen = false;
        webSshProfile.password = "";
        webSshProfile.privateKeyPassphrase = "";
    }
    sendJson(200, document);
}

void handleSshStop()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    webSshClient.close();
    webSshTerminalOpen = false;
    webSshAwaitingTrust = false;
    webSshProfile.password = "";
    webSshProfile.privateKeyPassphrase = "";
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

OperationResult ensureWebSftp()
{
    if (!webSshTerminalOpen || !webSshClient.isOpen()) {
        return {false, "Connect the SSH terminal before using SFTP"};
    }
    return webSshClient.openSftp(30000);
}

void handleSftpList()
{
    if (!sessionIsActive()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const OperationResult opened = ensureWebSftp();
    if (!opened.success) {
        sendJsonError(409, opened.error);
        return;
    }
    const SftpEntriesResult result = webSshClient.listSftpDirectory(server.arg("path"), 30000);
    if (!result.success) {
        sendJsonError(502, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    JsonArray entries = document["entries"].to<JsonArray>();
    for (const auto& entry : result.entries) {
        JsonObject item = entries.add<JsonObject>();
        item["name"] = entry.name;
        item["directory"] = entry.directory;
        item["size"] = entry.size;
    }
    sendJson(200, document);
}

void handleSftpDownload()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    OperationResult result = ensureWebSftp();
    if (result.success) result = webSshClient.downloadSftpFile(
        server.arg("path"), server.arg("name"), 60000);
    if (!result.success) {
        sendJsonError(502, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleSftpUpload()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    OperationResult result = ensureWebSftp();
    if (result.success) result = webSshClient.uploadSftpFile(
        server.arg("name"), server.arg("path"), 60000);
    if (!result.success) {
        sendJsonError(502, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleSshKeyUploadData()
{
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        sshKeyUploadError = "";
        sshKeyUploadBytes = 0;
        if (!requestHasValidCsrf()) {
            sshKeyUploadError = "Authentication required";
            return;
        }
        const OperationResult initialized = initializeSshStorage();
        if (!initialized.success) {
            sshKeyUploadError = initialized.error;
            return;
        }
        SD.remove(kSshKeyUploadPath);
        sshKeyUploadFile = SD.open(kSshKeyUploadPath, FILE_WRITE);
        if (!sshKeyUploadFile) {
            sshKeyUploadError = "Failed to create temporary SSH private-key upload";
        }
        return;
    }
    if (upload.status == UPLOAD_FILE_WRITE) {
        if (!sshKeyUploadError.isEmpty()) {
            return;
        }
        if (!sshKeyUploadFile || upload.currentSize > 16384 - sshKeyUploadBytes) {
            sshKeyUploadError = "SSH private key exceeds 16384 bytes";
            if (sshKeyUploadFile) {
                sshKeyUploadFile.close();
            }
            SD.remove(kSshKeyUploadPath);
            return;
        }
        const std::size_t written = sshKeyUploadFile.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            sshKeyUploadError = "microSD did not accept the complete SSH key chunk";
            sshKeyUploadFile.close();
            SD.remove(kSshKeyUploadPath);
            return;
        }
        sshKeyUploadBytes += written;
        return;
    }
    if (upload.status == UPLOAD_FILE_END) {
        if (!sshKeyUploadError.isEmpty()) {
            return;
        }
        sshKeyUploadFile.flush();
        sshKeyUploadFile.close();
        const OperationResult installed = installSshPrivateKey(kSshKeyUploadPath);
        SD.remove(kSshKeyUploadPath);
        if (!installed.success) {
            sshKeyUploadError = installed.error;
        }
        return;
    }
    if (upload.status == UPLOAD_FILE_ABORTED) {
        if (sshKeyUploadFile) {
            sshKeyUploadFile.close();
        }
        SD.remove(kSshKeyUploadPath);
        sshKeyUploadError = "SSH private-key upload was aborted";
    }
}

void handleSshKeyUploadComplete()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    if (!sshKeyUploadError.isEmpty()) {
        sendJsonError(400, sshKeyUploadError);
        return;
    }
    consoleStatus = "SSH private key installed";
    JsonDocument document;
    document["ok"] = true;
    document["bytes"] = sshKeyUploadBytes;
    sendJson(200, document);
}

void handleQrShow()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const String content = server.arg("content");
    const std::string payload = content.c_str();
    if (payload.empty() || payload.size() > kMaximumQrPayloadBytes ||
        !isValidUtf8(payload)) {
        sendJsonError(400, "QR content must be valid UTF-8 between 1 and 320 bytes");
        return;
    }
    consoleQrPayload = content;
    showQrCode("WEB QR", consoleQrPayload, "Manage in Web Console");
    JsonDocument document;
    document["ok"] = true;
    document["bytes"] = payload.size();
    sendJson(200, document);
}

void handleQrFile()
{
    if (!sessionIsActive()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const WorkspaceChunkResult result = readWorkspaceFileChunk(
        server.arg("name"), 0, kMaximumQrPayloadBytes + 1);
    if (!result.success) {
        sendJsonError(400, result.error);
        return;
    }
    if (!result.eof || result.totalBytes == 0 ||
        result.totalBytes > kMaximumQrPayloadBytes) {
        sendJsonError(400, "Selected file must contain 1 to 320 UTF-8 bytes for QR display");
        return;
    }
    consoleQrPayload = result.content.c_str();
    showQrCode("FILE QR", consoleQrPayload, "Manage in Web Console");
    JsonDocument document;
    document["ok"] = true;
    document["content"] = result.content;
    document["bytes"] = result.totalBytes;
    sendJson(200, document);
}

void handleQrClose()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    consoleQrPayload = "";
    showWebConsoleAccess("http://" + WiFi.localIP().toString(), accessPassword);
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleFileRead()
{
    if (!sessionIsActive()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    std::uint32_t offset = 0;
    if (!parseUnsignedArgument(server.arg("offset"), offset)) {
        sendJsonError(400, "File offset must be an unsigned integer");
        return;
    }
    const WorkspaceChunkResult result = readWorkspaceFileChunk(
        server.arg("name"), offset, kMaximumWebFileChunkBytes);
    if (!result.success) {
        sendJsonError(400, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["content"] = result.content;
    document["offset"] = result.offset;
    document["next_offset"] = result.nextOffset;
    document["total_bytes"] = result.totalBytes;
    document["eof"] = result.eof;
    sendJson(200, document);
}

void handleFileSave()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    std::uint32_t offset = 0;
    std::uint32_t originalBytes = 0;
    if (!parseUnsignedArgument(server.arg("offset"), offset) ||
        !parseUnsignedArgument(server.arg("original_bytes"), originalBytes)) {
        sendJsonError(400, "File offsets must be unsigned integers");
        return;
    }
    const std::string content = server.arg("content").c_str();
    if (content.size() > kMaximumWebFileChunkBytes || !isValidUtf8(content)) {
        sendJsonError(400, "File chunk must be valid UTF-8 up to 12288 bytes");
        return;
    }
    const OperationResult result = replaceWorkspaceFileRange(
        server.arg("name"), offset, originalBytes, content);
    if (!result.success) {
        sendJsonError(400, result.error);
        return;
    }
    consoleStatus = "File chunk saved";
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleFileRename()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const OperationResult result = renameWorkspaceFile(server.arg("name"), server.arg("new_name"));
    if (!result.success) {
        sendJsonError(400, result.error);
        return;
    }
    consoleStatus = "File renamed";
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleFileDelete()
{
    if (!requestHasValidCsrf()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const OperationResult result = deleteWorkspaceFile(server.arg("name"));
    if (!result.success) {
        sendJsonError(400, result.error);
        return;
    }
    consoleStatus = "File deleted";
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
}

void handleFileDownload()
{
    if (!sessionIsActive()) {
        sendJsonError(401, "Authentication required");
        return;
    }
    const String name = server.arg("name");
    if (!isValidWorkspaceFilename(name.c_str())) {
        sendJsonError(400, "Invalid workspace filename");
        return;
    }
    File file = SD.open(workspaceFilePath(name), FILE_READ);
    if (!file) {
        sendJsonError(404, "Workspace file does not exist: " + name);
        return;
    }
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    server.streamFile(file, "text/plain; charset=utf-8");
    file.close();
}

void handleFileUploadData()
{
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        uploadReplacing = server.arg("replace") == "1";
        uploadName = uploadReplacing ? server.arg("name") : upload.filename;
        uploadStorageName = uploadReplacing ? String(kWebSaveTemporaryName) : uploadName;
        uploadError = "";
        uploadBytes = 0;
        uploadCreated = false;
        if (!requestHasValidCsrf()) {
            uploadError = "Authentication required";
            return;
        }
        if (uploadReplacing &&
            (!isValidWorkspaceFilename(uploadName.c_str()) ||
             !SD.exists(workspaceFilePath(uploadName)))) {
            uploadError = "Workspace file does not exist: " + uploadName;
            return;
        }
        if (uploadReplacing && SD.exists(workspaceFilePath(uploadStorageName)) &&
            !SD.remove(workspaceFilePath(uploadStorageName))) {
            uploadError = "Failed to clear the temporary Web Console save file";
            return;
        }
        if (!uploadReplacing) {
            const OperationResult created = createWorkspaceFile(uploadStorageName);
            if (!created.success) {
                uploadError = created.error;
                return;
            }
        }
        uploadCreated = true;
        uploadFile = SD.open(workspaceFilePath(uploadStorageName),
                             uploadReplacing ? FILE_WRITE : FILE_APPEND);
        if (!uploadFile) {
            failUpload("Failed to open the new workspace file for upload");
        }
        return;
    }
    if (upload.status == UPLOAD_FILE_WRITE) {
        if (!uploadError.isEmpty()) {
            return;
        }
        if (!uploadFile || upload.currentSize > kMaximumWorkspaceFileBytes - uploadBytes) {
            failUpload("Uploaded file exceeds the 491520-byte size limit");
            return;
        }
        const std::size_t written = uploadFile.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            failUpload("microSD did not accept the complete upload chunk");
            return;
        }
        uploadBytes += written;
        return;
    }
    if (upload.status == UPLOAD_FILE_END) {
        if (!uploadError.isEmpty()) {
            return;
        }
        uploadFile.flush();
        uploadFile.close();
        const OperationResult valid = validateWorkspaceFileUtf8(uploadStorageName);
        if (!valid.success) {
            failUpload(valid.error);
            return;
        }
        if (uploadReplacing) {
            const OperationResult replaced = replaceWorkspaceFileWithTemporary(
                uploadName, uploadStorageName);
            if (!replaced.success) {
                failUpload(replaced.error);
                return;
            }
        }
        uploadCreated = false;
        return;
    }
    if (upload.status == UPLOAD_FILE_ABORTED) {
        failUpload("File upload was aborted before completion");
    }
}

void handleFileUploadComplete()
{
    if (!requestHasValidCsrf()) {
        failUpload("Authentication required");
        sendJsonError(401, "Authentication required");
        return;
    }
    if (!uploadError.isEmpty()) {
        sendJsonError(400, uploadError);
        return;
    }
    consoleStatus = uploadReplacing ? "File saved" : "File uploaded";
    JsonDocument document;
    document["ok"] = true;
    document["name"] = uploadName;
    document["bytes"] = uploadBytes;
    sendJson(200, document);
}

void updateConsoleSerial()
{
    while (Serial.available() > 0) {
        const char character = static_cast<char>(Serial.read());
        if (character == '\n') {
            consoleSerialInput.trim();
            if (consoleSerialInput == "PING") {
                Serial.println("PONG");
            } else if (consoleSerialInput == "STATUS") {
                Serial.printf("WEB_CONSOLE status=ready authenticated=%s heap=%u\n",
                              sessionToken.isEmpty() ? "no" : "yes",
                              static_cast<unsigned int>(ESP.getFreeHeap()));
            } else if (consoleSerialInput == "EXIT") {
                exitRequested = true;
                Serial.println("WEB_CONSOLE exit=requested");
            } else if (!consoleSerialInput.isEmpty()) {
                Serial.println("ERROR event=web_console_serial reason=unsupported_command");
            }
            Serial.flush();
            consoleSerialInput = "";
        } else if (character >= 'A' && character <= 'Z' && consoleSerialInput.length() < 32) {
            consoleSerialInput += character;
        } else if (character != '\r') {
            consoleSerialInput = "";
        }
    }
}

}  // namespace

WebConsoleResult runWebConsole(const Settings& settings, const String& initialChatId,
                               const String& version)
{
    if (WiFi.status() != WL_CONNECTED) {
        return {false, initialChatId, "Web console requires an active Wi-Fi connection"};
    }
    Serial.println("WEB_CONSOLE stage=load_password");
    Serial.flush();
    consoleSettings = settings;
    firmwareVersion = version;
    OperationResult result = loadSetupAccessPointPassword(accessPassword);
    if (!result.success || accessPassword.isEmpty()) {
        clearConsoleSecrets();
        return {false, initialChatId,
                result.success ? String("Installation password is missing") : result.error};
    }
    Serial.println("WEB_CONSOLE stage=refresh_chats");
    Serial.flush();
    result = refreshChats();
    if (!result.success) {
        clearConsoleSecrets();
        return {false, initialChatId, result.error};
    }
    Serial.println("WEB_CONSOLE stage=load_chat");
    Serial.flush();
    if (!initialChatId.isEmpty()) {
        result = loadActiveChat(initialChatId);
    } else if (!consoleChats.empty()) {
        result = loadActiveChat(consoleChats.front().id);
    } else {
        const ChatDocumentResult created = createChat("New chat");
        result = created.success ? OperationResult{true, ""}
                                 : OperationResult{false, created.error};
        if (created.success) {
            activeChat = created.chat;
            result = refreshChats();
        }
    }
    if (!result.success) {
        clearConsoleSecrets();
        return {false, initialChatId, result.error};
    }
    sessionToken = "";
    csrfToken = "";
    consoleStatus = "";
    exitRequested = false;
    pythonRestartRequested = false;
    consoleEscapeConsumed = false;
    loginFailures = 0;
    loginLockedUntil = 0;
    if (!routesConfigured) {
        const char* headers[] = {"Cookie", "X-CardMind-CSRF"};
        server.collectHeaders(headers, 2);
        server.on("/", HTTP_GET, sendRoot);
        server.on("/login", HTTP_POST, handleLogin);
        server.on("/logout", HTTP_POST, handleLogout);
        server.on("/api/session", HTTP_GET, handleSession);
        server.on("/api/console/close", HTTP_POST, handleCloseConsole);
        server.on("/api/state", HTTP_GET, handleState);
        server.on("/api/prompt", HTTP_POST, handlePrompt);
        server.on("/api/chat/select", HTTP_POST, handleSelectChat);
        server.on("/api/chat/new", HTTP_POST, handleNewChat);
        server.on("/api/chat/instructions", HTTP_POST, handleInstructions);
        server.on("/api/chat/permissions", HTTP_POST, handleChatPermissions);
        server.on("/api/chat/rename", HTTP_POST, handleRenameChat);
        server.on("/api/chat/pin", HTTP_POST, handlePinChat);
        server.on("/api/chat/archive", HTTP_POST, handleArchiveChat);
        server.on("/api/chat/duplicate", HTTP_POST, handleDuplicateChat);
        server.on("/api/chat/export", HTTP_POST, handleExportChat);
        server.on("/api/chat/export-bundle", HTTP_POST, handleExportChatBundle);
        server.on("/api/chat/import", HTTP_POST, handleImportChatBundle);
        server.on("/api/chat/delete", HTTP_POST, handleDeleteChat);
        server.on("/api/chat/clear", HTTP_POST, handleClearChat);
        server.on("/api/chat/archived", HTTP_GET, handleArchivedMessages);
        server.on("/api/settings", HTTP_POST, handleSettings);
        server.on("/api/models", HTTP_GET, handleModels);
        server.on("/api/diagnostics", HTTP_GET, handleDiagnosticsDownload);
        server.on("/api/python/start", HTTP_POST, handlePythonStart);
        server.on("/api/ssh/settings", HTTP_POST, handleSshSettings);
        server.on("/api/ssh/select", HTTP_POST, handleSshSelect);
        server.on("/api/ssh/delete", HTTP_POST, handleSshDelete);
        server.on("/api/ssh/start", HTTP_POST, handleSshStart);
        server.on("/api/ssh/trust", HTTP_POST, handleSshTrust);
        server.on("/api/ssh/input", HTTP_POST, handleSshInput);
        server.on("/api/ssh/output", HTTP_GET, handleSshOutput);
        server.on("/api/ssh/stop", HTTP_POST, handleSshStop);
        server.on("/api/ssh/sftp/list", HTTP_GET, handleSftpList);
        server.on("/api/ssh/sftp/download", HTTP_POST, handleSftpDownload);
        server.on("/api/ssh/sftp/upload", HTTP_POST, handleSftpUpload);
        server.on("/api/ssh/key", HTTP_POST,
                  handleSshKeyUploadComplete, handleSshKeyUploadData);
        server.on("/api/qr/show", HTTP_POST, handleQrShow);
        server.on("/api/qr/file", HTTP_GET, handleQrFile);
        server.on("/api/qr/close", HTTP_POST, handleQrClose);
        server.on("/api/file", HTTP_GET, handleFileRead);
        server.on("/api/file/save", HTTP_POST, handleFileSave);
        server.on("/api/file/rename", HTTP_POST, handleFileRename);
        server.on("/api/file/delete", HTTP_POST, handleFileDelete);
        server.on("/api/file/download", HTTP_GET, handleFileDownload);
        server.on("/api/file/upload", HTTP_POST,
                  handleFileUploadComplete, handleFileUploadData);
        server.onNotFound([]() { sendJsonError(404, "Not found"); });
        routesConfigured = true;
    }
    if (!serverStarted) {
        Serial.println("WEB_CONSOLE stage=server_begin");
        Serial.flush();
        server.begin();
        Serial.println("WEB_CONSOLE stage=server_ready");
        Serial.flush();
        serverStarted = true;
    }
    Serial.printf("WEB_CONSOLE result=ready address=http://%s/\n",
                  WiFi.localIP().toString().c_str());
    Serial.flush();
    showWebConsoleAccess("http://" + WiFi.localIP().toString(), accessPassword);
    bool escapeHeld = false;
    while (!exitRequested) {
        server.handleClient();
        if (pythonRestartRequested) {
            showBusyScreen("PYTHON WORKSPACE", "Restarting into MicroPython...");
            delay(500);
            ESP.restart();
        }
        updateConsoleSerial();
        M5Cardputer.update();
        const bool escapePressed = consoleEscapePressed();
        if (consoleEscapeConsumed) {
            escapeHeld = escapePressed;
            if (!escapePressed) {
                consoleEscapeConsumed = false;
            }
        } else {
            if (escapePressed && !escapeHeld) {
                exitRequested = true;
            }
            escapeHeld = escapePressed;
        }
        delay(2);
    }
    showBusyScreen("WEB CONSOLE", "Closing browser and SSH sessions...");
    // Keep the listener bound because repeated stop/begin cycles exhaust lwIP sockets.
    server.client().stop();
    clearConsoleSecrets();
    while (!M5Cardputer.Keyboard.keyList().empty()) {
        M5Cardputer.update();
        delay(5);
    }
    Serial.println("WEB_CONSOLE result=stopped");
    return {true, activeChat.summary.id, ""};
}

}  // namespace cardputer
