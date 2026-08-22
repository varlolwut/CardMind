#include <M5Cardputer.h>
#include <SD.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "src/api_client.h"
#include "src/app_types.h"
#include "src/audio_utils.h"
#include "src/chat_storage.h"
#include "src/file_workspace.h"
#include "src/file_portal.h"
#include "src/provisioning.h"
#include "src/stt_client.h"
#include "src/storage.h"
#include "src/text_utils.h"
#include "src/tts_client.h"
#include "src/ui.h"
#include "src/voice_input.h"
#include "src/web_search_client.h"
#include "src/wifi_networks.h"

#include <algorithm>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

SET_LOOP_TASK_STACK_SIZE(16384);

namespace {

constexpr const char* kFirmwareVersion = "1.6.1";
constexpr std::size_t kMaximumInputBytes = 1200;
constexpr std::size_t kMaximumWifiPasswordBytes = 63;
constexpr std::uint8_t kTtsVolumeStep = 64;

enum class Screen {
    Chat,
    SettingsMenu,
    ControlsHelp,
    ModelPicker,
    ChatList,
    DeleteChatConfirm,
    WifiPicker,
    WifiPassword,
};

cardputer::Settings settings;
std::vector<cardputer::Message> history;
std::vector<cardputer::ChatSummary> chats;
std::vector<String> availableModels;
std::string inputBuffer;
std::string activeResponse;
cardputer::KeyboardLayout keyboardLayout = cardputer::KeyboardLayout::English;
String statusMessage;
String transientStatusValue;
std::uint32_t transientStatusUntil = 0;
std::size_t scrollOffset = 0;
String serialInput;
std::vector<Point2D_t> pressedKeys;
Screen currentScreen = Screen::Chat;
String activeChatId;
String activeChatTitle = "New chat";
bool chatStorageReady = false;
String chatStorageError;
bool fileWorkspaceReady = false;
String fileWorkspaceError;
std::size_t chatListIndex = 0;
String deleteChatId;
String deleteChatTitle;
std::size_t settingsMenuIndex = 0;
std::size_t controlsHelpIndex = 0;
std::size_t modelPickerIndex = 0;
std::size_t wifiPickerIndex = 0;
std::vector<cardputer::WifiNetwork> scannedWifiNetworks;
std::string wifiPasswordInput;
String menuStatus;
bool voiceStorageReady = false;
String voiceStorageError;
bool sttCredentialsValidated = false;

void ensureNetworkReady();
void render();
void submitPrompt();
void runUiSearchEndToEndTest();

std::size_t historyBytes(const std::vector<cardputer::Message>& messages)
{
    std::size_t total = 0;
    for (const auto& message : messages) {
        total += message.content.size();
    }
    return total;
}

std::vector<cardputer::Message> trimmedHistory(const std::vector<cardputer::Message>& messages)
{
    std::vector<cardputer::Message> result = messages;
    while (result.size() > cardputer::kMaximumStoredMessages ||
           historyBytes(result) > cardputer::kMaximumStoredHistoryBytes) {
        if (result.size() < 2) {
            result.clear();
            break;
        }
        result.erase(result.begin(), result.begin() + 2);
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
        {activeChatId, activeChatTitle, updatedAt, static_cast<std::uint32_t>(history.size())},
        history,
    };
    return cardputer::saveChat(document);
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
    activeResponse.clear();
    inputBuffer.clear();
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
    history.clear();
    activeResponse.clear();
    inputBuffer.clear();
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
        const String marker = chat.id == activeChatId ? "[ON] " : "";
        items.push_back(marker + chat.title + "  [" + chat.messageCount + "]");
    }
    return items;
}

void renderChatList()
{
    cardputer::showSelectionList("CHATS", chatListItems(), chatListIndex,
                                 menuStatus.isEmpty()
                                     ? String("ENTER open/new  FN+DEL delete")
                                     : menuStatus);
}

void openChatList()
{
    const cardputer::OperationResult result = refreshChatList();
    if (!result.success) {
        statusMessage = result.error;
        render();
        return;
    }
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
    cardputer::showChat(history, activeResponse, inputBuffer, keyboardLayout,
                        activeChatTitle, statusMessage, scrollOffset, WiFi.status() == WL_CONNECTED);
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
    return utf8Backspace && russianLayout && sse && wav && chatText &&
           cardputer::fontSupportsCyrillic();
}

