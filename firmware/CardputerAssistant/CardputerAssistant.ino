#include <M5Cardputer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
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
#include "src/chat_storage.h"
#include "src/crash_journal.h"
#include "src/document_reader.h"
#include "src/file_workspace.h"
#include "src/offline_tools.h"
#include "src/ota_update.h"
#include "src/provisioning.h"
#include "src/python_mode.h"
#include "src/project_chat_storage.h"
#include "src/project_bundle.h"
#include "src/instruction_policy.h"
#include "src/project_storage.h"
#include "src/sd_storage.h"
#include "src/stt_client.h"
#include "src/storage.h"
#include "src/storage_migration.h"
#include "src/ssh_client.h"
#include "src/sftp_tool.h"
#include "src/ssh_command_output.h"
#include "src/ssh_terminal.h"
#include "src/ssh_tool.h"
#include "src/text_utils.h"
#include "src/tool_router.h"
#include "src/tool_activity.h"
#include "src/tts_client.h"
#include "src/ui.h"
#include "src/voice_input.h"
#include "src/web_search_client.h"
#include "src/web_console.h"
#include "src/wifi_networks.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <limits>
#include <string>
#include <utility>
#include <vector>

SET_LOOP_TASK_STACK_SIZE(16384);

namespace {

cardputer::OperationResult runSshCommandOutputStorageTest();
cardputer::OperationResult runSshCommandOutputRemoteTest(
    String& retainedName,
    std::uint32_t& outputBytes);
cardputer::OperationResult cleanupSshCommandOutputRemoteTest(
    bool& alreadyAbsent,
    bool& removed);
cardputer::OperationResult runModelSftpRemoteTest(bool& cleanupComplete);

constexpr const char* kFirmwareVersion = "1.12.1";
constexpr std::size_t kMaximumInputBytes = 16384;
constexpr std::size_t kMaximumWifiPasswordBytes = 63;
constexpr std::uint8_t kTtsVolumeStep = 64;
constexpr std::uint32_t kBatteryRefreshIntervalMs = 30000;
constexpr std::uint32_t kDraftAutosaveIdleMs = 1500;
constexpr std::uint32_t kDraftAutosaveMaximumDirtyMs = 30000;
constexpr std::uint32_t kSdStateRefreshIntervalMs = 1000;
constexpr std::size_t kFileViewerChunkBytes = 2048;
constexpr std::size_t kFileViewerPageLines = 8;
constexpr std::size_t kFileEditorMaximumBytes = 4096;
constexpr cardputer::ToolMessageIntent kAutomaticToolMessageIntent = {
    cardputer::ToolMessageIntentMode::Auto,
    0,
};

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
    ToolActivity,
    ModelPicker,
    GlobalInstructions,
    GlobalMasterPolicy,
    NewChatDefaultsPolicy,
    ProjectList,
    ProjectActions,
    ProjectModelPicker,
    ProjectToolPolicy,
    ProjectInstructions,
    ProjectRename,
    ChatList,
    ChatActions,
    ChatModelPicker,
    ChatToolPolicy,
    ChatCapabilityStatus,
    ComposerCapabilities,
    PendingToolPreview,
    PendingToolActions,
    ArchivedChatViewer,
    SearchSources,
    SearchSourceViewer,
    ChatInstructions,
    RequestInstructions,
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
    ImportProject,
};

cardputer::Settings settings;
std::vector<cardputer::Message> history;
std::vector<cardputer::ChatSummary> chats;
std::vector<cardputer::ProjectSummary> projects;
std::vector<String> availableModels;
std::string inputBuffer;
std::string persistedDraft;
std::uint32_t lastDraftAutosaveAt = 0;
std::uint32_t lastDraftEditAt = 0;
std::uint32_t draftDirtySinceAt = 0;
std::string activeResponse;
std::string retryPrompt;
String retryChatId;
std::uint32_t retryOutputTokens = 0;
cardputer::ToolMessageIntent retryToolIntent = kAutomaticToolMessageIntent;
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
String activeChatModel;
std::string activeChatInstructions;
bool activeChatPinned = false;
bool activeChatArchived = false;
std::uint32_t activeChatArchivedMessageCount = 0;
cardputer::ScopedToolPermissionPolicy activeChatToolPolicy =
    cardputer::defaultNewChatToolPermissionPolicy();
String activeChatSshProfile;
cardputer::ProjectDocument activeProjectDocument;
bool chatStorageReady = false;
String chatStorageError;
bool chatStorageInitialized = false;
String chatStorageInitializationError;
bool fileWorkspaceReady = false;
String fileWorkspaceError;
bool fileWorkspaceInitialized = false;
String fileWorkspaceInitializationError;
std::size_t chatListIndex = 0;
std::uint32_t chatPageOffset = 0;
std::uint32_t chatNextPageOffset = 0;
bool chatPageEof = true;
std::vector<std::uint32_t> chatPreviousPageOffsets;
std::size_t projectListIndex = 0;
std::size_t projectActionsIndex = 0;
std::string projectInstructionsInput;
String projectInstructionsStatus;
std::string projectRenameInput;
String projectRenameStatus;
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
String selectedChatModel;
cardputer::ContextUsage selectedChatContextUsage = {0, 0, 0, 0, 0};
bool selectedChatContextUsageReady = false;
String requestOutputOverrideChatId;
std::uint32_t requestOutputOverrideTokens = 0;
String requestInstructionsOverrideChatId;
std::string requestInstructionsOverride;
std::string retryRequestInstructions;
cardputer::ToolMessageIntent composerToolIntent = kAutomaticToolMessageIntent;
std::string instructionsInput;
String instructionsStatus;
std::string requestInstructionsInput;
String requestInstructionsStatus;
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
std::size_t fileActionsIndex = 0;
std::size_t diagnosticsIndex = 0;
std::size_t controlsHelpIndex = 0;
std::size_t aiMenuIndex = 0;
std::size_t modelPickerIndex = 0;
std::size_t capabilityPolicyIndex = 0;
std::size_t composerCapabilitiesIndex = 0;
Screen composerCapabilitiesReturnScreen = Screen::Chat;
std::size_t chatCapabilityStatusFirstLine = 0;
std::size_t chatCapabilityStatusLineCount = 0;
std::size_t pendingToolActionsIndex = 0;
std::size_t pendingToolPreviewFirstLine = 0;
std::size_t pendingToolPreviewLineCount = 0;
std::vector<std::string> pendingToolPreviewLines;
bool pendingToolPreviewActionable = false;
Screen pendingToolReturnScreen = Screen::AiMenu;

struct DevicePendingContinuationContext {
    bool present = false;
    String pendingId;
    String projectId;
    String chatId;
    cardputer::ResolvedProjectRequestPolicy requestPolicy = {"", 0, 0, false};
    String globalInstructions;
    std::string scopedInstructions;
    cardputer::ToolMessageIntent intent = kAutomaticToolMessageIntent;
};

DevicePendingContinuationContext devicePendingContext;
std::size_t wifiPickerIndex = 0;
std::vector<cardputer::WifiNetwork> scannedWifiNetworks;
std::vector<cardputer::WorkspaceFile> workspaceFiles;
String fileViewerName;
std::string fileViewerContent;
std::vector<std::string> fileViewerLines;
std::vector<std::uint32_t> fileViewerPreviousOffsets;
cardputer::DocumentReaderMode fileReaderMode = cardputer::DocumentReaderMode::Text;
std::size_t fileViewerFirstLine = 0;
std::vector<std::string> toolActivityLines;
std::size_t toolActivityFirstLine = 0;
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
bool voiceStorageInitialized = false;
String voiceStorageInitializationError;
cardputer::SdStorageStatus currentSdStorageStatus = {
    cardputer::SdStorageState::Missing, 0, 0, "microSD state has not been checked",
};
std::uint32_t lastSdStateRefreshAt = 0;
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
bool crashJournalInitialized = false;
String crashJournalInitializationError;
bool sshStorageReady = false;
std::uint64_t cachedSshToolProfileId = 0;
String sshStorageError;
bool sshStorageInitialized = false;
String sshStorageInitializationError;
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
void renderRequestInstructions();
void renderSearchSources();
void renderChatList();
void renderProjectList();
void renderProjectActions();
void renderProjectModelPicker();
void renderProjectToolPolicy();
void renderProjectInstructions();
void renderProjectRename();
void renderControlsHelp();
void renderAiMenu();
void renderToolActivity();
void renderGlobalInstructions();
void renderGlobalMasterPolicy();
void renderNewChatDefaultsPolicy();
void renderChatModelPicker();
void renderChatToolPolicy();
void renderChatCapabilityStatus();
cardputer::ChatCapabilityStates activeChatCapabilityStates();
void renderComposerCapabilities();
void renderPendingToolPreview();
void renderPendingToolActions();
String composerCapabilitiesLabel();
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
void openToolActivity();
void openPendingToolPreview();
void allowPendingToolOnce();
void allowPendingToolForChat();
void denyPendingTool();
void acknowledgeInterruptedPendingTool();
void clearDevicePendingContext();
cardputer::OperationResult captureDevicePendingContext(
    const String& projectId,
    const String& chatId,
    const cardputer::ResolvedProjectRequestPolicy& requestPolicy,
    const String& globalInstructions,
    std::string scopedInstructions,
    const cardputer::ToolMessageIntent& intent);
void openWebConsole(Screen returnScreen);
cardputer::OperationResult runSshTerminal();
cardputer::OperationResult runSshTool();
cardputer::OperationResult runSshProfileStorageTest();
cardputer::OperationResult runSshCommandOptionsTest();
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
void executeStoredPromptRequest(const std::string& prompt,
                                const cardputer::ProjectDocument& project,
                                const cardputer::ChatDocument& storedChat,
                                std::string requestInstructions,
                                cardputer::ResolvedProjectRequestPolicy requestPolicy,
                                const cardputer::ToolMessageIntent requestIntent,
                                const cardputer::ToolRequestPlan requestPlan);
void retryLastRequest();
void runUiSearchEndToEndTest();
void updateSerial();
bool refreshRuntimeSdState();
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
    const cardputer::SdStorageStatus storage = cardputer::inspectSdStorage();
    if (storage.state == cardputer::SdStorageState::Full) {
        menuStatus = storage.error;
        return {true, ""};
    }
    if (storage.state != cardputer::SdStorageState::Ready) {
        return {false, storage.error};
    }
    cardputer::ProjectStorageManifestResult manifest =
        cardputer::loadProjectStorageManifest();
    if (!manifest.success) {
        return {false, manifest.error};
    }
    if (manifest.manifest.activeProjectId == projectId) {
        return {true, ""};
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
    const cardputer::OperationResult access = cardputer::requireSdWriteAccess(
        0, cardputer::kStorageOperationalFloorBytes);
    if (!access.success) return access;
    cardputer::ChatDocumentResult loaded = cardputer::loadProjectChatMetadata(
        activeProjectId, activeChatId);
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
    loaded.chat.toolPolicy = activeChatToolPolicy;
    loaded.chat.model = activeChatModel;
    const cardputer::OperationResult result = cardputer::saveProjectChatMetadata(loaded.chat);
    if (result.success) {
        persistedDraft = inputBuffer;
        draftDirtySinceAt = 0;
    }
    return result;
}

