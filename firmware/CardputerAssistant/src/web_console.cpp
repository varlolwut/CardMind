#include "web_console.h"

#include <base64.h>

#include "api_client.h"
#include "chat_storage.h"
#include "crash_journal.h"
#include "file_workspace.h"
#include "python_mode.h"
#include "project_bundle.h"
#include "instruction_policy.h"
#include "project_chat_storage.h"
#include "project_storage.h"
#include "sd_storage.h"
#include "storage.h"
#include "ssh_client.h"
#include "ssh_tool.h"
#include "text_utils.h"
#include "tool_router.h"
#include "ui.h"
#include "web_console_routes.h"
#include "web_console_metrics.h"
#include "web_console_state.h"
#include "web_console_transport.h"
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
#include <cstring>
#include <ctime>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace cardputer {
namespace {

constexpr std::uint32_t kSessionIdleMs = 15U * 60U * 1000U;
constexpr std::uint32_t kLoginLockMs = 30U * 1000U;
constexpr std::size_t kMaximumLoginFailures = 5;
constexpr std::size_t kMaximumPromptBytes = 16384;
constexpr std::size_t kPromptFramePrefixBytes = 4;
constexpr std::size_t kMaximumPromptFrameBytes =
    kPromptFramePrefixBytes + kMaximumRequestInstructionsBytes + kMaximumPromptBytes;
constexpr const char* kPromptFrameContentType =
    "application/vnd.cardmind.prompt-v1";
constexpr std::size_t kMaximumWebFileChunkBytes = 12288;

struct RawTextRequestState {
    std::string body;
    String error;
    int errorStatus = 400;
    bool started = false;
    bool complete = false;
};

struct RawTextRequestResult {
    bool success;
    std::string body;
    int errorStatus;
    String error;
};

struct WebPromptRequest {
    bool success;
    std::string prompt;
    std::string requestInstructions;
    int errorStatus;
    String error;
};

struct WebContextSummaryResult {
    bool success;
    std::string summary;
    std::uint32_t includedMessages;
    String error;
};

struct DecodedHeaderResult {
    bool success;
    std::string value;
    int errorStatus;
    String error;
};

WebServer server(80);
Settings consoleSettings;
ChatDocument activeChat;
ProjectDocument activeProject;
std::vector<ProjectSummary> consoleProjects;
std::vector<ChatSummary> consoleChats;
std::vector<WorkspaceFile> consoleFiles;
std::vector<SshProfile> consoleSshProfiles;
std::size_t consoleSshSelected = 0;
bool consoleSshPrivateKeyInstalled = false;
bool filesIndexReady = false;
bool sshProfilesReady = false;
std::uint32_t chatsRevision = 0;
std::uint32_t chatRevision = 0;
std::uint32_t projectsRevision = 0;
std::uint32_t projectRevision = 0;
std::uint32_t chatsNextOffset = 0;
bool chatsPageEof = true;
std::uint32_t filesRevision = 0;
std::uint32_t sshRevision = 0;
std::uint32_t settingsRevision = 0;
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
RawTextRequestState rawTextRequest;
std::string failedWebRequestInstructions;
String failedWebRequestInstructionsChatId;
std::uint32_t failedWebRequestOutputTokens = 0;
File sshKeyUploadFile;
String sshKeyUploadError;
std::size_t sshKeyUploadBytes = 0;
constexpr const char* kSshKeyUploadPath = "/assistant/ssh/upload.tmp";
SshClient webSshClient;
SshProfile webSshProfile = {"", "", 22, "", "", SshAuthMode::Password, ""};
enum class WebSshStage : std::uint8_t {
    Idle,
    Connecting,
    AwaitingTrust,
    Authenticating,
    Opening,
    Connected,
    Stopping,
    Failed,
};
portMUX_TYPE webSshStateMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t webSshTask = nullptr;
WebSshStage webSshStage = WebSshStage::Idle;
bool webSshCancelRequested = false;
char webSshError[192] = {};
char webSshFingerprint[96] = {};
char webSshHostKeyType[32] = {};
bool webSshHostChanged = false;
std::uint32_t webSshConnectMs = 0;
std::uint32_t webSshAuthenticateMs = 0;
std::uint32_t webSshOpenMs = 0;
bool webSshAwaitingTrust = false;
bool webSshTerminalOpen = false;
bool consoleEscapeConsumed = false;
std::uint32_t passwordRevealUntil = 0;
String consoleQrPayload;
String firmwareVersion;
bool pythonRestartRequested = false;

const char* webSshStageName(WebSshStage stage)
{
    switch (stage) {
        case WebSshStage::Idle: return "idle";
        case WebSshStage::Connecting: return "connecting";
        case WebSshStage::AwaitingTrust: return "awaiting_trust";
        case WebSshStage::Authenticating: return "authenticating";
        case WebSshStage::Opening: return "opening";
        case WebSshStage::Connected: return "connected";
        case WebSshStage::Stopping: return "stopping";
        case WebSshStage::Failed: return "failed";
    }
    return "failed";
}

void publishWebSshStage(WebSshStage stage, const String& error)
{
    char copiedError[sizeof(webSshError)] = {};
    std::snprintf(copiedError, sizeof(copiedError), "%s", error.c_str());
    portENTER_CRITICAL(&webSshStateMux);
    webSshStage = stage;
    std::memcpy(webSshError, copiedError, sizeof(webSshError));
    portEXIT_CRITICAL(&webSshStateMux);
}

bool webSshTaskIsRunning()
{
    portENTER_CRITICAL(&webSshStateMux);
    const bool running = webSshTask != nullptr;
    portEXIT_CRITICAL(&webSshStateMux);
    return running;
}

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

void clearWebSshConnection()
{
    webSshClient.close();
    webSshAwaitingTrust = false;
    webSshTerminalOpen = false;
    webSshProfile.password = String();
    webSshProfile.privateKeyPassphrase = String();
}

void closeWebSshConnection()
{
    portENTER_CRITICAL(&webSshStateMux);
    const bool running = webSshTask != nullptr;
    if (running) {
        webSshCancelRequested = true;
        webSshStage = WebSshStage::Stopping;
    }
    portEXIT_CRITICAL(&webSshStateMux);
    if (!running) {
        clearWebSshConnection();
        publishWebSshStage(WebSshStage::Idle, "");
    }
}

String normalizedBaseUrl(String value)
{
    value.trim();
    while (value.endsWith("/")) {
        value.remove(value.length() - 1);
    }
    return value;
}

void releaseConsoleSessionState()
{
    closeWebSshConnection();
    webSshProfile = SshProfile{"", "", 22, "", "", SshAuthMode::Password, ""};
    consoleEscapeConsumed = false;
    passwordRevealUntil = 0;
    if (sshKeyUploadFile) {
        sshKeyUploadFile.close();
    }
    if (uploadFile) {
        uploadFile.close();
    }
    const bool storageWritable = requireSdWriteAccess(0, 0).success;
    if (storageWritable && uploadCreated && !uploadStorageName.isEmpty()) {
        SD.remove(workspaceFilePath(uploadStorageName));
    }
    if (storageWritable) {
        SD.remove(kSshKeyUploadPath);
    }
    sshKeyUploadError = String();
    sshKeyUploadBytes = 0;
    uploadName = String();
    uploadStorageName = String();
    uploadError = String();
    uploadBytes = 0;
    uploadCreated = false;
    uploadReplacing = false;
    accessPassword = String();
    sessionToken = String();
    csrfToken = String();
    consoleStatus = String();
    consoleSerialInput = String();
    consoleQrPayload = String();
    firmwareVersion = String();
    sessionLastActivityAt = 0;
    loginLockedUntil = 0;
    loginFailures = 0;
    exitRequested = false;
    pythonRestartRequested = false;
    consoleSettings = Settings{};
    activeChat = ChatDocument{};
    activeProject = ProjectDocument{};
    std::vector<ProjectSummary>().swap(consoleProjects);
    std::vector<ChatSummary>().swap(consoleChats);
    std::vector<WorkspaceFile>().swap(consoleFiles);
    std::vector<SshProfile>().swap(consoleSshProfiles);
    consoleSshSelected = 0;
    consoleSshPrivateKeyInstalled = false;
    filesIndexReady = false;
    sshProfilesReady = false;
    chatsRevision = 0;
    chatRevision = 0;
    projectsRevision = 0;
    projectRevision = 0;
    chatsNextOffset = 0;
    chatsPageEof = true;
    filesRevision = 0;
    sshRevision = 0;
    settingsRevision = 0;
    std::string().swap(activeResponse);
    std::string().swap(failedWebRequestInstructions);
    failedWebRequestInstructionsChatId = String();
    failedWebRequestOutputTokens = 0;
}

void failUpload(const String& error)
{
    if (uploadFile) {
        uploadFile.close();
    }
    if (requireSdWriteAccess(0, 0).success && uploadCreated &&
        !uploadStorageName.isEmpty()) {
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

void renderConsoleScreen();

bool sessionIsActive()
{
    if (sessionToken.isEmpty() ||
        static_cast<std::uint32_t>(millis() - sessionLastActivityAt) > kSessionIdleMs) {
        sessionToken = "";
        csrfToken = "";
        renderConsoleScreen();
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

enum class WebStorageAccess {
    None,
    Read,
    Write,
};

WebStorageAccess webStorageAccessForRoute(WebConsoleRouteHandler route)
{
    switch (route) {
        case WebConsoleRouteHandler::State: {
            const String path = server.uri();
            return path == "/api/projects" || path == "/api/chats" ||
                   path == "/api/chat" || path == "/api/files" ||
                   path == "/api/ssh/state"
                ? WebStorageAccess::Read
                : WebStorageAccess::None;
        }
        case WebConsoleRouteHandler::ProjectLinks:
        case WebConsoleRouteHandler::ArchivedMessages:
        case WebConsoleRouteHandler::SftpUpload:
        case WebConsoleRouteHandler::QrFile:
        case WebConsoleRouteHandler::FileRead:
        case WebConsoleRouteHandler::FileDownload:
            return WebStorageAccess::Read;
        case WebConsoleRouteHandler::SelectProject:
        case WebConsoleRouteHandler::NewProject:
        case WebConsoleRouteHandler::ProjectSettings:
        case WebConsoleRouteHandler::ProjectSettingsRawComplete:
        case WebConsoleRouteHandler::RenameProject:
        case WebConsoleRouteHandler::DuplicateProject:
        case WebConsoleRouteHandler::ArchiveProject:
        case WebConsoleRouteHandler::DeleteProject:
        case WebConsoleRouteHandler::ProjectLinkUpdate:
        case WebConsoleRouteHandler::Prompt:
        case WebConsoleRouteHandler::PromptRawComplete:
        case WebConsoleRouteHandler::PromptRetry:
        case WebConsoleRouteHandler::SelectChat:
        case WebConsoleRouteHandler::NewChat:
        case WebConsoleRouteHandler::Instructions:
        case WebConsoleRouteHandler::InstructionsRawComplete:
        case WebConsoleRouteHandler::ChatCompact:
        case WebConsoleRouteHandler::ChatPermissions:
        case WebConsoleRouteHandler::RenameChat:
        case WebConsoleRouteHandler::PinChat:
        case WebConsoleRouteHandler::ArchiveChat:
        case WebConsoleRouteHandler::DuplicateChat:
        case WebConsoleRouteHandler::ExportChat:
        case WebConsoleRouteHandler::ExportChatBundle:
        case WebConsoleRouteHandler::ImportChatBundle:
        case WebConsoleRouteHandler::DeleteChat:
        case WebConsoleRouteHandler::ClearChat:
        case WebConsoleRouteHandler::SshSettings:
        case WebConsoleRouteHandler::SshSelect:
        case WebConsoleRouteHandler::SshDelete:
        case WebConsoleRouteHandler::SshStart:
        case WebConsoleRouteHandler::SshTrust:
        case WebConsoleRouteHandler::SftpDownload:
        case WebConsoleRouteHandler::SshKeyComplete:
        case WebConsoleRouteHandler::FileSave:
        case WebConsoleRouteHandler::FileRename:
        case WebConsoleRouteHandler::FileDelete:
        case WebConsoleRouteHandler::FileUploadComplete:
            return WebStorageAccess::Write;
        default:
            return WebStorageAccess::None;
    }
}

int webStorageErrorStatus(SdStorageState state)
{
    switch (state) {
        case SdStorageState::Full: return 507;
        case SdStorageState::Replaced: return 409;
        case SdStorageState::Missing:
        case SdStorageState::Removed: return 503;
        case SdStorageState::Ready: return 500;
    }
    return 500;
}

bool allowWebStorageRoute(WebConsoleRouteHandler route)
{
    const WebStorageAccess access = webStorageAccessForRoute(route);
    if (access == WebStorageAccess::None || !sessionIsActive()) {
        return true;
    }
    if (access == WebStorageAccess::Write && !requestHasValidCsrf()) {
        return true;
    }
    const OperationResult result = access == WebStorageAccess::Read
        ? requireSdReadAccess()
        : requireSdWriteAccess(0, kStorageOperationalFloorBytes);
    if (result.success) {
        return true;
    }
    const SdStorageStatus storage = inspectSdStorage();
    sendWebJsonError(server, webStorageErrorStatus(storage.state), result.error);
    return false;
}

void releaseRawTextBody()
{
    std::string().swap(rawTextRequest.body);
}

void resetRawTextRequest()
{
    releaseRawTextBody();
    rawTextRequest.error = "";
    rawTextRequest.errorStatus = 400;
    rawTextRequest.started = false;
    rawTextRequest.complete = false;
}

void failRawTextRequest(int status, const String& error)
{
    if (rawTextRequest.error.isEmpty()) {
        rawTextRequest.errorStatus = status;
        rawTextRequest.error = error;
    }
}

bool requestHasRawTextContentType()
{
    String contentType = server.header("Content-Type");
    contentType.toLowerCase();
    return contentType == "text/plain" ||
           contentType.startsWith("text/plain;");
}

bool requestHasPromptFrameContentType()
{
    String contentType = server.header("Content-Type");
    contentType.toLowerCase();
    return contentType == kPromptFrameContentType;
}

void collectRawRequestBody(std::size_t maximumBytes)
{
    HTTPRaw& raw = server.raw();
    if (raw.status == RAW_START) {
        resetRawTextRequest();
        rawTextRequest.started = true;
        if (!requestHasValidCsrf()) {
            failRawTextRequest(401, "Authentication required");
            return;
        }
        if (!server.hasHeader("Content-Length") ||
            !server.header("Transfer-Encoding").isEmpty()) {
            failRawTextRequest(
                400, "Raw text requests require a fixed Content-Length");
            return;
        }
        const int contentLength = server.clientContentLength();
        if (contentLength < 0 ||
            static_cast<std::size_t>(contentLength) > maximumBytes) {
            failRawTextRequest(
                400, "Raw request exceeds its endpoint byte limit");
            return;
        }
        try {
            rawTextRequest.body.reserve(static_cast<std::size_t>(contentLength));
        } catch (const std::bad_alloc&) {
            failRawTextRequest(
                503, "Not enough contiguous memory to receive the raw text request");
        }
        return;
    }
    if (raw.status == RAW_ABORTED) {
        resetRawTextRequest();
        return;
    }
    if (!rawTextRequest.started || !rawTextRequest.error.isEmpty()) {
        return;
    }
    if (raw.status == RAW_WRITE) {
        if (raw.currentSize > maximumBytes - rawTextRequest.body.size()) {
            releaseRawTextBody();
            failRawTextRequest(
                400, "Raw request exceeds its endpoint byte limit");
            return;
        }
        try {
            rawTextRequest.body.append(
                reinterpret_cast<const char*>(raw.buf), raw.currentSize);
        } catch (const std::bad_alloc&) {
            releaseRawTextBody();
            failRawTextRequest(
                503, "Not enough contiguous memory to receive the raw text request");
        }
        return;
    }
    if (raw.status == RAW_END) {
        const int contentLength = server.clientContentLength();
        if (contentLength < 0 ||
            rawTextRequest.body.size() != static_cast<std::size_t>(contentLength) ||
            rawTextRequest.body.size() != raw.totalSize) {
            releaseRawTextBody();
            failRawTextRequest(
                400, "Raw text request ended before its declared byte length");
            return;
        }
        rawTextRequest.complete = true;
    }
}

void rejectRawRequestContentType(const String& error)
{
    if (!rawTextRequest.started) {
        resetRawTextRequest();
        rawTextRequest.started = true;
        failRawTextRequest(415, error);
    }
}

void collectRawTextRequest(std::size_t maximumBytes)
{
    if (!requestHasRawTextContentType()) {
        rejectRawRequestContentType(
            "Raw text endpoints require Content-Type text/plain");
        return;
    }
    collectRawRequestBody(maximumBytes);
}

void collectRawPromptRequest()
{
    if (requestHasRawTextContentType()) {
        collectRawRequestBody(kMaximumPromptBytes);
        return;
    }
    if (requestHasPromptFrameContentType()) {
        collectRawRequestBody(kMaximumPromptFrameBytes);
        return;
    }
    rejectRawRequestContentType(
        "Raw prompt endpoints require text/plain or application/vnd.cardmind.prompt-v1");
}

RawTextRequestResult consumeRawTextRequest()
{
    if (!rawTextRequest.started) {
        return {
            false, {}, 400,
            "Raw text request did not start correctly"};
    }
    if (!rawTextRequest.error.isEmpty()) {
        RawTextRequestResult result = {
            false, {}, rawTextRequest.errorStatus, rawTextRequest.error};
        resetRawTextRequest();
        return result;
    }
    if (!rawTextRequest.complete) {
        resetRawTextRequest();
        return {
            false, {}, 400,
            "Raw text request did not finish correctly"};
    }
    RawTextRequestResult result = {
        true, std::move(rawTextRequest.body), 200, ""};
    resetRawTextRequest();
    return result;
}

WebPromptRequest parseWebPromptRequest(RawTextRequestResult request,
                                       bool framed)
{
    if (!request.success) {
        return {
            false, {}, {}, request.errorStatus, request.error};
    }
    if (!framed) {
        return {
            true, std::move(request.body), {}, 200, ""};
    }
    if (request.body.size() < kPromptFramePrefixBytes) {
        return {
            false, {}, {}, 400,
            "Framed prompt is missing its four-byte instruction length"};
    }
    const auto* prefix = reinterpret_cast<const unsigned char*>(
        request.body.data());
    const std::uint32_t instructionBytes =
        (static_cast<std::uint32_t>(prefix[0]) << 24U) |
        (static_cast<std::uint32_t>(prefix[1]) << 16U) |
        (static_cast<std::uint32_t>(prefix[2]) << 8U) |
        static_cast<std::uint32_t>(prefix[3]);
    const std::size_t payloadBytes =
        request.body.size() - kPromptFramePrefixBytes;
    if (instructionBytes > kMaximumRequestInstructionsBytes) {
        return {
            false, {}, {}, 400,
            "Request instructions must not exceed 2048 bytes"};
    }
    if (instructionBytes > payloadBytes) {
        return {
            false, {}, {}, 400,
            "Framed prompt instruction length exceeds the received body"};
    }
    const std::size_t promptBytes = payloadBytes - instructionBytes;
    if (promptBytes == 0 || promptBytes > kMaximumPromptBytes) {
        return {
            false, {}, {}, 400,
            "Framed prompt must contain between 1 and 16384 bytes"};
    }
    std::string requestInstructions;
    try {
        requestInstructions = request.body.substr(
            kPromptFramePrefixBytes, instructionBytes);
    } catch (const std::bad_alloc&) {
        return {
            false, {}, {}, 503,
            "Not enough contiguous memory to parse request instructions"};
    }
    request.body.erase(0, kPromptFramePrefixBytes + instructionBytes);
    if (!isValidUtf8(requestInstructions) || !isValidUtf8(request.body)) {
        return {
            false, {}, {}, 400,
            "Prompt and request instructions must contain valid UTF-8"};
    }
    return {
        true, std::move(request.body), std::move(requestInstructions), 200, ""};
}

void clearFailedWebRequestInstructions()
{
    std::string().swap(failedWebRequestInstructions);
    failedWebRequestInstructionsChatId = "";
    failedWebRequestOutputTokens = 0;
}

std::uint8_t percentEncodedNibble(char value)
{
    if (value >= '0' && value <= '9') {
        return static_cast<std::uint8_t>(value - '0');
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<std::uint8_t>(value - 'A' + 10);
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<std::uint8_t>(value - 'a' + 10);
    }
    return UINT8_MAX;
}

DecodedHeaderResult decodePercentEncodedHeader(const String& header,
                                               std::size_t maximumBytes)
{
    if (!header.startsWith("v1:")) {
        return {false, {}, 400, "Encoded model header is missing or invalid"};
    }
    const std::string encoded = header.substring(3).c_str();
    if (encoded.size() > maximumBytes * 3U) {
        return {false, {}, 400, "Encoded model header exceeds its byte limit"};
    }
    std::string decoded;
    try {
        decoded.reserve(std::min(encoded.size(), maximumBytes));
        for (std::size_t index = 0; index < encoded.size(); ++index) {
            if (encoded[index] != '%') {
                decoded.push_back(encoded[index]);
                continue;
            }
            if (index + 2 >= encoded.size()) {
                return {false, {}, 400, "Encoded model header has an incomplete escape"};
            }
            const std::uint8_t high = percentEncodedNibble(encoded[index + 1]);
            const std::uint8_t low = percentEncodedNibble(encoded[index + 2]);
            if (high == UINT8_MAX || low == UINT8_MAX) {
                return {false, {}, 400, "Encoded model header has an invalid escape"};
            }
            decoded.push_back(static_cast<char>((high << 4U) | low));
            index += 2;
        }
    } catch (const std::bad_alloc&) {
        return {false, {}, 503, "Not enough memory to decode the model header"};
    }
    if (decoded.size() > maximumBytes || decoded.find('\0') != std::string::npos ||
        !isValidUtf8(decoded)) {
        return {false, {}, 400, "Decoded model header is not valid UTF-8"};
    }
    return {true, std::move(decoded), 200, ""};
}

void rejectLegacyRawTextRoute()
{
    const RawTextRequestResult request = consumeRawTextRequest();
    if (!request.success) {
        sendWebJsonError(server, request.errorStatus, request.error);
        return;
    }
    sendWebJsonError(
        server, 415,
        "Use the corresponding /raw endpoint with Content-Type text/plain");
}

std::uint64_t currentTimestamp()
{
    const std::time_t current = std::time(nullptr);
    return current >= 1700000000 ? static_cast<std::uint64_t>(current) : 0;
}

OperationResult refreshChats()
{
    const std::uint32_t startedAt = millis();
    const ProjectChatsPageResult result = listProjectChatsPage(
        activeProject.summary.id, 0, kMaximumProjectPageEntries);
    recordWebSdRead(millis() - startedAt);
    if (!result.success) {
        return {false, result.error};
    }
    consoleChats = result.chats;
    chatsNextOffset = result.nextOffset;
    chatsPageEof = result.eof;
    ++chatsRevision;
    return {true, ""};
}

OperationResult refreshProjects()
{
    const std::uint32_t startedAt = millis();
    const ProjectsPageResult result = listProjectsPage(0, kMaximumProjectPageEntries);
    recordWebSdRead(millis() - startedAt);
    if (!result.success) {
        return {false, result.error};
    }
    consoleProjects = result.projects;
    ++projectsRevision;
    return {true, ""};
}

OperationResult refreshFiles()
{
    const std::uint32_t startedAt = millis();
    const WorkspaceFilesPageResult result = listWorkspaceFilesPage(0, 64);
    recordWebSdRead(millis() - startedAt);
    if (!result.success) {
        return {false, result.error};
    }
    consoleFiles = result.files;
    filesIndexReady = true;
    ++filesRevision;
    return {true, ""};
}

OperationResult refreshSshProfiles()
{
    const std::uint32_t startedAt = millis();
    std::vector<SshProfile> profiles;
    std::size_t selected = 0;
    const OperationResult result = loadSshProfiles(profiles, selected);
    if (result.success) {
        consoleSshPrivateKeyInstalled = sshPrivateKeyIsInstalled();
    }
    recordWebSdRead(millis() - startedAt);
    if (!result.success) {
        return result;
    }
    for (SshProfile& profile : profiles) {
        profile.password = "";
        profile.privateKeyPassphrase = "";
    }
    consoleSshProfiles = std::move(profiles);
    consoleSshSelected = selected;
    sshProfilesReady = true;
    ++sshRevision;
    return {true, ""};
}

std::size_t activeProjectTailByteBudget()
{
    return std::min<std::size_t>(131072, activeProject.contextByteBudget);
}

OperationResult loadActiveChat(const String& id)
{
    const std::uint32_t startedAt = millis();
    const ChatDocumentResult loaded = loadProjectChat(
        activeProject.summary.id, id, 96, activeProjectTailByteBudget());
    recordWebSdRead(millis() - startedAt);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    activeChat = loaded.chat;
    activeResponse.clear();
    ++chatRevision;
    return {true, ""};
}

OperationResult saveActiveChat()
{
    const std::uint32_t startedAt = millis();
    const OperationResult result = saveProjectChatMetadata(activeChat);
    recordWebSdWrite(millis() - startedAt);
    if (result.success) {
        ++chatRevision;
    }
    return result;
}

OperationResult saveActiveChatSelection(const String& chatId)
{
    if (activeProject.activeChatId == chatId) {
        return {true, ""};
    }
    ProjectDocumentResult stored = loadProject(activeProject.summary.id);
    if (!stored.success) {
        return {false, stored.error};
    }
    stored.project.activeChatId = chatId;
    const OperationResult result = saveProject(stored.project);
    if (result.success) {
        activeProject = stored.project;
    }
    return result;
}

OperationResult selectActiveProject(const String& id)
{
    ProjectDocumentResult loaded = loadProject(id);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    ProjectStorageManifestResult manifest = loadProjectStorageManifest();
    if (!manifest.success) {
        return {false, manifest.error};
    }
    OperationResult result = {true, ""};
    if (manifest.manifest.activeProjectId != id) {
        manifest.manifest.activeProjectId = id;
        ++manifest.manifest.revision;
        result = saveProjectStorageManifest(manifest.manifest);
        if (!result.success) {
            return result;
        }
    }
    activeProject = loaded.project;
    ++projectRevision;
    result = refreshChats();
    if (!result.success) {
        return result;
    }
    if (!activeProject.activeChatId.isEmpty()) {
        result = loadActiveChat(activeProject.activeChatId);
    } else if (!consoleChats.empty()) {
        result = loadActiveChat(consoleChats.front().id);
    } else {
        const ChatDocumentResult created = createProjectChat(id, "New chat");
        if (!created.success) {
            return {false, created.error};
        }
        activeChat = created.chat;
        result = saveActiveChatSelection(created.chat.summary.id);
        if (result.success) {
            ++chatRevision;
            ++projectRevision;
            result = refreshChats();
        }
    }
    if (!result.success) {
        return result;
    }
    result = saveActiveChatSelection(activeChat.summary.id);
    if (result.success) {
        ++projectRevision;
    }
    return result;
}

OperationResult loadCommittedConsoleStorageReadOnly()
{
    const ProjectStorageManifestResult manifest = loadProjectStorageManifest();
    if (!manifest.success) {
        return {false, manifest.error};
    }
    if (manifest.manifest.activeProjectId.isEmpty()) {
        return {false, "Active project is missing"};
    }
    ProjectDocumentResult project = loadProject(manifest.manifest.activeProjectId);
    if (!project.success) {
        return {false, project.error};
    }
    activeProject = std::move(project.project);
    ++projectRevision;
    OperationResult result = refreshProjects();
    if (result.success) {
        result = refreshChats();
    }
    if (!result.success) {
        return result;
    }
    if (!activeProject.activeChatId.isEmpty()) {
        result = loadActiveChat(activeProject.activeChatId);
    } else {
        activeChat = ChatDocument{};
        activeResponse.clear();
        ++chatRevision;
    }
    if (result.success) {
        result = refreshFiles();
    }
    return result;
}

std::string effectiveProjectChatInstructions(const ProjectDocument& project,
                                             const ChatDocument& chat,
                                             const std::string& requestInstructions)
{
    return buildScopedInstructions(
        project.instructions, chat.instructions, requestInstructions,
        chat.contextSummary);
}

WebContextSummaryResult generateWebContextSummary(
    const std::string& previousSummary,
    const std::vector<Message>& messages)
{
    if (messages.empty()) {
        return {false, {}, 0, "No messages are available to summarize"};
    }
    constexpr std::size_t kCompactionHeapReserve = 16384;
    const std::size_t largestBlock =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (largestBlock <= kCompactionHeapReserve) {
        return {
            false, {}, 0,
            "Context compaction needs a larger contiguous heap block; close SSH and retry"};
    }
    ContextSummaryPromptResult prompt = buildContextSummaryPrompt(
        previousSummary, messages,
        std::min<std::size_t>(
            std::min<std::size_t>(activeProject.contextByteBudget, 32768),
            largestBlock - kCompactionHeapReserve));
    if (!prompt.success) {
        return {false, {}, 0, prompt.error.c_str()};
    }
    const std::uint32_t includedMessages = prompt.includedMessages;
    Settings summarySettings = consoleSettings;
    summarySettings.globalInstructions = "";
    if (!activeProject.model.isEmpty()) summarySettings.model = activeProject.model;
    std::vector<Message> request;
    request.push_back({"user", std::move(prompt.prompt)});
    ChatResult summary = streamChatCompletionWithBudget(
        summarySettings, request,
        "This is a context compaction operation, not a user-facing answer.",
        768, [](const std::string&) {}, []() {
            M5Cardputer.update();
            return consoleEscapePressed() || !server.client().connected();
        });
    if (!summary.success) {
        return {false, {}, 0, "Context summary failed: " + summary.error};
    }
    if (summary.response.empty() || !isValidUtf8(summary.response)) {
        return {false, {}, 0, "Context summary returned invalid UTF-8 or no content"};
    }
    return {true, std::move(summary.response), includedMessages, ""};
}

OperationResult regenerateWebContextSummary(const std::vector<Message>& omitted)
{
    if (omitted.empty()) {
        return {true, ""};
    }
    ChatDocumentResult stored = loadProjectChatMetadata(
        activeProject.summary.id, activeChat.summary.id);
    if (!stored.success) {
        return {false, stored.error};
    }
    WebContextSummaryResult generated = generateWebContextSummary(
        stored.chat.contextSummary, omitted);
    if (!generated.success) {
        return {false, generated.error};
    }
    stored = loadProjectChatMetadata(activeProject.summary.id, activeChat.summary.id);
    if (!stored.success) {
        return {false, stored.error};
    }
    stored.chat.contextSummary = std::move(generated.summary);
    stored.chat.summarizedMessageCount = std::min(
        stored.chat.summary.messageCount,
        stored.chat.summarizedMessageCount + generated.includedMessages);
    return saveProjectChatMetadata(stored.chat);
}

bool consolePasswordVisible()
{
    return static_cast<std::int32_t>(passwordRevealUntil - millis()) > 0;
}

void renderConsoleScreen()
{
    if (!consoleQrPayload.isEmpty()) {
        return;
    }
    showWebConsoleAccess("http://" + WiFi.localIP().toString(), accessPassword,
                         !sessionToken.isEmpty(), consolePasswordVisible());
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
    sendWebConsolePage(server);
}

void handleSession()
{
    if (!sessionIsActive()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["csrf"] = csrfToken;
    sendWebJson(server, 200, document);
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
    const bool existingSession = !sessionToken.isEmpty() &&
        static_cast<std::uint32_t>(millis() - sessionLastActivityAt) <= kSessionIdleMs;
    if (!existingSession) {
        sessionToken = randomHexToken();
        csrfToken = randomHexToken();
    }
    sessionLastActivityAt = millis();
    server.sendHeader("Set-Cookie", "cm_session=" + sessionToken +
                      "; HttpOnly; SameSite=Strict; Path=/; Max-Age=900");
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Authenticated");
    passwordRevealUntil = 0;
    renderConsoleScreen();
}

void handleLogout()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    clearFailedWebRequestInstructions();
    sessionToken = "";
    csrfToken = "";
    server.sendHeader("Set-Cookie", "cm_session=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0");
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
    renderConsoleScreen();
}

void handleCloseConsole()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["message"] = "Web Console is closing";
    sendWebJson(server, 200, document);
    clearFailedWebRequestInstructions();
    exitRequested = true;
}

void handleStorageConfirm()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const SdStorageStatus before = inspectSdStorage();
    if (before.state != SdStorageState::Replaced) {
        sendWebJsonError(server, 409,
                         "microSD replacement confirmation is not currently required");
        return;
    }
    const OperationResult result = confirmSdStorageReplacement();
    if (!result.success) {
        const SdStorageStatus storage = inspectSdStorage();
        sendWebJsonError(server, webStorageErrorStatus(storage.state), result.error);
        return;
    }
    clearFailedWebRequestInstructions();
    JsonDocument document;
    document["ok"] = true;
    document["message"] =
        "microSD replacement confirmed; restart CardMind to initialize the workspace";
    sendWebJson(server, 200, document);
    exitRequested = true;
}

void handleState()
{
    if (!sessionIsActive()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    JsonDocument document;
    const WebConsoleRuntimeState runtime = {
        consoleStatus,
        firmwareVersion,
        webSshTerminalOpen && webSshClient.isOpen(),
    };
    String view = server.arg("view");
    const String path = server.uri();
    if (path == "/api/status") view = "status";
    else if (path == "/api/projects") view = "projects";
    else if (path == "/api/chats") view = "chats";
    else if (path == "/api/chat") view = "chat";
    else if (path == "/api/files") view = "files";
    else if (path == "/api/ssh/state") view = "ssh";
    else if (path == "/api/settings") view = "settings";
    if (view == "status") {
        buildWebConsoleStatusState(runtime, webDiagnosticsEnabled(), document);
    } else if (view == "projects") {
        std::uint32_t offset = 0;
        if (!server.arg("offset").isEmpty() &&
            !parseUnsignedArgument(server.arg("offset"), offset)) {
            sendWebJsonError(server, 400, "Project offset must be an unsigned integer");
            return;
        }
        const ProjectsPageResult page = listProjectsPage(
            offset, kMaximumProjectPageEntries);
        if (!page.success) {
            sendWebJsonError(server, 500, page.error);
            return;
        }
        document["ok"] = true;
        document["projects_revision"] = projectsRevision;
        document["active_project_id"] = activeProject.summary.id;
        document["next_offset"] = page.nextOffset;
        document["eof"] = page.eof;
        JsonArray projects = document["projects"].to<JsonArray>();
        for (const ProjectSummary& project : page.projects) {
            JsonObject item = projects.add<JsonObject>();
            item["id"] = project.id;
            item["title"] = project.title;
            item["chat_count"] = project.chatCount;
            item["archived"] = project.archived;
        }
    } else if (view == "chats") {
        std::uint32_t offset = 0;
        if (!server.arg("offset").isEmpty() &&
            !parseUnsignedArgument(server.arg("offset"), offset)) {
            sendWebJsonError(server, 400, "Chat offset must be an unsigned integer");
            return;
        }
        const ProjectChatsPageResult page = listProjectChatsPage(
            activeProject.summary.id, offset, kMaximumProjectPageEntries);
        if (!page.success) {
            sendWebJsonError(server, 500, page.error);
            return;
        }
        buildWebConsoleChatsState(page.chats, chatsRevision, document);
        document["project_id"] = activeProject.summary.id;
        document["next_offset"] = page.nextOffset;
        document["eof"] = page.eof;
    } else if (view == "chat") {
        Settings requestSettings = consoleSettings;
        if (!activeProject.model.isEmpty()) requestSettings.model = activeProject.model;
        buildWebConsoleChatState(
            requestSettings, activeChat, activeProject.contextByteBudget,
            chatRevision, document);
        document["project_id"] = activeProject.summary.id;
        document["project_title"] = activeProject.summary.title;
        document["project_archived"] = activeProject.summary.archived;
        document["project_model"] = activeProject.model;
        document["project_instructions"] = activeProject.instructions;
        document["context_byte_budget"] = activeProject.contextByteBudget;
        document["maximum_output_tokens"] = activeProject.maximumOutputTokens;
        document["automatic_compaction"] = activeProject.automaticCompaction;
        document["context_summary"] = activeChat.contextSummary;
        document["summarized_messages"] = activeChat.summarizedMessageCount;
        document["total_messages"] = activeChat.summary.messageCount;
        const std::uint32_t displayedMessages =
            document["messages"].as<JsonArrayConst>().size();
        const std::uint32_t historyBeforeMessages =
            activeChat.summary.messageCount > displayedMessages
            ? activeChat.summary.messageCount - displayedMessages
            : 0;
        document["history_before_offset"] = historyBeforeMessages;
        document["history_before_messages"] = historyBeforeMessages;
    } else if (view == "files") {
        std::uint32_t offset = 0;
        if (!server.arg("offset").isEmpty() &&
            !parseUnsignedArgument(server.arg("offset"), offset)) {
            sendWebJsonError(server, 400, "File offset must be an unsigned integer");
            return;
        }
        const WorkspaceFilesPageResult page = listWorkspaceFilesPage(offset, 64);
        if (!page.success) {
            sendWebJsonError(server, 500, page.error);
            return;
        }
        buildWebConsoleFilesState(page.files, SD.totalBytes(), SD.usedBytes(),
                                  filesRevision, document);
        document["next_offset"] = page.nextOffset;
        document["eof"] = page.eof;
    } else if (view == "ssh") {
        if (!sshProfilesReady) {
            const OperationResult refreshed = refreshSshProfiles();
            if (!refreshed.success) {
                sendWebJsonError(server, 500, refreshed.error);
                return;
            }
        }
        buildWebConsoleSshState(consoleSshProfiles, consoleSshSelected,
                                consoleSshPrivateKeyInstalled, runtime,
                                sshRevision, document);
        char error[sizeof(webSshError)] = {};
        char fingerprint[sizeof(webSshFingerprint)] = {};
        char keyType[sizeof(webSshHostKeyType)] = {};
        WebSshStage stage = WebSshStage::Idle;
        bool hostChanged = false;
        std::uint32_t connectMs = 0;
        std::uint32_t authenticateMs = 0;
        std::uint32_t openMs = 0;
        portENTER_CRITICAL(&webSshStateMux);
        stage = webSshStage;
        std::memcpy(error, webSshError, sizeof(error));
        std::memcpy(fingerprint, webSshFingerprint, sizeof(fingerprint));
        std::memcpy(keyType, webSshHostKeyType, sizeof(keyType));
        hostChanged = webSshHostChanged;
        connectMs = webSshConnectMs;
        authenticateMs = webSshAuthenticateMs;
        openMs = webSshOpenMs;
        portEXIT_CRITICAL(&webSshStateMux);
        document["ssh_stage"] = webSshStageName(stage);
        document["ssh_error"] = error;
        document["ssh_fingerprint"] = fingerprint;
        document["ssh_key_type"] = keyType;
        document["ssh_host_changed"] = hostChanged;
        document["ssh_connect_ms"] = connectMs;
        document["ssh_authenticate_ms"] = authenticateMs;
        document["ssh_open_ms"] = openMs;
    } else if (view == "settings") {
        buildWebConsoleSettingsState(consoleSettings, runtime, settingsRevision,
                                     document);
    } else {
        sendWebJsonError(server, 400, "Use a specialized state endpoint");
        return;
    }
    sendWebJson(server, 200, document);
}

void streamStoredWebPrompt(const ChatDocument& storedChat,
                           std::vector<Message> requestMessages,
                           std::uint32_t requestOutputTokens,
                           const std::string& requestInstructions)
{
    const ResolvedProjectRequestPolicy requestPolicy = resolveProjectRequestPolicy(
        consoleSettings, activeProject, requestOutputTokens);
    ContextWindowResult requestFit = fitOwnedMessagesToByteBudget(
        std::move(requestMessages), requestPolicy.contextByteBudget);
    if (requestFit.retained.empty() ||
        requestFit.retained.back().role != "user") {
        sendWebJsonError(server, 500, "Prompt request context lost its user message");
        return;
    }
    const std::string& prompt = requestFit.retained.back().content;
    const bool sshWasOpen = webSshTerminalOpen || webSshAwaitingTrust;
    if (sshWasOpen) {
        closeWebSshConnection();
    }
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/event-stream; charset=utf-8", "");
    if (sshWasOpen) {
        sendWebSse(server, "notice", "",
                   "SSH terminal was disconnected to free memory for the AI request");
    }
    activeResponse.clear();
    consoleStatus = "Streaming from web console...";
    renderConsoleScreen();
    const ChatTextCallback onText = [](const std::string& text) {
        activeResponse += text;
        sendWebSse(server, "delta", text, "");
    };
    Settings requestSettings = consoleSettings;
    requestSettings.model = requestPolicy.model;
    const std::string effectiveInstructions = effectiveProjectChatInstructions(
        activeProject, storedChat, requestInstructions);
    failedWebRequestInstructions = requestInstructions;
    failedWebRequestInstructionsChatId = activeChat.summary.id;
    failedWebRequestOutputTokens = requestPolicy.maximumOutputTokens;
    const ChatToolPolicy toolPolicy = resolveChatToolPolicy(
        consoleSettings, prompt, storedChat.sshToolsEnabled, sshToolIsAvailable());
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
        ? streamChatCompletionWithToolsAndBudget(
              requestSettings, requestFit.retained, effectiveInstructions,
              toolPolicy.sshEnabled, requestPolicy.maximumOutputTokens, onText,
              [&toolPolicy, &isCancelled](const ToolCall& call) {
                  return routeProjectToolCall(
                      consoleSettings, toolPolicy, activeProject.summary.id, call,
                      isCancelled);
              }, isCancelled)
        : streamChatCompletionWithBudget(
              requestSettings, requestFit.retained, effectiveInstructions,
              requestPolicy.maximumOutputTokens, onText, isCancelled);
    markOperation("idle");
    if (!result.success) {
        consoleStatus = result.error;
        activeResponse = result.response;
        const OperationResult reloaded = loadActiveChat(activeChat.summary.id);
        if (reloaded.success) {
            refreshChats();
        }
        sendWebSse(server, "error", "", result.error);
        renderConsoleScreen();
        return;
    }
    clearFailedWebRequestInstructions();
    std::vector<Message> finalMessages = std::move(requestFit.retained);
    finalMessages.push_back({"assistant", result.response});
    std::vector<Message> omitted = takeMessagesDroppedToByteBudget(
        std::move(finalMessages), requestPolicy.contextByteBudget);
    OperationResult saved = appendProjectChatMessages(
        activeProject.summary.id, activeChat.summary.id,
        {{"assistant", result.response}}, currentTimestamp(),
        consoleSettings.projectChatHistoryQuotaBytes);
    if (saved.success && shouldAutomaticallyCompactRequest(
            requestPolicy, static_cast<std::uint32_t>(omitted.size()))) {
        const OperationResult compacted = regenerateWebContextSummary(omitted);
        if (!compacted.success) {
            consoleStatus = compacted.error;
            sendWebSse(server, "notice", "", compacted.error);
        } else {
            sendWebSse(
                server, "notice", "",
                "Context summary updated automatically; raw history was preserved");
        }
    }
    if (saved.success) {
        saved = loadActiveChat(activeChat.summary.id);
    }
    activeResponse.clear();
    consoleStatus = saved.success ? String("Saved") : saved.error;
    if (saved.success) {
        saved = refreshChats();
    }
    if (saved.success) {
        sendWebSse(server, "done", "", "");
    } else {
        sendWebSse(server, "error", "", saved.error);
    }
    renderConsoleScreen();
}

struct RequestOutputBudgetResult {
    bool success;
    std::uint32_t tokens;
    String error;
};

RequestOutputBudgetResult resolveWebRequestOutputBudget(
    const String& value, std::uint32_t projectTokens)
{
    std::uint32_t requestedTokens = projectTokens;
    if (!value.isEmpty() &&
        (!parseUnsignedArgument(value, requestedTokens) ||
         requestedTokens < 128 || requestedTokens > 16384)) {
        return {
            false, 0,
            "Request output tokens must be an integer between 128 and 16384"};
    }
    return {
        true,
        resolveRequestOutputTokens(
            projectTokens, value.isEmpty() ? 0 : requestedTokens),
        ""};
}

void handlePrompt()
{
    rejectLegacyRawTextRoute();
}

RequestOutputBudgetResult resolveRawWebRequestOutputBudget(
    const String& value, std::uint32_t projectTokens)
{
    std::uint32_t requestedTokens = 0;
    if (!parseUnsignedArgument(value, requestedTokens) ||
        (requestedTokens != 0 &&
         (requestedTokens < 128 || requestedTokens > 16384))) {
        return {
            false, 0,
            "Raw prompt output tokens must be 0 or an integer between 128 and 16384"};
    }
    return {
        true,
        resolveRequestOutputTokens(projectTokens, requestedTokens),
        ""};
}

void processWebPrompt(std::string prompt,
                      std::string requestInstructions,
                      const String& outputTokenValue)
{
    if (webSshTaskIsRunning()) {
        sendWebJsonError(
            server, 409,
            "Wait for the background SSH connection attempt to finish or stop it");
        return;
    }
    if (prompt.empty() || prompt.size() > kMaximumPromptBytes || !isValidUtf8(prompt)) {
        sendWebJsonError(server, 400, "Prompt must be valid UTF-8 between 1 and 16384 bytes");
        return;
    }
    const RequestOutputBudgetResult outputBudget = resolveRawWebRequestOutputBudget(
        outputTokenValue, activeProject.maximumOutputTokens);
    if (!outputBudget.success) {
        sendWebJsonError(server, 400, outputBudget.error);
        return;
    }
    ChatDocumentResult storedChat = loadProjectChat(
        activeProject.summary.id, activeChat.summary.id, 96,
        activeProjectTailByteBudget());
    if (!storedChat.success) {
        sendWebJsonError(server, 500, storedChat.error);
        return;
    }
    std::vector<Message> submittedMessages;
    submittedMessages.reserve(1);
    submittedMessages.push_back({"user", std::move(prompt)});
    std::vector<Message> requestMessages = unsummarizedChatTail(storedChat.chat);
    const std::uint32_t previousMessageCount =
        storedChat.chat.summary.messageCount;
    storedChat.chat = ChatDocument{};
    OperationResult saved = appendProjectChatMessages(
        activeProject.summary.id, activeChat.summary.id,
        submittedMessages, currentTimestamp(),
        consoleSettings.projectChatHistoryQuotaBytes);
    if (!saved.success) {
        sendWebJsonError(server, 500, saved.error);
        return;
    }
    const ResolvedProjectRequestPolicy submittedPolicy = resolveProjectRequestPolicy(
        consoleSettings, activeProject, outputBudget.tokens);
    failedWebRequestInstructions = std::move(requestInstructions);
    failedWebRequestInstructionsChatId = activeChat.summary.id;
    failedWebRequestOutputTokens = submittedPolicy.maximumOutputTokens;
    const std::string& submittedPrompt = submittedMessages.front().content;
    if (previousMessageCount == 0) {
        activeChat.summary.messageCount = 1;
        activeChat.summary.updatedAt = currentTimestamp();
        activeChat.summary.title = makeChatTitle(
            submittedPrompt, kMaximumChatTitleCells).c_str();
        saved = saveProjectChatMetadata(activeChat);
        if (!saved.success) {
            loadActiveChat(activeChat.summary.id);
            sendWebJsonError(server, 500, saved.error);
            return;
        }
    }
    requestMessages.reserve(requestMessages.size() + 1);
    requestMessages.push_back(std::move(submittedMessages.front()));
    streamStoredWebPrompt(
        activeChat, std::move(requestMessages), outputBudget.tokens,
        failedWebRequestInstructions);
}

void handlePromptRawData()
{
    collectRawPromptRequest();
}

void handlePromptRawComplete()
{
    WebPromptRequest request = parseWebPromptRequest(
        consumeRawTextRequest(), requestHasPromptFrameContentType());
    if (!request.success) {
        sendWebJsonError(server, request.errorStatus, request.error);
        return;
    }
    processWebPrompt(
        std::move(request.prompt), std::move(request.requestInstructions),
        server.header("X-CardMind-Output-Tokens"));
}

void handlePromptRetry()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    if (webSshTaskIsRunning()) {
        sendWebJsonError(
            server, 409,
            "Wait for the background SSH connection attempt to finish or stop it");
        return;
    }
    const String requestedOutputTokens = server.arg("maximum_output_tokens");
    const bool retryMatchesFailedRequest =
        failedWebRequestInstructionsChatId == activeChat.summary.id;
    RequestOutputBudgetResult outputBudget = resolveWebRequestOutputBudget(
        requestedOutputTokens, activeProject.maximumOutputTokens);
    if (requestedOutputTokens.isEmpty() && retryMatchesFailedRequest &&
        failedWebRequestOutputTokens != 0) {
        outputBudget = {true, failedWebRequestOutputTokens, ""};
    }
    if (!outputBudget.success) {
        sendWebJsonError(server, 400, outputBudget.error);
        return;
    }
    const ChatDocumentResult stored = loadProjectChat(
        activeProject.summary.id, activeChat.summary.id, 96,
        activeProjectTailByteBudget());
    if (!stored.success) {
        sendWebJsonError(server, 500, stored.error);
        return;
    }
    const ResolvedProjectRequestPolicy requestPolicy = resolveProjectRequestPolicy(
        consoleSettings, activeProject, outputBudget.tokens);
    RetryRequestResult retry = prepareRetryRequest(
        unsummarizedChatTail(stored.chat), requestPolicy.contextByteBudget);
    if (!retry.success) {
        sendWebJsonError(server, 409, retry.error.c_str());
        return;
    }
    const std::string emptyRequestInstructions;
    const std::string& requestInstructions = retryMatchesFailedRequest
        ? failedWebRequestInstructions : emptyRequestInstructions;
    streamStoredWebPrompt(
        stored.chat, std::move(retry.messages), outputBudget.tokens,
        requestInstructions);
}

void handleSelectProject()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    OperationResult result = selectActiveProject(server.arg("id"));
    if (result.success) result = refreshProjects();
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    clearFailedWebRequestInstructions();
    consoleStatus = "Project selected";
    JsonDocument document;
    document["ok"] = true;
    document["project_id"] = activeProject.summary.id;
    sendWebJson(server, 200, document);
}

void handleNewProject()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    std::string title = server.arg("title").c_str();
    if (title.empty()) title = "New project";
    if (title.size() > kMaximumProjectTitleBytes || !isValidUtf8(title)) {
        sendWebJsonError(server, 400,
                         "Project title must be valid UTF-8 up to 120 bytes");
        return;
    }
    const ProjectDocumentResult created = createProject(title.c_str());
    if (!created.success) {
        sendWebJsonError(server, 500, created.error);
        return;
    }
    OperationResult result = selectActiveProject(created.project.summary.id);
    if (result.success) result = refreshProjects();
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    clearFailedWebRequestInstructions();
    consoleStatus = "Project created";
    JsonDocument document;
    document["ok"] = true;
    document["project_id"] = activeProject.summary.id;
    sendWebJson(server, 200, document);
}

void handleProjectSettings()
{
    rejectLegacyRawTextRoute();
}

void handleProjectSettingsRawData()
{
    collectRawTextRequest(kMaximumProjectInstructionsBytes);
}

void handleProjectSettingsRawComplete()
{
    RawTextRequestResult request = consumeRawTextRequest();
    if (!request.success) {
        sendWebJsonError(server, request.errorStatus, request.error);
        return;
    }
    const DecodedHeaderResult decodedModel = decodePercentEncodedHeader(
        server.header("X-CardMind-Model-Encoded"), 120);
    if (!decodedModel.success) {
        sendWebJsonError(server, decodedModel.errorStatus, decodedModel.error);
        return;
    }
    String model = decodedModel.value.c_str();
    model.trim();
    std::uint32_t contextBytes = 0;
    std::uint32_t outputTokens = 0;
    const String automaticCompaction = server.header("X-CardMind-Auto-Compact");
    if (request.body.size() > kMaximumProjectInstructionsBytes ||
        !isValidUtf8(request.body) || model.length() > 120 ||
        !parseUnsignedArgument(server.header("X-CardMind-Context-Bytes"), contextBytes) ||
        contextBytes < 8192 || contextBytes > 262144 ||
        !parseUnsignedArgument(server.header("X-CardMind-Output-Tokens"), outputTokens) ||
        outputTokens < 128 || outputTokens > 8192 ||
        (automaticCompaction != "0" && automaticCompaction != "1")) {
        sendWebJsonError(server, 400,
                         "Project settings contain invalid instructions, model or budgets");
        return;
    }
    std::string previousInstructions = std::move(activeProject.instructions);
    const String previousModel = activeProject.model;
    const std::uint32_t previousContextBytes = activeProject.contextByteBudget;
    const std::uint32_t previousOutputTokens = activeProject.maximumOutputTokens;
    const bool previousAutomaticCompaction = activeProject.automaticCompaction;
    activeProject.instructions = std::move(request.body);
    activeProject.model = model;
    activeProject.contextByteBudget = contextBytes;
    activeProject.maximumOutputTokens = outputTokens;
    activeProject.automaticCompaction = automaticCompaction == "1";
    OperationResult result = saveProject(activeProject);
    if (!result.success) {
        activeProject.instructions = std::move(previousInstructions);
        activeProject.model = previousModel;
        activeProject.contextByteBudget = previousContextBytes;
        activeProject.maximumOutputTokens = previousOutputTokens;
        activeProject.automaticCompaction = previousAutomaticCompaction;
        sendWebJsonError(server, 500, result.error);
        return;
    }
    result = refreshProjects();
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    ++projectRevision;
    consoleStatus = "Project settings saved";
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleRenameProject()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    OperationResult result = renameProject(
        activeProject.summary.id, server.arg("title"));
    if (result.success) {
        const ProjectDocumentResult loaded = loadProject(activeProject.summary.id);
        result = loaded.success ? OperationResult{true, ""}
                                : OperationResult{false, loaded.error};
        if (loaded.success) activeProject = loaded.project;
    }
    if (result.success) result = refreshProjects();
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    ++projectRevision;
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleDuplicateProject()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    String title = server.arg("title");
    if (title.isEmpty()) title = activeProject.summary.title + " copy";
    const ProjectDocumentResult duplicated = duplicateProject(
        activeProject.summary.id, title);
    if (!duplicated.success) {
        sendWebJsonError(server, 500, duplicated.error);
        return;
    }
    OperationResult result = selectActiveProject(duplicated.project.summary.id);
    if (result.success) result = refreshProjects();
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["project_id"] = activeProject.summary.id;
    sendWebJson(server, 200, document);
}

void handleArchiveProject()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const bool archived = server.arg("archived") == "1";
    OperationResult result = setProjectArchived(activeProject.summary.id, archived);
    if (result.success) {
        const ProjectDocumentResult loaded = loadProject(activeProject.summary.id);
        result = loaded.success ? OperationResult{true, ""}
                                : OperationResult{false, loaded.error};
        if (loaded.success) activeProject = loaded.project;
    }
    if (result.success) result = refreshProjects();
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    ++projectRevision;
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleDeleteProject()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String deletedId = activeProject.summary.id;
    OperationResult result = deleteProject(deletedId);
    if (result.success) result = refreshProjects();
    if (result.success && consoleProjects.empty()) {
        const ProjectDocumentResult created = createProject("Default");
        result = created.success ? OperationResult{true, ""}
                                 : OperationResult{false, created.error};
        if (created.success) result = refreshProjects();
    }
    if (result.success) result = selectActiveProject(consoleProjects.front().id);
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["project_id"] = activeProject.summary.id;
    sendWebJson(server, 200, document);
}

