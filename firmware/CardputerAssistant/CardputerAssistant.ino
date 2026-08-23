#include <M5Cardputer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "src/api_client.h"
#include "src/app_types.h"
#include "src/audio_utils.h"
#include "src/chat_storage.h"
#include "src/crash_journal.h"
#include "src/file_workspace.h"
#include "src/file_portal.h"
#include "src/provisioning.h"
#include "src/stt_client.h"
#include "src/storage.h"
#include "src/ssh_client.h"
#include "src/ssh_terminal.h"
#include "src/text_utils.h"
#include "src/tts_client.h"
#include "src/ui.h"
#include "src/voice_input.h"
#include "src/web_search_client.h"
#include "src/web_console.h"
#include "src/wifi_networks.h"

#include <algorithm>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

SET_LOOP_TASK_STACK_SIZE(16384);

namespace {

constexpr const char* kFirmwareVersion = "1.8.0";
constexpr std::size_t kMaximumInputBytes = 1200;
constexpr std::size_t kMaximumWifiPasswordBytes = 63;
constexpr std::uint8_t kTtsVolumeStep = 64;
constexpr std::uint32_t kBatteryRefreshIntervalMs = 30000;
constexpr std::uint32_t kDraftAutosaveIntervalMs = 2000;
constexpr std::size_t kFileViewerChunkBytes = 2048;
constexpr std::size_t kFileViewerPageLines = 8;
constexpr std::size_t kFileEditorMaximumBytes = 4096;

enum class Screen {
    Chat,
    MainCarousel,
    VoiceMenu,
    DeviceMenu,
    FilesMenu,
    WorkspaceFileList,
    FileActions,
    FileViewer,
    FileEditor,
    FileFind,
    FileNameEntry,
    DeleteFileConfirm,
    Diagnostics,
    ControlsHelp,
    ModelPicker,
    ChatList,
    ChatActions,
    ChatInstructions,
    DeleteChatConfirm,
    WifiPicker,
    WifiPassword,
};

enum class FileNameAction {
    Create,
    Copy,
    Rename,
};

cardputer::Settings settings;
std::vector<cardputer::Message> history;
std::vector<cardputer::ChatSummary> chats;
std::vector<String> availableModels;
std::string inputBuffer;
std::string persistedDraft;
std::uint32_t lastDraftAutosaveAt = 0;
std::string activeResponse;
cardputer::KeyboardLayout keyboardLayout = cardputer::KeyboardLayout::English;
String statusMessage;
String transientStatusValue;
std::uint32_t transientStatusUntil = 0;
std::size_t scrollOffset = 0;
String serialInput;
std::vector<Point2D_t> pressedKeys;
Screen currentScreen = Screen::MainCarousel;
String activeChatId;
String activeChatTitle = "New chat";
std::string activeChatInstructions;
bool activeChatPinned = false;
bool activeChatArchived = false;
std::uint32_t activeChatArchivedMessageCount = 0;
bool chatStorageReady = false;
String chatStorageError;
bool fileWorkspaceReady = false;
String fileWorkspaceError;
std::size_t chatListIndex = 0;
std::size_t chatActionsIndex = 0;
String selectedChatId;
String selectedChatTitle;
std::string instructionsInput;
String instructionsStatus;
String deleteChatId;
String deleteChatTitle;
Screen deleteChatReturnScreen = Screen::ChatList;
std::size_t voiceMenuIndex = 0;
std::size_t deviceMenuIndex = 0;
std::size_t filesMenuIndex = 0;
std::size_t workspaceFileIndex = 0;
std::size_t fileActionsIndex = 0;
std::size_t diagnosticsIndex = 0;
std::size_t controlsHelpIndex = 0;
std::size_t modelPickerIndex = 0;
std::size_t wifiPickerIndex = 0;
std::vector<cardputer::WifiNetwork> scannedWifiNetworks;
std::vector<cardputer::WorkspaceFile> workspaceFiles;
String fileViewerName;
std::string fileViewerContent;
std::vector<std::string> fileViewerLines;
std::vector<std::uint32_t> fileViewerPreviousOffsets;
std::size_t fileViewerFirstLine = 0;
std::uint32_t fileViewerChunkOffset = 0;
std::uint32_t fileViewerNextOffset = 0;
std::uint32_t fileViewerTotalBytes = 0;
bool fileViewerEof = true;
std::string fileEditorInput;
std::size_t fileEditorCursor = 0;
std::uint32_t fileEditorOffset = 0;
std::uint32_t fileEditorOriginalBytes = 0;
String fileEditorStatus;
std::string fileFindInput;
std::string lastFileFindQuery;
std::uint32_t lastFileFindOffset = 0;
String fileFindStatus;
FileNameAction fileNameAction = FileNameAction::Create;
std::string fileNameInput;
String fileNameSource;
String fileNameStatus;
String deleteFileName;
std::string wifiPasswordInput;
String menuStatus;
bool voiceStorageReady = false;
String voiceStorageError;
bool sttCredentialsValidated = false;
std::size_t carouselIndex = 0;
Screen modelReturnScreen = Screen::Chat;
Screen wifiReturnScreen = Screen::Chat;
Screen chatListReturnScreen = Screen::Chat;
int batteryLevel = -1;
bool batteryCharging = false;
std::uint32_t lastBatteryRefreshAt = 0;
bool startupInProgress = true;
wl_status_t lastWifiStatus = WL_IDLE_STATUS;
bool crashJournalReady = false;
String crashJournalError;
bool sshStorageReady = false;
String sshStorageError;

void ensureNetworkReady();
void render();
void renderCarousel();
void renderChatActions();
void renderChatInstructions();
void renderChatList();
void renderControlsHelp();
void renderDeviceMenu();
void renderDiagnostics();
void renderFileActions();
void renderFileEditor();
void renderFileFind();
void renderFileNameEntry();
void renderFileViewer();
void renderFilesMenu();
void renderModelPicker();
void renderVoiceMenu();
void renderWifiPassword();
void renderWifiPicker();
void renderWorkspaceFileList();
void openWebConsole();
cardputer::OperationResult runSshTerminal();
void submitPrompt();
void runUiSearchEndToEndTest();

void refreshBatteryStatus()
{
    const std::int32_t measuredLevel = M5Cardputer.Power.getBatteryLevel();
    batteryLevel = measuredLevel < 0
        ? -1
        : std::min(100, static_cast<int>(measuredLevel));
    batteryCharging = M5Cardputer.Power.isCharging() == m5::Power_Class::is_charging;
    lastBatteryRefreshAt = millis();
}

std::size_t historyBytes(const std::vector<cardputer::Message>& messages)
{
    std::size_t total = 0;
    for (const auto& message : messages) {
        total += message.content.size();
    }
    return total;
}

struct HistoryFitResult {
    std::vector<cardputer::Message> retained;
    std::vector<cardputer::Message> archived;
};

HistoryFitResult fitHistoryToActiveContext(const std::vector<cardputer::Message>& messages)
{
    HistoryFitResult result = {messages, {}};
    while (result.retained.size() > cardputer::kMaximumStoredMessages ||
           historyBytes(result.retained) > cardputer::kMaximumStoredHistoryBytes) {
        if (result.retained.size() < 2) {
            result.archived.insert(result.archived.end(),
                                   result.retained.begin(), result.retained.end());
            result.retained.clear();
            break;
        }
        result.archived.insert(result.archived.end(),
                               result.retained.begin(), result.retained.begin() + 2);
        result.retained.erase(result.retained.begin(), result.retained.begin() + 2);
    }
    return result;
}

std::uint64_t currentChatTimestamp()
{
    const std::time_t current = std::time(nullptr);
    return current >= 1700000000 ? static_cast<std::uint64_t>(current) : 0;
}

cardputer::OperationResult refreshChatList()
{
    const cardputer::ChatsResult result = cardputer::listChats();
    if (!result.success) {
        return {false, result.error};
    }
    chats = result.chats;
    return {true, ""};
}

cardputer::OperationResult saveCurrentChat()
{
    if (!chatStorageReady || activeChatId.isEmpty()) {
        return {false, chatStorageError.isEmpty() ? String("Persistent chat storage is unavailable")
                                                  : chatStorageError};
    }
    std::uint64_t updatedAt = currentChatTimestamp();
    if (updatedAt == 0) {
        for (const auto& chat : chats) {
            if (chat.id == activeChatId) {
                updatedAt = chat.updatedAt;
                break;
            }
        }
    }
    const cardputer::ChatDocument document = {
        {activeChatId, activeChatTitle, updatedAt, static_cast<std::uint32_t>(history.size()),
         activeChatPinned, activeChatArchived, activeChatArchivedMessageCount},
        history,
        activeChatInstructions,
        inputBuffer,
    };
    const cardputer::OperationResult result = cardputer::saveChat(document);
    if (result.success) {
        persistedDraft = inputBuffer;
    }
    return result;
}

cardputer::OperationResult activateChat(const String& id)
{
    const cardputer::ChatDocumentResult loaded = cardputer::loadChat(id);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    const cardputer::OperationResult activeResult = cardputer::saveActiveChatId(id);
    if (!activeResult.success) {
        return activeResult;
    }
    activeChatId = loaded.chat.summary.id;
    activeChatTitle = loaded.chat.summary.title;
    history = loaded.chat.messages;
    activeChatInstructions = loaded.chat.instructions;
    activeChatPinned = loaded.chat.summary.pinned;
    activeChatArchived = loaded.chat.summary.archived;
    activeChatArchivedMessageCount = loaded.chat.summary.archivedMessageCount;
    activeResponse.clear();
    inputBuffer = loaded.chat.draft;
    persistedDraft = inputBuffer;
    lastDraftAutosaveAt = millis();
    scrollOffset = 0;
    return {true, ""};
}

cardputer::OperationResult createAndActivateChat()
{
    const cardputer::ChatDocumentResult created = cardputer::createChat("New chat");
    if (!created.success) {
        return {false, created.error};
    }
    const cardputer::OperationResult activeResult = cardputer::saveActiveChatId(created.chat.summary.id);
    if (!activeResult.success) {
        return activeResult;
    }
    activeChatId = created.chat.summary.id;
    activeChatTitle = created.chat.summary.title;
    activeChatInstructions.clear();
    activeChatPinned = false;
    activeChatArchived = false;
    activeChatArchivedMessageCount = 0;
    history.clear();
    activeResponse.clear();
    inputBuffer.clear();
    persistedDraft.clear();
    lastDraftAutosaveAt = millis();
    scrollOffset = 0;
    return refreshChatList();
}

cardputer::OperationResult initializeChats()
{
    cardputer::OperationResult result = cardputer::initializeChatStorage();
    if (!result.success) {
        return result;
    }
    result = refreshChatList();
    if (!result.success) {
        return result;
    }
    if (chats.empty()) {
        return createAndActivateChat();
    }
    String storedId;
    result = cardputer::loadActiveChatId(storedId);
    if (!result.success) {
        return result;
    }
    if (!storedId.isEmpty()) {
        for (const auto& chat : chats) {
            if (chat.id == storedId) {
                return activateChat(storedId);
            }
        }
        Serial.println("WARN event=active_chat reason=stored_id_not_found");
    }
    return activateChat(chats.front().id);
}

std::vector<String> chatListItems()
{
    std::vector<String> items = {"+ New chat"};
    items.reserve(chats.size() + 1);
    for (const auto& chat : chats) {
        const String marker = chat.id == activeChatId ? "[ON] " :
            (chat.pinned ? "[PIN] " : (chat.archived ? "[ARC] " : ""));
        const std::uint32_t totalMessages = chat.messageCount + chat.archivedMessageCount;
        items.push_back(marker + chat.title + "  [" + totalMessages + "]");
    }
    return items;
}

void renderChatList()
{
    cardputer::showSelectionList("CHATS", chatListItems(), chatListIndex,
                                 menuStatus.isEmpty()
                                     ? String("UP/DOWN  ENTER options  FN+DEL")
                                     : menuStatus);
}

std::vector<String> chatActionItems()
{
    cardputer::ChatSummary selected = {};
    for (const auto& chat : chats) {
        if (chat.id == selectedChatId) {
            selected = chat;
            break;
        }
    }
    return {
        "Open chat",
        "Chat instructions",
        "Context: " + String(selected.messageCount) + "/" +
            String(cardputer::kMaximumStoredMessages) + " + " +
            String(selected.archivedMessageCount) + " archived",
        selected.pinned ? "Unpin chat" : "Pin chat",
        selected.archived ? "Restore from archive" : "Archive chat",
        "Duplicate chat",
        "Export to workspace",
        "Delete chat",
        "Back",
    };
}

void renderChatActions()
{
    cardputer::showSelectionList(selectedChatTitle, chatActionItems(), chatActionsIndex,
                                 menuStatus.isEmpty()
                                     ? String("UP/DOWN  ENTER  ESC back")
                                     : menuStatus);
}

void openChatActions(const cardputer::ChatSummary& chat)
{
    selectedChatId = chat.id;
    selectedChatTitle = chat.title;
    chatActionsIndex = 0;
    menuStatus = "";
    currentScreen = Screen::ChatActions;
    renderChatActions();
}

void renderChatInstructions()
{
    cardputer::showTextEditor("CHAT INSTRUCTIONS", instructionsInput, keyboardLayout,
                             cardputer::kMaximumChatInstructionsBytes, instructionsStatus,
                             "(No instructions)",
                             "ENTER save  ESC cancel  Fn+3 lang");
}

void openChatList(Screen returnScreen)
{
    if (chatStorageReady && !activeChatId.isEmpty()) {
        const cardputer::OperationResult saved = saveCurrentChat();
        if (!saved.success) {
            statusMessage = saved.error;
            render();
            return;
        }
    }
    const cardputer::OperationResult result = refreshChatList();
    if (!result.success) {
        statusMessage = result.error;
        render();
        return;
    }
    chatListReturnScreen = returnScreen;
    chatListIndex = 0;
    for (std::size_t index = 0; index < chats.size(); ++index) {
        if (chats[index].id == activeChatId) {
            chatListIndex = index + 1;
            break;
        }
    }
    currentScreen = Screen::ChatList;
    renderChatList();
}

void render()
{
    if (startupInProgress) {
        cardputer::showBusyScreen("CARDMIND", statusMessage.isEmpty() ? String("Starting...")
                                                                       : statusMessage);
        return;
    }
    switch (currentScreen) {
    case Screen::Chat:
        cardputer::showChat(history, activeResponse, inputBuffer, keyboardLayout,
                            activeChatTitle, statusMessage, scrollOffset,
                            WiFi.status() == WL_CONNECTED, batteryLevel, batteryCharging);
        return;
    case Screen::MainCarousel:
        renderCarousel();
        return;
    case Screen::VoiceMenu:
        renderVoiceMenu();
        return;
    case Screen::DeviceMenu:
        renderDeviceMenu();
        return;
    case Screen::FilesMenu:
        renderFilesMenu();
        return;
    case Screen::WorkspaceFileList:
        renderWorkspaceFileList();
        return;
    case Screen::FileActions:
        renderFileActions();
        return;
    case Screen::FileViewer:
        renderFileViewer();
        return;
    case Screen::FileEditor:
        renderFileEditor();
        return;
    case Screen::FileFind:
        renderFileFind();
        return;
    case Screen::FileNameEntry:
        renderFileNameEntry();
        return;
    case Screen::DeleteFileConfirm:
        cardputer::showConfirmation("DELETE FILE", deleteFileName,
                                    "ENTER delete  ESC cancel");
        return;
    case Screen::Diagnostics:
        renderDiagnostics();
        return;
    case Screen::ControlsHelp:
        renderControlsHelp();
        return;
    case Screen::ModelPicker:
        renderModelPicker();
        return;
    case Screen::ChatList:
        renderChatList();
        return;
    case Screen::ChatActions:
        renderChatActions();
        return;
    case Screen::ChatInstructions:
        renderChatInstructions();
        return;
    case Screen::DeleteChatConfirm:
        cardputer::showConfirmation("DELETE CHAT", deleteChatTitle,
                                    "ENTER delete  ESC cancel");
        return;
    case Screen::WifiPicker:
        renderWifiPicker();
        return;
    case Screen::WifiPassword:
        renderWifiPassword();
        return;
    }
    cardputer::showFatalError("Current screen is invalid");
}

void setTransientStatus(const String& message, std::uint32_t durationMs)
{
    statusMessage = message;
    transientStatusValue = message;
    transientStatusUntil = millis() + durationMs;
}

void updateTransientStatus()
{
    if (transientStatusUntil == 0 ||
        static_cast<std::int32_t>(millis() - transientStatusUntil) < 0) {
        return;
    }
    if (statusMessage == transientStatusValue) {
        statusMessage = "";
        if (currentScreen == Screen::Chat) {
            render();
        }
    }
    transientStatusUntil = 0;
    transientStatusValue = "";
}

bool runPureSelfTest()
{
    const bool utf8Backspace = cardputer::removeLastUtf8CodePoint("Aя") == "A";
    const bool russianLayout = cardputer::mapKeyToRussian('Q') == "Й";
    std::string sseData;
    const bool sse = cardputer::extractSseData("data: [DONE]\r", sseData) && sseData == "[DONE]";
    const auto wavHeader = cardputer::buildPcmWavHeader(16000, 16000);
    const bool wav = wavHeader[0] == 'R' && wavHeader[8] == 'W' && wavHeader[40] == 0x00 &&
                     wavHeader[41] == 0x7D;
    const bool chatText = cardputer::makeChatTitle("  Привет  мир ", 20) == "Привет мир" &&
                          cardputer::isValidChatId("0123456789abcdef");
    const std::string cursorText = "AяB";
    const bool utf8Cursor = cardputer::previousUtf8Boundary(cursorText, 3) == 1 &&
        cardputer::nextUtf8Boundary(cursorText, 1) == 3 &&
        cardputer::insertUtf8At(cursorText, 3, "!") == "Aя!B" &&
        cardputer::eraseUtf8Before(cursorText, 3) == "AB";
    return utf8Backspace && russianLayout && sse && wav && chatText && utf8Cursor &&
           cardputer::fontSupportsCyrillic();
}

void printStatus()
{
    Serial.printf("STATUS board_adv=%s configured=%s voice_configured=%s search_configured=%s tts_configured=%s tts_auto=%s microsd=%s chats=%s chat_count=%u files=%s crash_journal=%s previous_operation=%s wifi=%s tls_time=%s battery=%d charging=%s history=%u heap=%u largest_heap=%u min_heap=%u stack_free=%u reset_reason=%d\n",
                  M5.getBoard() == m5::board_t::board_M5CardputerADV ? "yes" : "no",
                  cardputer::settingsAreComplete(settings) ? "yes" : "no",
                  cardputer::voiceSettingsAreComplete(settings) ? "yes" : "no",
                  cardputer::webSearchSettingsAreComplete(settings) ? "yes" : "no",
                  cardputer::ttsSettingsAreComplete(settings) ? "yes" : "no",
                  settings.ttsAutoPlay ? "yes" : "no",
                  voiceStorageReady ? "ready" : "unavailable",
                  chatStorageReady ? "ready" : "unavailable",
                  static_cast<unsigned int>(chats.size()),
                  fileWorkspaceReady ? "ready" : "unavailable",
                  crashJournalReady ? "ready" : "unavailable",
                  cardputer::previousOperation().c_str(),
                  WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
                  std::time(nullptr) >= 1700000000 ? "valid" : "invalid",
                  batteryLevel,
                  batteryCharging ? "yes" : "no",
                  static_cast<unsigned int>(history.size()),
                  static_cast<unsigned int>(ESP.getFreeHeap()),
                  static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                  static_cast<unsigned int>(ESP.getMinFreeHeap()),
                  static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)),
                  static_cast<int>(esp_reset_reason()));
}

