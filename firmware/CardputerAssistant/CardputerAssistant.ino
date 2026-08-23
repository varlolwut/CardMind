#include <M5Cardputer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp32-hal-cpu.h>

#include "src/api_client.h"
#include "src/app_types.h"
#include "src/audio_utils.h"
#include "src/backup_manager.h"
#include "src/chat_storage.h"
#include "src/crash_journal.h"
#include "src/document_reader.h"
#include "src/file_workspace.h"
#include "src/file_portal.h"
#include "src/offline_tools.h"
#include "src/ota_update.h"
#include "src/provisioning.h"
#include "src/python_mode.h"
#include "src/stt_client.h"
#include "src/storage.h"
#include "src/ssh_client.h"
#include "src/ssh_terminal.h"
#include "src/ssh_tool.h"
#include "src/text_utils.h"
#include "src/tts_client.h"
#include "src/ui.h"
#include "src/voice_input.h"
#include "src/web_search_client.h"
#include "src/web_console.h"
#include "src/wifi_networks.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

SET_LOOP_TASK_STACK_SIZE(16384);

namespace {

constexpr const char* kFirmwareVersion = "1.11.0";
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
    WebConsoleMenu,
    DeviceMenu,
    UtilitiesMenu,
    SystemMonitor,
    TimerMenu,
    Calculator,
    QrEntry,
    QrDisplay,
    RestoreBackupConfirm,
    FirmwareUpdateConfirm,
    FilesMenu,
    WorkspaceFileList,
    FileActions,
    FileViewer,
    FileEditor,
    FileFind,
    FileSpeechSelection,
    FileNameEntry,
    DeleteFileConfirm,
    Diagnostics,
    ControlsHelp,
    ModelPicker,
    ChatList,
    ChatActions,
    SearchSources,
    SearchSourceViewer,
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

enum class WorkspaceListMode {
    Browse,
    ImportChat,
};

cardputer::Settings settings;
std::vector<cardputer::Message> history;
std::vector<cardputer::ChatSummary> chats;
std::vector<String> availableModels;
std::string inputBuffer;
std::string persistedDraft;
std::uint32_t lastDraftAutosaveAt = 0;
std::string activeResponse;
std::string retryPrompt;
cardputer::KeyboardLayout keyboardLayout = cardputer::KeyboardLayout::English;
String statusMessage;
String transientStatusValue;
std::uint32_t transientStatusUntil = 0;
std::size_t scrollOffset = 0;
String serialInput;
std::vector<Point2D_t> pressedKeys;
std::uint32_t keyboardRepeatStartedAt = 0;
std::uint32_t lastKeyboardRepeatAt = 0;
Screen currentScreen = Screen::MainCarousel;
String activeChatId;
String activeChatTitle = "New chat";
std::string activeChatInstructions;
bool activeChatPinned = false;
bool activeChatArchived = false;
std::uint32_t activeChatArchivedMessageCount = 0;
bool activeChatSshToolsEnabled = false;
bool chatStorageReady = false;
String chatStorageError;
bool fileWorkspaceReady = false;
String fileWorkspaceError;
std::size_t chatListIndex = 0;
std::size_t chatActionsIndex = 0;
std::size_t searchSourceIndex = 0;
std::vector<cardputer::WebSearchSource> searchSources;
String searchSourcesQuery;
std::vector<std::string> searchSourceViewerLines;
std::size_t searchSourceViewerFirstLine = 0;
String selectedChatId;
String selectedChatTitle;
bool selectedChatSshToolsEnabled = false;
std::string instructionsInput;
String instructionsStatus;
String deleteChatId;
String deleteChatTitle;
Screen deleteChatReturnScreen = Screen::ChatList;
std::size_t voiceMenuIndex = 0;
std::size_t webConsoleMenuIndex = 0;
std::size_t deviceMenuIndex = 0;
std::size_t utilitiesMenuIndex = 0;
std::size_t timerMenuIndex = 0;
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
cardputer::DocumentReaderMode fileReaderMode = cardputer::DocumentReaderMode::Text;
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
std::size_t fileSpeechSelectionIndex = 0;
std::size_t fileSpeechSelectionStart = 0;
bool fileSpeechSelectionStarted = false;
String fileSpeechSelectionStatus;
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
Screen diagnosticsReturnScreen = Screen::DeviceMenu;
Screen workspaceListReturnScreen = Screen::FilesMenu;
WorkspaceListMode workspaceListMode = WorkspaceListMode::Browse;
int batteryLevel = -1;
bool batteryCharging = false;
std::uint32_t lastBatteryRefreshAt = 0;
bool startupInProgress = true;
wl_status_t lastWifiStatus = WL_IDLE_STATUS;
bool crashJournalReady = false;
String crashJournalError;
bool sshStorageReady = false;
String sshStorageError;
std::uint32_t lastUserActivityAt = 0;
bool displaySleeping = false;
cardputer::FirmwareUpdateInfo pendingFirmwareUpdate = {};
std::string calculatorInput;
String calculatorStatus;
std::string qrInput;
String qrStatus;
bool timerRunning = false;
std::uint32_t timerEndsAt = 0;
std::uint32_t timerDurationSeconds = 0;
std::uint32_t lastTimerRenderAt = 0;
std::uint32_t lastSystemMonitorRenderAt = 0;

void ensureNetworkReady();
void render();
void renderCarousel();
void renderChatActions();
void renderChatInstructions();
void renderSearchSources();
void renderChatList();
void renderControlsHelp();
void renderWebConsoleMenu();
void renderDeviceMenu();
void renderUtilitiesMenu();
void renderSystemMonitor();
void renderTimerMenu();
void renderCalculator();
void renderQrEntry();
void renderDiagnostics();
void renderFileActions();
void renderFileEditor();
void renderFileFind();
void renderFileSpeechSelection();
void renderFileNameEntry();
void renderFileViewer();
void renderFilesMenu();
void renderModelPicker();
void renderVoiceMenu();
void renderWifiPassword();
void renderWifiPicker();
void renderWorkspaceFileList();
void openSelectedWorkspaceFile();
void openWebConsole(Screen returnScreen);
cardputer::OperationResult runSshTerminal();
cardputer::OperationResult runSshTool();
cardputer::OperationResult runSshProfileStorageTest();
cardputer::OperationResult runSshSessionTest(bool testSftp);
cardputer::OperationResult runSshDemoTest();
cardputer::OperationResult saveAndApplyDeviceSettings(const cardputer::Settings& candidate);
bool keyboardWordContains(const Keyboard_Class::KeysState& keys, char expected);
bool confirmSshFingerprint(const cardputer::SshProfile& profile,
                           const String& keyType,
                           const String& fingerprint,
                           bool changed);
void submitPrompt();
void retryLastRequest();
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
        activeChatSshToolsEnabled,
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
    activeChatSshToolsEnabled = loaded.chat.sshToolsEnabled;
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
    activeChatSshToolsEnabled = false;
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
        String("LLM SSH access: ") + (selectedChatSshToolsEnabled ? "ON" : "OFF"),
        "Context: " + String(selected.messageCount) + "/" +
            String(cardputer::kMaximumStoredMessages) + " + " +
            String(selected.archivedMessageCount) + " archived",
        selected.id == activeChatId && !retryPrompt.empty()
            ? String("Retry failed request") : String("Retry unavailable"),
        "Latest search sources",
        selected.pinned ? "Unpin chat" : "Pin chat",
        selected.archived ? "Restore from archive" : "Archive chat",
        "Duplicate chat",
        "Export Markdown",
        "Export portable bundle",
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

void renderSearchSources()
{
    std::vector<String> items;
    items.reserve(searchSources.size());
    for (const auto& source : searchSources) {
        items.push_back(source.title);
    }
    cardputer::showSelectionList("SEARCH SOURCES", items, searchSourceIndex,
                                 menuStatus.isEmpty()
                                     ? String("UP/DOWN  ENTER view  ESC back")
                                     : menuStatus);
}

void openLatestSearchSources()
{
    const cardputer::WebSearchSourcesResult loaded = cardputer::loadLatestWebSearchSources();
    if (!loaded.success) {
        menuStatus = loaded.error;
        renderChatActions();
        return;
    }
    searchSources = loaded.sources;
    searchSourcesQuery = loaded.query;
    searchSourceIndex = 0;
    menuStatus = "";
    currentScreen = Screen::SearchSources;
    renderSearchSources();
}

void openChatActions(const cardputer::ChatSummary& chat)
{
    selectedChatId = chat.id;
    selectedChatTitle = chat.title;
    const cardputer::ChatDocumentResult loaded = cardputer::loadChat(chat.id);
    selectedChatSshToolsEnabled = loaded.success && loaded.chat.sshToolsEnabled;
    chatActionsIndex = 0;
    menuStatus = loaded.success ? String("") : loaded.error;
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
    case Screen::WebConsoleMenu:
        renderWebConsoleMenu();
        return;
    case Screen::DeviceMenu:
        renderDeviceMenu();
        return;
    case Screen::UtilitiesMenu:
        renderUtilitiesMenu();
        return;
    case Screen::SystemMonitor:
        renderSystemMonitor();
        return;
    case Screen::TimerMenu:
        renderTimerMenu();
        return;
    case Screen::Calculator:
        renderCalculator();
        return;
    case Screen::QrEntry:
        renderQrEntry();
        return;
    case Screen::QrDisplay:
        cardputer::showQrCode("QR CODE", String(qrInput.c_str()), "ESC back");
        return;
    case Screen::RestoreBackupConfirm:
        cardputer::showConfirmation("RESTORE BACKUP", "Replace chats and non-secret settings?",
                                    "ENTER restore  ESC cancel");
        return;
    case Screen::FirmwareUpdateConfirm:
        cardputer::showConfirmation(
            "FIRMWARE UPDATE",
            "Install " + pendingFirmwareUpdate.version + "? NVS and SD stay intact.",
            "ENTER install  ESC cancel");
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
    case Screen::FileSpeechSelection:
        renderFileSpeechSelection();
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
    case Screen::SearchSources:
        renderSearchSources();
        return;
    case Screen::SearchSourceViewer:
        cardputer::showTextViewer("SEARCH SOURCE", searchSourceViewerLines,
                                  searchSourceViewerFirstLine,
                                  "ESC back");
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
    const bool documentReader =
        cardputer::detectDocumentReaderMode("notes.md") ==
            cardputer::DocumentReaderMode::Markdown &&
        cardputer::formatDocumentChunk(cardputer::DocumentReaderMode::Csv,
                                        "name,\"one,two\"") == "name | one,two" &&
        cardputer::documentSpeechText(cardputer::DocumentReaderMode::HtmlSource,
                                      "<p>Hello</p>") == "Hello ";
    const cardputer::CalculationResult calculation = cardputer::calculateExpression(
        "(2 + 3) * 4");
    const bool calculator = calculation.success && calculation.value == 20.0 &&
        !cardputer::calculateExpression("4 / 0").success;
    return utf8Backspace && russianLayout && sse && wav && chatText && utf8Cursor &&
           documentReader && calculator && cardputer::fontSupportsCyrillic();
}

void printStatus()
{
    Serial.printf("STATUS version=%s board_adv=%s configured=%s voice_configured=%s search_configured=%s tts_configured=%s tts_auto=%s microsd=%s chats=%s chat_count=%u files=%s crash_journal=%s previous_operation=%s wifi=%s tls_time=%s battery=%d charging=%s history=%u heap=%u largest_heap=%u min_heap=%u stack_free=%u brightness=%u sleep_min=%u repeat_ms=%u power=%u cpu_mhz=%u reset_reason=%d\n",
                  kFirmwareVersion,
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
                  static_cast<unsigned int>(settings.displayBrightness),
                  static_cast<unsigned int>(settings.screenSleepMinutes),
                  static_cast<unsigned int>(settings.keyboardRepeatMs),
                  static_cast<unsigned int>(settings.powerProfile),
                  static_cast<unsigned int>(getCpuFrequencyMhz()),
                  static_cast<int>(esp_reset_reason()));
}

cardputer::ToolExecutionResult executeAvailableTool(
    const cardputer::ToolCall& call,
    const cardputer::CancelCallback& isCancelled)
{
    if (cardputer::isWebSearchToolName(call.name)) {
        return cardputer::executeWebSearchTool(settings, call, isCancelled);
    }
    if (cardputer::isWebFetchToolName(call.name)) {
        return cardputer::executeWebFetchTool(settings, call, isCancelled);
    }
    if (call.name == "list_files" || call.name == "read_file" ||
        call.name == "write_file" || call.name == "append_file") {
        return cardputer::executeWorkspaceTool(call);
    }
    if (cardputer::isSshToolName(call.name)) {
        if (!activeChatSshToolsEnabled) {
            return {false, "{\"ok\":false,\"error\":\"permission denied\"}",
                    "SSH tool access is disabled for this chat"};
        }
        return cardputer::executeSshTool(call, isCancelled);
    }
    return {
        false,
        "{\"ok\":false,\"error\":\"unsupported tool\"}",
        "API requested unsupported tool '" + String(call.name.c_str()) + "'",
    };
}

String serialSafeError(const String& value, std::size_t maximumLength)
{
    String error = value;
    error.replace("\r", " ");
    error.replace("\n", " ");
    if (error.length() > maximumLength) {
        error = error.substring(0, maximumLength) + "...";
    }
    return error.isEmpty() ? String("none") : error;
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
        settings, {"web-test", "web_search", "{\"query\":\"M5Stack official website\"}"},
        []() { return false; });
    const String safeError = serialSafeError(result.error, 180);
    Serial.printf("WEBTEST result=%s error=%s\n",
                  result.success ? "pass" : "failed", safeError.c_str());
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
        settings, {"fetch-test", "web_fetch", "{\"url\":\"https://m5stack.com/\"}"},
        []() { return false; });
    const String safeError = serialSafeError(result.error, 180);
    Serial.printf("FETCHTEST result=%s error=%s\n",
                  result.success ? "pass" : "failed", safeError.c_str());
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
        settings, testHistory, "", false, [](const std::string&) {},
        [&searchCalled](const cardputer::ToolCall& call) {
            if (cardputer::isWebSearchToolName(call.name)) {
                searchCalled = true;
                return cardputer::executeWebSearchTool(
                    settings, call, []() { return false; });
            }
            return executeAvailableTool(call, []() { return false; });
        }, []() { return false; });
    const String safeError = serialSafeError(result.error, 180);
    Serial.printf("SEARCHTEST result=%s search_called=%s response_bytes=%u error=%s\n",
                  result.success && searchCalled ? "pass" : "failed",
                  searchCalled ? "yes" : "no",
                  static_cast<unsigned int>(result.response.size()), safeError.c_str());
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
    Serial.println("STORAGETEST stage=chat_create");
    const cardputer::ChatDocumentResult created = cardputer::createChat("Storage test");
    if (!created.success) {
        Serial.println("STORAGETEST result=failed stage=chat_create");
        return;
    }
    cardputer::ChatDocument document = created.chat;
    document.messages = {{"user", "test"}, {"assistant", "OK"}};
    document.instructions = "Reply briefly.";
    document.sshToolsEnabled = true;
    Serial.println("STORAGETEST stage=chat_save");
    const cardputer::OperationResult saved = cardputer::saveChat(document);
    Serial.println("STORAGETEST stage=chat_load");
    const cardputer::ChatDocumentResult loaded = saved.success
        ? cardputer::loadChat(document.summary.id)
        : cardputer::ChatDocumentResult{false, {}, saved.error};
    const bool chatVerified = loaded.success && loaded.chat.messages.size() == 2 &&
        loaded.chat.messages[1].content == "OK" &&
        loaded.chat.instructions == "Reply briefly." && loaded.chat.sshToolsEnabled;
    Serial.println("STORAGETEST stage=chat_delete");
    const cardputer::OperationResult chatCleanup = cardputer::deleteChat(document.summary.id);
    if (!chatVerified || !chatCleanup.success) {
        Serial.println("STORAGETEST result=failed stage=chat_roundtrip");
        return;
    }

    const String testName = "firmware_storage_test.txt";
    Serial.println("STORAGETEST stage=file_write");
    const cardputer::ToolExecutionResult write = cardputer::executeWorkspaceTool(
        {"storage-write", "write_file",
         "{\"name\":\"firmware_storage_test.txt\",\"content\":\"OK\"}"});
    Serial.println("STORAGETEST stage=file_read");
    const cardputer::ToolExecutionResult read = write.success
        ? cardputer::executeWorkspaceTool(
              {"storage-read", "read_file",
               "{\"name\":\"firmware_storage_test.txt\",\"offset\":0,\"max_bytes\":12288}"})
        : cardputer::ToolExecutionResult{false, "", write.error};
    const bool fileVerified = read.success && read.output.find("\"content\":\"OK\"") != std::string::npos;
    const String testPath = cardputer::workspaceFilePath(testName);
    Serial.println("STORAGETEST stage=file_delete");
    const bool fileCleanup = SD.exists(testPath) && SD.remove(testPath);
    Serial.printf("STORAGETEST result=%s\n",
                  fileVerified && fileCleanup ? "pass" : "failed");
}

