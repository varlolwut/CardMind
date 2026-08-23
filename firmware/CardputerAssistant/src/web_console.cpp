#include "web_console.h"

#include "api_client.h"
#include "chat_storage.h"
#include "crash_journal.h"
#include "file_workspace.h"
#include "storage.h"
#include "ssh_client.h"
#include "text_utils.h"
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
File uploadFile;
String uploadName;
String uploadError;
std::size_t uploadBytes = 0;
bool uploadCreated = false;
File sshKeyUploadFile;
String sshKeyUploadError;
std::size_t sshKeyUploadBytes = 0;
constexpr const char* kSshKeyUploadPath = "/assistant/ssh/upload.tmp";
SshClient webSshClient;
SshProfile webSshProfile = {"", "", 22, "", "", SshAuthMode::Password, ""};
bool webSshAwaitingTrust = false;
bool webSshTerminalOpen = false;
bool consoleEscapeConsumed = false;

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
}

void failUpload(const String& error)
{
    if (uploadFile) {
        uploadFile.close();
    }
    if (uploadCreated && !uploadName.isEmpty()) {
        SD.remove(workspaceFilePath(uploadName));
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
    static const char pageStart[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CardMind Console</title><style>
:root{color-scheme:dark;--bg:#050b12;--sidebar:#08111c;--surface:#0d1825;--raised:#121f2e;--line:#203247;--line2:#2e4660;--text:#f3f7fb;--muted:#8fa4b9;--accent:#65f2cc;--blue:#6eb7ff;--warn:#ffd479;--danger:#ff7185;--shadow:0 18px 55px #0006}
*{box-sizing:border-box}html,body{min-height:100%;background:var(--bg)}body{margin:0;font:14px Inter,ui-sans-serif,system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:var(--text);letter-spacing:.005em}button,input,textarea,select{font:inherit}button{touch-action:manipulation}
.app{min-height:100vh;display:grid;grid-template-columns:224px minmax(0,1fr);background:radial-gradient(circle at 70% -10%,#143350 0,transparent 34rem)}.sidebar{position:sticky;top:0;height:100vh;padding:22px 14px;display:flex;flex-direction:column;border-right:1px solid var(--line);background:#07101bea;backdrop-filter:blur(18px)}
.brand{display:flex;align-items:center;gap:11px;padding:0 8px 24px}.brand-mark{width:38px;height:38px;border:1px solid #65f2cc66;border-radius:12px;display:grid;place-items:center;background:linear-gradient(145deg,#173a45,#0c202b);color:var(--accent);font-weight:900;box-shadow:inset 0 0 20px #65f2cc18}.brand strong{display:block;font-size:16px;letter-spacing:.12em}.brand small{color:var(--muted)}
.nav{display:grid;gap:6px}.tab{width:100%;display:flex;align-items:center;gap:11px;padding:11px 12px;border:0;border-radius:11px;background:transparent;color:var(--muted);text-align:left;cursor:pointer;font-weight:700}.tab:hover{background:#132335;color:var(--text)}.tab.active{background:linear-gradient(100deg,#183b42,#122738);color:var(--accent);box-shadow:inset 3px 0 var(--accent)}.nav-glyph{width:28px;height:28px;border:1px solid var(--line2);border-radius:8px;display:grid;place-items:center;font-size:10px;letter-spacing:.04em;color:var(--blue)}
.side-status{margin-top:auto;padding:12px;border:1px solid var(--line);border-radius:12px;background:#0b1724}.online{display:flex;align-items:center;gap:8px;font-weight:700}.online-dot{width:8px;height:8px;border-radius:50%;background:var(--accent);box-shadow:0 0 12px var(--accent)}.device-pill{display:block;margin-top:5px;color:var(--muted);font-size:12px}.workspace{min-width:0}.topbar{height:72px;padding:0 28px;display:flex;align-items:center;gap:14px;border-bottom:1px solid var(--line);background:#07101bb8;backdrop-filter:blur(18px);position:sticky;top:0;z-index:5}.page-title{font-size:17px;font-weight:800}.page-subtitle{color:var(--muted);font-size:12px}.spacer{flex:1}
.shell{padding:24px 28px 42px;max-width:1480px;margin:auto}.panel{display:none}.panel.active{display:block}.card{background:linear-gradient(145deg,#0f1c2a,#0a1521);border:1px solid var(--line);border-radius:17px;box-shadow:var(--shadow)}.card-pad{padding:18px}.section-head{display:flex;align-items:flex-start;gap:12px;margin-bottom:16px}.section-head h2{margin:0;font-size:17px}.section-head p{margin:4px 0 0;color:var(--muted);font-size:12px}.section-actions{margin-left:auto;display:flex;gap:8px;flex-wrap:wrap}
.chat-layout{display:grid;grid-template-columns:276px minmax(0,1fr);gap:16px;min-height:calc(100vh - 138px)}.chat-rail{padding:16px;align-self:start}.chat-main{display:flex;flex-direction:column;min-height:calc(100vh - 138px);overflow:hidden}.chat-main .section-head{padding:17px 18px 12px;margin:0;border-bottom:1px solid var(--line)}
#messages{flex:1;min-height:44vh;max-height:calc(100vh - 340px);overflow:auto;padding:20px;scrollbar-color:#39546f transparent}.message{white-space:pre-wrap;max-width:min(78%,760px);padding:12px 14px;border-radius:15px;margin:0 0 13px;line-height:1.5;border:1px solid transparent}.user{margin-left:auto;background:#163840;border-color:#24525a}.assistant{background:#152238;border-color:#273b5b}.stream{color:var(--warn)}.composer{padding:14px;border-top:1px solid var(--line);background:#0a1522}.composer textarea{min-height:74px;max-height:190px}.composer-bar{display:flex;align-items:center;gap:8px;margin-top:8px}.shortcut{color:var(--muted);font-size:11px;margin-right:auto}
textarea,input,select{box-sizing:border-box;width:100%;background:#07111c;color:var(--text);border:1px solid var(--line2);border-radius:10px;padding:10px 11px;outline:none}textarea:focus,input:focus,select:focus{border-color:var(--accent);box-shadow:0 0 0 3px #65f2cc16}textarea{min-height:96px;resize:vertical}button{border:0;border-radius:10px;padding:10px 13px;cursor:pointer;background:var(--accent);color:#052019;font-weight:850}button:hover{filter:brightness(1.08)}button:disabled{opacity:.45;cursor:not-allowed}.secondary{background:#1b3046;color:#dcecff}.ghost{background:transparent;color:var(--muted);border:1px solid var(--line2)}.danger{background:#512331;color:#ffc4cc}.icon-button{padding:9px 11px}
.field-label{display:block;color:#b9cada;font-size:12px;font-weight:700;margin:13px 0 6px}.stack{display:grid;gap:9px}.row{display:flex;gap:8px;flex-wrap:wrap}.row>*{flex:1 1 150px}.compact>*{flex:0 1 auto}.full{width:100%}small,.muted{color:var(--muted)}details{margin-top:12px;border-top:1px solid var(--line);padding-top:8px}summary{cursor:pointer;color:#b9cada;padding:7px 0;font-weight:700}
.split-layout{display:grid;grid-template-columns:minmax(250px,.72fr) minmax(0,1.55fr);gap:16px}.file-editor{min-height:58vh;font:13px ui-monospace,SFMono-Regular,Consolas,monospace;line-height:1.55}.terminal-layout{display:grid;grid-template-columns:320px minmax(0,1fr);gap:16px}.terminal{margin:0;background:#02060b;color:#c8ffe8;border:1px solid #274158;border-radius:12px;padding:14px;min-height:55vh;max-height:68vh;overflow:auto;white-space:pre-wrap;font:13px ui-monospace,SFMono-Regular,Consolas,monospace;line-height:1.48;outline:none}.terminal:focus{border-color:var(--accent);box-shadow:0 0 0 3px #65f2cc16}.terminal-card{padding:16px}.terminal-toolbar{display:flex;gap:8px;align-items:center;margin-bottom:10px;flex-wrap:wrap}.terminal-state{margin-right:auto;color:var(--muted);font-size:12px}.metrics{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px}.metric{background:#081522;border:1px solid var(--line);border-radius:12px;padding:13px}.metric b{display:block;color:var(--accent);font-size:20px}.metric span{color:var(--muted);font-size:11px}.settings-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}
.statusbar{position:fixed;right:22px;bottom:22px;z-index:20;max-width:min(430px,calc(100vw - 28px));padding:11px 14px;border:1px solid #765d28;border-radius:11px;background:#271f0eea;color:var(--warn);box-shadow:var(--shadow);backdrop-filter:blur(12px)}.statusbar:empty{display:none}.mobile-nav{display:none}.dialog{width:min(430px,calc(100vw - 28px));padding:0;border:1px solid var(--line2);border-radius:16px;background:#0d1825;color:var(--text);box-shadow:0 25px 80px #000b}.dialog::backdrop{background:#01050acc;backdrop-filter:blur(4px)}.dialog-body{padding:20px}.dialog h2{margin:0 0 7px}.dialog p{color:var(--muted);white-space:pre-wrap}.dialog-actions{display:flex;justify-content:flex-end;gap:8px;margin-top:16px}#saveSettings{margin:13px 0 9px}#saveSettings+small{display:block;line-height:1.45}
@media(max-width:960px){.app{grid-template-columns:86px minmax(0,1fr)}.brand{justify-content:center;padding-inline:0}.brand-copy,.nav-label,.side-status{display:none}.tab{justify-content:center;padding:10px}.chat-layout{grid-template-columns:230px minmax(0,1fr)}.terminal-layout{grid-template-columns:270px minmax(0,1fr)}}
@media(max-width:720px){body{padding-bottom:68px}.app{display:block}.sidebar{display:none}.topbar{height:62px;padding:0 14px}.shell{padding:13px 10px 22px}.page-subtitle{display:none}.mobile-nav{position:fixed;display:grid;grid-template-columns:repeat(4,1fr);left:8px;right:8px;bottom:8px;z-index:30;padding:6px;border:1px solid var(--line2);border-radius:15px;background:#091521ee;box-shadow:var(--shadow);backdrop-filter:blur(16px)}.mobile-nav .tab{display:grid;gap:2px;padding:6px 3px;font-size:10px;justify-items:center}.mobile-nav .tab.active{box-shadow:none;background:#17323b}.mobile-nav .nav-glyph{width:24px;height:24px}.chat-layout,.split-layout,.terminal-layout,.settings-grid{grid-template-columns:1fr}.chat-main{min-height:calc(100vh - 154px)}#messages{max-height:none;min-height:44vh;padding:13px}.message{max-width:92%}.chat-rail{order:2}.terminal-card{order:-1}.file-editor{min-height:44vh}.terminal{min-height:50vh}.metrics{grid-template-columns:repeat(2,minmax(0,1fr))}.section-actions{width:100%;margin-left:0}.statusbar{right:14px;bottom:82px}.shortcut{display:none}}
</style></head><body>
<div class="app"><aside class="sidebar"><div class="brand"><div class="brand-mark">CM</div><div class="brand-copy"><strong>CARDMIND</strong><small>Device console</small></div></div><nav class="nav" aria-label="Console sections"><button class="tab active" data-panel="chat"><span class="nav-glyph">CH</span><span class="nav-label">Chat</span></button><button class="tab" data-panel="files"><span class="nav-glyph">FS</span><span class="nav-label">Files</span></button><button class="tab" data-panel="ssh"><span class="nav-glyph">&gt;_</span><span class="nav-label">SSH &amp; SFTP</span></button><button class="tab" data-panel="settings"><span class="nav-glyph">SYS</span><span class="nav-label">Settings</span></button></nav><div class="side-status"><span class="online"><span class="online-dot"></span>Cardputer online</span><span class="device-pill" id="device"></span></div></aside>
<section class="workspace"><header class="topbar"><div><div class="page-title" id="pageTitle">Chat</div><div class="page-subtitle" id="pageSubtitle">Continue a conversation on your Cardputer</div></div><span class="spacer"></span><button class="ghost" id="refresh">Refresh</button><button class="secondary" id="logout">Log out</button></header><div class="shell"><main>
<section class="panel active" data-view="chat"><div class="chat-layout"><aside class="card chat-rail"><div class="section-head"><div><h2>Conversations</h2><p>Stored on microSD</p></div><button class="icon-button" id="newChat" title="New chat">＋</button></div><select id="chats" size="7" aria-label="Chat"></select><details><summary>Manage chat</summary><div class="stack"><button class="secondary" id="renameChat">Rename</button><div class="row"><button class="ghost" id="pinChat">Pin</button><button class="ghost" id="archiveChat">Archive</button></div><button class="ghost" id="duplicateChat">Duplicate</button><div class="row"><button class="ghost" id="exportChat">Export .md</button><button class="ghost" id="exportBundle">Export bundle</button></div><button class="danger" id="deleteChat">Delete chat</button></div></details><label class="field-label" for="instructions">Chat instructions</label><textarea id="instructions" maxlength="2048" placeholder="For example: answer briefly and in Russian"></textarea><button class="full" id="saveInstructions">Save instructions</button></aside>
<section class="card chat-main"><div class="section-head"><div><h2 id="activeChatTitle">Conversation</h2><p><span id="activeModel">Model</span> · streaming enabled</p></div><div class="section-actions"><button class="ghost" id="retry">Retry last</button></div></div><div id="messages" aria-live="polite"></div><div class="composer"><textarea id="prompt" maxlength="1200" placeholder="Ask CardMind anything..."></textarea><div class="composer-bar"><span class="shortcut">Ctrl/⌘ + Enter to send</span><button class="danger" id="stop">Stop</button><button id="send">Send message</button></div></div></section></div></section>
<section class="panel" data-view="files"><div class="section-head"><div><h2>microSD workspace</h2><p>Browse and edit large files without loading them fully into RAM</p></div></div><div class="split-layout"><section class="card card-pad"><label class="field-label" for="files">Workspace files</label><select id="files" size="12"></select><div class="stack"><div class="row"><button id="openFile">Open</button><button class="secondary" id="downloadFile">Download</button></div><input id="uploadInput" type="file" accept=".txt,.md,.json,.jsonl,.csv,.html,.svg"><button id="uploadFile">Upload new file</button><button class="ghost" id="importChat">Import selected chat bundle</button><div class="row"><button class="ghost" id="renameFile">Rename</button><button class="danger" id="deleteFile">Delete</button></div></div></section><section class="card card-pad"><div class="section-head"><div><h2>Chunk editor</h2><p id="filePosition">Select a file to begin</p></div><div class="section-actions"><button class="ghost" id="previousFilePage">Previous</button><button class="ghost" id="nextFilePage">Next</button><button id="saveFile">Save chunk</button></div></div><textarea class="file-editor" id="fileContent" maxlength="12288" placeholder="File content appears here"></textarea></section></div></section>
<section class="panel" data-view="ssh"><div class="section-head"><div><h2>Remote terminal</h2><p>Interactive SSH and microSD-backed SFTP transfers</p></div></div><div class="terminal-layout"><aside class="stack"><section class="card card-pad"><div class="section-head"><div><h2>Connection</h2><p>Saved profiles stay on-device</p></div><button class="icon-button" id="newSsh" title="New profile">＋</button></div><select id="sshProfiles"></select><label class="field-label" for="sshName">Profile name</label><input id="sshName" maxlength="32" placeholder="Server"><div class="row"><input id="sshHost" maxlength="253" placeholder="Host"><input id="sshPort" type="number" min="1" max="65535" placeholder="Port"></div><input id="sshUser" maxlength="64" placeholder="Username"><select id="sshAuth"><option value="password">Password</option><option value="key">Private key</option></select><input id="sshPassword" type="password" maxlength="192" placeholder="New password (blank keeps current)"><input id="sshPassphrase" type="password" maxlength="192" placeholder="New key passphrase (blank keeps current)"><div class="row"><button id="saveSsh">Save profile</button><button class="danger" id="deleteSsh">Delete</button></div><input id="sshKeyInput" type="file" accept=".pem,.key"><button class="ghost" id="uploadSshKey">Install private key</button><small>Secrets are write-only. New host fingerprints require confirmation.</small></section><section class="card card-pad"><h2>SFTP</h2><div class="row"><input id="sftpPath" value="/" maxlength="511"><button id="listSftp">List</button></div><select id="sftpEntries" size="7"></select><div class="row"><button class="ghost" id="sftpOpen">Open directory</button><button id="sftpDownload">Download to SD</button></div><label class="field-label" for="sftpLocal">Workspace file to upload</label><div class="row"><select id="sftpLocal"></select><button id="sftpUpload">Upload</button></div></section></aside><section class="card terminal-card"><div class="terminal-toolbar"><span class="terminal-state" id="terminalState">Disconnected</span><button id="connectSsh">Connect</button><button id="disconnectSsh" class="danger">Disconnect</button><button class="ghost" id="clearSsh">Clear</button><button class="ghost" id="fullSsh">Fullscreen</button></div><pre id="sshTerminal" class="terminal" tabindex="0">Choose a profile and connect. Click here to type directly.</pre><div class="row"><input id="sshInput" maxlength="512" placeholder="Command for touch keyboards"><button id="sendSshInput">Send</button></div></section></div></section>
<section class="panel" data-view="settings"><div class="section-head"><div><h2>Device settings</h2><p>Non-secret connection settings and live diagnostics</p></div></div><div class="settings-grid"><section class="card card-pad"><h2>LLM connection</h2><label class="field-label" for="apiBaseUrl">OpenAI-compatible base URL</label><input id="apiBaseUrl" maxlength="180"><label class="field-label" for="model">Model</label><input id="model" maxlength="120"><button id="saveSettings">Save settings</button><small>API keys remain write-only and are never sent to this page.</small></section><section class="card card-pad"><h2>Live device status</h2><div id="diagnostics" class="metrics"></div></section></div></section>
</main></div></section></div><nav class="mobile-nav" aria-label="Mobile console sections"><button class="tab active" data-panel="chat"><span class="nav-glyph">CH</span>Chat</button><button class="tab" data-panel="files"><span class="nav-glyph">FS</span>Files</button><button class="tab" data-panel="ssh"><span class="nav-glyph">&gt;_</span>SSH</button><button class="tab" data-panel="settings"><span class="nav-glyph">SYS</span>Settings</button></nav><p id="status" class="statusbar" role="status"></p>
<dialog class="dialog" id="actionDialog"><form method="dialog" class="dialog-body"><h2 id="dialogTitle">Confirm action</h2><p id="dialogMessage"></p><input id="dialogInput"><div class="dialog-actions"><button class="ghost" value="cancel">Cancel</button><button id="dialogConfirm" value="confirm">Confirm</button></div></form></dialog>
<script>)HTML";
    static const char pageEnd[] PROGMEM = R"HTML(
const q=s=>document.querySelector(s);let state=null,fileName='',fileOffset=0,fileNextOffset=0,fileOriginalBytes=0,fileEof=true,fileBack=[],activeRequest=null,lastPrompt='',sshConnected=false,sshPoll=null,sftpState=[],sshCreating=false;
const panelCopy={chat:['Chat','Continue a conversation on your Cardputer'],files:['Files','Browse and edit the microSD workspace'],ssh:['SSH & SFTP','Work on remote machines from CardMind'],settings:['Settings','Connection details and device health']};
function showPanel(name){const target=panelCopy[name]?name:'chat';for(const p of document.querySelectorAll('.panel'))p.classList.toggle('active',p.dataset.view===target);for(const b of document.querySelectorAll('.tab'))b.classList.toggle('active',b.dataset.panel===target);q('#pageTitle').textContent=panelCopy[target][0];q('#pageSubtitle').textContent=panelCopy[target][1];history.replaceState(null,'','#'+target)}
for(const b of document.querySelectorAll('.tab'))b.onclick=()=>showPanel(b.dataset.panel);showPanel(location.hash.slice(1)||'chat');
function showError(error){q('#status').textContent=error instanceof Error?error.message:String(error)}
window.addEventListener('unhandledrejection',event=>{showError(event.reason);event.preventDefault()});
q('#prompt').addEventListener('keydown',event=>{if((event.ctrlKey||event.metaKey)&&event.key==='Enter'){event.preventDefault();q('#send').click()}});
function askDialog(title,message,value,confirmLabel,danger,inputVisible){const dialog=q('#actionDialog');q('#dialogTitle').textContent=title;q('#dialogMessage').textContent=message;q('#dialogInput').value=value||'';q('#dialogInput').hidden=!inputVisible;q('#dialogConfirm').textContent=confirmLabel;q('#dialogConfirm').className=danger?'danger':'';dialog.showModal();if(inputVisible)q('#dialogInput').focus();return new Promise(resolve=>dialog.addEventListener('close',()=>resolve(dialog.returnValue==='confirm'?(inputVisible?q('#dialogInput').value:true):null),{once:true}))}
const askText=(title,value)=>askDialog(title,'Enter a new value.',value,'Save',false,true);const askConfirm=(title,message,label)=>askDialog(title,message,'',label,true,false);
q('#dialogInput').onkeydown=event=>{if(event.key==='Enter'){event.preventDefault();q('#actionDialog').close('confirm')}};
async function request(path,options={}){options.headers={...(options.headers||{}),'X-CardMind-CSRF':csrf};const r=await fetch(path,options);if(r.status===401){location='/';throw new Error('Session expired')}if(!r.ok){let message=`HTTP ${r.status}`;try{message=(await r.json()).error||message}catch{}throw new Error(message)}return r}
function render(s){state=s;q('#device').textContent=`${s.ip} · ${s.battery}%`;q('#status').textContent=s.status||'';
q('#chats').innerHTML='';for(const c of s.chats){const o=document.createElement('option');o.value=c.id;o.textContent=`${c.pinned?'📌 ':c.archived?'📦 ':''}${c.title} · ${c.total_messages}`;o.selected=c.id===s.active_chat_id;q('#chats').append(o)}
q('#messages').innerHTML='';for(const m of s.messages){const d=document.createElement('div');d.className='message '+m.role;d.textContent=(m.role==='user'?'You: ':'AI: ')+m.content;q('#messages').append(d)}
q('#activeChatTitle').textContent=s.active_chat_title||'Conversation';q('#activeModel').textContent=s.model||'Model';q('#instructions').value=s.instructions||'';q('#apiBaseUrl').value=s.api_base_url||'';q('#model').value=s.model||'';
q('#sshProfiles').innerHTML='';for(const [i,p] of (s.ssh_profiles||[]).entries()){const o=document.createElement('option');o.value=i;o.textContent=`${i===s.ssh_selected?'★ ':''}${p.name} · ${p.username}@${p.host}`;o.selected=i===s.ssh_selected;q('#sshProfiles').append(o)}
q('#sshName').value=s.ssh_name||'';q('#sshHost').value=s.ssh_host||'';q('#sshPort').value=s.ssh_port||22;q('#sshUser').value=s.ssh_username||'';q('#sshAuth').value=s.ssh_auth_mode||'password';sshConnected=!!s.ssh_terminal_open;q('#terminalState').textContent=sshConnected?'Connected':'Disconnected';
const metrics=[['Battery',`${s.battery}%`],['Wi-Fi',`${s.wifi_rssi} dBm`],['Free heap',`${Math.round(s.free_heap/1024)} KiB`],['Largest block',`${Math.round(s.largest_heap/1024)} KiB`],['microSD used',`${Math.round(s.sd_used_bytes/1048576)} MiB`],['microSD total',`${Math.round(s.sd_total_bytes/1048576)} MiB`]];q('#diagnostics').textContent='';for(const [label,value] of metrics){const d=document.createElement('div'),b=document.createElement('b'),span=document.createElement('span');d.className='metric';b.textContent=value;span.textContent=label;d.append(b,span);q('#diagnostics').append(d)}
const selected=q('#files').value;q('#files').innerHTML='';q('#sftpLocal').innerHTML='';for(const f of s.files){const o=document.createElement('option');o.value=f.name;o.textContent=`${f.name} · ${f.size} B`;o.selected=f.name===selected;q('#files').append(o);q('#sftpLocal').append(o.cloneNode(true))}q('#messages').scrollTop=q('#messages').scrollHeight}
async function refresh(){const r=await request('/api/state');render(await r.json())}
async function post(path,values){return request(path,{method:'POST',body:new URLSearchParams(values)})}
async function openFile(offset,pushBack){const selected=q('#files').value;if(!selected)return;if(pushBack&&fileName===selected)fileBack.push(fileOffset);else if(fileName!==selected)fileBack=[];const r=await request(`/api/file?name=${encodeURIComponent(selected)}&offset=${offset}`);const f=await r.json();fileName=selected;fileOffset=f.offset;fileNextOffset=f.next_offset;fileOriginalBytes=f.next_offset-f.offset;fileEof=f.eof;q('#fileContent').value=f.content;q('#filePosition').textContent=`${fileOffset}-${fileNextOffset} / ${f.total_bytes} bytes`}
q('#refresh').onclick=refresh;q('#chats').onchange=async()=>{await request('/api/chat/select',{method:'POST',body:new URLSearchParams({id:q('#chats').value})});await refresh()};
q('#newChat').onclick=async()=>{await request('/api/chat/new',{method:'POST'});await refresh()};
q('#renameChat').onclick=async()=>{const title=await askText('Rename chat',state.active_chat_title);if(title){await post('/api/chat/rename',{title});await refresh()}};
q('#pinChat').onclick=async()=>{await post('/api/chat/pin',{});await refresh()};q('#archiveChat').onclick=async()=>{await post('/api/chat/archive',{});await refresh()};
q('#duplicateChat').onclick=async()=>{await post('/api/chat/duplicate',{});await refresh()};q('#exportChat').onclick=async()=>{await post('/api/chat/export',{});await refresh()};q('#exportBundle').onclick=async()=>{await post('/api/chat/export-bundle',{});await refresh()};
q('#deleteChat').onclick=async()=>{if(await askConfirm('Delete chat',`"${state.active_chat_title}" and its history will be removed from microSD.`,'Delete')){await post('/api/chat/delete',{});await refresh()}};
q('#saveInstructions').onclick=async()=>{await request('/api/chat/instructions',{method:'POST',body:new URLSearchParams({instructions:q('#instructions').value})});await refresh()};
q('#saveSettings').onclick=async()=>{await post('/api/settings',{api_base_url:q('#apiBaseUrl').value,model:q('#model').value});await refresh()};
q('#saveSsh').onclick=async()=>{const password=q('#sshPassword').value,passphrase=q('#sshPassphrase').value;await post('/api/ssh/settings',{name:q('#sshName').value,host:q('#sshHost').value,port:q('#sshPort').value,username:q('#sshUser').value,auth_mode:q('#sshAuth').value,password,replace_password:password?'1':'0',key_passphrase:passphrase,replace_key_passphrase:passphrase?'1':'0',create:sshCreating?'1':'0'});sshCreating=false;q('#sshPassword').value='';q('#sshPassphrase').value='';await refresh()};
q('#sshProfiles').onchange=async()=>{sshCreating=false;await post('/api/ssh/select',{index:q('#sshProfiles').value});await refresh()};
q('#newSsh').onclick=()=>{sshCreating=true;q('#sshName').value='Server';q('#sshHost').value='';q('#sshPort').value=22;q('#sshUser').value='';q('#sshPassword').value='';q('#sshPassphrase').value=''};
q('#deleteSsh').onclick=async()=>{if(await askConfirm('Delete SSH profile','The saved host and credentials for this profile will be removed.','Delete')){await post('/api/ssh/delete',{index:q('#sshProfiles').value});await refresh()}};
q('#uploadSshKey').onclick=async()=>{const file=q('#sshKeyInput').files[0];if(!file)return;const data=new FormData();data.append('file',file,file.name);await request('/api/ssh/key',{method:'POST',body:data});q('#sshKeyInput').value='';await refresh()};
function cleanSsh(v){return v.replace(/\x1b\[[0-?]*[ -\/]*[@-~]/g,'').replace(/\r(?!\n)/g,'')}
async function sendTerminal(data){if(!sshConnected)return;await post('/api/ssh/input',{data})}
async function pollSsh(){if(!sshConnected)return;try{const r=await request('/api/ssh/output');const x=await r.json();if(x.output){const t=q('#sshTerminal');t.textContent+=cleanSsh(x.output);if(t.textContent.length>131072)t.textContent=t.textContent.slice(-98304);t.scrollTop=t.scrollHeight}sshConnected=x.open;if(!x.open)await refresh()}catch(e){sshConnected=false;q('#status').textContent=e.message}}
q('#connectSsh').onclick=async()=>{const r=await request('/api/ssh/start',{method:'POST'}),x=await r.json();if(x.trust_required){if(!await askConfirm('Trust SSH host?',`${x.host}:${x.port}\n${x.key_type}\n${x.fingerprint}`,'Trust host'))return;const t=await post('/api/ssh/trust',{fingerprint:x.fingerprint}),y=await t.json();sshConnected=y.open}else sshConnected=x.open;q('#sshTerminal').textContent='';q('#sshTerminal').focus();if(!sshPoll)sshPoll=setInterval(pollSsh,300);await refresh()};
q('#disconnectSsh').onclick=async()=>{await post('/api/ssh/stop',{});sshConnected=false;await refresh()};q('#clearSsh').onclick=()=>q('#sshTerminal').textContent='';q('#fullSsh').onclick=()=>q('#sshTerminal').requestFullscreen();
q('#sendSshInput').onclick=async()=>{const v=q('#sshInput').value;if(v){await sendTerminal(v+'\r');q('#sshInput').value='';q('#sshTerminal').focus()}};q('#sshInput').onkeydown=e=>{if(e.key==='Enter'){e.preventDefault();q('#sendSshInput').click()}};
q('#sshTerminal').onkeydown=async e=>{let d='';if(e.ctrlKey&&e.key.length===1)d=String.fromCharCode(e.key.toUpperCase().charCodeAt(0)&31);else if(e.key==='Enter')d='\r';else if(e.key==='Backspace')d='\x7f';else if(e.key==='Tab')d='\t';else if(e.key==='ArrowUp')d='\x1b[A';else if(e.key==='ArrowDown')d='\x1b[B';else if(e.key==='ArrowRight')d='\x1b[C';else if(e.key==='ArrowLeft')d='\x1b[D';else if(e.key.length===1&&!e.metaKey&&!e.altKey)d=e.key;if(d){e.preventDefault();await sendTerminal(d)}};
q('#listSftp').onclick=async()=>{const r=await request(`/api/ssh/sftp/list?path=${encodeURIComponent(q('#sftpPath').value)}`),x=await r.json();sftpState=x.entries;q('#sftpEntries').innerHTML='';for(const [i,e] of x.entries.entries()){const o=document.createElement('option');o.value=i;o.textContent=`${e.directory?'📁':'📄'} ${e.name}${e.directory?'':` · ${e.size} B`}`;q('#sftpEntries').append(o)}};
q('#sftpOpen').onclick=()=>{const e=sftpState[Number(q('#sftpEntries').value)];if(e&&e.directory){const p=q('#sftpPath').value;q('#sftpPath').value=(p==='/'?'':p)+'/'+e.name;q('#listSftp').click()}};
q('#sftpDownload').onclick=async()=>{const e=sftpState[Number(q('#sftpEntries').value)];if(!e||e.directory)return;const name=await askText('Workspace filename',e.name);if(name){await post('/api/ssh/sftp/download',{path:(q('#sftpPath').value==='/'?'':q('#sftpPath').value)+'/'+e.name,name});await refresh()}};
q('#sftpUpload').onclick=async()=>{const name=q('#sftpLocal').value;if(name){const remote=await askText('Remote filename',name);if(remote){await post('/api/ssh/sftp/upload',{name,path:(q('#sftpPath').value==='/'?'':q('#sftpPath').value)+'/'+remote});q('#listSftp').click()}}};
q('#openFile').onclick=()=>openFile(0,false);q('#nextFilePage').onclick=()=>{if(!fileEof)openFile(fileNextOffset,true)};
q('#previousFilePage').onclick=()=>{if(fileBack.length)openFile(fileBack.pop(),false)};
q('#saveFile').onclick=async()=>{if(!fileName)return;await post('/api/file/save',{name:fileName,offset:String(fileOffset),original_bytes:String(fileOriginalBytes),content:q('#fileContent').value});await refresh();await openFile(fileOffset,false)};
q('#downloadFile').onclick=()=>{const name=q('#files').value;if(name)location=`/api/file/download?name=${encodeURIComponent(name)}`};
q('#uploadFile').onclick=async()=>{const file=q('#uploadInput').files[0];if(!file)return;const data=new FormData();data.append('file',file,file.name);await request('/api/file/upload',{method:'POST',body:data});q('#uploadInput').value='';await refresh()};
q('#importChat').onclick=async()=>{const name=q('#files').value;if(name){await post('/api/chat/import',{name});await refresh()}};
q('#renameFile').onclick=async()=>{const name=q('#files').value,newName=await askText('Rename file',name);if(newName&&newName!==name){await post('/api/file/rename',{name,new_name:newName});fileName='';await refresh()}};
q('#deleteFile').onclick=async()=>{const name=q('#files').value;if(name&&await askConfirm('Delete file',`${name} will be removed from the microSD workspace.`,'Delete')){await post('/api/file/delete',{name});fileName='';q('#fileContent').value='';q('#filePosition').textContent='';await refresh()}};
async function sendPrompt(){const prompt=q('#prompt').value.trim();if(!prompt)return;lastPrompt=prompt;q('#send').disabled=true;q('#status').textContent='Streaming...';activeRequest=new AbortController();
const d=document.createElement('div');d.className='message assistant stream';d.textContent='AI: ';q('#messages').append(d);
try{const r=await request('/api/prompt',{method:'POST',body:new URLSearchParams({prompt}),signal:activeRequest.signal});const reader=r.body.getReader(),decoder=new TextDecoder();let buffer='';
while(true){const x=await reader.read();if(x.done)break;buffer+=decoder.decode(x.value,{stream:true});const events=buffer.split('\n\n');buffer=events.pop();for(const e of events){if(!e.startsWith('data:'))continue;const v=JSON.parse(e.slice(5));if(v.delta)d.textContent+=v.delta;if(v.error)q('#status').textContent=v.error}}
q('#prompt').value=''}catch(e){if(e.name==='AbortError')q('#status').textContent='Canceled';else throw e}finally{activeRequest=null;q('#send').disabled=false;await refresh()}}
q('#send').onclick=sendPrompt;q('#stop').onclick=()=>{if(activeRequest)activeRequest.abort()};q('#retry').onclick=()=>{if(lastPrompt){q('#prompt').value=lastPrompt;sendPrompt()}};
q('#logout').onclick=async()=>{await request('/logout',{method:'POST'});location='/'};refresh();
</script></body></html>)HTML";
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html; charset=utf-8", "");
    server.sendContent_P(pageStart);
    server.sendContent("const csrf='" + csrfToken + "';\n");
    server.sendContent_P(pageEnd);
    server.sendContent("", 0);
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
    document["instructions"] = activeChat.instructions;
    document["status"] = consoleStatus;
    document["model"] = consoleSettings.model;
    document["api_base_url"] = consoleSettings.apiBaseUrl;
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

ToolExecutionResult executeConsoleTool(const ToolCall& call,
                                       const CancelCallback& isCancelled)
{
    if (isWebSearchToolName(call.name)) {
        return executeWebSearchTool(consoleSettings, call, isCancelled);
    }
    if (isWebFetchToolName(call.name)) {
        return executeWebFetchTool(consoleSettings, call, isCancelled);
    }
    if (call.name == "list_files" || call.name == "read_file" ||
        call.name == "write_file" || call.name == "append_file") {
        return executeWorkspaceTool(call);
    }
    return {false, "{\"ok\":false,\"error\":\"unsupported tool\"}",
            "API requested unsupported tool '" + String(call.name.c_str()) + "'"};
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
    const bool useWorkspace = requestsWorkspaceAccess(prompt);
    const bool useSearch = webSearchSettingsAreComplete(consoleSettings);
    const CancelCallback isCancelled = []() {
        M5Cardputer.update();
        if (consoleEscapePressed()) {
            consoleEscapeConsumed = true;
            return true;
        }
        return !server.client().connected();
    };
    markOperation(useWorkspace || useSearch ? "web_console_tools" : "web_console_chat");
    const ChatResult result = useWorkspace || useSearch
        ? streamChatCompletionWithTools(consoleSettings, activeChat.messages,
                                        activeChat.instructions, onText,
                                        [&isCancelled](const ToolCall& call) {
                                            return executeConsoleTool(call, isCancelled);
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
    updated.apiBaseUrl = server.arg("api_base_url");
    updated.model = server.arg("model");
    const OperationResult result = saveSettings(updated);
    if (!result.success) {
        sendJsonError(400, result.error);
        return;
    }
    consoleSettings = updated;
    consoleStatus = "Connection settings saved";
    JsonDocument document;
    document["ok"] = true;
    sendJson(200, document);
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
        uploadName = upload.filename;
        uploadError = "";
        uploadBytes = 0;
        uploadCreated = false;
        if (!requestHasValidCsrf()) {
            uploadError = "Authentication required";
            return;
        }
        const OperationResult created = createWorkspaceFile(uploadName);
        if (!created.success) {
            uploadError = created.error;
            return;
        }
        uploadCreated = true;
        uploadFile = SD.open(workspaceFilePath(uploadName), FILE_APPEND);
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
        const OperationResult valid = validateWorkspaceFileUtf8(uploadName);
        if (!valid.success) {
            failUpload(valid.error);
            return;
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
    consoleStatus = "File uploaded";
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

WebConsoleResult runWebConsole(const Settings& settings, const String& initialChatId)
{
    if (WiFi.status() != WL_CONNECTED) {
        return {false, initialChatId, "Web console requires an active Wi-Fi connection"};
    }
    Serial.println("WEB_CONSOLE stage=load_password");
    Serial.flush();
    consoleSettings = settings;
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
    consoleEscapeConsumed = false;
    loginFailures = 0;
    loginLockedUntil = 0;
    if (!routesConfigured) {
        const char* headers[] = {"Cookie", "X-CardMind-CSRF"};
        server.collectHeaders(headers, 2);
        server.on("/", HTTP_GET, sendRoot);
        server.on("/login", HTTP_POST, handleLogin);
        server.on("/logout", HTTP_POST, handleLogout);
        server.on("/api/state", HTTP_GET, handleState);
        server.on("/api/prompt", HTTP_POST, handlePrompt);
        server.on("/api/chat/select", HTTP_POST, handleSelectChat);
        server.on("/api/chat/new", HTTP_POST, handleNewChat);
        server.on("/api/chat/instructions", HTTP_POST, handleInstructions);
        server.on("/api/chat/rename", HTTP_POST, handleRenameChat);
        server.on("/api/chat/pin", HTTP_POST, handlePinChat);
        server.on("/api/chat/archive", HTTP_POST, handleArchiveChat);
        server.on("/api/chat/duplicate", HTTP_POST, handleDuplicateChat);
        server.on("/api/chat/export", HTTP_POST, handleExportChat);
        server.on("/api/chat/export-bundle", HTTP_POST, handleExportChatBundle);
        server.on("/api/chat/import", HTTP_POST, handleImportChatBundle);
        server.on("/api/chat/delete", HTTP_POST, handleDeleteChat);
        server.on("/api/settings", HTTP_POST, handleSettings);
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
    Serial.println("WEB_CONSOLE stage=server_begin");
    Serial.flush();
    server.begin();
    Serial.printf("WEB_CONSOLE result=ready address=http://%s/\n",
                  WiFi.localIP().toString().c_str());
    showWebConsoleAccess("http://" + WiFi.localIP().toString(), accessPassword);
    bool escapeHeld = false;
    while (!exitRequested) {
        server.handleClient();
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
    server.stop();
    clearConsoleSecrets();
    while (!M5Cardputer.Keyboard.keyList().empty()) {
        M5Cardputer.update();
        delay(5);
    }
    Serial.println("WEB_CONSOLE result=stopped");
    return {true, activeChat.summary.id, ""};
}

}  // namespace cardputer