cardputer::ToolExecutionResult executeAvailableTool(const cardputer::ToolCall& call)
{
    if (cardputer::isWebSearchToolName(call.name)) {
        return cardputer::executeWebSearchTool(settings, call);
    }
    if (cardputer::isWebFetchToolName(call.name)) {
        return cardputer::executeWebFetchTool(settings, call);
    }
    if (call.name == "list_files" || call.name == "read_file" ||
        call.name == "write_file" || call.name == "append_file") {
        return cardputer::executeWorkspaceTool(call);
    }
    return {
        false,
        "{\"ok\":false,\"error\":\"unsupported tool\"}",
        "API requested unsupported tool '" + String(call.name.c_str()) + "'",
    };
}

void runWebSearchTest()
{
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        Serial.println("WEBTEST result=failed stage=network");
        return;
    }
    if (!cardputer::webSearchSettingsAreComplete(settings)) {
        Serial.println("WEBTEST result=failed stage=configuration");
        return;
    }
    const cardputer::ToolExecutionResult result = cardputer::executeWebSearchTool(
        settings, {"web-test", "web_search", "{\"query\":\"M5Stack official website\"}"});
    Serial.printf("WEBTEST result=%s\n", result.success ? "pass" : "failed");
}

void runWebFetchTest()
{
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        Serial.println("FETCHTEST result=failed stage=network");
        return;
    }
    if (!cardputer::webSearchSettingsAreComplete(settings)) {
        Serial.println("FETCHTEST result=failed stage=configuration");
        return;
    }
    const cardputer::ToolExecutionResult result = cardputer::executeWebFetchTool(
        settings, {"fetch-test", "web_fetch", "{\"url\":\"https://m5stack.com/\"}"});
    Serial.printf("FETCHTEST result=%s\n", result.success ? "pass" : "failed");
}

void runWebSearchRoundTripTest()
{
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        Serial.println("SEARCHTEST result=failed stage=network");
        return;
    }
    if (!cardputer::webSearchSettingsAreComplete(settings)) {
        Serial.println("SEARCHTEST result=failed stage=configuration");
        return;
    }
    bool searchCalled = false;
    const std::vector<cardputer::Message> testHistory = {
        {"user", "/search cardputer zero"},
    };
    const cardputer::ChatResult result = cardputer::streamChatCompletionWithTools(
        settings, testHistory, "", [](const std::string&) {},
        [&searchCalled](const cardputer::ToolCall& call) {
            if (cardputer::isWebSearchToolName(call.name)) {
                searchCalled = true;
                return cardputer::executeWebSearchTool(settings, call);
            }
            return executeAvailableTool(call);
        }, []() { return false; });
    Serial.printf("SEARCHTEST result=%s search_called=%s response_bytes=%u\n",
                  result.success && searchCalled ? "pass" : "failed",
                  searchCalled ? "yes" : "no",
                  static_cast<unsigned int>(result.response.size()));
}

void runApiTest()
{
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        Serial.println("APITEST result=failed stage=network");
        return;
    }
    const std::vector<cardputer::Message> testHistory = {
        {"user", "Reply with exactly OK."},
    };
    const cardputer::ChatResult result = cardputer::streamChatCompletion(
        settings, testHistory, "", [](const std::string&) {}, []() { return false; });
    if (!result.success) {
        String safeError = result.error;
        safeError.replace("\r", " ");
        safeError.replace("\n", " ");
        if (safeError.length() > 180) {
            safeError = safeError.substring(0, 180) + "...";
        }
        Serial.printf("APITEST result=failed detail=%s\n", safeError.c_str());
        return;
    }
    Serial.printf("APITEST result=pass response_bytes=%u\n",
                  static_cast<unsigned int>(result.response.size()));
}

void runStorageTest()
{
    const cardputer::ChatDocumentResult created = cardputer::createChat("Storage test");
    if (!created.success) {
        Serial.println("STORAGETEST result=failed stage=chat_create");
        return;
    }
    cardputer::ChatDocument document = created.chat;
    document.messages = {{"user", "test"}, {"assistant", "OK"}};
    document.instructions = "Reply briefly.";
    const cardputer::OperationResult saved = cardputer::saveChat(document);
    const cardputer::ChatDocumentResult loaded = saved.success
        ? cardputer::loadChat(document.summary.id)
        : cardputer::ChatDocumentResult{false, {}, saved.error};
    const bool chatVerified = loaded.success && loaded.chat.messages.size() == 2 &&
        loaded.chat.messages[1].content == "OK" &&
        loaded.chat.instructions == "Reply briefly.";
    const cardputer::OperationResult chatCleanup = cardputer::deleteChat(document.summary.id);
    if (!chatVerified || !chatCleanup.success) {
        Serial.println("STORAGETEST result=failed stage=chat_roundtrip");
        return;
    }

    const String testName = "firmware_storage_test.txt";
    const cardputer::ToolExecutionResult write = cardputer::executeWorkspaceTool(
        {"storage-write", "write_file",
         "{\"name\":\"firmware_storage_test.txt\",\"content\":\"OK\"}"});
    const cardputer::ToolExecutionResult read = write.success
        ? cardputer::executeWorkspaceTool(
              {"storage-read", "read_file",
               "{\"name\":\"firmware_storage_test.txt\",\"offset\":0,\"max_bytes\":12288}"})
        : cardputer::ToolExecutionResult{false, "", write.error};
    const bool fileVerified = read.success && read.output.find("\"content\":\"OK\"") != std::string::npos;
    const String testPath = cardputer::workspaceFilePath(testName);
    const bool fileCleanup = SD.exists(testPath) && SD.remove(testPath);
    Serial.printf("STORAGETEST result=%s\n",
                  fileVerified && fileCleanup ? "pass" : "failed");
}

void runChatQolTest()
{
    const String exportName = "firmware_chat_export.md";
    const String exportPath = cardputer::workspaceFilePath(exportName);
    if (SD.exists(exportPath)) {
        SD.remove(exportPath);
    }
    const cardputer::ChatDocumentResult created = cardputer::createChat("Chat QoL test");
    if (!created.success) {
        Serial.println("CHATQOLTEST result=failed stage=create");
        return;
    }
    cardputer::ChatDocument source = created.chat;
    source.messages = {{"user", "active"}, {"assistant", "answer"}};
    source.instructions = "Be concise.";
    source.draft = "unfinished";
    source.summary.pinned = true;
    const std::vector<cardputer::Message> archivedMessages = {
        {"user", "old"}, {"assistant", "reply"},
    };
    cardputer::OperationResult result = cardputer::archiveChatMessages(
        source.summary.id, archivedMessages);
    if (result.success) {
        source.summary.archivedMessageCount = archivedMessages.size();
        result = cardputer::saveChat(source);
    }
    const cardputer::ChatDocumentResult loaded = result.success
        ? cardputer::loadChat(source.summary.id)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    if (result.success && (!loaded.success || !loaded.chat.summary.pinned ||
                           loaded.chat.summary.archivedMessageCount != 2 ||
                           loaded.chat.draft != "unfinished")) {
        result = {false, "Chat version-3 metadata round trip failed"};
    }
    const cardputer::ChatDocumentResult duplicated = result.success
        ? cardputer::duplicateChat(source.summary.id)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    if (result.success && (!duplicated.success || duplicated.chat.messages.size() != 2 ||
                           duplicated.chat.draft != "unfinished")) {
        result = {false, "Chat duplication verification failed"};
    }
    if (result.success) {
        result = cardputer::exportChatToWorkspace(source.summary.id, exportName);
    }
    if (result.success && !SD.exists(exportPath)) {
        result = {false, "Chat export file was not created"};
    }
    if (duplicated.success) {
        const cardputer::OperationResult cleanup = cardputer::deleteChat(
            duplicated.chat.summary.id);
        if (result.success && !cleanup.success) {
            result = cleanup;
        }
    }
    const cardputer::OperationResult sourceCleanup = cardputer::deleteChat(source.summary.id);
    if (result.success && !sourceCleanup.success) {
        result = sourceCleanup;
    }
    if (SD.exists(exportPath) && !SD.remove(exportPath) && result.success) {
        result = {false, "Chat export cleanup failed"};
    }
    Serial.printf("CHATQOLTEST result=%s\n", result.success ? "pass" : "failed");
}

void runFileWorkspaceEditTest()
{
    const String sourceName = "firmware_editor_test.txt";
    const String copyName = "firmware_editor_copy.txt";
    const String renamedName = "firmware_editor_renamed.txt";
    const String sourcePath = cardputer::workspaceFilePath(sourceName);
    const String copyPath = cardputer::workspaceFilePath(copyName);
    const String renamedPath = cardputer::workspaceFilePath(renamedName);
    if (SD.exists(sourcePath)) {
        SD.remove(sourcePath);
    }
    if (SD.exists(copyPath)) {
        SD.remove(copyPath);
    }
    if (SD.exists(renamedPath)) {
        SD.remove(renamedPath);
    }
    cardputer::OperationResult result = cardputer::createWorkspaceFile(sourceName);
    if (result.success) {
        File file = SD.open(sourcePath, FILE_WRITE);
        if (!file) {
            result = {false, "Failed to open maximum-size workspace test file"};
        } else {
            std::uint8_t block[1024] = {};
            std::fill(block, block + sizeof(block), static_cast<std::uint8_t>('a'));
            std::size_t written = 0;
            while (result.success && written < cardputer::kMaximumWorkspaceFileBytes) {
                const std::size_t blockBytes = std::min(
                    sizeof(block), cardputer::kMaximumWorkspaceFileBytes - written);
                if (file.write(block, blockBytes) != blockBytes) {
                    result = {false, "Failed to write maximum-size workspace test file"};
                }
                written += blockBytes;
            }
            file.flush();
            file.close();
        }
    }
    constexpr std::uint32_t editOffset = 245760;
    const std::string replacement = "мир";
    if (result.success) {
        result = cardputer::replaceWorkspaceFileRange(
            sourceName, editOffset, static_cast<std::uint32_t>(replacement.size()), replacement);
    }
    const cardputer::WorkspaceChunkResult read = result.success
        ? cardputer::readWorkspaceFileChunk(sourceName, editOffset, 32)
        : cardputer::WorkspaceChunkResult{false, "", 0, 0, 0, true, result.error};
    if (result.success && (!read.success || read.content.rfind(replacement, 0) != 0 ||
                           read.totalBytes != cardputer::kMaximumWorkspaceFileBytes)) {
        result = {false, "Workspace editor content verification failed"};
    }
    if (result.success) {
        result = cardputer::validateWorkspaceFileUtf8(sourceName);
    }
    const cardputer::WorkspaceFindResult found = result.success
        ? cardputer::findWorkspaceText(sourceName, replacement, 0)
        : cardputer::WorkspaceFindResult{false, false, 0, result.error};
    if (result.success && (!found.success || !found.found || found.offset != editOffset)) {
        result = {false, "Workspace search verification failed"};
    }
    if (result.success) {
        result = cardputer::saveWorkspaceBookmark(sourceName, editOffset);
    }
    const cardputer::WorkspaceBookmarkResult sourceBookmark = result.success
        ? cardputer::loadWorkspaceBookmark(sourceName)
        : cardputer::WorkspaceBookmarkResult{false, false, 0, result.error};
    if (result.success && (!sourceBookmark.success || !sourceBookmark.found ||
                           sourceBookmark.offset != editOffset)) {
        result = {false, "Workspace bookmark verification failed"};
    }
    if (result.success) {
        result = cardputer::copyWorkspaceFile(sourceName, copyName);
    }
    if (result.success) {
        result = cardputer::renameWorkspaceFile(copyName, renamedName);
    }
    const cardputer::WorkspaceBookmarkResult renamedBookmark = result.success
        ? cardputer::loadWorkspaceBookmark(renamedName)
        : cardputer::WorkspaceBookmarkResult{false, false, 0, result.error};
    if (result.success && (!renamedBookmark.success || !renamedBookmark.found ||
                           renamedBookmark.offset != editOffset)) {
        result = {false, "Copied and renamed bookmark verification failed"};
    }
    if (result.success) {
        result = cardputer::deleteWorkspaceFile(sourceName);
    }
    if (result.success) {
        result = cardputer::deleteWorkspaceFile(renamedName);
    }
    if (SD.exists(sourcePath)) {
        SD.remove(sourcePath);
    }
    if (SD.exists(copyPath)) {
        SD.remove(copyPath);
    }
    if (SD.exists(renamedPath)) {
        SD.remove(renamedPath);
    }
    Serial.printf("FILETEST result=%s\n", result.success ? "pass" : "failed");
}