void runChatQolTest()
{
    const String exportName = "firmware_chat_export.md";
    const String bundleName = "firmware_chat_export.chat.jsonl";
    const String exportPath = cardputer::workspaceFilePath(exportName);
    const String bundlePath = cardputer::workspaceFilePath(bundleName);
    if (SD.exists(exportPath)) {
        SD.remove(exportPath);
    }
    if (SD.exists(bundlePath)) {
        SD.remove(bundlePath);
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
    source.sshToolsEnabled = true;
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
                           loaded.chat.draft != "unfinished" ||
                           !loaded.chat.sshToolsEnabled)) {
        result = {false, "Chat version-4 metadata round trip failed"};
    }
    const cardputer::ChatDocumentResult duplicated = result.success
        ? cardputer::duplicateChat(source.summary.id)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    if (result.success && (!duplicated.success || duplicated.chat.messages.size() != 2 ||
                           duplicated.chat.draft != "unfinished" ||
                           !duplicated.chat.sshToolsEnabled)) {
        result = {false, "Chat duplication verification failed"};
    }
    if (result.success) {
        result = cardputer::exportChatToWorkspace(source.summary.id, exportName);
    }
    if (result.success && !SD.exists(exportPath)) {
        result = {false, "Chat export file was not created"};
    }
    if (result.success) {
        result = cardputer::exportChatBundleToWorkspace(source.summary.id, bundleName);
    }
    const cardputer::ChatDocumentResult imported = result.success
        ? cardputer::importChatBundleFromWorkspace(bundleName)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    if (result.success && (!imported.success || imported.chat.messages.size() != 2 ||
                           imported.chat.summary.archivedMessageCount != 2 ||
                           imported.chat.instructions != "Be concise." ||
                           imported.chat.sshToolsEnabled)) {
        result = {false, "Portable chat import verification failed"};
    }
    if (duplicated.success) {
        const cardputer::OperationResult cleanup = cardputer::deleteChat(
            duplicated.chat.summary.id);
        if (result.success && !cleanup.success) {
            result = cleanup;
        }
    }
    if (imported.success) {
        const cardputer::OperationResult cleanup = cardputer::deleteChat(
            imported.chat.summary.id);
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
    if (SD.exists(bundlePath) && !SD.remove(bundlePath) && result.success) {
        result = {false, "Portable chat bundle cleanup failed"};
    }
    Serial.printf("CHATQOLTEST result=%s error=%s\n",
                  result.success ? "pass" : "failed",
                  result.success ? "none" : result.error.c_str());
}

