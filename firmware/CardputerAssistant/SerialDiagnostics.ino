namespace {

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
    const bool workspaceRouting = cardputer::requestsWorkspaceAccess(
        "Можешь набросать простой тестовый скрипт на Python и сохранить его?") &&
        cardputer::requestsWorkspaceWrite(
            "Можешь набросать простой тестовый скрипт на Python и сохранить его?") &&
        !cardputer::requestsWorkspaceAccess("Объясни простой скрипт на Python") &&
        !cardputer::requestsWorkspaceWrite("Покажи файлы на SD");
    return utf8Backspace && russianLayout && sse && wav && chatText && utf8Cursor &&
           documentReader && calculator && workspaceRouting &&
           cardputer::fontSupportsCyrillic();
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
    String observedToolName;
    const std::vector<cardputer::Message> testHistory = {
        {"user", "/search Call web_search exactly once with JSON query \"Cardputer Zero\", then summarize its result."},
    };
    const cardputer::ChatToolPolicy toolPolicy = cardputer::resolveChatToolPolicy(
        settings, testHistory.front().content, false, false);
    const cardputer::ChatResult result = cardputer::streamChatCompletionWithTools(
        settings, testHistory, "", false, [](const std::string&) {},
        [&searchCalled, &observedToolName, &toolPolicy](const cardputer::ToolCall& call) {
            if (observedToolName.isEmpty()) {
                observedToolName = String(call.name.c_str());
            }
            if (cardputer::isWebSearchToolName(call.name)) {
                searchCalled = true;
                return cardputer::executeWebSearchTool(
                    settings, call, []() { return false; });
            }
            return cardputer::routeToolCall(
                settings, toolPolicy, call, []() { return false; });
        }, []() { return false; });
    const String safeError = serialSafeError(result.error, 180);
    Serial.printf("SEARCHTEST result=%s search_called=%s tool=%s response_bytes=%u error=%s\n",
                  result.success && searchCalled ? "pass" : "failed",
                  searchCalled ? "yes" : "no",
                  observedToolName.isEmpty() ? "none" : observedToolName.c_str(),
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
    const cardputer::ArchivedMessagesPageResult archivedPage = result.success
        ? cardputer::readArchivedChatMessages(source.summary.id, 0, 8, 12000)
        : cardputer::ArchivedMessagesPageResult{false, {}, 0, true, result.error};
    if (result.success && (!archivedPage.success || archivedPage.messages.size() != 2 ||
                           archivedPage.messages[0].content != "old" ||
                           !archivedPage.eof)) {
        result = {false, "Archived chat viewer page verification failed"};
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
    if (result.success) {
        result = cardputer::clearChatHistory(source.summary.id);
    }
    const cardputer::ChatDocumentResult cleared = result.success
        ? cardputer::loadChat(source.summary.id)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    if (result.success && (!cleared.success || !cleared.chat.messages.empty() ||
                           cleared.chat.summary.archivedMessageCount != 0 ||
                           cleared.chat.instructions != "Be concise.")) {
        result = {false, "Clear chat verification failed"};
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
    constexpr std::size_t kWorkspaceFileTestBytes = 2U * 1024U * 1024U;
    constexpr std::uint32_t kWorkspaceFileEditOffset = 1024U * 1024U;
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
            result = {false, "Failed to open streaming workspace test file"};
        } else {
            std::uint8_t block[4096] = {};
            std::fill(block, block + sizeof(block), static_cast<std::uint8_t>('a'));
            std::size_t written = 0;
            while (result.success && written < kWorkspaceFileTestBytes) {
                const std::size_t blockBytes = std::min<std::size_t>(
                    sizeof(block), kWorkspaceFileTestBytes - written);
                if (file.write(block, blockBytes) != blockBytes) {
                    result = {false, "Failed to write streaming workspace test file"};
                }
                written += blockBytes;
            }
            file.flush();
            file.close();
        }
    }
    const std::string replacement = "мир";
    if (result.success) {
        Serial.println("FILETEST stage=replace_range");
        result = cardputer::replaceWorkspaceFileRange(
            sourceName, kWorkspaceFileEditOffset,
            static_cast<std::uint32_t>(replacement.size()), replacement);
    }
    const cardputer::WorkspaceChunkResult read = result.success
        ? cardputer::readWorkspaceFileChunk(sourceName, kWorkspaceFileEditOffset, 32)
        : cardputer::WorkspaceChunkResult{false, "", 0, 0, 0, true, result.error};
    if (result.success && (!read.success || read.content.rfind(replacement, 0) != 0 ||
                           read.totalBytes != kWorkspaceFileTestBytes)) {
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
    if (result.success && (!found.success || !found.found ||
                           found.offset != kWorkspaceFileEditOffset)) {
        result = {false, "Workspace search verification failed"};
    }
    if (result.success) {
        Serial.println("FILETEST stage=save_bookmark");
        result = cardputer::saveWorkspaceBookmark(sourceName, kWorkspaceFileEditOffset);
    }
    const cardputer::WorkspaceBookmarkResult sourceBookmark = result.success
        ? cardputer::loadWorkspaceBookmark(sourceName)
        : cardputer::WorkspaceBookmarkResult{false, false, 0, result.error};
    if (result.success && (!sourceBookmark.success || !sourceBookmark.found ||
                           sourceBookmark.offset != kWorkspaceFileEditOffset)) {
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
                           renamedBookmark.offset != kWorkspaceFileEditOffset)) {
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
    bool writeSucceeded = false;
    String writtenName;
    const String expectedName = "firmware_tool_" + String(millis()) + ".py";
    const std::string prompt =
        "Use write_file exactly once with JSON arguments name \"" +
        std::string(expectedName.c_str()) +
        "\" and content \"print('CARDMIND_TOOL_OK')\\n\". Do not answer until the tool result.";
    const std::vector<cardputer::Message> testHistory = {
        {"user", prompt},
    };
    const cardputer::ChatResult result = cardputer::streamChatCompletionWithTools(
        settings, testHistory, "", false, [](const std::string&) {},
        [&writeSucceeded, &writtenName, &expectedName](const cardputer::ToolCall& call) {
            const cardputer::ToolExecutionResult execution =
                cardputer::executeProjectWorkspaceTool(activeProjectId, call);
            if (call.name == "write_file" && execution.success) {
                JsonDocument arguments;
                const DeserializationError error = deserializeJson(arguments, call.arguments);
                if (!error && arguments["name"].is<const char*>()) {
                    writtenName = String(arguments["name"].as<const char*>());
                    writeSucceeded = writtenName == expectedName;
                }
            }
            return execution;
        }, []() { return false; });
    const String testPath = writtenName.isEmpty()
        ? String() : cardputer::workspaceFilePath(writtenName);
    const bool fileCreated = !testPath.isEmpty() && SD.exists(testPath);
    const cardputer::SharedFileLinkResult linked = fileCreated
        ? cardputer::projectHasSharedFileLink(activeProjectId, writtenName)
        : cardputer::SharedFileLinkResult{true, false, ""};
    cardputer::OperationResult cleanup = {true, ""};
    if (fileCreated && linked.success && linked.linked) {
        cleanup = cardputer::unlinkSharedFileFromProject(activeProjectId, writtenName);
    }
    if (fileCreated && cleanup.success) {
        cleanup = cardputer::deleteWorkspaceFile(writtenName);
    }
    if (!result.success || !writeSucceeded || !fileCreated || !linked.success ||
        !linked.linked || !cleanup.success) {
        Serial.printf("TOOLTEST result=failed stage=tool_roundtrip api=%s write=%s file=%s link=%s cleanup=%s error=%s\n",
                      result.success ? "pass" : "failed",
                      writeSucceeded ? "pass" : "failed",
                      fileCreated ? "pass" : "failed",
                      linked.success && linked.linked ? "pass" : "failed",
                      cleanup.success ? "pass" : "failed",
                      !result.error.isEmpty() ? result.error.c_str() :
                          (!linked.success ? linked.error.c_str() : cleanup.error.c_str()));
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

void runUiBenchmark()
{
    constexpr std::size_t kIterations = 12;
    const std::vector<cardputer::Message> sampleHistory = {
        {"user", "Measure a representative CardMind chat frame."},
        {"assistant", "Representative response with UTF-8: русский текст and status details."},
    };
    const std::uint32_t heapBefore = ESP.getFreeHeap();
    const std::uint32_t largestHeapBefore =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    std::uint64_t totalMicroseconds = 0;
    std::uint32_t maximumMicroseconds = 0;
    for (std::size_t iteration = 0; iteration < kIterations; ++iteration) {
        const std::uint32_t startedAt = micros();
        cardputer::showChat(sampleHistory, "Streaming response", "Input draft",
                            cardputer::KeyboardLayout::English, "UI benchmark", "Saved",
                            0, true, batteryLevel, batteryCharging);
        const std::uint32_t elapsed = micros() - startedAt;
        totalMicroseconds += elapsed;
        if (elapsed > maximumMicroseconds) {
            maximumMicroseconds = elapsed;
        }
    }
    const std::uint32_t heapAfter = ESP.getFreeHeap();
    const std::uint32_t largestHeapAfter =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const std::uint32_t averageMicroseconds = totalMicroseconds / kIterations;
    const bool passed = averageMicroseconds <= 50000U && maximumMicroseconds <= 75000U &&
        heapAfter >= heapBefore && largestHeapAfter >= largestHeapBefore;
    render();
    Serial.printf("UIBENCH result=%s iterations=%u average_us=%u maximum_us=%u heap_before=%u heap_after=%u largest_before=%u largest_after=%u\n",
                  passed ? "pass" : "failed",
                  static_cast<unsigned int>(kIterations),
                  static_cast<unsigned int>(averageMicroseconds),
                  static_cast<unsigned int>(maximumMicroseconds),
                  static_cast<unsigned int>(heapBefore),
                  static_cast<unsigned int>(heapAfter),
                  static_cast<unsigned int>(largestHeapBefore),
                  static_cast<unsigned int>(largestHeapAfter));
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
    if (command == "UIBENCH") {
        runUiBenchmark();
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
            result = cardputer::synchronizePythonModeSettings(settings, password, "");
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
    if (command == "AUDIOSTATUS") {
        const cardputer::OperationResult result =
            cardputer::verifyCardputerAdvAudioPoweredDown();
        Serial.printf("AUDIOSTATUS result=%s\n", result.success ? "pass" : "failed");
        return;
    }
    if (command == "TTSSTOPTEST") {
        const cardputer::OperationResult result = cardputer::playTtsHardwareTestControlled(
            settings.ttsVolume, []() { return cardputer::SpeechPlaybackCommand::Stop; });
        const bool passed = result.success && result.error == "Speech playback stopped";
        Serial.printf("TTSSTOPTEST result=%s\n", passed ? "pass" : "failed");
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

}  // namespace