void runToolApiTest()
{
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        Serial.println("TOOLTEST result=failed stage=network");
        return;
    }
    const String testName = "firmware_tool_test.txt";
    bool writeSucceeded = false;
    const std::vector<cardputer::Message> testHistory = {
        {"user", "Use write_file to save exactly OK in firmware_tool_test.txt, then confirm briefly."},
    };
    const cardputer::ChatResult result = cardputer::streamChatCompletionWithTools(
        settings, testHistory, "", [](const std::string&) {},
        [&writeSucceeded](const cardputer::ToolCall& call) {
            const cardputer::ToolExecutionResult execution = cardputer::executeWorkspaceTool(call);
            if (call.name == "write_file" && execution.success) {
                writeSucceeded = true;
            }
            return execution;
        }, []() { return false; });
    const String testPath = cardputer::workspaceFilePath(testName);
    const bool fileCreated = SD.exists(testPath);
    const bool cleanup = !fileCreated || SD.remove(testPath);
    if (!result.success || !writeSucceeded || !fileCreated || !cleanup) {
        Serial.println("TOOLTEST result=failed stage=tool_roundtrip");
        return;
    }
    Serial.printf("TOOLTEST result=pass response_bytes=%u\n",
                  static_cast<unsigned int>(result.response.size()));
}

void handleSerialCommand(const String& command)
{
    if (command == "STATUS") {
        printStatus();
        return;
    }
    if (command == "SELFTEST") {
        Serial.printf("SELFTEST result=%s\n", runPureSelfTest() ? "pass" : "fail");
        return;
    }
    if (command == "APITEST") {
        runApiTest();
        return;
    }
    if (command == "CANCELTEST") {
        const std::vector<cardputer::Message> testHistory = {{"user", "cancel"}};
        const cardputer::ChatResult result = cardputer::streamChatCompletion(
            settings, testHistory, "", [](const std::string&) {}, []() { return true; });
        Serial.printf("CANCELTEST result=%s\n",
                      !result.success && result.error == "Request canceled by user"
                          ? "pass" : "failed");
        return;
    }
    if (command == "STORAGETEST") {
        runStorageTest();
        return;
    }
    if (command == "CHATQOLTEST") {
        runChatQolTest();
        return;
    }
    if (command == "FILETEST") {
        runFileWorkspaceEditTest();
        return;
    }
    if (command == "SSHCHECK") {
        const cardputer::SshRuntimeProbeResult result = cardputer::probeSshRuntime();
        Serial.printf("SSHCHECK result=%s version=%s heap=%u largest_heap=%u stack_free=%u\n",
                      result.success ? "pass" : "failed",
                      result.success ? result.version.c_str() : "unavailable",
                      static_cast<unsigned int>(ESP.getFreeHeap()),
                      static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                      static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)));
        return;
    }
    if (command == "SSHPROBE") {
        ensureNetworkReady();
        cardputer::markOperation("ssh_probe");
        const cardputer::SshHostProbeResult result =
            cardputer::probeSshHost("ssh.github.com", 443, 60000);
        cardputer::markOperation("idle");
        Serial.printf("SSHPROBE result=%s key_type=%s heap=%u largest_heap=%u stack_free=%u error=%s\n",
                      result.success ? "pass" : "failed",
                      result.success ? result.hostKeyType.c_str() : "unavailable",
                      static_cast<unsigned int>(ESP.getFreeHeap()),
                      static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                      static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)),
                      result.success ? "none" : result.error.c_str());
        return;
    }
    if (command == "TOOLTEST") {
        runToolApiTest();
        return;
    }
    if (command == "WEBTEST") {
        runWebSearchTest();
        return;
    }
    if (command == "FETCHTEST") {
        runWebFetchTest();
        return;
    }
    if (command == "SEARCHTEST") {
        runWebSearchRoundTripTest();
        return;
    }
    if (command == "E2ETEST") {
        runUiSearchEndToEndTest();
        return;
    }
    if (command == "CONSOLE") {
        openWebConsole();
        return;
    }
    if (command == "STTTLS") {
        ensureNetworkReady();
        const cardputer::OperationResult result = cardputer::probeDefaultSttTls();
        Serial.printf("STTTLS result=%s\n", result.success ? "pass" : "failed");
        return;
    }
    if (command == "STTAUTH") {
        ensureNetworkReady();
        const cardputer::OperationResult result = cardputer::validateSttCredentials(settings);
        sttCredentialsValidated = result.success;
        Serial.printf("STTAUTH result=%s\n", result.success ? "pass" : "failed");
        return;
    }
    if (command == "TTSHW") {
        const cardputer::OperationResult result = cardputer::playTtsHardwareTest(settings.ttsVolume);
        Serial.printf("TTSHW result=%s\n", result.success ? "pass" : "failed");
        return;
    }
    if (command == "TTSTLS") {
        ensureNetworkReady();
        const cardputer::OperationResult result = cardputer::probeDefaultTtsTls();
        Serial.printf("TTSTLS result=%s\n", result.success ? "pass" : "failed");
        return;
    }
    if (command == "TTSAUTH") {
        ensureNetworkReady();
        const cardputer::OperationResult result = cardputer::validateTtsCredentials(settings);
        Serial.printf("TTSAUTH result=%s\n", result.success ? "pass" : "failed");
        return;
    }
    if (command == "TTSTEST") {
        ensureNetworkReady();
        const cardputer::OperationResult result = cardputer::synthesizeAndPlaySpeech(
            settings, "Hello. This is the Cardputer language assistant.");
        Serial.printf("TTSTEST result=%s\n", result.success ? "pass" : "failed");
        return;
    }
    Serial.println("ERROR event=serial_command reason=unsupported_command");
}

void updateSerial()
{
    while (Serial.available() > 0) {
        const char character = static_cast<char>(Serial.read());
        if (character == '\n') {
            serialInput.trim();
            if (!serialInput.isEmpty()) {
                handleSerialCommand(serialInput);
            }
            serialInput = "";
        } else if (character >= 'A' && character <= 'Z' && serialInput.length() < 32) {
            serialInput += character;
        } else if (character != '\r') {
            serialInput = "";
        }
    }
}

void refreshModels()
{
    statusMessage = "Loading models...";
    cardputer::showBusyScreen("MODELS", statusMessage);
    cardputer::markOperation("model_refresh");
    const cardputer::ModelsResult result = cardputer::fetchModels(settings);
    cardputer::markOperation("idle");
    if (!result.success) {
        availableModels.clear();
        statusMessage = result.error;
        Serial.println("WARN event=models_refresh result=failed");
        return;
    }
    availableModels = result.models;
    const auto selected = std::find(availableModels.begin(), availableModels.end(), settings.model);
    if (selected == availableModels.end()) {
        statusMessage = "Configured model not in /v1/models";
        Serial.println("WARN event=model_validation result=not_found");
        return;
    }
    statusMessage = "";
    Serial.printf("INFO event=models_refresh result=ok count=%u\n",
                  static_cast<unsigned int>(availableModels.size()));
}

void ensureNetworkReady()
{
    if (WiFi.status() != WL_CONNECTED) {
        statusMessage = "Connecting Wi-Fi...";
        cardputer::showBusyScreen("NETWORK", statusMessage);
        cardputer::markOperation("wifi_connect");
        const cardputer::OperationResult wifiResult = cardputer::connectToWifi(settings);
        cardputer::markOperation("idle");
        if (!wifiResult.success) {
            statusMessage = wifiResult.error;
            Serial.println("ERROR event=wifi_connect result=failed");
            return;
        }
    }
    if (std::time(nullptr) < 1700000000) {
        statusMessage = "Synchronizing TLS clock...";
        cardputer::showBusyScreen("NETWORK", statusMessage);
        cardputer::markOperation("tls_clock");
        const cardputer::OperationResult clockResult = cardputer::synchronizeTlsClock();
        cardputer::markOperation("idle");
        if (!clockResult.success) {
            statusMessage = clockResult.error;
            return;
        }
    }
    statusMessage = "";
}

void beginConfiguredNetwork()
{
    if (settings.wifiSsid.isEmpty()) {
        lastWifiStatus = WiFi.status();
        Serial.println("NETWORK startup=skipped reason=ssid_not_configured");
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
    configTime(0, 0, "time.cloudflare.com", "pool.ntp.org");
    lastWifiStatus = WiFi.status();
    Serial.println("NETWORK startup=background");
}

void submitPrompt()
{
    if (inputBuffer.empty()) {
        statusMessage = "Type a message before pressing Enter";
        render();
        return;
    }
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        render();
        return;
    }
    if (!chatStorageReady) {
        statusMessage = chatStorageError;
        render();
        return;
    }
    if (!fileWorkspaceReady) {
        statusMessage = fileWorkspaceError;
        render();
        return;
    }

    const std::string prompt = inputBuffer;
    const bool useWorkspaceTools = cardputer::requestsWorkspaceAccess(prompt);
    const bool useWebSearch = cardputer::requestsWebSearch(prompt);
    if (useWebSearch && !cardputer::webSearchSettingsAreComplete(settings)) {
        statusMessage = "Web search not configured; Fn+4 > Web setup";
        render();
        return;
    }

    std::vector<cardputer::Message> pendingHistory = history;
    pendingHistory.push_back({"user", prompt});
    HistoryFitResult pendingFit = fitHistoryToActiveContext(pendingHistory);
    if (!pendingFit.archived.empty()) {
        const cardputer::OperationResult archived = cardputer::archiveChatMessages(
            activeChatId, pendingFit.archived);
        if (!archived.success) {
            statusMessage = archived.error;
            render();
            return;
        }
        activeChatArchivedMessageCount +=
            static_cast<std::uint32_t>(pendingFit.archived.size());
    }
    const String pendingTitle = history.empty()
        ? String(cardputer::makeChatTitle(prompt, cardputer::kMaximumChatTitleCells).c_str())
        : activeChatTitle;
    cardputer::ChatDocument pendingDocument = {
        {activeChatId, pendingTitle, currentChatTimestamp(),
         static_cast<std::uint32_t>(pendingFit.retained.size()), activeChatPinned,
         activeChatArchived, activeChatArchivedMessageCount},
        std::move(pendingFit.retained),
        activeChatInstructions,
        "",
    };
    const cardputer::OperationResult pendingSave = cardputer::saveChat(pendingDocument);
    if (!pendingSave.success) {
        statusMessage = pendingSave.error;
        render();
        return;
    }
    history = std::move(pendingDocument.messages);
    activeChatTitle = pendingTitle;
    inputBuffer.clear();
    activeResponse.clear();
    scrollOffset = 0;
    statusMessage = "Streaming...";
    render();
    std::uint32_t lastStreamRenderAt = 0;
    const cardputer::ChatTextCallback onText = [&lastStreamRenderAt](const std::string& text) {
        activeResponse += text;
        const std::uint32_t now = millis();
        if (lastStreamRenderAt == 0 || now - lastStreamRenderAt >= 120) {
            render();
            lastStreamRenderAt = now;
        }
    };
    const bool webSearchAvailable = cardputer::webSearchSettingsAreComplete(settings);
    const bool useTools = useWorkspaceTools || webSearchAvailable;
    Serial.printf("INFO event=chat_route workspace_tools=%s web_search_available=%s web_search_intent=%s\n",
                  useWorkspaceTools ? "enabled" : "disabled",
                  webSearchAvailable ? "yes" : "no",
                  useWebSearch ? "enabled" : "disabled");
    cardputer::markOperation(useTools ? "chat_tools" : "chat_stream");
    const cardputer::CancelCallback isCancelled = []() {
        M5Cardputer.update();
        return M5Cardputer.Keyboard.keysState().esc;
    };
    const cardputer::ChatResult result = useTools
        ? cardputer::streamChatCompletionWithTools(
              settings, history, activeChatInstructions, onText, [](const cardputer::ToolCall& call) {
                  statusMessage = "Tool: " + String(call.name.c_str());
                  render();
                  return executeAvailableTool(call);
              }, isCancelled)
        : cardputer::streamChatCompletion(
              settings, history, activeChatInstructions, onText, isCancelled);
    cardputer::markOperation("idle");
    if (!result.success) {
        activeResponse = result.response;
        statusMessage = result.error;
        const cardputer::OperationResult listResult = refreshChatList();
        if (!listResult.success) {
            statusMessage += "; chat list: " + listResult.error;
        }
        Serial.println("ERROR event=chat_completion result=failed");
        render();
        return;
    }
    history.push_back({"assistant", result.response});
    HistoryFitResult finalFit = fitHistoryToActiveContext(history);
    if (!finalFit.archived.empty()) {
        const cardputer::OperationResult archived = cardputer::archiveChatMessages(
            activeChatId, finalFit.archived);
        if (!archived.success) {
            activeResponse.clear();
            statusMessage = "Response received but history archive failed: " + archived.error;
            render();
            return;
        }
        activeChatArchivedMessageCount +=
            static_cast<std::uint32_t>(finalFit.archived.size());
    }
    history = std::move(finalFit.retained);
    activeResponse.clear();
    const cardputer::OperationResult finalSave = saveCurrentChat();
    if (!finalSave.success) {
        statusMessage = "Response received but chat save failed: " + finalSave.error;
        Serial.println("ERROR event=chat_save result=failed stage=assistant");
        render();
        return;
    }
    const cardputer::OperationResult listResult = refreshChatList();
    if (listResult.success) {
        setTransientStatus("Saved", 1500);
    } else {
        statusMessage = listResult.error;
    }
    Serial.println("INFO event=chat_completion result=ok");
    if (settings.ttsAutoPlay) {
        cardputer::showBusyScreen("SPEAKING", "Generating multilingual audio...");
        cardputer::markOperation("tts_auto");
        const cardputer::OperationResult speech = cardputer::synthesizeAndPlaySpeech(
            settings, result.response);
        cardputer::markOperation("idle");
        if (!speech.success) {
            statusMessage = speech.error;
            Serial.println("ERROR event=tts_playback result=failed source=auto");
        } else {
            setTransientStatus("Spoken", 1500);
            Serial.println("INFO event=tts_playback result=ok source=auto");
        }
    }
    render();
}

