#include "web_console.h"

#include "api_client.h"
#include "chat_storage.h"
#include "crash_journal.h"
#include "file_workspace.h"
#include "storage.h"
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
    String output;
    serializeJson(document, output);
    server.send(statusCode, "application/json; charset=utf-8", output);
}

void sendJsonError(int statusCode, const String& error)
{
    JsonDocument document;
    document["ok"] = false;
    document["error"] = error;
    sendJson(statusCode, document);
}

std::size_t historyBytes(const std::vector<Message>& messages)
{
    std::size_t total = 0;
    for (const auto& message : messages) {
        total += message.content.size();
    }
    return total;
}

std::vector<Message> trimmedHistory(const std::vector<Message>& messages)
{
    std::vector<Message> result = messages;
    while (result.size() > kMaximumStoredMessages ||
           historyBytes(result) > kMaximumStoredHistoryBytes) {
        if (result.size() < 2) {
            result.clear();
            break;
        }
        result.erase(result.begin(), result.begin() + 2);
    }
    return result;
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
        "body{font:16px system-ui;background:#07111e;color:#eef;display:grid;place-items:center;min-height:90vh}"
        ".card{width:min(360px,86vw);background:#142136;padding:24px;border-radius:18px}"
        "input,button{box-sizing:border-box;width:100%;padding:12px;margin-top:12px;border-radius:9px}"
        "input{background:#091423;color:#fff;border:1px solid #47617f}"
        "button{border:0;background:#5eead4;color:#06201c;font-weight:800}"
        ".error{color:#fecaca}</style></head><body><form class='card' method='post' action='/login'>"
        "<h1>CardMind</h1><p>Enter the installation password shown by the device.</p>";
    if (!error.isEmpty()) {
        page += "<p class='error'>" + htmlEscape(error) + "</p>";
    }
    page += "<input name='password' type='password' required autocomplete='current-password'>"
            "<button type='submit'>Open console</button></form></body></html>";
    return page;
}