void handleProjectLinks()
{
    if (!sessionIsActive()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    std::uint32_t offset = 0;
    if (!server.arg("offset").isEmpty() &&
        !parseUnsignedArgument(server.arg("offset"), offset)) {
        sendWebJsonError(server, 400, "Link offset must be an unsigned integer");
        return;
    }
    const SharedFileLinksPageResult page = listProjectSharedLinksPage(
        activeProject.summary.id, offset, kMaximumProjectPageEntries);
    if (!page.success) {
        sendWebJsonError(server, 500, page.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["next_offset"] = page.nextOffset;
    document["eof"] = page.eof;
    JsonArray links = document["links"].to<JsonArray>();
    for (const SharedFileLink& link : page.links) links.add(link.path);
    sendWebJson(server, 200, document);
}

void handleProjectLinkUpdate()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String path = server.arg("path");
    const bool linked = server.arg("linked") == "1";
    const OperationResult result = linked
        ? linkSharedFileToProject(activeProject.summary.id, path)
        : unlinkSharedFileFromProject(activeProject.summary.id, path);
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleSelectChat()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String id = server.arg("id");
    const OperationResult result = loadActiveChat(id);
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    const OperationResult selected = saveActiveChatSelection(id);
    if (!selected.success) {
        sendWebJsonError(server, 500, selected.error);
        return;
    }
    clearFailedWebRequestInstructions();
    ++projectRevision;
    consoleStatus = "Chat selected";
    renderConsoleScreen();
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleNewChat()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const std::uint32_t startedAt = millis();
    const ChatDocumentResult created = createProjectChat(
        activeProject.summary.id, "New chat");
    recordWebSdWrite(millis() - startedAt);
    if (!created.success) {
        sendWebJsonError(server, 400, created.error);
        return;
    }
    activeChat = created.chat;
    OperationResult result = saveActiveChatSelection(created.chat.summary.id);
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    ++projectRevision;
    ++chatRevision;
    result = refreshChats();
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    clearFailedWebRequestInstructions();
    consoleStatus = "New chat created";
    renderConsoleScreen();
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleInstructions()
{
    rejectLegacyRawTextRoute();
}

void handleInstructionsRawData()
{
    collectRawTextRequest(kMaximumProjectChatInstructionsBytes);
}

void handleInstructionsRawComplete()
{
    RawTextRequestResult request = consumeRawTextRequest();
    if (!request.success) {
        sendWebJsonError(server, request.errorStatus, request.error);
        return;
    }
    if (request.body.size() > kMaximumProjectChatInstructionsBytes ||
        !isValidUtf8(request.body)) {
        sendWebJsonError(server, 400, "Instructions must be valid UTF-8 up to 16384 bytes");
        return;
    }
    std::string previousInstructions = std::move(activeChat.instructions);
    const std::uint64_t previousUpdatedAt = activeChat.summary.updatedAt;
    activeChat.instructions = std::move(request.body);
    activeChat.summary.updatedAt = currentTimestamp();
    const OperationResult saved = saveActiveChat();
    if (!saved.success) {
        activeChat.instructions = std::move(previousInstructions);
        activeChat.summary.updatedAt = previousUpdatedAt;
        sendWebJsonError(server, 500, saved.error);
        return;
    }
    const OperationResult refreshed = refreshChats();
    if (!refreshed.success) {
        sendWebJsonError(server, 500, refreshed.error);
        return;
    }
    consoleStatus = activeChat.instructions.empty()
        ? String("Instructions disabled") : String("Instructions saved");
    renderConsoleScreen();
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleChatCompact()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    if (webSshTaskIsRunning()) {
        sendWebJsonError(
            server, 409,
            "Wait for the background SSH connection attempt to finish or stop it");
        return;
    }
    const ChatDocumentResult stored = loadProjectChatMetadata(
        activeProject.summary.id, activeChat.summary.id);
    if (!stored.success) {
        sendWebJsonError(server, 500, stored.error);
        return;
    }
    constexpr std::uint32_t kRetainedMessages = 8;
    const std::uint32_t summaryTarget = stored.chat.summary.messageCount >
            kRetainedMessages
        ? stored.chat.summary.messageCount - kRetainedMessages : 0;
    if (summaryTarget == 0) {
        sendWebJsonError(
            server, 409, "This chat is too short to compact");
        return;
    }
    if (webSshTerminalOpen || webSshAwaitingTrust) {
        closeWebSshConnection();
    }
    std::string stagedSummary;
    std::uint32_t nextMessageIndex = 0;
    while (nextMessageIndex < summaryTarget) {
        const std::uint32_t remaining = summaryTarget - nextMessageIndex;
        const IndexedMessagesPageResult page = readProjectChatMessagesByIndex(
            activeProject.summary.id, activeChat.summary.id, nextMessageIndex,
            std::min<std::uint32_t>(remaining, 32), 32768);
        if (!page.success || page.messages.empty() ||
            page.nextMessageIndex <= nextMessageIndex ||
            page.nextMessageIndex > summaryTarget) {
            sendWebJsonError(
                server, 500,
                page.success ? String("Context summary raw-history page did not advance")
                             : page.error);
            return;
        }
        WebContextSummaryResult generated = generateWebContextSummary(
            stagedSummary, page.messages);
        if (!generated.success || generated.includedMessages == 0 ||
            generated.includedMessages > page.messages.size()) {
            sendWebJsonError(
                server, 500,
                generated.success ? String("Context summary request did not advance")
                                  : generated.error);
            return;
        }
        stagedSummary = std::move(generated.summary);
        nextMessageIndex += generated.includedMessages;
    }
    ChatDocumentResult current = loadProjectChatMetadata(
        activeProject.summary.id, activeChat.summary.id);
    if (!current.success) {
        sendWebJsonError(server, 500, current.error);
        return;
    }
    if (current.chat.summary.messageCount != stored.chat.summary.messageCount) {
        sendWebJsonError(
            server, 409,
            "Chat history changed while the context summary was being regenerated");
        return;
    }
    current.chat.contextSummary = std::move(stagedSummary);
    current.chat.summarizedMessageCount = summaryTarget;
    OperationResult result = saveProjectChatMetadata(current.chat);
    if (result.success) result = loadActiveChat(activeChat.summary.id);
    if (result.success) result = refreshChats();
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["summary_event"] = "manual_regenerated";
    document["summarized_messages"] = activeChat.summarizedMessageCount;
    sendWebJson(server, 200, document);
}

void handleChatPermissions()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String requested = server.arg("ssh_tools_enabled");
    if (requested != "0" && requested != "1") {
        sendWebJsonError(server, 400, "SSH tool permission must be either 0 or 1");
        return;
    }
    if (requested == "1") {
        if (!sshProfilesReady) {
            const OperationResult refreshed = refreshSshProfiles();
            if (!refreshed.success) {
                sendWebJsonError(server, 500, refreshed.error);
                return;
            }
        }
        const bool complete = !consoleSshProfiles.empty() &&
            consoleSshSelected < consoleSshProfiles.size() &&
            sshProfileIsComplete(consoleSshProfiles[consoleSshSelected]);
        if (!complete) {
            sendWebJsonError(server, 409,
                             "A complete selected SSH profile is required");
            return;
        }
    }
    activeChat.sshToolsEnabled = requested == "1";
    activeChat.summary.updatedAt = currentTimestamp();
    const OperationResult saved = saveActiveChat();
    if (!saved.success) {
        sendWebJsonError(server, 500, saved.error);
        return;
    }
    const OperationResult refreshed = refreshChats();
    if (!refreshed.success) {
        sendWebJsonError(server, 500, refreshed.error);
        return;
    }
    consoleStatus = activeChat.sshToolsEnabled
        ? String("Model SSH access enabled for this chat")
        : String("Model SSH access disabled for this chat");
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleRenameChat()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const std::string requestedTitle = server.arg("title").c_str();
    if (requestedTitle.empty() || requestedTitle.size() > 256 || !isValidUtf8(requestedTitle)) {
        sendWebJsonError(server, 400, "Chat title must be valid UTF-8 between 1 and 256 bytes");
        return;
    }
    activeChat.summary.title = makeChatTitle(requestedTitle, kMaximumChatTitleCells).c_str();
    activeChat.summary.updatedAt = currentTimestamp();
    OperationResult result = saveActiveChat();
    if (result.success) {
        result = refreshChats();
    }
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    consoleStatus = "Chat renamed";
    renderConsoleScreen();
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handlePinChat()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    activeChat.summary.pinned = !activeChat.summary.pinned;
    if (activeChat.summary.pinned) {
        activeChat.summary.archived = false;
    }
    OperationResult result = saveActiveChat();
    if (result.success) {
        result = refreshChats();
    }
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    consoleStatus = activeChat.summary.pinned ? String("Chat pinned") : String("Chat unpinned");
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleArchiveChat()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    activeChat.summary.archived = !activeChat.summary.archived;
    if (activeChat.summary.archived) {
        activeChat.summary.pinned = false;
    }
    OperationResult result = saveActiveChat();
    if (result.success) {
        result = refreshChats();
    }
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    consoleStatus = activeChat.summary.archived ? String("Chat archived")
                                                : String("Chat restored");
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleDuplicateChat()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const std::uint32_t startedAt = millis();
    const ChatDocumentResult duplicated = duplicateProjectChat(
        activeProject.summary.id, activeChat.summary.id);
    recordWebSdWrite(millis() - startedAt);
    if (!duplicated.success) {
        sendWebJsonError(server, 500, duplicated.error);
        return;
    }
    activeChat = duplicated.chat;
    OperationResult selected = saveActiveChatSelection(duplicated.chat.summary.id);
    if (!selected.success) {
        sendWebJsonError(server, 500, selected.error);
        return;
    }
    ++projectRevision;
    ++chatRevision;
    const OperationResult refreshed = refreshChats();
    if (!refreshed.success) {
        sendWebJsonError(server, 500, refreshed.error);
        return;
    }
    consoleStatus = "Chat duplicated";
    renderConsoleScreen();
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleExportChat()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String filename = "chat_" + activeChat.summary.id + ".md";
    const std::uint32_t startedAt = millis();
    const OperationResult exported = exportProjectChatMarkdown(
        activeProject.summary.id, activeChat.summary.id, filename);
    recordWebSdWrite(millis() - startedAt);
    if (!exported.success) {
        sendWebJsonError(server, 400, exported.error);
        return;
    }
    const OperationResult refreshed = refreshFiles();
    if (!refreshed.success) {
        sendWebJsonError(server, 500, refreshed.error);
        return;
    }
    consoleStatus = "Chat exported as " + filename;
    JsonDocument document;
    document["ok"] = true;
    document["filename"] = filename;
    sendWebJson(server, 200, document);
}

void handleExportChatBundle()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String filename =
        "project_" + activeProject.summary.id + ".cardmind-project.jsonl";
    const std::uint32_t startedAt = millis();
    const OperationResult exported = exportProjectBundle(
        activeProject.summary.id, filename);
    recordWebSdWrite(millis() - startedAt);
    if (!exported.success) {
        sendWebJsonError(server, 400, exported.error);
        return;
    }
    const OperationResult refreshed = refreshFiles();
    if (!refreshed.success) {
        sendWebJsonError(server, 500, refreshed.error);
        return;
    }
    consoleStatus = "Project bundle exported as " + filename;
    JsonDocument document;
    document["ok"] = true;
    document["filename"] = filename;
    sendWebJson(server, 200, document);
}

void handleImportChatBundle()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const std::uint32_t startedAt = millis();
    const ProjectDocumentResult imported = importProjectBundle(server.arg("name"));
    recordWebSdWrite(millis() - startedAt);
    if (!imported.success) {
        sendWebJsonError(server, 400, imported.error);
        return;
    }
    OperationResult refreshed = refreshProjects();
    if (refreshed.success) {
        refreshed = selectActiveProject(imported.project.summary.id);
    }
    if (!refreshed.success) {
        sendWebJsonError(server, 500, refreshed.error);
        return;
    }
    consoleStatus = "Project imported";
    renderConsoleScreen();
    JsonDocument document;
    document["ok"] = true;
    document["project_id"] = activeProject.summary.id;
    sendWebJson(server, 200, document);
}