void runUiSearchEndToEndTest()
{
    if (!chatStorageReady || activeChatId.isEmpty()) {
        Serial.println("E2ETEST result=failed stage=storage_not_ready");
        return;
    }
    const String originalChatId = activeChatId;
    const Screen originalScreen = currentScreen;
    const cardputer::ChatsResult existing = cardputer::listChats();
    if (!existing.success) {
        Serial.println("E2ETEST result=failed stage=list_chats");
        return;
    }
    for (const auto& chat : existing.chats) {
        if (chat.title == "E2E search test") {
            const cardputer::OperationResult cleanup = cardputer::deleteChat(chat.id);
            if (!cleanup.success) {
                Serial.println("E2ETEST result=failed stage=stale_cleanup");
                return;
            }
        }
    }
    const cardputer::ChatDocumentResult created = cardputer::createChat("E2E search test");
    if (!created.success) {
        Serial.println("E2ETEST result=failed stage=create_chat");
        return;
    }

    activeChatId = created.chat.summary.id;
    activeChatTitle = created.chat.summary.title;
    history.clear();
    activeResponse.clear();
    inputBuffer = "/search cardputer zero";
    scrollOffset = 0;
    currentScreen = Screen::Chat;
    Serial.printf("E2ETEST stage=submit heap=%u largest_heap=%u stack_free=%u\n",
                  static_cast<unsigned int>(ESP.getFreeHeap()),
                  static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                  static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)));
    submitPrompt();

    const bool responseReceived = history.size() >= 2 && history.back().role == "assistant" &&
        !history.back().content.empty();
    const String testChatId = activeChatId;
    const cardputer::OperationResult cleanup = cardputer::deleteChat(testChatId);
    const cardputer::ChatDocumentResult restored = cardputer::loadChat(originalChatId);
    if (!restored.success) {
        Serial.println("E2ETEST result=failed stage=restore_chat");
        statusMessage = restored.error;
        render();
        return;
    }
    activeChatId = restored.chat.summary.id;
    activeChatTitle = restored.chat.summary.title;
    history = restored.chat.messages;
    activeResponse.clear();
    inputBuffer.clear();
    scrollOffset = 0;
    currentScreen = originalScreen;
    const cardputer::OperationResult listResult = refreshChatList();
    const bool passed = responseReceived && cleanup.success && listResult.success;
    statusMessage = passed ? String() : String("E2E cleanup or response verification failed");
    Serial.printf("E2ETEST result=%s response=%s cleanup=%s heap=%u largest_heap=%u stack_free=%u\n",
                  passed ? "pass" : "failed",
                  responseReceived ? "yes" : "no",
                  cleanup.success ? "yes" : "no",
                  static_cast<unsigned int>(ESP.getFreeHeap()),
                  static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                  static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)));
    render();
}

void handleVoiceInput()
{
    if (!cardputer::voiceSettingsAreComplete(settings)) {
        statusMessage = "Voice STT not configured; Fn+4 > Web setup";
        render();
        return;
    }
    if (!voiceStorageReady) {
        statusMessage = voiceStorageError;
        render();
        return;
    }
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        render();
        return;
    }
    cardputer::markOperation("voice_recording");
    const cardputer::VoiceRecordingResult recording = cardputer::recordVoiceWhileButtonHeld(
        [](std::uint32_t elapsedMs, std::uint16_t level) {
            cardputer::showVoiceRecording(
                elapsedMs, cardputer::maximumVoiceRecordingMs(), level);
        });
    cardputer::markOperation("idle");
    if (!recording.success) {
        statusMessage = recording.error;
        render();
        return;
    }
    Serial.printf("INFO event=voice_recording result=ok samples=%u peak=%u mean=%u\n",
                  static_cast<unsigned int>(recording.sampleCount),
                  static_cast<unsigned int>(recording.peakLevel),
                  static_cast<unsigned int>(recording.meanLevel));

    if (!sttCredentialsValidated) {
        cardputer::showBusyScreen("CHECKING STT", "Validating credentials...");
        const cardputer::OperationResult validation = cardputer::validateSttCredentials(settings);
        if (!validation.success) {
            const cardputer::OperationResult cleanup = cardputer::removeVoiceRecording();
            statusMessage = cleanup.success ? validation.error
                                            : validation.error + "; " + cleanup.error;
            Serial.println("ERROR event=stt_credentials result=failed");
            render();
            return;
        }
        sttCredentialsValidated = true;
        Serial.println("INFO event=stt_credentials result=ok");
    }

    cardputer::showBusyScreen("TRANSCRIBING", "Verified STT request...");
    cardputer::markOperation("stt_request");
    const cardputer::TranscriptionResult transcription =
        cardputer::transcribeVoiceRecording(settings);
    cardputer::markOperation("idle");
    if (!transcription.success) {
        const cardputer::OperationResult cleanup = cardputer::removeVoiceRecording();
        statusMessage = cleanup.success ? transcription.error
                                        : transcription.error + "; " + cleanup.error;
        String safeError = transcription.error;
        safeError.replace("\r", " ");
        safeError.replace("\n", " ");
        if (safeError.length() > 180) {
            safeError = safeError.substring(0, 180) + "...";
        }
        Serial.printf("ERROR event=voice_transcription result=failed detail=%s\n",
                      safeError.c_str());
        render();
        return;
    }

    const bool addSpace = !inputBuffer.empty() && inputBuffer.back() != ' ' && inputBuffer.back() != '\n';
    const std::size_t addedBytes = transcription.text.size() + (addSpace ? 1U : 0U);
    if (inputBuffer.size() + addedBytes > kMaximumInputBytes) {
        const cardputer::OperationResult cleanup = cardputer::removeVoiceRecording();
        statusMessage = cleanup.success
            ? String("Voice text exceeds the 1200-byte input limit")
            : String("Voice text exceeds the 1200-byte input limit; ") + cleanup.error;
        render();
        return;
    }
    if (addSpace) {
        inputBuffer += ' ';
    }
    inputBuffer += transcription.text;
    const cardputer::OperationResult cleanup = cardputer::removeVoiceRecording();
    if (cleanup.success) {
        setTransientStatus("Voice text ready; edit or press Enter", 3000);
    } else {
        statusMessage = cleanup.error;
    }
    Serial.printf("INFO event=voice_transcription result=ok text_bytes=%u audio_samples=%u\n",
                  static_cast<unsigned int>(transcription.text.size()),
                  static_cast<unsigned int>(recording.sampleCount));
    render();
}

void speakLastAssistantResponse()
{
    const auto message = std::find_if(history.rbegin(), history.rend(), [](const cardputer::Message& item) {
        return item.role == "assistant" && !item.content.empty();
    });
    if (message == history.rend()) {
        statusMessage = "No assistant response to speak";
        render();
        return;
    }
    if (!cardputer::ttsSettingsAreComplete(settings)) {
        statusMessage = "TTS is not configured; Fn+4 > Web setup";
        render();
        return;
    }
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        render();
        return;
    }
    cardputer::showBusyScreen("SPEAKING", "Generating multilingual audio...");
    cardputer::markOperation("tts_request");
    const cardputer::OperationResult result = cardputer::synthesizeAndPlaySpeech(settings, message->content);
    cardputer::markOperation("idle");
    if (result.success) {
        setTransientStatus("Spoken", 1500);
        Serial.println("INFO event=tts_playback result=ok source=manual");
    } else {
        statusMessage = result.error;
        Serial.println("ERROR event=tts_playback result=failed source=manual");
    }
    render();
}

std::vector<String> voiceMenuItems()
{
    const unsigned int volumePercent =
        (static_cast<unsigned int>(settings.ttsVolume) * 100U + 127U) / 255U;
    return {
        settings.ttsAutoPlay ? "Auto TTS: ON" : "Auto TTS: OFF",
        "TTS volume: " + String(volumePercent) + "%",
        "Configure voice APIs",
        "Back to carousel",
    };
}

std::vector<String> deviceMenuItems()
{
    return {
        "API and services setup",
        "Web console",
        "SSH terminal",
        "Diagnostics",
        "Back to carousel",
    };
}

std::vector<String> filesMenuItems()
{
    return {
        "Browse SD workspace",
        "Web file manager",
        "Back to carousel",
    };
}

std::vector<String> diagnosticsItems()
{
    return {
        "Firmware: " + String(kFirmwareVersion),
        "Board: Cardputer ADV",
        "Battery: " + (batteryLevel >= 0 ? String(batteryLevel) + "%" : String("unavailable")),
        "Wi-Fi: " + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected"),
        "microSD: " + String(fileWorkspaceReady ? "ready" : "unavailable"),
        "Chats: " + String(chats.size()),
        "Free heap: " + String(ESP.getFreeHeap()) + " B",
        "Crash journal: " + String(crashJournalReady ? "ready" : "unavailable"),
        "SSH storage: " + String(sshStorageReady ? "ready" : "unavailable"),
        "Previous op: " + cardputer::previousOperation(),
        "Reset reason: " + String(static_cast<int>(esp_reset_reason())),
    };
}

std::vector<String> workspaceFileItems()
{
    std::vector<String> items = {"+ New text file"};
    items.reserve(workspaceFiles.size() + 1);
    for (const auto& file : workspaceFiles) {
        items.push_back(file.name + "  " + String(file.size) + " B");
    }
    return items;
}

std::vector<String> fileActionItems()
{
    return {
        "View file",
        "Edit current chunk",
        "Find text...",
        "Find next",
        "Save bookmark here",
        "Open bookmark",
        "Save copy as...",
        "Rename...",
        "Delete file",
        "Back",
    };
}

std::vector<cardputer::CarouselCard> carouselCards()
{
    String networkSubtitle = "Choose 2.4 GHz Wi-Fi";
    if (WiFi.status() == WL_CONNECTED) {
        networkSubtitle = String("Connected: ") + settings.wifiSsid;
    } else if (!settings.wifiSsid.isEmpty()) {
        networkSubtitle = String("Connecting: ") + settings.wifiSsid;
    }
    return {
        {"CONVERSATIONS", "CHATS", "Open · New · Manage", 0x2F1C, cardputer::CarouselIcon::Chats},
        {"MODELS & TOOLS", "AI", settings.model, 0xA23F, cardputer::CarouselIcon::Ai},
        {"SPEECH", "VOICE", "STT · TTS · Volume", 0xFD20, cardputer::CarouselIcon::Voice},
        {"CONNECTIVITY", "NETWORK", networkSubtitle, 0xB7E6, cardputer::CarouselIcon::Network},
        {"WORKSPACE", "FILES", "Edit · Read · Export", 0x4DFF, cardputer::CarouselIcon::Files},
        {"SYSTEM", "DEVICE", "Battery · SD · Diagnostics", 0xFFE0, cardputer::CarouselIcon::Device},
        {"REFERENCE", "HELP", "Controls · About · Support", 0xF81F, cardputer::CarouselIcon::Help},
    };
}

void renderCarousel()
{
    cardputer::showCarousel(carouselCards(), carouselIndex,
                            WiFi.status() == WL_CONNECTED, fileWorkspaceReady,
                            batteryLevel, batteryCharging, menuStatus);
}

void openCarousel()
{
    carouselIndex = 0;
    menuStatus = "";
    currentScreen = Screen::MainCarousel;
    renderCarousel();
}

void moveCarousel(cardputer::CarouselDirection direction)
{
    const auto cards = carouselCards();
    if (cards.empty()) {
        cardputer::showFatalError("Main carousel has no cards");
        return;
    }
    const std::size_t previousIndex = carouselIndex;
    if (direction == cardputer::CarouselDirection::Next) {
        carouselIndex = (carouselIndex + 1) % cards.size();
    } else {
        carouselIndex = carouselIndex == 0 ? cards.size() - 1 : carouselIndex - 1;
    }
    menuStatus = "";
    cardputer::animateCarousel(cards, previousIndex, carouselIndex, direction,
                               WiFi.status() == WL_CONNECTED, fileWorkspaceReady,
                               batteryLevel, batteryCharging, menuStatus);
}

std::uint8_t nextTtsVolume(std::uint8_t currentVolume)
{
    if (currentVolume < 64) {
        return 64;
    }
    if (currentVolume < 128) {
        return 128;
    }
    if (currentVolume < 192) {
        return 192;
    }
    if (currentVolume < 255) {
        return 255;
    }
    return kTtsVolumeStep;
}

std::vector<String> controlsHelpItems()
{
    return {
        "ENTER  Send prompt",
        "Hold G0  Record voice",
        "BACKSPACE  Delete character",
        "CTRL+BACKSPACE  Clear draft",
        "FN+1  Chats",
        "FN+2  Select model",
        "FN+3  English / Russian",
        "FN+4  Main menu",
        "Menu: plain arrow keys",
        "FN+5  Older messages / up",
        "FN+6  Newer messages / down",
        "FN+7  New chat",
        "FN+8  Speak last answer",
        "Menu: ENTER  Open/select",
        "Menu: ESC-marked key  Back",
        "Chats: ENTER  Actions",
        "Chat instructions: ENTER save",
        "Chats: FN+DEL  Delete",
    };
}

std::vector<String> wifiPickerItems(const std::vector<cardputer::WifiNetwork>& networks)
{
    std::vector<String> items;
    items.reserve(networks.size());
    for (const auto& network : networks) {
        items.push_back(String(network.secured ? "[LOCK] " : "[OPEN] ") + network.ssid +
                        "  " + network.rssi + "dB");
    }
    return items;
}