void printStatus()
{
    Serial.printf("STATUS board_adv=%s configured=%s voice_configured=%s search_configured=%s tts_configured=%s tts_auto=%s microsd=%s chats=%s chat_count=%u files=%s wifi=%s tls_time=%s history=%u heap=%u largest_heap=%u min_heap=%u stack_free=%u reset_reason=%d\n",
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
                  WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
                  std::time(nullptr) >= 1700000000 ? "valid" : "invalid",
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
        settings, testHistory, [](const std::string&) {},
        [&searchCalled](const cardputer::ToolCall& call) {
            if (cardputer::isWebSearchToolName(call.name)) {
                searchCalled = true;
                return cardputer::executeWebSearchTool(settings, call);
            }
            return executeAvailableTool(call);
        });
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
        settings, testHistory, [](const std::string&) {});
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
    const cardputer::OperationResult saved = cardputer::saveChat(document);
    const cardputer::ChatDocumentResult loaded = saved.success
        ? cardputer::loadChat(document.summary.id)
        : cardputer::ChatDocumentResult{false, {}, saved.error};
    const bool chatVerified = loaded.success && loaded.chat.messages.size() == 2 &&
        loaded.chat.messages[1].content == "OK";
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
        settings, testHistory, [](const std::string&) {},
        [&writeSucceeded](const cardputer::ToolCall& call) {
            const cardputer::ToolExecutionResult execution = cardputer::executeWorkspaceTool(call);
            if (call.name == "write_file" && execution.success) {
                writeSucceeded = true;
            }
            return execution;
        });
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
    if (command == "STORAGETEST") {
        runStorageTest();
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
        } else if (character != '\r' && serialInput.length() < 32) {
            serialInput += character;
        }
    }
}

void refreshModels()
{
    statusMessage = "Loading models...";
    render();
    const cardputer::ModelsResult result = cardputer::fetchModels(settings);
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
        render();
        const cardputer::OperationResult wifiResult = cardputer::connectToWifi(settings);
        if (!wifiResult.success) {
            statusMessage = wifiResult.error;
            Serial.println("ERROR event=wifi_connect result=failed");
            return;
        }
    }
    if (std::time(nullptr) < 1700000000) {
        statusMessage = "Synchronizing TLS clock...";
        render();
        const cardputer::OperationResult clockResult = cardputer::synchronizeTlsClock();
        if (!clockResult.success) {
            statusMessage = clockResult.error;
        }
    }
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
    pendingHistory = trimmedHistory(pendingHistory);
    const String pendingTitle = history.empty()
        ? String(cardputer::makeChatTitle(prompt, cardputer::kMaximumChatTitleCells).c_str())
        : activeChatTitle;
    cardputer::ChatDocument pendingDocument = {
        {activeChatId, pendingTitle, currentChatTimestamp(),
         static_cast<std::uint32_t>(pendingHistory.size())},
        std::move(pendingHistory),
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
    const cardputer::ChatResult result = useTools
        ? cardputer::streamChatCompletionWithTools(
              settings, history, onText, [](const cardputer::ToolCall& call) {
                  statusMessage = "Tool: " + String(call.name.c_str());
                  render();
                  return executeAvailableTool(call);
              })
        : cardputer::streamChatCompletion(settings, history, onText);
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
    history = trimmedHistory(history);
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
        const cardputer::OperationResult speech = cardputer::synthesizeAndPlaySpeech(
            settings, result.response);
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
    const cardputer::VoiceRecordingResult recording = cardputer::recordVoiceWhileButtonHeld(
        [](std::uint32_t elapsedMs, std::uint16_t level) {
            cardputer::showVoiceRecording(
                elapsedMs, cardputer::maximumVoiceRecordingMs(), level);
        });
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
    const cardputer::TranscriptionResult transcription =
        cardputer::transcribeVoiceRecording(settings);
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
    const cardputer::OperationResult result = cardputer::synthesizeAndPlaySpeech(settings, message->content);
    if (result.success) {
        setTransientStatus("Spoken", 1500);
        Serial.println("INFO event=tts_playback result=ok source=manual");
    } else {
        statusMessage = result.error;
        Serial.println("ERROR event=tts_playback result=failed source=manual");
    }
    render();
}

