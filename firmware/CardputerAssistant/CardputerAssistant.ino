#include <M5Cardputer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp32-hal-cpu.h>
#include <hal/usb_serial_jtag_ll.h>

#include "src/api_client.h"
#include "src/adv_audio_power.h"
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
#include "src/project_chat_storage.h"
#include "src/project_bundle.h"
#include "src/project_storage.h"
#include "src/stt_client.h"
#include "src/storage.h"
#include "src/storage_migration.h"
#include "src/ssh_client.h"
#include "src/ssh_terminal.h"
#include "src/ssh_tool.h"
#include "src/text_utils.h"
#include "src/tool_router.h"
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

constexpr const char* kFirmwareVersion = "1.12.1";
constexpr std::size_t kMaximumInputBytes = 16384;
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
    AiMenu,
    ModelPicker,
    GlobalInstructions,
    ProjectList,
    ProjectActions,
    ProjectModelPicker,
    ProjectInstructions,
    ChatList,
    ChatActions,
    ArchivedChatViewer,
    SearchSources,
    SearchSourceViewer,
    ChatInstructions,
    ClearChatConfirm,
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
std::vector<cardputer::ProjectSummary> projects;
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
String activeProjectId;
String activeProjectTitle;
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
std::uint32_t chatPageOffset = 0;
std::uint32_t chatNextPageOffset = 0;
bool chatPageEof = true;
std::vector<std::uint32_t> chatPreviousPageOffsets;
std::size_t projectListIndex = 0;
std::size_t projectActionsIndex = 0;
std::string projectInstructionsInput;
String projectInstructionsStatus;
std::uint32_t projectPageOffset = 0;
std::uint32_t projectNextPageOffset = 0;
bool projectPageEof = true;
std::vector<std::uint32_t> projectPreviousPageOffsets;
String selectedProjectId;
String selectedProjectTitle;
std::size_t chatActionsIndex = 0;
std::size_t searchSourceIndex = 0;
std::vector<cardputer::WebSearchSource> searchSources;
String searchSourcesQuery;
std::vector<std::string> searchSourceViewerLines;
std::size_t searchSourceViewerFirstLine = 0;
std::vector<std::string> archivedChatViewerLines;
std::vector<std::uint32_t> archivedChatPreviousOffsets;
std::size_t archivedChatViewerFirstLine = 0;
std::uint32_t archivedChatPageOffset = 0;
std::uint32_t archivedChatNextOffset = 0;
bool archivedChatEof = true;
String selectedChatId;
String selectedChatTitle;
bool selectedChatSshToolsEnabled = false;
std::string instructionsInput;
String instructionsStatus;
std::string globalInstructionsInput;
String globalInstructionsStatus;
String deleteChatId;
String deleteChatTitle;
Screen deleteChatReturnScreen = Screen::ChatList;
String clearChatId;
String clearChatTitle;
std::size_t voiceMenuIndex = 0;
std::size_t webConsoleMenuIndex = 0;
std::size_t deviceMenuIndex = 0;
std::size_t utilitiesMenuIndex = 0;
std::size_t timerMenuIndex = 0;
std::size_t filesMenuIndex = 0;
std::size_t workspaceFileIndex = 0;
std::uint32_t workspacePageOffset = 0;
std::uint32_t workspaceNextPageOffset = 0;
bool workspacePageEof = true;
std::vector<std::uint32_t> workspacePreviousPageOffsets;
std::size_t fileActionsIndex = 0;
std::size_t diagnosticsIndex = 0;
std::size_t controlsHelpIndex = 0;
std::size_t aiMenuIndex = 0;
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
bool webConsoleStartupPending = false;
std::uint32_t webConsoleStartupNotBefore = 0;
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

void ensureNetworkReady();
void render();
void renderCarousel();
void renderChatActions();
void renderChatInstructions();
void renderSearchSources();
void renderChatList();
void renderProjectList();
void renderProjectActions();
void renderProjectModelPicker();
void renderProjectInstructions();
void renderControlsHelp();
void renderAiMenu();
void renderGlobalInstructions();
void renderWebConsoleMenu();
void renderDeviceMenu();
void renderUtilitiesMenu();
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
void openProjectList();
void openAiMenu();
void openWebConsole(Screen returnScreen);
cardputer::OperationResult runSshTerminal();
cardputer::OperationResult runSshTool();
cardputer::OperationResult runSshProfileStorageTest();
cardputer::OperationResult runSshSessionTest(bool testSftp);
cardputer::OperationResult runSshDemoTest();
cardputer::OperationResult saveAndApplyDeviceSettings(const cardputer::Settings& candidate);
bool keyboardWordContains(const Keyboard_Class::KeysState& keys, char expected);
bool cardputerEscapePressed();
void waitForModalKeyRelease();
bool confirmSshFingerprint(const cardputer::SshProfile& profile,
                           const String& keyType,
                           const String& fingerprint,
                           bool changed);