String consolePage()
{
    String page = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CardMind Console</title><style>
:root{color-scheme:dark}body{margin:0;font:15px system-ui;background:#07111e;color:#edf6ff}
header{position:sticky;top:0;background:#10233b;padding:12px 16px;display:flex;gap:10px;align-items:center}
header strong{flex:1;color:#67e8f9}.layout{max-width:900px;margin:auto;padding:14px;display:grid;gap:12px}
.card{background:#111f33;border:1px solid #29405e;border-radius:14px;padding:14px}
#messages{min-height:42vh;max-height:60vh;overflow:auto}.message{white-space:pre-wrap;padding:10px;border-radius:10px;margin:8px 0}
.user{background:#123b4a}.assistant{background:#25284a}.stream{color:#fde68a}
textarea,input,select,button{box-sizing:border-box;background:#081524;color:#fff;border:1px solid #405b7d;border-radius:8px;padding:10px}
textarea{width:100%;min-height:88px;resize:vertical}button{cursor:pointer;background:#5eead4;color:#06201c;border:0;font-weight:800}
.danger{background:#fb7185;color:#300}.file-editor{min-height:260px;font:14px ui-monospace,monospace}
.row{display:flex;gap:8px;flex-wrap:wrap}.row>*{flex:1}small{color:#9fb3ca}#status{color:#fde68a}</style></head><body>
<header><strong>CardMind</strong><span id="device"></span><button id="logout">Logout</button></header>
<main class="layout"><section class="card"><div class="row"><select id="chats"></select><button id="newChat">New chat</button></div>
<div class="row"><button id="renameChat">Rename</button><button class="danger" id="deleteChat">Delete</button></div>
<p id="status"></p><div id="messages"></div></section><section class="card"><textarea id="prompt" maxlength="1200" placeholder="Message CardMind..."></textarea>
<div class="row"><button id="send">Send</button><button class="danger" id="stop">Stop</button><button id="retry">Retry</button><button id="refresh">Refresh</button></div></section>
<section class="card"><label>Chat instructions</label><textarea id="instructions" maxlength="2048"></textarea><button id="saveInstructions">Save instructions</button></section>
<section class="card"><h2>Connection</h2><label>OpenAI-compatible base URL</label><input id="apiBaseUrl" maxlength="180">
<label>Model</label><input id="model" maxlength="120"><button id="saveSettings">Save non-secret settings</button><small>API keys remain write-only and are never sent to this page.</small></section>
<section class="card"><h2>Device diagnostics</h2><p id="diagnostics"></p></section>
<section class="card"><h2>microSD files</h2><div class="row"><select id="files"></select><button id="openFile">Open</button><button id="downloadFile">Download</button></div>
<div class="row"><input id="uploadInput" type="file" accept=".txt,.md,.json,.csv,.html,.svg"><button id="uploadFile">Upload new file</button></div>
<textarea class="file-editor" id="fileContent" maxlength="12288" placeholder="Select a workspace file"></textarea><p id="filePosition"></p>
<div class="row"><button id="previousFilePage">Previous</button><button id="nextFilePage">Next</button><button id="saveFile">Save chunk</button></div>
<div class="row"><button id="renameFile">Rename</button><button class="danger" id="deleteFile">Delete</button></div></section></main>
<script>)HTML";
    page += "const csrf='" + csrfToken + "';\n";
    page += R"HTML(
const q=s=>document.querySelector(s);let state=null,fileName='',fileOffset=0,fileNextOffset=0,fileOriginalBytes=0,fileEof=true,fileBack=[],activeRequest=null,lastPrompt='';
async function request(path,options={}){options.headers={...(options.headers||{}),'X-CardMind-CSRF':csrf};const r=await fetch(path,options);if(r.status===401){location='/';throw new Error('Session expired')}if(!r.ok){let message=`HTTP ${r.status}`;try{message=(await r.json()).error||message}catch{}throw new Error(message)}return r}
function render(s){state=s;q('#device').textContent=`${s.ip} · ${s.battery}%`;q('#status').textContent=s.status||'';
q('#chats').innerHTML='';for(const c of s.chats){const o=document.createElement('option');o.value=c.id;o.textContent=c.title;o.selected=c.id===s.active_chat_id;q('#chats').append(o)}
q('#messages').innerHTML='';for(const m of s.messages){const d=document.createElement('div');d.className='message '+m.role;d.textContent=(m.role==='user'?'You: ':'AI: ')+m.content;q('#messages').append(d)}
q('#instructions').value=s.instructions||'';q('#apiBaseUrl').value=s.api_base_url||'';q('#model').value=s.model||'';
q('#diagnostics').textContent=`Wi-Fi ${s.wifi_rssi} dBm · heap ${s.free_heap} B · largest ${s.largest_heap} B · SD ${s.sd_used_bytes}/${s.sd_total_bytes} B`;
const selected=q('#files').value;q('#files').innerHTML='';for(const f of s.files){const o=document.createElement('option');o.value=f.name;o.textContent=`${f.name} · ${f.size} B`;o.selected=f.name===selected;q('#files').append(o)}q('#messages').scrollTop=q('#messages').scrollHeight}
async function refresh(){const r=await request('/api/state');render(await r.json())}
async function post(path,values){return request(path,{method:'POST',body:new URLSearchParams(values)})}
async function openFile(offset,pushBack){const selected=q('#files').value;if(!selected)return;if(pushBack&&fileName===selected)fileBack.push(fileOffset);else if(fileName!==selected)fileBack=[];const r=await request(`/api/file?name=${encodeURIComponent(selected)}&offset=${offset}`);const f=await r.json();fileName=selected;fileOffset=f.offset;fileNextOffset=f.next_offset;fileOriginalBytes=f.next_offset-f.offset;fileEof=f.eof;q('#fileContent').value=f.content;q('#filePosition').textContent=`${fileOffset}-${fileNextOffset} / ${f.total_bytes} bytes`}
q('#refresh').onclick=refresh;q('#chats').onchange=async()=>{await request('/api/chat/select',{method:'POST',body:new URLSearchParams({id:q('#chats').value})});await refresh()};
q('#newChat').onclick=async()=>{await request('/api/chat/new',{method:'POST'});await refresh()};
q('#renameChat').onclick=async()=>{const title=prompt('Chat title',state.active_chat_title);if(title){await post('/api/chat/rename',{title});await refresh()}};
q('#deleteChat').onclick=async()=>{if(confirm(`Delete chat "${state.active_chat_title}"?`)){await post('/api/chat/delete',{});await refresh()}};
q('#saveInstructions').onclick=async()=>{await request('/api/chat/instructions',{method:'POST',body:new URLSearchParams({instructions:q('#instructions').value})});await refresh()};
q('#saveSettings').onclick=async()=>{await post('/api/settings',{api_base_url:q('#apiBaseUrl').value,model:q('#model').value});await refresh()};
q('#openFile').onclick=()=>openFile(0,false);q('#nextFilePage').onclick=()=>{if(!fileEof)openFile(fileNextOffset,true)};
q('#previousFilePage').onclick=()=>{if(fileBack.length)openFile(fileBack.pop(),false)};
q('#saveFile').onclick=async()=>{if(!fileName)return;await post('/api/file/save',{name:fileName,offset:String(fileOffset),original_bytes:String(fileOriginalBytes),content:q('#fileContent').value});await refresh();await openFile(fileOffset,false)};
q('#downloadFile').onclick=()=>{const name=q('#files').value;if(name)location=`/api/file/download?name=${encodeURIComponent(name)}`};
q('#uploadFile').onclick=async()=>{const file=q('#uploadInput').files[0];if(!file)return;const data=new FormData();data.append('file',file,file.name);await request('/api/file/upload',{method:'POST',body:data});q('#uploadInput').value='';await refresh()};
q('#renameFile').onclick=async()=>{const name=q('#files').value,newName=prompt('New filename',name);if(newName&&newName!==name){await post('/api/file/rename',{name,new_name:newName});fileName='';await refresh()}};
q('#deleteFile').onclick=async()=>{const name=q('#files').value;if(name&&confirm(`Delete ${name}?`)){await post('/api/file/delete',{name});fileName='';q('#fileContent').value='';q('#filePosition').textContent='';await refresh()}};
async function sendPrompt(){const prompt=q('#prompt').value.trim();if(!prompt)return;lastPrompt=prompt;q('#send').disabled=true;q('#status').textContent='Streaming...';activeRequest=new AbortController();
const d=document.createElement('div');d.className='message assistant stream';d.textContent='AI: ';q('#messages').append(d);
try{const r=await request('/api/prompt',{method:'POST',body:new URLSearchParams({prompt}),signal:activeRequest.signal});const reader=r.body.getReader(),decoder=new TextDecoder();let buffer='';
while(true){const x=await reader.read();if(x.done)break;buffer+=decoder.decode(x.value,{stream:true});const events=buffer.split('\n\n');buffer=events.pop();for(const e of events){if(!e.startsWith('data:'))continue;const v=JSON.parse(e.slice(5));if(v.delta)d.textContent+=v.delta;if(v.error)q('#status').textContent=v.error}}
q('#prompt').value=''}catch(e){if(e.name==='AbortError')q('#status').textContent='Canceled';else throw e}finally{activeRequest=null;q('#send').disabled=false;await refresh()}}
q('#send').onclick=sendPrompt;q('#stop').onclick=()=>{if(activeRequest)activeRequest.abort()};q('#retry').onclick=()=>{if(lastPrompt){q('#prompt').value=lastPrompt;sendPrompt()}};
q('#logout').onclick=async()=>{await request('/logout',{method:'POST'});location='/'};refresh();
</script></body></html>)HTML";
    return page;
}

void sendLoginPage()
{
    server.send(200, "text/html; charset=utf-8", loginPage(""));
}

void sendRoot()
{
    if (!sessionIsActive()) {
        sendLoginPage();
        return;
    }
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html; charset=utf-8", consolePage());
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
    document["instructions"] = activeChat.instructions;
    document["status"] = consoleStatus;
    document["model"] = consoleSettings.model;
    document["api_base_url"] = consoleSettings.apiBaseUrl;
    JsonArray chats = document["chats"].to<JsonArray>();
    for (const auto& chat : consoleChats) {
        JsonObject item = chats.add<JsonObject>();
        item["id"] = chat.id;
        item["title"] = chat.title;
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

ToolExecutionResult executeConsoleTool(const ToolCall& call)
{
    if (isWebSearchToolName(call.name)) {
        return executeWebSearchTool(consoleSettings, call);
    }
    if (isWebFetchToolName(call.name)) {
        return executeWebFetchTool(consoleSettings, call);
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
    activeChat.messages = trimmedHistory(pending);
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
        return M5Cardputer.Keyboard.keysState().esc || !server.client().connected();
    };
    markOperation(useWorkspace || useSearch ? "web_console_tools" : "web_console_chat");
    const ChatResult result = useWorkspace || useSearch
        ? streamChatCompletionWithTools(consoleSettings, activeChat.messages,
                                        activeChat.instructions, onText, executeConsoleTool,
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
    activeChat.messages = trimmedHistory(activeChat.messages);
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
            if (consoleSerialInput == "STATUS") {
                Serial.printf("WEB_CONSOLE status=ready authenticated=%s heap=%u\n",
                              sessionToken.isEmpty() ? "no" : "yes",
                              static_cast<unsigned int>(ESP.getFreeHeap()));
            } else if (consoleSerialInput == "EXIT") {
                exitRequested = true;
                Serial.println("WEB_CONSOLE exit=requested");
            } else if (!consoleSerialInput.isEmpty()) {
                Serial.println("ERROR event=web_console_serial reason=unsupported_command");
            }
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
    consoleSettings = settings;
    OperationResult result = loadSetupAccessPointPassword(accessPassword);
    if (!result.success || accessPassword.isEmpty()) {
        clearConsoleSecrets();
        return {false, initialChatId,
                result.success ? String("Installation password is missing") : result.error};
    }
    result = refreshChats();
    if (!result.success) {
        clearConsoleSecrets();
        return {false, initialChatId, result.error};
    }
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
        server.on("/api/chat/delete", HTTP_POST, handleDeleteChat);
        server.on("/api/settings", HTTP_POST, handleSettings);
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
    server.begin();
    Serial.printf("WEB_CONSOLE result=ready address=http://%s/\n",
                  WiFi.localIP().toString().c_str());
    showWebConsoleAccess("http://" + WiFi.localIP().toString(), accessPassword);
    bool escapeHeld = false;
    while (!exitRequested) {
        server.handleClient();
        updateConsoleSerial();
        M5Cardputer.update();
        const bool escapePressed = M5Cardputer.Keyboard.keysState().esc;
        if (escapePressed && !escapeHeld) {
            exitRequested = true;
        }
        escapeHeld = escapePressed;
        delay(2);
    }
    server.stop();
    clearConsoleSecrets();
    Serial.println("WEB_CONSOLE result=stopped");
    return {true, activeChat.summary.id, ""};
}

}  // namespace cardputer