void runFileWorkspaceEditTest()
{
    const String sourceName = "firmware_editor_test.txt";
    const String copyName = "firmware_editor_copy.txt";
    const String renamedName = "firmware_editor_renamed.txt";
    const String replacementName = "firmware_editor_upload.txt";
    const String sourcePath = cardputer::workspaceFilePath(sourceName);
    const String copyPath = cardputer::workspaceFilePath(copyName);
    const String renamedPath = cardputer::workspaceFilePath(renamedName);
    const String replacementPath = cardputer::workspaceFilePath(replacementName);
    if (SD.exists(sourcePath)) {
        SD.remove(sourcePath);
    }
    if (SD.exists(copyPath)) {
        SD.remove(copyPath);
    }
    if (SD.exists(renamedPath)) {
        SD.remove(renamedPath);
    }
    if (SD.exists(replacementPath)) {
        SD.remove(replacementPath);
    }
    Serial.println("FILETEST stage=create");
    cardputer::OperationResult result = cardputer::createWorkspaceFile(sourceName);
    if (result.success) {
        Serial.println("FILETEST stage=write_large_file");
        File file = SD.open(sourcePath, FILE_WRITE);
        if (!file) {
            result = {false, "Failed to open maximum-size workspace test file"};
        } else {
            std::uint8_t block[4096] = {};
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
        Serial.println("FILETEST stage=replace_range");
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
        Serial.println("FILETEST stage=validate_utf8");
        result = cardputer::validateWorkspaceFileUtf8(sourceName);
    }
    Serial.println("FILETEST stage=find_text");
    const cardputer::WorkspaceFindResult found = result.success
        ? cardputer::findWorkspaceText(sourceName, replacement, 0)
        : cardputer::WorkspaceFindResult{false, false, 0, result.error};
    if (result.success && (!found.success || !found.found || found.offset != editOffset)) {
        result = {false, "Workspace search verification failed"};
    }
    if (result.success) {
        Serial.println("FILETEST stage=save_bookmark");
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
        Serial.println("FILETEST stage=copy");
        result = cardputer::copyWorkspaceFile(sourceName, copyName);
    }
    if (result.success) {
        Serial.println("FILETEST stage=rename");
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
        Serial.println("FILETEST stage=replace_complete_file");
        result = cardputer::createWorkspaceFile(replacementName);
    }
    const std::string completeReplacement = "complete UTF-8 replacement: файл\n";
    if (result.success) {
        File replacementFile = SD.open(replacementPath, FILE_APPEND);
        if (!replacementFile ||
            replacementFile.write(
                reinterpret_cast<const std::uint8_t*>(completeReplacement.data()),
                completeReplacement.size()) != completeReplacement.size()) {
            result = {false, "Failed to write complete replacement test file"};
        }
        if (replacementFile) {
            replacementFile.flush();
            replacementFile.close();
        }
    }
    if (result.success) {
        result = cardputer::replaceWorkspaceFileWithTemporary(sourceName, replacementName);
    }
    const cardputer::WorkspaceChunkResult completeRead = result.success
        ? cardputer::readWorkspaceFileChunk(sourceName, 0, 128)
        : cardputer::WorkspaceChunkResult{false, "", 0, 0, 0, true, result.error};
    if (result.success && (!completeRead.success || !completeRead.eof ||
                           completeRead.content != completeReplacement)) {
        result = {false, "Complete workspace file replacement verification failed"};
    }
    if (result.success) {
        Serial.println("FILETEST stage=delete_source");
        result = cardputer::deleteWorkspaceFile(sourceName);
    }
    if (result.success) {
        Serial.println("FILETEST stage=delete_copy");
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
    if (SD.exists(replacementPath)) {
        SD.remove(replacementPath);
    }
    Serial.printf("FILETEST result=%s\n", result.success ? "pass" : "failed");
}

void runDeviceSettingsTest()
{
    const cardputer::Settings original = settings;
    cardputer::Settings candidate = settings;
    candidate.displayBrightness = 128;
    candidate.screenSleepMinutes = 1;
    candidate.keyboardRepeatMs = 75;
    candidate.powerProfile = 0;
    cardputer::OperationResult result = saveAndApplyDeviceSettings(candidate);
    cardputer::Settings loaded;
    if (result.success) {
        result = cardputer::loadSettings(loaded);
    }
    if (result.success &&
        (loaded.displayBrightness != candidate.displayBrightness ||
         loaded.screenSleepMinutes != candidate.screenSleepMinutes ||
         loaded.keyboardRepeatMs != candidate.keyboardRepeatMs ||
         loaded.powerProfile != candidate.powerProfile || getCpuFrequencyMhz() != 240)) {
        result = {false, "Device settings did not survive an NVS round trip"};
    }
    const cardputer::OperationResult restored = saveAndApplyDeviceSettings(original);
    if (result.success && !restored.success) {
        result = {false, "Device settings test passed but original settings could not be restored"};
    }
    Serial.printf("DEVICESETTINGSTEST result=%s error=%s\n",
                  result.success ? "pass" : "failed",
                  result.success ? "none" : result.error.c_str());
}

void runBackupTest()
{
    cardputer::OperationResult result = saveCurrentChat();
    if (result.success) {
        result = cardputer::createLocalBackup(settings, activeChatId);
    }
    String summary;
    if (result.success) {
        result = cardputer::localBackupSummary(summary);
    }
    String restoredId;
    if (result.success) {
        result = cardputer::restoreLocalBackup(settings, restoredId);
    }
    if (result.success) {
        result = refreshChatList();
    }
    if (result.success) {
        result = activateChat(restoredId);
    }
    Serial.printf("BACKUPTEST result=%s error=%s\n",
                  result.success ? "pass" : "failed",
                  result.success ? "none" : result.error.c_str());
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
        settings, testHistory, "", false, [](const std::string&) {},
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

void runApiProbe()
{
    ensureNetworkReady();
    String authority = settings.apiBaseUrl.substring(8);
    const int pathStart = authority.indexOf('/');
    if (pathStart >= 0) {
        authority = authority.substring(0, pathStart);
    }
    String host = authority;
    std::uint16_t port = 443;
    const int portSeparator = authority.lastIndexOf(':');
    if (portSeparator >= 0) {
        const long parsedPort = authority.substring(portSeparator + 1).toInt();
        if (parsedPort <= 0 || parsedPort > 65535) {
            Serial.printf("APIPROBE result=failed base=%s error=invalid_port\n",
                          settings.apiBaseUrl.c_str());
            return;
        }
        host = authority.substring(0, portSeparator);
        port = static_cast<std::uint16_t>(parsedPort);
    }
    IPAddress address;
    const bool resolved = !host.isEmpty() && WiFi.hostByName(host.c_str(), address) == 1;
    WiFiClient client;
    const bool connected = resolved && client.connect(address, port, 10000);
    client.stop();
    Serial.printf("APIPROBE result=%s base=%s host=%s port=%u dns=%s address=%s tcp=%s\n",
                  connected ? "pass" : "failed",
                  settings.apiBaseUrl.c_str(), host.c_str(),
                  static_cast<unsigned int>(port), resolved ? "pass" : "failed",
                  resolved ? address.toString().c_str() : "unresolved",
                  connected ? "pass" : "failed");
}

void handleSerialCommand(const String& command)
{
    if (command == "PING") {
        Serial.println("PONG");
        return;
    }
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
    if (command == "APIPROBE") {
        runApiProbe();
        return;
    }
    if (command.startsWith("APIBASEHEX")) {
        const String encoded = command.substring(10);
        if (encoded.isEmpty() || encoded.length() % 2 != 0 || encoded.length() > 360) {
            Serial.println("APIBASE result=failed error=invalid_hex_length");
            return;
        }
        String decoded;
        decoded.reserve(encoded.length() / 2);
        for (std::size_t index = 0; index < encoded.length(); index += 2) {
            const auto hexValue = [](char character) -> int {
                if (character >= '0' && character <= '9') {
                    return character - '0';
                }
                if (character >= 'A' && character <= 'F') {
                    return character - 'A' + 10;
                }
                return -1;
            };
            const int high = hexValue(encoded[index]);
            const int low = hexValue(encoded[index + 1]);
            if (high < 0 || low < 0) {
                Serial.println("APIBASE result=failed error=invalid_hex_character");
                return;
            }
            decoded += static_cast<char>((high << 4) | low);
        }
        while (decoded.endsWith("/")) {
            decoded.remove(decoded.length() - 1);
        }
        cardputer::Settings updated = settings;
        updated.apiBaseUrl = decoded;
        const cardputer::OperationResult result = cardputer::saveSettings(updated);
        if (result.success) {
            settings = updated;
        }
        Serial.printf("APIBASE result=%s error=%s\n",
                      result.success ? "pass" : "failed",
                      result.success ? "none" : result.error.c_str());
        return;
    }
    if (command == "CANCELTEST") {
        Serial.println("CANCELTEST stage=chat");
        const std::vector<cardputer::Message> testHistory = {{"user", "cancel"}};
        const cardputer::ChatResult result = cardputer::streamChatCompletion(
            settings, testHistory, "", [](const std::string&) {}, []() { return true; });
        Serial.println("CANCELTEST stage=search");
        const cardputer::ToolExecutionResult searchResult =
            cardputer::webSearchSettingsAreComplete(settings)
                ? cardputer::executeWebSearchTool(
                      settings,
                      {"cancel-search", "web_search", "{\"query\":\"cancel\"}"},
                      []() { return true; })
                : cardputer::ToolExecutionResult{false, "", "Web search canceled by user"};
        Serial.println("CANCELTEST stage=stt");
        const cardputer::TranscriptionResult sttResult =
            cardputer::voiceSettingsAreComplete(settings)
                ? cardputer::transcribeVoiceRecording(settings, []() { return true; })
                : cardputer::TranscriptionResult{false, {}, "STT request canceled by user"};
        Serial.println("CANCELTEST stage=tts");
        const cardputer::OperationResult ttsResult =
            cardputer::ttsSettingsAreComplete(settings)
                ? cardputer::synthesizeAndPlaySpeechControlled(
                      settings, "cancel", []() {
                          return cardputer::SpeechPlaybackCommand::Stop;
                      })
                : cardputer::OperationResult{false, "Speech synthesis canceled by user"};
        const bool passed = !result.success && result.error == "Request canceled by user" &&
            !searchResult.success && searchResult.error == "Web search canceled by user" &&
            !sttResult.success && sttResult.error == "STT request canceled by user" &&
            !ttsResult.success && ttsResult.error == "Speech synthesis canceled by user";
        Serial.printf("CANCELTEST result=%s\n",
                      passed ? "pass" : "failed");
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
    if (command == "DEVICESETTINGSTEST") {
        runDeviceSettingsTest();
        return;
    }
    if (command == "BACKUPTEST") {
        runBackupTest();
        return;
    }
    if (command == "OFFLINETEST") {
        const cardputer::CalculationResult calculation = cardputer::calculateExpression(
            "7 * (8 - 3) / 5");
        const bool calculationOk = calculation.success && calculation.value == 7.0;
        cardputer::showQrCode("QR SELF TEST", "https://example.com/cardmind", "Serial test");
        currentScreen = Screen::MainCarousel;
        renderCarousel();
        Serial.printf("OFFLINETEST result=%s\n", calculationOk ? "pass" : "failed");
        return;
    }
    if (command == "OTACHECK" || command == "OTADOWNLOADTEST" ||
        command == "OTAINSTALLTEST") {
        ensureNetworkReady();
        cardputer::markOperation("ota_check");
        cardputer::FirmwareUpdateInfo info =
            cardputer::checkLatestFirmwareUpdate(kFirmwareVersion);
        cardputer::markOperation("idle");
        if (!info.success) {
            Serial.printf("%s result=failed error=%s\n", command.c_str(), info.error.c_str());
            return;
        }
        if (command == "OTACHECK") {
            Serial.printf("OTACHECK result=pass latest=%s newer=%s bytes=%u python_recovery=%s\n",
                          info.version.c_str(), info.newerAvailable ? "yes" : "no",
                          static_cast<unsigned int>(info.assetBytes),
                          info.pythonRecoveryReady ? "yes" : "no");
            return;
        }
        if (command == "OTAINSTALLTEST" &&
            (!info.newerAvailable || !info.pythonRecoveryReady)) {
            Serial.println("OTAINSTALLTEST result=failed error=no_verified_newer_python_recovery_release");
            return;
        }
        if (command == "OTADOWNLOADTEST") {
            info.newerAvailable = true;
        }
        cardputer::markOperation(command == "OTAINSTALLTEST"
            ? "ota_install_test" : "ota_download_test");
        const cardputer::OperationResult downloaded = cardputer::downloadFirmwareUpdate(
            info, [](std::uint32_t, std::uint32_t) {}, []() { return false; });
        if (command == "OTAINSTALLTEST" && downloaded.success) {
            const cardputer::OperationResult installed = cardputer::installDownloadedFirmware(
                info, [](std::uint32_t, std::uint32_t) {}, []() { return false; });
            cardputer::markOperation("idle");
            const bool passed = installed.success;
            const String error = installed.error;
            Serial.printf("OTAINSTALLTEST result=%s target=%s error=%s\n",
                          passed ? "pass" : "failed", info.version.c_str(),
                          passed ? "none" : error.c_str());
            Serial.flush();
            if (passed) {
                delay(200);
                ESP.restart();
            }
            return;
        }
        const cardputer::OperationResult removed = downloaded.success
            ? cardputer::removeDownloadedFirmware()
            : cardputer::OperationResult{true, ""};
        cardputer::markOperation("idle");
        const bool passed = downloaded.success && removed.success;
        const String error = !downloaded.success ? downloaded.error : removed.error;
        Serial.printf("OTADOWNLOADTEST result=%s bytes=%u error=%s\n",
                      passed ? "pass" : "failed",
                      static_cast<unsigned int>(info.assetBytes),
                      passed ? "none" : error.c_str());
        return;
    }
    if (command == "PYTHONCHECK") {
        const cardputer::PythonModeStatus status = cardputer::inspectPythonMode();
        Serial.printf("PYTHONCHECK result=%s layout=%s image=%s cardmind_bytes=%u python_bytes=%u runtime_error=%s error=%s\n",
                      status.partitionLayoutReady && status.pythonImageReady ? "pass" : "failed",
                      status.partitionLayoutReady ? "yes" : "no",
                      status.pythonImageReady ? "yes" : "no",
                      static_cast<unsigned int>(status.cardMindPartitionBytes),
                      static_cast<unsigned int>(status.pythonPartitionBytes),
                      status.lastRuntimeError.isEmpty() ? "none" : status.lastRuntimeError.c_str(),
                      status.error.isEmpty() ? "none" : status.error.c_str());
        return;
    }
    if (command == "PYTHONBOOTTEST") {
        String password;
        cardputer::OperationResult result = cardputer::loadSetupAccessPointPassword(password);
        if (result.success) {
            result = cardputer::synchronizePythonModeSettings(settings, password);
        }
        if (result.success) {
            result = cardputer::activatePythonMode();
        }
        Serial.printf("PYTHONBOOTTEST result=%s error=%s\n",
                      result.success ? "restarting" : "failed",
                      result.success ? "none" : result.error.c_str());
        Serial.flush();
        if (result.success) {
            delay(200);
            ESP.restart();
        }
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
    if (command == "SSHPROFILETEST") {
        const cardputer::OperationResult result = runSshProfileStorageTest();
        Serial.printf("SSHPROFILETEST result=%s error=%s\n",
                      result.success ? "pass" : "failed",
                      result.success ? "none" : result.error.c_str());
        return;
    }
    if (command == "SSHSESSIONTEST" || command == "SFTPTEST") {
        ensureNetworkReady();
        cardputer::markOperation(command == "SFTPTEST" ? "sftp_test" : "ssh_session_test");
        const cardputer::OperationResult result = runSshSessionTest(command == "SFTPTEST");
        cardputer::markOperation("idle");
        Serial.printf("%s result=%s heap=%u stack_free=%u error=%s\n",
                      command.c_str(), result.success ? "pass" : "failed",
                      static_cast<unsigned int>(ESP.getFreeHeap()),
                      static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)),
                      result.success ? "none" : result.error.c_str());
        return;
    }
    if (command == "SSHDEMOTEST") {
        ensureNetworkReady();
        cardputer::markOperation("ssh_demo_test");
        const cardputer::OperationResult result = runSshDemoTest();
        cardputer::markOperation("idle");
        Serial.printf("SSHDEMOTEST result=%s heap=%u stack_free=%u error=%s\n",
                      result.success ? "pass" : "failed",
                      static_cast<unsigned int>(ESP.getFreeHeap()),
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
    if (command == "SEARCHCACHETEST") {
        const cardputer::WebSearchSourcesResult sources =
            cardputer::loadLatestWebSearchSources();
        Serial.printf("SEARCHCACHETEST result=%s count=%u error=%s\n",
                      sources.success && !sources.sources.empty() ? "pass" : "failed",
                      static_cast<unsigned int>(sources.sources.size()),
                      sources.success ? "none" : sources.error.c_str());
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
        openWebConsole(Screen::DeviceMenu);
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
    if (command == "MICTEST") {
        const cardputer::VoiceRecordingResult result = cardputer::probeMicrophone(2000);
        String safeError = result.error;
        safeError.replace("\r", " ");
        safeError.replace("\n", " ");
        Serial.printf("MICTEST result=%s samples=%u peak=%u mean=%u error=%s\n",
                      result.success ? "pass" : "failed",
                      static_cast<unsigned int>(result.sampleCount),
                      static_cast<unsigned int>(result.peakLevel),
                      static_cast<unsigned int>(result.meanLevel),
                      result.success ? "none" : safeError.c_str());
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
        String safeError = result.error;
        safeError.replace("\r", " ");
        safeError.replace("\n", " ");
        if (safeError.length() > 180) {
            safeError = safeError.substring(0, 180) + "...";
        }
        Serial.printf("TTSAUTH result=%s error=%s\n",
                      result.success ? "pass" : "failed",
                      result.success ? "none" : safeError.c_str());
        return;
    }
    if (command == "TTSTEST") {
        ensureNetworkReady();
        const cardputer::OperationResult result = cardputer::synthesizeAndPlaySpeech(
            settings, "Hello. This is the Cardputer language assistant.");
        String safeError = result.error;
        safeError.replace("\r", " ");
        safeError.replace("\n", " ");
        if (safeError.length() > 180) {
            safeError = safeError.substring(0, 180) + "...";
        }
        Serial.printf("TTSTEST result=%s error=%s\n",
                      result.success ? "pass" : "failed",
                      result.success ? "none" : safeError.c_str());
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
                Serial.flush();
            }
            serialInput = "";
        } else if (((character >= 'A' && character <= 'Z') ||
                    (character >= '0' && character <= '9')) &&
                   serialInput.length() < 380) {
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
    cardputer::HistoryFitResult pendingFit =
        cardputer::fitHistoryToActiveContext(pendingHistory);
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
        activeChatSshToolsEnabled,
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
    const bool sshToolsAvailable = activeChatSshToolsEnabled &&
        cardputer::sshToolIsAvailable();
    const bool useTools = useWorkspaceTools || webSearchAvailable || sshToolsAvailable;
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
              settings, history, activeChatInstructions, sshToolsAvailable, onText,
              [&isCancelled](const cardputer::ToolCall& call) {
                  statusMessage = "Tool: " + String(call.name.c_str());
                  render();
                  return executeAvailableTool(call, isCancelled);
              }, isCancelled)
        : cardputer::streamChatCompletion(
              settings, history, activeChatInstructions, onText, isCancelled);
    cardputer::markOperation("idle");
    if (!result.success) {
        activeResponse = result.response;
        retryPrompt = prompt;
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
    retryPrompt.clear();
    cardputer::HistoryFitResult finalFit = cardputer::fitHistoryToActiveContext(history);
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
    const cardputer::CancelCallback sttCancelled = []() {
        M5Cardputer.update();
        return M5Cardputer.Keyboard.keysState().esc;
    };
    const cardputer::TranscriptionResult transcription =
        cardputer::transcribeVoiceRecording(settings, sttCancelled);
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

std::string joinedViewerLines(std::size_t firstLine, std::size_t lastLine)
{
    if (fileViewerLines.empty() || firstLine >= fileViewerLines.size()) {
        return "";
    }
    const std::size_t boundedLast = std::min(lastLine, fileViewerLines.size() - 1);
    std::string text;
    for (std::size_t index = firstLine; index <= boundedLast; ++index) {
        if (!text.empty()) {
            text += '\n';
        }
        text += fileViewerLines[index];
    }
    return text;
}

std::size_t speechSegmentBytes(const std::string& text, std::size_t offset)
{
    constexpr std::size_t maximumBytes = 4500;
    const std::size_t remaining = text.size() - offset;
    std::size_t bytes = std::min(maximumBytes, remaining);
    while (bytes > 0 && !cardputer::isValidUtf8(text.substr(offset, bytes))) {
        --bytes;
    }
    if (bytes == 0) {
        return 0;
    }
    if (bytes < remaining) {
        const std::size_t separator = text.find_last_of("\n.?! ", offset + bytes - 1);
        if (separator != std::string::npos && separator >= offset + bytes / 2) {
            bytes = separator - offset + 1;
        }
    }
    return bytes;
}

cardputer::OperationResult playDocumentSpeechText(const std::string& text,
                                                  const String& source)
{
    if (text.empty()) {
        return {false, "Selected document text is empty"};
    }
    bool paused = false;
    bool stopped = false;
    const cardputer::SpeechPlaybackControl control = [&paused, &stopped, &source]() {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            const Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
            if (keys.esc || keyboardWordContains(keys, '`')) {
                stopped = true;
            } else if (keys.enter) {
                paused = !paused;
                cardputer::showBusyScreen(
                    "DOCUMENT TTS",
                    paused ? String("Paused - ENTER resume, ESC stop")
                           : source + " - ENTER pause, ESC stop");
            }
        }
        if (stopped) {
            return cardputer::SpeechPlaybackCommand::Stop;
        }
        return paused ? cardputer::SpeechPlaybackCommand::Pause
                      : cardputer::SpeechPlaybackCommand::Continue;
    };
    std::size_t offset = 0;
    while (offset < text.size()) {
        const std::size_t bytes = speechSegmentBytes(text, offset);
        if (bytes == 0) {
            return {false, "Document TTS could not split text at a valid UTF-8 boundary"};
        }
        cardputer::showBusyScreen("DOCUMENT TTS", source + " - ENTER pause, ESC stop");
        const cardputer::OperationResult spoken = cardputer::synthesizeAndPlaySpeechControlled(
            settings, text.substr(offset, bytes), control);
        if (!spoken.success || stopped || !spoken.error.isEmpty()) {
            return spoken;
        }
        offset += bytes;
    }
    return {true, ""};
}

cardputer::OperationResult prepareDocumentSpeech()
{
    if (!cardputer::ttsSettingsAreComplete(settings)) {
        return {false, "TTS is not configured; use Voice > Web setup"};
    }
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED) {
        return {false, statusMessage.isEmpty() ? String("Wi-Fi is not connected") : statusMessage};
    }
    if (std::time(nullptr) < 1700000000) {
        return {false, "TLS time is not synchronized"};
    }
    return {true, ""};
}

cardputer::OperationResult speakEntireDocument()
{
    std::uint32_t offset = 0;
    while (true) {
        const cardputer::WorkspaceChunkResult chunk = cardputer::readWorkspaceFileChunk(
            fileViewerName, offset, 3000);
        if (!chunk.success) {
            return {false, chunk.error};
        }
        const std::string speech = cardputer::documentSpeechText(fileReaderMode, chunk.content);
        const cardputer::OperationResult spoken = playDocumentSpeechText(
            speech, String("Reading ") + fileViewerName);
        if (!spoken.success || !spoken.error.isEmpty()) {
            return spoken;
        }
        if (chunk.eof) {
            return {true, ""};
        }
        offset = chunk.nextOffset;
    }
}

void retryLastRequest()
{
    if (retryPrompt.empty()) {
        menuStatus = "No failed request is available to retry";
        renderChatActions();
        return;
    }
    if (history.empty() || history.back().role != "user" ||
        history.back().content != retryPrompt) {
        menuStatus = "Retry context no longer matches the failed request";
        renderChatActions();
        return;
    }
    history.pop_back();
    inputBuffer = retryPrompt;
    activeResponse.clear();
    const cardputer::OperationResult saved = saveCurrentChat();
    if (!saved.success) {
        history.push_back({"user", retryPrompt});
        inputBuffer.clear();
        menuStatus = saved.error;
        renderChatActions();
        return;
    }
    currentScreen = Screen::Chat;
    menuStatus = "";
    submitPrompt();
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

String brightnessSettingLabel(std::uint8_t brightness)
{
    const unsigned int percent =
        (static_cast<unsigned int>(brightness) * 100U + 127U) / 255U;
    return String(percent) + "%";
}

String sleepSettingLabel(std::uint16_t minutes)
{
    return minutes == 0 ? String("Off") : String(minutes) + " min";
}

String keyboardRepeatSettingLabel(std::uint16_t intervalMs)
{
    if (intervalMs == 0) {
        return "Off";
    }
    if (intervalMs == 200) {
        return "Slow";
    }
    if (intervalMs == 125) {
        return "Normal";
    }
    return "Fast";
}

String powerProfileLabel(std::uint8_t profile)
{
    if (profile == 0) {
        return "Performance";
    }
    if (profile == 1) {
        return "Balanced";
    }
    return "Saver";
}

cardputer::OperationResult applyDisplayAndCpuSettings(const cardputer::Settings& candidate)
{
    M5Cardputer.Display.setBrightness(candidate.displayBrightness);
    const std::uint32_t frequency = candidate.powerProfile == 0
        ? 240U
        : (candidate.powerProfile == 1 ? 160U : 80U);
    return setCpuFrequencyMhz(frequency)
        ? cardputer::OperationResult{true, ""}
        : cardputer::OperationResult{false, "ESP32 rejected the selected CPU frequency"};
}

cardputer::OperationResult applyWifiPowerSetting(const cardputer::Settings& candidate)
{
    if (WiFi.getMode() == WIFI_OFF) {
        return {true, ""};
    }
    const wifi_ps_type_t requested = candidate.powerProfile == 2
        ? WIFI_PS_MIN_MODEM
        : WIFI_PS_NONE;
    if (WiFi.getSleep() == requested) {
        return {true, ""};
    }
    return WiFi.setSleep(requested)
        ? cardputer::OperationResult{true, ""}
        : cardputer::OperationResult{false, "ESP32 rejected the Wi-Fi power-save setting"};
}

cardputer::OperationResult saveAndApplyDeviceSettings(const cardputer::Settings& candidate)
{
    cardputer::OperationResult result = cardputer::saveSettings(candidate);
    if (!result.success) {
        return result;
    }
    settings = candidate;
    result = applyDisplayAndCpuSettings(settings);
    if (result.success) {
        result = applyWifiPowerSetting(settings);
    }
    return result;
}

std::vector<String> deviceMenuItems()
{
    return {
        "Brightness: " + brightnessSettingLabel(settings.displayBrightness),
        "Screen sleep: " + sleepSettingLabel(settings.screenSleepMinutes),
        "Keyboard repeat: " + keyboardRepeatSettingLabel(settings.keyboardRepeatMs),
        "Power: " + powerProfileLabel(settings.powerProfile),
        "Create local backup",
        "Restore local backup...",
        "Backup information",
        "API and services setup",
        "Firmware update",
        "Diagnostics",
        "Back to carousel",
    };
}

std::vector<String> filesMenuItems()
{
    return {
        "Browse SD workspace",
        "Import chat bundle",
        "Back to carousel",
    };
}

String timerStatusLabel()
{
    if (!timerRunning) {
        return "No active timer";
    }
    const std::int32_t remainingMs = static_cast<std::int32_t>(timerEndsAt - millis());
    const std::uint32_t remainingSeconds = remainingMs > 0
        ? (static_cast<std::uint32_t>(remainingMs) + 999U) / 1000U
        : 0U;
    char value[16] = {};
    std::snprintf(value, sizeof(value), "%02lu:%02lu",
                  static_cast<unsigned long>(remainingSeconds / 60U),
                  static_cast<unsigned long>(remainingSeconds % 60U));
    return String(value);
}

std::vector<String> utilitiesMenuItems()
{
    return {
        "Quick notes",
        "Checklist",
        "Timer: " + timerStatusLabel(),
        "Calculator",
        "QR display",
        "SSH tool",
        "System monitor",
        "Back to carousel",
    };
}

std::vector<String> webConsoleMenuItems()
{
    const String address = WiFi.status() == WL_CONNECTED
        ? String("Address: http://") + WiFi.localIP().toString()
        : String("Address: connect Wi-Fi first");
    const cardputer::PythonModeStatus python = cardputer::inspectPythonMode();
    const String pythonStatus = python.partitionLayoutReady && python.pythonImageReady
        ? String("Python workspace: ready")
        : String("Python workspace: not installed");
    return {
        "Open Web Console",
        address,
        "Session timeout: 15 min",
        pythonStatus,
        "Start Python workspace",
        "Configure API and Wi-Fi",
        "Back to carousel",
    };
}

std::vector<String> timerMenuItems()
{
    return {
        "Start 5 minutes",
        "Start 15 minutes",
        "Start 25 minutes",
        timerRunning ? String("Cancel active timer") : String("No timer to cancel"),
        "Back",
    };
}

std::vector<String> diagnosticsItems()
{
    return {
        "Firmware: " + String(kFirmwareVersion),
        "Board: Cardputer ADV",
        "Battery: " + (batteryLevel >= 0 ? String(batteryLevel) + "%" : String("unavailable")),
        "Brightness: " + brightnessSettingLabel(settings.displayBrightness),
        "Screen sleep: " + sleepSettingLabel(settings.screenSleepMinutes),
        "Keyboard repeat: " + keyboardRepeatSettingLabel(settings.keyboardRepeatMs),
        "Power: " + powerProfileLabel(settings.powerProfile),
        "CPU: " + String(getCpuFrequencyMhz()) + " MHz",
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
    std::vector<String> items = {
        workspaceListMode == WorkspaceListMode::ImportChat
            ? String("Choose a chat bundle below")
            : String("+ New text file")};
    items.reserve(workspaceFiles.size() + 1);
    for (const auto& file : workspaceFiles) {
        items.push_back(file.name + "  " + String(file.size) + " B");
    }
    return items;
}

std::vector<String> fileActionItems()
{
    const String mode = cardputer::documentReaderModeLabel(fileReaderMode).c_str();
    return {
        "View as " + mode,
        "Edit current page",
        "Read current page",
        "Read selected lines...",
        "Read entire document",
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
        {"BROWSER CONTROL", "WEB CONSOLE", "Chat · Files · Terminal", 0xFB4D, cardputer::CarouselIcon::Web},
        {"SYSTEM", "DEVICE", "Settings · Backup · Update", 0xFFE0, cardputer::CarouselIcon::Device},
        {"UTILITIES", "TOOLS", "Notes · SSH · Monitor", 0x07FF, cardputer::CarouselIcon::Tools},
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

std::uint8_t nextDisplayBrightness(std::uint8_t currentBrightness)
{
    if (currentBrightness < 64) {
        return 64;
    }
    if (currentBrightness < 128) {
        return 128;
    }
    if (currentBrightness < 192) {
        return 192;
    }
    if (currentBrightness < 255) {
        return 255;
    }
    return 64;
}

std::uint16_t nextScreenSleepMinutes(std::uint16_t currentMinutes)
{
    if (currentMinutes == 0) {
        return 1;
    }
    if (currentMinutes == 1) {
        return 5;
    }
    if (currentMinutes == 5) {
        return 10;
    }
    if (currentMinutes == 10) {
        return 30;
    }
    return 0;
}

std::uint16_t nextKeyboardRepeatMs(std::uint16_t currentIntervalMs)
{
    if (currentIntervalMs == 0) {
        return 200;
    }
    if (currentIntervalMs == 200) {
        return 125;
    }
    if (currentIntervalMs == 125) {
        return 75;
    }
    return 0;
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

void renderWebConsoleMenu()
{
    cardputer::showSelectionList("WEB CONSOLE", webConsoleMenuItems(), webConsoleMenuIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderDeviceMenu()
{
    cardputer::showSelectionList("DEVICE", deviceMenuItems(), deviceMenuIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderUtilitiesMenu()
{
    cardputer::showSelectionList("TOOLS", utilitiesMenuItems(),
                                 utilitiesMenuIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

String systemMonitorUptime()
{
    const std::uint32_t totalSeconds = millis() / 1000U;
    const std::uint32_t hours = totalSeconds / 3600U;
    const std::uint32_t minutes = (totalSeconds / 60U) % 60U;
    const std::uint32_t seconds = totalSeconds % 60U;
    char value[24] = {};
    std::snprintf(value, sizeof(value), "%luh %02lum %02lus",
                  static_cast<unsigned long>(hours),
                  static_cast<unsigned long>(minutes),
                  static_cast<unsigned long>(seconds));
    return String(value);
}

void renderSystemMonitor()
{
    refreshBatteryStatus();
    const String battery = batteryLevel >= 0
        ? String(batteryLevel) + "%" + (batteryCharging ? " charging" : " battery")
        : String("unavailable");
    const String wifi = WiFi.status() == WL_CONNECTED
        ? String(WiFi.RSSI()) + " dBm"
        : String("disconnected");
    const std::uint64_t totalBytes = fileWorkspaceReady ? SD.totalBytes() : 0;
    const std::uint64_t usedBytes = fileWorkspaceReady ? SD.usedBytes() : 0;
    const String storage = fileWorkspaceReady
        ? String(static_cast<unsigned long>(usedBytes / (1024U * 1024U))) + "/" +
          String(static_cast<unsigned long>(totalBytes / (1024U * 1024U))) + " MiB"
        : String("unavailable");
    const auto line = [](const String& value) { return std::string(value.c_str()); };
    const std::vector<std::string> lines = {
        line("Battery  " + battery),
        line("Wi-Fi    " + wifi),
        line("Heap     " + String(ESP.getFreeHeap() / 1024U) + " KiB"),
        line("Largest  " + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024U) + " KiB"),
        line("Stack    " + String(uxTaskGetStackHighWaterMark(nullptr)) + " B"),
        line("microSD  " + storage),
        line("CPU      " + String(getCpuFrequencyMhz()) + " MHz"),
        line("Uptime   " + systemMonitorUptime()),
    };
    cardputer::showTextViewer("SYSTEM MONITOR", lines, 0,
                             "Live every second  ESC back");
    lastSystemMonitorRenderAt = millis();
}

void renderTimerMenu()
{
    cardputer::showSelectionList("TIMER " + timerStatusLabel(), timerMenuItems(),
                                 timerMenuIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderCalculator()
{
    cardputer::showTextEditor("CALCULATOR", calculatorInput,
                             cardputer::KeyboardLayout::English, 96,
                             calculatorStatus, "2*(3+4)",
                             "ENTER calculate  ESC back");
}

void renderQrEntry()
{
    cardputer::showTextEditor("QR CONTENT", qrInput, keyboardLayout,
                             cardputer::kMaximumQrPayloadBytes,
                             qrStatus, "Text or URL",
                             "ENTER show  ESC back  Fn+3 lang");
}

void openWebConsole(Screen returnScreen)
{
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        menuStatus = statusMessage;
        currentScreen = returnScreen;
        render();
        return;
    }
    cardputer::markOperation("web_console");
    const cardputer::WebConsoleResult result = cardputer::runWebConsole(settings, activeChatId);
    cardputer::markOperation("idle");
    if (!result.success) {
        menuStatus = result.error;
    } else {
        const cardputer::OperationResult settingsResult = cardputer::loadSettings(settings);
        const cardputer::OperationResult runtimeResult = settingsResult.success
            ? applyDisplayAndCpuSettings(settings)
            : settingsResult;
        const cardputer::OperationResult activeResult = activateChat(result.activeChatId);
        const cardputer::OperationResult listResult = refreshChatList();
        if (!runtimeResult.success) {
            menuStatus = runtimeResult.error;
        } else if (!activeResult.success) {
            menuStatus = activeResult.error;
        } else if (!listResult.success) {
            menuStatus = listResult.error;
        } else {
            menuStatus = "Web console closed";
        }
    }
    currentScreen = returnScreen;
    render();
}

bool keyboardWordContains(const Keyboard_Class::KeysState& keys, char expected)
{
    return std::find(keys.word.begin(), keys.word.end(), expected) != keys.word.end();
}

void waitForModalKeyRelease()
{
    while (!M5Cardputer.Keyboard.keyList().empty()) {
        M5Cardputer.update();
        delay(5);
    }
}

int modalSelection(const String& title, const std::vector<String>& items,
                   std::size_t initialIndex, const String& footer)
{
    if (items.empty()) {
        return -1;
    }
    std::size_t index = std::min(initialIndex, items.size() - 1);
    waitForModalKeyRelease();
    cardputer::showSelectionList(title, items, index, footer);
    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            const Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
            if (keys.esc || keyboardWordContains(keys, '`')) {
                waitForModalKeyRelease();
                return -1;
            }
            const bool plainUp = !keys.fn && keyboardWordContains(keys, ';');
            const bool plainDown = !keys.fn && keyboardWordContains(keys, '.');
            if (keys.up || keys.f5 || plainUp) {
                index = index == 0 ? items.size() - 1 : index - 1;
                cardputer::showSelectionList(title, items, index, footer);
            } else if (keys.down || keys.f6 || plainDown) {
                index = (index + 1) % items.size();
                cardputer::showSelectionList(title, items, index, footer);
            } else if (keys.enter) {
                waitForModalKeyRelease();
                return static_cast<int>(index);
            }
        }
        delay(5);
    }
}

bool modalTextInput(const String& title, const String& label,
                    const std::string& initialValue, std::size_t maximumBytes,
                    bool secret, std::string& result)
{
    std::string value = initialValue;
    waitForModalKeyRelease();
    while (true) {
        if (secret) {
            cardputer::showSecretEntry(title, label, value.size(), "",
                                       "ENTER save  ESC cancel");
        } else {
            cardputer::showTextEditor(title, value, cardputer::KeyboardLayout::English,
                                     maximumBytes, "", label,
                                     "ENTER save  ESC cancel");
        }
        M5Cardputer.update();
        if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
            delay(5);
            continue;
        }
        const Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
        if (keys.esc || keyboardWordContains(keys, '`')) {
            waitForModalKeyRelease();
            return false;
        }
        if (keys.enter) {
            result = value;
            waitForModalKeyRelease();
            return true;
        }
        if (keys.backspace || keys.del) {
            value = cardputer::removeLastUtf8CodePoint(value);
            continue;
        }
        if (!keys.ctrl && !keys.alt && !keys.opt && !keys.fn) {
            for (const char character : keys.word) {
                const unsigned char byte = static_cast<unsigned char>(character);
                if (byte >= 0x20 && byte <= 0x7E && value.size() < maximumBytes) {
                    value.push_back(character);
                }
            }
        }
    }
}

bool editSshProfile(cardputer::SshProfile current, cardputer::SshProfile& result)
{
    std::string value;
    if (!modalTextInput("SSH PROFILE", "Profile name", current.name.c_str(), 32,
                        false, value)) return false;
    current.name = value.c_str();
    if (!modalTextInput("SSH HOST", "Hostname or IPv4", current.host.c_str(), 253,
                        false, value)) return false;
    current.host = value.c_str();
    if (!modalTextInput("SSH PORT", "1-65535", std::to_string(current.port), 5,
                        false, value)) return false;
    std::uint32_t port = 0;
    for (const char character : value) {
        if (character < '0' || character > '9') {
            cardputer::showTextViewer("SSH PORT ERROR", {"Port must contain digits only."},
                                     0, "Returning to profiles...");
            delay(1500);
            return false;
        }
        port = port * 10U + static_cast<std::uint32_t>(character - '0');
    }
    if (port == 0 || port > 65535) {
        cardputer::showTextViewer("SSH PORT ERROR", {"Port must be between 1 and 65535."},
                                 0, "Returning to profiles...");
        delay(1500);
        return false;
    }
    current.port = static_cast<std::uint16_t>(port);
    if (!modalTextInput("SSH USER", "Username", current.username.c_str(), 64,
                        false, value)) return false;
    current.username = value.c_str();
    const int auth = modalSelection("SSH AUTH", {"Password", "Private key"},
                                    current.authMode == cardputer::SshAuthMode::PrivateKey ? 1 : 0,
                                    "UP/DOWN  ENTER  ESC cancel");
    if (auth < 0) return false;
    current.authMode = auth == 1 ? cardputer::SshAuthMode::PrivateKey
                                 : cardputer::SshAuthMode::Password;
    if (current.authMode == cardputer::SshAuthMode::Password) {
        if (!modalTextInput("SSH PASSWORD", current.host, current.password.c_str(), 192,
                            true, value)) return false;
        current.password = value.c_str();
    } else {
        if (!cardputer::sshPrivateKeyIsInstalled()) {
            cardputer::showTextViewer("SSH KEY", {"No private key installed.",
                                                  "Use SSH > Install key first."},
                                     0, "ESC/ENTER close");
            delay(1800);
            return false;
        }
        if (!modalTextInput("KEY PASSPHRASE", "Empty if unencrypted",
                            current.privateKeyPassphrase.c_str(), 192, true, value)) return false;
        current.privateKeyPassphrase = value.c_str();
    }
    result = current;
    return true;
}

cardputer::OperationResult connectTrustedSsh(const cardputer::SshProfile& profile,
                                             cardputer::SshClient& client)
{
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED) {
        return {false, statusMessage.isEmpty() ? String("Wi-Fi is not connected") : statusMessage};
    }
    cardputer::showBusyScreen("SSH", "Connecting and checking host key...");
    cardputer::markOperation("ssh_handshake");
    cardputer::OperationResult result = client.connect(profile, 60000);
    if (!result.success) return result;
    const cardputer::SshTrustResult trust = cardputer::checkTrustedSshHost(
        profile.host, profile.port, client.fingerprint());
    if (!trust.success) {
        client.close();
        return {false, trust.error};
    }
    if (!trust.found || !trust.matches) {
        if (!confirmSshFingerprint(profile, client.hostKeyType(), client.fingerprint(),
                                   trust.found && !trust.matches)) {
            client.close();
            return {false, "SSH connection cancelled before trusting the host key"};
        }
        result = cardputer::trustSshHost(profile.host, profile.port, client.fingerprint());
        if (!result.success) {
            client.close();
            return result;
        }
    }
    cardputer::showBusyScreen("SSH", "Authenticating...");
    cardputer::markOperation("ssh_auth");
    result = client.authenticate(profile, 60000);
    if (!result.success) client.close();
    return result;
}

cardputer::OperationResult runSshProfileStorageTest()
{
    std::vector<cardputer::SshProfile> original;
    std::size_t originalSelected = 0;
    cardputer::OperationResult result = cardputer::loadSshProfiles(original, originalSelected);
    if (!result.success) return result;
    if (original.size() >= cardputer::kMaximumSshProfiles) {
        return {false, "All SSH profile slots are occupied"};
    }
    const cardputer::SshProfile temporary = {
        "Storage test", "127.0.0.1", 22, "test", "temporary",
        cardputer::SshAuthMode::Password, ""};
    result = cardputer::saveSshProfileAt(temporary, original.size());
    if (!result.success) return result;
    std::vector<cardputer::SshProfile> verified;
    std::size_t verifiedSelected = 0;
    result = cardputer::loadSshProfiles(verified, verifiedSelected);
    const bool matches = result.success && verified.size() == original.size() + 1 &&
        verified.back().name == temporary.name && verified.back().host == temporary.host &&
        verified.back().username == temporary.username;
    const cardputer::OperationResult removed = cardputer::deleteSshProfile(original.size());
    const cardputer::OperationResult selected = !original.empty() && removed.success
        ? cardputer::selectSshProfile(originalSelected)
        : cardputer::OperationResult{true, ""};
    if (!removed.success) return removed;
    if (!selected.success) return selected;
    return matches ? cardputer::OperationResult{true, ""}
                   : cardputer::OperationResult{false, "SSH profile NVS round trip did not match"};
}

cardputer::OperationResult runSshSessionTest(bool testSftp)
{
    cardputer::SshProfile profile;
    cardputer::OperationResult result = cardputer::loadSshProfile(profile);
    if (!result.success || !cardputer::sshProfileIsComplete(profile)) {
        return {false, result.success ? String("Selected SSH profile is incomplete") : result.error};
    }
    if (WiFi.status() != WL_CONNECTED) return {false, "Wi-Fi is not connected"};
    cardputer::SshClient client;
    result = client.connect(profile, 60000);
    if (!result.success) return result;
    const cardputer::SshTrustResult trust = cardputer::checkTrustedSshHost(
        profile.host, profile.port, client.fingerprint());
    if (!trust.success || !trust.found || !trust.matches) {
        client.close();
        return {false, trust.success ? String("SSH host key is not trusted yet") : trust.error};
    }
    result = client.authenticate(profile, 60000);
    if (result.success && testSftp) {
        result = client.openSftp(30000);
        if (result.success) {
            const cardputer::SftpEntriesResult listed = client.listSftpDirectory("/", 30000);
            result = listed.success ? cardputer::OperationResult{true, ""}
                                    : cardputer::OperationResult{false, listed.error};
        }
    } else if (result.success) {
        result = client.openTerminal(40, 8, 30000);
    }
    client.close();
    return result;
}

cardputer::OperationResult runSshDemoTest()
{
    const cardputer::SshProfile profile = {
        "Rebex test", "test.rebex.net", 22, "demo", "password",
        cardputer::SshAuthMode::Password, ""};
    cardputer::SshClient client;
    Serial.println("SSHDEMOTEST stage=connect");
    cardputer::OperationResult result = client.connect(profile, 60000);
    if (!result.success) return result;
    const cardputer::SshTrustResult existing = cardputer::checkTrustedSshHost(
        profile.host, profile.port, client.fingerprint());
    if (!existing.success || (existing.found && !existing.matches)) {
        client.close();
        return {false, existing.success ? String("Rebex test host key changed") : existing.error};
    }
    const bool temporaryTrust = !existing.found;
    if (temporaryTrust) {
        result = cardputer::trustSshHost(profile.host, profile.port, client.fingerprint());
    }
    Serial.println("SSHDEMOTEST stage=authenticate");
    if (result.success) result = client.authenticate(profile, 60000);
    Serial.println("SSHDEMOTEST stage=sftp_open");
    if (result.success) result = client.openSftp(30000);
    if (result.success) {
        Serial.println("SSHDEMOTEST stage=list");
        const cardputer::SftpEntriesResult listed = client.listSftpDirectory("/pub/example", 30000);
        result = listed.success && !listed.entries.empty()
            ? cardputer::OperationResult{true, ""}
            : cardputer::OperationResult{false, listed.success
                ? String("Rebex SFTP test directory was empty") : listed.error};
    }
    if (result.success) {
        Serial.println("SSHDEMOTEST stage=download");
        result = client.downloadSftpFile("/pub/example/readme.txt",
                                         "ssh-demo-readme.txt", 60000);
    }
    if (result.success) {
        Serial.println("SSHDEMOTEST stage=pty");
        result = client.openTerminal(40, 8, 30000);
    }
    Serial.println("SSHDEMOTEST stage=close");
    client.close();
    if (SD.exists(cardputer::workspaceFilePath("ssh-demo-readme.txt")) &&
        !SD.remove(cardputer::workspaceFilePath("ssh-demo-readme.txt"))) {
        return {false, "Failed to remove the SFTP demo download"};
    }
    if (temporaryTrust) {
        const cardputer::OperationResult forgotten = cardputer::forgetTrustedSshHost(
            profile.host, profile.port);
        if (result.success && !forgotten.success) result = forgotten;
    }
    return result;
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
        return {false, "Select or create a complete SSH profile"};
    }

    cardputer::SshClient client;
    const cardputer::OperationResult authenticated = connectTrustedSsh(profile, client);
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
    String terminalStatus = "Fn+8 help  ESC disconnect";
    bool redraw = true;
    std::size_t lineOffset = 0;
    std::vector<std::string> commandHistory;
    std::string currentCommand;
    std::size_t historyIndex = 0;
    cardputer::markOperation("ssh_terminal");
    while (client.isOpen()) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            const Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
            if (keys.esc || keyboardWordContains(keys, '`')) {
                terminalStatus = "Disconnected by user";
                break;
            }
            if (keys.fn && keys.f8) {
                cardputer::showTextViewer("SSH SHORTCUTS", {
                    "Arrows/Tab/Ctrl: remote keys",
                    "Fn+5/Fn+6: scroll output",
                    "Fn+7: follow newest output",
                    "Opt+Up/Down: command history",
                    "Fn+8: this help",
                    "ESC: disconnect",
                    "Scrollback: SD /assistant/ssh"}, 0, "ENTER/ESC return");
                waitForModalKeyRelease();
                while (true) {
                    M5Cardputer.update();
                    if (M5Cardputer.Keyboard.isChange() &&
                        M5Cardputer.Keyboard.isPressed()) {
                        const auto helpKeys = M5Cardputer.Keyboard.keysState();
                        if (helpKeys.enter || helpKeys.esc) break;
                    }
                    delay(5);
                }
                waitForModalKeyRelease();
                redraw = true;
                continue;
            }
            if (keys.fn && keys.f5) {
                lineOffset += 6;
                redraw = true;
                continue;
            }
            if (keys.fn && keys.f6) {
                lineOffset = lineOffset > 6 ? lineOffset - 6 : 0;
                redraw = true;
                continue;
            }
            if (keys.fn && keys.f7) {
                lineOffset = 0;
                redraw = true;
                continue;
            }
            std::vector<std::uint8_t> outbound;
            if (keys.opt && (keys.up || keys.down) && !commandHistory.empty()) {
                if (keys.up && historyIndex > 0) --historyIndex;
                if (keys.down && historyIndex < commandHistory.size()) ++historyIndex;
                const std::string recalled = historyIndex < commandHistory.size()
                    ? commandHistory[historyIndex] : std::string();
                outbound.push_back(0x15);
                outbound.insert(outbound.end(), recalled.begin(), recalled.end());
                currentCommand = recalled;
            } else if (keys.ctrl && !keys.word.empty()) {
                const unsigned char character = static_cast<unsigned char>(keys.word.front());
                if (character >= '@' && character <= '_') {
                    outbound.push_back(static_cast<std::uint8_t>(character & 0x1F));
                } else if (character >= 'a' && character <= 'z') {
                    outbound.push_back(static_cast<std::uint8_t>(character - 'a' + 1));
                }
            } else {
                for (const char character : keys.word) {
                    outbound.push_back(static_cast<std::uint8_t>(character));
                    if (static_cast<unsigned char>(character) >= 0x20) {
                        currentCommand.push_back(character);
                    }
                }
            }
            if (keys.enter) {
                outbound.push_back('\r');
                if (!currentCommand.empty()) {
                    if (commandHistory.empty() || commandHistory.back() != currentCommand) {
                        commandHistory.push_back(currentCommand);
                        if (commandHistory.size() > 20) commandHistory.erase(commandHistory.begin());
                    }
                    currentCommand.clear();
                    historyIndex = commandHistory.size();
                }
            }
            if (keys.backspace || keys.del) {
                outbound.push_back(0x7F);
                currentCommand = cardputer::removeLastUtf8CodePoint(currentCommand);
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
            if (lineOffset == 0) {
                terminalStatus = "Fn+8 help  ESC disconnect";
            }
        }
        if (redraw) {
            cardputer::showTextViewer(
                "SSH " + profile.host,
                cardputer::sshTerminalLinesFromBottom(terminal, 38, 8, lineOffset),
                0, lineOffset == 0 ? terminalStatus
                    : String("Scrolled ") + lineOffset + " lines  Fn+7 bottom");
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

String joinSftpPath(const String& directory, const String& name)
{
    return directory == "/" ? String("/") + name : directory + "/" + name;
}

String parentSftpPath(const String& path)
{
    if (path == "/") return "/";
    const int separator = path.lastIndexOf('/');
    return separator <= 0 ? String("/") : path.substring(0, separator);
}

cardputer::OperationResult runSftpBrowser(const cardputer::SshProfile& profile)
{
    cardputer::SshClient client;
    cardputer::OperationResult result = connectTrustedSsh(profile, client);
    if (!result.success) {
        cardputer::markOperation("idle");
        return result;
    }
    cardputer::showBusyScreen("SFTP", "Opening remote filesystem...");
    cardputer::markOperation("sftp_open");
    result = client.openSftp(30000);
    if (!result.success) {
        client.close();
        cardputer::markOperation("idle");
        return result;
    }
    String path = "/";
    while (true) {
        cardputer::markOperation("sftp_list");
        const cardputer::SftpEntriesResult listed = client.listSftpDirectory(path, 30000);
        if (!listed.success) {
            client.close();
            cardputer::markOperation("idle");
            return {false, listed.error};
        }
        std::vector<String> items = {"[..] Parent", "[+] Upload from SD", "[+] New directory"};
        for (const auto& entry : listed.entries) {
            items.push_back(String(entry.directory ? "[D] " : "[F] ") + entry.name +
                            (entry.directory ? String() : String("  ") + entry.size + " B"));
        }
        items.push_back("Close SFTP");
        const int selected = modalSelection("SFTP " + path, items, 0,
                                            "UP/DOWN  ENTER  ESC close");
        if (selected < 0 || static_cast<std::size_t>(selected) == items.size() - 1) break;
        if (selected == 0) {
            path = parentSftpPath(path);
            continue;
        }
        if (selected == 1) {
            const cardputer::WorkspaceFilesResult workspace = cardputer::listWorkspaceFiles();
            if (!workspace.success) {
                result = {false, workspace.error};
                break;
            }
            std::vector<String> names;
            for (const auto& file : workspace.files) {
                names.push_back(file.name + "  " + String(file.size) + " B");
            }
            if (names.empty()) {
                cardputer::showTextViewer("SFTP UPLOAD", {"SD workspace is empty."}, 0,
                                         "ENTER/ESC close");
                delay(1200);
                continue;
            }
            const int fileIndex = modalSelection("UPLOAD FROM SD", names, 0,
                                                 "UP/DOWN  ENTER  ESC cancel");
            if (fileIndex < 0) continue;
            std::string remoteName = workspace.files[fileIndex].name.c_str();
            if (!modalTextInput("REMOTE NAME", "Destination filename", remoteName, 255,
                                false, remoteName)) continue;
            cardputer::showBusyScreen("SFTP UPLOAD", "Writing remote file...");
            cardputer::markOperation("sftp_upload");
            result = client.uploadSftpFile(workspace.files[fileIndex].name,
                                           joinSftpPath(path, remoteName.c_str()), 60000);
            if (!result.success) break;
            continue;
        }
        if (selected == 2) {
            std::string name;
            if (!modalTextInput("NEW REMOTE DIR", "Directory name", "", 120,
                                false, name) || name.empty()) continue;
            cardputer::showBusyScreen("SFTP", "Creating directory...");
            result = client.createSftpDirectory(joinSftpPath(path, name.c_str()), 30000);
            if (!result.success) break;
            continue;
        }
        const cardputer::SftpEntry& entry = listed.entries[static_cast<std::size_t>(selected - 3)];
        const String remotePath = joinSftpPath(path, entry.name);
        if (entry.directory) {
            const int action = modalSelection(entry.name,
                {"Open directory", "Rename", "Delete empty directory", "Cancel"}, 0,
                "UP/DOWN  ENTER  ESC cancel");
            if (action == 0) {
                path = remotePath;
            } else if (action == 1) {
                std::string name = entry.name.c_str();
                if (modalTextInput("RENAME REMOTE", "New name", name, 120, false, name)) {
                    result = client.renameSftpPath(remotePath,
                                                  joinSftpPath(path, name.c_str()), 30000);
                    if (!result.success) break;
                }
            } else if (action == 2 && modalSelection("DELETE DIRECTORY",
                       {"Cancel", "Delete permanently"}, 0,
                       "UP/DOWN  ENTER  ESC cancel") == 1) {
                result = client.removeSftpPath(remotePath, true, 30000);
                if (!result.success) break;
            }
            continue;
        }
        const int action = modalSelection(entry.name,
            {"Download to SD", "Rename", "Delete remote file", "Cancel"}, 0,
            "UP/DOWN  ENTER  ESC cancel");
        if (action == 0) {
            std::string localName = entry.name.c_str();
            if (!modalTextInput("DOWNLOAD TO SD", "Workspace filename", localName, 48,
                                false, localName)) continue;
            cardputer::showBusyScreen("SFTP DOWNLOAD", "Saving to microSD workspace...");
            cardputer::markOperation("sftp_download");
            result = client.downloadSftpFile(remotePath, localName.c_str(), 60000);
            if (!result.success) break;
        } else if (action == 1) {
            std::string name = entry.name.c_str();
            if (modalTextInput("RENAME REMOTE", "New name", name, 120, false, name)) {
                result = client.renameSftpPath(remotePath, joinSftpPath(path, name.c_str()), 30000);
                if (!result.success) break;
            }
        } else if (action == 2 && modalSelection("DELETE REMOTE FILE",
                   {"Cancel", "Delete permanently"}, 0,
                   "UP/DOWN  ENTER  ESC cancel") == 1) {
            result = client.removeSftpPath(remotePath, false, 30000);
            if (!result.success) break;
        }
    }
    client.close();
    cardputer::markOperation("idle");
    return result.success ? cardputer::OperationResult{true, "SFTP session closed"} : result;
}

cardputer::OperationResult installSshKeyFromWorkspace()
{
    const cardputer::WorkspaceFilesResult workspace = cardputer::listWorkspaceFiles();
    if (!workspace.success) return {false, workspace.error};
    std::vector<std::size_t> indexes;
    std::vector<String> items;
    for (std::size_t index = 0; index < workspace.files.size(); ++index) {
        const String lower = workspace.files[index].name;
        if (lower.endsWith(".pem") || lower.endsWith(".key")) {
            indexes.push_back(index);
            items.push_back(workspace.files[index].name);
        }
    }
    if (items.empty()) return {false, "Put a .pem or .key file in the SD workspace first"};
    const int selected = modalSelection("INSTALL SSH KEY", items, 0,
                                        "UP/DOWN  ENTER  ESC cancel");
    if (selected < 0) return {true, "Key installation cancelled"};
    const String sourcePath = cardputer::workspaceFilePath(
        workspace.files[indexes[static_cast<std::size_t>(selected)]].name);
    cardputer::OperationResult result = cardputer::installSshPrivateKey(sourcePath);
    if (result.success && !SD.remove(sourcePath)) {
        result = {false, "Private key installed, but its workspace source could not be removed"};
    }
    return result;
}

cardputer::OperationResult runSshTool()
{
    String status;
    while (true) {
        std::vector<cardputer::SshProfile> profiles;
        std::size_t selectedIndex = 0;
        cardputer::OperationResult result = cardputer::loadSshProfiles(profiles, selectedIndex);
        if (!result.success) return result;
        const String selectedName = profiles.empty() ? String("not configured")
                                                      : profiles[selectedIndex].name;
        const int action = modalSelection("SSH TOOL", {
            "Connect: " + selectedName,
            "SFTP: " + selectedName,
            "Manage profiles (" + String(profiles.size()) + ")",
            String("Install private key: ") + (cardputer::sshPrivateKeyIsInstalled() ? "yes" : "no"),
            "Terminal shortcuts",
            "Back to Tools"}, 0, status.isEmpty() ? "UP/DOWN  ENTER  ESC back" : status);
        status = "";
        if (action < 0 || action == 5) return {true, ""};
        if ((action == 0 || action == 1) && profiles.empty()) {
            status = "Create an SSH profile first";
            continue;
        }
        if (action == 0) {
            result = runSshTerminal();
            status = result.error;
        } else if (action == 1) {
            result = runSftpBrowser(profiles[selectedIndex]);
            status = result.success ? String("SFTP session closed") : result.error;
        } else if (action == 3) {
            result = installSshKeyFromWorkspace();
            status = result.success ? String("Private key installed") : result.error;
        } else if (action == 4) {
            cardputer::showTextViewer("SSH SHORTCUTS", {
                "Arrows/Tab/Ctrl -> remote",
                "Fn+5/Fn+6 -> scroll",
                "Fn+7 -> newest output",
                "Opt+Up/Down -> command history",
                "Fn+8 -> help",
                "ESC -> disconnect",
                "SFTP exchanges SD workspace files"}, 0, "ENTER/ESC return");
            waitForModalKeyRelease();
            while (true) {
                M5Cardputer.update();
                if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) break;
                delay(5);
            }
            waitForModalKeyRelease();
        } else {
            bool profilesDone = false;
            while (!profilesDone) {
                result = cardputer::loadSshProfiles(profiles, selectedIndex);
                if (!result.success) return result;
                std::vector<String> items;
                for (std::size_t index = 0; index < profiles.size(); ++index) {
                    items.push_back(String(index == selectedIndex ? "* " : "  ") +
                                    profiles[index].name + "  " + profiles[index].username + "@" +
                                    profiles[index].host);
                }
                items.push_back("+ New profile");
                items.push_back("Back");
                const int profileIndex = modalSelection("SSH PROFILES", items, selectedIndex,
                                                        "* default  ENTER actions  ESC");
                if (profileIndex < 0 || static_cast<std::size_t>(profileIndex) == items.size() - 1) {
                    profilesDone = true;
                    continue;
                }
                if (static_cast<std::size_t>(profileIndex) == profiles.size()) {
                    cardputer::SshProfile created = {"Server", "", 22, "", "",
                                                     cardputer::SshAuthMode::Password, ""};
                    cardputer::SshProfile edited;
                    if (editSshProfile(created, edited)) {
                        result = cardputer::saveSshProfileAt(edited, profiles.size());
                        status = result.success ? String("Profile created") : result.error;
                    }
                    continue;
                }
                const std::size_t index = static_cast<std::size_t>(profileIndex);
                const int profileAction = modalSelection(profiles[index].name,
                    {"Connect terminal", "Browse SFTP", "Make default", "Edit profile",
                     "Forget trusted host key", "Delete profile", "Back"}, 0,
                    "UP/DOWN  ENTER  ESC back");
                if (profileAction == 0) {
                    result = cardputer::selectSshProfile(index);
                    if (result.success) result = runSshTerminal();
                    status = result.error;
                } else if (profileAction == 1) {
                    result = runSftpBrowser(profiles[index]);
                    status = result.error;
                } else if (profileAction == 2) {
                    result = cardputer::selectSshProfile(index);
                    status = result.success ? String("Default profile selected") : result.error;
                } else if (profileAction == 3) {
                    cardputer::SshProfile edited;
                    if (editSshProfile(profiles[index], edited)) {
                        result = cardputer::saveSshProfileAt(edited, index);
                        status = result.success ? String("Profile saved") : result.error;
                    }
                } else if (profileAction == 4) {
                    result = cardputer::forgetTrustedSshHost(profiles[index].host,
                                                             profiles[index].port);
                    status = result.success ? String("Trusted host key removed") : result.error;
                } else if (profileAction == 5 && modalSelection("DELETE SSH PROFILE",
                           {"Cancel", "Delete permanently"}, 0,
                           "UP/DOWN  ENTER  ESC cancel") == 1) {
                    result = cardputer::deleteSshProfile(index);
                    status = result.success ? String("Profile deleted") : result.error;
                }
            }
        }
    }
}

void renderFilesMenu()
{
    cardputer::showSelectionList("FILES", filesMenuItems(), filesMenuIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderWorkspaceFileList()
{
    cardputer::showSelectionList(workspaceListMode == WorkspaceListMode::ImportChat
                                     ? String("IMPORT CHAT") : String("SD WORKSPACE"),
                                 workspaceFileItems(), workspaceFileIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderFileActions()
{
    cardputer::showSelectionList(fileViewerName, fileActionItems(), fileActionsIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderFileViewer()
{
    const String position = String(cardputer::documentReaderModeLabel(fileReaderMode).c_str()) +
        "  " + String(fileViewerChunkOffset) + "/" +
        String(fileViewerTotalBytes) + " B";
    cardputer::showTextViewer(fileViewerName, fileViewerLines,
                              fileViewerFirstLine, position);
}

void renderFileSpeechSelection()
{
    std::vector<String> items;
    items.reserve(fileViewerLines.size());
    for (std::size_t index = 0; index < fileViewerLines.size(); ++index) {
        items.push_back(String(index + 1) + "  " + fileViewerLines[index].c_str());
    }
    const String footer = fileSpeechSelectionStatus.isEmpty()
        ? String("UP/DOWN  ENTER start  ESC")
        : fileSpeechSelectionStatus;
    cardputer::showSelectionList("READ SELECTED LINES", items,
                                 fileSpeechSelectionIndex, footer);
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

void openWebConsoleMenu()
{
    webConsoleMenuIndex = 0;
    menuStatus = "";
    currentScreen = Screen::WebConsoleMenu;
    renderWebConsoleMenu();
}

void openDeviceMenu()
{
    deviceMenuIndex = 0;
    menuStatus = "";
    currentScreen = Screen::DeviceMenu;
    renderDeviceMenu();
}

void openUtilitiesMenu()
{
    utilitiesMenuIndex = 0;
    menuStatus = "";
    currentScreen = Screen::UtilitiesMenu;
    renderUtilitiesMenu();
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
    workspaceListMode = WorkspaceListMode::Browse;
    workspaceListReturnScreen = Screen::FilesMenu;
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

void openChatImportList()
{
    const cardputer::WorkspaceFilesResult result = cardputer::listWorkspaceFiles();
    if (!result.success) {
        menuStatus = result.error;
        renderFilesMenu();
        return;
    }
    workspaceFiles.clear();
    for (const auto& file : result.files) {
        if (file.name.endsWith(".chat.jsonl")) {
            workspaceFiles.push_back(file);
        }
    }
    workspaceFileIndex = workspaceFiles.empty() ? 0 : 1;
    workspaceListMode = WorkspaceListMode::ImportChat;
    workspaceListReturnScreen = Screen::FilesMenu;
    menuStatus = workspaceFiles.empty() ? String("No .chat.jsonl bundles found")
                                        : String("Choose a .chat.jsonl bundle");
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

cardputer::OperationResult openUtilityWorkspaceFile(const String& name)
{
    workspaceListMode = WorkspaceListMode::Browse;
    workspaceListReturnScreen = Screen::UtilitiesMenu;
    cardputer::OperationResult result = selectWorkspaceFileByName(name);
    if (!result.success) {
        result = cardputer::createWorkspaceFile(name);
        if (result.success) {
            result = selectWorkspaceFileByName(name);
        }
    }
    if (!result.success) {
        return result;
    }
    openSelectedWorkspaceFile();
    return currentScreen == Screen::FileActions
        ? cardputer::OperationResult{true, ""}
        : cardputer::OperationResult{false, menuStatus};
}

cardputer::OperationResult loadFileViewerChunk(std::uint32_t offset)
{
    const cardputer::WorkspaceChunkResult result = cardputer::readWorkspaceFileChunk(
        fileViewerName, offset, kFileViewerChunkBytes);
    if (!result.success) {
        return {false, result.error};
    }
    fileViewerContent = result.content;
    const std::string formatted = cardputer::formatDocumentChunk(
        fileReaderMode, fileViewerContent);
    fileViewerLines = cardputer::wrapUtf8Text(formatted, 38);
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
    fileReaderMode = cardputer::detectDocumentReaderMode(fileViewerName.c_str());
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
    fileReaderMode = cardputer::detectDocumentReaderMode(fileViewerName.c_str());
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
    std::vector<Point2D_t> newPresses = newKeyPresses(currentKeys, pressedKeys);
    const bool sameKeys = currentKeys.size() == pressedKeys.size() &&
        std::equal(currentKeys.begin(), currentKeys.end(), pressedKeys.begin());
    const std::uint32_t now = millis();
    if (!sameKeys || currentKeys.empty()) {
        keyboardRepeatStartedAt = now;
        lastKeyboardRepeatAt = now;
    } else if (newPresses.empty() && settings.keyboardRepeatMs > 0 &&
               now - keyboardRepeatStartedAt >= 500U &&
               now - lastKeyboardRepeatAt >= settings.keyboardRepeatMs) {
        newPresses = currentKeys;
        lastKeyboardRepeatAt = now;
    }
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
            const cardputer::ChatDocumentResult loaded = cardputer::loadChat(selectedChatId);
            if (!loaded.success) {
                menuStatus = loaded.error;
                renderChatActions();
                return;
            }
            cardputer::ChatDocument updated = loaded.chat;
            if (!updated.sshToolsEnabled && !cardputer::sshToolIsAvailable()) {
                menuStatus = "Configure and trust an SSH profile first";
                renderChatActions();
                return;
            }
            updated.sshToolsEnabled = !updated.sshToolsEnabled;
            updated.summary.updatedAt = currentChatTimestamp();
            const cardputer::OperationResult saved = cardputer::saveChat(updated);
            if (!saved.success) {
                menuStatus = saved.error;
            } else {
                selectedChatSshToolsEnabled = updated.sshToolsEnabled;
                if (selectedChatId == activeChatId) {
                    activeChatSshToolsEnabled = updated.sshToolsEnabled;
                }
                menuStatus = updated.sshToolsEnabled
                    ? String("Model SSH access enabled")
                    : String("Model SSH access disabled");
            }
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 3) {
            menuStatus = "Older turns are preserved on microSD";
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 4) {
            retryLastRequest();
        } else if (enterPressed && chatActionsIndex == 5) {
            openLatestSearchSources();
        } else if (enterPressed && (chatActionsIndex == 6 || chatActionsIndex == 7)) {
            const cardputer::ChatDocumentResult loaded = cardputer::loadChat(selectedChatId);
            if (!loaded.success) {
                menuStatus = loaded.error;
                renderChatActions();
                return;
            }
            cardputer::ChatDocument updated = loaded.chat;
            if (chatActionsIndex == 6) {
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
                menuStatus = chatActionsIndex == 6
                    ? (updated.summary.pinned ? String("Chat pinned") : String("Chat unpinned"))
                    : (updated.summary.archived ? String("Chat archived")
                                                : String("Chat restored"));
            }
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 8) {
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
            selectedChatSshToolsEnabled = duplicated.chat.sshToolsEnabled;
            menuStatus = "Chat duplicated";
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 9) {
            const String filename = "chat_" + selectedChatId + ".md";
            const cardputer::OperationResult exported = cardputer::exportChatToWorkspace(
                selectedChatId, filename);
            menuStatus = exported.success ? "Exported as " + filename : exported.error;
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 10) {
            const String filename = "chat_" + selectedChatId + ".chat.jsonl";
            const cardputer::OperationResult exported = cardputer::exportChatBundleToWorkspace(
                selectedChatId, filename);
            menuStatus = exported.success ? "Exported as " + filename : exported.error;
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 11) {
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

    if (currentScreen == Screen::SearchSources) {
        const std::size_t itemCount = searchSources.size();
        if (cancelPressed) {
            currentScreen = Screen::ChatActions;
            menuStatus = "";
            renderChatActions();
        } else if (upPressed) {
            searchSourceIndex = searchSourceIndex > 0 ? searchSourceIndex - 1 : 0;
            renderSearchSources();
        } else if (downPressed) {
            searchSourceIndex = std::min(searchSourceIndex + 1, itemCount - 1);
            renderSearchSources();
        } else if (enterPressed) {
            if (searchSourceIndex >= searchSources.size()) {
                menuStatus = "Search source selection is out of range";
                renderSearchSources();
                return;
            }
            const auto& source = searchSources[searchSourceIndex];
            const std::string text = std::string("Query: ") + searchSourcesQuery.c_str() +
                "\n\n" + source.title.c_str() + "\n" + source.url.c_str() +
                "\n\n" + source.snippet;
            searchSourceViewerLines = cardputer::wrapUtf8Text(text, 38);
            searchSourceViewerFirstLine = 0;
            currentScreen = Screen::SearchSourceViewer;
            render();
        }
        return;
    }

    if (currentScreen == Screen::SearchSourceViewer) {
        if (cancelPressed) {
            currentScreen = Screen::SearchSources;
            renderSearchSources();
        } else if (upPressed) {
            searchSourceViewerFirstLine = searchSourceViewerFirstLine > 0
                ? searchSourceViewerFirstLine - 1 : 0;
            render();
        } else if (downPressed &&
                   searchSourceViewerFirstLine + 8 < searchSourceViewerLines.size()) {
            ++searchSourceViewerFirstLine;
            render();
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
                openWebConsoleMenu();
            } else if (carouselIndex == 6) {
                openDeviceMenu();
            } else if (carouselIndex == 7) {
                openUtilitiesMenu();
            } else if (carouselIndex == 8) {
                controlsHelpIndex = 0;
                currentScreen = Screen::ControlsHelp;
                renderControlsHelp();
            } else {
                cardputer::showFatalError("Carousel selection is out of range");
            }
        }
        return;
    }

    if (currentScreen == Screen::FirmwareUpdateConfirm) {
        if (cancelPressed) {
            pendingFirmwareUpdate = {};
            currentScreen = Screen::DeviceMenu;
            menuStatus = "Firmware update canceled";
            renderDeviceMenu();
        } else if (enterPressed) {
            std::uint32_t lastPercent = 101;
            const cardputer::FirmwareProgressCallback progress = [&lastPercent](
                std::uint32_t current, std::uint32_t total) {
                const std::uint32_t percent = total == 0 ? 0 : current * 100U / total;
                if (percent != lastPercent) {
                    lastPercent = percent;
                    cardputer::showBusyScreen("FIRMWARE UPDATE",
                        "Progress " + String(percent) + "% - ESC cancels");
                }
            };
            const cardputer::FirmwareCancelCallback cancelled = []() {
                M5Cardputer.update();
                return M5Cardputer.Keyboard.keysState().esc;
            };
            cardputer::markOperation("ota_download");
            cardputer::OperationResult result = cardputer::downloadFirmwareUpdate(
                pendingFirmwareUpdate, progress, cancelled);
            if (result.success) {
                lastPercent = 101;
                cardputer::markOperation("ota_install");
                result = cardputer::installDownloadedFirmware(
                    pendingFirmwareUpdate, progress, cancelled);
            }
            cardputer::markOperation("idle");
            if (!result.success) {
                currentScreen = Screen::DeviceMenu;
                menuStatus = result.error;
                renderDeviceMenu();
                return;
            }
            cardputer::showBusyScreen("FIRMWARE UPDATE", "Verified. Recovery is installing...");
            delay(800);
            ESP.restart();
        }
        return;
    }

    if (currentScreen == Screen::RestoreBackupConfirm) {
        if (cancelPressed) {
            currentScreen = Screen::DeviceMenu;
            menuStatus = "Restore canceled";
            renderDeviceMenu();
        } else if (enterPressed) {
            cardputer::showBusyScreen("RESTORE BACKUP", "Validating and restoring...");
            cardputer::markOperation("backup_restore");
            String restoredActiveChatId;
            cardputer::OperationResult result = cardputer::restoreLocalBackup(
                settings, restoredActiveChatId);
            if (result.success) {
                result = refreshChatList();
            }
            if (result.success) {
                result = activateChat(restoredActiveChatId);
            }
            if (result.success) {
                result = applyDisplayAndCpuSettings(settings);
            }
            if (result.success) {
                result = applyWifiPowerSetting(settings);
            }
            cardputer::markOperation("idle");
            menuStatus = result.success ? String("Backup restored") : result.error;
            currentScreen = Screen::DeviceMenu;
            renderDeviceMenu();
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

    if (currentScreen == Screen::UtilitiesMenu) {
        const std::size_t itemCount = utilitiesMenuItems().size();
        if (cancelPressed) {
            currentScreen = Screen::MainCarousel;
            menuStatus = "";
            renderCarousel();
        } else if (upPressed) {
            utilitiesMenuIndex = utilitiesMenuIndex > 0 ? utilitiesMenuIndex - 1 : 0;
            renderUtilitiesMenu();
        } else if (downPressed) {
            utilitiesMenuIndex = std::min(utilitiesMenuIndex + 1, itemCount - 1);
            renderUtilitiesMenu();
        } else if (enterPressed) {
            if (utilitiesMenuIndex == 0 || utilitiesMenuIndex == 1) {
                const String name = utilitiesMenuIndex == 0 ? "notes.md" : "checklist.md";
                const cardputer::OperationResult result = openUtilityWorkspaceFile(name);
                if (!result.success) {
                    menuStatus = result.error;
                    currentScreen = Screen::UtilitiesMenu;
                    renderUtilitiesMenu();
                }
            } else if (utilitiesMenuIndex == 2) {
                timerMenuIndex = 0;
                menuStatus = "";
                currentScreen = Screen::TimerMenu;
                renderTimerMenu();
            } else if (utilitiesMenuIndex == 3) {
                calculatorStatus = "";
                currentScreen = Screen::Calculator;
                renderCalculator();
            } else if (utilitiesMenuIndex == 4) {
                qrStatus = "";
                currentScreen = Screen::QrEntry;
                renderQrEntry();
            } else if (utilitiesMenuIndex == 5) {
                const cardputer::OperationResult result = runSshTool();
                menuStatus = result.error;
                currentScreen = Screen::UtilitiesMenu;
                renderUtilitiesMenu();
            } else if (utilitiesMenuIndex == 6) {
                menuStatus = "";
                currentScreen = Screen::SystemMonitor;
                renderSystemMonitor();
            } else {
                currentScreen = Screen::MainCarousel;
                menuStatus = "";
                renderCarousel();
            }
        }
        return;
    }

    if (currentScreen == Screen::WebConsoleMenu) {
        const std::size_t itemCount = webConsoleMenuItems().size();
        if (cancelPressed) {
            currentScreen = Screen::MainCarousel;
            menuStatus = "";
            renderCarousel();
        } else if (upPressed) {
            webConsoleMenuIndex = webConsoleMenuIndex > 0 ? webConsoleMenuIndex - 1 : 0;
            renderWebConsoleMenu();
        } else if (downPressed) {
            webConsoleMenuIndex = std::min(webConsoleMenuIndex + 1, itemCount - 1);
            renderWebConsoleMenu();
        } else if (enterPressed) {
            if (webConsoleMenuIndex == 0) {
                openWebConsole(Screen::WebConsoleMenu);
            } else if (webConsoleMenuIndex == 1) {
                menuStatus = WiFi.status() == WL_CONNECTED
                    ? String("Open the address in a trusted local browser")
                    : String("Connect CardMind to 2.4 GHz Wi-Fi first");
                renderWebConsoleMenu();
            } else if (webConsoleMenuIndex == 2) {
                menuStatus = "Inactive browser sessions expire after 15 minutes";
                renderWebConsoleMenu();
            } else if (webConsoleMenuIndex == 3) {
                const cardputer::PythonModeStatus status = cardputer::inspectPythonMode();
                menuStatus = !status.lastRuntimeError.isEmpty()
                    ? String("Last Python start failed: ") + status.lastRuntimeError
                    : status.partitionLayoutReady && status.pythonImageReady
                        ? String("Python workspace is installed and ready")
                        : status.error;
                renderWebConsoleMenu();
            } else if (webConsoleMenuIndex == 4) {
                String password;
                cardputer::OperationResult result =
                    cardputer::loadSetupAccessPointPassword(password);
                if (result.success && password.isEmpty()) {
                    result = {false, "Run configuration once to create an installation password"};
                }
                if (result.success) {
                    result = cardputer::synchronizePythonModeSettings(settings, password);
                }
                if (result.success) {
                    result = cardputer::activatePythonMode();
                }
                if (!result.success) {
                    menuStatus = result.error;
                    renderWebConsoleMenu();
                    return;
                }
                const String address = WiFi.status() == WL_CONNECTED
                    ? String("Open http://") + WiFi.localIP().toString() + " after restart"
                    : String("Connect Wi-Fi after Python starts");
                cardputer::showBusyScreen("PYTHON WORKSPACE", address);
                delay(800);
                ESP.restart();
            } else if (webConsoleMenuIndex == 5) {
                cardputer::markOperation("provisioning");
                cardputer::runProvisioningPortal(settings);
                cardputer::markOperation("idle");
                menuStatus = "Configuration portal closed";
                renderWebConsoleMenu();
            } else {
                currentScreen = Screen::MainCarousel;
                menuStatus = "";
                renderCarousel();
            }
        }
        return;
    }

    if (currentScreen == Screen::SystemMonitor) {
        if (cancelPressed) {
            currentScreen = Screen::UtilitiesMenu;
            menuStatus = "";
            renderUtilitiesMenu();
        }
        return;
    }

    if (currentScreen == Screen::TimerMenu) {
        const std::size_t itemCount = timerMenuItems().size();
        if (cancelPressed) {
            currentScreen = Screen::UtilitiesMenu;
            menuStatus = "";
            renderUtilitiesMenu();
        } else if (upPressed) {
            timerMenuIndex = timerMenuIndex > 0 ? timerMenuIndex - 1 : 0;
            renderTimerMenu();
        } else if (downPressed) {
            timerMenuIndex = std::min(timerMenuIndex + 1, itemCount - 1);
            renderTimerMenu();
        } else if (enterPressed) {
            if (timerMenuIndex < 3) {
                const std::uint32_t minutes = timerMenuIndex == 0 ? 5U
                    : (timerMenuIndex == 1 ? 15U : 25U);
                timerDurationSeconds = minutes * 60U;
                timerEndsAt = millis() + timerDurationSeconds * 1000U;
                timerRunning = true;
                menuStatus = "Timer started";
            } else if (timerMenuIndex == 3) {
                timerRunning = false;
                timerEndsAt = 0;
                timerDurationSeconds = 0;
                menuStatus = "Timer canceled";
            } else {
                currentScreen = Screen::UtilitiesMenu;
                menuStatus = "";
                renderUtilitiesMenu();
                return;
            }
            renderTimerMenu();
        }
        return;
    }

    if (currentScreen == Screen::Calculator) {
        if (cancelPressed) {
            calculatorStatus = "";
            currentScreen = Screen::UtilitiesMenu;
            renderUtilitiesMenu();
        } else if (clearDraftPressed) {
            calculatorInput.clear();
            calculatorStatus = "";
            renderCalculator();
        } else if (backspacePressed) {
            calculatorInput = cardputer::removeLastUtf8CodePoint(calculatorInput);
            calculatorStatus = "";
            renderCalculator();
        } else if (enterPressed) {
            const cardputer::CalculationResult result = cardputer::calculateExpression(
                calculatorInput);
            calculatorStatus = result.success
                ? "= " + String(cardputer::formatCalculationResult(result.value).c_str())
                : String(result.error.c_str());
            renderCalculator();
        } else if (!keys.fn && !keys.ctrl && !keys.alt && !keys.opt) {
            for (const char character : printableNewKeys(newPresses)) {
                const bool allowed = (character >= '0' && character <= '9') ||
                    character == '.' || character == '+' || character == '-' ||
                    character == '*' || character == '/' || character == '(' ||
                    character == ')' || character == ' ';
                if (!allowed) {
                    calculatorStatus = "Use digits and + - * / ( )";
                    continue;
                }
                if (calculatorInput.size() < 96) {
                    calculatorInput += character;
                    calculatorStatus = "";
                }
            }
            renderCalculator();
        }
        return;
    }

    if (currentScreen == Screen::QrEntry) {
        if (cancelPressed) {
            qrStatus = "";
            currentScreen = Screen::UtilitiesMenu;
            renderUtilitiesMenu();
        } else if (keys.fn && keys.f3) {
            keyboardLayout = keyboardLayout == cardputer::KeyboardLayout::English
                ? cardputer::KeyboardLayout::Russian
                : cardputer::KeyboardLayout::English;
            qrStatus = keyboardLayout == cardputer::KeyboardLayout::English
                ? "English layout" : "Russian layout";
            renderQrEntry();
        } else if (clearDraftPressed) {
            qrInput.clear();
            qrStatus = "";
            renderQrEntry();
        } else if (backspacePressed) {
            qrInput = cardputer::removeLastUtf8CodePoint(qrInput);
            qrStatus = "";
            renderQrEntry();
        } else if (enterPressed) {
            if (qrInput.empty()) {
                qrStatus = "Enter text or a URL first";
                renderQrEntry();
            } else {
                currentScreen = Screen::QrDisplay;
                render();
            }
        } else if (!keys.fn && !keys.ctrl && !keys.alt && !keys.opt) {
            for (const char character : printableNewKeys(newPresses)) {
                const std::string text = keyboardLayout == cardputer::KeyboardLayout::Russian
                    ? cardputer::mapKeyToRussian(character)
                    : std::string(1, character);
                if (qrInput.size() + text.size() <= cardputer::kMaximumQrPayloadBytes) {
                    qrInput += text;
                    qrStatus = "";
                }
            }
            renderQrEntry();
        }
        return;
    }

    if (currentScreen == Screen::QrDisplay) {
        if (cancelPressed || enterPressed) {
            currentScreen = Screen::QrEntry;
            renderQrEntry();
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
                cardputer::Settings candidate = settings;
                candidate.displayBrightness = nextDisplayBrightness(candidate.displayBrightness);
                const cardputer::OperationResult result = saveAndApplyDeviceSettings(candidate);
                menuStatus = result.success
                    ? "Brightness set to " + brightnessSettingLabel(settings.displayBrightness)
                    : result.error;
                renderDeviceMenu();
            } else if (deviceMenuIndex == 1) {
                cardputer::Settings candidate = settings;
                candidate.screenSleepMinutes = nextScreenSleepMinutes(
                    candidate.screenSleepMinutes);
                const cardputer::OperationResult result = saveAndApplyDeviceSettings(candidate);
                menuStatus = result.success
                    ? "Screen sleep: " + sleepSettingLabel(settings.screenSleepMinutes)
                    : result.error;
                lastUserActivityAt = millis();
                renderDeviceMenu();
            } else if (deviceMenuIndex == 2) {
                cardputer::Settings candidate = settings;
                candidate.keyboardRepeatMs = nextKeyboardRepeatMs(candidate.keyboardRepeatMs);
                const cardputer::OperationResult result = saveAndApplyDeviceSettings(candidate);
                menuStatus = result.success
                    ? "Keyboard repeat: " + keyboardRepeatSettingLabel(settings.keyboardRepeatMs)
                    : result.error;
                renderDeviceMenu();
            } else if (deviceMenuIndex == 3) {
                cardputer::Settings candidate = settings;
                candidate.powerProfile = static_cast<std::uint8_t>(
                    (candidate.powerProfile + 1U) % 3U);
                const cardputer::OperationResult result = saveAndApplyDeviceSettings(candidate);
                menuStatus = result.success
                    ? "Power profile: " + powerProfileLabel(settings.powerProfile)
                    : result.error;
                renderDeviceMenu();
            } else if (deviceMenuIndex == 4) {
                cardputer::OperationResult result = saveCurrentChat();
                if (result.success) {
                    cardputer::showBusyScreen("BACKUP", "Copying chats and metadata...");
                    cardputer::markOperation("backup_create");
                    result = cardputer::createLocalBackup(settings, activeChatId);
                    cardputer::markOperation("idle");
                }
                menuStatus = result.success ? String("Local backup updated") : result.error;
                renderDeviceMenu();
            } else if (deviceMenuIndex == 5) {
                currentScreen = Screen::RestoreBackupConfirm;
                render();
            } else if (deviceMenuIndex == 6) {
                String summary;
                const cardputer::OperationResult result = cardputer::localBackupSummary(summary);
                menuStatus = result.success ? summary : result.error;
                renderDeviceMenu();
            } else if (deviceMenuIndex == 7) {
                cardputer::markOperation("provisioning");
                cardputer::runProvisioningPortal(settings);
                cardputer::markOperation("idle");
                const cardputer::OperationResult result = applyDisplayAndCpuSettings(settings);
                menuStatus = result.success ? String("Settings portal closed") : result.error;
                renderDeviceMenu();
            } else if (deviceMenuIndex == 8) {
                ensureNetworkReady();
                if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
                    menuStatus = statusMessage;
                    renderDeviceMenu();
                    return;
                }
                cardputer::showBusyScreen("FIRMWARE UPDATE", "Checking GitHub release...");
                cardputer::markOperation("ota_check");
                pendingFirmwareUpdate = cardputer::checkLatestFirmwareUpdate(kFirmwareVersion);
                cardputer::markOperation("idle");
                if (!pendingFirmwareUpdate.success) {
                    menuStatus = pendingFirmwareUpdate.error;
                    renderDeviceMenu();
                } else if (!pendingFirmwareUpdate.newerAvailable) {
                    menuStatus = "Latest release is " + pendingFirmwareUpdate.version +
                        "; current is v" + kFirmwareVersion;
                    renderDeviceMenu();
                } else if (!pendingFirmwareUpdate.pythonRecoveryReady) {
                    menuStatus = "Update disabled: MicroPython recovery is unavailable";
                    renderDeviceMenu();
                } else {
                    currentScreen = Screen::FirmwareUpdateConfirm;
                    render();
                }
            } else if (deviceMenuIndex == 9) {
                diagnosticsReturnScreen = Screen::DeviceMenu;
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
                openChatImportList();
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
            currentScreen = workspaceListReturnScreen;
            menuStatus = "";
            render();
        } else if (upPressed) {
            workspaceFileIndex = workspaceFileIndex > 0 ? workspaceFileIndex - 1 : 0;
            renderWorkspaceFileList();
        } else if (downPressed) {
            workspaceFileIndex = std::min(workspaceFileIndex + 1, itemCount - 1);
            renderWorkspaceFileList();
        } else if (enterPressed) {
            if (workspaceListMode == WorkspaceListMode::ImportChat) {
                if (workspaceFileIndex == 0 || workspaceFileIndex > workspaceFiles.size()) {
                    menuStatus = "Select a .chat.jsonl bundle";
                    renderWorkspaceFileList();
                    return;
                }
                const String filename = workspaceFiles[workspaceFileIndex - 1].name;
                cardputer::markOperation("chat_import");
                const cardputer::ChatDocumentResult imported =
                    cardputer::importChatBundleFromWorkspace(filename);
                cardputer::markOperation("idle");
                if (!imported.success) {
                    menuStatus = imported.error;
                    renderWorkspaceFileList();
                    return;
                }
                const cardputer::OperationResult refreshed = refreshChatList();
                const cardputer::OperationResult activated = refreshed.success
                    ? activateChat(imported.chat.summary.id)
                    : refreshed;
                if (!activated.success) {
                    menuStatus = activated.error;
                    renderWorkspaceFileList();
                    return;
                }
                currentScreen = Screen::Chat;
                setTransientStatus("Chat imported", 2000);
                render();
            } else if (workspaceFileIndex == 0) {
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
                cardputer::OperationResult result = prepareDocumentSpeech();
                if (result.success) {
                    cardputer::markOperation("document_tts_page");
                    result = playDocumentSpeechText(
                        joinedViewerLines(fileViewerFirstLine,
                                          fileViewerFirstLine + kFileViewerPageLines - 1),
                        "Reading current page");
                    cardputer::markOperation("idle");
                }
                menuStatus = result.success
                    ? (result.error.isEmpty() ? String("Current page spoken") : result.error)
                    : result.error;
                renderFileActions();
            } else if (fileActionsIndex == 3) {
                fileSpeechSelectionIndex = std::min(
                    fileViewerFirstLine,
                    fileViewerLines.empty() ? std::size_t{0} : fileViewerLines.size() - 1);
                fileSpeechSelectionStart = fileSpeechSelectionIndex;
                fileSpeechSelectionStarted = false;
                fileSpeechSelectionStatus = "";
                currentScreen = Screen::FileSpeechSelection;
                renderFileSpeechSelection();
            } else if (fileActionsIndex == 4) {
                cardputer::OperationResult result = prepareDocumentSpeech();
                if (result.success) {
                    cardputer::markOperation("document_tts_all");
                    result = speakEntireDocument();
                    cardputer::markOperation("idle");
                }
                menuStatus = result.success
                    ? (result.error.isEmpty() ? String("Document spoken") : result.error)
                    : result.error;
                renderFileActions();
            } else if (fileActionsIndex == 5) {
                beginFileFind();
            } else if (fileActionsIndex == 6) {
                if (lastFileFindQuery.empty()) {
                    menuStatus = "Run Find text first";
                    renderFileActions();
                } else {
                    const std::uint32_t nextOffset = lastFileFindOffset +
                        static_cast<std::uint32_t>(lastFileFindQuery.size());
                    findFileText(lastFileFindQuery, nextOffset);
                }
            } else if (fileActionsIndex == 7) {
                const cardputer::OperationResult result = cardputer::saveWorkspaceBookmark(
                    fileViewerName, fileViewerChunkOffset);
                menuStatus = result.success
                    ? String("Bookmark saved at byte ") + String(fileViewerChunkOffset)
                    : result.error;
                renderFileActions();
            } else if (fileActionsIndex == 8) {
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
            } else if (fileActionsIndex == 9) {
                beginFileNameEntry(FileNameAction::Copy, fileViewerName);
            } else if (fileActionsIndex == 10) {
                beginFileNameEntry(FileNameAction::Rename, fileViewerName);
            } else if (fileActionsIndex == 11) {
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

    if (currentScreen == Screen::FileSpeechSelection) {
        if (cancelPressed) {
            fileSpeechSelectionStarted = false;
            fileSpeechSelectionStatus = "";
            currentScreen = Screen::FileActions;
            renderFileActions();
        } else if (upPressed && fileSpeechSelectionIndex > 0) {
            --fileSpeechSelectionIndex;
            renderFileSpeechSelection();
        } else if (downPressed && fileSpeechSelectionIndex + 1 < fileViewerLines.size()) {
            ++fileSpeechSelectionIndex;
            renderFileSpeechSelection();
        } else if (enterPressed && !fileViewerLines.empty()) {
            if (!fileSpeechSelectionStarted) {
                fileSpeechSelectionStart = fileSpeechSelectionIndex;
                fileSpeechSelectionStarted = true;
                fileSpeechSelectionStatus = "Start " + String(fileSpeechSelectionStart + 1) +
                    "; choose end + ENTER";
                renderFileSpeechSelection();
            } else {
                const std::size_t first = std::min(
                    fileSpeechSelectionStart, fileSpeechSelectionIndex);
                const std::size_t last = std::max(
                    fileSpeechSelectionStart, fileSpeechSelectionIndex);
                cardputer::OperationResult result = prepareDocumentSpeech();
                if (result.success) {
                    cardputer::markOperation("document_tts_selection");
                    result = playDocumentSpeechText(
                        joinedViewerLines(first, last), "Reading selected lines");
                    cardputer::markOperation("idle");
                }
                fileSpeechSelectionStarted = false;
                fileSpeechSelectionStatus = "";
                menuStatus = result.success
                    ? (result.error.isEmpty() ? String("Selection spoken") : result.error)
                    : result.error;
                currentScreen = Screen::FileActions;
                renderFileActions();
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
            fileEditorStatus = "Page cleared; ENTER to save";
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
            menuStatus = "File saved atomically";
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
            currentScreen = diagnosticsReturnScreen;
            menuStatus = "";
            render();
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
    const cardputer::OperationResult deviceSettingsResult = applyDisplayAndCpuSettings(settings);
    if (!deviceSettingsResult.success) {
        cardputer::showFatalError(deviceSettingsResult.error);
        Serial.println("FATAL event=device_settings result=failed");
        while (true) {
            delay(1000);
        }
    }
    lastUserActivityAt = millis();
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
    const cardputer::OperationResult wifiPowerResult = applyWifiPowerSetting(settings);
    if (!wifiPowerResult.success) {
        menuStatus = wifiPowerResult.error;
        renderCarousel();
        Serial.println("ERROR event=wifi_power_setting result=failed");
    }
    Serial.println("READY");
}

void loop()
{
    M5Cardputer.update();
    const bool inputActivity = M5Cardputer.Keyboard.isChange() || M5Cardputer.BtnA.wasPressed();
    if (inputActivity) {
        lastUserActivityAt = millis();
        if (displaySleeping) {
            displaySleeping = false;
            M5Cardputer.Display.setBrightness(settings.displayBrightness);
            pressedKeys = M5Cardputer.Keyboard.keyList();
            render();
            delay(5);
            return;
        }
    }
    if (!displaySleeping && settings.screenSleepMinutes > 0 &&
        millis() - lastUserActivityAt >=
            static_cast<std::uint32_t>(settings.screenSleepMinutes) * 60000U) {
        displaySleeping = true;
        M5Cardputer.Display.setBrightness(0);
    }
    if (displaySleeping) {
        updateSerial();
        delay(20);
        return;
    }
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
    if (timerRunning && static_cast<std::int32_t>(millis() - timerEndsAt) >= 0) {
        timerRunning = false;
        timerEndsAt = 0;
        timerDurationSeconds = 0;
        M5Cardputer.Speaker.setVolume(settings.ttsVolume);
        M5Cardputer.Speaker.tone(1200, 220);
        menuStatus = "Timer finished";
        if (currentScreen == Screen::TimerMenu || currentScreen == Screen::UtilitiesMenu) {
            render();
        }
    } else if (timerRunning && currentScreen == Screen::TimerMenu &&
               millis() - lastTimerRenderAt >= 1000U) {
        lastTimerRenderAt = millis();
        renderTimerMenu();
    }
    if (currentScreen == Screen::SystemMonitor &&
        millis() - lastSystemMonitorRenderAt >= 1000U) {
        renderSystemMonitor();
    }
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