std::vector<String> settingsMenuItems()
{
    const unsigned int volumePercent =
        (static_cast<unsigned int>(settings.ttsVolume) * 100U + 127U) / 255U;
    return {
        "Controls help",
        "Wi-Fi network",
        "Files download portal",
        "Web setup (API/key)",
        settings.ttsAutoPlay ? "Auto TTS: ON" : "Auto TTS: OFF",
        "TTS volume: " + String(volumePercent) + "%",
        "Back to chat",
    };
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
        "FN+5  Older messages / up",
        "FN+6  Newer messages / down",
        "FN+7  New chat",
        "FN+8  Speak last answer",
        "Menu: ENTER  Select",
        "Menu: FN+`  Back",
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

void renderSettingsMenu()
{
    cardputer::showSelectionList("MAIN MENU", settingsMenuItems(), settingsMenuIndex,
                                 menuStatus.isEmpty() ? "UP/DN  ENTER  FN+` back" : menuStatus);
}

void renderControlsHelp()
{
    cardputer::showSelectionList("CONTROLS HELP", controlsHelpItems(), controlsHelpIndex,
                                 "FN+5/6 scroll   FN+` back");
}

void renderModelPicker()
{
    cardputer::showSelectionList("SELECT MODEL", availableModels, modelPickerIndex,
                                 menuStatus.isEmpty() ? "UP/DN  ENTER  FN+` cancel" : menuStatus);
}

void renderWifiPicker()
{
    cardputer::showSelectionList("SELECT WI-FI", wifiPickerItems(scannedWifiNetworks),
                                 wifiPickerIndex,
                                 menuStatus.isEmpty() ? "UP/DN  ENTER  FN+` back" : menuStatus);
}

void openModelPicker()
{
    if (availableModels.empty()) {
        refreshModels();
        if (availableModels.empty()) {
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

void openSettingsMenu()
{
    settingsMenuIndex = 0;
    menuStatus = "";
    currentScreen = Screen::SettingsMenu;
    renderSettingsMenu();
}

void openWifiPicker()
{
    cardputer::showBusyScreen("WI-FI", "Scanning 2.4 GHz...");
    const cardputer::WifiScanResult scanResult = cardputer::scanWifiNetworks();
    if (!scanResult.success) {
        menuStatus = scanResult.error;
        currentScreen = Screen::SettingsMenu;
        renderSettingsMenu();
        Serial.println("WARN event=wifi_scan result=failed source=device_ui");
        return;
    }
    if (scanResult.networks.empty()) {
        menuStatus = "No 2.4 GHz networks found";
        currentScreen = Screen::SettingsMenu;
        renderSettingsMenu();
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
        currentScreen = Screen::Chat;
        render();
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
    currentScreen = Screen::Chat;
    Serial.println("INFO event=model_update result=ok source=device_ui");
    render();
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
    const cardputer::OperationResult connectResult = cardputer::connectToWifi(candidate);
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
    currentScreen = Screen::Chat;
    setTransientStatus("Wi-Fi connected", 2500);
    Serial.println("INFO event=wifi_update result=ok source=device_ui");
    refreshModels();
    render();
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
    const bool cancelPressed = keys.fn && keys.esc;
    const bool upPressed = keys.fn && (keys.f5 || keys.up);
    const bool downPressed = keys.fn && (keys.f6 || keys.down);
    const bool enterPressed = newPressContains(newPresses, KEY_ENTER);
    const bool deletePressed = keys.fn && keys.del;
    const bool clearDraftPressed = keys.ctrl && newPressContains(newPresses, KEY_BACKSPACE);
    const bool backspacePressed = (keys.fn && keys.del) ||
        newPressContains(newPresses, KEY_BACKSPACE);

    if (currentScreen == Screen::ChatList) {
        const std::size_t itemCount = chats.size() + 1;
        if (cancelPressed) {
            currentScreen = Screen::Chat;
            menuStatus = "";
            render();
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
                currentScreen = Screen::DeleteChatConfirm;
                cardputer::showConfirmation("DELETE CHAT", deleteChatTitle,
                                            "ENTER delete  FN+` cancel");
            }
        } else if (enterPressed) {
            const cardputer::OperationResult result = chatListIndex == 0
                ? createAndActivateChat()
                : activateChat(chats[chatListIndex - 1].id);
            if (!result.success) {
                menuStatus = result.error;
                renderChatList();
            } else {
                currentScreen = Screen::Chat;
                menuStatus = "";
                setTransientStatus(chatListIndex == 0 ? String("New chat created")
                                                      : String("Chat opened"), 2000);
                render();
            }
        }
        return;
    }

    if (currentScreen == Screen::DeleteChatConfirm) {
        if (cancelPressed) {
            currentScreen = Screen::ChatList;
            deleteChatId = "";
            deleteChatTitle = "";
            renderChatList();
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

    if (currentScreen == Screen::SettingsMenu) {
        const std::size_t itemCount = settingsMenuItems().size();
        if (cancelPressed) {
            currentScreen = Screen::Chat;
            statusMessage = "Ready";
            render();
        } else if (upPressed) {
            settingsMenuIndex = settingsMenuIndex > 0 ? settingsMenuIndex - 1 : 0;
            renderSettingsMenu();
        } else if (downPressed) {
            settingsMenuIndex = std::min(settingsMenuIndex + 1, itemCount - 1);
            renderSettingsMenu();
        } else if (enterPressed) {
            if (settingsMenuIndex == 0) {
                controlsHelpIndex = 0;
                currentScreen = Screen::ControlsHelp;
                renderControlsHelp();
            } else if (settingsMenuIndex == 1) {
                openWifiPicker();
            } else if (settingsMenuIndex == 2) {
                cardputer::runFilePortal();
            } else if (settingsMenuIndex == 3) {
                cardputer::runProvisioningPortal(settings);
            } else if (settingsMenuIndex == 4) {
                if (!settings.ttsAutoPlay && !cardputer::ttsSettingsAreComplete(settings)) {
                    menuStatus = "Configure TTS in Web setup first";
                    renderSettingsMenu();
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
                renderSettingsMenu();
            } else if (settingsMenuIndex == 5) {
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
                renderSettingsMenu();
            } else {
                currentScreen = Screen::Chat;
                statusMessage = "Ready";
                render();
            }
        }
        return;
    }

    if (currentScreen == Screen::ControlsHelp) {
        const std::size_t itemCount = controlsHelpItems().size();
        if (cancelPressed) {
            currentScreen = Screen::SettingsMenu;
            menuStatus = "";
            renderSettingsMenu();
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
            currentScreen = Screen::Chat;
            statusMessage = "Model selection cancelled";
            render();
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
            currentScreen = Screen::SettingsMenu;
            menuStatus = "";
            renderSettingsMenu();
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
        openChatList();
        return;
    } else if (keys.fn && keys.f2) {
        menuStatus = "";
        openModelPicker();
        return;
    } else if (keys.fn && keys.f3) {
        keyboardLayout = keyboardLayout == cardputer::KeyboardLayout::English
            ? cardputer::KeyboardLayout::Russian
            : cardputer::KeyboardLayout::English;
        setTransientStatus(keyboardLayout == cardputer::KeyboardLayout::English
                               ? String("English layout") : String("Russian layout"),
                           1800);
    } else if (keys.fn && keys.f4) {
        openSettingsMenu();
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
    const bool isAdv = M5.getBoard() == m5::board_t::board_M5CardputerADV;
    Serial.printf("BOARD adv=%s type=%d\n", isAdv ? "yes" : "no", static_cast<int>(M5.getBoard()));
    if (!isAdv) {
        cardputer::showFatalError("Expected M5Stack Cardputer ADV; board detection returned another type");
        Serial.println("FATAL event=board_detection expected=cardputer_adv");
        while (true) {
            delay(1000);
        }
    }
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
        cardputer::runProvisioningPortal(settings);
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

    statusMessage = fileWorkspaceReady ? String("Starting...") : fileWorkspaceError;
    render();
    ensureNetworkReady();
    if (WiFi.status() == WL_CONNECTED && std::time(nullptr) >= 1700000000) {
        Serial.println("NETWORK wifi=connected tls_time=valid");
        refreshModels();
    } else {
        Serial.println("ERROR event=network_start result=failed");
    }
    render();
    Serial.println("READY");
}

void loop()
{
    M5Cardputer.update();
    updateTransientStatus();
    updateSerial();
    if (currentScreen == Screen::Chat && M5Cardputer.BtnA.wasPressed()) {
        handleVoiceInput();
    } else {
        handleKeyboard();
    }
    delay(5);
}