void clearRetryRequestState()
{
    retryPrompt.clear();
    retryChatId.clear();
    retryOutputTokens = 0;
    std::string().swap(retryRequestInstructions);
    retryToolIntent = kAutomaticToolMessageIntent;
}

void captureRetryRequestState(const std::string& prompt,
                              const String& chatId,
                              std::uint32_t outputTokens,
                              const std::string& requestInstructions,
                              const cardputer::ToolMessageIntent& intent)
{
    retryPrompt = prompt;
    retryChatId = chatId;
    retryOutputTokens = outputTokens;
    retryRequestInstructions = requestInstructions;
    retryToolIntent = intent;
}

cardputer::ToolMessageIntent consumeComposerToolIntent()
{
    const cardputer::ToolMessageIntent intent = composerToolIntent;
    composerToolIntent = kAutomaticToolMessageIntent;
    return intent;
}

String toolRequestPlanError(const cardputer::ToolRequestPlan& plan)
{
    if (plan.error != cardputer::ToolPolicyContractError::None) {
        return "Selected tool intent is invalid";
    }
    if (plan.missingRequiredGroups != 0) {
        return "A required capability is denied or unavailable";
    }
    return "";
}

const char* capabilityLabel(std::size_t index)
{
    static constexpr const char* labels[cardputer::kToolCapabilityCount] = {
        "Web search",
        "Web fetch",
        "Files read",
        "Files write/delete",
        "SSH read",
        "SSH mutate",
        "SFTP read/write",
        "Python write/run",
    };
    return index < cardputer::kToolCapabilityCount ? labels[index] : "Invalid";
}

const char* toolPermissionLabel(cardputer::ToolPermission permission)
{
    switch (permission) {
        case cardputer::ToolPermission::Off: return "Off";
        case cardputer::ToolPermission::Ask: return "Ask";
        case cardputer::ToolPermission::Allow: return "Allow";
        case cardputer::ToolPermission::Count: break;
    }
    return "Invalid";
}

const char* scopedToolPermissionLabel(cardputer::ScopedToolPermission permission)
{
    switch (permission) {
        case cardputer::ScopedToolPermission::Inherit: return "Inherit";
        case cardputer::ScopedToolPermission::Off: return "Off";
        case cardputer::ScopedToolPermission::Ask: return "Ask";
        case cardputer::ScopedToolPermission::Allow: return "Allow";
        case cardputer::ScopedToolPermission::Count: break;
    }
    return "Invalid";
}

const char* toolPermissionDecisionLabel(cardputer::ToolPermissionDecision decision)
{
    switch (decision) {
        case cardputer::ToolPermissionDecision::Deny: return "Off";
        case cardputer::ToolPermissionDecision::Ask: return "Ask";
        case cardputer::ToolPermissionDecision::Allow: return "Allow";
        case cardputer::ToolPermissionDecision::Unavailable: return "Unavailable";
    }
    return "Invalid";
}

const char* toolPermissionSourceLabel(cardputer::ToolPermissionSource source)
{
    switch (source) {
        case cardputer::ToolPermissionSource::None: return "None";
        case cardputer::ToolPermissionSource::BuiltIn: return "Built-in";
        case cardputer::ToolPermissionSource::Global: return "Global";
        case cardputer::ToolPermissionSource::Project: return "Project";
        case cardputer::ToolPermissionSource::Chat: return "Chat";
        case cardputer::ToolPermissionSource::Message: return "Message";
        case cardputer::ToolPermissionSource::Availability: return "Unavailable";
    }
    return "Invalid";
}

cardputer::ToolPermission nextToolPermission(cardputer::ToolPermission permission)
{
    switch (permission) {
        case cardputer::ToolPermission::Off: return cardputer::ToolPermission::Ask;
        case cardputer::ToolPermission::Ask: return cardputer::ToolPermission::Allow;
        case cardputer::ToolPermission::Allow: return cardputer::ToolPermission::Off;
        case cardputer::ToolPermission::Count: break;
    }
    return cardputer::ToolPermission::Off;
}

cardputer::ScopedToolPermission nextScopedToolPermission(
    cardputer::ScopedToolPermission permission)
{
    switch (permission) {
        case cardputer::ScopedToolPermission::Inherit:
            return cardputer::ScopedToolPermission::Off;
        case cardputer::ScopedToolPermission::Off:
            return cardputer::ScopedToolPermission::Ask;
        case cardputer::ScopedToolPermission::Ask:
            return cardputer::ScopedToolPermission::Allow;
        case cardputer::ScopedToolPermission::Allow:
            return cardputer::ScopedToolPermission::Inherit;
        case cardputer::ScopedToolPermission::Count:
            break;
    }
    return cardputer::ScopedToolPermission::Inherit;
}

std::vector<String> capabilityPolicyItems(
    const cardputer::ToolPermissionPolicy& policy)
{
    std::vector<String> items;
    items.reserve(cardputer::kToolCapabilityCount + 1);
    for (std::size_t index = 0; index < cardputer::kToolCapabilityCount; ++index) {
        items.push_back(String(capabilityLabel(index)) + ": " +
                        toolPermissionLabel(policy[index]));
    }
    items.push_back("Back");
    return items;
}

std::vector<String> capabilityPolicyItems(
    const cardputer::ScopedToolPermissionPolicy& policy)
{
    std::vector<String> items;
    items.reserve(cardputer::kToolCapabilityCount + 1);
    for (std::size_t index = 0; index < cardputer::kToolCapabilityCount; ++index) {
        items.push_back(String(capabilityLabel(index)) + ": " +
                        scopedToolPermissionLabel(policy[index]));
    }
    items.push_back("Back");
    return items;
}

void clearChatScopedEphemeralState()
{
    requestOutputOverrideChatId.clear();
    requestOutputOverrideTokens = 0;
    requestInstructionsOverrideChatId.clear();
    std::string().swap(requestInstructionsOverride);
    composerToolIntent = kAutomaticToolMessageIntent;
    clearRetryRequestState();
}

cardputer::OperationResult setRequestInstructionsOverride(
    const String& chatId,
    const std::string& instructions)
{
    if (chatId.isEmpty()) {
        return {false, "Request instructions require a selected chat"};
    }
    if (instructions.size() > cardputer::kMaximumRequestInstructionsBytes ||
        !cardputer::isValidUtf8(instructions)) {
        return {false, "Request instructions must be valid UTF-8 up to 2048 bytes"};
    }
    requestInstructionsOverrideChatId = instructions.empty() ? String() : chatId;
    requestInstructionsOverride = instructions;
    return {true, ""};
}

std::string consumeRequestInstructionsOverride(const String& chatId)
{
    if (requestInstructionsOverrideChatId != chatId) {
        return {};
    }
    requestInstructionsOverrideChatId.clear();
    std::string result = std::move(requestInstructionsOverride);
    std::string().swap(requestInstructionsOverride);
    return result;
}

cardputer::OperationResult activateChat(const String& id)
{
    const bool switchingChat = !activeChatId.isEmpty() && activeChatId != id;
    const cardputer::ChatDocumentResult loaded = cardputer::loadProjectChat(
        activeProjectId, id, 64, 65536);
    if (!loaded.success) {
        return {false, loaded.error};
    }
    cardputer::ProjectDocumentResult project = cardputer::loadProject(activeProjectId);
    if (!project.success) {
        return {false, project.error};
    }
    if (project.project.activeChatId != id) {
        const cardputer::SdStorageStatus storage = cardputer::inspectSdStorage();
        if (storage.state == cardputer::SdStorageState::Ready) {
            project.project.activeChatId = id;
            const cardputer::OperationResult activeResult =
                cardputer::saveProject(project.project);
            if (!activeResult.success) {
                return activeResult;
            }
        } else if (storage.state == cardputer::SdStorageState::Full) {
            menuStatus = storage.error;
        } else {
            return {false, storage.error};
        }
    }
    if (switchingChat) {
        clearChatScopedEphemeralState();
    }
    activeChatId = loaded.chat.summary.id;
    activeChatTitle = loaded.chat.summary.title;
    activeChatModel = loaded.chat.model;
    history = cardputer::unsummarizedChatTail(loaded.chat);
    activeChatInstructions = loaded.chat.instructions;
    activeChatPinned = loaded.chat.summary.pinned;
    activeChatArchived = loaded.chat.summary.archived;
    activeChatArchivedMessageCount = 0;
    activeChatToolPolicy = loaded.chat.toolPolicy;
    activeChatSshProfile = loaded.chat.sshProfile;
    activeProjectDocument = project.project;
    activeResponse.clear();
    inputBuffer = loaded.chat.draft;
    persistedDraft = inputBuffer;
    lastDraftAutosaveAt = millis();
    lastDraftEditAt = lastDraftAutosaveAt;
    draftDirtySinceAt = 0;
    scrollOffset = 0;
    return {true, ""};
}