void renderVoiceMenu()
{
    cardputer::showSelectionList("VOICE", voiceMenuItems(), voiceMenuIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderDeviceMenu()
{
    cardputer::showSelectionList("DEVICE", deviceMenuItems(), deviceMenuIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void openWebConsole()
{
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        menuStatus = statusMessage;
        currentScreen = Screen::DeviceMenu;
        renderDeviceMenu();
        return;
    }
    cardputer::markOperation("web_console");
    const cardputer::WebConsoleResult result = cardputer::runWebConsole(settings, activeChatId);
    cardputer::markOperation("idle");
    if (!result.success) {
        menuStatus = result.error;
    } else {
        const cardputer::OperationResult settingsResult = cardputer::loadSettings(settings);
        const cardputer::OperationResult activeResult = activateChat(result.activeChatId);
        const cardputer::OperationResult listResult = refreshChatList();
        if (!settingsResult.success) {
            menuStatus = settingsResult.error;
        } else if (!activeResult.success) {
            menuStatus = activeResult.error;
        } else if (!listResult.success) {
            menuStatus = listResult.error;
        } else {
            menuStatus = "Web console closed";
        }
    }
    currentScreen = Screen::DeviceMenu;
    renderDeviceMenu();
}

bool keyboardWordContains(const Keyboard_Class::KeysState& keys, char expected)
{
    return std::find(keys.word.begin(), keys.word.end(), expected) != keys.word.end();
}

bool confirmSshFingerprint(const cardputer::SshProfile& profile,
                           const String& keyType,
                           const String& fingerprint,
                           bool changed)
{
    std::vector<std::string> lines;
    lines.push_back(changed ? "WARNING: HOST KEY CHANGED" : "First connection to host");
    lines.push_back(std::string(profile.host.c_str()) + ":" +
                    std::to_string(profile.port));
    lines.push_back(std::string("Type: ") + keyType.c_str());
    const std::vector<std::string> fingerprintLines = cardputer::wrapUtf8Text(
        std::string(fingerprint.c_str()), 38);
    lines.insert(lines.end(), fingerprintLines.begin(), fingerprintLines.end());
    cardputer::showTextViewer("SSH HOST KEY", lines, 0,
                             "ENTER trust  ESC cancel");
    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            const Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
            if (keys.esc || keyboardWordContains(keys, '`')) {
                return false;
            }
            if (keys.enter) {
                return true;
            }
        }
        delay(5);
    }
}

cardputer::OperationResult runSshTerminal()
{
    cardputer::SshProfile profile;
    const cardputer::OperationResult loaded = cardputer::loadSshProfile(profile);
    if (!loaded.success) {
        return loaded;
    }
    if (!cardputer::sshProfileIsComplete(profile)) {
        return {false, "Configure SSH in the protected Web console first"};
    }
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED) {
        return {false, statusMessage.isEmpty() ? String("Wi-Fi is not connected") : statusMessage};
    }

    cardputer::showBusyScreen("SSH", "Connecting and checking host key...");
    cardputer::markOperation("ssh_handshake");
    cardputer::SshClient client;
    const cardputer::OperationResult connected = client.connect(profile, 60000);
    if (!connected.success) {
        cardputer::markOperation("idle");
        return connected;
    }
    const cardputer::SshTrustResult trust = cardputer::checkTrustedSshHost(
        profile.host, profile.port, client.fingerprint());
    if (!trust.success) {
        client.close();
        cardputer::markOperation("idle");
        return {false, trust.error};
    }
    if (!trust.found || !trust.matches) {
        const bool confirmed = confirmSshFingerprint(
            profile, client.hostKeyType(), client.fingerprint(), trust.found && !trust.matches);
        if (!confirmed) {
            client.close();
            cardputer::markOperation("idle");
            return {false, "SSH connection cancelled before trusting the host key"};
        }
        const cardputer::OperationResult trusted = cardputer::trustSshHost(
            profile.host, profile.port, client.fingerprint());
        if (!trusted.success) {
            client.close();
            cardputer::markOperation("idle");
            return trusted;
        }
    }

    cardputer::showBusyScreen("SSH", "Authenticating...");
    cardputer::markOperation("ssh_auth");
    const cardputer::OperationResult authenticated = client.authenticate(profile, 60000);
    if (!authenticated.success) {
        client.close();
        cardputer::markOperation("idle");
        return authenticated;
    }
    const cardputer::OperationResult terminalOpened = client.openTerminal(40, 8, 30000);
    if (!terminalOpened.success) {
        client.close();
        cardputer::markOperation("idle");
        return terminalOpened;
    }

    constexpr const char* scrollbackPath = "/assistant/ssh/terminal.log";
    constexpr const char* oldScrollbackPath = "/assistant/ssh/terminal.old.log";
    File existing = SD.open(scrollbackPath, FILE_READ);
    if (existing && existing.size() > 512U * 1024U) {
        existing.close();
        SD.remove(oldScrollbackPath);
        if (!SD.rename(scrollbackPath, oldScrollbackPath)) {
            client.close();
            cardputer::markOperation("idle");
            return {false, "Failed to rotate SSH terminal scrollback on microSD"};
        }
    } else if (existing) {
        existing.close();
    }
    File scrollback = SD.open(scrollbackPath, FILE_APPEND);
    if (!scrollback) {
        client.close();
        cardputer::markOperation("idle");
        return {false, "Failed to open SSH terminal scrollback on microSD"};
    }
    scrollback.println();
    scrollback.println(String("--- SSH session ") + profile.host + ":" + profile.port + " ---");
    scrollback.flush();

    cardputer::SshTerminalText terminal = {"", "", false};
    String terminalStatus = "ESC disconnect  SD scrollback";
    bool redraw = true;
    cardputer::markOperation("ssh_terminal");
    while (client.isOpen()) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            const Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
            if (keys.esc || keyboardWordContains(keys, '`')) {
                terminalStatus = "Disconnected by user";
                break;
            }
            std::vector<std::uint8_t> outbound;
            if (keys.ctrl && !keys.word.empty()) {
                const unsigned char character = static_cast<unsigned char>(keys.word.front());
                if (character >= '@' && character <= '_') {
                    outbound.push_back(static_cast<std::uint8_t>(character & 0x1F));
                } else if (character >= 'a' && character <= 'z') {
                    outbound.push_back(static_cast<std::uint8_t>(character - 'a' + 1));
                }
            } else {
                for (const char character : keys.word) {
                    outbound.push_back(static_cast<std::uint8_t>(character));
                }
            }
            if (keys.enter) {
                outbound.push_back('\r');
            }
            if (keys.backspace || keys.del) {
                outbound.push_back(0x7F);
            }
            if (keys.tab) {
                outbound.push_back('\t');
            }
            const char* arrow = keys.up ? "\x1B[A" : (keys.down ? "\x1B[B" :
                (keys.right ? "\x1B[C" : (keys.left ? "\x1B[D" : nullptr)));
            if (arrow != nullptr) {
                outbound.insert(outbound.end(), arrow, arrow + 3);
            }
            if (!outbound.empty()) {
                const cardputer::OperationResult written = client.write(
                    outbound.data(), outbound.size(), 5000);
                if (!written.success) {
                    terminalStatus = written.error;
                    break;
                }
            }
        }

        std::uint8_t incoming[256] = {};
        const int readBytes = client.read(incoming, sizeof(incoming));
        if (readBytes < 0) {
            terminalStatus = "SSH terminal read failed with code " + String(readBytes);
            break;
        }
        if (readBytes > 0) {
            const std::string previousText = terminal.text;
            terminal = cardputer::appendSshTerminalBytes(
                std::move(terminal), incoming, static_cast<std::size_t>(readBytes), 16384);
            if (terminal.text.size() >= previousText.size() &&
                terminal.text.compare(0, previousText.size(), previousText) == 0) {
                const std::string appended = terminal.text.substr(previousText.size());
                if (!appended.empty() &&
                    scrollback.write(reinterpret_cast<const std::uint8_t*>(appended.data()),
                                     appended.size()) != appended.size()) {
                    terminalStatus = "SSH scrollback write failed";
                    break;
                }
                scrollback.flush();
            }
            redraw = true;
        }
        if (redraw) {
            cardputer::showTextViewer(
                "SSH " + profile.host,
                cardputer::sshTerminalVisibleLines(terminal, 38, 8),
                0, terminalStatus);
            redraw = false;
        }
        delay(5);
    }
    scrollback.println();
    scrollback.println("--- session closed ---");
    scrollback.flush();
    scrollback.close();
    client.close();
    cardputer::markOperation("idle");
    return {true, terminalStatus};
}