void submitPrompt();
void retryLastRequest();
void runUiSearchEndToEndTest();
void updateSerial();
void handleKeyboard();
void handleVoiceInput();
void speakLastAssistantResponse();
void openWifiPicker(Screen returnScreen);
bool runPureSelfTest();
cardputer::OperationResult applyDisplayAndCpuSettings(
    const cardputer::Settings& candidate);
cardputer::OperationResult applyWifiPowerSetting(
    const cardputer::Settings& candidate);
std::string joinedViewerLines(std::size_t firstLine, std::size_t lastLine);
cardputer::OperationResult playDocumentSpeechText(const std::string& text,
                                                  const String& source);
cardputer::OperationResult prepareDocumentSpeech();
cardputer::OperationResult speakEntireDocument();
void saveSelectedModel();
void selectWifiNetwork();
void connectSelectedWifi(const String& enteredPassword);

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

cardputer::OperationResult refreshChatPage(std::uint32_t offset)
{
    if (activeProjectId.isEmpty()) {
        return {false, "No active project is selected"};
    }
    const cardputer::ProjectChatsPageResult result = cardputer::listProjectChatsPage(
        activeProjectId, offset, cardputer::kMaximumProjectPageEntries);
    if (!result.success) {
        return {false, result.error};
    }
    chats = result.chats;
    chatPageOffset = offset;
    chatNextPageOffset = result.nextOffset;
    chatPageEof = result.eof;
    chatListIndex = 0;
    return {true, ""};
}

cardputer::OperationResult refreshChatList()
{
    chatPreviousPageOffsets.clear();
    return refreshChatPage(0);
}

cardputer::OperationResult saveActiveProjectSelection(const String& projectId)
{
    cardputer::ProjectStorageManifestResult manifest =
        cardputer::loadProjectStorageManifest();
    if (!manifest.success) {
        return {false, manifest.error};
    }
    manifest.manifest.activeProjectId = projectId;
    ++manifest.manifest.revision;
    return cardputer::saveProjectStorageManifest(manifest.manifest);
}

cardputer::OperationResult saveCurrentChat()
{
    if (!chatStorageReady || activeChatId.isEmpty()) {
        return {false, chatStorageError.isEmpty() ? String("Persistent chat storage is unavailable")
                                                  : chatStorageError};
    }
    cardputer::ChatDocumentResult loaded = cardputer::loadProjectChat(
        activeProjectId, activeChatId, 1, 1);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    loaded.chat.summary.title = activeChatTitle;
    const std::uint64_t updatedAt = currentChatTimestamp();
    if (updatedAt != 0) {
        loaded.chat.summary.updatedAt = updatedAt;
    }
    loaded.chat.summary.pinned = activeChatPinned;
    loaded.chat.summary.archived = activeChatArchived;
    loaded.chat.instructions = activeChatInstructions;
    loaded.chat.draft = inputBuffer;
    loaded.chat.sshToolsEnabled = activeChatSshToolsEnabled;
    const cardputer::OperationResult result = cardputer::saveProjectChatMetadata(loaded.chat);
    if (result.success) {
        persistedDraft = inputBuffer;
    }
    return result;
}