void handleDeleteChat()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const std::uint32_t startedAt = millis();
    OperationResult result = deleteProjectChat(
        activeProject.summary.id, activeChat.summary.id);
    recordWebSdWrite(millis() - startedAt);
    if (result.success) {
        result = refreshChats();
    }
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    if (consoleChats.empty()) {
        const ChatDocumentResult created = createProjectChat(
            activeProject.summary.id, "New chat");
        if (!created.success) {
            sendWebJsonError(server, 500, created.error);
            return;
        }
        activeChat = created.chat;
        result = saveActiveChatSelection(created.chat.summary.id);
        if (!result.success) {
            sendWebJsonError(server, 500, result.error);
            return;
        }
        ++projectRevision;
        ++chatRevision;
        result = refreshChats();
    } else {
        result = loadActiveChat(consoleChats.front().id);
    }
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    result = saveActiveChatSelection(activeChat.summary.id);
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    ++projectRevision;
    consoleStatus = "Chat deleted";
    renderConsoleScreen();
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleSettings()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    Settings updated = consoleSettings;
    const String wifiSsid = server.arg("wifi_ssid");
    const String wifiPassword = server.arg("wifi_password");
    if (wifiSsid.isEmpty() || wifiSsid.length() > 32 || wifiPassword.length() > 63) {
        sendWebJsonError(server, 400, "Wi-Fi SSID must contain 1-32 bytes and password at most 63 bytes");
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
        sendWebJsonError(server, 400, "Model id must not exceed 120 characters");
        return;
    }
    updated.globalInstructions = server.arg("global_instructions");
    if (updated.globalInstructions.length() > 2048 ||
        !isValidUtf8(std::string(updated.globalInstructions.c_str()))) {
        sendWebJsonError(
            server, 400,
            "Global instructions must be valid UTF-8 and at most 2048 bytes");
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
    std::uint32_t historyQuotaMiB = 0;
    std::uint32_t volume = 0;
    std::uint32_t brightness = 0;
    std::uint32_t sleepMinutes = 0;
    std::uint32_t repeatMs = 0;
    std::uint32_t powerProfile = 0;
    if (!parseUnsignedArgument(
            server.arg("project_chat_history_quota_mib"), historyQuotaMiB) ||
        (historyQuotaMiB != 0 &&
         (historyQuotaMiB < 2 || historyQuotaMiB > 4095)) ||
        !parseUnsignedArgument(server.arg("tts_volume"), volume) || volume > 255 ||
        !parseUnsignedArgument(server.arg("display_brightness"), brightness) ||
        brightness < 32 || brightness > 255 ||
        !parseUnsignedArgument(server.arg("screen_sleep_minutes"), sleepMinutes) ||
        sleepMinutes > UINT16_MAX ||
        !parseUnsignedArgument(server.arg("keyboard_repeat_ms"), repeatMs) ||
        repeatMs > UINT16_MAX ||
        !parseUnsignedArgument(server.arg("power_profile"), powerProfile) ||
        powerProfile > 2) {
        sendWebJsonError(server, 400, "Device preference values are outside their supported ranges");
        return;
    }
    updated.ttsVolume = static_cast<std::uint8_t>(volume);
    updated.displayBrightness = static_cast<std::uint8_t>(brightness);
    updated.screenSleepMinutes = static_cast<std::uint16_t>(sleepMinutes);
    updated.keyboardRepeatMs = static_cast<std::uint16_t>(repeatMs);
    updated.powerProfile = static_cast<std::uint8_t>(powerProfile);
    updated.projectChatHistoryQuotaBytes = historyQuotaMiB * 1024U * 1024U;
    const OperationResult result = saveSettings(updated);
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    consoleSettings = updated;
    ++settingsRevision;
    consoleStatus = "Settings saved; Wi-Fi changes apply after closing the console";
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleClearChat()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const std::uint32_t startedAt = millis();
    OperationResult result = clearProjectChatHistory(
        activeProject.summary.id, activeChat.summary.id);
    recordWebSdWrite(millis() - startedAt);
    if (result.success) {
        result = loadActiveChat(activeChat.summary.id);
    }
    if (result.success) {
        result = refreshChats();
    }
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    clearFailedWebRequestInstructions();
    consoleStatus = "Chat messages cleared";
    renderConsoleScreen();
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleArchivedMessages()
{
    if (!sessionIsActive()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    std::uint32_t offset = 0;
    if (!parseUnsignedArgument(server.arg("offset"), offset)) {
        sendWebJsonError(server, 400, "Archive offset must be an unsigned integer");
        return;
    }
    const std::uint32_t startedAt = millis();
    const ArchivedMessagesPageResult result = readProjectChatMessages(
        activeProject.summary.id, activeChat.summary.id, offset, 8, 12000);
    recordWebSdRead(millis() - startedAt);
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
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
    sendWebJson(server, 200, document);
}

void handleModels()
{
    if (!sessionIsActive()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const ModelsResult result = fetchModels(consoleSettings);
    if (!result.success) {
        sendWebJsonError(server, 502, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    JsonArray models = document["models"].to<JsonArray>();
    for (const auto& model : result.models) {
        models.add(model);
    }
    sendWebJson(server, 200, document);
}

void handleDiagnosticsDownload()
{
    if (!sessionIsActive()) {
        sendWebJsonError(server, 401, "Authentication required");
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

void handleDiagnosticMetrics()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String enabled = server.arg("enabled");
    if (enabled != "0" && enabled != "1") {
        sendWebJsonError(server, 400, "Diagnostic metrics enabled must be 0 or 1");
        return;
    }
    setWebDiagnosticsEnabled(enabled == "1");
    JsonDocument document;
    document["ok"] = true;
    document["enabled"] = webDiagnosticsEnabled();
    sendWebJson(server, 200, document);
}

void handlePythonStart()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const PythonModeStatus status = inspectPythonMode();
    if (!status.partitionLayoutReady || !status.pythonImageReady) {
        sendWebJsonError(server, 409, status.error);
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
        sendWebJsonError(server, 409, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["restarting"] = true;
    document["address"] = "http://" + WiFi.localIP().toString() + "/";
    document["handoff_token"] = handoffToken;
    sendWebJson(server, 200, document);
    handoffToken = "";
    pythonRestartRequested = true;
}

void handleSshSettings()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    std::uint32_t port = 0;
    if (!parseUnsignedArgument(server.arg("port"), port) || port == 0 || port > 65535) {
        sendWebJsonError(server, 400, "SSH port must be between 1 and 65535");
        return;
    }
    const bool create = server.arg("create") == "1";
    SshProfile profile = {"", "", 22, "", "", SshAuthMode::Password, ""};
    const OperationResult loaded = create ? OperationResult{true, ""} : loadSshProfile(profile);
    if (!loaded.success) {
        sendWebJsonError(server, 500, loaded.error);
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
        sendWebJsonError(server, 400, "SSH auth mode must be 'password' or 'key'");
        return;
    }
    if (server.arg("replace_password") == "1") {
        profile.password = server.arg("password");
    }
    if (server.arg("replace_key_passphrase") == "1") {
        profile.privateKeyPassphrase = server.arg("key_passphrase");
    }
    OperationResult saved;
    const std::uint32_t saveStartedAt = millis();
    if (create) {
        std::vector<SshProfile> profiles;
        std::size_t selectedIndex = 0;
        saved = loadSshProfiles(profiles, selectedIndex);
        if (saved.success) saved = saveSshProfileAt(profile, profiles.size());
    } else {
        saved = saveSshProfile(profile);
    }
    recordWebSdWrite(millis() - saveStartedAt);
    profile.password = "";
    profile.privateKeyPassphrase = "";
    if (!saved.success) {
        sendWebJsonError(server, 400, saved.error);
        return;
    }
    const OperationResult refreshed = refreshSshProfiles();
    if (!refreshed.success) {
        sendWebJsonError(server, 500, refreshed.error);
        return;
    }
    consoleStatus = "SSH profile saved";
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleSshSelect()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    std::uint32_t index = 0;
    if (!parseUnsignedArgument(server.arg("index"), index)) {
        sendWebJsonError(server, 400, "SSH profile index must be an unsigned integer");
        return;
    }
    const std::uint32_t startedAt = millis();
    const OperationResult result = selectSshProfile(index);
    recordWebSdWrite(millis() - startedAt);
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    const OperationResult refreshed = refreshSshProfiles();
    if (!refreshed.success) {
        sendWebJsonError(server, 500, refreshed.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleSshDelete()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    std::uint32_t index = 0;
    if (!parseUnsignedArgument(server.arg("index"), index)) {
        sendWebJsonError(server, 400, "SSH profile index must be an unsigned integer");
        return;
    }
    const std::uint32_t startedAt = millis();
    const OperationResult result = deleteSshProfile(index);
    recordWebSdWrite(millis() - startedAt);
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    const OperationResult refreshed = refreshSshProfiles();
    if (!refreshed.success) {
        sendWebJsonError(server, 500, refreshed.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

bool webSshCancellationRequested()
{
    portENTER_CRITICAL(&webSshStateMux);
    const bool requested = webSshCancelRequested;
    portEXIT_CRITICAL(&webSshStateMux);
    return requested;
}

void completeWebSshWorker(WebSshStage stage, const String& error,
                          bool closeConnection)
{
    if (closeConnection) {
        clearWebSshConnection();
    }
    publishWebSshStage(stage, error);
    portENTER_CRITICAL(&webSshStateMux);
    webSshTask = nullptr;
    webSshCancelRequested = false;
    portEXIT_CRITICAL(&webSshStateMux);
}

void runWebSshWorker(void* parameter)
{
    const bool authenticateOnly = reinterpret_cast<std::uintptr_t>(parameter) == 1U;
    OperationResult result = {true, ""};
    if (!authenticateOnly) {
        publishWebSshStage(WebSshStage::Connecting, "");
        const std::uint32_t startedAt = millis();
        result = webSshClient.connect(webSshProfile, 10000);
        const std::uint32_t durationMs = millis() - startedAt;
        portENTER_CRITICAL(&webSshStateMux);
        webSshConnectMs = durationMs;
        portEXIT_CRITICAL(&webSshStateMux);
        if (!result.success || webSshCancellationRequested()) {
            completeWebSshWorker(
                webSshCancellationRequested() ? WebSshStage::Idle : WebSshStage::Failed,
                webSshCancellationRequested() ? String() : result.error, true);
            vTaskDelete(nullptr);
            return;
        }
        const SshTrustResult trust = checkTrustedSshHost(
            webSshProfile.host, webSshProfile.port, webSshClient.fingerprint());
        if (!trust.success) {
            completeWebSshWorker(WebSshStage::Failed, trust.error, true);
            vTaskDelete(nullptr);
            return;
        }
        if (!trust.found || !trust.matches) {
            const String fingerprint = webSshClient.fingerprint();
            const String keyType = webSshClient.hostKeyType();
            portENTER_CRITICAL(&webSshStateMux);
            std::snprintf(webSshFingerprint, sizeof(webSshFingerprint), "%s",
                          fingerprint.c_str());
            std::snprintf(webSshHostKeyType, sizeof(webSshHostKeyType), "%s",
                          keyType.c_str());
            webSshHostChanged = trust.found && !trust.matches;
            webSshAwaitingTrust = true;
            webSshStage = WebSshStage::AwaitingTrust;
            webSshTask = nullptr;
            portEXIT_CRITICAL(&webSshStateMux);
            vTaskDelete(nullptr);
            return;
        }
    }
    publishWebSshStage(WebSshStage::Authenticating, "");
    std::uint32_t startedAt = millis();
    result = webSshClient.authenticate(webSshProfile, 10000);
    const std::uint32_t authenticateMs = millis() - startedAt;
    portENTER_CRITICAL(&webSshStateMux);
    webSshAuthenticateMs = authenticateMs;
    portEXIT_CRITICAL(&webSshStateMux);
    if (!result.success || webSshCancellationRequested()) {
        completeWebSshWorker(
            webSshCancellationRequested() ? WebSshStage::Idle : WebSshStage::Failed,
            webSshCancellationRequested() ? String() : result.error, true);
        vTaskDelete(nullptr);
        return;
    }
    publishWebSshStage(WebSshStage::Opening, "");
    startedAt = millis();
    result = webSshClient.openTerminal(120, 36, 10000);
    const std::uint32_t openMs = millis() - startedAt;
    portENTER_CRITICAL(&webSshStateMux);
    webSshOpenMs = openMs;
    portEXIT_CRITICAL(&webSshStateMux);
    if (!result.success || webSshCancellationRequested()) {
        completeWebSshWorker(
            webSshCancellationRequested() ? WebSshStage::Idle : WebSshStage::Failed,
            webSshCancellationRequested() ? String() : result.error, true);
        vTaskDelete(nullptr);
        return;
    }
    webSshAwaitingTrust = false;
    webSshTerminalOpen = true;
    webSshProfile.password = "";
    webSshProfile.privateKeyPassphrase = "";
    completeWebSshWorker(WebSshStage::Connected, "", false);
    if (webDiagnosticsEnabled()) {
        Serial.printf("SSH_METRIC connect_ms=%u auth_ms=%u open_ms=%u heap=%u\n",
                      static_cast<unsigned int>(webSshConnectMs),
                      static_cast<unsigned int>(webSshAuthenticateMs),
                      static_cast<unsigned int>(webSshOpenMs),
                      static_cast<unsigned int>(ESP.getFreeHeap()));
    }
    vTaskDelete(nullptr);
}

OperationResult startWebSshWorker(bool authenticateOnly)
{
    portENTER_CRITICAL(&webSshStateMux);
    webSshCancelRequested = false;
    portEXIT_CRITICAL(&webSshStateMux);
    TaskHandle_t task = nullptr;
    vTaskSuspendAll();
    const BaseType_t created = xTaskCreate(
        runWebSshWorker, "web-ssh-connect", 12288,
        reinterpret_cast<void*>(authenticateOnly ? 1U : 0U), 1, &task);
    if (created == pdPASS && task != nullptr) {
        portENTER_CRITICAL(&webSshStateMux);
        webSshTask = task;
        portEXIT_CRITICAL(&webSshStateMux);
    }
    xTaskResumeAll();
    if (created != pdPASS || task == nullptr) {
        return {false, "Failed to allocate the background SSH connection task"};
    }
    return {true, ""};
}

void handleSshStart()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    if (webSshTaskIsRunning() || webSshAwaitingTrust ||
        (webSshTerminalOpen && webSshClient.isOpen())) {
        sendWebJsonError(server, 409,
                         "SSH terminal is already open; disconnect it before reconnecting");
        return;
    }
    clearWebSshConnection();
    publishWebSshStage(WebSshStage::Idle, "");
    const std::uint32_t loadStartedAt = millis();
    const OperationResult loaded = loadSshProfile(webSshProfile);
    recordWebSdRead(millis() - loadStartedAt);
    if (!loaded.success || !sshProfileIsComplete(webSshProfile)) {
        sendWebJsonError(server, 400, loaded.success ? String("Selected SSH profile is incomplete") : loaded.error);
        return;
    }
    portENTER_CRITICAL(&webSshStateMux);
    webSshConnectMs = 0;
    webSshAuthenticateMs = 0;
    webSshOpenMs = 0;
    webSshFingerprint[0] = '\0';
    webSshHostKeyType[0] = '\0';
    webSshHostChanged = false;
    portEXIT_CRITICAL(&webSshStateMux);
    const OperationResult result = startWebSshWorker(false);
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["stage"] = "connecting";
    sendWebJson(server, 202, document);
}

void handleSshTrust()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    if (!webSshAwaitingTrust || server.arg("fingerprint") != webSshFingerprint) {
        sendWebJsonError(server, 409, "SSH trust request does not match the pending connection");
        return;
    }
    OperationResult result = trustSshHost(webSshProfile.host, webSshProfile.port,
                                          webSshFingerprint);
    if (result.success) {
        webSshAwaitingTrust = false;
        result = startWebSshWorker(true);
    }
    if (!result.success) {
        sendWebJsonError(server, 502, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["stage"] = "authenticating";
    sendWebJson(server, 202, document);
}

void handleSshInput()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String data = server.arg("data");
    if (!webSshTerminalOpen || !webSshClient.isOpen() || data.isEmpty() || data.length() > 512) {
        sendWebJsonError(server, 409, "SSH terminal is closed or input is outside the 1-512 byte limit");
        return;
    }
    const OperationResult result = webSshClient.write(
        reinterpret_cast<const std::uint8_t*>(data.c_str()), data.length(), 5000);
    if (!result.success) {
        sendWebJsonError(server, 502, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleSshResize()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    std::uint32_t columns = 0;
    std::uint32_t rows = 0;
    if (!webSshTerminalOpen || !webSshClient.isOpen() ||
        !parseUnsignedArgument(server.arg("columns"), columns) ||
        !parseUnsignedArgument(server.arg("rows"), rows)) {
        sendWebJsonError(server, 409,
                         "SSH terminal is closed or resize dimensions are invalid");
        return;
    }
    const OperationResult result = webSshClient.resizeTerminal(columns, rows, 3000);
    if (!result.success) {
        sendWebJsonError(server, 502, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleSshOutput()
{
    if (!sessionIsActive()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    std::uint8_t buffer[2048] = {};
    const int readBytes = webSshTerminalOpen ? webSshClient.read(buffer, sizeof(buffer)) : 0;
    if (readBytes < 0) {
        closeWebSshConnection();
        sendWebJsonError(server, 502, "SSH terminal read failed with code " + String(readBytes));
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    const bool open = webSshTerminalOpen && webSshClient.isOpen();
    document["open"] = open;
    if (readBytes > 0) {
        const String encoded = base64::encode(buffer, static_cast<std::size_t>(readBytes));
        if (encoded == "-FAIL-") {
            sendWebJsonError(server, 500, "Failed to encode SSH terminal output");
            return;
        }
        document["output_base64"] = encoded;
    }
    if (!open) {
        closeWebSshConnection();
    }
    sendWebJson(server, 200, document);
}

void handleSshStop()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    closeWebSshConnection();
    JsonDocument document;
    document["ok"] = true;
    document["stage"] = webSshTaskIsRunning() ? "stopping" : "idle";
    sendWebJson(server, webSshTaskIsRunning() ? 202 : 200, document);
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
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const OperationResult opened = ensureWebSftp();
    if (!opened.success) {
        sendWebJsonError(server, 409, opened.error);
        return;
    }
    const SftpEntriesResult result = webSshClient.listSftpDirectory(server.arg("path"), 30000);
    if (!result.success) {
        sendWebJsonError(server, 502, result.error);
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
    sendWebJson(server, 200, document);
}

void handleSftpDownload()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    OperationResult result = ensureWebSftp();
    if (result.success) {
        const std::uint32_t startedAt = millis();
        result = webSshClient.downloadSftpFile(
            server.arg("path"), server.arg("name"), 60000);
        recordWebSdWrite(millis() - startedAt);
    }
    if (!result.success) {
        sendWebJsonError(server, 502, result.error);
        return;
    }
    result = refreshFiles();
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleSftpUpload()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    OperationResult result = ensureWebSftp();
    if (result.success) result = webSshClient.uploadSftpFile(
        server.arg("name"), server.arg("path"), 60000);
    if (!result.success) {
        sendWebJsonError(server, 502, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
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
        const OperationResult storage = requireSdWriteAccess(
            0, kStorageOperationalFloorBytes);
        if (!storage.success) {
            sshKeyUploadError = storage.error;
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
        const OperationResult storage = requireSdWriteAccess(
            0, kStorageOperationalFloorBytes);
        if (!storage.success) {
            sshKeyUploadError = storage.error;
            if (sshKeyUploadFile) {
                sshKeyUploadFile.close();
            }
            return;
        }
        if (!sshKeyUploadFile || upload.currentSize > 16384 - sshKeyUploadBytes) {
            sshKeyUploadError = "SSH private key exceeds 16384 bytes";
            if (sshKeyUploadFile) {
                sshKeyUploadFile.close();
            }
            if (requireSdWriteAccess(0, 0).success) {
                SD.remove(kSshKeyUploadPath);
            }
            return;
        }
        const std::size_t written = sshKeyUploadFile.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            sshKeyUploadError = "microSD did not accept the complete SSH key chunk";
            sshKeyUploadFile.close();
            if (requireSdWriteAccess(0, 0).success) {
                SD.remove(kSshKeyUploadPath);
            }
            return;
        }
        sshKeyUploadBytes += written;
        return;
    }
    if (upload.status == UPLOAD_FILE_END) {
        if (!sshKeyUploadError.isEmpty()) {
            return;
        }
        OperationResult storage = requireSdWriteAccess(
            0, kStorageOperationalFloorBytes);
        if (!storage.success) {
            sshKeyUploadError = storage.error;
            if (sshKeyUploadFile) {
                sshKeyUploadFile.close();
            }
            return;
        }
        sshKeyUploadFile.flush();
        sshKeyUploadFile.close();
        storage = requireSdWriteAccess(0, kStorageOperationalFloorBytes);
        if (!storage.success) {
            sshKeyUploadError = storage.error;
            return;
        }
        const OperationResult installed = installSshPrivateKey(kSshKeyUploadPath);
        if (requireSdWriteAccess(0, 0).success) {
            SD.remove(kSshKeyUploadPath);
        }
        if (!installed.success) {
            sshKeyUploadError = installed.error;
        }
        return;
    }
    if (upload.status == UPLOAD_FILE_ABORTED) {
        if (sshKeyUploadFile) {
            sshKeyUploadFile.close();
        }
        if (requireSdWriteAccess(0, 0).success) {
            SD.remove(kSshKeyUploadPath);
        }
        sshKeyUploadError = "SSH private-key upload was aborted";
    }
}

void handleSshKeyUploadComplete()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    if (!sshKeyUploadError.isEmpty()) {
        const SdStorageStatus storage = inspectSdStorage();
        const int errorStatus = storage.state == SdStorageState::Ready
            ? 400
            : webStorageErrorStatus(storage.state);
        sendWebJsonError(server, errorStatus, sshKeyUploadError);
        return;
    }
    const OperationResult refreshed = refreshSshProfiles();
    if (!refreshed.success) {
        sendWebJsonError(server, 500, refreshed.error);
        return;
    }
    consoleStatus = "SSH private key installed";
    JsonDocument document;
    document["ok"] = true;
    document["bytes"] = sshKeyUploadBytes;
    sendWebJson(server, 200, document);
}

void handleQrShow()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String content = server.arg("content");
    const std::string payload = content.c_str();
    if (payload.empty() || payload.size() > kMaximumQrPayloadBytes ||
        !isValidUtf8(payload)) {
        sendWebJsonError(server, 400, "QR content must be valid UTF-8 between 1 and 320 bytes");
        return;
    }
    consoleQrPayload = content;
    showQrCode("WEB QR", consoleQrPayload, "Manage in Web Console");
    JsonDocument document;
    document["ok"] = true;
    document["bytes"] = payload.size();
    sendWebJson(server, 200, document);
}

void handleQrFile()
{
    if (!sessionIsActive()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String name = server.arg("name");
    if (!isWorkspaceTextFile(std::string(name.c_str()))) {
        sendWebJsonError(server, 400, "Selected workspace file is not editable text");
        return;
    }
    const std::uint32_t startedAt = millis();
    const WorkspaceChunkResult result = readWorkspaceFileChunk(
        name, 0, kMaximumQrPayloadBytes + 1);
    recordWebSdRead(millis() - startedAt);
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    if (!result.eof || result.totalBytes == 0 ||
        result.totalBytes > kMaximumQrPayloadBytes) {
        sendWebJsonError(server, 400, "Selected file must contain 1 to 320 UTF-8 bytes for QR display");
        return;
    }
    consoleQrPayload = result.content.c_str();
    showQrCode("FILE QR", consoleQrPayload, "Manage in Web Console");
    JsonDocument document;
    document["ok"] = true;
    document["content"] = result.content;
    document["bytes"] = result.totalBytes;
    sendWebJson(server, 200, document);
}

void handleQrClose()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    consoleQrPayload = "";
    renderConsoleScreen();
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleFileRead()
{
    if (!sessionIsActive()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String name = server.arg("name");
    if (!isWorkspaceTextFile(std::string(name.c_str()))) {
        sendWebJsonError(server, 400, "Selected workspace file is not editable text");
        return;
    }
    std::uint32_t offset = 0;
    if (!parseUnsignedArgument(server.arg("offset"), offset)) {
        sendWebJsonError(server, 400, "File offset must be an unsigned integer");
        return;
    }
    const std::uint32_t startedAt = millis();
    const WorkspaceChunkResult result = readWorkspaceFileChunk(
        name, offset, kMaximumWebFileChunkBytes);
    recordWebSdRead(millis() - startedAt);
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    JsonDocument document;
    document["ok"] = true;
    document["content"] = result.content;
    document["offset"] = result.offset;
    document["next_offset"] = result.nextOffset;
    document["total_bytes"] = result.totalBytes;
    document["eof"] = result.eof;
    sendWebJson(server, 200, document);
}

void handleFileSave()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String name = server.arg("name");
    if (!isWorkspaceTextFile(std::string(name.c_str()))) {
        sendWebJsonError(server, 400, "Selected workspace file is not editable text");
        return;
    }
    std::uint32_t offset = 0;
    std::uint32_t originalBytes = 0;
    if (!parseUnsignedArgument(server.arg("offset"), offset) ||
        !parseUnsignedArgument(server.arg("original_bytes"), originalBytes)) {
        sendWebJsonError(server, 400, "File offsets must be unsigned integers");
        return;
    }
    const std::string content = server.arg("content").c_str();
    if (content.size() > kMaximumWebFileChunkBytes || !isValidUtf8(content)) {
        sendWebJsonError(server, 400, "Editor window must be valid UTF-8 up to 12288 bytes");
        return;
    }
    const std::uint32_t startedAt = millis();
    OperationResult result = replaceWorkspaceFileRange(
        name, offset, originalBytes, content);
    recordWebSdWrite(millis() - startedAt);
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    result = refreshFiles();
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    consoleStatus = "File changes saved";
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleFileRename()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const std::uint32_t startedAt = millis();
    OperationResult result = renameWorkspaceFile(server.arg("name"), server.arg("new_name"));
    recordWebSdWrite(millis() - startedAt);
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    result = refreshFiles();
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    consoleStatus = "File renamed";
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleFileDelete()
{
    if (!requestHasValidCsrf()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const std::uint32_t startedAt = millis();
    OperationResult result = deleteWorkspaceFile(server.arg("name"));
    recordWebSdWrite(millis() - startedAt);
    if (!result.success) {
        sendWebJsonError(server, 400, result.error);
        return;
    }
    result = refreshFiles();
    if (!result.success) {
        sendWebJsonError(server, 500, result.error);
        return;
    }
    consoleStatus = "File deleted";
    JsonDocument document;
    document["ok"] = true;
    sendWebJson(server, 200, document);
}

void handleFileDownload()
{
    if (!sessionIsActive()) {
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    const String name = server.arg("name");
    if (!isValidWorkspaceFilename(name.c_str())) {
        sendWebJsonError(server, 400, "Invalid workspace filename");
        return;
    }
    const std::uint32_t startedAt = millis();
    File file = SD.open(workspaceFilePath(name), FILE_READ);
    recordWebSdRead(millis() - startedAt);
    if (!file) {
        sendWebJsonError(server, 404, "Workspace file does not exist: " + name);
        return;
    }
    const std::size_t totalBytesValue = file.size();
    if (totalBytesValue > kMaximumWorkspaceFileBytes) {
        file.close();
        sendWebJsonError(server, 400,
                         "Workspace file exceeds the supported 32-bit file range");
        return;
    }
    const std::uint32_t totalBytes = static_cast<std::uint32_t>(totalBytesValue);
    Client& client = server.client();
    const String responseHeader =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Disposition: attachment\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "Content-Length: " + String(static_cast<unsigned long long>(totalBytes)) +
        "\r\n\r\n";
    auto writeExact = [&client](const std::uint8_t* data, std::size_t bytes) -> bool {
        std::size_t written = 0;
        while (written < bytes) {
            if (!client.connected()) {
                return false;
            }
            const std::size_t accepted = client.write(data + written, bytes - written);
            if (accepted == 0 || accepted > bytes - written) {
                return false;
            }
            written += accepted;
        }
        return true;
    };
    bool success = writeExact(
        reinterpret_cast<const std::uint8_t*>(responseHeader.c_str()),
        responseHeader.length());
    std::uint8_t buffer[4096] = {};
    std::uint32_t sentBytes = 0;
    while (success && sentBytes < totalBytes) {
        const std::size_t blockBytes = std::min<std::size_t>(
            sizeof(buffer), totalBytes - sentBytes);
        const std::size_t readBytes = file.read(buffer, blockBytes);
        if (readBytes != blockBytes || !writeExact(buffer, readBytes)) {
            success = false;
            break;
        }
        sentBytes += static_cast<std::uint32_t>(readBytes);
        if ((sentBytes % (64U * 1024U)) == 0) {
            delay(0);
        }
    }
    file.close();
    client.flush();
    client.stop();
    if (!success || sentBytes != totalBytes) {
        Serial.printf("WEB_FILE_DOWNLOAD result=failed sent_bytes=%u expected_bytes=%u\n",
                      static_cast<unsigned int>(sentBytes),
                      static_cast<unsigned int>(totalBytes));
    }
}

void handleFileUploadData()
{
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        uploadReplacing = server.arg("replace") == "1";
        uploadName = uploadReplacing ? server.arg("name") : upload.filename;
        uploadStorageName = uploadReplacing ? uploadName + ".tmp" : uploadName;
        uploadError = "";
        uploadBytes = 0;
        uploadCreated = false;
        if (!requestHasValidCsrf()) {
            uploadError = "Authentication required";
            return;
        }
        const OperationResult storage = requireSdWriteAccess(
            0, kStorageOperationalFloorBytes);
        if (!storage.success) {
            uploadError = storage.error;
            return;
        }
        if (uploadReplacing && !isValidWorkspaceFilename(uploadName.c_str())) {
            uploadError = "Invalid workspace filename";
            return;
        }
        if (uploadReplacing) {
            const OperationResult recovered = recoverAtomicSdFile(
                workspaceFilePath(uploadName));
            if (!recovered.success) {
                uploadError = recovered.error;
                return;
            }
        }
        if (uploadReplacing && !SD.exists(workspaceFilePath(uploadName))) {
            uploadError = "Workspace file does not exist: " + uploadName;
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
        if (!uploadFile || uploadBytes > kMaximumWorkspaceFileBytes ||
            upload.currentSize > kMaximumWorkspaceFileBytes - uploadBytes) {
            failUpload("Uploaded file exceeds the supported 32-bit file range");
            return;
        }
        const OperationResult space = checkSdOperationSpace(
            upload.currentSize, kStorageOperationalFloorBytes);
        if (!space.success) {
            failUpload(space.error);
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
        sendWebJsonError(server, 401, "Authentication required");
        return;
    }
    if (!uploadError.isEmpty()) {
        sendWebJsonError(server, 400, uploadError);
        return;
    }
    const OperationResult refreshed = refreshFiles();
    if (!refreshed.success) {
        sendWebJsonError(server, 500, refreshed.error);
        return;
    }
    consoleStatus = uploadReplacing ? "File saved" : "File uploaded";
    JsonDocument document;
    document["ok"] = true;
    document["name"] = uploadName;
    document["bytes"] = uploadBytes;
    sendWebJson(server, 200, document);
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
    setWebDiagnosticsEnabled(false);
    Serial.println("WEB_CONSOLE stage=load_password");
    Serial.flush();
    consoleSettings = settings;
    firmwareVersion = version;
    OperationResult result = loadSetupAccessPointPassword(accessPassword);
    if (!result.success || accessPassword.isEmpty()) {
        releaseConsoleSessionState();
        return {false, initialChatId,
                result.success ? String("Installation password is missing") : result.error};
    }
    Serial.println("WEB_CONSOLE stage=refresh_projects");
    Serial.flush();
    settingsRevision = 1;
    String storageStartupError;
    const SdStorageStatus startupStorage = inspectSdStorage();
    result = requireSdReadAccess();
    if (result.success && startupStorage.state == SdStorageState::Ready) {
        result = refreshProjects();
        if (!result.success) {
            releaseConsoleSessionState();
            return {false, initialChatId, result.error};
        }
        Serial.println("WEB_CONSOLE stage=load_project");
        Serial.flush();
        const ProjectStorageManifestResult manifest = loadProjectStorageManifest();
        if (!manifest.success || manifest.manifest.activeProjectId.isEmpty()) {
            releaseConsoleSessionState();
            return {false, initialChatId,
                    manifest.success ? String("Active project is missing") : manifest.error};
        }
        result = selectActiveProject(manifest.manifest.activeProjectId);
        if (result.success && !initialChatId.isEmpty() &&
            initialChatId != activeChat.summary.id) {
            const ChatDocumentResult requested = loadProjectChat(
                activeProject.summary.id, initialChatId, 96,
                activeProjectTailByteBudget());
            if (requested.success) {
                activeChat = requested.chat;
                result = saveActiveChatSelection(initialChatId);
                if (result.success) {
                    ++chatRevision;
                    ++projectRevision;
                }
            }
        }
        if (!result.success) {
            releaseConsoleSessionState();
            return {false, initialChatId, result.error};
        }
    } else if (result.success && startupStorage.state == SdStorageState::Full) {
        result = loadCommittedConsoleStorageReadOnly();
        storageStartupError = startupStorage.error;
        if (!result.success) {
            activeProject = ProjectDocument{};
            activeChat = ChatDocument{};
            consoleProjects.clear();
            consoleChats.clear();
            consoleFiles.clear();
            storageStartupError += "; ";
            storageStartupError += result.error;
        }
    } else {
        activeProject = ProjectDocument{};
        activeChat = ChatDocument{};
        consoleProjects.clear();
        consoleChats.clear();
        consoleFiles.clear();
        storageStartupError = result.success ? startupStorage.error : result.error;
    }
    sessionToken = "";
    csrfToken = "";
    consoleStatus = storageStartupError;
    exitRequested = false;
    pythonRestartRequested = false;
    consoleEscapeConsumed = consoleEscapePressed();
    loginFailures = 0;
    loginLockedUntil = 0;
    if (!routesConfigured) {
        const WebConsoleRouteHandlers handlers = {{
            sendRoot,
            handleLogin,
            handleLogout,
            handleSession,
            handleCloseConsole,
            handleState,
            handleStorageConfirm,
            handleSelectProject,
            handleNewProject,
            handleProjectSettings,
            handleProjectSettingsRawComplete,
            handleProjectSettingsRawData,
            handleRenameProject,
            handleDuplicateProject,
            handleArchiveProject,
            handleDeleteProject,
            handleProjectLinks,
            handleProjectLinkUpdate,
            handlePrompt,
            handlePromptRawComplete,
            handlePromptRawData,
            handlePromptRetry,
            handleSelectChat,
            handleNewChat,
            handleInstructions,
            handleInstructionsRawComplete,
            handleInstructionsRawData,
            handleChatCompact,
            handleChatPermissions,
            handleRenameChat,
            handlePinChat,
            handleArchiveChat,
            handleDuplicateChat,
            handleExportChat,
            handleExportChatBundle,
            handleImportChatBundle,
            handleDeleteChat,
            handleClearChat,
            handleArchivedMessages,
            handleSettings,
            handleModels,
            handleDiagnosticsDownload,
            handleDiagnosticMetrics,
            handlePythonStart,
            handleSshSettings,
            handleSshSelect,
            handleSshDelete,
            handleSshStart,
            handleSshTrust,
            handleSshInput,
            handleSshResize,
            handleSshOutput,
            handleSshStop,
            handleSftpList,
            handleSftpDownload,
            handleSftpUpload,
            handleSshKeyUploadComplete,
            handleSshKeyUploadData,
            handleQrShow,
            handleQrFile,
            handleQrClose,
            handleFileRead,
            handleFileSave,
            handleFileRename,
            handleFileDelete,
            handleFileDownload,
            handleFileUploadComplete,
            handleFileUploadData,
            []() { sendWebJsonError(server, 404, "Not found"); },
        }, allowWebStorageRoute};
        configureWebConsoleRoutes(server, handlers);
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
    passwordRevealUntil = millis() + 30000U;
    renderConsoleScreen();
    bool escapeHeld = false;
    bool enterHeld = false;
    bool passwordWasVisible = consolePasswordVisible();
    while (!exitRequested) {
        server.handleClient();
        if (pythonRestartRequested) {
            showPythonWorkspaceRunning("http://" + WiFi.localIP().toString() + "/",
                                       accessPassword);
            delay(500);
            ESP.restart();
        }
        updateConsoleSerial();
        M5Cardputer.update();
        const Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
        const bool escapePressed = consoleEscapePressed();
        const bool enterPressed = keys.enter;
        if (consoleEscapeConsumed) {
            escapeHeld = escapePressed;
            if (!escapePressed) {
                consoleEscapeConsumed = false;
            }
        } else {
            if (escapePressed && !escapeHeld) {
                exitRequested = true;
                Serial.println("WEB_CONSOLE exit=keyboard");
            }
            escapeHeld = escapePressed;
        }
        if (enterPressed && !enterHeld) {
            passwordRevealUntil = millis() + 30000U;
            renderConsoleScreen();
        }
        enterHeld = enterPressed;
        const bool passwordIsVisible = consolePasswordVisible();
        if (passwordIsVisible != passwordWasVisible) {
            passwordWasVisible = passwordIsVisible;
            renderConsoleScreen();
        }
        delay(2);
    }
    showBusyScreen("WEB CONSOLE", "Closing browser and SSH sessions...");
    closeWebSshConnection();
    while (webSshTaskIsRunning()) {
        delay(10);
    }
    // Keep the listener bound because repeated stop/begin cycles exhaust lwIP sockets.
    server.client().stop();
    const String activeChatId = activeChat.summary.id;
    releaseConsoleSessionState();
    while (!M5Cardputer.Keyboard.keyList().empty()) {
        M5Cardputer.update();
        delay(5);
    }
    Serial.println("WEB_CONSOLE result=stopped");
    return {true, activeChatId, ""};
}

}  // namespace cardputer