void renderFilesMenu()
{
    cardputer::showSelectionList("FILES", filesMenuItems(), filesMenuIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderWorkspaceFileList()
{
    cardputer::showSelectionList("SD WORKSPACE", workspaceFileItems(), workspaceFileIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderFileActions()
{
    cardputer::showSelectionList(fileViewerName, fileActionItems(), fileActionsIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderFileViewer()
{
    const String position = String(fileViewerChunkOffset) + "/" +
        String(fileViewerTotalBytes) + " B";
    cardputer::showTextViewer(fileViewerName, fileViewerLines,
                              fileViewerFirstLine, position);
}

void renderFileEditor()
{
    const String position = String(fileEditorOffset) + "/" + String(fileViewerTotalBytes) + " B";
    cardputer::showFileEditor(fileViewerName, fileEditorInput, fileEditorCursor, keyboardLayout,
                             kFileEditorMaximumBytes, position, fileEditorStatus);
}

void renderFileFind()
{
    cardputer::showTextEditor("FIND IN FILE", fileFindInput, keyboardLayout, 128,
                             fileFindStatus, "Type search text",
                             "ENTER find  ESC cancel  Fn+3 lang");
}

void renderFileNameEntry()
{
    const String title = fileNameAction == FileNameAction::Create
        ? String("CREATE FILE")
        : (fileNameAction == FileNameAction::Copy ? String("SAVE COPY AS")
                                                   : String("RENAME FILE"));
    cardputer::showFilenameEntry(title, fileNameInput, fileNameStatus);
}

void renderDiagnostics()
{
    cardputer::showSelectionList("DIAGNOSTICS", diagnosticsItems(), diagnosticsIndex,
                                 "UP/DOWN scroll  ESC back");
}

void renderControlsHelp()
{
    cardputer::showSelectionList("CONTROLS HELP", controlsHelpItems(), controlsHelpIndex,
                                 "UP/DOWN scroll  ESC back");
}

void renderModelPicker()
{
    cardputer::showSelectionList("SELECT MODEL", availableModels, modelPickerIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderWifiPicker()
{
    cardputer::showSelectionList("SELECT WI-FI", wifiPickerItems(scannedWifiNetworks),
                                 wifiPickerIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void openModelPicker(Screen returnScreen)
{
    modelReturnScreen = returnScreen;
    if (availableModels.empty()) {
        refreshModels();
        if (availableModels.empty()) {
            if (returnScreen == Screen::MainCarousel) {
                menuStatus = statusMessage;
            }
            currentScreen = returnScreen;
            render();
            return;
        }
    }
    const auto selected = std::find(availableModels.begin(), availableModels.end(), settings.model);
    modelPickerIndex = selected == availableModels.end()
        ? 0
        : static_cast<std::size_t>(std::distance(availableModels.begin(), selected));
    currentScreen = Screen::ModelPicker;
    renderModelPicker();
}

void openVoiceMenu()
{
    voiceMenuIndex = 0;
    menuStatus = "";
    currentScreen = Screen::VoiceMenu;
    renderVoiceMenu();
}

void openDeviceMenu()
{
    deviceMenuIndex = 0;
    menuStatus = "";
    currentScreen = Screen::DeviceMenu;
    renderDeviceMenu();
}

void openFilesMenu()
{
    filesMenuIndex = 0;
    menuStatus = "";
    currentScreen = Screen::FilesMenu;
    renderFilesMenu();
}

void openWorkspaceFileList()
{
    const cardputer::WorkspaceFilesResult result = cardputer::listWorkspaceFiles();
    if (!result.success) {
        menuStatus = result.error;
        renderFilesMenu();
        return;
    }
    workspaceFiles = result.files;
    workspaceFileIndex = 0;
    menuStatus = workspaceFiles.empty() ? String("Workspace is empty") : String();
    currentScreen = Screen::WorkspaceFileList;
    renderWorkspaceFileList();
}

cardputer::OperationResult selectWorkspaceFileByName(const String& name)
{
    const cardputer::WorkspaceFilesResult result = cardputer::listWorkspaceFiles();
    if (!result.success) {
        return {false, result.error};
    }
    workspaceFiles = result.files;
    for (std::size_t index = 0; index < workspaceFiles.size(); ++index) {
        if (workspaceFiles[index].name == name) {
            workspaceFileIndex = index + 1;
            return {true, ""};
        }
    }
    return {false, "Workspace file was not found after the operation: " + name};
}

cardputer::OperationResult loadFileViewerChunk(std::uint32_t offset)
{
    const cardputer::WorkspaceChunkResult result = cardputer::readWorkspaceFileChunk(
        fileViewerName, offset, kFileViewerChunkBytes);
    if (!result.success) {
        return {false, result.error};
    }
    fileViewerContent = result.content;
    fileViewerLines = cardputer::wrapUtf8Text(fileViewerContent, 38);
    if (fileViewerLines.empty()) {
        fileViewerLines.push_back("");
    }
    fileViewerChunkOffset = result.offset;
    fileViewerNextOffset = result.nextOffset;
    fileViewerTotalBytes = result.totalBytes;
    fileViewerEof = result.eof;
    fileViewerFirstLine = 0;
    return {true, ""};
}

void openSelectedWorkspaceFile()
{
    if (workspaceFileIndex == 0 || workspaceFileIndex > workspaceFiles.size()) {
        menuStatus = "File selection is out of range";
        renderWorkspaceFileList();
        return;
    }
    fileViewerName = workspaceFiles[workspaceFileIndex - 1].name;
    lastFileFindQuery.clear();
    lastFileFindOffset = 0;
    fileViewerPreviousOffsets.clear();
    const cardputer::OperationResult result = loadFileViewerChunk(0);
    if (!result.success) {
        menuStatus = result.error;
        renderWorkspaceFileList();
        return;
    }
    menuStatus = "";
    fileActionsIndex = 0;
    currentScreen = Screen::FileActions;
    renderFileActions();
}

void beginFileEditor()
{
    fileEditorInput = fileViewerContent;
    fileEditorCursor = fileEditorInput.size();
    fileEditorOffset = fileViewerChunkOffset;
    fileEditorOriginalBytes = fileViewerNextOffset - fileViewerChunkOffset;
    fileEditorStatus = "";
    currentScreen = Screen::FileEditor;
    renderFileEditor();
}

void beginFileFind()
{
    fileFindInput = lastFileFindQuery;
    fileFindStatus = "";
    currentScreen = Screen::FileFind;
    renderFileFind();
}

void findFileText(const std::string& query, std::uint32_t startOffset)
{
    cardputer::markOperation("file_find");
    const cardputer::WorkspaceFindResult found = cardputer::findWorkspaceText(
        fileViewerName, query, startOffset);
    cardputer::markOperation("idle");
    if (!found.success) {
        menuStatus = found.error;
        currentScreen = Screen::FileActions;
        renderFileActions();
        return;
    }
    if (!found.found) {
        menuStatus = "Text not found after byte " + String(startOffset);
        currentScreen = Screen::FileActions;
        renderFileActions();
        return;
    }
    lastFileFindQuery = query;
    lastFileFindOffset = found.offset;
    fileViewerPreviousOffsets.clear();
    const cardputer::OperationResult loaded = loadFileViewerChunk(found.offset);
    if (!loaded.success) {
        menuStatus = loaded.error;
        currentScreen = Screen::FileActions;
        renderFileActions();
        return;
    }
    currentScreen = Screen::FileViewer;
    renderFileViewer();
}

void beginFileNameEntry(FileNameAction action, const String& sourceName)
{
    fileNameAction = action;
    fileNameSource = sourceName;
    fileNameInput = action == FileNameAction::Rename ? std::string(sourceName.c_str()) : std::string();
    fileNameStatus = "";
    currentScreen = Screen::FileNameEntry;
    renderFileNameEntry();
}

void completeFileNameEntry()
{
    if (!cardputer::isValidWorkspaceFilename(fileNameInput)) {
        fileNameStatus = "Use a valid text filename and extension";
        renderFileNameEntry();
        return;
    }
    const String destination = fileNameInput.c_str();
    cardputer::OperationResult result = {false, "File action was not selected"};
    if (fileNameAction == FileNameAction::Create) {
        result = cardputer::createWorkspaceFile(destination);
    } else if (fileNameAction == FileNameAction::Copy) {
        result = cardputer::copyWorkspaceFile(fileNameSource, destination);
    } else {
        if (destination == fileNameSource) {
            fileNameStatus = "Choose a different filename";
            renderFileNameEntry();
            return;
        }
        result = cardputer::renameWorkspaceFile(fileNameSource, destination);
    }
    if (!result.success) {
        fileNameStatus = result.error;
        renderFileNameEntry();
        return;
    }
    fileViewerName = destination;
    fileViewerPreviousOffsets.clear();
    result = selectWorkspaceFileByName(destination);
    if (result.success) {
        result = loadFileViewerChunk(0);
    }
    if (!result.success) {
        currentScreen = Screen::WorkspaceFileList;
        menuStatus = result.error;
        renderWorkspaceFileList();
        return;
    }
    fileNameInput.clear();
    fileNameSource = "";
    fileNameStatus = "";
    if (fileNameAction == FileNameAction::Create) {
        beginFileEditor();
        return;
    }
    fileActionsIndex = 0;
    menuStatus = fileNameAction == FileNameAction::Copy ? String("File copy created")
                                                         : String("File renamed");
    currentScreen = Screen::FileActions;
    renderFileActions();
}

void openWifiPicker(Screen returnScreen)
{
    wifiReturnScreen = returnScreen;
    cardputer::showBusyScreen("WI-FI", "Scanning 2.4 GHz...");
    cardputer::markOperation("wifi_scan");
    const cardputer::WifiScanResult scanResult = cardputer::scanWifiNetworks();
    cardputer::markOperation("idle");
    if (!scanResult.success) {
        menuStatus = scanResult.error;
        currentScreen = wifiReturnScreen;
        if (currentScreen == Screen::MainCarousel) {
            renderCarousel();
        } else {
            render();
        }
        Serial.println("WARN event=wifi_scan result=failed source=device_ui");
        return;
    }
    if (scanResult.networks.empty()) {
        menuStatus = "No 2.4 GHz networks found";
        currentScreen = wifiReturnScreen;
        if (currentScreen == Screen::MainCarousel) {
            renderCarousel();
        } else {
            render();
        }
        Serial.println("WARN event=wifi_scan result=empty source=device_ui");
        return;
    }
    scannedWifiNetworks = scanResult.networks;
    const auto selected = std::find_if(
        scannedWifiNetworks.begin(), scannedWifiNetworks.end(), [](const cardputer::WifiNetwork& network) {
            return network.ssid == settings.wifiSsid;
        });
    wifiPickerIndex = selected == scannedWifiNetworks.end()
        ? 0
        : static_cast<std::size_t>(std::distance(scannedWifiNetworks.begin(), selected));
    menuStatus = "";
    currentScreen = Screen::WifiPicker;
    renderWifiPicker();
    Serial.printf("INFO event=wifi_scan result=ok source=device_ui count=%u\n",
                  static_cast<unsigned int>(scannedWifiNetworks.size()));
}

void saveSelectedModel()
{
    if (modelPickerIndex >= availableModels.size()) {
        statusMessage = "Model selection is out of range";
        currentScreen = modelReturnScreen;
        if (currentScreen == Screen::MainCarousel) {
            menuStatus = statusMessage;
            renderCarousel();
        } else {
            render();
        }
        return;
    }
    const cardputer::OperationResult result = cardputer::saveModel(availableModels[modelPickerIndex]);
    if (!result.success) {
        menuStatus = result.error;
        renderModelPicker();
        return;
    }
    settings.model = availableModels[modelPickerIndex];
    setTransientStatus("Model: " + settings.model, 2500);
    currentScreen = modelReturnScreen;
    Serial.println("INFO event=model_update result=ok source=device_ui");
    if (currentScreen == Screen::MainCarousel) {
        menuStatus = "Model selected";
        renderCarousel();
    } else {
        render();
    }
}

void renderWifiPassword()
{
    if (wifiPickerIndex >= scannedWifiNetworks.size()) {
        currentScreen = Screen::WifiPicker;
        menuStatus = "Wi-Fi selection is out of range";
        renderWifiPicker();
        return;
    }
    cardputer::showPasswordEntry(scannedWifiNetworks[wifiPickerIndex].ssid,
                                 wifiPasswordInput.size(), menuStatus);
}

void connectSelectedWifi(const String& enteredPassword)
{
    if (wifiPickerIndex >= scannedWifiNetworks.size()) {
        menuStatus = "Wi-Fi selection is out of range";
        currentScreen = Screen::WifiPicker;
        renderWifiPicker();
        return;
    }
    const cardputer::WifiNetwork& network = scannedWifiNetworks[wifiPickerIndex];
    String password = enteredPassword;
    if (network.secured && password.isEmpty()) {
        if (network.ssid == settings.wifiSsid && !settings.wifiPassword.isEmpty()) {
            password = settings.wifiPassword;
        } else {
            menuStatus = "Password is required";
            currentScreen = Screen::WifiPassword;
            renderWifiPassword();
            return;
        }
    }
    cardputer::Settings candidate = settings;
    candidate.wifiSsid = network.ssid;
    candidate.wifiPassword = network.secured ? password : String("");
    cardputer::showBusyScreen("WI-FI", "Connecting...");
    cardputer::markOperation("wifi_connect");
    const cardputer::OperationResult connectResult = cardputer::connectToWifi(candidate);
    cardputer::markOperation("idle");
    if (!connectResult.success) {
        menuStatus = connectResult.error;
        currentScreen = network.secured ? Screen::WifiPassword : Screen::WifiPicker;
        if (currentScreen == Screen::WifiPassword) {
            renderWifiPassword();
        } else {
            renderWifiPicker();
        }
        Serial.println("WARN event=wifi_update result=connection_failed source=device_ui");
        return;
    }
    const cardputer::OperationResult saveResult = cardputer::saveSettings(candidate);
    if (!saveResult.success) {
        menuStatus = saveResult.error;
        currentScreen = Screen::WifiPassword;
        renderWifiPassword();
        Serial.println("ERROR event=wifi_update result=nvs_failed source=device_ui");
        return;
    }
    settings = candidate;
    wifiPasswordInput.clear();
    currentScreen = wifiReturnScreen;
    if (currentScreen == Screen::MainCarousel) {
        menuStatus = "Wi-Fi connected";
    } else {
        setTransientStatus("Wi-Fi connected", 2500);
    }
    Serial.println("INFO event=wifi_update result=ok source=device_ui");
    refreshModels();
    if (currentScreen == Screen::MainCarousel) {
        renderCarousel();
    } else {
        render();
    }
}

void selectWifiNetwork()
{
    if (wifiPickerIndex >= scannedWifiNetworks.size()) {
        menuStatus = "Wi-Fi selection is out of range";
        renderWifiPicker();
        return;
    }
    const cardputer::WifiNetwork& network = scannedWifiNetworks[wifiPickerIndex];
    wifiPasswordInput.clear();
    menuStatus = network.ssid == settings.wifiSsid && !settings.wifiPassword.isEmpty()
        ? "Blank ENTER uses saved password"
        : "Type the Wi-Fi password";
    if (!network.secured) {
        connectSelectedWifi("");
        return;
    }
    currentScreen = Screen::WifiPassword;
    renderWifiPassword();
}

void appendKeyboardWord(const std::vector<char>& word)
{
    for (const char character : word) {
        const std::string text = keyboardLayout == cardputer::KeyboardLayout::Russian
            ? cardputer::mapKeyToRussian(character)
            : std::string(1, character);
        if (inputBuffer.size() + text.size() > kMaximumInputBytes) {
            statusMessage = "Input limit reached (1200 bytes)";
            return;
        }
        inputBuffer += text;
    }
}

std::vector<Point2D_t> newKeyPresses(const std::vector<Point2D_t>& current,
                                     const std::vector<Point2D_t>& previous)
{
    std::vector<Point2D_t> result;
    for (const auto& key : current) {
        if (std::find(previous.begin(), previous.end(), key) == previous.end()) {
            result.push_back(key);
        }
    }
    return result;
}

std::vector<char> printableNewKeys(const std::vector<Point2D_t>& newPresses)
{
    std::vector<char> result;
    for (const auto& key : newPresses) {
        const char value = static_cast<char>(M5Cardputer.Keyboard.getKey(key));
        const auto unsignedValue = static_cast<unsigned char>(value);
        if (unsignedValue >= 0x20 && unsignedValue <= 0x7E) {
            result.push_back(value);
        }
    }
    return result;
}

bool newPressContains(const std::vector<Point2D_t>& newPresses, std::uint8_t expected)
{
    for (const auto& key : newPresses) {
        if (M5Cardputer.Keyboard.getKey(key) == expected) {
            return true;
        }
    }
    return false;
}

void handleKeyboard()
{
    const std::vector<Point2D_t> currentKeys = M5Cardputer.Keyboard.keyList();
    const std::vector<Point2D_t> newPresses = newKeyPresses(currentKeys, pressedKeys);
    pressedKeys = currentKeys;
    if (newPresses.empty()) {
        return;
    }
    auto& keys = M5Cardputer.Keyboard.keysState();
    const bool cancelPressed = keys.esc || (!keys.fn && newPressContains(newPresses, '`'));
    const bool upPressed = keys.f5 || keys.up || (!keys.fn && newPressContains(newPresses, ';'));
    const bool downPressed = keys.f6 || keys.down || (!keys.fn && newPressContains(newPresses, '.'));
    const bool leftPressed = keys.left || (!keys.fn && newPressContains(newPresses, ','));
    const bool rightPressed = keys.right || (!keys.fn && newPressContains(newPresses, '/'));
    const bool enterPressed = newPressContains(newPresses, KEY_ENTER);
    const bool deletePressed = keys.fn && keys.del;
    const bool clearDraftPressed = keys.ctrl && newPressContains(newPresses, KEY_BACKSPACE);
    const bool backspacePressed = (keys.fn && keys.del) ||
        newPressContains(newPresses, KEY_BACKSPACE);

    if (currentScreen == Screen::ChatList) {
        const std::size_t itemCount = chats.size() + 1;
        if (cancelPressed) {
            currentScreen = chatListReturnScreen;
            menuStatus = "";
            if (currentScreen == Screen::MainCarousel) {
                renderCarousel();
            } else {
                render();
            }
        } else if (upPressed) {
            chatListIndex = chatListIndex > 0 ? chatListIndex - 1 : 0;
            menuStatus = "";
            renderChatList();
        } else if (downPressed) {
            chatListIndex = std::min(chatListIndex + 1, itemCount - 1);
            menuStatus = "";
            renderChatList();
        } else if (deletePressed) {
            if (chatListIndex == 0) {
                menuStatus = "Select an existing chat to delete";
                renderChatList();
            } else {
                const cardputer::ChatSummary& selected = chats[chatListIndex - 1];
                deleteChatId = selected.id;
                deleteChatTitle = selected.title;
                deleteChatReturnScreen = Screen::ChatList;
                currentScreen = Screen::DeleteChatConfirm;
                cardputer::showConfirmation("DELETE CHAT", deleteChatTitle,
                                            "ENTER delete  ` cancel");
            }
        } else if (enterPressed) {
            if (chatListIndex > 0) {
                openChatActions(chats[chatListIndex - 1]);
            } else {
                const cardputer::OperationResult result = createAndActivateChat();
                if (!result.success) {
                    menuStatus = result.error;
                    renderChatList();
                    return;
                }
                currentScreen = Screen::Chat;
                menuStatus = "";
                setTransientStatus("New chat created", 2000);
                render();
            }
        }
        return;
    }

    if (currentScreen == Screen::ChatActions) {
        const std::size_t itemCount = chatActionItems().size();
        if (cancelPressed) {
            currentScreen = Screen::ChatList;
            menuStatus = "";
            renderChatList();
        } else if (upPressed) {
            chatActionsIndex = chatActionsIndex > 0 ? chatActionsIndex - 1 : 0;
            menuStatus = "";
            renderChatActions();
        } else if (downPressed) {
            chatActionsIndex = std::min(chatActionsIndex + 1, itemCount - 1);
            menuStatus = "";
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 0) {
            const cardputer::OperationResult result = activateChat(selectedChatId);
            if (!result.success) {
                menuStatus = result.error;
                renderChatActions();
            } else {
                currentScreen = Screen::Chat;
                setTransientStatus("Chat opened", 2000);
                render();
            }
        } else if (enterPressed && chatActionsIndex == 1) {
            const cardputer::ChatDocumentResult loaded = cardputer::loadChat(selectedChatId);
            if (!loaded.success) {
                menuStatus = loaded.error;
                renderChatActions();
            } else {
                instructionsInput = loaded.chat.instructions;
                instructionsStatus = "";
                currentScreen = Screen::ChatInstructions;
                renderChatInstructions();
            }
        } else if (enterPressed && chatActionsIndex == 2) {
            menuStatus = "Older turns are preserved on microSD";
            renderChatActions();
        } else if (enterPressed && (chatActionsIndex == 3 || chatActionsIndex == 4)) {
            const cardputer::ChatDocumentResult loaded = cardputer::loadChat(selectedChatId);
            if (!loaded.success) {
                menuStatus = loaded.error;
                renderChatActions();
                return;
            }
            cardputer::ChatDocument updated = loaded.chat;
            if (chatActionsIndex == 3) {
                updated.summary.pinned = !updated.summary.pinned;
            } else {
                updated.summary.archived = !updated.summary.archived;
                if (updated.summary.archived) {
                    updated.summary.pinned = false;
                }
            }
            cardputer::OperationResult result = cardputer::saveChat(updated);
            if (result.success) {
                result = refreshChatList();
            }
            if (!result.success) {
                menuStatus = result.error;
            } else {
                if (selectedChatId == activeChatId) {
                    activeChatPinned = updated.summary.pinned;
                    activeChatArchived = updated.summary.archived;
                }
                menuStatus = chatActionsIndex == 3
                    ? (updated.summary.pinned ? String("Chat pinned") : String("Chat unpinned"))
                    : (updated.summary.archived ? String("Chat archived")
                                                : String("Chat restored"));
            }
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 5) {
            const cardputer::ChatDocumentResult duplicated = cardputer::duplicateChat(selectedChatId);
            if (!duplicated.success) {
                menuStatus = duplicated.error;
                renderChatActions();
                return;
            }
            const cardputer::OperationResult refreshed = refreshChatList();
            if (!refreshed.success) {
                menuStatus = refreshed.error;
                renderChatActions();
                return;
            }
            selectedChatId = duplicated.chat.summary.id;
            selectedChatTitle = duplicated.chat.summary.title;
            menuStatus = "Chat duplicated";
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 6) {
            const String filename = "chat_" + selectedChatId + ".md";
            const cardputer::OperationResult exported = cardputer::exportChatToWorkspace(
                selectedChatId, filename);
            menuStatus = exported.success ? "Exported as " + filename : exported.error;
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 7) {
            deleteChatId = selectedChatId;
            deleteChatTitle = selectedChatTitle;
            deleteChatReturnScreen = Screen::ChatActions;
            currentScreen = Screen::DeleteChatConfirm;
            cardputer::showConfirmation("DELETE CHAT", deleteChatTitle,
                                        "ENTER delete  ESC cancel");
        } else if (enterPressed) {
            currentScreen = Screen::ChatList;
            menuStatus = "";
            renderChatList();
        }
        return;
    }

    if (currentScreen == Screen::ChatInstructions) {
        if (cancelPressed) {
            instructionsInput.clear();
            instructionsStatus = "";
            currentScreen = Screen::ChatActions;
            renderChatActions();
        } else if (keys.fn && keys.f3) {
            keyboardLayout = keyboardLayout == cardputer::KeyboardLayout::English
                ? cardputer::KeyboardLayout::Russian
                : cardputer::KeyboardLayout::English;
            instructionsStatus = keyboardLayout == cardputer::KeyboardLayout::English
                ? String("English layout")
                : String("Russian layout");
            renderChatInstructions();
        } else if (clearDraftPressed) {
            instructionsInput.clear();
            instructionsStatus = "Instructions cleared; ENTER to save";
            renderChatInstructions();
        } else if (backspacePressed) {
            if (!instructionsInput.empty()) {
                instructionsInput = cardputer::removeLastUtf8CodePoint(instructionsInput);
            }
            instructionsStatus = "";
            renderChatInstructions();
        } else if (enterPressed) {
            cardputer::ChatDocumentResult loaded = cardputer::loadChat(selectedChatId);
            if (!loaded.success) {
                instructionsStatus = loaded.error;
                renderChatInstructions();
                return;
            }
            loaded.chat.instructions = instructionsInput;
            const std::uint64_t updatedAt = currentChatTimestamp();
            if (updatedAt != 0) {
                loaded.chat.summary.updatedAt = updatedAt;
            }
            const cardputer::OperationResult saved = cardputer::saveChat(loaded.chat);
            if (!saved.success) {
                instructionsStatus = saved.error;
                renderChatInstructions();
                return;
            }
            if (selectedChatId == activeChatId) {
                activeChatInstructions = instructionsInput;
            }
            const cardputer::OperationResult listResult = refreshChatList();
            if (!listResult.success) {
                instructionsStatus = listResult.error;
                renderChatInstructions();
                return;
            }
            instructionsInput.clear();
            instructionsStatus = "";
            menuStatus = loaded.chat.instructions.empty()
                ? String("Instructions disabled")
                : String("Instructions saved");
            currentScreen = Screen::ChatActions;
            renderChatActions();
        } else if (!keys.fn && !keys.ctrl && !keys.alt && !keys.opt) {
            for (const char character : printableNewKeys(newPresses)) {
                const std::string text = keyboardLayout == cardputer::KeyboardLayout::Russian
                    ? cardputer::mapKeyToRussian(character)
                    : std::string(1, character);
                if (instructionsInput.size() + text.size() > cardputer::kMaximumChatInstructionsBytes) {
                    instructionsStatus = "Instruction limit: 2048 bytes";
                    break;
                }
                instructionsInput += text;
                instructionsStatus = "";
            }
            renderChatInstructions();
        }
        return;
    }

    if (currentScreen == Screen::DeleteChatConfirm) {
        if (cancelPressed) {
            currentScreen = deleteChatReturnScreen;
            deleteChatId = "";
            deleteChatTitle = "";
            if (currentScreen == Screen::ChatActions) {
                renderChatActions();
            } else {
                renderChatList();
            }
        } else if (enterPressed) {
            const bool deletingActive = deleteChatId == activeChatId;
            cardputer::OperationResult result = cardputer::deleteChat(deleteChatId);
            if (result.success) {
                result = refreshChatList();
            }
            if (result.success && deletingActive) {
                result = chats.empty() ? createAndActivateChat() : activateChat(chats.front().id);
            }
            if (!result.success) {
                currentScreen = Screen::ChatList;
                menuStatus = result.error;
                renderChatList();
            } else {
                currentScreen = Screen::Chat;
                setTransientStatus("Chat deleted", 2000);
                menuStatus = "";
                render();
            }
            deleteChatId = "";
            deleteChatTitle = "";
        }
        return;
    }

    if (currentScreen == Screen::MainCarousel) {
        if (cancelPressed) {
            currentScreen = Screen::Chat;
            menuStatus = "";
            render();
        } else if (leftPressed) {
            moveCarousel(cardputer::CarouselDirection::Previous);
        } else if (rightPressed) {
            moveCarousel(cardputer::CarouselDirection::Next);
        } else if (enterPressed) {
            if (carouselIndex == 0) {
                openChatList(Screen::MainCarousel);
            } else if (carouselIndex == 1) {
                openModelPicker(Screen::MainCarousel);
            } else if (carouselIndex == 2) {
                openVoiceMenu();
            } else if (carouselIndex == 3) {
                openWifiPicker(Screen::MainCarousel);
            } else if (carouselIndex == 4) {
                openFilesMenu();
            } else if (carouselIndex == 5) {
                openDeviceMenu();
            } else if (carouselIndex == 6) {
                controlsHelpIndex = 0;
                currentScreen = Screen::ControlsHelp;
                renderControlsHelp();
            } else {
                cardputer::showFatalError("Carousel selection is out of range");
            }
        }
        return;
    }

    if (currentScreen == Screen::VoiceMenu) {
        const std::size_t itemCount = voiceMenuItems().size();
        if (cancelPressed) {
            currentScreen = Screen::MainCarousel;
            menuStatus = "";
            renderCarousel();
        } else if (upPressed) {
            voiceMenuIndex = voiceMenuIndex > 0 ? voiceMenuIndex - 1 : 0;
            renderVoiceMenu();
        } else if (downPressed) {
            voiceMenuIndex = std::min(voiceMenuIndex + 1, itemCount - 1);
            renderVoiceMenu();
        } else if (enterPressed) {
            if (voiceMenuIndex == 0) {
                if (!settings.ttsAutoPlay && !cardputer::ttsSettingsAreComplete(settings)) {
                    menuStatus = "Configure TTS in Web setup first";
                    renderVoiceMenu();
                    return;
                }
                cardputer::Settings candidate = settings;
                candidate.ttsAutoPlay = !candidate.ttsAutoPlay;
                const cardputer::OperationResult result = cardputer::saveSettings(candidate);
                if (!result.success) {
                    menuStatus = result.error;
                } else {
                    settings = candidate;
                    menuStatus = settings.ttsAutoPlay ? "Auto TTS enabled" : "Auto TTS disabled";
                }
                renderVoiceMenu();
            } else if (voiceMenuIndex == 1) {
                cardputer::Settings candidate = settings;
                candidate.ttsVolume = nextTtsVolume(candidate.ttsVolume);
                const cardputer::OperationResult result = cardputer::saveSettings(candidate);
                if (!result.success) {
                    menuStatus = result.error;
                } else {
                    settings = candidate;
                    const unsigned int volumePercent =
                        (static_cast<unsigned int>(settings.ttsVolume) * 100U + 127U) / 255U;
                    menuStatus = "TTS volume set to " + String(volumePercent) + "%";
                }
                renderVoiceMenu();
            } else if (voiceMenuIndex == 2) {
                cardputer::markOperation("provisioning");
                cardputer::runProvisioningPortal(settings);
                cardputer::markOperation("idle");
            } else {
                currentScreen = Screen::MainCarousel;
                menuStatus = "";
                renderCarousel();
            }
        }
        return;
    }

    if (currentScreen == Screen::DeviceMenu) {
        const std::size_t itemCount = deviceMenuItems().size();
        if (cancelPressed) {
            currentScreen = Screen::MainCarousel;
            menuStatus = "";
            renderCarousel();
        } else if (upPressed) {
            deviceMenuIndex = deviceMenuIndex > 0 ? deviceMenuIndex - 1 : 0;
            renderDeviceMenu();
        } else if (downPressed) {
            deviceMenuIndex = std::min(deviceMenuIndex + 1, itemCount - 1);
            renderDeviceMenu();
        } else if (enterPressed) {
            if (deviceMenuIndex == 0) {
                cardputer::markOperation("provisioning");
                cardputer::runProvisioningPortal(settings);
                cardputer::markOperation("idle");
            } else if (deviceMenuIndex == 1) {
                openWebConsole();
            } else if (deviceMenuIndex == 2) {
                const cardputer::OperationResult result = runSshTerminal();
                menuStatus = result.error;
                currentScreen = Screen::DeviceMenu;
                renderDeviceMenu();
            } else if (deviceMenuIndex == 3) {
                diagnosticsIndex = 0;
                currentScreen = Screen::Diagnostics;
                renderDiagnostics();
            } else {
                currentScreen = Screen::MainCarousel;
                menuStatus = "";
                renderCarousel();
            }
        }
        return;
    }

    if (currentScreen == Screen::FilesMenu) {
        const std::size_t itemCount = filesMenuItems().size();
        if (cancelPressed) {
            currentScreen = Screen::MainCarousel;
            menuStatus = "";
            renderCarousel();
        } else if (upPressed) {
            filesMenuIndex = filesMenuIndex > 0 ? filesMenuIndex - 1 : 0;
            renderFilesMenu();
        } else if (downPressed) {
            filesMenuIndex = std::min(filesMenuIndex + 1, itemCount - 1);
            renderFilesMenu();
        } else if (enterPressed) {
            if (filesMenuIndex == 0) {
                openWorkspaceFileList();
            } else if (filesMenuIndex == 1) {
                openWebConsole();
            } else {
                currentScreen = Screen::MainCarousel;
                menuStatus = "";
                renderCarousel();
            }
        }
        return;
    }

    if (currentScreen == Screen::WorkspaceFileList) {
        const std::size_t itemCount = workspaceFiles.size() + 1;
        if (cancelPressed) {
            currentScreen = Screen::FilesMenu;
            menuStatus = "";
            renderFilesMenu();
        } else if (upPressed) {
            workspaceFileIndex = workspaceFileIndex > 0 ? workspaceFileIndex - 1 : 0;
            renderWorkspaceFileList();
        } else if (downPressed) {
            workspaceFileIndex = std::min(workspaceFileIndex + 1, itemCount - 1);
            renderWorkspaceFileList();
        } else if (enterPressed) {
            if (workspaceFileIndex == 0) {
                beginFileNameEntry(FileNameAction::Create, "");
            } else {
                openSelectedWorkspaceFile();
            }
        }
        return;
    }

    if (currentScreen == Screen::FileActions) {
        const std::size_t itemCount = fileActionItems().size();
        if (cancelPressed) {
            currentScreen = Screen::WorkspaceFileList;
            menuStatus = "";
            renderWorkspaceFileList();
        } else if (upPressed) {
            fileActionsIndex = fileActionsIndex > 0 ? fileActionsIndex - 1 : 0;
            menuStatus = "";
            renderFileActions();
        } else if (downPressed) {
            fileActionsIndex = std::min(fileActionsIndex + 1, itemCount - 1);
            menuStatus = "";
            renderFileActions();
        } else if (enterPressed) {
            if (fileActionsIndex == 0) {
                currentScreen = Screen::FileViewer;
                renderFileViewer();
            } else if (fileActionsIndex == 1) {
                beginFileEditor();
            } else if (fileActionsIndex == 2) {
                beginFileFind();
            } else if (fileActionsIndex == 3) {
                if (lastFileFindQuery.empty()) {
                    menuStatus = "Run Find text first";
                    renderFileActions();
                } else {
                    const std::uint32_t nextOffset = lastFileFindOffset +
                        static_cast<std::uint32_t>(lastFileFindQuery.size());
                    findFileText(lastFileFindQuery, nextOffset);
                }
            } else if (fileActionsIndex == 4) {
                const cardputer::OperationResult result = cardputer::saveWorkspaceBookmark(
                    fileViewerName, fileViewerChunkOffset);
                menuStatus = result.success
                    ? String("Bookmark saved at byte ") + String(fileViewerChunkOffset)
                    : result.error;
                renderFileActions();
            } else if (fileActionsIndex == 5) {
                const cardputer::WorkspaceBookmarkResult bookmark =
                    cardputer::loadWorkspaceBookmark(fileViewerName);
                if (!bookmark.success || !bookmark.found) {
                    menuStatus = bookmark.success ? String("No bookmark for this file")
                                                  : bookmark.error;
                    renderFileActions();
                } else {
                    fileViewerPreviousOffsets.clear();
                    const cardputer::OperationResult loaded = loadFileViewerChunk(bookmark.offset);
                    if (!loaded.success) {
                        menuStatus = loaded.error;
                        renderFileActions();
                    } else {
                        currentScreen = Screen::FileViewer;
                        renderFileViewer();
                    }
                }
            } else if (fileActionsIndex == 6) {
                beginFileNameEntry(FileNameAction::Copy, fileViewerName);
            } else if (fileActionsIndex == 7) {
                beginFileNameEntry(FileNameAction::Rename, fileViewerName);
            } else if (fileActionsIndex == 8) {
                deleteFileName = fileViewerName;
                currentScreen = Screen::DeleteFileConfirm;
                cardputer::showConfirmation("DELETE FILE", deleteFileName,
                                            "ENTER delete  ESC cancel");
            } else {
                currentScreen = Screen::WorkspaceFileList;
                menuStatus = "";
                renderWorkspaceFileList();
            }
        }
        return;
    }

    if (currentScreen == Screen::FileViewer) {
        if (cancelPressed) {
            currentScreen = Screen::FileActions;
            menuStatus = "";
            renderFileActions();
        } else if (upPressed) {
            if (fileViewerFirstLine > 0) {
                fileViewerFirstLine = fileViewerFirstLine > kFileViewerPageLines - 1
                    ? fileViewerFirstLine - (kFileViewerPageLines - 1)
                    : 0;
                renderFileViewer();
            } else if (!fileViewerPreviousOffsets.empty()) {
                const std::uint32_t previousOffset = fileViewerPreviousOffsets.back();
                fileViewerPreviousOffsets.pop_back();
                const cardputer::OperationResult result = loadFileViewerChunk(previousOffset);
                if (!result.success) {
                    currentScreen = Screen::WorkspaceFileList;
                    menuStatus = result.error;
                    renderWorkspaceFileList();
                } else {
                    fileViewerFirstLine = fileViewerLines.size() > kFileViewerPageLines
                        ? fileViewerLines.size() - kFileViewerPageLines
                        : 0;
                    renderFileViewer();
                }
            }
        } else if (downPressed) {
            if (fileViewerFirstLine + kFileViewerPageLines < fileViewerLines.size()) {
                fileViewerFirstLine = std::min(
                    fileViewerFirstLine + kFileViewerPageLines - 1,
                    fileViewerLines.size() - 1);
                renderFileViewer();
            } else if (!fileViewerEof) {
                const std::uint32_t currentOffset = fileViewerChunkOffset;
                const cardputer::OperationResult result = loadFileViewerChunk(fileViewerNextOffset);
                if (!result.success) {
                    currentScreen = Screen::WorkspaceFileList;
                    menuStatus = result.error;
                    renderWorkspaceFileList();
                } else {
                    fileViewerPreviousOffsets.push_back(currentOffset);
                    renderFileViewer();
                }
            }
        } else if (enterPressed) {
            beginFileEditor();
        }
        return;
    }

    if (currentScreen == Screen::FileEditor) {
        if (cancelPressed) {
            fileEditorInput.clear();
            fileEditorCursor = 0;
            fileEditorStatus = "";
            currentScreen = Screen::FileViewer;
            renderFileViewer();
        } else if (keys.fn && keys.f3) {
            keyboardLayout = keyboardLayout == cardputer::KeyboardLayout::English
                ? cardputer::KeyboardLayout::Russian
                : cardputer::KeyboardLayout::English;
            fileEditorStatus = keyboardLayout == cardputer::KeyboardLayout::English
                ? String("English layout")
                : String("Russian layout");
            renderFileEditor();
        } else if (keys.opt && leftPressed) {
            fileEditorCursor = cardputer::previousUtf8Boundary(
                fileEditorInput, fileEditorCursor);
            fileEditorStatus = "";
            renderFileEditor();
        } else if (keys.opt && rightPressed) {
            fileEditorCursor = cardputer::nextUtf8Boundary(
                fileEditorInput, fileEditorCursor);
            fileEditorStatus = "";
            renderFileEditor();
        } else if (clearDraftPressed) {
            fileEditorInput.clear();
            fileEditorCursor = 0;
            fileEditorStatus = "Chunk cleared; ENTER to save";
            renderFileEditor();
        } else if (backspacePressed) {
            if (fileEditorCursor > 0) {
                const std::size_t previous = cardputer::previousUtf8Boundary(
                    fileEditorInput, fileEditorCursor);
                fileEditorInput = cardputer::eraseUtf8Before(
                    fileEditorInput, fileEditorCursor);
                fileEditorCursor = previous;
            }
            fileEditorStatus = "";
            renderFileEditor();
        } else if (keys.fn && enterPressed) {
            if (fileEditorInput.size() >= kFileEditorMaximumBytes) {
                fileEditorStatus = "Editor limit: 4096 bytes";
            } else {
                fileEditorInput = cardputer::insertUtf8At(
                    fileEditorInput, fileEditorCursor, "\n");
                ++fileEditorCursor;
                fileEditorStatus = "";
            }
            renderFileEditor();
        } else if (enterPressed) {
            cardputer::markOperation("file_edit");
            const cardputer::OperationResult result = cardputer::replaceWorkspaceFileRange(
                fileViewerName, fileEditorOffset, fileEditorOriginalBytes, fileEditorInput);
            cardputer::markOperation("idle");
            if (!result.success) {
                fileEditorStatus = result.error;
                renderFileEditor();
                return;
            }
            const std::uint32_t savedOffset = fileEditorOffset;
            fileEditorInput.clear();
            fileEditorCursor = 0;
            fileEditorStatus = "";
            const cardputer::OperationResult loaded = loadFileViewerChunk(savedOffset);
            if (!loaded.success) {
                currentScreen = Screen::WorkspaceFileList;
                menuStatus = loaded.error;
                renderWorkspaceFileList();
                return;
            }
            currentScreen = Screen::FileViewer;
            menuStatus = "Chunk saved atomically";
            renderFileViewer();
        } else if (!keys.fn && !keys.ctrl && !keys.alt && !keys.opt) {
            for (const char character : printableNewKeys(newPresses)) {
                const std::string text = keyboardLayout == cardputer::KeyboardLayout::Russian
                    ? cardputer::mapKeyToRussian(character)
                    : std::string(1, character);
                if (fileEditorInput.size() + text.size() > kFileEditorMaximumBytes) {
                    fileEditorStatus = "Editor limit: 4096 bytes";
                    break;
                }
                fileEditorInput = cardputer::insertUtf8At(
                    fileEditorInput, fileEditorCursor, text);
                fileEditorCursor += text.size();
                fileEditorStatus = "";
            }
            renderFileEditor();
        }
        return;
    }

    if (currentScreen == Screen::FileNameEntry) {
        if (cancelPressed) {
            fileNameInput.clear();
            fileNameStatus = "";
            currentScreen = fileNameAction == FileNameAction::Create
                ? Screen::WorkspaceFileList
                : Screen::FileActions;
            render();
        } else if (backspacePressed) {
            if (!fileNameInput.empty()) {
                fileNameInput = cardputer::removeLastUtf8CodePoint(fileNameInput);
            }
            fileNameStatus = "";
            renderFileNameEntry();
        } else if (enterPressed) {
            completeFileNameEntry();
        } else if (!keys.fn && !keys.ctrl && !keys.alt && !keys.opt) {
            for (const char character : printableNewKeys(newPresses)) {
                if (fileNameInput.size() >= 48) {
                    fileNameStatus = "Filename limit: 48 bytes";
                    break;
                }
                fileNameInput += character;
                fileNameStatus = "";
            }
            renderFileNameEntry();
        }
        return;
    }

    if (currentScreen == Screen::FileFind) {
        if (cancelPressed) {
            fileFindStatus = "";
            currentScreen = Screen::FileActions;
            renderFileActions();
        } else if (keys.fn && keys.f3) {
            keyboardLayout = keyboardLayout == cardputer::KeyboardLayout::English
                ? cardputer::KeyboardLayout::Russian
                : cardputer::KeyboardLayout::English;
            fileFindStatus = keyboardLayout == cardputer::KeyboardLayout::English
                ? String("English layout")
                : String("Russian layout");
            renderFileFind();
        } else if (clearDraftPressed) {
            fileFindInput.clear();
            fileFindStatus = "";
            renderFileFind();
        } else if (backspacePressed) {
            if (!fileFindInput.empty()) {
                fileFindInput = cardputer::removeLastUtf8CodePoint(fileFindInput);
            }
            fileFindStatus = "";
            renderFileFind();
        } else if (enterPressed) {
            if (fileFindInput.empty()) {
                fileFindStatus = "Search text is required";
                renderFileFind();
            } else {
                const std::string query = fileFindInput;
                fileFindStatus = "";
                findFileText(query, 0);
            }
        } else if (!keys.fn && !keys.ctrl && !keys.alt && !keys.opt) {
            for (const char character : printableNewKeys(newPresses)) {
                const std::string text = keyboardLayout == cardputer::KeyboardLayout::Russian
                    ? cardputer::mapKeyToRussian(character)
                    : std::string(1, character);
                if (fileFindInput.size() + text.size() > 128) {
                    fileFindStatus = "Search limit: 128 bytes";
                    break;
                }
                fileFindInput += text;
                fileFindStatus = "";
            }
            renderFileFind();
        }
        return;
    }

    if (currentScreen == Screen::DeleteFileConfirm) {
        if (cancelPressed) {
            deleteFileName = "";
            currentScreen = Screen::FileActions;
            renderFileActions();
        } else if (enterPressed) {
            cardputer::markOperation("file_delete");
            const cardputer::OperationResult result = cardputer::deleteWorkspaceFile(deleteFileName);
            cardputer::markOperation("idle");
            deleteFileName = "";
            if (!result.success) {
                currentScreen = Screen::FileActions;
                menuStatus = result.error;
                renderFileActions();
                return;
            }
            openWorkspaceFileList();
            menuStatus = "File deleted";
            renderWorkspaceFileList();
        }
        return;
    }

    if (currentScreen == Screen::Diagnostics) {
        const std::size_t itemCount = diagnosticsItems().size();
        if (cancelPressed) {
            currentScreen = Screen::DeviceMenu;
            menuStatus = "";
            renderDeviceMenu();
        } else if (upPressed) {
            diagnosticsIndex = diagnosticsIndex > 0 ? diagnosticsIndex - 1 : 0;
            renderDiagnostics();
        } else if (downPressed) {
            diagnosticsIndex = std::min(diagnosticsIndex + 1, itemCount - 1);
            renderDiagnostics();
        }
        return;
    }

    if (currentScreen == Screen::ControlsHelp) {
        const std::size_t itemCount = controlsHelpItems().size();
        if (cancelPressed) {
            currentScreen = Screen::MainCarousel;
            menuStatus = "";
            renderCarousel();
        } else if (upPressed) {
            controlsHelpIndex = controlsHelpIndex > 0 ? controlsHelpIndex - 1 : 0;
            renderControlsHelp();
        } else if (downPressed) {
            controlsHelpIndex = std::min(controlsHelpIndex + 1, itemCount - 1);
            renderControlsHelp();
        }
        return;
    }

    if (currentScreen == Screen::ModelPicker) {
        if (cancelPressed) {
            currentScreen = modelReturnScreen;
            if (currentScreen == Screen::MainCarousel) {
                menuStatus = "Model selection cancelled";
                renderCarousel();
            } else {
                statusMessage = "Model selection cancelled";
                render();
            }
        } else if (upPressed) {
            modelPickerIndex = modelPickerIndex > 0 ? modelPickerIndex - 1 : 0;
            renderModelPicker();
        } else if (downPressed && !availableModels.empty()) {
            modelPickerIndex = std::min(modelPickerIndex + 1, availableModels.size() - 1);
            renderModelPicker();
        } else if (enterPressed) {
            saveSelectedModel();
        }
        return;
    }

    if (currentScreen == Screen::WifiPicker) {
        if (cancelPressed) {
            currentScreen = wifiReturnScreen;
            menuStatus = "";
            if (currentScreen == Screen::MainCarousel) {
                renderCarousel();
            } else {
                render();
            }
        } else if (upPressed) {
            wifiPickerIndex = wifiPickerIndex > 0 ? wifiPickerIndex - 1 : 0;
            menuStatus = "";
            renderWifiPicker();
        } else if (downPressed && !scannedWifiNetworks.empty()) {
            wifiPickerIndex = std::min(wifiPickerIndex + 1, scannedWifiNetworks.size() - 1);
            menuStatus = "";
            renderWifiPicker();
        } else if (enterPressed) {
            selectWifiNetwork();
        }
        return;
    }

    if (currentScreen == Screen::WifiPassword) {
        if (cancelPressed) {
            wifiPasswordInput.clear();
            menuStatus = "";
            currentScreen = Screen::WifiPicker;
            renderWifiPicker();
        } else if (backspacePressed) {
            if (!wifiPasswordInput.empty()) {
                wifiPasswordInput = cardputer::removeLastUtf8CodePoint(wifiPasswordInput);
            }
            menuStatus = "ENTER to connect";
            renderWifiPassword();
        } else if (enterPressed) {
            connectSelectedWifi(String(wifiPasswordInput.c_str()));
        } else if (!keys.fn && !keys.ctrl && !keys.alt && !keys.opt) {
            const std::vector<char> characters = printableNewKeys(newPresses);
            if (wifiPasswordInput.size() + characters.size() > kMaximumWifiPasswordBytes) {
                menuStatus = "Password limit: 63 bytes";
            } else {
                wifiPasswordInput.insert(wifiPasswordInput.end(), characters.begin(), characters.end());
                menuStatus = "ENTER to connect";
            }
            renderWifiPassword();
        }
        return;
    }

    if (keys.fn && keys.f1) {
        menuStatus = "";
        openChatList(Screen::Chat);
        return;
    } else if (keys.fn && keys.f2) {
        menuStatus = "";
        openModelPicker(Screen::Chat);
        return;
    } else if (keys.fn && keys.f3) {
        keyboardLayout = keyboardLayout == cardputer::KeyboardLayout::English
            ? cardputer::KeyboardLayout::Russian
            : cardputer::KeyboardLayout::English;
        setTransientStatus(keyboardLayout == cardputer::KeyboardLayout::English
                               ? String("English layout") : String("Russian layout"),
                           1800);
    } else if (keys.fn && keys.f4) {
        openCarousel();
        return;
    } else if (keys.fn && keys.f7) {
        const cardputer::OperationResult result = createAndActivateChat();
        if (result.success) {
            setTransientStatus("New chat created", 2000);
        } else {
            statusMessage = result.error;
        }
    } else if (keys.fn && keys.f8) {
        speakLastAssistantResponse();
        return;
    } else if (keys.fn && (keys.f5 || keys.up)) {
        const std::size_t maximum = cardputer::maximumChatScrollOffset(
            history, activeResponse, statusMessage);
        scrollOffset = std::min(scrollOffset + 4, maximum);
    } else if (keys.fn && (keys.f6 || keys.down)) {
        scrollOffset = scrollOffset > 4 ? scrollOffset - 4 : 0;
    } else if (clearDraftPressed) {
        inputBuffer.clear();
        setTransientStatus("Draft cleared", 1800);
    } else if (backspacePressed) {
        if (!inputBuffer.empty()) {
            inputBuffer = cardputer::removeLastUtf8CodePoint(inputBuffer);
        }
    } else if (enterPressed) {
        submitPrompt();
        return;
    } else if (!keys.fn && !keys.ctrl && !keys.alt && !keys.opt) {
        appendKeyboardWord(printableNewKeys(newPresses));
    }
    render();
}

}  // namespace

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.printf("BOOT firmware=%s reset_reason=%d\n",
                  kFirmwareVersion, static_cast<int>(esp_reset_reason()));
    Serial.printf("RUNTIME idf=%s\n", esp_get_idf_version());
    M5Cardputer.begin();
    const cardputer::OperationResult uiResult = cardputer::beginUi();
    if (!uiResult.success) {
        cardputer::showFatalError(uiResult.error);
        Serial.println("FATAL event=ui_initialization result=failed");
        while (true) {
            delay(1000);
        }
    }
    statusMessage = "Starting...";
    cardputer::showBusyScreen("CARDMIND", statusMessage);
    const bool isAdv = M5.getBoard() == m5::board_t::board_M5CardputerADV;
    Serial.printf("BOARD adv=%s type=%d\n", isAdv ? "yes" : "no", static_cast<int>(M5.getBoard()));
    if (!isAdv) {
        cardputer::showFatalError("Expected M5Stack Cardputer ADV; board detection returned another type");
        Serial.println("FATAL event=board_detection expected=cardputer_adv");
        while (true) {
            delay(1000);
        }
    }
    refreshBatteryStatus();
    Serial.printf("POWER battery=%d charging=%s\n",
                  batteryLevel, batteryCharging ? "yes" : "no");
    Serial.printf("SELFTEST result=%s\n", runPureSelfTest() ? "pass" : "fail");
    if (!runPureSelfTest()) {
        cardputer::showFatalError("Built-in UTF-8, layout, SSE, or Cyrillic font self-test failed");
        while (true) {
            delay(1000);
        }
    }
    const cardputer::OperationResult loadResult = cardputer::loadSettings(settings);
    if (!loadResult.success) {
        cardputer::showFatalError(loadResult.error);
        Serial.println("FATAL event=settings_load result=failed");
        while (true) {
            delay(1000);
        }
    }
    Serial.printf("CONFIG configured=%s\n", cardputer::settingsAreComplete(settings) ? "yes" : "no");
    if (!cardputer::settingsAreComplete(settings)) {
        cardputer::markOperation("provisioning");
        cardputer::runProvisioningPortal(settings);
        cardputer::markOperation("idle");
    }

    const cardputer::OperationResult voiceStorageResult = cardputer::initializeVoiceStorage();
    voiceStorageReady = voiceStorageResult.success;
    voiceStorageError = voiceStorageResult.success ? String() : voiceStorageResult.error;
    Serial.printf("VOICE_STORAGE result=%s\n", voiceStorageReady ? "ready" : "failed");

    const cardputer::OperationResult chatResult = voiceStorageReady
        ? initializeChats()
        : cardputer::OperationResult{false, voiceStorageError};
    chatStorageReady = chatResult.success;
    chatStorageError = chatResult.success ? String() : chatResult.error;
    Serial.printf("CHAT_STORAGE result=%s count=%u\n",
                  chatStorageReady ? "ready" : "failed",
                  static_cast<unsigned int>(chats.size()));

    const cardputer::OperationResult workspaceResult = chatStorageReady
        ? cardputer::initializeFileWorkspace()
        : cardputer::OperationResult{false, chatStorageError};
    fileWorkspaceReady = workspaceResult.success;
    fileWorkspaceError = workspaceResult.success ? String() : workspaceResult.error;
    Serial.printf("FILE_WORKSPACE result=%s\n", fileWorkspaceReady ? "ready" : "failed");

    const cardputer::OperationResult journalResult = voiceStorageReady
        ? cardputer::appendBootJournal(kFirmwareVersion)
        : cardputer::OperationResult{false, voiceStorageError};
    crashJournalReady = journalResult.success;
    crashJournalError = journalResult.success ? String() : journalResult.error;
    Serial.printf("CRASH_JOURNAL result=%s\n", crashJournalReady ? "ready" : "failed");

    const cardputer::OperationResult sshStorageResult = fileWorkspaceReady
        ? cardputer::initializeSshStorage()
        : cardputer::OperationResult{false, fileWorkspaceError};
    sshStorageReady = sshStorageResult.success;
    sshStorageError = sshStorageResult.success ? String() : sshStorageResult.error;
    Serial.printf("SSH_STORAGE result=%s\n", sshStorageReady ? "ready" : "failed");

    carouselIndex = 0;
    menuStatus = !fileWorkspaceReady
        ? fileWorkspaceError
        : (!crashJournalReady ? crashJournalError
                              : (sshStorageReady ? String() : sshStorageError));
    statusMessage = "";
    startupInProgress = false;
    currentScreen = Screen::MainCarousel;
    renderCarousel();
    beginConfiguredNetwork();
    Serial.println("READY");
}

void loop()
{
    M5Cardputer.update();
    const wl_status_t wifiStatus = WiFi.status();
    if (wifiStatus != lastWifiStatus) {
        lastWifiStatus = wifiStatus;
        Serial.printf("NETWORK status=%d\n", static_cast<int>(wifiStatus));
        if (currentScreen == Screen::MainCarousel) {
            renderCarousel();
        }
    }
    if (millis() - lastBatteryRefreshAt >= kBatteryRefreshIntervalMs) {
        const int previousBatteryLevel = batteryLevel;
        const bool previousBatteryCharging = batteryCharging;
        refreshBatteryStatus();
        if (batteryLevel != previousBatteryLevel || batteryCharging != previousBatteryCharging) {
            if (currentScreen == Screen::Chat) {
                render();
            } else if (currentScreen == Screen::MainCarousel) {
                renderCarousel();
            }
        }
    }
    updateTransientStatus();
    updateSerial();
    if (currentScreen == Screen::Chat && inputBuffer != persistedDraft &&
        millis() - lastDraftAutosaveAt >= kDraftAutosaveIntervalMs) {
        lastDraftAutosaveAt = millis();
        const cardputer::OperationResult saved = saveCurrentChat();
        if (!saved.success) {
            statusMessage = "Draft autosave failed: " + saved.error;
            render();
        }
    }
    if (currentScreen == Screen::Chat && M5Cardputer.BtnA.wasPressed()) {
        handleVoiceInput();
    } else {
        handleKeyboard();
    }
    delay(5);
}