cardputer::OperationResult createAndActivateChat()
{
    const cardputer::OperationResult access = cardputer::requireSdWriteAccess(
        0, cardputer::kStorageOperationalFloorBytes);
    if (!access.success) return access;
    const cardputer::ChatDocumentResult created = cardputer::createProjectChat(
        activeProjectId, "New chat", settings.newChatToolPolicy);
    if (!created.success) {
        return {false, created.error};
    }
    clearChatScopedEphemeralState();
    activeChatId = created.chat.summary.id;
    activeChatTitle = created.chat.summary.title;
    activeChatModel = created.chat.model;
    activeChatInstructions.clear();
    activeChatPinned = false;
    activeChatArchived = false;
    activeChatArchivedMessageCount = 0;
    activeChatToolPolicy = created.chat.toolPolicy;
    activeChatSshProfile = created.chat.sshProfile;
    history.clear();
    activeResponse.clear();
    inputBuffer.clear();
    persistedDraft.clear();
    lastDraftAutosaveAt = millis();
    lastDraftEditAt = lastDraftAutosaveAt;
    draftDirtySinceAt = 0;
    scrollOffset = 0;
    cardputer::ProjectDocumentResult project = cardputer::loadProject(activeProjectId);
    if (!project.success) {
        return {false, project.error};
    }
    activeProjectDocument = project.project;
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
        return activateChat(storedId);
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
    if (!activeChatId.isEmpty() && inputBuffer != persistedDraft) {
        const cardputer::SdStorageStatus storage = cardputer::inspectSdStorage();
        if (storage.state == cardputer::SdStorageState::Ready) {
            const cardputer::OperationResult saved = saveCurrentChat();
            if (!saved.success) {
                return saved;
            }
        } else if (storage.state == cardputer::SdStorageState::Full &&
                   inputBuffer == persistedDraft) {
            menuStatus = storage.error;
        } else {
            return {false, storage.error};
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
    clearChatScopedEphemeralState();
    activeProjectId = projectId;
    activeProjectTitle = project.project.summary.title;
    activeProjectDocument = project.project;
    activeChatId.clear();
    activeChatSshProfile.clear();
    result = refreshChatList();
    if (!result.success) {
        return result;
    }
    if (chats.empty()) {
        return createAndActivateChat();
    }
    if (!project.project.activeChatId.isEmpty()) {
        return activateChat(project.project.activeChatId);
    }
    return activateChat(chats.front().id);
}

std::vector<String> projectListItems()
{
    std::vector<String> items = {"+ New project", "Import project bundle"};
    items.reserve(projects.size() + 4);
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
        "Rename project",
        "Duplicate project",
        project.project.summary.archived ? "Restore project" : "Archive project",
        "Export project bundle",
        "Capability policies",
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

bool refreshRuntimeSdState()
{
    const cardputer::SdStorageStatus status = cardputer::inspectSdStorage();
    const bool changed = status.state != currentSdStorageStatus.state ||
        status.error != currentSdStorageStatus.error;
    currentSdStorageStatus = status;
    const bool readable = status.state == cardputer::SdStorageState::Ready ||
        status.state == cardputer::SdStorageState::Full;
    const bool writable = status.state == cardputer::SdStorageState::Ready;
    voiceStorageReady = voiceStorageInitialized && readable;
    chatStorageReady = chatStorageInitialized && readable;
    fileWorkspaceReady = fileWorkspaceInitialized && readable;
    crashJournalReady = crashJournalInitialized && writable;
    sshStorageReady = sshStorageInitialized && writable;
    voiceStorageError = readable ? voiceStorageInitializationError : status.error;
    chatStorageError = readable ? chatStorageInitializationError : status.error;
    fileWorkspaceError = readable ? fileWorkspaceInitializationError : status.error;
    crashJournalError = writable ? crashJournalInitializationError : status.error;
    sshStorageError = writable ? sshStorageInitializationError : status.error;
    lastSdStateRefreshAt = millis();
    return changed;
}

void renderProjectRename()
{
    cardputer::showTextEditor(
        "RENAME PROJECT", projectRenameInput, keyboardLayout,
        cardputer::kMaximumProjectTitleBytes, projectRenameStatus,
        "Project storage ID does not change",
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
    projectListIndex = 0;
    for (std::size_t index = 0; index < projects.size(); ++index) {
        if (projects[index].id == activeProjectId) {
            projectListIndex = index + 2;
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
        selectedChatModel.isEmpty()
            ? String("Chat model: Project default")
            : String("Chat model: ") + selectedChatModel,
        selectedChatContextUsageReady
            ? "Ctx:" +
                  String(static_cast<unsigned int>(
                      selectedChatContextUsage.retainedBytes)) +
                  "B/" + String(selectedChatContextUsage.retainedMessages) +
                  "m Sum:" + String(selectedChatContextUsage.summarizedMessages) +
                  "/" + String(selectedChatContextUsage.totalMessages)
            : "View full history (" + String(selected.messageCount) + ")",
        selected.id == retryChatId && !retryPrompt.empty()
            ? String("Retry failed request") : String("Retry unavailable"),
        "Latest search sources",
        selected.pinned ? "Unpin chat" : "Pin chat",
        selected.archived ? "Restore from archive" : "Archive chat",
        "Duplicate chat",
        "Export Markdown",
        "Export project bundle",
        "Regenerate context summary",
        selectedChatId == requestOutputOverrideChatId && requestOutputOverrideTokens != 0
            ? "Next output: " + String(requestOutputOverrideTokens) + " tokens"
            : String("Next output: project default"),
        selectedChatId == requestInstructionsOverrideChatId &&
                !requestInstructionsOverride.empty()
            ? String("Next instructions: ON")
            : String("Next instructions: OFF"),
        "Capability policies",
        "Capability status",
        "Next capabilities: " + composerCapabilitiesLabel(),
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
        activeProjectId, chat.id, 64, 65536);
    const cardputer::ProjectDocumentResult project = cardputer::loadProject(
        activeProjectId);
    selectedChatModel = loaded.success ? loaded.chat.model : String();
    selectedChatContextUsageReady = loaded.success && project.success;
    if (selectedChatContextUsageReady) {
        selectedChatContextUsage = cardputer::resolveContextUsage(
            loaded.chat, project.project.contextByteBudget);
    } else {
        selectedChatContextUsage = {0, 0, 0, 0, 0};
    }
    chatActionsIndex = 0;
    menuStatus = !loaded.success ? loaded.error
        : (!project.success ? project.error : String(""));
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
    if (chatStorageReady && !activeChatId.isEmpty() && inputBuffer != persistedDraft) {
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
                            activeChatCapabilityStates(),
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
    case Screen::ToolActivity:
        renderToolActivity();
        return;
    case Screen::ModelPicker:
        renderModelPicker();
        return;
    case Screen::GlobalInstructions:
        renderGlobalInstructions();
        return;
    case Screen::GlobalMasterPolicy:
        renderGlobalMasterPolicy();
        return;
    case Screen::NewChatDefaultsPolicy:
        renderNewChatDefaultsPolicy();
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
    case Screen::ProjectToolPolicy:
        renderProjectToolPolicy();
        return;
    case Screen::ProjectInstructions:
        renderProjectInstructions();
        return;
    case Screen::ProjectRename:
        renderProjectRename();
        return;
    case Screen::ChatList:
        renderChatList();
        return;
    case Screen::ChatActions:
        renderChatActions();
        return;
    case Screen::ChatModelPicker:
        renderChatModelPicker();
        return;
    case Screen::ChatToolPolicy:
        renderChatToolPolicy();
        return;
    case Screen::ChatCapabilityStatus:
        renderChatCapabilityStatus();
        return;
    case Screen::ComposerCapabilities:
        renderComposerCapabilities();
        return;
    case Screen::PendingToolPreview:
        renderPendingToolPreview();
        return;
    case Screen::PendingToolActions:
        renderPendingToolActions();
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
    case Screen::RequestInstructions:
        renderRequestInstructions();
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
                                             const cardputer::ChatDocument& chat,
                                             const std::string& requestInstructions)
{
    return cardputer::buildScopedInstructions(
        project.instructions, chat.instructions, requestInstructions,
        chat.contextSummary);
}

struct ContextSummaryPageResult {
    bool success;
    std::string summary;
    std::uint32_t includedMessages;
    String error;
};

ContextSummaryPageResult generateContextSummaryPage(
    const cardputer::ProjectDocument& project,
    const cardputer::ChatDocument& chat,
    const std::string& previousSummary,
    const std::vector<cardputer::Message>& source)
{
    if (source.empty()) {
        return {false, "", 0,
                "No omitted messages are available to summarize"};
    }
    constexpr std::size_t kCompactionHeapReserve = 16384;
    const std::size_t largestBlock =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (largestBlock <= kCompactionHeapReserve) {
        return {false, "", 0,
                "Context compaction needs a larger contiguous heap block; close SSH and retry"};
    }
    cardputer::ContextSummaryPromptResult prompt =
        cardputer::buildContextSummaryPrompt(
            previousSummary, source,
            std::min<std::size_t>(
                std::min<std::size_t>(project.contextByteBudget, 32768),
                largestBlock - kCompactionHeapReserve));
    if (!prompt.success) {
        return {false, "", 0, prompt.error.c_str()};
    }
    cardputer::Settings summarySettings = settings;
    summarySettings.globalInstructions = "";
    summarySettings.model = cardputer::resolveProjectRequestPolicy(
        settings, project, chat, 0).model;
    std::vector<cardputer::Message> summaryRequest;
    summaryRequest.push_back({"user", std::move(prompt.prompt)});
    cardputer::ChatResult summary = cardputer::streamChatCompletionWithBudget(
        summarySettings, summaryRequest,
        "This is a context compaction operation, not a user-facing answer.",
        768, [](const std::string&) {}, []() {
            M5Cardputer.update();
            return cardputerEscapePressed();
        });
    if (!summary.success) {
        return {false, "", 0, "Context summary failed: " + summary.error};
    }
    if (summary.response.empty() ||
        summary.response.size() > cardputer::kMaximumProjectChatSummaryBytes) {
        return {false, "", 0,
                "Context summary response is empty or exceeds its storage limit"};
    }
    return {true, std::move(summary.response), prompt.includedMessages, ""};
}

cardputer::OperationResult appendActiveContextSummary(
    const cardputer::ProjectDocument& project,
    const std::vector<cardputer::Message>& source)
{
    const cardputer::ChatDocumentResult current = cardputer::loadProjectChatMetadata(
        activeProjectId, activeChatId);
    if (!current.success) {
        return {false, current.error};
    }
    ContextSummaryPageResult summary = generateContextSummaryPage(
        project, current.chat, current.chat.contextSummary, source);
    if (!summary.success) {
        return {false, summary.error};
    }
    cardputer::ChatDocumentResult stored = cardputer::loadProjectChatMetadata(
        activeProjectId, activeChatId);
    if (!stored.success) {
        return {false, stored.error};
    }
    stored.chat.contextSummary = std::move(summary.summary);
    stored.chat.summarizedMessageCount = std::min(
        stored.chat.summary.messageCount,
        stored.chat.summarizedMessageCount + summary.includedMessages);
    return cardputer::saveProjectChatMetadata(stored.chat);
}

cardputer::OperationResult regenerateProjectChatContextSummary(
    const String& projectId,
    const String& chatId,
    const cardputer::ProjectDocument& project)
{
    cardputer::OperationResult result = cardputer::requireSdWriteAccess(
        0, cardputer::kStorageOperationalFloorBytes);
    if (!result.success) {
        return result;
    }
    std::uint32_t originalMessageCount = 0;
    cardputer::ChatDocument chatConfiguration;
    {
        const cardputer::ChatDocumentResult original = cardputer::loadProjectChatMetadata(
            projectId, chatId);
        if (!original.success) {
            return {false, original.error};
        }
        originalMessageCount = original.chat.summary.messageCount;
        chatConfiguration = original.chat;
    }
    constexpr std::uint32_t kRetainedMessages = 8;
    const std::uint32_t target = originalMessageCount > kRetainedMessages
        ? originalMessageCount - kRetainedMessages : 0;
    if (target == 0) {
        return {false, "This chat is too short to regenerate its context summary"};
    }
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        return {false, statusMessage.isEmpty()
            ? String("Network is not ready for context regeneration") : statusMessage};
    }
    cardputer::showBusyScreen("COMPACTING", "ESC cancels");
    std::string stagedSummary;
    std::uint32_t nextMessageIndex = 0;
    while (nextMessageIndex < target) {
        const cardputer::IndexedMessagesPageResult page =
            cardputer::readProjectChatMessagesByIndex(
                projectId, chatId, nextMessageIndex,
                std::min<std::uint32_t>(32, target - nextMessageIndex), 32768);
        if (!page.success || page.messages.empty()) {
            return {false, page.success
                ? String("Context regeneration reached an empty raw-history page")
                : page.error};
        }
        ContextSummaryPageResult generated = generateContextSummaryPage(
            project, chatConfiguration, stagedSummary, page.messages);
        if (!generated.success) {
            return {false, generated.error};
        }
        if (generated.includedMessages == 0 ||
            generated.includedMessages > page.messages.size() ||
            generated.includedMessages > target - nextMessageIndex) {
            return {false, "Context regeneration produced an invalid page boundary"};
        }
        nextMessageIndex += generated.includedMessages;
        stagedSummary = std::move(generated.summary);
    }
    cardputer::ChatDocumentResult stored = cardputer::loadProjectChatMetadata(
        projectId, chatId);
    if (!stored.success) {
        return {false, stored.error};
    }
    if (stored.chat.summary.messageCount != originalMessageCount) {
        return {false, "Chat history changed while its context summary was regenerating"};
    }
    stored.chat.contextSummary = std::move(stagedSummary);
    stored.chat.summarizedMessageCount = target;
    return cardputer::saveProjectChatMetadata(stored.chat);
}

void executeStoredPromptRequest(const std::string& prompt,
                                const cardputer::ProjectDocument& project,
                                const cardputer::ChatDocument& storedChat,
                                std::string requestInstructions,
                                cardputer::ResolvedProjectRequestPolicy requestPolicy,
                                const cardputer::ToolMessageIntent requestIntent,
                                const cardputer::ToolRequestPlan requestPlan)
{
    cardputer::Settings requestSettings = settings;
    requestSettings.model = requestPolicy.model;
    std::string effectiveInstructions = effectiveProjectChatInstructions(
        project, storedChat, requestInstructions);
    clearDevicePendingContext();
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
    const std::uint8_t filesGroup = static_cast<std::uint8_t>(
        1U << static_cast<std::uint8_t>(cardputer::ToolCapabilityGroup::Files));
    const std::uint8_t webGroup = static_cast<std::uint8_t>(
        1U << static_cast<std::uint8_t>(cardputer::ToolCapabilityGroup::Web));
    const std::uint8_t sshGroup = static_cast<std::uint8_t>(
        1U << static_cast<std::uint8_t>(cardputer::ToolCapabilityGroup::Ssh));
    const bool useTools = requestPlan.schemas != 0;
    Serial.printf("INFO event=chat_route workspace_tools=%s web_tools=%s ssh_tools=%s required_groups=%u\n",
                  (requestPlan.includedGroups & filesGroup) != 0 ? "enabled" : "disabled",
                  (requestPlan.includedGroups & webGroup) != 0 ? "yes" : "no",
                  (requestPlan.includedGroups & sshGroup) != 0 ? "yes" : "no",
                  static_cast<unsigned int>(requestPlan.intent.requiredGroups));
    cardputer::markOperation(useTools ? "chat_tools" : "chat_stream");
    const String requestProjectId = activeProjectId;
    const String requestChatId = activeChatId;
    const cardputer::CancelCallback isCancelled = []() {
        M5Cardputer.update();
        return cardputerEscapePressed();
    };
    const cardputer::ChatResult result = useTools
        ? cardputer::streamChatCompletionWithToolsAndBudget(
              requestSettings, history, effectiveInstructions, requestPlan,
              requestPolicy.maximumOutputTokens, onText,
              [&requestPlan, &isCancelled,
               requestProjectId](const cardputer::ToolCall& call) {
                  statusMessage = "Tool: " + String(call.name.c_str()) +
                      " · ESC cancels";
                  render();
                  return cardputer::routeProjectToolCall(
                      settings, requestPlan, requestProjectId, call, isCancelled);
              },
              [&requestPlan, requestProjectId, requestChatId](
                  const cardputer::PendingToolContinuation& continuation) {
                  return cardputer::savePendingToolCall(
                      requestPlan, requestProjectId, requestChatId,
                      continuation);
              }, isCancelled)
        : cardputer::streamChatCompletionWithBudget(
              requestSettings, history, effectiveInstructions,
              requestPolicy.maximumOutputTokens, onText, isCancelled);
    cardputer::markOperation("idle");
    if (result.outcome ==
        cardputer::ChatCompletionOutcome::AwaitingConfirmation) {
        activeResponse.clear();
        clearRetryRequestState();
        const cardputer::OperationResult captured = captureDevicePendingContext(
            requestProjectId, requestChatId, requestPolicy,
            requestSettings.globalInstructions, std::move(effectiveInstructions),
            requestIntent);
        if (!captured.success) {
            statusMessage = "Pending confirmation unavailable: " + captured.error;
            render();
            return;
        }
        statusMessage = "Waiting for confirmation: " + result.error;
        pendingToolReturnScreen = Screen::Chat;
        openPendingToolPreview();
        return;
    }
    if (!result.success) {
        activeResponse = result.response;
        captureRetryRequestState(
            prompt, activeChatId, requestPolicy.maximumOutputTokens,
            requestInstructions, requestIntent);
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
    clearRetryRequestState();
    cardputer::ContextWindowResult finalFit = cardputer::fitMessagesToByteBudget(
        history, requestPolicy.contextByteBudget);
    std::vector<cardputer::Message> compactedMessages;
    if (finalFit.droppedMessages > 0) {
        compactedMessages.assign(history.begin(),
                                 history.begin() + finalFit.droppedMessages);
    }
    history = std::move(finalFit.retained);
    activeResponse.clear();
    cardputer::OperationResult finalSave = cardputer::requireSdWriteAccess(
        0, cardputer::kStorageOperationalFloorBytes);
    if (finalSave.success) {
        finalSave = cardputer::appendProjectChatMessages(
            activeProjectId, activeChatId, {{"assistant", result.response}},
            currentChatTimestamp(), settings.projectChatHistoryQuotaBytes);
    }
    if (finalSave.success) {
        finalSave = saveCurrentChat();
    }
    bool automaticSummarySaved = false;
    if (finalSave.success && cardputer::shouldAutomaticallyCompactRequest(
            requestPolicy, static_cast<std::uint32_t>(compactedMessages.size()))) {
        finalSave = appendActiveContextSummary(
            project, compactedMessages);
        automaticSummarySaved = finalSave.success;
    }
    if (!finalSave.success) {
        statusMessage = "Response received but chat save failed: " + finalSave.error;
        Serial.println("ERROR event=chat_save result=failed stage=assistant");
        render();
        return;
    }
    if (automaticSummarySaved) {
        const cardputer::ChatDocumentResult summaryState =
            cardputer::loadProjectChatMetadata(activeProjectId, activeChatId);
        if (!summaryState.success) {
            statusMessage = "Response and context summary saved, but summary reload failed: " +
                summaryState.error;
            Serial.println(
                "ERROR event=context_summary source=automatic result=failed error=reload_failed");
            render();
            return;
        }
        Serial.printf(
            "INFO event=context_summary source=automatic result=ok summarized=%u total=%u\n",
            static_cast<unsigned int>(summaryState.chat.summarizedMessageCount),
            static_cast<unsigned int>(summaryState.chat.summary.messageCount));
    }
    const cardputer::OperationResult listResult = refreshChatList();
    if (listResult.success) {
        setTransientStatus(
            automaticSummarySaved ? String("Saved + context summarized")
                                  : String("Saved"),
            1500);
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

void submitPrompt()
{
    const std::uint32_t submitStartedAt = millis();
    if (inputBuffer.empty()) {
        statusMessage = "Type a message before pressing Enter";
        render();
        return;
    }
    cardputer::PendingToolCallResult pending = cardputer::loadPendingToolCall();
    if (!pending.success || pending.found) {
        if (!pending.success) {
            statusMessage = pending.error;
            render();
            return;
        }
        pendingToolReturnScreen = Screen::Chat;
        statusMessage = "Resolve the pending tool request before sending another message";
        std::string().swap(pending.pending.continuation.call.arguments);
        openPendingToolPreview();
        return;
    }
    ensureNetworkReady();
    const std::uint32_t networkReadyAt = millis();
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
    statusMessage = "Saving message...";
    render();
    const cardputer::OperationResult writeAccess = cardputer::requireSdWriteAccess(
        0, cardputer::kStorageOperationalFloorBytes);
    const std::uint32_t writeAccessReadyAt = millis();
    if (!writeAccess.success) {
        statusMessage = writeAccess.error;
        render();
        return;
    }

    const std::string prompt = inputBuffer;
    const cardputer::ProjectDocumentResult activeProject =
        cardputer::loadProject(activeProjectId);
    const cardputer::ChatDocumentResult storedChat = cardputer::loadProjectChatMetadata(
        activeProjectId, activeChatId);
    const std::uint32_t storageLoadedAt = millis();
    if (!activeProject.success || !storedChat.success) {
        statusMessage = activeProject.success ? storedChat.error : activeProject.error;
        render();
        return;
    }
    const cardputer::ToolMessageIntent requestIntent = composerToolIntent;
    const cardputer::ToolRequestPlan requestPlan =
        cardputer::resolveChatToolRequestPlan(
            settings, activeProject.project, storedChat.chat, requestIntent,
            fileWorkspaceReady,
            fileWorkspaceReady &&
                currentSdStorageStatus.state == cardputer::SdStorageState::Ready,
            currentSdStorageStatus.state == cardputer::SdStorageState::Ready,
            cachedSshToolProfileId);
    const String planError = toolRequestPlanError(requestPlan);
    if (!planError.isEmpty()) {
        statusMessage = planError;
        render();
        return;
    }
    const std::uint32_t requestOutputTokens = requestOutputOverrideChatId == activeChatId
        ? requestOutputOverrideTokens : 0;
    cardputer::ResolvedProjectRequestPolicy requestPolicy =
        cardputer::resolveProjectRequestPolicy(
            settings, activeProject.project, storedChat.chat, requestOutputTokens);
    std::vector<cardputer::Message> pendingHistory = history;
    pendingHistory.push_back({"user", prompt});
    cardputer::ContextWindowResult pendingFit = cardputer::fitMessagesToByteBudget(
        pendingHistory, requestPolicy.contextByteBudget);
    const std::uint32_t contextReadyAt = millis();
    const String pendingTitle = history.empty()
        ? String(cardputer::makeChatTitle(prompt, cardputer::kMaximumChatTitleCells).c_str())
        : activeChatTitle;
    const cardputer::OperationResult pendingSave = cardputer::appendProjectChatMessages(
        activeProjectId, activeChatId, {{"user", prompt}}, currentChatTimestamp(),
        settings.projectChatHistoryQuotaBytes);
    const std::uint32_t promptSavedAt = millis();
    if (!pendingSave.success) {
        statusMessage = pendingSave.error;
        render();
        return;
    }
    history = std::move(pendingFit.retained);
    activeChatTitle = pendingTitle;
    inputBuffer.clear();
    std::string requestInstructions = consumeRequestInstructionsOverride(activeChatId);
    const cardputer::ToolMessageIntent submittedIntent = consumeComposerToolIntent();
    captureRetryRequestState(
        prompt, activeChatId, requestPolicy.maximumOutputTokens,
        requestInstructions, submittedIntent);
    requestOutputOverrideChatId.clear();
    requestOutputOverrideTokens = 0;
    const cardputer::OperationResult pendingMetadataSave = saveCurrentChat();
    const std::uint32_t metadataSavedAt = millis();
    if (!pendingMetadataSave.success) {
        statusMessage = pendingMetadataSave.error;
        render();
        return;
    }
    Serial.printf(
        "INFO event=chat_submit_timing network_ms=%u access_ms=%u load_ms=%u context_ms=%u append_ms=%u metadata_ms=%u total_ms=%u\n",
        static_cast<unsigned int>(networkReadyAt - submitStartedAt),
        static_cast<unsigned int>(writeAccessReadyAt - networkReadyAt),
        static_cast<unsigned int>(storageLoadedAt - writeAccessReadyAt),
        static_cast<unsigned int>(contextReadyAt - storageLoadedAt),
        static_cast<unsigned int>(promptSavedAt - contextReadyAt),
        static_cast<unsigned int>(metadataSavedAt - promptSavedAt),
        static_cast<unsigned int>(metadataSavedAt - submitStartedAt));
    executeStoredPromptRequest(
        prompt, activeProject.project, storedChat.chat,
        std::move(requestInstructions), std::move(requestPolicy),
        submittedIntent, requestPlan);
}

std::vector<String> chatModelItems()
{
    std::vector<String> items = {"Use project default"};
    items.reserve(availableModels.size() + 1);
    items.insert(items.end(), availableModels.begin(), availableModels.end());
    return items;
}

void renderChatModelPicker()
{
    cardputer::showSelectionList(
        "CHAT MODEL", chatModelItems(), modelPickerIndex,
        menuStatus.isEmpty() ? String("UP/DOWN  ENTER  ESC chat") : menuStatus);
}

void renderChatToolPolicy()
{
    const cardputer::ChatDocumentResult chat =
        cardputer::loadProjectChatMetadata(activeProjectId, selectedChatId);
    const std::vector<String> items = chat.success
        ? capabilityPolicyItems(chat.chat.toolPolicy)
        : std::vector<String>{"Back"};
    cardputer::showSelectionList(
        "CHAT CAPABILITIES", items, capabilityPolicyIndex,
        !chat.success ? chat.error :
            (menuStatus.isEmpty() ? String("ENTER change  ESC chat") : menuStatus));
}

String composerCapabilitiesLabel()
{
    if (composerToolIntent.mode == cardputer::ToolMessageIntentMode::Auto) {
        return "Auto";
    }
    if (composerToolIntent.mode == cardputer::ToolMessageIntentMode::NoTools) {
        return "No tools";
    }
    String label;
    static constexpr const char* names[cardputer::kToolCapabilityGroupCount] = {
        "W", "F", "S", "P",
    };
    for (std::size_t index = 0; index < cardputer::kToolCapabilityGroupCount; ++index) {
        const std::uint8_t bit = static_cast<std::uint8_t>(1U << index);
        if ((composerToolIntent.requiredGroups & bit) != 0) {
            if (!label.isEmpty()) label += "/";
            label += names[index];
        }
    }
    return label.isEmpty() ? String("Auto") : label;
}

std::vector<String> composerCapabilitiesItems()
{
    std::vector<String> items = {
        composerToolIntent.mode == cardputer::ToolMessageIntentMode::Auto
            ? String("[ON] Auto") : String("Auto"),
        composerToolIntent.mode == cardputer::ToolMessageIntentMode::NoTools
            ? String("[ON] No tools") : String("No tools"),
    };
    static constexpr const char* names[cardputer::kToolCapabilityGroupCount] = {
        "Web", "Files", "SSH", "Python",
    };
    for (std::size_t index = 0; index < cardputer::kToolCapabilityGroupCount; ++index) {
        const std::uint8_t bit = static_cast<std::uint8_t>(1U << index);
        const bool enabled = composerToolIntent.mode ==
                cardputer::ToolMessageIntentMode::Required &&
            (composerToolIntent.requiredGroups & bit) != 0;
        items.push_back(String(enabled ? "[ON] " : "[OFF] ") + names[index]);
    }
    items.push_back("Back");
    return items;
}

void renderComposerCapabilities()
{
    cardputer::showSelectionList(
        "NEXT CAPABILITIES", composerCapabilitiesItems(),
        composerCapabilitiesIndex,
        menuStatus.isEmpty()
            ? String("ENTER select  Sends once  ESC") : menuStatus);
}

cardputer::ToolPolicyResolutionResult activeChatToolPermissionResolution()
{
    cardputer::ChatDocument chat;
    chat.toolPolicy = activeChatToolPolicy;
    chat.sshProfile = activeChatSshProfile;
    return cardputer::resolveChatToolPermissions(
        settings, activeProjectDocument, chat, composerToolIntent,
        fileWorkspaceReady,
        fileWorkspaceReady &&
            currentSdStorageStatus.state == cardputer::SdStorageState::Ready,
        currentSdStorageStatus.state == cardputer::SdStorageState::Ready,
        cachedSshToolProfileId);
}

cardputer::ChatCapabilityState chatCapabilityGroupState(
    std::size_t groupIndex,
    const cardputer::ScopedToolPermissionPolicy& chatPolicy,
    const cardputer::ToolMessageIntent& intent,
    const cardputer::ToolPolicyResolutionResult& resolution)
{
    const std::uint8_t groupBit = static_cast<std::uint8_t>(1U << groupIndex);
    if (intent.mode == cardputer::ToolMessageIntentMode::NoTools) {
        return cardputer::ChatCapabilityState::Off;
    }
    if (intent.mode == cardputer::ToolMessageIntentMode::Required &&
        (intent.requiredGroups & groupBit) != 0) {
        return cardputer::ChatCapabilityState::Required;
    }
    bool allInherit = true;
    bool anyAsk = false;
    for (std::size_t index = 0; index < cardputer::kToolCapabilityCount; ++index) {
        const cardputer::ToolCapabilityGroupMaskResult group =
            cardputer::toolCapabilityGroupMask(
                static_cast<cardputer::ToolCapability>(index));
        if (group.error != cardputer::ToolPolicyContractError::None ||
            group.mask != groupBit) {
            continue;
        }
        allInherit = allInherit &&
            chatPolicy[index] == cardputer::ScopedToolPermission::Inherit;
        const cardputer::ToolPermissionDecision decision =
            resolution.permissions[index].decision;
        if (decision == cardputer::ToolPermissionDecision::Deny ||
            decision == cardputer::ToolPermissionDecision::Unavailable) {
            return cardputer::ChatCapabilityState::Off;
        }
        anyAsk = anyAsk || decision == cardputer::ToolPermissionDecision::Ask;
    }
    if (allInherit) return cardputer::ChatCapabilityState::Inherit;
    return anyAsk ? cardputer::ChatCapabilityState::Ask
                  : cardputer::ChatCapabilityState::Allow;
}

cardputer::ChatCapabilityStates activeChatCapabilityStates()
{
    const cardputer::ToolPolicyResolutionResult resolution =
        activeChatToolPermissionResolution();
    cardputer::ChatCapabilityStates states = {
        cardputer::ChatCapabilityState::Off,
        cardputer::ChatCapabilityState::Off,
        cardputer::ChatCapabilityState::Off,
        cardputer::ChatCapabilityState::Off,
    };
    if (resolution.error != cardputer::ToolPolicyContractError::None) {
        return states;
    }
    for (std::size_t index = 0; index < states.size(); ++index) {
        states[index] = chatCapabilityGroupState(
            index, activeChatToolPolicy, composerToolIntent, resolution);
    }
    return states;
}

void renderChatCapabilityStatus()
{
    const cardputer::ProjectDocumentResult project =
        cardputer::loadProject(activeProjectId);
    const cardputer::ChatDocumentResult chat =
        cardputer::loadProjectChatMetadata(activeProjectId, selectedChatId);
    std::vector<std::string> lines;
    if (!project.success || !chat.success) {
        lines.push_back(std::string(
            (project.success ? chat.error : project.error).c_str()));
    } else {
        const cardputer::ToolMessageIntent intent = selectedChatId == activeChatId
            ? composerToolIntent : kAutomaticToolMessageIntent;
        const cardputer::ToolPolicyResolutionResult resolution =
            cardputer::resolveChatToolPermissions(
                settings, project.project, chat.chat, intent,
                fileWorkspaceReady,
                fileWorkspaceReady &&
                    currentSdStorageStatus.state == cardputer::SdStorageState::Ready,
                currentSdStorageStatus.state == cardputer::SdStorageState::Ready,
                cachedSshToolProfileId);
        if (resolution.error != cardputer::ToolPolicyContractError::None) {
            lines.push_back("Invalid capability policy");
        } else {
            lines.reserve(cardputer::kToolCapabilityCount * 2);
            for (std::size_t index = 0; index < cardputer::kToolCapabilityCount; ++index) {
                const cardputer::ResolvedToolPermission& effective =
                    resolution.permissions[index];
                const cardputer::ToolCapabilityGroupMaskResult group =
                    cardputer::toolCapabilityGroupMask(
                        static_cast<cardputer::ToolCapability>(index));
                const bool required =
                    intent.mode == cardputer::ToolMessageIntentMode::Required &&
                    group.error == cardputer::ToolPolicyContractError::None &&
                    (intent.requiredGroups & group.mask) != 0;
                lines.push_back(
                    std::string(capabilityLabel(index)) + ": " +
                    scopedToolPermissionLabel(chat.chat.toolPolicy[index]));
                std::string effectiveLine = "  > ";
                if (required) effectiveLine += "Required/";
                effectiveLine +=
                    std::string(toolPermissionDecisionLabel(effective.decision)) +
                    " (" + toolPermissionSourceLabel(effective.source) + ")";
                lines.push_back(std::move(effectiveLine));
            }
        }
    }
    const String position = lines.empty()
        ? String("0/0")
        : String(chatCapabilityStatusFirstLine + 1) + "/" + String(lines.size());
    chatCapabilityStatusLineCount = lines.size();
    cardputer::showReadOnlyTextViewer(
        "CAPABILITY STATUS", lines, chatCapabilityStatusFirstLine, position);
}

bool devicePendingContextMatches(const cardputer::PendingToolCall& pending)
{
    return devicePendingContext.present &&
        devicePendingContext.pendingId == pending.pendingId &&
        devicePendingContext.projectId == pending.projectId &&
        devicePendingContext.chatId == pending.chatId;
}

bool pendingIdentityMatchesCurrent(const cardputer::PendingToolCall& pending)
{
    const cardputer::ProjectDocumentResult project =
        cardputer::loadProject(pending.projectId);
    const cardputer::ChatDocumentResult chat =
        cardputer::loadProjectChatMetadata(pending.projectId, pending.chatId);
    return project.success && chat.success &&
        project.project.summary.revision == pending.projectRevision &&
        chat.chat.summary.revision == pending.chatRevision &&
        chat.chat.summary.messageCount == pending.chatMessageCount;
}

void clearDevicePendingContext()
{
    devicePendingContext.present = false;
    devicePendingContext.pendingId.clear();
    devicePendingContext.projectId.clear();
    devicePendingContext.chatId.clear();
    devicePendingContext.requestPolicy = {"", 0, 0, false};
    devicePendingContext.globalInstructions.clear();
    std::string().swap(devicePendingContext.scopedInstructions);
    devicePendingContext.intent = kAutomaticToolMessageIntent;
}

cardputer::OperationResult captureDevicePendingContext(
    const String& projectId,
    const String& chatId,
    const cardputer::ResolvedProjectRequestPolicy& requestPolicy,
    const String& globalInstructions,
    std::string scopedInstructions,
    const cardputer::ToolMessageIntent& intent)
{
    const cardputer::PendingToolCallResult pending = cardputer::loadPendingToolCall();
    if (!pending.success || !pending.found ||
        pending.pending.state != cardputer::PendingToolCallState::Awaiting ||
        pending.pending.projectId != projectId || pending.pending.chatId != chatId ||
        !cardputer::pendingToolCallIsResumableThisBoot(pending.pending.pendingId)) {
        return {
            false,
            pending.success && pending.found
                ? String("Pending confirmation does not match this request")
                : (pending.success ? String("Pending confirmation was not saved")
                                   : pending.error),
        };
    }
    clearDevicePendingContext();
    devicePendingContext.present = true;
    devicePendingContext.pendingId = pending.pending.pendingId;
    devicePendingContext.projectId = projectId;
    devicePendingContext.chatId = chatId;
    devicePendingContext.requestPolicy = requestPolicy;
    devicePendingContext.globalInstructions = globalInstructions;
    devicePendingContext.scopedInstructions = std::move(scopedInstructions);
    devicePendingContext.intent = intent;
    return {true, ""};
}

std::vector<std::string> splitPendingPreviewDisplayLines(
    const std::string& value,
    std::size_t maximumLineBytes)
{
    std::vector<std::string> lines;
    if (value.empty() || maximumLineBytes == 0) return lines;
    std::size_t offset = 0;
    while (offset < value.size()) {
        const std::size_t newline = value.find('\n', offset);
        const std::size_t logicalEnd = newline == std::string::npos
            ? value.size() : newline;
        if (offset == logicalEnd) lines.emplace_back();
        while (offset < logicalEnd) {
            std::size_t end = std::min(offset + maximumLineBytes, logicalEnd);
            while (end > offset && end < logicalEnd &&
                   (static_cast<unsigned char>(value[end]) & 0xC0U) == 0x80U) {
                --end;
            }
            if (end == offset) {
                end = offset + 1;
                while (end < logicalEnd &&
                       (static_cast<unsigned char>(value[end]) & 0xC0U) == 0x80U) {
                    ++end;
                }
            }
            lines.push_back(value.substr(offset, end - offset));
            offset = end;
        }
        if (newline == std::string::npos) break;
        offset = logicalEnd + 1;
        if (offset == value.size()) lines.emplace_back();
    }
    return lines;
}

void clearPendingToolPreviewCache()
{
    std::vector<std::string>().swap(pendingToolPreviewLines);
    pendingToolPreviewLineCount = 0;
    pendingToolPreviewActionable = false;
}

void cachePendingToolPreview(const cardputer::PendingToolCall& pending)
{
    clearPendingToolPreviewCache();
    const cardputer::PendingToolPreviewResult preview =
        cardputer::loadPendingToolPreview(pending.pendingId);
    pendingToolPreviewActionable = preview.success &&
        pending.state == cardputer::PendingToolCallState::Awaiting &&
        devicePendingContextMatches(pending) &&
        pendingIdentityMatchesCurrent(pending) &&
        cardputer::pendingToolCallIsResumableThisBoot(pending.pendingId);
    if (!preview.success) {
        pendingToolPreviewLines.push_back(std::string(preview.error.c_str()));
    } else {
        pendingToolPreviewLines.push_back(
            "Tool: " + std::string(preview.preview.toolName.c_str()));
        pendingToolPreviewLines.push_back(
            preview.preview.reason == cardputer::PendingToolConfirmationReason::Mandatory
                ? "Reason: mandatory confirmation"
                : "Reason: policy asks");
        if (!preview.preview.targetName.isEmpty()) {
            const std::vector<std::string> target = splitPendingPreviewDisplayLines(
                "Target: " + std::string(preview.preview.targetName.c_str()), 38);
            pendingToolPreviewLines.insert(
                pendingToolPreviewLines.end(), target.begin(), target.end());
        }
        const std::vector<std::string> body = splitPendingPreviewDisplayLines(
            preview.preview.body, 38);
        pendingToolPreviewLines.insert(
            pendingToolPreviewLines.end(), body.begin(), body.end());
        if (preview.preview.truncated) {
            pendingToolPreviewLines.push_back("[Preview truncated]");
        }
    }
    if (!pendingToolPreviewActionable) {
        pendingToolPreviewLines.push_back("Interrupted; execution is disabled");
    }
    pendingToolPreviewLineCount = pendingToolPreviewLines.size();
}

void renderPendingToolPreview()
{
    const String position = pendingToolPreviewLines.empty()
        ? String("0/0")
        : String(pendingToolPreviewFirstLine + 1) + "/" +
              String(pendingToolPreviewLines.size());
    cardputer::showReadOnlyTextViewer(
        "TOOL CONFIRMATION", pendingToolPreviewLines,
        pendingToolPreviewFirstLine, position);
}

std::vector<String> pendingToolActionItems()
{
    cardputer::PendingToolCallResult pending = cardputer::loadPendingToolCall();
    if (!pending.success || !pending.found) return {"Back"};
    std::string().swap(pending.pending.continuation.call.arguments);
    if (pendingToolPreviewActionable &&
        pending.pending.state == cardputer::PendingToolCallState::Awaiting &&
        devicePendingContextMatches(pending.pending) &&
        pendingIdentityMatchesCurrent(pending.pending) &&
        cardputer::pendingToolCallIsResumableThisBoot(pending.pending.pendingId)) {
        if (pending.pending.reason ==
            cardputer::PendingToolConfirmationReason::PolicyAsk) {
            return {"Allow once", "Allow for chat", "Deny", "Back"};
        }
        return {"Allow once", "Deny", "Back"};
    }
    if (pending.pending.state == cardputer::PendingToolCallState::ClaimedApprove) {
        return {"Acknowledge unknown effect", "Back"};
    }
    if (pending.pending.state == cardputer::PendingToolCallState::Denied) {
        return {"Acknowledge interrupted response", "Back"};
    }
    return {"Discard interrupted request", "Back"};
}

void renderPendingToolActions()
{
    cardputer::showSelectionList(
        "CONFIRM TOOL", pendingToolActionItems(), pendingToolActionsIndex,
        menuStatus.isEmpty() ? String("ENTER choose  ESC preview") : menuStatus);
}

void openPendingToolPreview()
{
    clearPendingToolPreviewCache();
    cardputer::PendingToolCallResult pending = cardputer::loadPendingToolCall();
    if (!pending.success || !pending.found) {
        menuStatus = pending.success ? String("No pending confirmation") : pending.error;
        currentScreen = pendingToolReturnScreen;
        if (currentScreen == Screen::Chat) render();
        else renderAiMenu();
        return;
    }
    pendingToolPreviewFirstLine = 0;
    pendingToolActionsIndex = 0;
    menuStatus = "";
    currentScreen = Screen::PendingToolPreview;
    std::string().swap(pending.pending.continuation.call.arguments);
    cachePendingToolPreview(pending.pending);
    renderPendingToolPreview();
}

struct PendingContinuationInputs {
    bool success = false;
    String pendingId;
    cardputer::PendingToolConfirmationReason reason =
        cardputer::PendingToolConfirmationReason::PolicyAsk;
    cardputer::ToolRequestPlan plan = {};
    String error;
};

PendingContinuationInputs loadPendingContinuationInputs()
{
    PendingContinuationInputs result;
    if (!pendingToolPreviewActionable) {
        result.error = "Pending preview is unavailable; execution is disabled";
        return result;
    }
    if (!devicePendingContext.present) {
        result.error = "This request was interrupted and cannot be resumed";
        return result;
    }
    cardputer::PendingToolCallResult pending = cardputer::loadPendingToolCall();
    if (!pending.success || !pending.found ||
        pending.pending.state != cardputer::PendingToolCallState::Awaiting ||
        !devicePendingContextMatches(pending.pending) ||
        !cardputer::pendingToolCallIsResumableThisBoot(pending.pending.pendingId)) {
        result.error = pending.success
            ? String("Pending request is no longer resumable") : pending.error;
        return result;
    }
    std::string().swap(pending.pending.continuation.call.arguments);
    if (!pendingIdentityMatchesCurrent(pending.pending)) {
        result.error = "Pending request is no longer resumable";
        return result;
    }
    result.pendingId = pending.pending.pendingId;
    result.reason = pending.pending.reason;
    const cardputer::ProjectDocumentResult project =
        cardputer::loadProject(pending.pending.projectId);
    const cardputer::ChatDocumentResult chat = cardputer::loadProjectChatMetadata(
        pending.pending.projectId, pending.pending.chatId);
    if (!project.success || !chat.success) {
        result.error = project.success ? chat.error : project.error;
        return result;
    }
    result.plan = cardputer::resolveChatToolRequestPlan(
        settings, project.project, chat.chat, devicePendingContext.intent,
        fileWorkspaceReady,
        fileWorkspaceReady &&
            currentSdStorageStatus.state == cardputer::SdStorageState::Ready,
        currentSdStorageStatus.state == cardputer::SdStorageState::Ready,
        cachedSshToolProfileId);
    result.error = toolRequestPlanError(result.plan);
    const cardputer::ToolCatalogEntry* entry = cardputer::toolCatalogEntryForName(
        pending.pending.continuation.call.name);
    if (result.error.isEmpty() &&
        (entry == nullptr ||
         !cardputer::toolRequestPlanIncludesSchema(result.plan, entry->schema))) {
        result.error = "Current policy no longer permits the pending tool";
    }
    result.success = true;
    return result;
}

void showPendingDecisionError(const String& error)
{
    menuStatus = error;
    currentScreen = Screen::PendingToolActions;
    renderPendingToolActions();
}

void continuePendingToolDecision(
    cardputer::PendingToolDecisionResult decision,
    const cardputer::ToolRequestPlan& continuationPlan,
    const String& warning)
{
    const String oldPendingId = decision.pending.pendingId;
    const cardputer::PendingToolCallState terminalState = decision.pending.state;
    clearPendingToolPreviewCache();
    cardputer::Settings requestSettings = settings;
    requestSettings.model = devicePendingContext.requestPolicy.model;
    requestSettings.globalInstructions = devicePendingContext.globalInstructions;
    const std::uint32_t contextBudget =
        devicePendingContext.requestPolicy.contextByteBudget;
    const bool ownerIsActive = decision.pending.projectId == activeProjectId &&
        decision.pending.chatId == activeChatId;
    std::vector<cardputer::Message> storedMessages;
    const std::vector<cardputer::Message>* continuationMessages = &history;
    String historyError;
    if (ownerIsActive) {
        if (history.empty() || history.back().role != "user") {
            historyError = "Pending continuation lost its final user message";
        }
    } else {
        cardputer::ChatDocumentResult stored = cardputer::loadProjectChat(
            decision.pending.projectId, decision.pending.chatId, 64,
            std::min<std::size_t>(contextBudget, 65536));
        if (!stored.success) {
            historyError = stored.error;
        } else if (stored.chat.summary.messageCount !=
                   decision.pending.chatMessageCount) {
            historyError = "Pending continuation chat history changed";
        } else {
            cardputer::ContextWindowResult fitted =
                cardputer::fitOwnedMessagesToByteBudget(
                    cardputer::takeUnsummarizedChatTail(std::move(stored.chat)),
                    contextBudget);
            if (fitted.retained.empty() || fitted.retained.back().role != "user") {
                historyError = "Pending continuation lost its final user message";
            } else {
                storedMessages = std::move(fitted.retained);
                continuationMessages = &storedMessages;
            }
        }
    }
    if (!historyError.isEmpty()) {
        std::string().swap(decision.pending.continuation.call.arguments);
        clearDevicePendingContext();
        const String error = warning.isEmpty()
            ? String("Tool decision recorded; response was not continued: ") + historyError
            : warning + "; response was not continued: " + historyError;
        if (ownerIsActive) {
            activeResponse.clear();
            statusMessage = error;
            currentScreen = Screen::Chat;
            render();
        } else {
            showPendingDecisionError(error);
        }
        return;
    }
    if (ownerIsActive) {
        activeResponse.clear();
        statusMessage = "Continuing response...";
        currentScreen = Screen::Chat;
        render();
    }
    std::uint32_t lastStreamRenderAt = 0;
    const cardputer::ChatTextCallback onText =
        [ownerIsActive, &lastStreamRenderAt](const std::string& text) {
            if (!ownerIsActive) return;
            activeResponse += text;
            const std::uint32_t now = millis();
            if (lastStreamRenderAt == 0 || now - lastStreamRenderAt >= 120) {
                render();
                lastStreamRenderAt = now;
            }
        };
    const cardputer::CancelCallback isCancelled = []() {
        M5Cardputer.update();
        return cardputerEscapePressed();
    };
    cardputer::markOperation("chat_tools");
    const cardputer::ChatResult result =
        cardputer::continueChatCompletionAfterPendingToolResult(
            requestSettings, *continuationMessages,
            devicePendingContext.scopedInstructions, continuationPlan,
            devicePendingContext.requestPolicy.maximumOutputTokens,
            std::move(decision.pending.continuation),
            std::move(decision.toolResult), onText,
            [&continuationPlan, &isCancelled,
             projectId = decision.pending.projectId](
                const cardputer::ToolCall& call) {
                return cardputer::routeProjectToolCall(
                    settings, continuationPlan, projectId, call, isCancelled);
            },
            [&continuationPlan, oldPendingId, terminalState,
             projectId = decision.pending.projectId,
             chatId = decision.pending.chatId](
                const cardputer::PendingToolContinuation& continuation) {
                return cardputer::replaceTerminalPendingToolCall(
                    oldPendingId, terminalState, continuationPlan,
                    projectId, chatId, continuation);
            },
            isCancelled);
    cardputer::markOperation("idle");
    std::string().swap(decision.pending.continuation.call.arguments);
    if (result.outcome == cardputer::ChatCompletionOutcome::AwaitingConfirmation) {
        cardputer::PendingToolCallResult next = cardputer::loadPendingToolCall();
        if (!next.success || !next.found ||
            next.pending.state != cardputer::PendingToolCallState::Awaiting ||
            next.pending.projectId != decision.pending.projectId ||
            next.pending.chatId != decision.pending.chatId ||
            !cardputer::pendingToolCallIsResumableThisBoot(next.pending.pendingId)) {
            clearDevicePendingContext();
            statusMessage = next.success
                ? String("Next confirmation could not be recovered") : next.error;
            if (ownerIsActive) render();
            else showPendingDecisionError(statusMessage);
            return;
        }
        devicePendingContext.pendingId = next.pending.pendingId;
        std::string().swap(next.pending.continuation.call.arguments);
        pendingToolReturnScreen = ownerIsActive ? Screen::Chat : Screen::AiMenu;
        statusMessage = warning.isEmpty()
            ? result.error : warning + "; " + result.error;
        openPendingToolPreview();
        return;
    }
    if (!result.success) {
        clearDevicePendingContext();
        if (ownerIsActive) {
            activeResponse = result.response;
            statusMessage = warning.isEmpty()
                ? result.error : warning + "; " + result.error;
            render();
        } else {
            showPendingDecisionError(
                warning.isEmpty() ? result.error : warning + "; " + result.error);
        }
        return;
    }
    cardputer::OperationResult saved = cardputer::appendProjectChatMessages(
        decision.pending.projectId, decision.pending.chatId,
        {{"assistant", result.response}}, currentChatTimestamp(),
        settings.projectChatHistoryQuotaBytes);
    if (!saved.success) {
        clearDevicePendingContext();
        if (ownerIsActive) {
            activeResponse = result.response;
            statusMessage = "Response received but chat save failed: " + saved.error;
            render();
        } else {
            showPendingDecisionError(
                "Response received but chat save failed: " + saved.error);
        }
        return;
    }
    const cardputer::OperationResult cleared = cardputer::clearPendingToolCall(
        oldPendingId, terminalState);
    clearDevicePendingContext();
    if (ownerIsActive) {
        history.push_back({"assistant", result.response});
        cardputer::ContextWindowResult fitted = cardputer::fitMessagesToByteBudget(
            history, contextBudget);
        history = std::move(fitted.retained);
        activeResponse.clear();
        statusMessage = !cleared.success
            ? "Response saved; pending cleanup failed: " + cleared.error
            : (warning.isEmpty() ? String("Response complete") : warning);
        const cardputer::OperationResult listed = refreshChatList();
        if (!listed.success) statusMessage += "; chat list: " + listed.error;
        currentScreen = Screen::Chat;
        render();
    } else {
        menuStatus = !cleared.success
            ? "Response saved; pending cleanup failed: " + cleared.error
            : (warning.isEmpty() ? String("Response complete") : warning);
        currentScreen = Screen::AiMenu;
        renderAiMenu();
    }
}

void allowPendingToolOnce()
{
    PendingContinuationInputs inputs = loadPendingContinuationInputs();
    if (!inputs.success) {
        showPendingDecisionError(inputs.error);
        return;
    }
    if (!inputs.error.isEmpty()) {
        showPendingDecisionError(inputs.error);
        return;
    }
    const cardputer::CancelCallback isCancelled = []() {
        M5Cardputer.update();
        return cardputerEscapePressed();
    };
    cardputer::PendingToolDecisionResult decision =
        cardputer::approvePendingProjectToolCall(
            settings, inputs.plan, inputs.pendingId, isCancelled);
    if (!decision.success) {
        showPendingDecisionError(decision.error);
        return;
    }
    const cardputer::ToolRequestPlan continuationPlan = inputs.plan;
    continuePendingToolDecision(
        std::move(decision), continuationPlan, "");
}

void allowPendingToolForChat()
{
    PendingContinuationInputs inputs = loadPendingContinuationInputs();
    if (!inputs.success) {
        showPendingDecisionError(inputs.error);
        return;
    }
    if (!inputs.error.isEmpty()) {
        showPendingDecisionError(inputs.error);
        return;
    }
    if (inputs.reason !=
        cardputer::PendingToolConfirmationReason::PolicyAsk) {
        showPendingDecisionError("Mandatory confirmation cannot be saved for chat");
        return;
    }
    const cardputer::CancelCallback isCancelled = []() {
        M5Cardputer.update();
        return cardputerEscapePressed();
    };
    cardputer::PendingToolDecisionResult decision =
        cardputer::approvePendingProjectToolCall(
            settings, inputs.plan, inputs.pendingId, isCancelled);
    if (!decision.success) {
        showPendingDecisionError(decision.error);
        return;
    }
    cardputer::ToolRequestPlan continuationPlan = inputs.plan;
    String warning;
    const cardputer::ToolCatalogEntry* entry = cardputer::toolCatalogEntryForName(
        decision.pending.continuation.call.name);
    cardputer::ChatDocumentResult chat = cardputer::loadProjectChatMetadata(
        decision.pending.projectId, decision.pending.chatId);
    if (entry == nullptr || !chat.success) {
        warning = entry == nullptr
            ? String("Tool ran, but its chat permission was not saved")
            : "Tool ran, but chat permission save failed: " + chat.error;
    } else {
        chat.chat.toolPolicy[static_cast<std::size_t>(entry->capability)] =
            cardputer::ScopedToolPermission::Allow;
        chat.chat.summary.updatedAt = currentChatTimestamp();
        const cardputer::OperationResult saved =
            cardputer::saveProjectChatMetadata(chat.chat);
        if (!saved.success) {
            warning = "Tool ran, but chat permission save failed: " + saved.error;
        } else {
            if (decision.pending.projectId == activeProjectId &&
                decision.pending.chatId == activeChatId) {
                activeChatToolPolicy = chat.chat.toolPolicy;
            }
            const cardputer::ProjectDocumentResult project =
                cardputer::loadProject(decision.pending.projectId);
            const cardputer::ChatDocumentResult updatedChat =
                cardputer::loadProjectChatMetadata(
                    decision.pending.projectId, decision.pending.chatId);
            if (!project.success || !updatedChat.success) {
                warning = "Tool ran; saved permission could not be reloaded";
            } else {
                const cardputer::ToolRequestPlan updatedPlan =
                    cardputer::resolveChatToolRequestPlan(
                        settings, project.project, updatedChat.chat,
                        devicePendingContext.intent, fileWorkspaceReady,
                        fileWorkspaceReady &&
                            currentSdStorageStatus.state ==
                                cardputer::SdStorageState::Ready,
                        currentSdStorageStatus.state ==
                            cardputer::SdStorageState::Ready,
                        cachedSshToolProfileId);
                if (toolRequestPlanError(updatedPlan).isEmpty()) {
                    continuationPlan = updatedPlan;
                } else {
                    warning = "Tool ran; continuing with the original bounded policy";
                }
            }
        }
    }
    continuePendingToolDecision(
        std::move(decision), continuationPlan, warning);
}

void denyPendingTool()
{
    PendingContinuationInputs inputs = loadPendingContinuationInputs();
    if (!inputs.success) {
        showPendingDecisionError(inputs.error);
        return;
    }
    cardputer::PendingToolDecisionResult decision =
        cardputer::denyPendingProjectToolCall(inputs.pendingId);
    if (!decision.success) {
        showPendingDecisionError(decision.error);
        return;
    }
    if (!inputs.error.isEmpty()) {
        std::string().swap(decision.pending.continuation.call.arguments);
        clearPendingToolPreviewCache();
        clearDevicePendingContext();
        showPendingDecisionError(
            "Tool denied; response was not continued: " + inputs.error);
        return;
    }
    const cardputer::ToolRequestPlan continuationPlan = inputs.plan;
    continuePendingToolDecision(
        std::move(decision), continuationPlan, "");
}

void acknowledgeInterruptedPendingTool()
{
    cardputer::PendingToolCallResult pending = cardputer::loadPendingToolCall();
    if (!pending.success || !pending.found) {
        showPendingDecisionError(
            pending.success ? String("No pending confirmation") : pending.error);
        return;
    }
    const String pendingId = pending.pending.pendingId;
    const cardputer::PendingToolCallState pendingState = pending.pending.state;
    std::string().swap(pending.pending.continuation.call.arguments);
    const cardputer::OperationResult cleared = cardputer::clearPendingToolCall(
        pendingId, pendingState);
    if (!cleared.success) {
        showPendingDecisionError(cleared.error);
        return;
    }
    clearPendingToolPreviewCache();
    const String acknowledged =
        pendingState == cardputer::PendingToolCallState::ClaimedApprove
            ? String("Interrupted effect acknowledged; it was not replayed")
            : (pendingState == cardputer::PendingToolCallState::Denied
                ? String("Interrupted denied response acknowledged")
                : String("Interrupted request discarded without execution"));
    clearDevicePendingContext();
    if (pendingToolReturnScreen == Screen::Chat) {
        currentScreen = Screen::Chat;
        statusMessage = acknowledged;
        render();
    } else {
        currentScreen = Screen::AiMenu;
        menuStatus = acknowledged;
        renderAiMenu();
    }
}

void renderProjectToolPolicy()
{
    const cardputer::ProjectDocumentResult project =
        cardputer::loadProject(selectedProjectId);
    const std::vector<String> items = project.success
        ? capabilityPolicyItems(project.project.toolPolicy)
        : std::vector<String>{"Back"};
    cardputer::showSelectionList(
        "PROJECT CAPABILITIES", items, capabilityPolicyIndex,
        !project.success ? project.error :
            (menuStatus.isEmpty() ? String("ENTER change  ESC project") : menuStatus));
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
        activeProjectId, "E2E search " + String(millis()),
        settings.newChatToolPolicy);
    if (!created.success) {
        Serial.println("E2ETEST result=failed stage=create_chat");
        return;
    }

    activeChatId = created.chat.summary.id;
    activeChatTitle = created.chat.summary.title;
    activeChatToolPolicy = created.chat.toolPolicy;
    activeChatSshProfile = created.chat.sshProfile;
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
    activeChatToolPolicy = restored.chat.toolPolicy;
    activeChatSshProfile = restored.chat.sshProfile;
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
    voiceStorageInitialized = voiceStorageResult.success;
    voiceStorageInitializationError = voiceStorageResult.success
        ? String() : voiceStorageResult.error;
    voiceStorageReady = voiceStorageInitialized;
    voiceStorageError = voiceStorageInitializationError;
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

    const cardputer::OperationResult diagnosticRecoveryResult = legacyChatResult.success
        ? cardputer::recoverInterruptedProjectMigrationDiagnostic()
        : cardputer::OperationResult{false, legacyChatResult.error};
    const cardputer::OperationResult workspaceResult = diagnosticRecoveryResult.success
        ? cardputer::initializeFileWorkspace()
        : cardputer::OperationResult{false, diagnosticRecoveryResult.error};
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
    chatStorageInitialized = chatResult.success;
    chatStorageInitializationError = chatResult.success ? String() : chatResult.error;
    Serial.printf("CHAT_STORAGE result=%s count=%u\n",
                  chatStorageReady ? "ready" : "failed",
                  static_cast<unsigned int>(chats.size()));
    fileWorkspaceReady = workspaceResult.success && migrationResult.success &&
        chatStorageReady;
    fileWorkspaceError = fileWorkspaceReady ? String() : migrationResult.error;
    fileWorkspaceInitialized = fileWorkspaceReady;
    fileWorkspaceInitializationError = fileWorkspaceError;
    refreshRuntimeSdState();
    Serial.printf("FILE_WORKSPACE result=%s\n", fileWorkspaceReady ? "ready" : "failed");
    Serial.printf("PROJECT_STORAGE result=%s migrated=%s chats=%u\n",
                  migrationResult.success ? "ready" : "failed",
                  migrationResult.migrated ? "yes" : "no",
                  static_cast<unsigned int>(migrationResult.migratedChats));

    cardputer::OperationResult journalResult = voiceStorageReady
        ? cardputer::requireSdWriteAccess(0, cardputer::kStorageOperationalFloorBytes)
        : cardputer::OperationResult{false, voiceStorageError};
    if (journalResult.success) {
        journalResult = cardputer::appendBootJournal(kFirmwareVersion);
    }
    crashJournalReady = journalResult.success;
    crashJournalError = journalResult.success ? String() : journalResult.error;
    crashJournalInitialized = journalResult.success;
    crashJournalInitializationError = crashJournalError;
    Serial.printf("CRASH_JOURNAL result=%s\n", crashJournalReady ? "ready" : "failed");

    const cardputer::OperationResult toolActivityResult = fileWorkspaceReady
        ? cardputer::initializeToolActivityJournal()
        : cardputer::OperationResult{false, fileWorkspaceError};
    Serial.printf("TOOL_ACTIVITY result=%s\n",
                  toolActivityResult.success ? "ready" : "failed");

    const cardputer::OperationResult sshStorageResult = fileWorkspaceReady
        ? cardputer::initializeSshStorage()
        : cardputer::OperationResult{false, fileWorkspaceError};
    sshStorageReady = sshStorageResult.success;
    cachedSshToolProfileId = sshStorageReady
        ? cardputer::sshToolAvailableProfileId() : 0;
    sshStorageError = sshStorageResult.success ? String() : sshStorageResult.error;
    sshStorageInitialized = sshStorageResult.success;
    sshStorageInitializationError = sshStorageError;
    refreshRuntimeSdState();
    Serial.printf("SSH_STORAGE result=%s\n", sshStorageReady ? "ready" : "failed");

    carouselIndex = 0;
    menuStatus = !pythonHandoff.success
        ? pythonHandoff.error
        : !fileWorkspaceReady
        ? fileWorkspaceError
        : (!crashJournalReady ? crashJournalError
                              : (!toolActivityResult.success
                                     ? toolActivityResult.error
                                     : (sshStorageReady ? String() : sshStorageError)));
    statusMessage = "";
    cardputer::PendingToolCallResult startupPending =
        cardputer::loadPendingToolCall();
    if (menuStatus.isEmpty() && startupPending.success && startupPending.found) {
        menuStatus = startupPending.pending.state ==
                cardputer::PendingToolCallState::ClaimedApprove
            ? String("Interrupted tool effect needs acknowledgement")
            : String("Interrupted tool request was not resumed");
    }
    std::string().swap(startupPending.pending.continuation.call.arguments);
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
    if (millis() - lastSdStateRefreshAt >= kSdStateRefreshIntervalMs) {
        const bool changed = refreshRuntimeSdState();
        if (changed) {
            Serial.printf("SD_STATE state=%s error=%s\n",
                          cardputer::sdStorageStateName(currentSdStorageStatus.state),
                          cardputer::sdStorageErrorCode(currentSdStorageStatus.state));
            if (currentSdStorageStatus.state != cardputer::SdStorageState::Ready) {
                menuStatus = currentSdStorageStatus.error;
                statusMessage = currentSdStorageStatus.error;
            }
            if (!displaySleeping &&
                (currentScreen == Screen::MainCarousel ||
                 currentScreen == Screen::Diagnostics ||
                 currentScreen == Screen::Chat)) {
                render();
            }
        }
    }
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
    if (currentScreen == Screen::Chat && M5Cardputer.BtnA.wasPressed()) {
        handleVoiceInput();
    } else {
        handleKeyboard();
    }
    const std::uint32_t now = millis();
    const bool draftIdle = now - lastDraftEditAt >= kDraftAutosaveIdleMs;
    const bool draftSaveRetryReady =
        now - lastDraftAutosaveAt >= kDraftAutosaveIdleMs;
    const bool draftMaximumAgeReached = draftDirtySinceAt != 0 &&
        now - draftDirtySinceAt >= kDraftAutosaveMaximumDirtyMs;
    if (currentScreen == Screen::Chat && inputBuffer != persistedDraft &&
        draftSaveRetryReady && (draftIdle || draftMaximumAgeReached)) {
        lastDraftAutosaveAt = now;
        const cardputer::OperationResult saved = saveCurrentChat();
        if (!saved.success) {
            statusMessage = "Draft autosave failed: " + saved.error;
            render();
        }
    }
    delay(5);
}