cardputer::OperationResult activateChat(const String& id)
{
    const cardputer::ChatDocumentResult loaded = cardputer::loadProjectChat(
        activeProjectId, id, 64, 65536);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    cardputer::ProjectDocumentResult project = cardputer::loadProject(activeProjectId);
    if (!project.success) {
        return {false, project.error};
    }
    project.project.activeChatId = id;
    cardputer::OperationResult activeResult = cardputer::saveProject(project.project);
    if (!activeResult.success) {
        return activeResult;
    }
    activeChatId = loaded.chat.summary.id;
    activeChatTitle = loaded.chat.summary.title;
    history = loaded.chat.messages;
    activeChatInstructions = loaded.chat.instructions;
    activeChatPinned = loaded.chat.summary.pinned;
    activeChatArchived = loaded.chat.summary.archived;
    activeChatArchivedMessageCount = 0;
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
    const cardputer::ChatDocumentResult created = cardputer::createProjectChat(
        activeProjectId, "New chat");
    if (!created.success) {
        return {false, created.error};
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
    cardputer::ProjectDocumentResult project = cardputer::loadProject(activeProjectId);
    if (!project.success) {
        return {false, project.error};
    }
    project.project.activeChatId = activeChatId;
    const cardputer::OperationResult saved = cardputer::saveProject(project.project);
    return saved.success ? refreshChatList() : saved;
}

cardputer::OperationResult initializeChats()
{
    const cardputer::ProjectStorageManifestResult manifest =
        cardputer::loadProjectStorageManifest();
    if (!manifest.success) {
        return {false, manifest.error};
    }
    activeProjectId = manifest.manifest.activeProjectId;
    const cardputer::ProjectDocumentResult project = cardputer::loadProject(activeProjectId);
    if (!project.success) {
        return {false, project.error};
    }
    activeProjectTitle = project.project.summary.title;
    cardputer::OperationResult result = refreshChatList();
    if (!result.success) {
        return result;
    }
    if (chats.empty()) {
        return createAndActivateChat();
    }
    const String storedId = project.project.activeChatId;
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

cardputer::OperationResult refreshProjectPage(std::uint32_t offset)
{
    const cardputer::ProjectsPageResult page = cardputer::listProjectsPage(
        offset, cardputer::kMaximumProjectPageEntries);
    if (!page.success) {
        return {false, page.error};
    }
    projects = page.projects;
    projectPageOffset = offset;
    projectNextPageOffset = page.nextOffset;
    projectPageEof = page.eof;
    projectListIndex = 0;
    return {true, ""};
}

cardputer::OperationResult activateProject(const String& projectId)
{
    if (!activeChatId.isEmpty()) {
        const cardputer::OperationResult saved = saveCurrentChat();
        if (!saved.success) {
            return saved;
        }
    }
    const cardputer::ProjectDocumentResult project = cardputer::loadProject(projectId);
    if (!project.success) {
        return {false, project.error};
    }
    cardputer::OperationResult result = saveActiveProjectSelection(projectId);
    if (!result.success) {
        return result;
    }
    activeProjectId = projectId;
    activeProjectTitle = project.project.summary.title;
    activeChatId.clear();
    result = refreshChatList();
    if (!result.success) {
        return result;
    }
    if (chats.empty()) {
        return createAndActivateChat();
    }
    if (!project.project.activeChatId.isEmpty()) {
        for (const cardputer::ChatSummary& chat : chats) {
            if (chat.id == project.project.activeChatId) {
                return activateChat(chat.id);
            }
        }
    }
    return activateChat(chats.front().id);
}

std::vector<String> projectListItems()
{
    std::vector<String> items = {"+ New project"};
    items.reserve(projects.size() + 3);
    for (const cardputer::ProjectSummary& project : projects) {
        const String marker = project.id == activeProjectId ? "[ON] " :
            (project.pinned ? "[PIN] " : (project.archived ? "[ARC] " : ""));
        items.push_back(marker + project.title + "  [" + project.chatCount + "]");
    }
    if (!projectPreviousPageOffsets.empty()) {
        items.push_back("< Previous projects");
    }
    if (!projectPageEof) {
        items.push_back("Next projects >");
    }
    return items;
}

void renderProjectList()
{
    cardputer::showSelectionList("PROJECTS", projectListItems(), projectListIndex,
                                 menuStatus.isEmpty()
                                     ? String("UP/DOWN  ENTER open  ESC home")
                                     : menuStatus);
}

std::vector<String> projectActionItems()
{
    const cardputer::ProjectDocumentResult project = cardputer::loadProject(selectedProjectId);
    if (!project.success) {
        return {"Back"};
    }
    return {
        "Open chats",
        project.project.model.isEmpty()
            ? String("Model: Global default")
            : String("Model: ") + project.project.model,
        project.project.instructions.empty()
            ? String("Project instructions: OFF")
            : String("Project instructions: ON"),
        "Context: " + String(project.project.contextByteBudget / 1024) + " KiB",
        "Output: " + String(project.project.maximumOutputTokens) + " tokens",
        String("Auto compact: ") + (project.project.automaticCompaction ? "ON" : "OFF"),
        "Duplicate project",
        project.project.summary.archived ? "Restore project" : "Archive project",
        "Export project bundle",
        "Back",
    };
}

std::vector<String> projectModelItems()
{
    std::vector<String> items = {"Use global default"};
    items.reserve(availableModels.size() + 1);
    items.insert(items.end(), availableModels.begin(), availableModels.end());
    return items;
}

void renderProjectModelPicker()
{
    cardputer::showSelectionList("PROJECT MODEL", projectModelItems(), modelPickerIndex,
                                 menuStatus.isEmpty()
                                     ? String("UP/DOWN  ENTER  ESC project")
                                     : menuStatus);
}

void renderProjectInstructions()
{
    cardputer::showTextEditor(
        "PROJECT INSTRUCTIONS", projectInstructionsInput, keyboardLayout,
        cardputer::kMaximumProjectInstructionsBytes, projectInstructionsStatus,
        "Applied after global instructions",
        "ENTER save  FN+DEL clear  ESC back");
}

void renderProjectActions()
{
    cardputer::showSelectionList(selectedProjectTitle, projectActionItems(),
                                 projectActionsIndex,
                                 menuStatus.isEmpty()
                                     ? String("UP/DOWN  ENTER  ESC projects")
                                     : menuStatus);
}

void openProjectActions(const cardputer::ProjectSummary& project)
{
    selectedProjectId = project.id;
    selectedProjectTitle = project.title;
    projectActionsIndex = 0;
    menuStatus = "";
    currentScreen = Screen::ProjectActions;
    renderProjectActions();
}

void openProjectList()
{
    projectPreviousPageOffsets.clear();
    const cardputer::OperationResult result = refreshProjectPage(0);
    if (!result.success) {
        menuStatus = result.error;
        renderCarousel();
        return;
    }
    menuStatus = "";
    currentScreen = Screen::ProjectList;
    for (std::size_t index = 0; index < projects.size(); ++index) {
        if (projects[index].id == activeProjectId) {
            projectListIndex = index + 1;
            break;
        }
    }
    renderProjectList();
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
    if (!chatPreviousPageOffsets.empty()) {
        items.push_back("< Previous chats");
    }
    if (!chatPageEof) {
        items.push_back("Next chats >");
    }
    return items;
}

void renderChatList()
{
    const String title = activeProjectTitle.isEmpty()
        ? String("CHATS") : activeProjectTitle;
    cardputer::showSelectionList(title, chatListItems(), chatListIndex,
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
        "View full history (" + String(selected.messageCount) + ")",
        selected.id == activeChatId && !retryPrompt.empty()
            ? String("Retry failed request") : String("Retry unavailable"),
        "Latest search sources",
        selected.pinned ? "Unpin chat" : "Pin chat",
        selected.archived ? "Restore from archive" : "Archive chat",
        "Duplicate chat",
        "Export Markdown",
        "Export project bundle",
        "Regenerate context summary",
        "Clear messages",
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

cardputer::OperationResult loadArchivedChatViewerPage(std::uint32_t offset)
{
    const cardputer::ArchivedMessagesPageResult loaded =
        cardputer::readProjectChatMessages(activeProjectId, selectedChatId,
                                           offset, 8, 12000);
    if (!loaded.success) {
        return {false, loaded.error};
    }

    std::vector<std::string> lines;
    for (const auto& message : loaded.messages) {
        const std::string heading = message.role == "user"
            ? std::string("YOU") : std::string("AI");
        const std::vector<std::string> wrapped = cardputer::wrapUtf8Text(
            heading + "\n" + message.content, 38);
        lines.insert(lines.end(), wrapped.begin(), wrapped.end());
        lines.emplace_back("");
    }
    if (lines.empty()) {
        lines.emplace_back("No archived messages.");
    }

    archivedChatViewerLines = std::move(lines);
    archivedChatViewerFirstLine = 0;
    archivedChatPageOffset = offset;
    archivedChatNextOffset = loaded.nextOffset;
    archivedChatEof = loaded.eof;
    return {true, ""};
}

void renderArchivedChatViewer()
{
    const String suffix = archivedChatEof ? String("END") : String("MORE");
    cardputer::showTextViewer("CHAT ARCHIVE - " + suffix,
                             archivedChatViewerLines,
                             archivedChatViewerFirstLine,
                             "UP/DOWN scroll  ESC back");
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
    const cardputer::ChatDocumentResult loaded = cardputer::loadProjectChat(
        activeProjectId, chat.id, 1, 1);
    selectedChatSshToolsEnabled = loaded.success && loaded.chat.sshToolsEnabled;
    chatActionsIndex = 0;
    menuStatus = loaded.success ? String("") : loaded.error;
    currentScreen = Screen::ChatActions;
    renderChatActions();
}

void renderChatInstructions()
{
    cardputer::showTextEditor("CHAT INSTRUCTIONS", instructionsInput, keyboardLayout,
                             cardputer::kMaximumProjectChatInstructionsBytes, instructionsStatus,
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
    case Screen::AiMenu:
        renderAiMenu();
        return;
    case Screen::ModelPicker:
        renderModelPicker();
        return;
    case Screen::GlobalInstructions:
        renderGlobalInstructions();
        return;
    case Screen::ProjectList:
        renderProjectList();
        return;
    case Screen::ProjectActions:
        renderProjectActions();
        return;
    case Screen::ProjectModelPicker:
        renderProjectModelPicker();
        return;
    case Screen::ProjectInstructions:
        renderProjectInstructions();
        return;
    case Screen::ChatList:
        renderChatList();
        return;
    case Screen::ChatActions:
        renderChatActions();
        return;
    case Screen::ArchivedChatViewer:
        renderArchivedChatViewer();
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
    case Screen::ClearChatConfirm:
        cardputer::showConfirmation("CLEAR MESSAGES", clearChatTitle,
                                    "ENTER clear  ESC cancel");
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

std::string effectiveProjectChatInstructions(const cardputer::ProjectDocument& project,
                                             const cardputer::ChatDocument& chat)
{
    std::string result;
    if (!project.instructions.empty()) {
        result = "Project instructions supplied by the user:\n" + project.instructions;
    }
    if (!chat.instructions.empty()) {
        if (!result.empty()) {
            result += "\n\n";
        }
        result += "Chat-specific instructions override conflicting project instructions:\n";
        result += chat.instructions;
    }
    if (!chat.contextSummary.empty()) {
        if (!result.empty()) {
            result += "\n\n";
        }
        result += "Conversation summary for turns omitted from the active context:\n";
        result += chat.contextSummary;
    }
    return result;
}

cardputer::OperationResult regenerateActiveContextSummary(
    const cardputer::ProjectDocument& project,
    const std::vector<cardputer::Message>& source,
    std::uint32_t summarizedMessageCount)
{
    if (source.empty()) {
        return {false, "No omitted messages are available to summarize"};
    }
    const cardputer::ChatDocumentResult current = cardputer::loadProjectChat(
        activeProjectId, activeChatId, 1, 1);
    if (!current.success) {
        return {false, current.error};
    }
    if (summarizedMessageCount < current.chat.summarizedMessageCount ||
        summarizedMessageCount > current.chat.summary.messageCount) {
        return {false, "Context summary message range is invalid"};
    }
    std::string transcript;
    transcript.reserve(std::min<std::size_t>(project.contextByteBudget, 32768));
    if (!current.chat.contextSummary.empty()) {
        transcript += "Previous summary:\n";
        transcript += current.chat.contextSummary;
        transcript += "\n\nNewly omitted messages:\n";
    }
    for (const cardputer::Message& message : source) {
        const std::string prefix = message.role == "user" ? "You: " : "AI: ";
        if (transcript.size() + prefix.size() + message.content.size() + 1 > 32768) {
            break;
        }
        transcript += prefix;
        transcript += message.content;
        transcript += '\n';
    }
    if (transcript.empty()) {
        return {false, "Messages selected for summary exceed the safe request budget"};
    }
    cardputer::Settings summarySettings = settings;
    summarySettings.globalInstructions = "";
    if (!project.model.isEmpty()) {
        summarySettings.model = project.model;
    }
    const std::vector<cardputer::Message> summaryRequest = {{
        "user",
        "Create a compact factual conversation summary. Preserve decisions, names, "
        "constraints, unresolved questions and file or command references. Do not add "
        "facts. Return only the summary.\n\n" + transcript,
    }};
    const cardputer::ChatResult summary = cardputer::streamChatCompletionWithBudget(
        summarySettings, summaryRequest,
        "This is a context compaction operation, not a user-facing answer.",
        768, [](const std::string&) {}, []() {
            M5Cardputer.update();
            return cardputerEscapePressed();
        });
    if (!summary.success) {
        return {false, "Context summary failed: " + summary.error};
    }
    cardputer::ChatDocumentResult stored = cardputer::loadProjectChat(
        activeProjectId, activeChatId, 1, 1);
    if (!stored.success) {
        return {false, stored.error};
    }
    stored.chat.contextSummary = summary.response;
    stored.chat.summarizedMessageCount = summarizedMessageCount;
    return cardputer::saveProjectChatMetadata(stored.chat);
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
    const bool useWebSearch = cardputer::requestsWebSearch(prompt);
    if (useWebSearch && !cardputer::webSearchSettingsAreComplete(settings)) {
        statusMessage = "Web search not configured; Fn+4 > Web setup";
        render();
        return;
    }

    const cardputer::ProjectDocumentResult activeProject =
        cardputer::loadProject(activeProjectId);
    const cardputer::ChatDocumentResult storedChat = cardputer::loadProjectChat(
        activeProjectId, activeChatId, 1, 1);
    if (!activeProject.success || !storedChat.success) {
        statusMessage = activeProject.success ? storedChat.error : activeProject.error;
        render();
        return;
    }
    cardputer::Settings requestSettings = settings;
    if (!activeProject.project.model.isEmpty()) {
        requestSettings.model = activeProject.project.model;
    }
    const std::string effectiveInstructions = effectiveProjectChatInstructions(
        activeProject.project, storedChat.chat);
    std::vector<cardputer::Message> pendingHistory = history;
    pendingHistory.push_back({"user", prompt});
    cardputer::ContextWindowResult pendingFit = cardputer::fitMessagesToByteBudget(
        pendingHistory, activeProject.project.contextByteBudget);
    const String pendingTitle = history.empty()
        ? String(cardputer::makeChatTitle(prompt, cardputer::kMaximumChatTitleCells).c_str())
        : activeChatTitle;
    const cardputer::OperationResult pendingSave = cardputer::appendProjectChatMessages(
        activeProjectId, activeChatId, {{"user", prompt}}, currentChatTimestamp());
    if (!pendingSave.success) {
        statusMessage = pendingSave.error;
        render();
        return;
    }
    history = std::move(pendingFit.retained);
    activeChatTitle = pendingTitle;
    inputBuffer.clear();
    const cardputer::OperationResult pendingMetadataSave = saveCurrentChat();
    if (!pendingMetadataSave.success) {
        statusMessage = pendingMetadataSave.error;
        render();
        return;
    }
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
    const cardputer::ChatToolPolicy toolPolicy = cardputer::resolveChatToolPolicy(
        settings, prompt, activeChatSshToolsEnabled, cardputer::sshToolIsAvailable());
    const bool useTools = cardputer::chatToolPolicyIsEnabled(toolPolicy);
    Serial.printf("INFO event=chat_route workspace_tools=%s web_search_available=%s web_search_intent=%s\n",
                  toolPolicy.workspaceEnabled ? "enabled" : "disabled",
                  toolPolicy.webSearchEnabled ? "yes" : "no",
                  useWebSearch ? "enabled" : "disabled");
    cardputer::markOperation(useTools ? "chat_tools" : "chat_stream");
    const cardputer::CancelCallback isCancelled = []() {
        M5Cardputer.update();
        return cardputerEscapePressed();
    };
    const cardputer::ChatResult result = useTools
        ? cardputer::streamChatCompletionWithToolsAndBudget(
              requestSettings, history, effectiveInstructions, toolPolicy.sshEnabled,
              activeProject.project.maximumOutputTokens, onText,
              [&toolPolicy, &isCancelled](const cardputer::ToolCall& call) {
                  statusMessage = "Tool: " + String(call.name.c_str());
                  render();
                  return cardputer::routeProjectToolCall(
                      settings, toolPolicy, activeProjectId, call, isCancelled);
              }, isCancelled)
        : cardputer::streamChatCompletionWithBudget(
              requestSettings, history, effectiveInstructions,
              activeProject.project.maximumOutputTokens, onText, isCancelled);
    cardputer::markOperation("idle");
    if (!result.success) {
        activeResponse = result.response;
        retryPrompt = prompt;
        statusMessage = result.error;
        const cardputer::OperationResult listResult = refreshChatList();
        if (!listResult.success) {
            statusMessage += "; chat list: " + listResult.error;
        }
        String safeError = result.error;
        safeError.replace("\r", " ");
        safeError.replace("\n", " ");
        if (safeError.length() > 180) {
            safeError = safeError.substring(0, 180) + "...";
        }
        Serial.printf("ERROR event=chat_completion result=failed error=%s\n",
                      safeError.c_str());
        render();
        return;
    }
    history.push_back({"assistant", result.response});
    retryPrompt.clear();
    cardputer::ContextWindowResult finalFit = cardputer::fitMessagesToByteBudget(
        history, activeProject.project.contextByteBudget);
    std::vector<cardputer::Message> compactedMessages;
    if (finalFit.droppedMessages > 0) {
        compactedMessages.assign(history.begin(),
                                 history.begin() + finalFit.droppedMessages);
    }
    history = std::move(finalFit.retained);
    activeResponse.clear();
    cardputer::OperationResult finalSave = cardputer::appendProjectChatMessages(
        activeProjectId, activeChatId, {{"assistant", result.response}},
        currentChatTimestamp());
    if (finalSave.success) {
        finalSave = saveCurrentChat();
    }
    if (finalSave.success && activeProject.project.automaticCompaction &&
        !compactedMessages.empty()) {
        finalSave = regenerateActiveContextSummary(
            activeProject.project, compactedMessages,
            storedChat.chat.summarizedMessageCount +
                static_cast<std::uint32_t>(compactedMessages.size()));
    }
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
        cardputer::showBusyScreen("SPEAKING", "ESC stops download or playback");
        cardputer::markOperation("tts_auto");
        const cardputer::OperationResult speech = cardputer::synthesizeAndPlaySpeechControlled(
            settings, result.response, []() {
                M5Cardputer.update();
                return cardputerEscapePressed()
                    ? cardputer::SpeechPlaybackCommand::Stop
                    : cardputer::SpeechPlaybackCommand::Continue;
            });
        cardputer::markOperation("idle");
        const bool stopped = speech.error == "Speech playback stopped" ||
            speech.error == "Speech synthesis canceled by user";
        if (stopped) {
            waitForModalKeyRelease();
            pressedKeys.clear();
            setTransientStatus("Speech stopped", 1500);
            Serial.println("INFO event=tts_playback result=stopped source=auto");
        } else if (!speech.success) {
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
    if (!chatStorageReady || activeProjectId.isEmpty() || activeChatId.isEmpty()) {
        Serial.println("E2ETEST result=failed stage=storage_not_ready");
        return;
    }
    const String originalProjectId = activeProjectId;
    const String originalChatId = activeChatId;
    const Screen originalScreen = currentScreen;
    const cardputer::ChatDocumentResult created = cardputer::createProjectChat(
        activeProjectId, "E2E search " + String(millis()));
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
    const String submissionStatus = statusMessage;
    const String testChatId = activeChatId;
    const cardputer::OperationResult cleanup = cardputer::deleteProjectChat(
        activeProjectId, testChatId);
    const cardputer::ChatDocumentResult restored = cardputer::loadProjectChat(
        originalProjectId, originalChatId, 64, 65536);
    if (!restored.success) {
        Serial.println("E2ETEST result=failed stage=restore_chat");
        statusMessage = restored.error;
        render();
        return;
    }
    activeProjectId = originalProjectId;
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
    String safeError = passed ? String("none")
        : (!responseReceived && !submissionStatus.isEmpty()
            ? submissionStatus
            : statusMessage);
    safeError.replace("\r", " ");
    safeError.replace("\n", " ");
    if (safeError.length() > 180) {
        safeError = safeError.substring(0, 180) + "...";
    }
    Serial.printf("E2ETEST result=%s response=%s cleanup=%s heap=%u largest_heap=%u stack_free=%u error=%s\n",
                  passed ? "pass" : "failed",
                  responseReceived ? "yes" : "no",
                  cleanup.success ? "yes" : "no",
                  static_cast<unsigned int>(ESP.getFreeHeap()),
                  static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                  static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)),
                  safeError.c_str());
    render();
}

}  // namespace

void setup()
{
    usb_serial_jtag_ll_phy_enable_external(false);
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
    const cardputer::OperationResult audioPowerResult =
        cardputer::initializeCardputerAdvAudioPowerControl();
    if (!audioPowerResult.success) {
        cardputer::showFatalError(audioPowerResult.error);
        Serial.printf("FATAL event=audio_power_control reason=%s\n",
                      audioPowerResult.error.c_str());
        while (true) {
            delay(1000);
        }
    }
    Serial.println("AUDIO_CODEC power_control=ready");
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
    cardputer::PythonHandoffRequest pythonHandoff =
        cardputer::consumePythonHandoffRequest();
    if (!pythonHandoff.success) {
        Serial.printf("ERROR event=python_handoff reason=%s\n",
                      pythonHandoff.error.c_str());
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
    if (voiceStorageReady && pythonHandoff.success) {
        const cardputer::PythonHandoffRequest sdHandoff =
            cardputer::consumePythonSdHandoffRequest();
        if (!sdHandoff.success) {
            pythonHandoff = sdHandoff;
            Serial.printf("ERROR event=python_sd_handoff reason=%s\n",
                          sdHandoff.error.c_str());
        } else if (sdHandoff.openWebConsole) {
            pythonHandoff.openWebConsole = true;
        }
    }

    const cardputer::OperationResult legacyChatResult = voiceStorageReady
        ? cardputer::initializeChatStorage()
        : cardputer::OperationResult{false, voiceStorageError};

    const cardputer::OperationResult workspaceResult = legacyChatResult.success
        ? cardputer::initializeFileWorkspace()
        : cardputer::OperationResult{false, legacyChatResult.error};
    cardputer::ProjectMigrationResult migrationResult = {
        false, false, "", 0,
        workspaceResult.success ? String() : workspaceResult.error,
    };
    if (workspaceResult.success) {
        cardputer::showBusyScreen("CARDMIND", "Checking project storage...");
        migrationResult = cardputer::migrateLegacyStorageToProjects();
    }
    const cardputer::OperationResult chatResult = migrationResult.success
        ? initializeChats()
        : cardputer::OperationResult{false, migrationResult.error};
    chatStorageReady = chatResult.success;
    chatStorageError = chatResult.success ? String() : chatResult.error;
    Serial.printf("CHAT_STORAGE result=%s count=%u\n",
                  chatStorageReady ? "ready" : "failed",
                  static_cast<unsigned int>(chats.size()));
    fileWorkspaceReady = workspaceResult.success && migrationResult.success &&
        chatStorageReady;
    fileWorkspaceError = fileWorkspaceReady ? String() : migrationResult.error;
    Serial.printf("FILE_WORKSPACE result=%s\n", fileWorkspaceReady ? "ready" : "failed");
    Serial.printf("PROJECT_STORAGE result=%s migrated=%s chats=%u\n",
                  migrationResult.success ? "ready" : "failed",
                  migrationResult.migrated ? "yes" : "no",
                  static_cast<unsigned int>(migrationResult.migratedChats));

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
    menuStatus = !pythonHandoff.success
        ? pythonHandoff.error
        : !fileWorkspaceReady
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
    if (pythonHandoff.success && pythonHandoff.openWebConsole) {
        webConsoleStartupPending = true;
        webConsoleStartupNotBefore = millis() + 750U;
        Serial.println("PYTHON_HANDOFF action=queue_web_console");
    }
    Serial.println("READY");
}

void loop()
{
    M5Cardputer.update();
    if (webConsoleStartupPending &&
        static_cast<std::int32_t>(millis() - webConsoleStartupNotBefore) >= 0 &&
        M5Cardputer.Keyboard.keyList().empty()) {
        webConsoleStartupPending = false;
        Serial.println("PYTHON_HANDOFF action=open_web_console");
        openWebConsole(Screen::MainCarousel);
        return;
    }
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
        while (M5Cardputer.Speaker.isPlaying()) {
            delay(1);
        }
        const cardputer::OperationResult audioPowerResult =
            cardputer::powerDownCardputerAdvAudio();
        menuStatus = audioPowerResult.success
            ? String("Timer finished")
            : audioPowerResult.error;
        if (currentScreen == Screen::TimerMenu || currentScreen == Screen::UtilitiesMenu) {
            render();
        }
    } else if (timerRunning && currentScreen == Screen::TimerMenu &&
               millis() - lastTimerRenderAt >= 1000U) {
        lastTimerRenderAt = millis();
        renderTimerMenu();
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
