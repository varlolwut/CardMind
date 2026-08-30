namespace {

cardputer::ToolPermissionPolicy diagnosticMasterToolPolicy()
{
    return {
        cardputer::ToolPermission::Ask,
        cardputer::ToolPermission::Allow,
        cardputer::ToolPermission::Off,
        cardputer::ToolPermission::Ask,
        cardputer::ToolPermission::Allow,
        cardputer::ToolPermission::Ask,
        cardputer::ToolPermission::Off,
        cardputer::ToolPermission::Allow,
    };
}

cardputer::ScopedToolPermissionPolicy diagnosticScopedToolPolicy()
{
    return {
        cardputer::ScopedToolPermission::Inherit,
        cardputer::ScopedToolPermission::Off,
        cardputer::ScopedToolPermission::Ask,
        cardputer::ScopedToolPermission::Allow,
        cardputer::ScopedToolPermission::Ask,
        cardputer::ScopedToolPermission::Allow,
        cardputer::ScopedToolPermission::Off,
        cardputer::ScopedToolPermission::Inherit,
    };
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
    refreshRuntimeSdState();
    Serial.printf("STATUS version=%s board_adv=%s configured=%s voice_configured=%s search_configured=%s tts_configured=%s tts_auto=%s microsd=%s microsd_state=%s microsd_error=%s chats=%s chat_count=%u files=%s crash_journal=%s previous_operation=%s wifi=%s tls_time=%s battery=%d charging=%s history=%u heap=%u largest_heap=%u min_heap=%u stack_free=%u brightness=%u sleep_min=%u repeat_ms=%u power=%u cpu_mhz=%u reset_reason=%d\n",
                  kFirmwareVersion,
                  M5.getBoard() == m5::board_t::board_M5CardputerADV ? "yes" : "no",
                  cardputer::settingsAreComplete(settings) ? "yes" : "no",
                  cardputer::voiceSettingsAreComplete(settings) ? "yes" : "no",
                  cardputer::webSearchSettingsAreComplete(settings) ? "yes" : "no",
                  cardputer::ttsSettingsAreComplete(settings) ? "yes" : "no",
                  settings.ttsAutoPlay ? "yes" : "no",
                  currentSdStorageStatus.state == cardputer::SdStorageState::Ready
                      ? "ready" : "unavailable",
                  cardputer::sdStorageStateName(currentSdStorageStatus.state),
                  cardputer::sdStorageErrorCode(currentSdStorageStatus.state),
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
    cardputer::Settings requestSettings = settings;
    requestSettings.masterToolPolicy =
        cardputer::defaultGlobalToolPermissionPolicy();
    cardputer::ProjectDocument project = {};
    project.toolPolicy = cardputer::inheritedToolPermissionPolicy();
    cardputer::ChatDocument chat = {};
    chat.toolPolicy = cardputer::inheritedToolPermissionPolicy();
    const std::uint8_t webGroup = static_cast<std::uint8_t>(
        1U << static_cast<std::uint8_t>(cardputer::ToolCapabilityGroup::Web));
    const bool webStorageWritable = cardputer::requireSdWriteAccess(
        0, cardputer::kStorageOperationalFloorBytes).success;
    const cardputer::ToolRequestPlan requestPlan =
        cardputer::resolveChatToolRequestPlan(
            requestSettings, project, chat,
            {cardputer::ToolMessageIntentMode::Required, webGroup},
            false, false, webStorageWritable, false);
    const String planError = toolRequestPlanError(requestPlan);
    if (!planError.isEmpty()) {
        Serial.printf(
            "SEARCHTEST result=failed search_called=no tool=none response_bytes=0 error=%s\n",
            planError.c_str());
        return;
    }
    const cardputer::ChatResult result = cardputer::streamChatCompletionWithTools(
        requestSettings, testHistory, "", requestPlan, [](const std::string&) {},
        [&requestSettings, &searchCalled, &observedToolName,
         &requestPlan](const cardputer::ToolCall& call) {
            if (observedToolName.isEmpty()) {
                observedToolName = String(call.name.c_str());
            }
            const cardputer::ToolExecutionResult execution =
                cardputer::routeToolCall(
                requestSettings, requestPlan, call, []() { return false; });
            if (call.name == "web_search" && execution.success) {
                searchCalled = true;
            }
            return execution;
        },
        [](const cardputer::PendingToolContinuation&) {
            return cardputer::OperationResult{
                false, "SEARCHTEST did not authorize confirmation"};
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
    document.toolPolicy = cardputer::setLegacySshToolsEnabled(
        document.toolPolicy, true);
    Serial.println("STORAGETEST stage=chat_save");
    const cardputer::OperationResult saved = cardputer::saveChat(document);
    Serial.println("STORAGETEST stage=chat_load");
    const cardputer::ChatDocumentResult loaded = saved.success
        ? cardputer::loadChat(document.summary.id)
        : cardputer::ChatDocumentResult{false, {}, saved.error};
    const bool chatVerified = loaded.success && loaded.chat.messages.size() == 2 &&
        loaded.chat.messages[1].content == "OK" &&
        loaded.chat.instructions == "Reply briefly." &&
        cardputer::legacySshToolsEnabled(loaded.chat.toolPolicy);
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

void runHotfixNavigationLatencyTest()
{
    constexpr std::uint32_t kIterations = 8;
    if (!chatStorageReady || activeProjectId.isEmpty()) {
        Serial.println("HOTFIXNAVTEST result=failed error=active_project_unavailable");
        return;
    }
    cardputer::OperationResult result = {true, ""};
    const std::uint32_t projectsStartedAt = millis();
    for (std::uint32_t index = 0; index < kIterations && result.success; ++index) {
        const cardputer::ProjectsPageResult page = cardputer::listProjectsPage(
            0, cardputer::kMaximumProjectPageEntries);
        if (!page.success) result = {false, page.error};
    }
    const std::uint32_t projectsElapsedMs = millis() - projectsStartedAt;
    const std::uint32_t chatsStartedAt = millis();
    for (std::uint32_t index = 0; index < kIterations && result.success; ++index) {
        const cardputer::ProjectChatsPageResult page = cardputer::listProjectChatsPage(
            activeProjectId, 0, cardputer::kMaximumProjectPageEntries);
        if (!page.success) result = {false, page.error};
    }
    const std::uint32_t chatsElapsedMs = millis() - chatsStartedAt;
    const bool responsive = result.success && projectsElapsedMs <= 1600 &&
        chatsElapsedMs <= 1600;
    Serial.printf("HOTFIXNAVTEST result=%s iterations=%u projects_ms=%u chats_ms=%u average_ms=%u error=%s\n",
                  responsive ? "pass" : "failed",
                  static_cast<unsigned int>(kIterations),
                  static_cast<unsigned int>(projectsElapsedMs),
                  static_cast<unsigned int>(chatsElapsedMs),
                  static_cast<unsigned int>(
                      (projectsElapsedMs + chatsElapsedMs) / (kIterations * 2U)),
                  result.success ? (responsive ? "none" : "latency_budget_exceeded")
                                 : result.error.c_str());
}

void runHotfixInputLatencyTest()
{
    constexpr std::uint32_t kFullIterations = 4;
    constexpr std::uint32_t kInputIterations = 32;
    std::vector<cardputer::Message> benchmarkHistory;
    benchmarkHistory.reserve(32);
    const std::string payload(240, 'x');
    for (std::uint32_t index = 0; index < 32; ++index) {
        benchmarkHistory.push_back({index % 2 == 0 ? "user" : "assistant", payload});
    }
    const std::uint32_t fullStartedAt = micros();
    for (std::uint32_t index = 0; index < kFullIterations; ++index) {
        cardputer::showChat(
            benchmarkHistory, "", inputBuffer, keyboardLayout, activeChatTitle,
            statusMessage, 0,
            {cardputer::ChatCapabilityState::Inherit,
             cardputer::ChatCapabilityState::Inherit,
             cardputer::ChatCapabilityState::Inherit,
             cardputer::ChatCapabilityState::Inherit},
            WiFi.status() == WL_CONNECTED, batteryLevel,
            batteryCharging);
    }
    const std::uint32_t fullElapsedUs = micros() - fullStartedAt;
    const std::uint32_t inputStartedAt = micros();
    for (std::uint32_t index = 0; index < kInputIterations; ++index) {
        cardputer::updateChatInput(inputBuffer + std::to_string(index));
    }
    const std::uint32_t inputElapsedUs = micros() - inputStartedAt;
    const std::uint32_t fullAverageUs = fullElapsedUs / kFullIterations;
    const std::uint32_t inputAverageUs = inputElapsedUs / kInputIterations;
    const bool responsive = inputAverageUs <= 50000U && inputAverageUs < fullAverageUs;
    render();
    Serial.printf(
        "HOTFIXINPUTTEST result=%s full_average_us=%u input_average_us=%u error=%s\n",
        responsive ? "pass" : "failed",
        static_cast<unsigned int>(fullAverageUs),
        static_cast<unsigned int>(inputAverageUs),
        responsive ? "none" : "input_render_budget_exceeded");
}

void runHotfixSdAccessSafetyTest()
{
    if (activeProjectId.isEmpty()) {
        Serial.println("HOTFIXSDTEST result=failed removed=failed replaced=failed nonmutation=failed error=active_project_unavailable");
        return;
    }
    const String target = cardputer::projectDirectoryPath(activeProjectId) + "/project.json";
    const bool targetBefore = SD.exists(target);
    const bool temporaryBefore = SD.exists(target + ".tmp");
    const bool recoveryBefore = SD.exists(target + ".bak");
    cardputer::setSdStorageFaultOverrideForDiagnostics(cardputer::SdStorageState::Removed);
    const cardputer::OperationResult removed = cardputer::recoverAtomicSdFile(target);
    cardputer::setSdStorageFaultOverrideForDiagnostics(cardputer::SdStorageState::Replaced);
    const cardputer::OperationResult replaced = cardputer::recoverAtomicSdFile(target);
    cardputer::clearSdStorageFaultOverrideForDiagnostics();
    const bool nonmutation = targetBefore == SD.exists(target) &&
        temporaryBefore == SD.exists(target + ".tmp") &&
        recoveryBefore == SD.exists(target + ".bak");
    const bool passed = !removed.success && !replaced.success && nonmutation;
    Serial.printf(
        "HOTFIXSDTEST result=%s removed=%s replaced=%s nonmutation=%s error=%s\n",
        passed ? "pass" : "failed", !removed.success ? "pass" : "failed",
        !replaced.success ? "pass" : "failed", nonmutation ? "pass" : "failed",
        passed ? "none" : "sd_access_guard_failed");
}

struct P2UnicodeFileDigestResult {
    bool success;
    std::uint32_t bytes;
    std::uint32_t fnv32;
    String error;
};

struct P2UnicodeOwnershipResult {
    bool success;
    String state;
    bool completed;
    std::uint32_t bytes;
    std::uint32_t fnv32;
    bool testAddedTrust;
    bool trustCleaned;
    String trustFingerprint;
    String error;
};

struct P2UnicodeCleanupResult {
    bool success;
    bool alreadyAbsent;
    std::uint32_t removedFiles;
    std::uint32_t removedDirectories;
    String error;
};

bool p2UnicodeFixtureIsEntirelyAbsent(const String& nonce);

bool isP2UnicodeNonce(const String& nonce)
{
    if (nonce.isEmpty() || nonce.length() > 20) {
        return false;
    }
    for (std::size_t index = 0; index < nonce.length(); ++index) {
        if (nonce[index] < '0' || nonce[index] > '9') {
            return false;
        }
    }
    return true;
}

String p2UnicodeTopDirectory(const String& nonce)
{
    return "cardmind_p2_19_" + nonce;
}

String p2UnicodeFirstDirectory(const String& nonce)
{
    return p2UnicodeTopDirectory(nonce) + "/Проекты-世界";
}

String p2UnicodeLeafDirectory(const String& nonce)
{
    return p2UnicodeFirstDirectory(nonce) + "/глубокая-папка-مرحبا";
}

String p2UnicodeFileName(const String& nonce)
{
    return p2UnicodeLeafDirectory(nonce) + "/заметки-データ-🌍.txt";
}

String p2UnicodeOwnershipName(const String& nonce)
{
    return p2UnicodeTopDirectory(nonce) + "/.cardmind-p2-19-owner.json";
}

P2UnicodeFileDigestResult digestP2UnicodeFile(const String& name)
{
    File file = SD.open(cardputer::workspaceFilePath(name), FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return {false, 0, 0, "P2 Unicode fixture could not be opened"};
    }
    if (file.size() > std::numeric_limits<std::uint32_t>::max()) {
        file.close();
        return {false, 0, 0, "P2 Unicode fixture exceeds the diagnostic offset range"};
    }
    const std::uint32_t expectedBytes = static_cast<std::uint32_t>(file.size());
    std::uint32_t bytes = 0;
    std::uint32_t fnv32 = 2166136261U;
    std::uint8_t buffer[1024] = {};
    while (file.available() > 0) {
        const std::size_t readBytes = file.read(buffer, sizeof(buffer));
        if (readBytes == 0) {
            file.close();
            return {false, bytes, fnv32,
                    "microSD stopped before the P2 Unicode fixture was complete"};
        }
        for (std::size_t index = 0; index < readBytes; ++index) {
            fnv32 ^= buffer[index];
            fnv32 *= 16777619U;
        }
        bytes += static_cast<std::uint32_t>(readBytes);
    }
    file.close();
    return bytes == expectedBytes
        ? P2UnicodeFileDigestResult{true, bytes, fnv32, ""}
        : P2UnicodeFileDigestResult{false, bytes, fnv32,
                                    "P2 Unicode fixture size changed while hashing"};
}

cardputer::OperationResult writeP2UnicodeOwnership(const String& nonce,
                                                   const P2UnicodeOwnershipResult& ownership)
{
    const String ownershipName = p2UnicodeOwnershipName(nonce);
    const cardputer::OperationResult parent = cardputer::ensureWorkspaceFileParent(
        ownershipName);
    if (!parent.success) {
        return parent;
    }
    JsonDocument document;
    document["version"] = 1;
    document["nonce"] = nonce;
    document["path"] = p2UnicodeFileName(nonce);
    document["state"] = ownership.state;
    document["content_verified"] = ownership.completed;
    document["trust_host"] = "test.rebex.net";
    document["trust_port"] = 22;
    document["test_added_trust"] = ownership.testAddedTrust;
    document["trust_cleaned"] = ownership.trustCleaned;
    document["trust_fingerprint"] = ownership.trustFingerprint;
    if (ownership.completed) {
        document["bytes"] = ownership.bytes;
        document["fnv32"] = ownership.fnv32;
    }
    return cardputer::writeAtomicJsonSdFile(
        cardputer::workspaceFilePath(ownershipName), document);
}

P2UnicodeOwnershipResult loadP2UnicodeOwnership(const String& nonce)
{
    const String ownershipPath = cardputer::workspaceFilePath(
        p2UnicodeOwnershipName(nonce));
    const cardputer::OperationResult recovered = cardputer::recoverAtomicSdFile(
        ownershipPath);
    if (!recovered.success) {
        return {false, "", false, 0, 0, false, false, "", recovered.error};
    }
    File file = SD.open(ownershipPath, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return {false, "", false, 0, 0, false, false, "",
                "P2 Unicode ownership metadata is missing"};
    }
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, file);
    file.close();
    if (error || !document["version"].is<std::uint32_t>() ||
        document["version"].as<std::uint32_t>() != 1 ||
        !document["nonce"].is<const char*>() ||
        String(document["nonce"].as<const char*>()) != nonce ||
        !document["path"].is<const char*>() ||
        String(document["path"].as<const char*>()) != p2UnicodeFileName(nonce) ||
        !document["state"].is<const char*>() ||
        !document["content_verified"].is<bool>() ||
        !document["trust_host"].is<const char*>() ||
        String(document["trust_host"].as<const char*>()) != "test.rebex.net" ||
        !document["trust_port"].is<std::uint16_t>() ||
        document["trust_port"].as<std::uint16_t>() != 22 ||
        !document["test_added_trust"].is<bool>() ||
        !document["trust_cleaned"].is<bool>() ||
        !document["trust_fingerprint"].is<const char*>()) {
        return {false, "", false, 0, 0, false, false, "",
                "P2 Unicode ownership metadata is invalid or belongs to another run"};
    }
    const String state = document["state"].as<const char*>();
    const bool contentVerified = document["content_verified"].as<bool>();
    const bool testAddedTrust = document["test_added_trust"].as<bool>();
    const bool trustCleaned = document["trust_cleaned"].as<bool>();
    const String trustFingerprint = document["trust_fingerprint"].as<const char*>();
    if ((!testAddedTrust && (!trustCleaned || !trustFingerprint.isEmpty())) ||
        (testAddedTrust && trustFingerprint.isEmpty())) {
        return {false, "", false, 0, 0, false, false, "",
                "P2 Unicode ownership trust record is inconsistent"};
    }
    if (state == "pending" && !contentVerified) {
        return {true, state, false, 0, 0, testAddedTrust, trustCleaned,
                trustFingerprint, ""};
    }
    if ((state != "complete" && state != "cleaning") ||
        (state == "complete" && !contentVerified) ||
        (contentVerified && (!document["bytes"].is<std::uint32_t>() ||
                             !document["fnv32"].is<std::uint32_t>()))) {
        return {false, "", false, 0, 0, false, false, "",
                "P2 Unicode ownership metadata has an invalid completion record"};
    }
    return {true, state, contentVerified,
            contentVerified ? document["bytes"].as<std::uint32_t>() : 0,
            contentVerified ? document["fnv32"].as<std::uint32_t>() : 0,
            testAddedTrust, trustCleaned,
            trustFingerprint, ""};
}

cardputer::OperationResult cleanupP2UnicodeTemporaryTrust(
    const String& nonce,
    P2UnicodeOwnershipResult& ownership)
{
    if (!ownership.testAddedTrust || ownership.trustCleaned) {
        return {true, ""};
    }
    const cardputer::OperationResult initialized = cardputer::initializeSshStorage();
    if (!initialized.success) {
        return initialized;
    }
    const cardputer::SshTrustResult trust = cardputer::checkTrustedSshHost(
        "test.rebex.net", 22, ownership.trustFingerprint);
    if (!trust.success) {
        return {false, trust.error};
    }
    if (trust.found && !trust.matches) {
        return {false, "Stored Rebex trust does not match the P2 Unicode ownership record"};
    }
    if (trust.found) {
        const cardputer::OperationResult forgotten = cardputer::forgetTrustedSshHost(
            "test.rebex.net", 22);
        if (!forgotten.success) {
            return forgotten;
        }
    }
    ownership.trustCleaned = true;
    return writeP2UnicodeOwnership(nonce, ownership);
}

cardputer::OperationResult removeP2UnicodeEmptyDirectory(const String& relativePath,
                                                         bool& removed)
{
    removed = false;
    const String path = cardputer::workspaceFilePath(relativePath);
    if (!SD.exists(path)) {
        return {true, ""};
    }
    File directory = SD.open(path, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) {
            directory.close();
        }
        return {false, "P2 Unicode cleanup path is not a directory: " + relativePath};
    }
    File entry = directory.openNextFile();
    if (entry) {
        entry.close();
        directory.close();
        return {false, "P2 Unicode cleanup directory is not empty: " + relativePath};
    }
    directory.close();
    if (!SD.rmdir(path)) {
        return {false, "Failed to remove empty P2 Unicode directory: " + relativePath};
    }
    removed = true;
    return {true, ""};
}

cardputer::OperationResult validateP2UnicodeDirectoryEntries(
    const String& relativePath,
    const String* expectedPaths,
    const bool* expectedDirectories,
    std::size_t expectedCount)
{
    if (expectedCount > 4) {
        return {false, "P2 Unicode directory validation contract is invalid"};
    }
    File directory = SD.open(cardputer::workspaceFilePath(relativePath), FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) {
            directory.close();
        }
        return {false, "P2 Unicode owned directory is missing: " + relativePath};
    }
    bool found[4] = {false, false, false, false};
    File entry = directory.openNextFile();
    while (entry) {
        const String entryPath = entry.path();
        const bool entryDirectory = entry.isDirectory();
        bool matched = false;
        for (std::size_t index = 0; index < expectedCount; ++index) {
            if (!found[index] && entryPath == expectedPaths[index] &&
                entryDirectory == expectedDirectories[index]) {
                found[index] = true;
                matched = true;
                break;
            }
        }
        entry.close();
        if (!matched) {
            directory.close();
            return {false, "P2 Unicode owned directory contains an unexpected entry: " +
                               relativePath};
        }
        entry = directory.openNextFile();
    }
    directory.close();
    for (std::size_t index = 0; index < expectedCount; ++index) {
        if (!found[index]) {
            return {false, "P2 Unicode owned directory is incomplete: " + relativePath};
        }
    }
    return {true, ""};
}

cardputer::OperationResult validateP2UnicodeOwnedTree(const String& nonce)
{
    const String targetPaths[] = {cardputer::workspaceFilePath(p2UnicodeFileName(nonce))};
    const bool targetTypes[] = {false};
    cardputer::OperationResult result = validateP2UnicodeDirectoryEntries(
        p2UnicodeLeafDirectory(nonce), targetPaths, targetTypes, 1);
    if (!result.success) {
        return result;
    }
    const String leafPaths[] = {cardputer::workspaceFilePath(
        p2UnicodeLeafDirectory(nonce))};
    const bool leafTypes[] = {true};
    result = validateP2UnicodeDirectoryEntries(
        p2UnicodeFirstDirectory(nonce), leafPaths, leafTypes, 1);
    if (!result.success) {
        return result;
    }
    const String topPaths[] = {
        cardputer::workspaceFilePath(p2UnicodeFirstDirectory(nonce)),
        cardputer::workspaceFilePath(p2UnicodeOwnershipName(nonce)),
    };
    const bool topTypes[] = {true, false};
    return validateP2UnicodeDirectoryEntries(
        p2UnicodeTopDirectory(nonce), topPaths, topTypes, 2);
}

cardputer::OperationResult validatePendingP2UnicodeOwnedTree(const String& nonce)
{
    const String name = p2UnicodeFileName(nonce);
    const String target = cardputer::workspaceFilePath(name);
    const String leaf = cardputer::workspaceFilePath(p2UnicodeLeafDirectory(nonce));
    const String first = cardputer::workspaceFilePath(p2UnicodeFirstDirectory(nonce));
    const String top = cardputer::workspaceFilePath(p2UnicodeTopDirectory(nonce));
    const String ownership = cardputer::workspaceFilePath(p2UnicodeOwnershipName(nonce));
    if (!SD.exists(top) || !SD.exists(ownership)) {
        return {false, "Pending P2 Unicode owned tree is missing its root metadata"};
    }
    if (SD.exists(leaf) && !SD.exists(first)) {
        return {false, "Pending P2 Unicode owned tree has a detached leaf directory"};
    }
    if (SD.exists(first) && !SD.exists(top)) {
        return {false, "Pending P2 Unicode owned tree has a detached nested directory"};
    }
    if (SD.exists(leaf)) {
        const String candidates[] = {
            target, target + ".sftp-part", target + ".tmp", target + ".bak",
        };
        String expectedPaths[4];
        bool expectedTypes[4] = {false, false, false, false};
        std::size_t expectedCount = 0;
        for (const String& candidate : candidates) {
            if (SD.exists(candidate)) {
                expectedPaths[expectedCount++] = candidate;
            }
        }
        const cardputer::OperationResult leafResult = validateP2UnicodeDirectoryEntries(
            p2UnicodeLeafDirectory(nonce), expectedPaths, expectedTypes, expectedCount);
        if (!leafResult.success) {
            return leafResult;
        }
    }
    if (SD.exists(first)) {
        String expectedPaths[1];
        bool expectedTypes[1] = {true};
        const std::size_t expectedCount = SD.exists(leaf) ? 1 : 0;
        if (expectedCount == 1) {
            expectedPaths[0] = leaf;
        }
        const cardputer::OperationResult firstResult = validateP2UnicodeDirectoryEntries(
            p2UnicodeFirstDirectory(nonce), expectedPaths, expectedTypes, expectedCount);
        if (!firstResult.success) {
            return firstResult;
        }
    }
    String topPaths[2] = {ownership, ""};
    bool topTypes[2] = {false, true};
    std::size_t topCount = 1;
    if (SD.exists(first)) {
        topPaths[topCount++] = first;
    }
    return validateP2UnicodeDirectoryEntries(
        p2UnicodeTopDirectory(nonce), topPaths, topTypes, topCount);
}

cardputer::OperationResult validateVerifiedP2UnicodeCleaningTree(const String& nonce)
{
    const String name = p2UnicodeFileName(nonce);
    const String target = cardputer::workspaceFilePath(name);
    const String leaf = cardputer::workspaceFilePath(p2UnicodeLeafDirectory(nonce));
    const String first = cardputer::workspaceFilePath(p2UnicodeFirstDirectory(nonce));
    const String top = cardputer::workspaceFilePath(p2UnicodeTopDirectory(nonce));
    const String ownership = cardputer::workspaceFilePath(p2UnicodeOwnershipName(nonce));
    if (SD.exists(target + ".sftp-part") || SD.exists(target + ".tmp") ||
        SD.exists(target + ".bak")) {
        return {false, "Verified P2 Unicode cleanup found an unexpected target artifact"};
    }
    if (!SD.exists(top) || !SD.exists(ownership)) {
        return {false, "Verified P2 Unicode cleanup is missing its ownership root"};
    }
    if (SD.exists(leaf)) {
        String expectedPaths[1];
        bool expectedTypes[1] = {false};
        const std::size_t expectedCount = SD.exists(target) ? 1 : 0;
        if (expectedCount == 1) {
            expectedPaths[0] = target;
        }
        const cardputer::OperationResult leafResult = validateP2UnicodeDirectoryEntries(
            p2UnicodeLeafDirectory(nonce), expectedPaths, expectedTypes, expectedCount);
        if (!leafResult.success) {
            return leafResult;
        }
    } else if (SD.exists(target)) {
        return {false, "Verified P2 Unicode target is detached from its leaf directory"};
    }
    if (SD.exists(first)) {
        String expectedPaths[1];
        bool expectedTypes[1] = {true};
        const std::size_t expectedCount = SD.exists(leaf) ? 1 : 0;
        if (expectedCount == 1) {
            expectedPaths[0] = leaf;
        }
        const cardputer::OperationResult firstResult = validateP2UnicodeDirectoryEntries(
            p2UnicodeFirstDirectory(nonce), expectedPaths, expectedTypes, expectedCount);
        if (!firstResult.success) {
            return firstResult;
        }
    } else if (SD.exists(leaf)) {
        return {false, "Verified P2 Unicode leaf is detached from its parent directory"};
    }
    String topPaths[2] = {ownership, ""};
    bool topTypes[2] = {false, true};
    std::size_t topCount = 1;
    if (SD.exists(first)) {
        topPaths[topCount++] = first;
    }
    return validateP2UnicodeDirectoryEntries(
        p2UnicodeTopDirectory(nonce), topPaths, topTypes, topCount);
}

P2UnicodeCleanupResult cleanupOwnerlessEmptyP2UnicodeTree(const String& nonce)
{
    const String name = p2UnicodeFileName(nonce);
    const String target = cardputer::workspaceFilePath(name);
    const String ownership = cardputer::workspaceFilePath(p2UnicodeOwnershipName(nonce));
    if (SD.exists(target) || SD.exists(target + ".sftp-part") ||
        SD.exists(target + ".tmp") || SD.exists(target + ".bak") ||
        SD.exists(ownership) || SD.exists(ownership + ".tmp") ||
        SD.exists(ownership + ".bak")) {
        return {false, false, 0, 0,
                "Ownerless P2 Unicode recovery found a file and preserved it"};
    }
    const String leaf = cardputer::workspaceFilePath(p2UnicodeLeafDirectory(nonce));
    const String first = cardputer::workspaceFilePath(p2UnicodeFirstDirectory(nonce));
    const String top = cardputer::workspaceFilePath(p2UnicodeTopDirectory(nonce));
    if (!SD.exists(top)) {
        return {false, false, 0, 0,
                "Ownerless P2 Unicode recovery has no exact root directory"};
    }
    if (SD.exists(leaf)) {
        const cardputer::OperationResult leafResult = validateP2UnicodeDirectoryEntries(
            p2UnicodeLeafDirectory(nonce), nullptr, nullptr, 0);
        if (!leafResult.success) {
            return {false, false, 0, 0, leafResult.error};
        }
    }
    if (SD.exists(first)) {
        String expectedPaths[1];
        bool expectedTypes[1] = {true};
        const std::size_t expectedCount = SD.exists(leaf) ? 1 : 0;
        if (expectedCount == 1) {
            expectedPaths[0] = leaf;
        }
        const cardputer::OperationResult firstResult = validateP2UnicodeDirectoryEntries(
            p2UnicodeFirstDirectory(nonce), expectedPaths, expectedTypes, expectedCount);
        if (!firstResult.success) {
            return {false, false, 0, 0, firstResult.error};
        }
    } else if (SD.exists(leaf)) {
        return {false, false, 0, 0,
                "Ownerless P2 Unicode leaf is detached from its parent"};
    }
    String topPaths[1];
    bool topTypes[1] = {true};
    const std::size_t topCount = SD.exists(first) ? 1 : 0;
    if (topCount == 1) {
        topPaths[0] = first;
    }
    const cardputer::OperationResult topResult = validateP2UnicodeDirectoryEntries(
        p2UnicodeTopDirectory(nonce), topPaths, topTypes, topCount);
    if (!topResult.success) {
        return {false, false, 0, 0, topResult.error};
    }
    std::uint32_t removedDirectories = 0;
    const String directories[] = {p2UnicodeLeafDirectory(nonce),
                                  p2UnicodeFirstDirectory(nonce),
                                  p2UnicodeTopDirectory(nonce)};
    for (const String& directory : directories) {
        bool removed = false;
        const cardputer::OperationResult result = removeP2UnicodeEmptyDirectory(
            directory, removed);
        if (!result.success) {
            return {false, false, 0, removedDirectories, result.error};
        }
        if (removed) {
            ++removedDirectories;
        }
    }
    return p2UnicodeFixtureIsEntirelyAbsent(nonce)
        ? P2UnicodeCleanupResult{true, false, 0, removedDirectories, ""}
        : P2UnicodeCleanupResult{false, false, 0, removedDirectories,
                                 "Ownerless P2 Unicode recovery left paths behind"};
}

bool p2UnicodeFixtureIsEntirelyAbsent(const String& nonce)
{
    const String name = p2UnicodeFileName(nonce);
    const String target = cardputer::workspaceFilePath(name);
    const String ownership = cardputer::workspaceFilePath(
        p2UnicodeOwnershipName(nonce));
    return !SD.exists(target) && !SD.exists(target + ".sftp-part") &&
        !SD.exists(target + ".tmp") && !SD.exists(target + ".bak") &&
        !SD.exists(ownership) && !SD.exists(ownership + ".tmp") &&
        !SD.exists(ownership + ".bak") &&
        !SD.exists(cardputer::workspaceFilePath(p2UnicodeLeafDirectory(nonce))) &&
        !SD.exists(cardputer::workspaceFilePath(p2UnicodeFirstDirectory(nonce))) &&
        !SD.exists(cardputer::workspaceFilePath(p2UnicodeTopDirectory(nonce)));
}

P2UnicodeCleanupResult cleanupP2UnicodeFixture(const String& nonce)
{
    if (p2UnicodeFixtureIsEntirelyAbsent(nonce)) {
        return {true, true, 0, 0, ""};
    }
    P2UnicodeOwnershipResult ownership = loadP2UnicodeOwnership(nonce);
    if (!ownership.success) {
        const String ownershipPath = cardputer::workspaceFilePath(
            p2UnicodeOwnershipName(nonce));
        if (!SD.exists(ownershipPath) && !SD.exists(ownershipPath + ".tmp") &&
            !SD.exists(ownershipPath + ".bak")) {
            const P2UnicodeCleanupResult ownerless =
                cleanupOwnerlessEmptyP2UnicodeTree(nonce);
            return ownerless;
        }
        return {false, false, 0, 0, ownership.error};
    }
    cardputer::OperationResult result = cleanupP2UnicodeTemporaryTrust(
        nonce, ownership);
    if (!result.success) {
        return {false, false, 0, 0, result.error};
    }
    const String name = p2UnicodeFileName(nonce);
    const String target = cardputer::workspaceFilePath(name);
    if (ownership.state == "complete") {
        if (!SD.exists(target)) {
            return {false, false, 0, 0,
                    "P2 Unicode owned target is missing while metadata remains"};
        }
        if (SD.exists(target + ".sftp-part") || SD.exists(target + ".tmp") ||
            SD.exists(target + ".bak")) {
            return {false, false, 0, 0,
                    "P2 Unicode target has ambiguous recovery artifacts"};
        }
        const P2UnicodeFileDigestResult digest = digestP2UnicodeFile(name);
        if (!digest.success || digest.bytes != ownership.bytes ||
            digest.fnv32 != ownership.fnv32) {
            return {false, false, 0, 0,
                    digest.success
                        ? String("P2 Unicode target no longer matches its ownership record")
                        : digest.error};
        }
        const cardputer::OperationResult tree = validateP2UnicodeOwnedTree(nonce);
        if (!tree.success) {
            return {false, false, 0, 0, tree.error};
        }
        ownership.state = "cleaning";
        result = writeP2UnicodeOwnership(nonce, ownership);
        if (!result.success) {
            return {false, false, 0, 0, result.error};
        }
    } else if (ownership.state == "pending") {
        const cardputer::OperationResult tree = validatePendingP2UnicodeOwnedTree(nonce);
        if (!tree.success) {
            return {false, false, 0, 0, tree.error};
        }
        ownership.state = "cleaning";
        result = writeP2UnicodeOwnership(nonce, ownership);
        if (!result.success) {
            return {false, false, 0, 0, result.error};
        }
    } else if (ownership.state == "cleaning") {
        const cardputer::OperationResult tree = ownership.completed
            ? validateVerifiedP2UnicodeCleaningTree(nonce)
            : validatePendingP2UnicodeOwnedTree(nonce);
        if (!tree.success) {
            return {false, false, 0, 0, tree.error};
        }
        if (ownership.completed && SD.exists(target)) {
            const P2UnicodeFileDigestResult digest = digestP2UnicodeFile(name);
            if (!digest.success || digest.bytes != ownership.bytes ||
                digest.fnv32 != ownership.fnv32) {
                return {false, false, 0, 0,
                        digest.success
                            ? String("P2 Unicode cleaning target no longer matches ownership")
                            : digest.error};
            }
        }
    } else {
        return {false, false, 0, 0, "P2 Unicode cleanup state is unsupported"};
    }
    std::uint32_t removedFiles = 0;
    std::uint32_t removedDirectories = 0;
    if (SD.exists(target)) {
        const cardputer::OperationResult removed = cardputer::deleteWorkspaceFile(name);
        if (!removed.success) {
            return {false, false, removedFiles, removedDirectories, removed.error};
        }
        ++removedFiles;
    }
    const String artifacts[] = {target + ".sftp-part", target + ".tmp", target + ".bak"};
    for (const String& artifact : artifacts) {
        if (SD.exists(artifact)) {
            if (ownership.completed || !SD.remove(artifact)) {
                return {false, false, removedFiles, removedDirectories,
                        ownership.completed
                            ? String("P2 Unicode cleanup refused an ambiguous artifact")
                            : String("Failed to remove an owned P2 Unicode transfer artifact")};
            }
            ++removedFiles;
        }
    }
    const String childDirectories[] = {p2UnicodeLeafDirectory(nonce),
                                       p2UnicodeFirstDirectory(nonce)};
    for (const String& directory : childDirectories) {
        bool removed = false;
        result = removeP2UnicodeEmptyDirectory(directory, removed);
        if (!result.success) {
            return {false, false, removedFiles, removedDirectories, result.error};
        }
        if (removed) {
            ++removedDirectories;
        }
    }
    const String ownershipPath = cardputer::workspaceFilePath(
        p2UnicodeOwnershipName(nonce));
    if (SD.exists(ownershipPath + ".tmp") || SD.exists(ownershipPath + ".bak")) {
        return {false, false, removedFiles, removedDirectories,
                "P2 Unicode ownership metadata has unresolved atomic artifacts"};
    }
    if (!SD.exists(ownershipPath) || !SD.remove(ownershipPath)) {
        return {false, false, removedFiles, removedDirectories,
                "Failed to remove P2 Unicode ownership metadata"};
    }
    ++removedFiles;
    bool removedTop = false;
    result = removeP2UnicodeEmptyDirectory(p2UnicodeTopDirectory(nonce), removedTop);
    if (!result.success) {
        return {false, false, removedFiles, removedDirectories, result.error};
    }
    if (removedTop) {
        ++removedDirectories;
    }
    if (!p2UnicodeFixtureIsEntirelyAbsent(nonce)) {
        return {false, false, removedFiles, removedDirectories,
                "P2 Unicode cleanup left fixture paths behind"};
    }
    return {true, false, removedFiles, removedDirectories, ""};
}

cardputer::OperationResult downloadP2UnicodeFixture(
    const String& nonce,
    const String& name,
    P2UnicodeOwnershipResult& ownership)
{
    const cardputer::SshProfile profile = {
        "Rebex test", "test.rebex.net", 22, "demo", "password",
        cardputer::SshAuthMode::Password, ""};
    cardputer::SshClient client;
    cardputer::OperationResult result = cardputer::initializeSshStorage();
    if (result.success) {
        result = client.connect(profile, 60000);
    }
    bool temporaryTrust = false;
    if (result.success) {
        const cardputer::SshTrustResult trust = cardputer::checkTrustedSshHost(
            profile.host, profile.port, client.fingerprint());
        if (!trust.success || (trust.found && !trust.matches)) {
            result = {false, trust.success ? String("Rebex SFTP host key changed")
                                           : trust.error};
        } else if (!trust.found) {
            ownership.testAddedTrust = true;
            ownership.trustCleaned = false;
            ownership.trustFingerprint = client.fingerprint();
            result = writeP2UnicodeOwnership(nonce, ownership);
            if (result.success) {
                result = cardputer::trustSshHost(
                    profile.host, profile.port, ownership.trustFingerprint);
                temporaryTrust = result.success;
            }
        }
    }
    if (result.success) {
        result = client.authenticate(profile, 60000);
    }
    if (result.success) {
        result = client.openSftp(30000);
    }
    if (result.success) {
        result = client.downloadSftpFile("/pub/example/readme.txt", name, 60000);
    }
    client.close();
    if (temporaryTrust || (ownership.testAddedTrust && !ownership.trustCleaned)) {
        const cardputer::OperationResult trustCleanup = cleanupP2UnicodeTemporaryTrust(
            nonce, ownership);
        if (!trustCleanup.success) {
            result = result.success
                ? trustCleanup
                : cardputer::OperationResult{
                    false, result.error + "; temporary trust cleanup also failed: " +
                        trustCleanup.error};
        }
    }
    return result;
}

cardputer::OperationResult findP2UnicodeFixtureInDeviceList(const String& name)
{
    std::uint32_t offset = 0;
    while (true) {
        const cardputer::WorkspaceFilesPageResult page = cardputer::listWorkspaceFilesPage(
            offset, 32);
        if (!page.success) {
            return {false, page.error};
        }
        for (const cardputer::WorkspaceFile& file : page.files) {
            if (file.name == name && !file.directory) {
                return {true, ""};
            }
        }
        if (page.eof) {
            return {false, "P2 Unicode path was not found in the device workspace list"};
        }
        if (page.nextOffset <= offset) {
            return {false, "P2 Unicode device pagination did not advance"};
        }
        offset = page.nextOffset;
    }
}

void runP2UnicodeSetup(const String& nonce)
{
    const String name = p2UnicodeFileName(nonce);
    const String target = cardputer::workspaceFilePath(name);
    cardputer::OperationResult result = {true, ""};
    P2UnicodeFileDigestResult digest = {false, 0, 0, ""};
    P2UnicodeOwnershipResult ownership = {
        true, "pending", false, 0, 0, false, true, "", ""};
    bool ownershipCreated = false;
    bool deviceListPassed = false;
    bool deviceViewPassed = false;
    if (!isP2UnicodeNonce(nonce)) {
        result = {false, "Nonce must contain 1-20 decimal digits"};
    } else if (!fileWorkspaceReady) {
        result = {false, "microSD workspace is required for P2 Unicode setup"};
    } else if (SD.exists(target) || SD.exists(target + ".sftp-part") ||
               SD.exists(target + ".tmp") || SD.exists(target + ".bak") ||
               SD.exists(cardputer::workspaceFilePath(p2UnicodeOwnershipName(nonce))) ||
               SD.exists(cardputer::workspaceFilePath(p2UnicodeOwnershipName(nonce)) + ".tmp") ||
               SD.exists(cardputer::workspaceFilePath(p2UnicodeOwnershipName(nonce)) + ".bak") ||
               SD.exists(cardputer::workspaceFilePath(p2UnicodeLeafDirectory(nonce))) ||
               SD.exists(cardputer::workspaceFilePath(p2UnicodeFirstDirectory(nonce))) ||
               SD.exists(cardputer::workspaceFilePath(p2UnicodeTopDirectory(nonce)))) {
        result = {false, "P2 Unicode fixture paths already exist"};
    }
    if (result.success) {
        result = writeP2UnicodeOwnership(nonce, ownership);
        ownershipCreated = result.success;
    }
    if (result.success) {
        ensureNetworkReady();
        if (WiFi.status() != WL_CONNECTED) {
            result = {false, "Wi-Fi is required for the public SFTP fixture"};
        }
    }
    if (result.success) {
        result = downloadP2UnicodeFixture(nonce, name, ownership);
    }
    if (result.success) {
        digest = digestP2UnicodeFile(name);
        if (!digest.success) {
            result = {false, digest.error};
        }
    }
    if (result.success) {
        ownership.state = "complete";
        ownership.completed = true;
        ownership.bytes = digest.bytes;
        ownership.fnv32 = digest.fnv32;
        result = writeP2UnicodeOwnership(nonce, ownership);
    }
    if (result.success) {
        const cardputer::OperationResult listed = findP2UnicodeFixtureInDeviceList(name);
        deviceListPassed = listed.success;
        if (!listed.success) {
            result = listed;
        }
    }
    if (result.success) {
        const std::vector<cardputer::WorkspaceFile> savedWorkspaceFiles = workspaceFiles;
        const std::size_t savedWorkspaceFileIndex = workspaceFileIndex;
        const std::uint32_t savedWorkspacePageOffset = workspacePageOffset;
        const std::uint32_t savedWorkspaceNextPageOffset = workspaceNextPageOffset;
        const bool savedWorkspacePageEof = workspacePageEof;
        const WorkspaceListMode savedWorkspaceListMode = workspaceListMode;
        const Screen savedWorkspaceReturnScreen = workspaceListReturnScreen;
        const String savedViewerName = fileViewerName;
        const std::string savedViewerContent = fileViewerContent;
        const std::vector<std::string> savedViewerLines = fileViewerLines;
        const std::string savedLastFileFindQuery = lastFileFindQuery;
        const std::uint32_t savedLastFileFindOffset = lastFileFindOffset;
        const std::vector<std::uint32_t> savedViewerPreviousOffsets =
            fileViewerPreviousOffsets;
        const cardputer::DocumentReaderMode savedReaderMode = fileReaderMode;
        const std::size_t savedViewerFirstLine = fileViewerFirstLine;
        const std::uint32_t savedViewerChunkOffset = fileViewerChunkOffset;
        const std::uint32_t savedViewerNextOffset = fileViewerNextOffset;
        const std::uint32_t savedViewerTotalBytes = fileViewerTotalBytes;
        const bool savedViewerEof = fileViewerEof;
        const std::size_t savedFileActionsIndex = fileActionsIndex;
        cardputer::OperationResult selected = selectWorkspaceFileByName(name);
        if (selected.success) {
            openSelectedWorkspaceFile();
            const cardputer::WorkspaceChunkResult expected =
                cardputer::readWorkspaceFileChunk(name, 0, kFileViewerChunkBytes);
            deviceViewPassed = currentScreen == Screen::FileActions &&
                fileViewerName == name && expected.success && !expected.content.empty() &&
                fileViewerContent == expected.content &&
                fileViewerChunkOffset == expected.offset &&
                fileViewerNextOffset == expected.nextOffset &&
                fileViewerTotalBytes == expected.totalBytes &&
                fileViewerTotalBytes == digest.bytes && fileViewerEof == expected.eof;
            if (!deviceViewPassed) {
                selected = {false, "P2 Unicode device viewer did not open the exact path"};
            }
        }
        workspaceFiles = savedWorkspaceFiles;
        workspaceFileIndex = savedWorkspaceFileIndex;
        workspacePageOffset = savedWorkspacePageOffset;
        workspaceNextPageOffset = savedWorkspaceNextPageOffset;
        workspacePageEof = savedWorkspacePageEof;
        workspaceListMode = savedWorkspaceListMode;
        workspaceListReturnScreen = savedWorkspaceReturnScreen;
        fileViewerName = savedViewerName;
        fileViewerContent = savedViewerContent;
        fileViewerLines = savedViewerLines;
        lastFileFindQuery = savedLastFileFindQuery;
        lastFileFindOffset = savedLastFileFindOffset;
        fileViewerPreviousOffsets = savedViewerPreviousOffsets;
        fileReaderMode = savedReaderMode;
        fileViewerFirstLine = savedViewerFirstLine;
        fileViewerChunkOffset = savedViewerChunkOffset;
        fileViewerNextOffset = savedViewerNextOffset;
        fileViewerTotalBytes = savedViewerTotalBytes;
        fileViewerEof = savedViewerEof;
        fileActionsIndex = savedFileActionsIndex;
        result = selected;
    }
    currentScreen = Screen::MainCarousel;
    menuStatus = "";
    renderCarousel();
    if (!result.success && ownershipCreated) {
        const P2UnicodeCleanupResult cleanup = cleanupP2UnicodeFixture(nonce);
        if (!cleanup.success) {
            result.error += "; safe cleanup also failed: " + cleanup.error;
        }
    }
    const String safeError = result.success
        ? String("none") : serialSafeError(result.error, 180);
    Serial.printf(
        "P2UNICODESETUP result=%s nonce=%s path_bytes=%u depth=4 bytes=%u fnv32=%08x device_list=%s device_view=%s error=%s\n",
        result.success ? "pass" : "failed", nonce.c_str(),
        static_cast<unsigned int>(name.length()),
        static_cast<unsigned int>(digest.bytes),
        static_cast<unsigned int>(digest.fnv32),
        deviceListPassed ? "pass" : "failed",
        deviceViewPassed ? "pass" : "failed", safeError.c_str());
}

void runP2UnicodeCleanup(const String& nonce)
{
    P2UnicodeCleanupResult cleanup = {false, false, 0, 0, ""};
    if (!isP2UnicodeNonce(nonce)) {
        cleanup.error = "Nonce must contain 1-20 decimal digits";
    } else if (!fileWorkspaceReady) {
        cleanup.error = "microSD workspace is required for P2 Unicode cleanup";
    } else {
        cleanup = cleanupP2UnicodeFixture(nonce);
    }
    currentScreen = Screen::MainCarousel;
    menuStatus = "";
    renderCarousel();
    const bool remaining = isP2UnicodeNonce(nonce) &&
        !p2UnicodeFixtureIsEntirelyAbsent(nonce);
    const String safeError = cleanup.success
        ? String("none") : serialSafeError(cleanup.error, 180);
    Serial.printf(
        "P2UNICODECLEAN result=%s nonce=%s already_absent=%s removed_files=%u removed_dirs=%u remaining=%u errors=%u error=%s\n",
        cleanup.success ? "pass" : "failed", nonce.c_str(),
        cleanup.alreadyAbsent ? "yes" : "no",
        static_cast<unsigned int>(cleanup.removedFiles),
        static_cast<unsigned int>(cleanup.removedDirectories),
        remaining ? 1U : 0U, cleanup.success ? 0U : 1U, safeError.c_str());
}

constexpr std::uint32_t kP2LargeFixtureBytes = 335544320U;
constexpr std::size_t kP2LargeBlockBytes = 4096;
constexpr std::uint32_t kP2LargeExpectedFnv32 = 0x09529dc5U;
constexpr std::uint32_t kP2LargeAboveBoundaryOffset = 268435520U;
constexpr std::uint32_t kP2LargeSearchStartOffset = 268435457U;
constexpr std::uint32_t kP2LargeExpectedSearchOffset = 268439552U;
constexpr char kP2LargeMarker[] = "CARDMIND_P2_21_BLOCK";
constexpr std::size_t kP2LargeMarkerBytes = sizeof(kP2LargeMarker) - 1;

struct P2LargeOwnershipResult {
    bool success;
    String state;
    std::uint32_t bytes;
    std::uint32_t fnv32;
    String error;
};

struct P2LargeCleanupResult {
    bool success;
    bool alreadyAbsent;
    bool removedFile;
    bool removedDirectory;
    String error;
};

String p2LargeRootName(const String& nonce)
{
    return "cardmind_p2_21_" + nonce;
}

String p2LargeFileName(const String& nonce)
{
    return p2LargeRootName(nonce) + "/large-320mib.txt";
}

String p2LargeOwnerPath(const String& nonce)
{
    return "/assistant/.cardmind-p2-21-" + nonce + "-owner.json";
}

cardputer::OperationResult writeP2LargeOwnership(const String& nonce,
                                                  const String& state,
                                                  std::uint32_t bytes,
                                                  std::uint32_t fnv32)
{
    JsonDocument document;
    document["version"] = 1;
    document["nonce"] = nonce;
    document["path"] = p2LargeFileName(nonce);
    document["state"] = state;
    document["bytes"] = bytes;
    document["fnv32"] = fnv32;
    return cardputer::writeAtomicJsonSdFile(p2LargeOwnerPath(nonce), document);
}

P2LargeOwnershipResult loadP2LargeOwnership(const String& nonce)
{
    const String ownerPath = p2LargeOwnerPath(nonce);
    const cardputer::OperationResult recovered = cardputer::recoverAtomicSdFile(ownerPath);
    if (!recovered.success) {
        return {false, "", 0, 0, recovered.error};
    }
    File file = SD.open(ownerPath, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return {false, "", 0, 0, "P2 large ownership metadata is missing"};
    }
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, file);
    file.close();
    if (error || !document["version"].is<std::uint32_t>() ||
        document["version"].as<std::uint32_t>() != 1 ||
        !document["nonce"].is<const char*>() ||
        String(document["nonce"].as<const char*>()) != nonce ||
        !document["path"].is<const char*>() ||
        String(document["path"].as<const char*>()) != p2LargeFileName(nonce) ||
        !document["state"].is<const char*>() ||
        !document["bytes"].is<std::uint32_t>() ||
        !document["fnv32"].is<std::uint32_t>()) {
        return {false, "", 0, 0,
                "P2 large ownership metadata is invalid or belongs to another run"};
    }
    const String state = document["state"].as<const char*>();
    const std::uint32_t bytes = document["bytes"].as<std::uint32_t>();
    const std::uint32_t fnv32 = document["fnv32"].as<std::uint32_t>();
    const bool pending = state == "pending" && bytes == 0 && fnv32 == 0;
    const bool owned = (state == "complete" || state == "cleaning") &&
        bytes == kP2LargeFixtureBytes && fnv32 == kP2LargeExpectedFnv32;
    return pending || owned
        ? P2LargeOwnershipResult{true, state, bytes, fnv32, ""}
        : P2LargeOwnershipResult{false, "", 0, 0,
                                 "P2 large ownership state is inconsistent"};
}

cardputer::OperationResult validateP2LargeOwnedTree(const String& nonce,
                                                     const String& state)
{
    const String rootPath = cardputer::workspaceFilePath(p2LargeRootName(nonce));
    if (!SD.exists(rootPath)) {
        return state == "complete"
            ? cardputer::OperationResult{false, "Completed P2 large fixture is missing"}
            : cardputer::OperationResult{true, ""};
    }
    File directory = SD.open(rootPath, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) {
            directory.close();
        }
        return {false, "P2 large owned root is not a directory"};
    }
    const String expectedPath = cardputer::workspaceFilePath(p2LargeFileName(nonce));
    bool foundFile = false;
    File entry = directory.openNextFile();
    while (entry) {
        const bool expected = !entry.isDirectory() && expectedPath == entry.path() &&
            !foundFile;
        if (expected) {
            foundFile = true;
        }
        entry.close();
        if (!expected) {
            directory.close();
            return {false, "P2 large owned root contains unknown data; preserved"};
        }
        entry = directory.openNextFile();
    }
    directory.close();
    if (state == "complete" && !foundFile) {
        return {false, "Completed P2 large fixture file is missing"};
    }
    if (foundFile) {
        File file = SD.open(expectedPath, FILE_READ);
        if (!file || file.isDirectory()) {
            if (file) {
                file.close();
            }
            return {false, "P2 large fixture is not a readable file"};
        }
        const std::size_t bytes = file.size();
        file.close();
        if (state == "complete" && bytes != kP2LargeFixtureBytes) {
            return {false, "P2 large fixture size differs from its ownership record"};
        }
        if (state == "pending" && bytes > kP2LargeFixtureBytes) {
            return {false, "Pending P2 large fixture exceeds its owned size"};
        }
    }
    return {true, ""};
}

P2LargeCleanupResult cleanupP2LargeFixture(const String& nonce)
{
    const String rootPath = cardputer::workspaceFilePath(p2LargeRootName(nonce));
    const String filePath = cardputer::workspaceFilePath(p2LargeFileName(nonce));
    const String ownerPath = p2LargeOwnerPath(nonce);
    const bool hasOwner = SD.exists(ownerPath) || SD.exists(ownerPath + ".tmp") ||
        SD.exists(ownerPath + ".bak");
    if (!hasOwner && !SD.exists(rootPath)) {
        return {true, true, false, false, ""};
    }
    if (!hasOwner) {
        return {false, false, false, false,
                "Ownerless P2 large data was found and preserved"};
    }
    P2LargeOwnershipResult ownership = loadP2LargeOwnership(nonce);
    if (!ownership.success) {
        return {false, false, false, false, ownership.error};
    }
    cardputer::OperationResult result = validateP2LargeOwnedTree(nonce, ownership.state);
    if (!result.success) {
        return {false, false, false, false, result.error};
    }
    result = writeP2LargeOwnership(
        nonce, "cleaning", kP2LargeFixtureBytes, kP2LargeExpectedFnv32);
    if (!result.success) {
        return {false, false, false, false, result.error};
    }
    bool removedFile = false;
    if (SD.exists(filePath)) {
        if (!SD.remove(filePath)) {
            return {false, false, false, false,
                    "Failed to remove the owned P2 large fixture file"};
        }
        removedFile = true;
    }
    bool removedDirectory = false;
    if (SD.exists(rootPath)) {
        if (!SD.rmdir(rootPath)) {
            return {false, false, removedFile, false,
                    "Failed to remove the empty P2 large fixture directory"};
        }
        removedDirectory = true;
    }
    if (!SD.exists(ownerPath) || !SD.remove(ownerPath)) {
        return {false, false, removedFile, removedDirectory,
                "Failed to remove P2 large ownership metadata"};
    }
    return !SD.exists(rootPath) && !SD.exists(ownerPath)
        ? P2LargeCleanupResult{true, false, removedFile, removedDirectory, ""}
        : P2LargeCleanupResult{false, false, removedFile, removedDirectory,
                               "P2 large owned data remains after cleanup"};
}

void fillP2LargeBlock(std::uint8_t* block)
{
    for (std::size_t index = 0; index < kP2LargeBlockBytes; ++index) {
        block[index] = static_cast<std::uint8_t>('x');
    }
    for (std::size_t index = 0; index < kP2LargeMarkerBytes; ++index) {
        block[index] = static_cast<std::uint8_t>(kP2LargeMarker[index]);
    }
}

bool p2LargeChunkMatches(const cardputer::WorkspaceChunkResult& chunk,
                         std::uint32_t expectedOffset,
                         std::size_t expectedBytes,
                         bool expectedEof)
{
    if (!chunk.success || chunk.offset != expectedOffset ||
        chunk.content.size() != expectedBytes || chunk.eof != expectedEof ||
        chunk.totalBytes != kP2LargeFixtureBytes) {
        return false;
    }
    for (std::size_t index = 0; index < chunk.content.size(); ++index) {
        const std::uint32_t blockOffset =
            (expectedOffset + static_cast<std::uint32_t>(index)) % kP2LargeBlockBytes;
        const char expected = blockOffset < kP2LargeMarkerBytes
            ? kP2LargeMarker[blockOffset] : 'x';
        if (chunk.content[index] != expected) {
            return false;
        }
    }
    return true;
}

void runP2LargeSetup(const String& nonce)
{
    const String name = p2LargeFileName(nonce);
    const std::uint32_t freeHeapBefore = ESP.getFreeHeap();
    const std::uint32_t largestHeapBefore =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const std::uint32_t minimumHeapBefore = ESP.getMinFreeHeap();
    cardputer::OperationResult result = {true, ""};
    bool ownershipCreated = false;
    if (!isP2UnicodeNonce(nonce)) {
        result = {false, "Nonce must contain 1-20 decimal digits"};
    } else if (!fileWorkspaceReady) {
        result = {false, "microSD workspace is required for P2 large setup"};
    } else {
        const String rootPath = cardputer::workspaceFilePath(p2LargeRootName(nonce));
        const String ownerPath = p2LargeOwnerPath(nonce);
        if (SD.exists(rootPath) || SD.exists(ownerPath) || SD.exists(ownerPath + ".tmp") ||
            SD.exists(ownerPath + ".bak")) {
            const P2LargeCleanupResult cleanup = cleanupP2LargeFixture(nonce);
            if (!cleanup.success) {
                result = {false, cleanup.error};
            }
        }
    }
    if (result.success) {
        result = cardputer::checkSdOperationSpace(
            kP2LargeFixtureBytes, cardputer::kStorageOperationalFloorBytes);
    }
    if (result.success) {
        result = writeP2LargeOwnership(nonce, "pending", 0, 0);
        ownershipCreated = result.success;
    }
    if (result.success) {
        result = cardputer::ensureWorkspaceFileParent(name);
    }
    File output;
    if (result.success) {
        output = SD.open(cardputer::workspaceFilePath(name), FILE_WRITE);
        if (!output) {
            result = {false, "Failed to create the P2 large fixture"};
        }
    }
    std::uint8_t block[kP2LargeBlockBytes] = {};
    fillP2LargeBlock(block);
    std::uint32_t writtenBytes = 0;
    while (result.success && writtenBytes < kP2LargeFixtureBytes) {
        const std::size_t written = output.write(block, sizeof(block));
        if (written != sizeof(block)) {
            result = {false, "microSD rejected a complete P2 large fixture block"};
            break;
        }
        writtenBytes += static_cast<std::uint32_t>(written);
        if ((writtenBytes % (32U * 1024U * 1024U)) == 0) {
            Serial.printf("P2LARGESETUP progress nonce=%s bytes=%u\n", nonce.c_str(),
                          static_cast<unsigned int>(writtenBytes));
        }
        if ((writtenBytes % (64U * 1024U)) == 0) {
            delay(0);
        }
    }
    if (output) {
        output.flush();
        output.close();
    }
    if (result.success) {
        File check = SD.open(cardputer::workspaceFilePath(name), FILE_READ);
        const bool exact = check && !check.isDirectory() &&
            check.size() == kP2LargeFixtureBytes;
        if (check) {
            check.close();
        }
        result = exact
            ? writeP2LargeOwnership(
                nonce, "complete", kP2LargeFixtureBytes, kP2LargeExpectedFnv32)
            : cardputer::OperationResult{
                false, "P2 large fixture size differs after the streamed write"};
    }
    if (!result.success && ownershipCreated) {
        const P2LargeCleanupResult cleanup = cleanupP2LargeFixture(nonce);
        if (!cleanup.success) {
            result.error += "; safe cleanup also failed: " + cleanup.error;
        }
    }
    const std::uint32_t freeHeapAfter = ESP.getFreeHeap();
    const std::uint32_t largestHeapAfter =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const std::uint32_t minimumHeapAfter = ESP.getMinFreeHeap();
    const String safeError = result.success
        ? String("none") : serialSafeError(result.error, 180);
    Serial.printf(
        "P2LARGESETUP result=%s nonce=%s path=%s bytes=%u fnv32=%08x free_heap_before=%u free_heap_after=%u largest_heap_before=%u largest_heap_after=%u min_heap_before=%u min_heap_after=%u error=%s\n",
        result.success ? "pass" : "failed", nonce.c_str(), name.c_str(),
        static_cast<unsigned int>(writtenBytes),
        static_cast<unsigned int>(kP2LargeExpectedFnv32),
        static_cast<unsigned int>(freeHeapBefore),
        static_cast<unsigned int>(freeHeapAfter),
        static_cast<unsigned int>(largestHeapBefore),
        static_cast<unsigned int>(largestHeapAfter),
        static_cast<unsigned int>(minimumHeapBefore),
        static_cast<unsigned int>(minimumHeapAfter), safeError.c_str());
}

void runP2LargeVerify(const String& nonce)
{
    const String name = p2LargeFileName(nonce);
    const std::uint32_t freeHeapBefore = ESP.getFreeHeap();
    const std::uint32_t largestHeapBefore =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const std::uint32_t minimumHeapBefore = ESP.getMinFreeHeap();
    cardputer::OperationResult result = {true, ""};
    bool listPassed = false;
    bool read0Passed = false;
    bool readAbovePassed = false;
    bool readEofPassed = false;
    bool searchPassed = false;
    std::uint32_t searchOffset = 0;
    if (!isP2UnicodeNonce(nonce)) {
        result = {false, "Nonce must contain 1-20 decimal digits"};
    } else if (!fileWorkspaceReady) {
        result = {false, "microSD workspace is required for P2 large verification"};
    }
    P2LargeOwnershipResult ownership = {false, "", 0, 0, ""};
    if (result.success) {
        ownership = loadP2LargeOwnership(nonce);
        if (!ownership.success || ownership.state != "complete") {
            result = {false, ownership.success
                ? String("P2 large ownership is not complete") : ownership.error};
        }
    }
    std::uint32_t offset = 0;
    bool eof = false;
    while (result.success && !eof && !listPassed) {
        const cardputer::WorkspaceFilesPageResult page =
            cardputer::listWorkspaceFilesPage(offset, 64);
        if (!page.success) {
            result = {false, page.error};
            break;
        }
        for (const cardputer::WorkspaceFile& file : page.files) {
            if (file.name == name) {
                listPassed = !file.directory && file.size == kP2LargeFixtureBytes;
                if (!listPassed) {
                    result = {false, "Production workspace listing returned the wrong size"};
                }
                break;
            }
        }
        if (!page.eof && page.nextOffset <= offset) {
            result = {false, "Production workspace pagination did not advance"};
            break;
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    if (result.success && !listPassed) {
        result = {false, "Production workspace listing omitted the P2 large fixture"};
    }
    if (result.success) {
        read0Passed = p2LargeChunkMatches(
            cardputer::readWorkspaceFileChunk(name, 0, 64), 0, 64, false);
        readAbovePassed = p2LargeChunkMatches(
            cardputer::readWorkspaceFileChunk(name, kP2LargeAboveBoundaryOffset, 64),
            kP2LargeAboveBoundaryOffset, 64, false);
        readEofPassed = p2LargeChunkMatches(
            cardputer::readWorkspaceFileChunk(name, kP2LargeFixtureBytes, 64),
            kP2LargeFixtureBytes, 0, true);
        if (!read0Passed || !readAbovePassed || !readEofPassed) {
            result = {false, "Production workspace bounded reads did not match the fixture"};
        }
    }
    if (result.success) {
        const cardputer::WorkspaceFindResult found = cardputer::findWorkspaceText(
            name, kP2LargeMarker, kP2LargeSearchStartOffset);
        searchOffset = found.offset;
        searchPassed = found.success && found.found &&
            found.offset == kP2LargeExpectedSearchOffset;
        if (!searchPassed) {
            result = {false, found.success
                ? String("Production workspace search returned the wrong offset")
                : found.error};
        }
    }
    const std::uint32_t freeHeapAfter = ESP.getFreeHeap();
    const std::uint32_t largestHeapAfter =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const std::uint32_t minimumHeapAfter = ESP.getMinFreeHeap();
    const String safeError = result.success
        ? String("none") : serialSafeError(result.error, 180);
    Serial.printf(
        "P2LARGEVERIFY result=%s nonce=%s path=%s bytes=%u list=%s read0=%s read_above=%s read_eof=%s search=%s search_offset=%u free_heap_before=%u free_heap_after=%u largest_heap_before=%u largest_heap_after=%u min_heap_before=%u min_heap_after=%u error=%s\n",
        result.success ? "pass" : "failed", nonce.c_str(), name.c_str(),
        static_cast<unsigned int>(ownership.bytes), listPassed ? "pass" : "failed",
        read0Passed ? "pass" : "failed", readAbovePassed ? "pass" : "failed",
        readEofPassed ? "pass" : "failed", searchPassed ? "pass" : "failed",
        static_cast<unsigned int>(searchOffset),
        static_cast<unsigned int>(freeHeapBefore),
        static_cast<unsigned int>(freeHeapAfter),
        static_cast<unsigned int>(largestHeapBefore),
        static_cast<unsigned int>(largestHeapAfter),
        static_cast<unsigned int>(minimumHeapBefore),
        static_cast<unsigned int>(minimumHeapAfter), safeError.c_str());
}

void runP2LargeCleanup(const String& nonce)
{
    P2LargeCleanupResult cleanup = {false, false, false, false, ""};
    if (!isP2UnicodeNonce(nonce)) {
        cleanup.error = "Nonce must contain 1-20 decimal digits";
    } else if (!fileWorkspaceReady) {
        cleanup.error = "microSD workspace is required for P2 large cleanup";
    } else {
        cleanup = cleanupP2LargeFixture(nonce);
    }
    const String safeError = cleanup.success
        ? String("none") : serialSafeError(cleanup.error, 180);
    Serial.printf(
        "P2LARGECLEAN result=%s nonce=%s already_absent=%s removed_file=%s removed_directory=%s error=%s\n",
        cleanup.success ? "pass" : "failed", nonce.c_str(),
        cleanup.alreadyAbsent ? "yes" : "no",
        cleanup.removedFile ? "yes" : "no",
        cleanup.removedDirectory ? "yes" : "no", safeError.c_str());
}

constexpr const char* kP2AtomicContent = "CARDMIND_P2_22_ORIGINAL\n";
constexpr std::uint32_t kP2AtomicBytes = 24;
constexpr std::uint32_t kP2AtomicFnv32 = 0x9e5f863bU;

String p2AtomicRootName(const String& nonce)
{
    return "cardmind_p2_22_" + nonce;
}

String p2AtomicFileName(const String& nonce)
{
    return p2AtomicRootName(nonce) + "/atomic.txt";
}

String p2AtomicOwnerPath(const String& nonce)
{
    return "/assistant/.cardmind-p2-22-" + nonce + "-owner.json";
}

cardputer::OperationResult writeP2AtomicOwnership(const String& nonce,
                                                   const String& state)
{
    JsonDocument document;
    document["version"] = 1;
    document["nonce"] = nonce;
    document["path"] = p2AtomicFileName(nonce);
    document["state"] = state;
    document["bytes"] = kP2AtomicBytes;
    document["fnv32"] = kP2AtomicFnv32;
    document["backup_directory"] = p2AtomicFileName(nonce) + ".bak";
    return cardputer::writeAtomicJsonSdFile(p2AtomicOwnerPath(nonce), document);
}

cardputer::OperationResult loadP2AtomicOwnership(const String& nonce,
                                                  String& state)
{
    const String ownerPath = p2AtomicOwnerPath(nonce);
    cardputer::OperationResult result = cardputer::recoverAtomicSdFile(ownerPath);
    if (!result.success) {
        return result;
    }
    File file = SD.open(ownerPath, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return {false, "P2 atomic ownership metadata is missing"};
    }
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, file);
    file.close();
    if (error || !document["version"].is<std::uint32_t>() ||
        document["version"].as<std::uint32_t>() != 1 ||
        !document["nonce"].is<const char*>() ||
        String(document["nonce"].as<const char*>()) != nonce ||
        !document["path"].is<const char*>() ||
        String(document["path"].as<const char*>()) != p2AtomicFileName(nonce) ||
        !document["state"].is<const char*>() ||
        !document["bytes"].is<std::uint32_t>() ||
        document["bytes"].as<std::uint32_t>() != kP2AtomicBytes ||
        !document["fnv32"].is<std::uint32_t>() ||
        document["fnv32"].as<std::uint32_t>() != kP2AtomicFnv32 ||
        !document["backup_directory"].is<const char*>() ||
        String(document["backup_directory"].as<const char*>()) !=
            p2AtomicFileName(nonce) + ".bak") {
        return {false, "P2 atomic ownership metadata is invalid or belongs to another run"};
    }
    state = document["state"].as<const char*>();
    return state == "pending" || state == "complete" || state == "cleaning"
        ? cardputer::OperationResult{true, ""}
        : cardputer::OperationResult{false, "P2 atomic ownership state is invalid"};
}

cardputer::OperationResult digestP2AtomicFile(const String& nonce,
                                               std::uint32_t& bytes,
                                               std::uint32_t& fnv32)
{
    File file = SD.open(cardputer::workspaceFilePath(p2AtomicFileName(nonce)), FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return {false, "P2 atomic fixture is not a readable file"};
    }
    if (file.size() > kP2AtomicBytes) {
        file.close();
        return {false, "P2 atomic fixture is larger than its ownership record"};
    }
    bytes = 0;
    fnv32 = 2166136261U;
    while (bytes < kP2AtomicBytes) {
        const int value = file.read();
        if (value < 0) {
            break;
        }
        fnv32 ^= static_cast<std::uint8_t>(value);
        fnv32 *= 16777619U;
        ++bytes;
    }
    file.close();
    return bytes == kP2AtomicBytes && fnv32 == kP2AtomicFnv32
        ? cardputer::OperationResult{true, ""}
        : cardputer::OperationResult{
            false, "P2 atomic fixture differs from its ownership record"};
}

cardputer::OperationResult validateP2AtomicOwnedTree(const String& nonce,
                                                      const String& state)
{
    const String rootPath = cardputer::workspaceFilePath(p2AtomicRootName(nonce));
    if (!SD.exists(rootPath)) {
        return state == "complete"
            ? cardputer::OperationResult{false, "Completed P2 atomic fixture is missing"}
            : cardputer::OperationResult{true, ""};
    }
    File directory = SD.open(rootPath, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) {
            directory.close();
        }
        return {false, "P2 atomic owned root is not a directory"};
    }
    const String filePath = cardputer::workspaceFilePath(p2AtomicFileName(nonce));
    const String backupPath = filePath + ".bak";
    bool foundFile = false;
    bool foundBackup = false;
    File entry = directory.openNextFile();
    while (entry) {
        const String path = entry.path();
        const bool expectedFile = path == filePath && !entry.isDirectory() && !foundFile;
        const bool expectedBackup = path == backupPath && entry.isDirectory() && !foundBackup;
        if (expectedFile) {
            foundFile = true;
        } else if (expectedBackup) {
            File child = entry.openNextFile();
            const bool empty = !child;
            if (child) {
                child.close();
            }
            if (!empty) {
                entry.close();
                directory.close();
                return {false, "P2 atomic backup directory contains unknown data; preserved"};
            }
            foundBackup = true;
        } else {
            entry.close();
            directory.close();
            return {false, "P2 atomic owned root contains unknown data; preserved"};
        }
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();
    if (state == "complete" && (!foundFile || !foundBackup)) {
        return {false, "Completed P2 atomic fixture is missing an owned entry"};
    }
    if (state == "complete") {
        std::uint32_t bytes = 0;
        std::uint32_t fnv32 = 0;
        return digestP2AtomicFile(nonce, bytes, fnv32);
    }
    return {true, ""};
}

P2LargeCleanupResult cleanupP2AtomicFixture(const String& nonce)
{
    const String rootPath = cardputer::workspaceFilePath(p2AtomicRootName(nonce));
    const String filePath = cardputer::workspaceFilePath(p2AtomicFileName(nonce));
    const String backupPath = filePath + ".bak";
    const String ownerPath = p2AtomicOwnerPath(nonce);
    const bool hasOwner = SD.exists(ownerPath) || SD.exists(ownerPath + ".tmp") ||
        SD.exists(ownerPath + ".bak");
    if (!hasOwner && !SD.exists(rootPath)) {
        return {true, true, false, false, ""};
    }
    if (!hasOwner) {
        return {false, false, false, false,
                "Ownerless P2 atomic data was found and preserved"};
    }
    String state;
    cardputer::OperationResult result = loadP2AtomicOwnership(nonce, state);
    if (result.success) {
        result = validateP2AtomicOwnedTree(nonce, state);
    }
    if (!result.success) {
        return {false, false, false, false, result.error};
    }
    result = writeP2AtomicOwnership(nonce, "cleaning");
    if (!result.success) {
        return {false, false, false, false, result.error};
    }
    bool removedFile = false;
    if (SD.exists(filePath)) {
        if (!SD.remove(filePath)) {
            return {false, false, false, false, "Failed to remove P2 atomic fixture"};
        }
        removedFile = true;
    }
    if (SD.exists(backupPath) && !SD.rmdir(backupPath)) {
        return {false, false, removedFile, false,
                "Failed to remove the empty P2 atomic backup directory"};
    }
    bool removedDirectory = false;
    if (SD.exists(rootPath)) {
        if (!SD.rmdir(rootPath)) {
            return {false, false, removedFile, false,
                    "Failed to remove the empty P2 atomic root"};
        }
        removedDirectory = true;
    }
    if (!SD.exists(ownerPath) || !SD.remove(ownerPath)) {
        return {false, false, removedFile, removedDirectory,
                "Failed to remove P2 atomic ownership metadata"};
    }
    return {true, false, removedFile, removedDirectory, ""};
}

void runP2AtomicSetup(const String& nonce)
{
    const String name = p2AtomicFileName(nonce);
    cardputer::OperationResult result = {true, ""};
    bool ownershipCreated = false;
    bool preflightPassed = false;
    bool rangePassed = false;
    bool filesystemPassed = false;
    if (!isP2UnicodeNonce(nonce)) {
        result = {false, "Nonce must contain 1-20 decimal digits"};
    } else if (!fileWorkspaceReady) {
        result = {false, "microSD workspace is required for P2 atomic setup"};
    } else {
        const String rootPath = cardputer::workspaceFilePath(p2AtomicRootName(nonce));
        const String ownerPath = p2AtomicOwnerPath(nonce);
        if (SD.exists(rootPath) || SD.exists(ownerPath) || SD.exists(ownerPath + ".tmp") ||
            SD.exists(ownerPath + ".bak")) {
            const P2LargeCleanupResult cleanup = cleanupP2AtomicFixture(nonce);
            if (!cleanup.success) {
                result = {false, cleanup.error};
            }
        }
    }
    if (result.success) {
        result = writeP2AtomicOwnership(nonce, "pending");
        ownershipCreated = result.success;
    }
    if (result.success) {
        result = cardputer::ensureWorkspaceFileParent(name);
    }
    if (result.success) {
        File file = SD.open(cardputer::workspaceFilePath(name), FILE_WRITE);
        if (!file) {
            result = {false, "Failed to create P2 atomic fixture"};
        } else {
            const std::size_t written = file.print(kP2AtomicContent);
            file.flush();
            file.close();
            if (written != kP2AtomicBytes) {
                result = {false, "Failed to write complete P2 atomic fixture"};
            }
        }
    }
    std::uint32_t beforeBytes = 0;
    std::uint32_t beforeFnv32 = 0;
    if (result.success) {
        result = digestP2AtomicFile(nonce, beforeBytes, beforeFnv32);
    }
    if (result.success) {
        const std::uint64_t totalBytes = SD.totalBytes();
        const std::uint64_t usedBytes = SD.usedBytes();
        if (totalBytes == 0 || usedBytes > totalBytes) {
            result = {false, "microSD free space is unavailable for P2 atomic preflight"};
        } else {
            const std::uint64_t freeBytes = totalBytes - usedBytes;
            const cardputer::OperationResult rejected = cardputer::checkSdOperationSpace(
                freeBytes + 1, cardputer::kStorageOperationalFloorBytes);
            std::uint32_t bytes = 0;
            std::uint32_t fnv32 = 0;
            const cardputer::OperationResult unchanged =
                digestP2AtomicFile(nonce, bytes, fnv32);
            preflightPassed = !rejected.success && unchanged.success &&
                bytes == beforeBytes && fnv32 == beforeFnv32;
            if (!preflightPassed) {
                result = {false, "Low-space preflight did not reject unchanged"};
            }
        }
    }
    if (result.success) {
        const cardputer::OperationResult rejected = cardputer::replaceWorkspaceFileRange(
            name, std::numeric_limits<std::uint32_t>::max() - 2U, 16U, "X");
        std::uint32_t bytes = 0;
        std::uint32_t fnv32 = 0;
        const cardputer::OperationResult unchanged = digestP2AtomicFile(nonce, bytes, fnv32);
        rangePassed = !rejected.success && unchanged.success &&
            bytes == beforeBytes && fnv32 == beforeFnv32;
        if (!rangePassed) {
            result = {false, "Out-of-range edit did not reject unchanged"};
        }
    }
    const String backupPath = cardputer::workspaceFilePath(name) + ".bak";
    if (result.success && !SD.mkdir(backupPath)) {
        result = {false, "Failed to create the owned P2 atomic backup directory"};
    }
    if (result.success) {
        const cardputer::OperationResult rejected = cardputer::replaceWorkspaceFileRange(
            name, 0, 0, "X");
        std::uint32_t bytes = 0;
        std::uint32_t fnv32 = 0;
        const cardputer::OperationResult unchanged = digestP2AtomicFile(nonce, bytes, fnv32);
        filesystemPassed = !rejected.success && unchanged.success &&
            bytes == beforeBytes && fnv32 == beforeFnv32 && SD.exists(backupPath);
        if (!filesystemPassed) {
            result = {false, "Filesystem replacement failure did not preserve the source"};
        }
    }
    if (result.success) {
        result = writeP2AtomicOwnership(nonce, "complete");
    }
    if (!result.success && ownershipCreated) {
        const P2LargeCleanupResult cleanup = cleanupP2AtomicFixture(nonce);
        if (!cleanup.success) {
            result.error += "; safe cleanup also failed: " + cleanup.error;
        }
    }
    const String safeError = result.success
        ? String("none") : serialSafeError(result.error, 180);
    Serial.printf(
        "P2ATOMICSETUP result=%s nonce=%s path=%s bytes=%u fnv32=%08x preflight=%s range=%s filesystem=%s backup=%s error=%s\n",
        result.success ? "pass" : "failed", nonce.c_str(), name.c_str(),
        static_cast<unsigned int>(beforeBytes), static_cast<unsigned int>(beforeFnv32),
        preflightPassed ? "pass" : "failed", rangePassed ? "pass" : "failed",
        filesystemPassed ? "pass" : "failed", SD.exists(backupPath) ? "present" : "absent",
        safeError.c_str());
}

void runP2AtomicCleanup(const String& nonce)
{
    P2LargeCleanupResult cleanup = {false, false, false, false, ""};
    if (!isP2UnicodeNonce(nonce)) {
        cleanup.error = "Nonce must contain 1-20 decimal digits";
    } else if (!fileWorkspaceReady) {
        cleanup.error = "microSD workspace is required for P2 atomic cleanup";
    } else {
        cleanup = cleanupP2AtomicFixture(nonce);
    }
    const String safeError = cleanup.success
        ? String("none") : serialSafeError(cleanup.error, 180);
    Serial.printf(
        "P2ATOMICCLEAN result=%s nonce=%s already_absent=%s removed_file=%s removed_directory=%s error=%s\n",
        cleanup.success ? "pass" : "failed", nonce.c_str(),
        cleanup.alreadyAbsent ? "yes" : "no", cleanup.removedFile ? "yes" : "no",
        cleanup.removedDirectory ? "yes" : "no", safeError.c_str());
}

void runProjectSchemaTest()
{
    constexpr std::size_t kTestChatCount = cardputer::kMaximumProjectPageEntries + 1;
    const String sharedName = "firmware_project_schema_test.txt";
    const String sharedPath = cardputer::workspaceFilePath(sharedName);
    if (SD.exists(sharedPath)) {
        SD.remove(sharedPath);
    }
    cardputer::OperationResult result = cardputer::validateCommittedProjectStorage();
    const cardputer::ProjectDocumentResult created = result.success
        ? cardputer::createProject("Schema pagination test")
        : cardputer::ProjectDocumentResult{false, {}, result.error};
    String projectId = created.success ? created.project.summary.id : String();
    String activeChatId;
    for (std::size_t index = 0; result.success && index < kTestChatCount; ++index) {
        const cardputer::ChatDocumentResult chat = cardputer::createProjectChat(
            projectId, "Schema chat " + String(index + 1),
            cardputer::defaultNewChatToolPermissionPolicy());
        if (!chat.success) {
            result = {false, chat.error};
            break;
        }
        activeChatId = chat.chat.summary.id;
    }
    if (result.success) {
        cardputer::ProjectDocumentResult project = cardputer::loadProject(projectId);
        if (!project.success) {
            result = {false, project.error};
        } else {
            project.project.activeChatId = activeChatId;
            result = cardputer::saveProject(project.project);
        }
    }
    if (result.success) {
        result = cardputer::createWorkspaceFile(sharedName);
    }
    if (result.success) {
        result = cardputer::linkSharedFileToProject(projectId, sharedName);
    }
    const cardputer::ProjectChatsPageResult firstPage = result.success
        ? cardputer::listProjectChatsPage(
              projectId, 0, cardputer::kMaximumProjectPageEntries)
        : cardputer::ProjectChatsPageResult{false, {}, 0, false, result.error};
    const cardputer::ProjectChatsPageResult secondPage = firstPage.success && !firstPage.eof
        ? cardputer::listProjectChatsPage(
              projectId, firstPage.nextOffset, cardputer::kMaximumProjectPageEntries)
        : cardputer::ProjectChatsPageResult{false, {}, firstPage.nextOffset, false,
                                            firstPage.error};
    const cardputer::ProjectDocumentResult stored = secondPage.success
        ? cardputer::loadProject(projectId)
        : cardputer::ProjectDocumentResult{false, {}, secondPage.error};
    if (result.success && (!firstPage.success || firstPage.eof ||
                           firstPage.chats.size() != cardputer::kMaximumProjectPageEntries ||
                           !secondPage.success || !secondPage.eof ||
                           secondPage.chats.size() != 1 || !stored.success ||
                           stored.project.summary.chatCount != kTestChatCount ||
                           stored.project.chatIndexRevision != kTestChatCount ||
                           stored.project.sharedLinksRevision != 1)) {
        result = {false, "Project pagination or revision verification failed"};
    }
    if (result.success) {
        result = cardputer::validateCommittedProjectStorage();
    }
    if (!projectId.isEmpty()) {
        const cardputer::OperationResult cleanup = cardputer::deleteProject(projectId);
        if (result.success && !cleanup.success) {
            result = {false, cleanup.error};
        }
    }
    if (SD.exists(sharedPath)) {
        const cardputer::OperationResult cleanup = cardputer::deleteWorkspaceFile(sharedName);
        if (result.success && !cleanup.success) {
            result = {false, cleanup.error};
        }
    }
    Serial.printf("PROJECTSCHEMATEST result=%s chats=%u error=%s\n",
                  result.success ? "pass" : "failed",
                  static_cast<unsigned int>(kTestChatCount),
                  result.success ? "none" : result.error.c_str());
}

void runProjectMigrationTest()
{
    const cardputer::ProjectMigrationDiagnosticResult result =
        cardputer::runProjectMigrationDiagnostic();
    Serial.printf("MIGRATIONTEST result=%s legacy=%u matched=%u messages=%u archived=%u history_fnv32=%08x metadata=%s history=%s revision=%u error=%s\n",
                  result.success ? "pass" : "failed",
                  static_cast<unsigned int>(result.legacyChats),
                  static_cast<unsigned int>(result.matchedChats),
                  static_cast<unsigned int>(result.matchedMessages),
                  static_cast<unsigned int>(result.matchedArchivedMessages),
                  static_cast<unsigned int>(result.historyFnv32),
                  result.success ? "pass" : "failed",
                  result.success ? "pass" : "failed",
                  static_cast<unsigned int>(result.revision),
                  result.success ? "none" : result.error.c_str());
}

void runProjectMigrationRecoveryTest()
{
    const cardputer::ProjectMigrationRecoveryDiagnosticResult result =
        cardputer::runProjectMigrationRecoveryDiagnostic();
    Serial.printf("MIGRATIONRECOVERYTEST result=%s staging=%s corruption=%s restored=%s error=%s\n",
                  result.success ? "pass" : "failed",
                  result.stagingRecovered ? "yes" : "no",
                  result.corruptionDetected ? "yes" : "no",
                  result.originalRestored ? "yes" : "no",
                  result.success ? "none" : result.error.c_str());
}

void runProjectParityTest()
{
    const String sharedName = "firmware_project_parity.txt";
    const String bundleName = "firmware_project_parity.cardmind-project.jsonl";
    const String sharedPath = cardputer::workspaceFilePath(sharedName);
    const String bundlePath = cardputer::workspaceFilePath(bundleName);
    if (SD.exists(sharedPath)) {
        SD.remove(sharedPath);
    }
    if (SD.exists(bundlePath)) {
        SD.remove(bundlePath);
    }
    cardputer::OperationResult result = {true, ""};
    const cardputer::ScopedToolPermissionPolicy projectPolicy =
        diagnosticScopedToolPolicy();
    const cardputer::ScopedToolPermissionPolicy chatPolicy =
        cardputer::setLegacySshToolsEnabled(
            diagnosticScopedToolPolicy(), true);
    cardputer::ProjectDocumentResult source = cardputer::createProject("Parity source");
    if (!source.success) {
        result = {false, source.error};
    }
    cardputer::ChatDocumentResult chat = result.success
        ? cardputer::createProjectChat(
              source.project.summary.id, "Parity chat",
              chatPolicy)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    if (result.success && !chat.success) {
        result = {false, chat.error};
    }
    if (result.success) {
        result = cardputer::appendProjectChatMessages(
            source.project.summary.id, chat.chat.summary.id,
            {{"user", "parity"}, {"assistant", "ok"}}, 1700000000,
            settings.projectChatHistoryQuotaBytes);
    }
    if (result.success) {
        result = cardputer::createWorkspaceFile(sharedName);
    }
    if (result.success) {
        result = cardputer::linkSharedFileToProject(source.project.summary.id, sharedName);
    }
    if (result.success) {
        source = cardputer::loadProject(source.project.summary.id);
        if (!source.success) {
            result = {false, source.error};
        }
    }
    if (result.success) {
        source.project.instructions = "Parity instructions";
        source.project.model = settings.model;
        source.project.contextByteBudget = 65536;
        source.project.maximumOutputTokens = 2048;
        source.project.automaticCompaction = false;
        source.project.activeChatId = chat.chat.summary.id;
        source.project.toolPolicy = projectPolicy;
        result = cardputer::saveProject(source.project);
    }
    if (result.success) {
        result = cardputer::renameProject(source.project.summary.id, "Parity renamed");
    }
    cardputer::ProjectDocumentResult duplicate = result.success
        ? cardputer::duplicateProject(source.project.summary.id, "Parity duplicate")
        : cardputer::ProjectDocumentResult{false, {}, result.error};
    if (result.success && !duplicate.success) {
        result = {false, duplicate.error};
    }
    if (result.success) {
        result = cardputer::setProjectArchived(duplicate.project.summary.id, true);
    }
    if (result.success) {
        result = cardputer::setProjectArchived(duplicate.project.summary.id, false);
    }
    const cardputer::ChatDocumentResult duplicateChat = result.success
        ? cardputer::loadProjectChat(
              duplicate.project.summary.id, chat.chat.summary.id, 4, 1024)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    const cardputer::SharedFileLinkResult duplicateLink = result.success
        ? cardputer::projectHasSharedFileLink(duplicate.project.summary.id, sharedName)
        : cardputer::SharedFileLinkResult{false, false, result.error};
    const cardputer::ProjectDocumentResult storedDuplicate = result.success
        ? cardputer::loadProject(duplicate.project.summary.id)
        : cardputer::ProjectDocumentResult{false, {}, result.error};
    if (result.success && (!duplicateChat.success || duplicateChat.chat.messages.size() != 2 ||
                           !duplicateLink.success || !duplicateLink.linked ||
                           !storedDuplicate.success ||
                           storedDuplicate.project.instructions != "Parity instructions" ||
                           storedDuplicate.project.contextByteBudget != 65536 ||
                           storedDuplicate.project.maximumOutputTokens != 2048 ||
                           storedDuplicate.project.automaticCompaction ||
                           storedDuplicate.project.toolPolicy != projectPolicy ||
                           duplicateChat.chat.toolPolicy != chatPolicy)) {
        result = {false, "Duplicated project content verification failed"};
    }
    if (result.success) {
        result = cardputer::exportProjectBundle(source.project.summary.id, bundleName);
    }
    cardputer::ProjectDocumentResult imported = result.success
        ? cardputer::importProjectBundle(bundleName)
        : cardputer::ProjectDocumentResult{false, {}, result.error};
    if (result.success && !imported.success) {
        result = {false, imported.error};
    }
    const cardputer::ChatDocumentResult importedChat = result.success
        ? cardputer::loadProjectChat(imported.project.summary.id, chat.chat.summary.id, 4, 1024)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    const cardputer::SharedFileLinkResult importedLink = result.success
        ? cardputer::projectHasSharedFileLink(imported.project.summary.id, sharedName)
        : cardputer::SharedFileLinkResult{false, false, result.error};
    const cardputer::ScopedToolPermissionPolicy importedChatPolicy =
        cardputer::setLegacySshToolsEnabled(chatPolicy, false);
    if (result.success && (!importedChat.success || importedChat.chat.messages.size() != 2 ||
                           !importedLink.success || !importedLink.linked ||
                           imported.project.instructions != "Parity instructions" ||
                           imported.project.toolPolicy != projectPolicy ||
                           importedChat.chat.toolPolicy != importedChatPolicy)) {
        result = {false, "Imported project content verification failed"};
    }
    bool uiRoutesReady = false;
    if (result.success) {
        const cardputer::OperationResult refreshed = refreshProjectPage(0);
        if (!refreshed.success) {
            result = refreshed;
        } else {
            openProjectActions(source.project.summary);
            const std::vector<String> actions = projectActionItems();
            const bool renameRoute = actions.size() == 12 &&
                actions[6] == "Rename project" &&
                actions[10] == "Capability policies";
            openProjectImportList();
            const bool importRoute = currentScreen == Screen::WorkspaceFileList &&
                workspaceListMode == WorkspaceListMode::ImportProject;
            uiRoutesReady = renameRoute && importRoute;
            if (!uiRoutesReady) {
                result = {false, "Project device UI routes are incomplete"};
            }
        }
    }
    if (imported.success) {
        const cardputer::OperationResult cleanup = cardputer::deleteProject(
            imported.project.summary.id);
        if (result.success && !cleanup.success) {
            result = cleanup;
        }
    }
    if (duplicate.success) {
        const cardputer::OperationResult cleanup = cardputer::deleteProject(
            duplicate.project.summary.id);
        if (result.success && !cleanup.success) {
            result = cleanup;
        }
    }
    if (source.success) {
        const cardputer::OperationResult cleanup = cardputer::deleteProject(
            source.project.summary.id);
        if (result.success && !cleanup.success) {
            result = cleanup;
        }
    }
    if (SD.exists(bundlePath) && !SD.remove(bundlePath) && result.success) {
        result = {false, "Project parity bundle cleanup failed"};
    }
    if (SD.exists(sharedPath)) {
        const cardputer::OperationResult cleanup = cardputer::deleteWorkspaceFile(sharedName);
        if (result.success && !cleanup.success) {
            result = cleanup;
        }
    }
    const cardputer::OperationResult validation = cardputer::validateCommittedProjectStorage();
    if (result.success && !validation.success) {
        result = validation;
    }
    currentScreen = Screen::MainCarousel;
    renderCarousel();
    Serial.printf("PROJECTPARITYTEST result=%s ui=%s error=%s\n",
                  result.success ? "pass" : "failed",
                  uiRoutesReady ? "pass" : "failed",
                  result.success ? "none" : result.error.c_str());
}

struct P2SharedOwnershipResult {
    bool success;
    String state;
    String firstProjectId;
    String secondProjectId;
    String deniedProjectId;
    String error;
};

struct P2SharedCleanupResult {
    bool success;
    bool alreadyAbsent;
    std::uint32_t removedProjects;
    std::uint32_t removedFiles;
    String error;
};

struct P2SharedProjectPresenceResult {
    bool success;
    bool present;
    String error;
};

struct P2SharedProjectIdResult {
    bool success;
    String id;
    String error;
};

bool isP2SharedNonce(const String& nonce)
{
    if (nonce.length() < 8 || nonce.length() > 20) {
        return false;
    }
    for (std::size_t index = 0; index < nonce.length(); ++index) {
        if (nonce[index] < '0' || nonce[index] > '9') {
            return false;
        }
    }
    return true;
}

String p2SharedRootName(const String& nonce)
{
    return "cardmind_p2_20_" + nonce;
}

String p2SharedFileName(const String& nonce)
{
    return p2SharedRootName(nonce) + "/shared.txt";
}

String p2SharedOwnershipName(const String& nonce)
{
    return p2SharedRootName(nonce) + "/.cardmind-p2-20-owner.json";
}

String p2SharedProjectTitle(const String& nonce, std::size_t role)
{
    const char* labels[] = {"A", "B", "Denied"};
    return "P2 Shared " + String(labels[role]) + " " + nonce;
}

std::string p2SharedMarker(const String& nonce)
{
    return std::string("shared-identity-") + nonce.c_str() + "\n";
}

P2SharedProjectIdResult generateP2SharedProjectId(const String& firstId,
                                                  const String& secondId)
{
    for (std::size_t attempt = 0; attempt < 8; ++attempt) {
        char buffer[17] = {};
        std::snprintf(buffer, sizeof(buffer), "%08x%08x",
                      static_cast<unsigned int>(esp_random()),
                      static_cast<unsigned int>(esp_random()));
        const String id(buffer);
        if (id != firstId && id != secondId &&
            !SD.exists(cardputer::projectDirectoryPath(id))) {
            return {true, id, ""};
        }
    }
    return {false, "", "Failed to reserve a unique P2 Shared project id"};
}

cardputer::OperationResult writeP2SharedOwnership(
    const String& nonce,
    const P2SharedOwnershipResult& ownership)
{
    JsonDocument document;
    document["version"] = 1;
    document["nonce"] = nonce;
    document["path"] = p2SharedFileName(nonce);
    document["state"] = ownership.state;
    document["first_title"] = p2SharedProjectTitle(nonce, 0);
    document["second_title"] = p2SharedProjectTitle(nonce, 1);
    document["denied_title"] = p2SharedProjectTitle(nonce, 2);
    document["first_project_id"] = ownership.firstProjectId;
    document["second_project_id"] = ownership.secondProjectId;
    document["denied_project_id"] = ownership.deniedProjectId;
    return cardputer::writeAtomicJsonSdFile(
        cardputer::workspaceFilePath(p2SharedOwnershipName(nonce)), document);
}

P2SharedOwnershipResult loadP2SharedOwnership(const String& nonce)
{
    const String path = cardputer::workspaceFilePath(p2SharedOwnershipName(nonce));
    const cardputer::OperationResult recovered = cardputer::recoverAtomicSdFile(path);
    if (!recovered.success) {
        return {false, "", "", "", "", recovered.error};
    }
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return {false, "", "", "", "", "P2 Shared ownership metadata is missing"};
    }
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, file);
    file.close();
    if (error || !document["version"].is<std::uint32_t>() ||
        document["version"].as<std::uint32_t>() != 1 ||
        !document["nonce"].is<const char*>() ||
        String(document["nonce"].as<const char*>()) != nonce ||
        !document["path"].is<const char*>() ||
        String(document["path"].as<const char*>()) != p2SharedFileName(nonce) ||
        !document["state"].is<const char*>() ||
        !document["first_title"].is<const char*>() ||
        String(document["first_title"].as<const char*>()) != p2SharedProjectTitle(nonce, 0) ||
        !document["second_title"].is<const char*>() ||
        String(document["second_title"].as<const char*>()) != p2SharedProjectTitle(nonce, 1) ||
        !document["denied_title"].is<const char*>() ||
        String(document["denied_title"].as<const char*>()) != p2SharedProjectTitle(nonce, 2) ||
        !document["first_project_id"].is<const char*>() ||
        !document["second_project_id"].is<const char*>() ||
        !document["denied_project_id"].is<const char*>()) {
        return {false, "", "", "", "",
                "P2 Shared ownership metadata is invalid or belongs to another run"};
    }
    const String state = document["state"].as<const char*>();
    if (state != "pending" && state != "complete" && state != "cleaning") {
        return {false, "", "", "", "", "P2 Shared ownership state is invalid"};
    }
    const String firstId = document["first_project_id"].as<const char*>();
    const String secondId = document["second_project_id"].as<const char*>();
    const String deniedId = document["denied_project_id"].as<const char*>();
    const String ids[] = {firstId, secondId, deniedId};
    for (const String& id : ids) {
        if (id.isEmpty() || !cardputer::isValidChatId(id.c_str())) {
            return {false, "", "", "", "", "P2 Shared ownership contains an invalid project id"};
        }
    }
    if ((!firstId.isEmpty() && (firstId == secondId || firstId == deniedId)) ||
        (!secondId.isEmpty() && secondId == deniedId)) {
        return {false, "", "", "", "", "P2 Shared ownership reuses a project id"};
    }
    return {true, state, firstId, secondId, deniedId, ""};
}

cardputer::OperationResult validateP2SharedDirectory(const String& nonce)
{
    const String root = cardputer::workspaceFilePath(p2SharedRootName(nonce));
    File directory = SD.open(root, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) {
            directory.close();
        }
        return {false, "P2 Shared owned directory is missing"};
    }
    const String filePath = cardputer::workspaceFilePath(p2SharedFileName(nonce));
    const String ownerPath = cardputer::workspaceFilePath(p2SharedOwnershipName(nonce));
    File entry = directory.openNextFile();
    while (entry) {
        const String path = entry.path();
        const bool valid = !entry.isDirectory() &&
            (path == filePath || path == ownerPath);
        entry.close();
        if (!valid) {
            directory.close();
            return {false, "P2 Shared owned directory contains an unknown entry"};
        }
        entry = directory.openNextFile();
    }
    directory.close();
    return {true, ""};
}

cardputer::OperationResult validateP2SharedOwnedFile(const String& nonce,
                                                     const String& state)
{
    const String name = p2SharedFileName(nonce);
    const String path = cardputer::workspaceFilePath(name);
    const cardputer::OperationResult recovered = cardputer::recoverAtomicSdFile(path);
    if (!recovered.success) {
        return recovered;
    }
    if (!SD.exists(path)) {
        return state == "complete"
            ? cardputer::OperationResult{false, "Completed P2 Shared fixture file is missing"}
            : cardputer::OperationResult{true, ""};
    }
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) {
            file.close();
        }
        return {false, "P2 Shared fixture path is not a readable file"};
    }
    const std::string marker = p2SharedMarker(nonce);
    const std::size_t fileBytes = file.size();
    if (state == "pending" && fileBytes == 0) {
        file.close();
        return {true, ""};
    }
    if (fileBytes != marker.size()) {
        file.close();
        return {false, "P2 Shared fixture content no longer matches its ownership record"};
    }
    bool matches = true;
    for (std::size_t index = 0; index < marker.size(); ++index) {
        const int value = file.read();
        if (value < 0 || static_cast<char>(value) != marker[index]) {
            matches = false;
            break;
        }
    }
    file.close();
    if (!matches) {
        return {false, "P2 Shared fixture content no longer matches its ownership record"};
    }
    return {true, ""};
}

cardputer::OperationResult requireP2SharedTitlesAbsent(const String& nonce)
{
    std::uint32_t offset = 0;
    bool eof = false;
    while (!eof) {
        const cardputer::ProjectsPageResult page = cardputer::listProjectsPage(offset, 32);
        if (!page.success) {
            return {false, page.error};
        }
        for (const cardputer::ProjectSummary& project : page.projects) {
            for (std::size_t role = 0; role < 3; ++role) {
                if (project.title == p2SharedProjectTitle(nonce, role)) {
                    return {false, "P2 Shared project title already exists"};
                }
            }
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, "Project pagination did not advance during P2 Shared preflight"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    return {true, ""};
}

cardputer::OperationResult validateP2SharedOwnedProjectIds(
    const String& nonce,
    const P2SharedOwnershipResult& ownership)
{
    const String ids[] = {
        ownership.firstProjectId,
        ownership.secondProjectId,
        ownership.deniedProjectId,
    };
    std::uint32_t offset = 0;
    bool eof = false;
    while (!eof) {
        const cardputer::ProjectsPageResult page = cardputer::listProjectsPage(offset, 32);
        if (!page.success) {
            return {false, page.error};
        }
        for (const cardputer::ProjectSummary& project : page.projects) {
            for (std::size_t role = 0; role < 3; ++role) {
                const String title = p2SharedProjectTitle(nonce, role);
                if (project.id == ids[role] && project.title != title) {
                    return {false, "P2 Shared owned project id has an unexpected title"};
                }
                if (project.title != title) {
                    continue;
                }
                if (project.id != ids[role]) {
                    return {false, "P2 Shared project title collides with an unknown id"};
                }
            }
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, "Project pagination did not advance during P2 Shared recovery"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    return {true, ""};
}

P2SharedProjectPresenceResult findP2SharedOwnedProject(const String& id,
                                                       const String& title)
{
    if (id.isEmpty()) {
        return {true, false, ""};
    }
    std::uint32_t offset = 0;
    bool eof = false;
    while (!eof) {
        const cardputer::ProjectsPageResult page = cardputer::listProjectsPage(offset, 32);
        if (!page.success) {
            return {false, false, page.error};
        }
        for (const cardputer::ProjectSummary& project : page.projects) {
            if (project.id == id) {
                return project.title == title
                    ? P2SharedProjectPresenceResult{true, true, ""}
                    : P2SharedProjectPresenceResult{
                        false, false, "P2 Shared owned project title changed"};
            }
        }
        if (!page.eof && page.nextOffset <= offset) {
            return {false, false, "Project pagination did not advance during cleanup"};
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    return {true, false, ""};
}

cardputer::OperationResult validateP2SharedPartialDirectoryEntries(
    const String& directoryPath,
    const String* expectedPaths,
    const bool* expectedDirectories,
    std::size_t expectedCount)
{
    File directory = SD.open(directoryPath, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        if (directory) {
            directory.close();
        }
        return {false, "P2 Shared partial project path is not a directory"};
    }
    bool found[3] = {false, false, false};
    File entry = directory.openNextFile();
    while (entry) {
        const String path = entry.path();
        const bool isDirectory = entry.isDirectory();
        bool matched = false;
        for (std::size_t index = 0; index < expectedCount; ++index) {
            if (!found[index] && path == expectedPaths[index] &&
                isDirectory == expectedDirectories[index]) {
                found[index] = true;
                matched = true;
                break;
            }
        }
        entry.close();
        if (!matched) {
            directory.close();
            return {false, "P2 Shared partial project contains an unknown entry"};
        }
        entry = directory.openNextFile();
    }
    directory.close();
    for (std::size_t index = 0; index < expectedCount; ++index) {
        if (!found[index]) {
            return {false, "P2 Shared partial project is missing an expected entry"};
        }
    }
    return {true, ""};
}

cardputer::OperationResult validateP2SharedEmptyFile(const String& path)
{
    if (!SD.exists(path)) {
        return {true, ""};
    }
    File file = SD.open(path, FILE_READ);
    const bool valid = file && !file.isDirectory() && file.size() == 0;
    if (file) {
        file.close();
    }
    return valid ? cardputer::OperationResult{true, ""}
                 : cardputer::OperationResult{
                    false, "P2 Shared partial project index is not empty"};
}

cardputer::OperationResult validateP2SharedPartialProject(
    const String& id,
    const String& title)
{
    const String root = cardputer::projectDirectoryPath(id);
    const String chats = cardputer::projectChatsDirectoryPath(id);
    const String metadata = root + "/project.json";
    const String links = root + "/shared-links.jsonl";
    const String chatsIndex = chats + "/index.jsonl";
    const String atomicPaths[] = {metadata, links, chatsIndex};
    for (const String& path : atomicPaths) {
        const cardputer::OperationResult recovered = cardputer::recoverAtomicSdFile(path);
        if (!recovered.success) {
            return recovered;
        }
    }
    if (SD.exists(metadata)) {
        const cardputer::ProjectDocumentResult loaded = cardputer::loadProject(id);
        const bool expected = loaded.success &&
            loaded.project.summary.id == id &&
            loaded.project.summary.title == title &&
            loaded.project.summary.chatCount == 0 &&
            !loaded.project.summary.pinned &&
            !loaded.project.summary.archived &&
            loaded.project.summary.revision == 1 &&
            loaded.project.instructions.empty() &&
            loaded.project.activeChatId.isEmpty() &&
            loaded.project.model.isEmpty() &&
            loaded.project.apiProfile.isEmpty() &&
            loaded.project.toolPolicy ==
                cardputer::inheritedToolPermissionPolicy() &&
            loaded.project.sshProfile.isEmpty() &&
            loaded.project.contextByteBudget == 32768 &&
            loaded.project.maximumOutputTokens == 1024 &&
            loaded.project.automaticCompaction &&
            loaded.project.chatIndexRevision == 0 &&
            loaded.project.sharedLinksRevision == 0;
        if (!expected) {
            return {false, loaded.success
                ? String("P2 Shared partial project metadata changed")
                : loaded.error};
        }
    }
    cardputer::OperationResult result = validateP2SharedEmptyFile(links);
    if (result.success) {
        result = validateP2SharedEmptyFile(chatsIndex);
    }
    if (!result.success) {
        return result;
    }
    if (SD.exists(chats)) {
        String chatPaths[1];
        bool chatTypes[1] = {false};
        const std::size_t chatCount = SD.exists(chatsIndex) ? 1 : 0;
        if (chatCount == 1) {
            chatPaths[0] = chatsIndex;
        }
        result = validateP2SharedPartialDirectoryEntries(
            chats, chatPaths, chatTypes, chatCount);
        if (!result.success) {
            return result;
        }
    }
    String rootPaths[3];
    bool rootTypes[3] = {false, false, true};
    std::size_t rootCount = 0;
    if (SD.exists(metadata)) {
        rootPaths[rootCount] = metadata;
        rootTypes[rootCount++] = false;
    }
    if (SD.exists(links)) {
        rootPaths[rootCount] = links;
        rootTypes[rootCount++] = false;
    }
    if (SD.exists(chats)) {
        rootPaths[rootCount] = chats;
        rootTypes[rootCount++] = true;
    }
    return validateP2SharedPartialDirectoryEntries(
        root, rootPaths, rootTypes, rootCount);
}

bool p2SharedFixtureIsEntirelyAbsent(const String& nonce)
{
    if (SD.exists(cardputer::workspaceFilePath(p2SharedRootName(nonce)))) {
        return false;
    }
    return requireP2SharedTitlesAbsent(nonce).success;
}

P2SharedCleanupResult cleanupP2SharedFixture(const String& nonce)
{
    const String rootName = p2SharedRootName(nonce);
    const String rootPath = cardputer::workspaceFilePath(rootName);
    const String ownerPath = cardputer::workspaceFilePath(p2SharedOwnershipName(nonce));
    if (!SD.exists(rootPath)) {
        return requireP2SharedTitlesAbsent(nonce).success
            ? P2SharedCleanupResult{true, true, 0, 0, ""}
            : P2SharedCleanupResult{
                false, false, 0, 0,
                "P2 Shared projects remain without ownership metadata"};
    }
    if (!SD.exists(ownerPath) && !SD.exists(ownerPath + ".tmp") &&
        !SD.exists(ownerPath + ".bak")) {
        File directory = SD.open(rootPath, FILE_READ);
        if (!directory || !directory.isDirectory()) {
            if (directory) {
                directory.close();
            }
            return {false, false, 0, 0, "Ownerless P2 Shared root is not a directory"};
        }
        File entry = directory.openNextFile();
        const bool empty = !entry;
        if (entry) {
            entry.close();
        }
        directory.close();
        if (!empty || !requireP2SharedTitlesAbsent(nonce).success) {
            return {false, false, 0, 0,
                    "Ownerless P2 Shared recovery found data and preserved it"};
        }
        return SD.rmdir(rootPath)
            ? P2SharedCleanupResult{true, false, 0, 0, ""}
            : P2SharedCleanupResult{
                false, false, 0, 0, "Failed to remove empty P2 Shared root"};
    }
    P2SharedOwnershipResult ownership = loadP2SharedOwnership(nonce);
    if (!ownership.success) {
        if (!SD.exists(ownerPath) && !SD.exists(ownerPath + ".tmp") &&
            !SD.exists(ownerPath + ".bak")) {
            return cleanupP2SharedFixture(nonce);
        }
        return {false, false, 0, 0, ownership.error};
    }
    cardputer::OperationResult result = validateP2SharedOwnedFile(nonce, ownership.state);
    if (result.success) {
        result = validateP2SharedDirectory(nonce);
    }
    if (result.success) {
        result = validateP2SharedOwnedProjectIds(nonce, ownership);
    }
    if (!result.success) {
        return {false, false, 0, 0, result.error};
    }
    ownership.state = "cleaning";
    result = writeP2SharedOwnership(nonce, ownership);
    if (!result.success) {
        return {false, false, 0, 0, result.error};
    }
    std::uint32_t removedProjects = 0;
    const String ids[] = {
        ownership.firstProjectId,
        ownership.secondProjectId,
        ownership.deniedProjectId,
    };
    for (std::size_t role = 0; role < 3; ++role) {
        const P2SharedProjectPresenceResult presence = findP2SharedOwnedProject(
            ids[role], p2SharedProjectTitle(nonce, role));
        if (!presence.success) {
            return {false, false, removedProjects, 0, presence.error};
        }
        if (!presence.present) {
            const String directory = cardputer::projectDirectoryPath(ids[role]);
            if (!SD.exists(directory)) {
                continue;
            }
            result = validateP2SharedPartialProject(
                ids[role], p2SharedProjectTitle(nonce, role));
            if (result.success) {
                result = cardputer::removeSdDirectoryTree(directory);
            }
            if (!result.success) {
                return {false, false, removedProjects, 0, result.error};
            }
            ++removedProjects;
            continue;
        }
        result = cardputer::deleteProject(ids[role]);
        if (!result.success) {
            return {false, false, removedProjects, 0, result.error};
        }
        ++removedProjects;
    }
    for (const String& id : ids) {
        if (SD.exists(cardputer::projectDirectoryPath(id))) {
            return {false, false, removedProjects, 0,
                    "P2 Shared owned project directory remains after cleanup"};
        }
    }
    std::uint32_t removedFiles = 0;
    const String filePath = cardputer::workspaceFilePath(p2SharedFileName(nonce));
    if (SD.exists(filePath)) {
        result = cardputer::deleteWorkspaceFile(p2SharedFileName(nonce));
        if (!result.success) {
            return {false, false, removedProjects, 0, result.error};
        }
        removedFiles = 1;
    }
    if (SD.exists(ownerPath + ".tmp") || SD.exists(ownerPath + ".bak")) {
        return {false, false, removedProjects, removedFiles,
                "P2 Shared ownership has unresolved atomic artifacts"};
    }
    if (!SD.exists(ownerPath) || !SD.remove(ownerPath)) {
        return {false, false, removedProjects, removedFiles,
                "Failed to remove P2 Shared ownership metadata"};
    }
    if (!SD.rmdir(rootPath)) {
        return {false, false, removedProjects, removedFiles,
                "Failed to remove empty P2 Shared root"};
    }
    return p2SharedFixtureIsEntirelyAbsent(nonce)
        ? P2SharedCleanupResult{true, false, removedProjects, removedFiles, ""}
        : P2SharedCleanupResult{
            false, false, removedProjects, removedFiles,
            "P2 Shared owned data remains after cleanup"};
}

bool p2SharedReadToolMatches(const cardputer::ToolExecutionResult& result,
                             const String& name,
                             const std::string& marker)
{
    if (!result.success) {
        return false;
    }
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, result.output);
    return !error && document["ok"].is<bool>() && document["ok"].as<bool>() &&
        document["name"].is<const char*>() &&
        String(document["name"].as<const char*>()) == name &&
        document["content"].is<const char*>() &&
        std::string(document["content"].as<const char*>()) == marker &&
        document["eof"].is<bool>() && document["eof"].as<bool>();
}

bool p2SharedListToolOmits(const cardputer::ToolExecutionResult& result,
                           const String& name)
{
    if (!result.success) {
        return false;
    }
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, result.output);
    if (error || !document["ok"].is<bool>() || !document["ok"].as<bool>() ||
        !document["files"].is<JsonArrayConst>() ||
        !document["next_offset"].is<std::uint32_t>() ||
        !document["eof"].is<bool>()) {
        return false;
    }
    for (const JsonObjectConst file : document["files"].as<JsonArrayConst>()) {
        if (!file["name"].is<const char*>() ||
            String(file["name"].as<const char*>()) == name) {
            return false;
        }
    }
    return true;
}

cardputer::ToolExecutionResult runP2SharedProjectTool(
    const String& projectId,
    const cardputer::ToolCall& call)
{
    return cardputer::executeProjectWorkspaceTool(projectId, call);
}

void runP2SharedCleanup(const String& nonce)
{
    if (!isP2SharedNonce(nonce)) {
        Serial.printf(
            "P2SHAREDCLEAN result=failed nonce=%s already_absent=no removed_projects=0 removed_files=0 remaining=1 errors=1 error=invalid_nonce\n",
            nonce.c_str());
        return;
    }
    const P2SharedCleanupResult cleanup = cleanupP2SharedFixture(nonce);
    const bool remaining = !p2SharedFixtureIsEntirelyAbsent(nonce);
    const String error = cleanup.success && remaining
        ? String("P2 Shared owned data remains after cleanup")
        : cleanup.error;
    Serial.printf(
        "P2SHAREDCLEAN result=%s nonce=%s already_absent=%s removed_projects=%u removed_files=%u remaining=%u errors=%u error=%s\n",
        cleanup.success && !remaining ? "pass" : "failed",
        nonce.c_str(),
        cleanup.alreadyAbsent ? "yes" : "no",
        static_cast<unsigned int>(cleanup.removedProjects),
        static_cast<unsigned int>(cleanup.removedFiles),
        remaining ? 1U : 0U,
        cleanup.success && !remaining ? 0U : 1U,
        cleanup.success && !remaining ? "none" : error.c_str());
}

void runP2SharedProjectIsolationTest(const String& nonce)
{
    cardputer::OperationResult result = {true, ""};
    bool identityPassed = false;
    bool toolsPassed = false;
    bool isolationPassed = false;
    bool ownershipCreated = false;
    P2SharedOwnershipResult ownership = {true, "pending", "", "", "", ""};
    if (!isP2SharedNonce(nonce)) {
        result = {false, "invalid_nonce"};
    }
    const String rootPath = cardputer::workspaceFilePath(p2SharedRootName(nonce));
    if (result.success && SD.exists(rootPath)) {
        result = {false, "P2 Shared fixture already exists; run cleanup first"};
    }
    if (result.success) {
        result = requireP2SharedTitlesAbsent(nonce);
    }
    if (result.success) {
        const P2SharedProjectIdResult firstId = generateP2SharedProjectId("", "");
        if (!firstId.success) {
            result = {false, firstId.error};
        } else {
            ownership.firstProjectId = firstId.id;
        }
    }
    if (result.success) {
        const P2SharedProjectIdResult secondId = generateP2SharedProjectId(
            ownership.firstProjectId, "");
        if (!secondId.success) {
            result = {false, secondId.error};
        } else {
            ownership.secondProjectId = secondId.id;
        }
    }
    if (result.success) {
        const P2SharedProjectIdResult deniedId = generateP2SharedProjectId(
            ownership.firstProjectId, ownership.secondProjectId);
        if (!deniedId.success) {
            result = {false, deniedId.error};
        } else {
            ownership.deniedProjectId = deniedId.id;
        }
    }
    if (result.success) {
        result = cardputer::ensureWorkspaceFileParent(p2SharedOwnershipName(nonce));
    }
    if (result.success) {
        result = writeP2SharedOwnership(nonce, ownership);
        ownershipCreated = result.success;
    }
    const std::string marker = p2SharedMarker(nonce);
    const String sharedName = p2SharedFileName(nonce);
    if (result.success) {
        result = cardputer::createWorkspaceFile(sharedName);
    }
    if (result.success) {
        result = cardputer::replaceWorkspaceFileRange(sharedName, 0, 0, marker);
    }
    cardputer::ProjectDocumentResult first = result.success
        ? cardputer::createProjectWithId(
            p2SharedProjectTitle(nonce, 0), ownership.firstProjectId)
        : cardputer::ProjectDocumentResult{false, {}, result.error};
    if (result.success && !first.success) {
        result = {false, first.error};
    }
    cardputer::ProjectDocumentResult second = result.success
        ? cardputer::createProjectWithId(
            p2SharedProjectTitle(nonce, 1), ownership.secondProjectId)
        : cardputer::ProjectDocumentResult{false, {}, result.error};
    if (result.success && !second.success) {
        result = {false, second.error};
    }
    cardputer::ProjectDocumentResult denied = result.success
        ? cardputer::createProjectWithId(
            p2SharedProjectTitle(nonce, 2), ownership.deniedProjectId)
        : cardputer::ProjectDocumentResult{false, {}, result.error};
    if (result.success && !denied.success) {
        result = {false, denied.error};
    }
    if (result.success) {
        result = cardputer::linkSharedFileToProject(ownership.firstProjectId, sharedName);
    }
    if (result.success) {
        result = cardputer::linkSharedFileToProject(ownership.secondProjectId, sharedName);
    }
    if (result.success) {
        ownership.state = "complete";
        result = writeP2SharedOwnership(nonce, ownership);
    }

    const std::string readArguments = std::string("{\"name\":\"") +
        sharedName.c_str() + "\",\"offset\":0,\"max_bytes\":64}";
    const std::string appendArguments = std::string("{\"name\":\"") +
        sharedName.c_str() + "\",\"content\":\"DENIED\"}";
    const std::string writeArguments = std::string("{\"name\":\"") +
        sharedName.c_str() + "\",\"content\":\"DENIED\"}";
    const cardputer::ToolCall readCall = {
        "p2-shared-read", "read_file", readArguments};
    const cardputer::ToolCall listCall = {
        "p2-shared-list", "list_files", "{\"offset\":0,\"max_entries\":16}"};
    const cardputer::ToolCall appendCall = {
        "p2-shared-append", "append_file", appendArguments};
    const cardputer::ToolCall writeCall = {
        "p2-shared-write", "write_file", writeArguments};
    if (result.success) {
        const cardputer::ToolExecutionResult firstRead = runP2SharedProjectTool(
            ownership.firstProjectId, readCall);
        const cardputer::ToolExecutionResult secondRead = runP2SharedProjectTool(
            ownership.secondProjectId, readCall);
        identityPassed = p2SharedReadToolMatches(firstRead, sharedName, marker) &&
            p2SharedReadToolMatches(secondRead, sharedName, marker);
        if (!identityPassed) {
            result = {false, "Linked projects did not read the same Shared bytes"};
        }
    }
    if (result.success) {
        const cardputer::ToolExecutionResult deniedList = runP2SharedProjectTool(
            ownership.deniedProjectId, listCall);
        const cardputer::ToolExecutionResult deniedRead = runP2SharedProjectTool(
            ownership.deniedProjectId, readCall);
        const cardputer::ToolExecutionResult deniedAppend = runP2SharedProjectTool(
            ownership.deniedProjectId, appendCall);
        const cardputer::ToolExecutionResult deniedWrite = runP2SharedProjectTool(
            ownership.deniedProjectId, writeCall);
        const String deniedAccessError =
            "Shared file is not linked to the active project: " + sharedName;
        const String deniedWriteError =
            "Existing Shared file is not linked to the active project: " + sharedName;
        const cardputer::WorkspaceChunkResult unchanged = cardputer::readWorkspaceFileChunk(
            sharedName, 0, 64);
        toolsPassed = p2SharedListToolOmits(deniedList, sharedName) &&
            !deniedRead.success && deniedRead.error == deniedAccessError &&
            !deniedAppend.success && deniedAppend.error == deniedAccessError &&
            !deniedWrite.success && deniedWrite.error == deniedWriteError &&
            unchanged.success && unchanged.content == marker;
        if (!toolsPassed) {
            result = {false, "Unlinked project workspace-tool authorization failed"};
        }
    }
    if (result.success) {
        result = cardputer::deleteProject(ownership.firstProjectId);
    }
    if (result.success) {
        const cardputer::SharedFileLinkResult survivingLink =
            cardputer::projectHasSharedFileLink(ownership.secondProjectId, sharedName);
        const cardputer::SharedFileLinkResult deniedLink =
            cardputer::projectHasSharedFileLink(ownership.deniedProjectId, sharedName);
        const cardputer::ToolExecutionResult survivingRead = runP2SharedProjectTool(
            ownership.secondProjectId, readCall);
        isolationPassed = survivingLink.success && survivingLink.linked &&
            deniedLink.success && !deniedLink.linked &&
            p2SharedReadToolMatches(survivingRead, sharedName, marker);
        identityPassed = identityPassed && isolationPassed;
        if (!isolationPassed) {
            result = {false, "Deleting one project changed Shared identity or another link"};
        }
    }

    const String testError = result.success ? String("") : result.error;
    P2SharedCleanupResult cleanup = {true, true, 0, 0, ""};
    if (ownershipCreated || (isP2SharedNonce(nonce) && SD.exists(rootPath))) {
        cleanup = cleanupP2SharedFixture(nonce);
    }
    const bool remaining = isP2SharedNonce(nonce) &&
        !p2SharedFixtureIsEntirelyAbsent(nonce);
    const bool passed = result.success && cleanup.success && !remaining;
    String finalError = testError;
    if (!cleanup.success) {
        if (!finalError.isEmpty()) {
            finalError += "; cleanup: ";
        }
        finalError += cleanup.error;
    }
    if (cleanup.success && remaining) {
        if (!finalError.isEmpty()) {
            finalError += "; cleanup: ";
        }
        finalError += "P2 Shared owned data remains after cleanup";
    }
    Serial.printf(
        "P2SHAREDTEST result=%s nonce=%s identity=%s tools=%s isolation=%s cleanup=%s remaining=%u errors=%u error=%s\n",
        passed ? "pass" : "failed",
        nonce.c_str(),
        identityPassed ? "pass" : "failed",
        toolsPassed ? "pass" : "failed",
        isolationPassed ? "pass" : "failed",
        cleanup.success && !remaining ? "pass" : "failed",
        remaining ? 1U : 0U,
        passed ? 0U : 1U,
        passed ? "none" : finalError.c_str());
}

void runProjectChatIsolationTest()
{
    cardputer::OperationResult result = {true, ""};
    const cardputer::ProjectDocumentResult project = cardputer::createProject(
        "Chat isolation");
    if (!project.success) {
        result = {false, project.error};
    }
    const std::vector<String> titles = {"Alpha", "Beta", "Gamma"};
    const cardputer::ScopedToolPermissionPolicy policy =
        diagnosticScopedToolPolicy();
    std::vector<cardputer::ChatDocument> expected;
    for (std::size_t index = 0; result.success && index < titles.size(); ++index) {
        cardputer::ChatDocumentResult created = cardputer::createProjectChat(
            project.project.summary.id, titles[index],
            policy);
        if (!created.success) {
            result = {false, created.error};
            break;
        }
        created.chat.instructions = "instruction-" + std::to_string(index);
        created.chat.draft = "draft-" + std::to_string(index);
        created.chat.contextSummary = "summary-" + std::to_string(index);
        result = cardputer::saveProjectChatMetadata(created.chat);
        if (result.success) {
            result = cardputer::appendProjectChatMessages(
                project.project.summary.id, created.chat.summary.id,
                {{"user", "question-" + std::to_string(index)},
                 {"assistant", "answer-" + std::to_string(index)}},
                1700000100 + static_cast<std::uint32_t>(index),
                settings.projectChatHistoryQuotaBytes);
        }
        if (result.success) {
            expected.push_back(created.chat);
        }
    }
    for (std::size_t index = 0; result.success && index < expected.size(); ++index) {
        const cardputer::ChatDocumentResult loaded = cardputer::loadProjectChat(
            project.project.summary.id, expected[index].summary.id, 8, 4096);
        const std::string suffix = std::to_string(index);
        if (!loaded.success || loaded.chat.summary.title != titles[index] ||
            loaded.chat.instructions != "instruction-" + suffix ||
            loaded.chat.draft != "draft-" + suffix ||
            loaded.chat.contextSummary != "summary-" + suffix ||
            loaded.chat.toolPolicy != policy ||
            loaded.chat.messages.size() != 2 ||
            loaded.chat.messages[0].content != "question-" + suffix ||
            loaded.chat.messages[1].content != "answer-" + suffix) {
            result = {false, loaded.success
                ? String("Project chat state leaked across chats")
                : loaded.error};
        }
    }
    if (result.success) {
        const cardputer::ProjectChatsPageResult page = cardputer::listProjectChatsPage(
            project.project.summary.id, 0, 8);
        if (!page.success || page.chats.size() != expected.size() || !page.eof) {
            result = {false, page.success
                ? String("Project chat index does not contain exactly three chats")
                : page.error};
        }
    }
    if (project.success) {
        const cardputer::OperationResult cleanup = cardputer::deleteProject(
            project.project.summary.id);
        if (result.success && !cleanup.success) {
            result = cleanup;
        }
    }
    Serial.printf("PROJECTCHATTEST result=%s chats=%u error=%s\n",
                  result.success ? "pass" : "failed",
                  static_cast<unsigned int>(expected.size()),
                  result.success ? "none" : result.error.c_str());
}

void runRetryPersistenceTest()
{
    cardputer::OperationResult result = {true, ""};
    std::size_t userCopies = 0;
    std::size_t messageCount = 0;
    const cardputer::ProjectDocumentResult project = cardputer::createProject(
        "Retry persistence");
    const cardputer::ChatDocumentResult chat = project.success
        ? cardputer::createProjectChat(
              project.project.summary.id, "Retry target",
              cardputer::defaultNewChatToolPermissionPolicy())
        : cardputer::ChatDocumentResult{false, {}, project.error};
    if (!project.success || !chat.success) {
        result = {false, project.success ? chat.error : project.error};
    }
    const std::string prompt = "retry-persistence-marker";
    if (result.success) {
        result = cardputer::appendProjectChatMessages(
            project.project.summary.id, chat.chat.summary.id,
            {{"user", prompt}}, 1700000200,
            settings.projectChatHistoryQuotaBytes);
    }
    cardputer::ChatDocumentResult pending = result.success
        ? cardputer::loadProjectChat(
              project.project.summary.id, chat.chat.summary.id, 8, 4096)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    cardputer::RetryRequestResult retry = pending.success
        ? cardputer::prepareRetryRequest(pending.chat.messages, 4096)
        : cardputer::RetryRequestResult{false, "", {}, pending.error.c_str()};
    if (result.success && (!pending.success || !retry.success || retry.prompt != prompt ||
                           retry.messages.size() != 1)) {
        result = {false, pending.success
            ? String(retry.error.c_str()) : pending.error};
    }
    if (result.success) {
        result = cardputer::appendProjectChatMessages(
            project.project.summary.id, chat.chat.summary.id,
            {{"assistant", "retry-completed"}}, 1700000201,
            settings.projectChatHistoryQuotaBytes);
    }
    const cardputer::ChatDocumentResult completed = result.success
        ? cardputer::loadProjectChat(
              project.project.summary.id, chat.chat.summary.id, 8, 4096)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    if (completed.success) {
        messageCount = completed.chat.messages.size();
        for (const cardputer::Message& message : completed.chat.messages) {
            if (message.role == "user" && message.content == prompt) {
                ++userCopies;
            }
        }
    }
    if (result.success && (!completed.success || messageCount != 2 || userCopies != 1)) {
        result = {false, completed.success
            ? String("Retry duplicated the stored user request") : completed.error};
    }
    if (project.success) {
        const cardputer::OperationResult cleanup = cardputer::deleteProject(
            project.project.summary.id);
        if (result.success && !cleanup.success) result = cleanup;
    }
    Serial.printf("RETRYPERSISTENCETEST result=%s messages=%u user_copies=%u error=%s\n",
                  result.success ? "pass" : "failed",
                  static_cast<unsigned int>(messageCount),
                  static_cast<unsigned int>(userCopies),
                  result.success ? "none" : result.error.c_str());
}

void runCompactionPersistenceTest()
{
    cardputer::OperationResult result = {true, ""};
    std::size_t manualTail = 0;
    std::size_t automaticTail = 0;
    std::size_t rawCount = 0;
    const cardputer::ProjectDocumentResult project = cardputer::createProject(
        "Compaction persistence");
    const cardputer::ChatDocumentResult chat = project.success
        ? cardputer::createProjectChat(
              project.project.summary.id, "Compaction target",
              cardputer::defaultNewChatToolPermissionPolicy())
        : cardputer::ChatDocumentResult{false, {}, project.error};
    if (!project.success || !chat.success) {
        result = {false, project.success ? chat.error : project.error};
    }
    std::vector<cardputer::Message> messages;
    for (std::size_t index = 0; index < 12; ++index) {
        messages.push_back({
            index % 2 == 0 ? "user" : "assistant",
            "compaction-message-" + std::to_string(index),
        });
    }
    if (result.success) {
        result = cardputer::appendProjectChatMessages(
            project.project.summary.id, chat.chat.summary.id,
            messages, 1700000300, settings.projectChatHistoryQuotaBytes);
    }
    const cardputer::IndexedMessagesPageResult indexed = result.success
        ? cardputer::readProjectChatMessagesByIndex(
              project.project.summary.id, chat.chat.summary.id, 4, 3, 32768)
        : cardputer::IndexedMessagesPageResult{false, {}, 4, true, result.error};
    if (result.success && (!indexed.success || indexed.messages.size() != 3 ||
                           indexed.nextMessageIndex != 7 || indexed.eof ||
                           indexed.messages.front().content != "compaction-message-4" ||
                           indexed.messages.back().content != "compaction-message-6")) {
        result = {false, indexed.success
            ? String("Indexed project chat page boundaries are incorrect")
            : indexed.error};
    }
    cardputer::ChatDocumentResult stored = result.success
        ? cardputer::loadProjectChat(
              project.project.summary.id, chat.chat.summary.id, 16, 32768)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    if (stored.success) {
        stored.chat.contextSummary = "manual-summary";
        stored.chat.summarizedMessageCount = 4;
        result = cardputer::saveProjectChatMetadata(stored.chat);
    }
    stored = result.success
        ? cardputer::loadProjectChat(
              project.project.summary.id, chat.chat.summary.id, 16, 32768)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    if (stored.success) {
        manualTail = cardputer::unsummarizedChatTail(stored.chat).size();
        stored.chat.contextSummary = "automatic-summary";
        stored.chat.summarizedMessageCount = 8;
        result = cardputer::saveProjectChatMetadata(stored.chat);
    }
    stored = result.success
        ? cardputer::loadProjectChat(
              project.project.summary.id, chat.chat.summary.id, 16, 32768)
        : cardputer::ChatDocumentResult{false, {}, result.error};
    const cardputer::ArchivedMessagesPageResult raw = result.success
        ? cardputer::readProjectChatMessages(
              project.project.summary.id, chat.chat.summary.id, 0, 16, 32768)
        : cardputer::ArchivedMessagesPageResult{false, {}, 0, true, result.error};
    if (stored.success) {
        automaticTail = cardputer::unsummarizedChatTail(stored.chat).size();
    }
    if (raw.success) {
        rawCount = raw.messages.size();
    }
    bool rawMatches = raw.success && raw.eof && raw.messages.size() == messages.size();
    for (std::size_t index = 0; rawMatches && index < messages.size(); ++index) {
        rawMatches = raw.messages[index].role == messages[index].role &&
            raw.messages[index].content == messages[index].content;
    }
    if (result.success && (!stored.success || !rawMatches || manualTail != 8 ||
                           automaticTail != 4 || stored.chat.summary.messageCount != 12 ||
                           stored.chat.summarizedMessageCount != 8)) {
        result = {false, stored.success
            ? String("Context compaction changed raw history or active-tail boundaries")
            : stored.error};
    }
    if (project.success) {
        const cardputer::OperationResult cleanup = cardputer::deleteProject(
            project.project.summary.id);
        if (result.success && !cleanup.success) result = cleanup;
    }
    Serial.printf("COMPACTIONTEST result=%s raw=%u manual_tail=%u auto_tail=%u error=%s\n",
                  result.success ? "pass" : "failed",
                  static_cast<unsigned int>(rawCount),
                  static_cast<unsigned int>(manualTail),
                  static_cast<unsigned int>(automaticTail),
                  result.success ? "none" : result.error.c_str());
}

struct P2ArchiveFileDigest {
    bool success;
    std::uint64_t bytes;
    std::uint32_t fnv32;
    String error;
};

struct P2ArchiveSnapshot {
    bool success;
    std::uint32_t messageCount;
    P2ArchiveFileDigest history;
    P2ArchiveFileDigest tail;
    P2ArchiveFileDigest metadata;
    P2ArchiveFileDigest index;
    String error;
};

std::uint32_t updateP2ArchiveFnv(std::uint32_t hash, const std::string& value)
{
    for (const unsigned char character : value) {
        hash ^= character;
        hash *= 16777619U;
    }
    return hash;
}

std::uint32_t updateP2ArchiveFnv(std::uint32_t hash, const String& value)
{
    for (std::size_t index = 0; index < value.length(); ++index) {
        hash ^= static_cast<std::uint8_t>(value[index]);
        hash *= 16777619U;
    }
    return hash;
}

P2ArchiveFileDigest digestP2ArchiveFile(const String& path)
{
    File file = SD.open(path, FILE_READ);
    if (!file) {
        return {false, 0, 0, "Archive diagnostic file is unavailable"};
    }
    const std::uint64_t expectedBytes = file.size();
    std::uint64_t bytes = 0;
    std::uint32_t hash = 2166136261U;
    std::uint8_t buffer[512] = {};
    while (file.available() > 0) {
        const std::size_t readBytes = file.read(buffer, sizeof(buffer));
        if (readBytes == 0) {
            file.close();
            return {false, bytes, hash, "Archive diagnostic file read stopped early"};
        }
        for (std::size_t index = 0; index < readBytes; ++index) {
            hash ^= buffer[index];
            hash *= 16777619U;
        }
        bytes += readBytes;
    }
    file.close();
    return bytes == expectedBytes
        ? P2ArchiveFileDigest{true, bytes, hash, ""}
        : P2ArchiveFileDigest{false, bytes, hash,
                              "Archive diagnostic file size changed while reading"};
}

P2ArchiveSnapshot captureP2ArchiveSnapshot(const String& projectId,
                                           const String& chatId)
{
    const String chatRoot = cardputer::projectChatDirectoryPath(projectId, chatId);
    const String chatIndex = cardputer::projectChatsDirectoryPath(projectId) +
        "/index.jsonl";
    const cardputer::ChatDocumentResult chat = cardputer::loadProjectChatMetadata(
        projectId, chatId);
    const P2ArchiveFileDigest history = digestP2ArchiveFile(
        chatRoot + "/history.jsonl");
    const P2ArchiveFileDigest tail = digestP2ArchiveFile(chatRoot + "/tail.jsonl");
    const P2ArchiveFileDigest metadata = digestP2ArchiveFile(chatRoot + "/chat.json");
    const P2ArchiveFileDigest index = digestP2ArchiveFile(chatIndex);
    if (!chat.success || !history.success || !tail.success || !metadata.success ||
        !index.success) {
        return {false, 0, history, tail, metadata, index,
                chat.success ? String("Archive diagnostic snapshot is incomplete")
                             : chat.error};
    }
    return {true, chat.chat.summary.messageCount, history, tail, metadata, index, ""};
}

bool equalP2ArchiveDigest(const P2ArchiveFileDigest& left,
                          const P2ArchiveFileDigest& right)
{
    return left.success && right.success && left.bytes == right.bytes &&
        left.fnv32 == right.fnv32;
}

bool equalP2ArchiveSnapshot(const P2ArchiveSnapshot& left,
                            const P2ArchiveSnapshot& right)
{
    return left.success && right.success && left.messageCount == right.messageCount &&
        equalP2ArchiveDigest(left.history, right.history) &&
        equalP2ArchiveDigest(left.tail, right.tail) &&
        equalP2ArchiveDigest(left.metadata, right.metadata) &&
        equalP2ArchiveDigest(left.index, right.index);
}

bool p2ArchiveArtifactsAbsent(const String& projectId, const String& chatId)
{
    const String chatRoot = cardputer::projectChatDirectoryPath(projectId, chatId);
    const String chatIndex = cardputer::projectChatsDirectoryPath(projectId) +
        "/index.jsonl";
    const std::vector<String> targets = {
        chatRoot + "/append.pending.json",
        chatRoot + "/history.jsonl.tmp", chatRoot + "/history.jsonl.bak",
        chatRoot + "/tail.jsonl.tmp", chatRoot + "/tail.jsonl.bak",
        chatRoot + "/chat.json.tmp", chatRoot + "/chat.json.bak",
        chatIndex + ".tmp", chatIndex + ".bak",
    };
    for (const String& target : targets) {
        if (SD.exists(target)) {
            return false;
        }
    }
    return true;
}

String p2ArchiveProjectId(const String& nonce)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::size_t index = 0; index < nonce.length(); ++index) {
        hash ^= static_cast<std::uint8_t>(nonce[index]);
        hash *= 1099511628211ULL;
    }
    char value[17] = {};
    std::snprintf(value, sizeof(value), "%016llx",
                  static_cast<unsigned long long>(hash));
    return String(value);
}

constexpr std::size_t kP2ArchiveMessageBytes = 16384;

std::string p2ArchiveMessageContent(const String& nonce, std::uint32_t index)
{
    std::string content = "p2archive-" + std::string(nonce.c_str()) + "-" +
        std::to_string(index) + "|";
    content.resize(kP2ArchiveMessageBytes, static_cast<char>('a' + (index % 26U)));
    return content;
}

bool matchesP2ArchiveMessageContent(const std::string& content,
                                    const String& nonce,
                                    std::uint32_t index)
{
    const std::string prefix = "p2archive-" + std::string(nonce.c_str()) + "-" +
        std::to_string(index) + "|";
    if (content.size() != kP2ArchiveMessageBytes ||
        content.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    const char fill = static_cast<char>('a' + (index % 26U));
    for (std::size_t offset = prefix.size(); offset < content.size(); ++offset) {
        if (content[offset] != fill) {
            return false;
        }
    }
    return true;
}

void runP2BinaryFileTest(const String& nonce)
{
    bool matrix = false;
    bool ui = false;
    bool read = false;
    bool write = false;
    bool append = false;
    bool nonmutation = false;
    bool cleanup = false;
    String error = "none";
    const String name = "p2-binary-" + nonce + ".bin";
    const String temporaryName = name + ".tmp";
    const String path = cardputer::workspaceFilePath(name);
    const String temporaryPath = cardputer::workspaceFilePath(temporaryName);
    cardputer::OperationResult result = {true, ""};
    bool ownsPaths = false;

    if (!isP2UnicodeNonce(nonce)) {
        result = {false, "invalid_nonce"};
    } else if (!fileWorkspaceReady) {
        result = {false, "workspace_unavailable"};
    } else if (SD.exists(path) || SD.exists(temporaryPath)) {
        result = {false, "binary_fixture_path_already_exists"};
    }
    ownsPaths = result.success;
    matrix = result.success && cardputer::isValidWorkspaceFilename(name.c_str()) &&
        !cardputer::isWorkspaceTextFile(name.c_str()) &&
        cardputer::isWorkspaceTextFile("nested/CASE.CPP") &&
        !cardputer::isValidWorkspaceFilename("unsafe.tmp") &&
        !cardputer::isValidWorkspaceFilename("unsafe.bak");
    if (result.success && !matrix) {
        result = {false, "workspace_extension_matrix_mismatch"};
    }
    if (result.success) {
        const std::uint8_t fixture[] = {0x00, 0xff, 0x10, 0x80, 0x41, 0x0a};
        File temporary = SD.open(temporaryPath, FILE_WRITE);
        if (!temporary) {
            result = {false, "binary_fixture_temporary_open_failed"};
        } else {
            const std::size_t written = temporary.write(fixture, sizeof(fixture));
            temporary.flush();
            temporary.close();
            if (written != sizeof(fixture)) {
                result = {false, "binary_fixture_temporary_write_failed"};
            }
        }
    }
    if (result.success) {
        result = cardputer::commitWorkspaceBinaryTemporary(name, temporaryName);
    }
    const P2ArchiveFileDigest before = result.success
        ? digestP2ArchiveFile(path)
        : P2ArchiveFileDigest{false, 0, 0, result.error};
    if (result.success && !before.success) {
        result = {false, before.error};
    }
    if (result.success) {
        const String originalName = fileViewerName;
        fileViewerName = name;
        const std::vector<String> actions = fileActionItems();
        ui = actions.size() == 5 && actions.front() == "Save copy as..." &&
            actions[1] == "Rename..." && actions[3] == "Delete file" &&
            actions.back() == "Back";
        fileViewerName = originalName;
        if (!ui) {
            result = {false, "binary_management_menu_mismatch"};
        }
    }
    const std::string toolArguments = "{\"name\":\"" +
        std::string(name.c_str()) + "\",\"offset\":0,\"max_bytes\":16," +
        "\"content\":\"replacement\"}";
    if (result.success) {
        const cardputer::ToolExecutionResult rejected =
            cardputer::executeWorkspaceTool(
                {"p2-binary-read", "read_file", toolArguments});
        read = !rejected.success;
        if (!read) result = {false, "binary_read_tool_was_not_rejected"};
    }
    if (result.success) {
        const cardputer::ToolExecutionResult rejected =
            cardputer::executeWorkspaceTool(
                {"p2-binary-write", "write_file", toolArguments});
        write = !rejected.success;
        if (!write) result = {false, "binary_write_tool_was_not_rejected"};
    }
    if (result.success) {
        const cardputer::ToolExecutionResult rejected =
            cardputer::executeWorkspaceTool(
                {"p2-binary-append", "append_file", toolArguments});
        append = !rejected.success;
        if (!append) result = {false, "binary_append_tool_was_not_rejected"};
    }
    if (result.success) {
        const P2ArchiveFileDigest after = digestP2ArchiveFile(path);
        nonmutation = equalP2ArchiveDigest(before, after);
        if (!nonmutation) {
            result = {false, "binary_tool_rejection_changed_file"};
        }
    }
    if (SD.exists(temporaryPath)) {
        SD.remove(temporaryPath);
    }
    if (ownsPaths && SD.exists(path)) {
        const cardputer::OperationResult removed = cardputer::deleteWorkspaceFile(name);
        cleanup = removed.success && !SD.exists(path) && !SD.exists(temporaryPath);
        if (result.success && !cleanup) {
            result = {false, removed.success
                ? String("binary_fixture_remains") : removed.error};
        }
    } else if (ownsPaths) {
        cleanup = !SD.exists(path) && !SD.exists(temporaryPath);
    }
    if (!result.success) {
        error = serialSafeError(result.error, 180);
    }
    const bool passed = result.success && matrix && ui && read && write && append &&
        nonmutation && cleanup;
    Serial.printf(
        "P2BINARYTEST result=%s nonce=%s matrix=%s ui=%s read=%s write=%s append=%s nonmutation=%s cleanup=%s error=%s\n",
        passed ? "pass" : "failed", nonce.c_str(),
        matrix ? "pass" : "failed", ui ? "pass" : "failed",
        read ? "pass" : "failed", write ? "pass" : "failed",
        append ? "pass" : "failed", nonmutation ? "pass" : "failed",
        cleanup ? "pass" : "failed", passed ? "none" : error.c_str());
}

void runP2ArchiveTest(const String& nonce)
{
    constexpr std::uint32_t kTargetMessages = 129;
    constexpr std::uint32_t kMiddleMessage = kTargetMessages / 2U;
    const std::uint32_t heapBefore = ESP.getFreeHeap();
    const std::uint32_t largestBefore =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    bool beyondTwoMib = false;
    bool quota = false;
    bool full = false;
    bool planner = false;
    bool nonmutation = false;
    bool first = false;
    bool middle = false;
    bool last = false;
    bool count = false;
    bool hash = false;
    bool artifacts = false;
    bool cleanup = false;
    bool faultSet = false;
    String error = "none";
    const String projectId = p2ArchiveProjectId(nonce);
    const String title = "P2 archive " + nonce;
    String chatId;
    std::uint32_t expectedHash = 2166136261U;
    cardputer::OperationResult result = {true, ""};

    constexpr std::uint64_t kPlannerMarkerBytes = 128;
    constexpr std::uint64_t kPlannerRawBytes = 1024;
    constexpr std::uint64_t kPlannerTailBytes = 2048;
    constexpr std::uint64_t kPlannerMetadataBytes = 4096;
    constexpr std::uint64_t kPlannerRequiredBytes = kPlannerMarkerBytes +
        kPlannerRawBytes + kPlannerTailBytes + kPlannerMetadataBytes +
        cardputer::kStorageOperationalFloorBytes;
    const cardputer::ProjectChatAppendPlanResult exactSpace =
        cardputer::planProjectChatAppend(
            0, kPlannerRawBytes, 0, kPlannerMarkerBytes, kPlannerTailBytes,
            kPlannerMetadataBytes, kPlannerRequiredBytes,
            cardputer::kStorageOperationalFloorBytes);
    const cardputer::ProjectChatAppendPlanResult insufficientSpace =
        cardputer::planProjectChatAppend(
            0, kPlannerRawBytes, 0, kPlannerMarkerBytes, kPlannerTailBytes,
            kPlannerMetadataBytes, kPlannerRequiredBytes - 1U,
            cardputer::kStorageOperationalFloorBytes);
    planner = exactSpace.success &&
        exactSpace.requiredFreeBytes == kPlannerRequiredBytes &&
        !insufficientSpace.success &&
        insufficientSpace.requiredFreeBytes == kPlannerRequiredBytes;
    if (!planner) {
        result = {false, "append_space_planner_boundary_mismatch"};
    }

    if (result.success && !isP2UnicodeNonce(nonce)) {
        result = {false, "invalid_nonce"};
    }
    if (result.success && SD.exists(cardputer::projectDirectoryPath(projectId))) {
        const cardputer::ProjectDocumentResult previous = cardputer::loadProject(projectId);
        if (!previous.success || previous.project.summary.title != title) {
            result = {false, "archive_fixture_id_is_not_owned_by_nonce"};
        } else {
            result = cardputer::deleteProject(projectId);
        }
    }
    const cardputer::ProjectDocumentResult project = result.success
        ? cardputer::createProjectWithId(title, projectId)
        : cardputer::ProjectDocumentResult{false, {}, result.error};
    if (result.success && !project.success) {
        result = {false, project.error};
    }
    const cardputer::ChatDocumentResult chat = result.success
        ? cardputer::createProjectChat(
              projectId, "Archive boundary",
              cardputer::defaultNewChatToolPermissionPolicy())
        : cardputer::ChatDocumentResult{false, {}, result.error};
    if (result.success && !chat.success) {
        result = {false, chat.error};
    }
    if (chat.success) {
        chatId = chat.chat.summary.id;
    }

    try {
        for (std::uint32_t firstIndex = 0;
             result.success && firstIndex < kTargetMessages;
             firstIndex += 2U) {
            std::vector<cardputer::Message> batch;
            batch.reserve(2);
            const std::uint32_t batchEnd = std::min(
                firstIndex + 2U, kTargetMessages);
            for (std::uint32_t index = firstIndex; index < batchEnd; ++index) {
                batch.push_back({index % 2U == 0 ? "user" : "assistant",
                                 p2ArchiveMessageContent(nonce, index)});
                expectedHash = updateP2ArchiveFnv(expectedHash, batch.back().role);
                expectedHash = updateP2ArchiveFnv(expectedHash, batch.back().content);
            }
            result = cardputer::appendProjectChatMessages(
                projectId, chatId, batch, 1700000500ULL + firstIndex, 0);
            if (batchEnd % 16U == 0 || batchEnd == kTargetMessages) {
                Serial.printf("P2ARCHIVETEST stage=append messages=%u heap=%u\n",
                              static_cast<unsigned int>(batchEnd),
                              static_cast<unsigned int>(ESP.getFreeHeap()));
                Serial.flush();
            }
        }
    } catch (const std::bad_alloc&) {
        result = {false, "archive_fixture_batch_allocation_failed"};
    }

    P2ArchiveSnapshot baseline = {false, 0, {}, {}, {}, {}, ""};
    if (result.success) {
        baseline = captureP2ArchiveSnapshot(projectId, chatId);
        result = baseline.success
            ? cardputer::OperationResult{true, ""}
            : cardputer::OperationResult{false, baseline.error};
        beyondTwoMib = baseline.success &&
            baseline.history.bytes > 2ULL * 1024ULL * 1024ULL;
        if (!beyondTwoMib) {
            result = {false, "archive_fixture_did_not_cross_2_mib"};
        }
    }

    const std::vector<cardputer::Message> rejectedBatch = {
        {"user", "archive-rejection-probe"},
    };
    if (result.success && baseline.history.bytes <=
            std::numeric_limits<std::uint32_t>::max()) {
        const cardputer::OperationResult rejected =
            cardputer::appendProjectChatMessages(
                projectId, chatId, rejectedBatch, 1700000600ULL,
                static_cast<std::uint32_t>(baseline.history.bytes));
        const P2ArchiveSnapshot afterQuota = captureP2ArchiveSnapshot(projectId, chatId);
        quota = !rejected.success && equalP2ArchiveSnapshot(baseline, afterQuota) &&
            p2ArchiveArtifactsAbsent(projectId, chatId);
        if (!quota) {
            result = {false, "quota_rejection_changed_archive_state"};
        }
    }

    if (result.success) {
        cardputer::setSdStorageFaultOverrideForDiagnostics(
            cardputer::SdStorageState::Full);
        faultSet = true;
        const cardputer::OperationResult rejected =
            cardputer::appendProjectChatMessages(
                projectId, chatId, rejectedBatch, 1700000601ULL, 0);
        cardputer::clearSdStorageFaultOverrideForDiagnostics();
        faultSet = false;
        const P2ArchiveSnapshot afterFull = captureP2ArchiveSnapshot(projectId, chatId);
        full = !rejected.success && equalP2ArchiveSnapshot(baseline, afterFull) &&
            p2ArchiveArtifactsAbsent(projectId, chatId);
        nonmutation = quota && full;
        artifacts = p2ArchiveArtifactsAbsent(projectId, chatId);
        if (!full) {
            result = {false, "full_rejection_changed_archive_state"};
        }
    }

    std::uint32_t actualHash = 2166136261U;
    std::uint32_t messageIndex = 0;
    std::uint32_t offset = 0;
    bool eof = false;
    while (result.success && !eof) {
        const cardputer::ArchivedMessagesPageResult page =
            cardputer::readProjectChatMessages(
                projectId, chatId, offset, 2, 32768);
        if (!page.success || page.messages.empty() || page.nextOffset <= offset) {
            result = {false, page.success
                ? String("archive_page_did_not_advance") : page.error};
            break;
        }
        for (const cardputer::Message& message : page.messages) {
            if (messageIndex >= kTargetMessages) {
                result = {false, "archive_readback_has_extra_messages"};
                break;
            }
            const char* expectedRole =
                messageIndex % 2U == 0 ? "user" : "assistant";
            const bool exact = message.role == expectedRole &&
                matchesP2ArchiveMessageContent(message.content, nonce, messageIndex);
            if (messageIndex == 0) first = exact;
            if (messageIndex == kMiddleMessage) middle = exact;
            if (messageIndex == kTargetMessages - 1U) last = exact;
            actualHash = updateP2ArchiveFnv(actualHash, message.role);
            actualHash = updateP2ArchiveFnv(actualHash, message.content);
            ++messageIndex;
        }
        offset = page.nextOffset;
        eof = page.eof;
    }
    count = result.success && messageIndex == kTargetMessages && eof;
    hash = count && actualHash == expectedHash;
    if (result.success && (!first || !middle || !last || !count || !hash)) {
        result = {false, "archive_readback_mismatch"};
    }

    if (faultSet) {
        cardputer::clearSdStorageFaultOverrideForDiagnostics();
    }
    if (project.success || SD.exists(cardputer::projectDirectoryPath(projectId))) {
        const cardputer::OperationResult removed = cardputer::deleteProject(projectId);
        cleanup = removed.success && !SD.exists(cardputer::projectDirectoryPath(projectId));
        if (result.success && !cleanup) {
            result = {false, removed.success
                ? String("archive_fixture_remains") : removed.error};
        }
    } else {
        cleanup = true;
    }
    const cardputer::SdStorageStatus finalStorage = cardputer::inspectSdStorage();
    if (result.success && finalStorage.state != cardputer::SdStorageState::Ready) {
        result = {false, "sd_fault_override_was_not_cleared"};
    }
    if (!result.success) {
        error = serialSafeError(result.error, 180);
    }
    const bool passed = result.success && beyondTwoMib && quota && full && planner &&
        nonmutation && first && middle && last && count && hash && artifacts && cleanup;
    Serial.printf(
        "P2ARCHIVETEST result=%s nonce=%s beyond_2mib=%s quota=%s full=%s planner=%s nonmutation=%s first=%s middle=%s last=%s count=%s hash=%s artifacts=%s cleanup=%s heap_before=%u heap_after=%u largest_before=%u largest_after=%u error=%s\n",
        passed ? "pass" : "failed", nonce.c_str(),
        beyondTwoMib ? "pass" : "failed", quota ? "pass" : "failed",
        full ? "pass" : "failed", planner ? "pass" : "failed",
        nonmutation ? "pass" : "failed",
        first ? "pass" : "failed", middle ? "pass" : "failed",
        last ? "pass" : "failed", count ? "pass" : "failed",
        hash ? "pass" : "failed", artifacts ? "pass" : "failed",
        cleanup ? "pass" : "failed", static_cast<unsigned int>(heapBefore),
        static_cast<unsigned int>(ESP.getFreeHeap()),
        static_cast<unsigned int>(largestBefore),
        static_cast<unsigned int>(
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
        passed ? "none" : error.c_str());
}

void runP2SummaryTest(const String& nonce)
{
    bool provider = false;
    bool replaced = false;
    bool covered = false;
    bool raw = false;
    bool context = false;
    bool cleanup = false;
    String error = "none";
    const Screen originalScreen = currentScreen;
    const String originalStatusMessage = statusMessage;
    cardputer::ProjectDocumentResult project = {
        false, {}, "Summary diagnostic was not initialized"};
    if (!isP2UnicodeNonce(nonce)) {
        error = "invalid_nonce";
    } else if (!cardputer::settingsAreComplete(settings)) {
        error = "chat_provider_not_configured";
    } else {
        project = cardputer::createProject("P2 summary " + nonce);
        const cardputer::ChatDocumentResult chat = project.success
            ? cardputer::createProjectChat(
                  project.project.summary.id, "P2 summary target",
                  cardputer::defaultNewChatToolPermissionPolicy())
            : cardputer::ChatDocumentResult{false, {}, project.error};
        cardputer::OperationResult result = project.success && chat.success
            ? cardputer::OperationResult{true, ""}
            : cardputer::OperationResult{
                  false, project.success ? chat.error : project.error};
        std::vector<cardputer::Message> messages;
        messages.reserve(14);
        for (std::size_t index = 0; index < 14; ++index) {
            messages.push_back({
                index % 2 == 0 ? "user" : "assistant",
                "summary-contract-message-" + std::to_string(index),
            });
        }
        if (result.success) {
            result = cardputer::appendProjectChatMessages(
                project.project.summary.id, chat.chat.summary.id,
                messages, 1700000400, settings.projectChatHistoryQuotaBytes);
        }
        cardputer::ChatDocumentResult staged = result.success
            ? cardputer::loadProjectChatMetadata(
                  project.project.summary.id, chat.chat.summary.id)
            : cardputer::ChatDocumentResult{false, {}, result.error};
        constexpr const char* kSentinelSummary = "p2-summary-sentinel";
        if (staged.success) {
            staged.chat.contextSummary = kSentinelSummary;
            staged.chat.summarizedMessageCount = 6;
            result = cardputer::saveProjectChatMetadata(staged.chat);
        }
        if (result.success) {
            result = regenerateProjectChatContextSummary(
                project.project.summary.id, chat.chat.summary.id,
                project.project);
            provider = result.success;
        }
        const cardputer::ChatDocumentResult verified = result.success
            ? cardputer::loadProjectChat(
                  project.project.summary.id, chat.chat.summary.id, 16, 32768)
            : cardputer::ChatDocumentResult{false, {}, result.error};
        const cardputer::IndexedMessagesPageResult rawPage = result.success
            ? cardputer::readProjectChatMessagesByIndex(
                  project.project.summary.id, chat.chat.summary.id, 0, 16, 32768)
            : cardputer::IndexedMessagesPageResult{false, {}, 0, true, result.error};
        if (verified.success) {
            const cardputer::ContextUsage usage = cardputer::resolveContextUsage(
                verified.chat, project.project.contextByteBudget);
            replaced = !verified.chat.contextSummary.empty() &&
                verified.chat.contextSummary != kSentinelSummary;
            covered = verified.chat.summarizedMessageCount == 6;
            context = usage.retainedMessages == 8 &&
                usage.summarizedMessages == 6 && usage.totalMessages == 14;
        }
        raw = rawPage.success && rawPage.eof &&
            rawPage.nextMessageIndex == 14 && rawPage.messages.size() == 14;
        for (std::size_t index = 0; raw && index < messages.size(); ++index) {
            raw = rawPage.messages[index].role == messages[index].role &&
                rawPage.messages[index].content == messages[index].content;
        }
        if (result.success && !(replaced && covered && raw && context)) {
            result = {false, "Summary production contract verification failed"};
        }
        if (!result.success) {
            error = "production_regeneration";
        }
    }
    if (project.success) {
        const String projectId = project.project.summary.id;
        const cardputer::OperationResult removed = cardputer::deleteProject(projectId);
        cleanup = removed.success && !cardputer::loadProject(projectId).success;
        if (!cleanup && error == "none") {
            error = removed.success ? String("cleanup_verify")
                                    : String("cleanup_delete");
        }
    }
    currentScreen = originalScreen;
    statusMessage = originalStatusMessage;
    render();
    const bool passed = provider && replaced && covered && raw && context && cleanup;
    Serial.printf(
        "P2SUMMARYTEST result=%s nonce=%s provider=%s replace=%s covered=%s raw=%s context=%s cleanup=%s error=%s\n",
        passed ? "pass" : "failed", nonce.c_str(),
        provider ? "pass" : "failed", replaced ? "pass" : "failed",
        covered ? "pass" : "failed", raw ? "pass" : "failed",
        context ? "pass" : "failed", cleanup ? "pass" : "failed",
        passed ? "none" : error.c_str());
}

void runPhaseTwoLimitTest()
{
    Serial.println("P2LIMITTEST stage=create");
    Serial.flush();
    cardputer::OperationResult result = {true, ""};
    bool projectBoundaries = false;
    bool chatBoundaries = false;
    const cardputer::ProjectDocumentResult project =
        cardputer::createProject("P2 limit boundaries");
    const cardputer::ChatDocumentResult chat = project.success
        ? cardputer::createProjectChat(
              project.project.summary.id, "Boundary chat",
              cardputer::defaultNewChatToolPermissionPolicy())
        : cardputer::ChatDocumentResult{false, {}, project.error};
    if (!project.success || !chat.success) {
        result = {false, project.success ? chat.error : project.error};
    }
    if (result.success) {
        Serial.println("P2LIMITTEST stage=project_exact");
        Serial.flush();
        cardputer::ProjectDocument boundary = project.project;
        boundary.instructions.assign(
            cardputer::kMaximumProjectInstructionsBytes, 'p');
        boundary.contextByteBudget = 8192;
        boundary.maximumOutputTokens = 128;
        result = cardputer::saveProject(boundary);
        if (result.success) {
            boundary.contextByteBudget = 262144;
            boundary.maximumOutputTokens = 8192;
            result = cardputer::saveProject(boundary);
        }
        boundary.instructions.clear();
        boundary.instructions.shrink_to_fit();
        Serial.println("P2LIMITTEST stage=project_rejections");
        Serial.flush();
        boundary.instructions.assign(
            cardputer::kMaximumProjectInstructionsBytes + 1, 'x');
        const cardputer::OperationResult oversizedInstructions =
            cardputer::saveProject(boundary);
        boundary.instructions.clear();
        boundary.instructions.shrink_to_fit();
        boundary.contextByteBudget = 8191;
        const cardputer::OperationResult undersizedContext =
            cardputer::saveProject(boundary);
        boundary.contextByteBudget = 262145;
        const cardputer::OperationResult oversizedContext =
            cardputer::saveProject(boundary);
        boundary.contextByteBudget = 262144;
        boundary.maximumOutputTokens = 127;
        const cardputer::OperationResult undersizedOutput =
            cardputer::saveProject(boundary);
        boundary.maximumOutputTokens = 8193;
        const cardputer::OperationResult oversizedOutput =
            cardputer::saveProject(boundary);
        projectBoundaries = result.success && !oversizedInstructions.success &&
            !undersizedContext.success && !oversizedContext.success &&
            !undersizedOutput.success && !oversizedOutput.success;
        if (!projectBoundaries && result.success) {
            result = {false,
                oversizedInstructions.success ? String("Oversized project instructions accepted") :
                undersizedContext.success ? String("Undersized project context accepted") :
                oversizedContext.success ? String("Oversized project context accepted") :
                undersizedOutput.success ? String("Undersized project output accepted") :
                oversizedOutput.success ? String("Oversized project output accepted") :
                String("Exact project boundary persistence failed")};
        }
    }
    if (result.success) {
        Serial.println("P2LIMITTEST stage=chat_exact");
        Serial.flush();
        cardputer::ChatDocument boundary = chat.chat;
        boundary.instructions.assign(
            cardputer::kMaximumProjectChatInstructionsBytes, 'i');
        boundary.draft.assign(cardputer::kMaximumProjectChatDraftBytes, 'd');
        Serial.println("P2LIMITTEST stage=chat_save");
        Serial.flush();
        result = cardputer::saveProjectChatMetadata(boundary);
        boundary.instructions.clear();
        boundary.instructions.shrink_to_fit();
        boundary.draft.clear();
        boundary.draft.shrink_to_fit();
        bool exactPersisted = false;
        if (result.success) {
            Serial.println("P2LIMITTEST stage=chat_load");
            Serial.flush();
            {
                const cardputer::ChatDocumentResult stored =
                    cardputer::loadProjectChatMetadata(
                        project.project.summary.id, chat.chat.summary.id);
                exactPersisted = stored.success &&
                    stored.chat.instructions.size() ==
                        cardputer::kMaximumProjectChatInstructionsBytes &&
                    stored.chat.draft.size() ==
                        cardputer::kMaximumProjectChatDraftBytes;
                if (!stored.success) result = {false, stored.error};
            }
        }
        Serial.println("P2LIMITTEST stage=chat_rejections");
        Serial.flush();
        boundary.instructions.assign(
            cardputer::kMaximumProjectChatInstructionsBytes + 1, 'x');
        const cardputer::OperationResult oversizedInstructions =
            cardputer::saveProjectChatMetadata(boundary);
        boundary.instructions.clear();
        boundary.instructions.shrink_to_fit();
        boundary.draft.assign(cardputer::kMaximumProjectChatDraftBytes + 1, 'x');
        const cardputer::OperationResult oversizedDraft =
            cardputer::saveProjectChatMetadata(boundary);
        chatBoundaries = exactPersisted &&
            !oversizedInstructions.success && !oversizedDraft.success;
        if (!chatBoundaries && result.success) {
            result = {false,
                !exactPersisted ? String("Exact chat boundary persistence failed") :
                oversizedInstructions.success ? String("Oversized chat instructions accepted") :
                oversizedDraft.success ? String("Oversized chat draft accepted") :
                String("Chat boundary validation failed")};
        }
    }
    Serial.println("P2LIMITTEST stage=cleanup");
    Serial.flush();
    if (project.success) {
        const cardputer::OperationResult cleanup =
            cardputer::deleteProject(project.project.summary.id);
        if (result.success && !cleanup.success) result = cleanup;
    }
    const bool promptBoundary = kMaximumInputBytes == 16384;
    if (result.success && !promptBoundary) {
        result = {false, "Device prompt boundary is not 16384 bytes"};
    }
    Serial.printf(
        "P2LIMITTEST result=%s prompt=%s project=%s chat=%s error=%s\n",
        result.success ? "pass" : "failed",
        promptBoundary ? "pass" : "failed",
        projectBoundaries ? "pass" : "failed",
        chatBoundaries ? "pass" : "failed",
        result.success ? "none" : result.error.c_str());
}

void runInstructionPrecedenceTest()
{
    cardputer::Settings fixtureSettings = {};
    fixtureSettings.model = "instruction-contract-model";
    cardputer::ProjectDocument project = {};
    project.model = fixtureSettings.model;
    project.contextByteBudget = 32768;
    project.maximumOutputTokens = 2048;
    project.automaticCompaction = true;
    const cardputer::ChatDocument chat = {};
    const cardputer::ResolvedProjectRequestPolicy policy =
        cardputer::resolveProjectRequestPolicy(fixtureSettings, project, chat, 0);
    const cardputer::ChatRequestSerializationValidation validation =
        cardputer::validateChatRequestSerialization(
            fixtureSettings, policy, "global-marker", "project-marker",
            "chat-marker", "request-marker", "summary-marker");
    const bool ordered = validation.success;
    Serial.printf("INSTRUCTIONTEST result=%s order=%s error=%s\n",
                   ordered ? "pass" : "failed", ordered ? "pass" : "failed",
                   ordered ? "none" : "Instruction scopes are out of order");
}

void runP2RequestSettingsTest(const String& nonce)
{
    bool precedence = false;
    bool inheritance = false;
    bool model = false;
    bool context = false;
    bool projectOutput = false;
    bool requestOutput = false;
    bool autoCompact = false;
    bool noTools = false;
    bool tools = false;
    bool ui = false;
    bool cleanup = false;
    String error = "none";

    const String originalOutputOverrideChatId = requestOutputOverrideChatId;
    const std::uint32_t originalOutputOverrideTokens = requestOutputOverrideTokens;
    const String originalOverrideChatId = requestInstructionsOverrideChatId;
    std::string originalOverride = requestInstructionsOverride;
    const std::string originalRetryPrompt = retryPrompt;
    const String originalRetryChatId = retryChatId;
    const std::uint32_t originalRetryOutputTokens = retryOutputTokens;
    std::string originalRetry = retryRequestInstructions;
    clearChatScopedEphemeralState();

    if (!isP2UnicodeNonce(nonce)) {
        error = "invalid_nonce";
    } else {
        cardputer::Settings fixtureSettings = {};
        fixtureSettings.model = "p2-global-model";
        cardputer::ProjectDocument project = {};
        project.model = "p2-project-model";
        project.contextByteBudget = 65536;
        project.maximumOutputTokens = 4096;
        project.automaticCompaction = true;
        const cardputer::ChatDocument chat = {};
        const cardputer::ResolvedProjectRequestPolicy projectPolicy =
            cardputer::resolveProjectRequestPolicy(fixtureSettings, project, chat, 0);
        const cardputer::ResolvedProjectRequestPolicy requestPolicy =
            cardputer::resolveProjectRequestPolicy(
                fixtureSettings, project, chat, 8192);
        const cardputer::ChatRequestSerializationValidation projectValidation =
            cardputer::validateChatRequestSerialization(
                fixtureSettings, projectPolicy, "p2-global-marker",
                "p2-project-marker", "p2-chat-marker", "p2-request-marker",
                "p2-summary-marker");
        const cardputer::ChatRequestSerializationValidation requestValidation =
            cardputer::validateChatRequestSerialization(
                fixtureSettings, requestPolicy, "p2-global-marker",
                "p2-project-marker", "p2-chat-marker", "p2-request-marker",
                "p2-summary-marker");
        precedence = requestValidation.precedence;
        inheritance = requestValidation.inheritance;
        model = projectPolicy.model == project.model &&
            requestPolicy.model == project.model &&
            projectValidation.model && requestValidation.model;
        context = projectPolicy.contextByteBudget == 65536 &&
            requestPolicy.contextByteBudget == 65536;
        projectOutput = projectPolicy.maximumOutputTokens == 4096 &&
            projectValidation.outputTokens;
        requestOutput = requestPolicy.maximumOutputTokens == 8192 &&
            requestValidation.outputTokens;
        cardputer::ResolvedProjectRequestPolicy disabledPolicy = requestPolicy;
        disabledPolicy.automaticCompaction = false;
        autoCompact = !cardputer::shouldAutomaticallyCompactRequest(
                requestPolicy, 0) &&
            cardputer::shouldAutomaticallyCompactRequest(requestPolicy, 1) &&
            !cardputer::shouldAutomaticallyCompactRequest(disabledPolicy, 1);
        noTools = projectValidation.noTools && requestValidation.noTools;
        tools = projectValidation.tools && requestValidation.tools;

        const String ownedChatId = "p2req" + nonce;
        const std::string ownedInstructions(
            cardputer::kMaximumRequestInstructionsBytes, 'r');
        const cardputer::OperationResult staged = setRequestInstructionsOverride(
            ownedChatId, ownedInstructions);
        const std::string wrongChat = consumeRequestInstructionsOverride(
            ownedChatId + "x");
        const std::string consumed = consumeRequestInstructionsOverride(ownedChatId);
        const std::string consumedAgain = consumeRequestInstructionsOverride(ownedChatId);
        const cardputer::OperationResult oversized = setRequestInstructionsOverride(
            ownedChatId,
            std::string(cardputer::kMaximumRequestInstructionsBytes + 1, 'x'));
        const cardputer::OperationResult invalidUtf8 = setRequestInstructionsOverride(
            ownedChatId, std::string("\xC3\x28", 2));
        ui = staged.success && wrongChat.empty() && consumed == ownedInstructions &&
            consumedAgain.empty() && !oversized.success && !invalidUtf8.success &&
            requestInstructionsOverrideChatId.isEmpty() &&
            requestInstructionsOverride.empty();
        requestOutputOverrideChatId = ownedChatId;
        requestOutputOverrideTokens = 8192;
        retryPrompt = "p2-retry-prompt";
        retryChatId = ownedChatId;
        retryOutputTokens = 8192;
        retryRequestInstructions = ownedInstructions;
        clearChatScopedEphemeralState();
        clearChatScopedEphemeralState();
        cleanup = requestOutputOverrideChatId.isEmpty() &&
            requestOutputOverrideTokens == 0 &&
            requestInstructionsOverrideChatId.isEmpty() &&
            requestInstructionsOverride.empty() && retryPrompt.empty() &&
            retryChatId.isEmpty() && retryOutputTokens == 0 &&
            retryRequestInstructions.empty();
        if (!(precedence && inheritance && model && context && projectOutput &&
              requestOutput && autoCompact && noTools && tools && ui && cleanup)) {
            error = "request_contract";
        }
    }

    requestOutputOverrideChatId = originalOutputOverrideChatId;
    requestOutputOverrideTokens = originalOutputOverrideTokens;
    requestInstructionsOverrideChatId = originalOverrideChatId;
    requestInstructionsOverride = std::move(originalOverride);
    retryPrompt = originalRetryPrompt;
    retryChatId = originalRetryChatId;
    retryOutputTokens = originalRetryOutputTokens;
    retryRequestInstructions = std::move(originalRetry);
    const bool passed = precedence && inheritance && model && context &&
        projectOutput && requestOutput && autoCompact && noTools && tools && ui &&
        cleanup;
    Serial.printf(
        "P2REQUESTSETTINGSTEST result=%s nonce=%s precedence=%s inheritance=%s model=%s context=%s project_output=%s request_output=%s auto_compact=%s no_tools=%s tools=%s ui=%s cleanup=%s error=%s\n",
        passed ? "pass" : "failed", nonce.c_str(),
        precedence ? "pass" : "failed", inheritance ? "pass" : "failed",
        model ? "pass" : "failed", context ? "pass" : "failed",
        projectOutput ? "pass" : "failed", requestOutput ? "pass" : "failed",
        autoCompact ? "pass" : "failed", noTools ? "pass" : "failed",
        tools ? "pass" : "failed", ui ? "pass" : "failed",
        cleanup ? "pass" : "failed", passed ? "none" : error.c_str());
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
    source.toolPolicy = cardputer::setLegacySshToolsEnabled(
        diagnosticScopedToolPolicy(), true);
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
                           loaded.chat.toolPolicy != source.toolPolicy)) {
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
                           duplicated.chat.toolPolicy != source.toolPolicy)) {
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
    const cardputer::ScopedToolPermissionPolicy importedPolicy =
        cardputer::setLegacySshToolsEnabled(source.toolPolicy, false);
    if (result.success && (!imported.success || imported.chat.messages.size() != 2 ||
                           imported.chat.summary.archivedMessageCount != 2 ||
                           imported.chat.instructions != "Be concise." ||
                           imported.chat.toolPolicy != importedPolicy)) {
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

constexpr std::size_t kWorkspaceScaleFileCount = 500;
constexpr char kWorkspaceScaleContent[] = "P2-WORKSPACE-SCALE\n";

bool isValidWorkspaceScaleNonce(const String& nonce)
{
    if (nonce.isEmpty() || nonce.length() > 20) {
        return false;
    }
    for (std::size_t index = 0; index < nonce.length(); ++index) {
        if (nonce[index] < '0' || nonce[index] > '9') {
            return false;
        }
    }
    return true;
}

String workspaceScaleFileName(const String& nonce, std::size_t index)
{
    char suffix[16];
    std::snprintf(suffix, sizeof(suffix), "%03u.txt", static_cast<unsigned int>(index));
    return String("cardmind_p2_17_") + nonce + "_" + suffix;
}

bool shouldReportWorkspaceScaleCheckpoint(std::size_t count)
{
    return count == 1 || count == 32 || count == 64 || count == 100 ||
        count == 200 || count == 300 || count == 400 || count == 500;
}

void printWorkspaceScaleCheckpoint(const char* command,
                                   const String& nonce,
                                   std::size_t count)
{
    Serial.printf(
        "%s checkpoint=%u nonce=%s heap=%u largest_heap=%u\n",
        command,
        static_cast<unsigned int>(count),
        nonce.c_str(),
        static_cast<unsigned int>(ESP.getFreeHeap()),
        static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    Serial.flush();
}

void runWorkspaceScaleSetup(const String& nonce)
{
    constexpr const char* kCommand = "P2FILESCALESETUP";
    if (!isValidWorkspaceScaleNonce(nonce)) {
        Serial.printf(
            "%s result=failed nonce=%s stage=validation files=0 error=nonce_must_be_1_to_20_digits\n",
            kCommand, nonce.c_str());
        return;
    }
    if (!fileWorkspaceReady) {
        Serial.printf(
            "%s result=failed nonce=%s stage=storage files=0 error=workspace_unavailable\n",
            kCommand, nonce.c_str());
        return;
    }

    const std::uint32_t heapBefore = ESP.getFreeHeap();
    const std::uint32_t largestHeapBefore =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    printWorkspaceScaleCheckpoint(kCommand, nonce, 0);
    for (std::size_t index = 0; index < kWorkspaceScaleFileCount; ++index) {
        const String name = workspaceScaleFileName(nonce, index);
        const String target = cardputer::workspaceFilePath(name);
        if (SD.exists(target) || SD.exists(target + ".tmp") || SD.exists(target + ".bak")) {
            const String safeError = serialSafeError(
                "Reserved workspace scale path exists", 180);
            Serial.printf(
                "%s result=failed nonce=%s stage=collision files=0 failed_index=%u heap_before=%u heap_after=%u largest_before=%u largest_after=%u error=%s\n",
                kCommand,
                nonce.c_str(),
                static_cast<unsigned int>(index),
                static_cast<unsigned int>(heapBefore),
                static_cast<unsigned int>(ESP.getFreeHeap()),
                static_cast<unsigned int>(largestHeapBefore),
                static_cast<unsigned int>(
                    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                safeError.c_str());
            return;
        }
    }

    std::size_t created = 0;
    std::size_t failedIndex = kWorkspaceScaleFileCount;
    String failureStage;
    String failureError;
    for (std::size_t index = 0; index < kWorkspaceScaleFileCount; ++index) {
        const String name = workspaceScaleFileName(nonce, index);
        const String target = cardputer::workspaceFilePath(name);
        File file = SD.open(target, FILE_WRITE);
        if (!file) {
            failedIndex = index;
            failureStage = "write";
            failureError = "Failed to create workspace scale fixture";
            break;
        }
        const std::size_t written = file.write(
            reinterpret_cast<const std::uint8_t*>(kWorkspaceScaleContent),
            sizeof(kWorkspaceScaleContent) - 1);
        file.flush();
        file.close();
        if (written != sizeof(kWorkspaceScaleContent) - 1) {
            failedIndex = index;
            failureStage = "write";
            failureError = "Workspace scale fixture write was incomplete";
            break;
        }
        ++created;
        if (shouldReportWorkspaceScaleCheckpoint(created)) {
            printWorkspaceScaleCheckpoint(kCommand, nonce, created);
        }
    }

    const bool passed = created == kWorkspaceScaleFileCount && failureError.isEmpty();
    const String safeError = passed ? String("none") : serialSafeError(failureError, 180);
    Serial.printf(
        "%s result=%s nonce=%s stage=%s files=%u failed_index=%u heap_before=%u heap_after=%u largest_before=%u largest_after=%u error=%s\n",
        kCommand,
        passed ? "pass" : "failed",
        nonce.c_str(),
        passed ? "complete" : failureStage.c_str(),
        static_cast<unsigned int>(created),
        static_cast<unsigned int>(failedIndex),
        static_cast<unsigned int>(heapBefore),
        static_cast<unsigned int>(ESP.getFreeHeap()),
        static_cast<unsigned int>(largestHeapBefore),
        static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
        safeError.c_str());
}

void runWorkspaceToolPaginationTest(const String& nonce)
{
    constexpr const char* kCommand = "P2FILETOOLPAGETEST";
    if (!isValidWorkspaceScaleNonce(nonce)) {
        Serial.printf(
            "%s result=failed nonce=%s pages=0 files=0 compatibility=failed malformed=failed heap=failed error=nonce_must_be_1_to_20_digits\n",
            kCommand, nonce.c_str());
        return;
    }
    const String fixturePrefix = String("cardmind_p2_17_") + nonce + "_";
    std::vector<bool> seen(kWorkspaceScaleFileCount, false);
    std::uint32_t offset = 0;
    std::size_t pageCount = 0;
    std::size_t fixtureCount = 0;
    bool eof = false;
    String failure;
    const std::uint32_t heapBefore = ESP.getFreeHeap();
    const std::uint32_t largestBefore =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const bool compatibilityPassed = []() {
        const cardputer::ToolExecutionResult compatible = cardputer::executeWorkspaceTool(
            {"p2-file-tool-compatible", "list_files", "{}"});
        JsonDocument document;
        const DeserializationError error = deserializeJson(document, compatible.output);
        return compatible.success && !error && document["ok"].is<bool>() &&
            document["ok"].as<bool>() && document["files"].is<JsonArrayConst>() &&
            document["next_offset"].is<std::uint32_t>() &&
            document["eof"].is<bool>();
    }();
    while (failure.isEmpty() && !eof && pageCount < 1024) {
        const std::string arguments = std::string("{\"offset\":") +
            std::to_string(offset) + ",\"max_entries\":16}";
        const cardputer::ToolExecutionResult listed = cardputer::executeWorkspaceTool(
            {"p2-file-tool-page", "list_files", arguments});
        if (!listed.success) {
            failure = listed.error;
            break;
        }
        JsonDocument document;
        const DeserializationError error = deserializeJson(document, listed.output);
        if (error || !document["ok"].is<bool>() || !document["ok"].as<bool>() ||
            !document["files"].is<JsonArrayConst>() ||
            !document["next_offset"].is<std::uint32_t>() ||
            !document["eof"].is<bool>()) {
            failure = "list_files returned an invalid pagination document";
            break;
        }
        for (const JsonObjectConst file : document["files"].as<JsonArrayConst>()) {
            if (!file["name"].is<const char*>() || !file["bytes"].is<std::uint32_t>()) {
                failure = "list_files returned an invalid file entry";
                break;
            }
            const String name = file["name"].as<const char*>();
            if (!name.startsWith(fixturePrefix)) {
                continue;
            }
            const String suffix = name.substring(fixturePrefix.length());
            if (suffix.length() != 7 || suffix.substring(3) != ".txt" ||
                suffix[0] < '0' || suffix[0] > '9' ||
                suffix[1] < '0' || suffix[1] > '9' ||
                suffix[2] < '0' || suffix[2] > '9') {
                failure = "list_files returned a malformed fixture filename";
                break;
            }
            const std::size_t index =
                static_cast<std::size_t>(suffix[0] - '0') * 100U +
                static_cast<std::size_t>(suffix[1] - '0') * 10U +
                static_cast<std::size_t>(suffix[2] - '0');
            if (index >= seen.size() || seen[index] ||
                file["bytes"].as<std::uint32_t>() !=
                    sizeof(kWorkspaceScaleContent) - 1) {
                failure = "list_files returned duplicate or incorrect fixture metadata";
                break;
            }
            seen[index] = true;
            ++fixtureCount;
        }
        const std::uint32_t nextOffset = document["next_offset"].as<std::uint32_t>();
        eof = document["eof"].as<bool>();
        if (failure.isEmpty() && !eof && nextOffset <= offset) {
            failure = "list_files pagination did not advance";
        }
        offset = nextOffset;
        ++pageCount;
        delay(0);
    }
    if (failure.isEmpty() && !eof) {
        failure = "list_files pagination exceeded the diagnostic page guard";
    }
    if (failure.isEmpty() && fixtureCount != kWorkspaceScaleFileCount) {
        failure = "list_files did not return the complete 500-file fixture";
    }
    const bool malformedPassed = []() {
        const cardputer::ToolExecutionResult negative = cardputer::executeWorkspaceTool(
            {"p2-file-tool-negative", "list_files", "{\"offset\":-1}"});
        const cardputer::ToolExecutionResult oversized = cardputer::executeWorkspaceTool(
            {"p2-file-tool-oversized", "list_files", "{\"max_entries\":17}"});
        const cardputer::ToolExecutionResult unknown = cardputer::executeWorkspaceTool(
            {"p2-file-tool-unknown", "list_files", "{\"unexpected\":1}"});
        return !negative.success && !oversized.success && !unknown.success;
    }();
    const std::uint32_t heapAfter = ESP.getFreeHeap();
    const std::uint32_t largestAfter =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    constexpr std::uint32_t kMaximumDiagnosticHeapDrift = 8192;
    const bool heapPassed = heapAfter + kMaximumDiagnosticHeapDrift >= heapBefore &&
        largestAfter + kMaximumDiagnosticHeapDrift >= largestBefore;
    const bool passed = failure.isEmpty() && compatibilityPassed &&
        malformedPassed && heapPassed;
    Serial.printf(
        "%s result=%s nonce=%s pages=%u files=%u compatibility=%s malformed=%s heap=%s heap_before=%u heap_after=%u largest_before=%u largest_after=%u error=%s\n",
        kCommand,
        passed ? "pass" : "failed",
        nonce.c_str(),
        static_cast<unsigned int>(pageCount),
        static_cast<unsigned int>(fixtureCount),
        compatibilityPassed ? "pass" : "failed",
        malformedPassed ? "pass" : "failed",
        heapPassed ? "pass" : "failed",
        static_cast<unsigned int>(heapBefore),
        static_cast<unsigned int>(heapAfter),
        static_cast<unsigned int>(largestBefore),
        static_cast<unsigned int>(largestAfter),
        passed ? "none" : serialSafeError(
            failure.isEmpty() ? String("list_files contract validation failed") : failure,
            180).c_str());
}

void runWorkspaceScaleCleanup(const String& nonce)
{
    constexpr const char* kCommand = "P2FILESCALECLEAN";
    if (!isValidWorkspaceScaleNonce(nonce)) {
        Serial.printf(
            "%s result=failed nonce=%s stage=validation removed=0 remaining=0 errors=1 error=nonce_must_be_1_to_20_digits\n",
            kCommand, nonce.c_str());
        return;
    }
    if (!fileWorkspaceReady) {
        Serial.printf(
            "%s result=failed nonce=%s stage=storage removed=0 remaining=500 errors=1 error=workspace_unavailable\n",
            kCommand, nonce.c_str());
        return;
    }

    const std::uint32_t heapBefore = ESP.getFreeHeap();
    const std::uint32_t largestHeapBefore =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    std::size_t removed = 0;
    std::size_t alreadyAbsent = 0;
    std::size_t errors = 0;
    String firstError;
    printWorkspaceScaleCheckpoint(kCommand, nonce, 0);
    for (std::size_t index = 0; index < kWorkspaceScaleFileCount; ++index) {
        const String name = workspaceScaleFileName(nonce, index);
        const String target = cardputer::workspaceFilePath(name);
        if (SD.exists(target + ".tmp") || SD.exists(target + ".bak")) {
            ++errors;
            if (firstError.isEmpty()) {
                firstError = String("Reserved recovery artifact exists for ") + name;
            }
        }
        if (!SD.exists(target)) {
            ++alreadyAbsent;
        } else {
            File file = SD.open(target, FILE_READ);
            char content[sizeof(kWorkspaceScaleContent)] = {};
            const std::size_t read = file
                ? file.read(reinterpret_cast<std::uint8_t*>(content),
                            sizeof(kWorkspaceScaleContent) - 1)
                : 0;
            const bool owned = file && file.size() == sizeof(kWorkspaceScaleContent) - 1 &&
                read == sizeof(kWorkspaceScaleContent) - 1 &&
                std::equal(content, content + sizeof(kWorkspaceScaleContent) - 1,
                           kWorkspaceScaleContent);
            if (file) {
                file.close();
            }
            if (!owned) {
                ++errors;
                if (firstError.isEmpty()) {
                    firstError = String("Refused to delete non-owned workspace scale file: ") +
                        name;
                }
            } else {
                const bool deleted = SD.remove(target);
                if (!deleted || SD.exists(target)) {
                    ++errors;
                    if (firstError.isEmpty()) {
                        firstError = deleted
                            ? String("Workspace scale file remained after deletion: ") + name
                            : String("Failed to delete workspace scale file: ") + name;
                    }
                } else {
                    ++removed;
                }
            }
        }
        const std::size_t processed = index + 1;
        if (processed % 50 == 0) {
            printWorkspaceScaleCheckpoint(kCommand, nonce, processed);
        }
    }

    const std::size_t remaining = errors;
    const bool passed = errors == 0 && remaining == 0;
    const String safeError = passed ? String("none") : serialSafeError(firstError, 180);
    Serial.printf(
        "%s result=%s nonce=%s removed=%u already_absent=%u remaining=%u errors=%u heap_before=%u heap_after=%u largest_before=%u largest_after=%u error=%s\n",
        kCommand,
        passed ? "pass" : "failed",
        nonce.c_str(),
        static_cast<unsigned int>(removed),
        static_cast<unsigned int>(alreadyAbsent),
        static_cast<unsigned int>(remaining),
        static_cast<unsigned int>(errors),
        static_cast<unsigned int>(heapBefore),
        static_cast<unsigned int>(ESP.getFreeHeap()),
        static_cast<unsigned int>(largestHeapBefore),
        static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
        safeError.c_str());
}

void runFileWorkspaceEditTest()
{
    constexpr std::size_t kWorkspaceFileTestBytes = 2U * 1024U * 1024U;
    constexpr std::uint32_t kWorkspaceFileEditOffset = 1024U * 1024U;
    const String sourceName = "firmware_editor_test.txt";
    const String copyName = "firmware_editor_copy.txt";
    const String renamedName = "firmware_editor_renamed.txt";
    const String replacementName = sourceName + ".tmp";
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
    const std::string completeReplacement = "complete UTF-8 replacement: файл\n";
    if (result.success) {
        Serial.println("FILETEST stage=replace_complete_file");
        File replacementFile = SD.open(replacementPath, FILE_WRITE);
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
    candidate.masterToolPolicy = diagnosticMasterToolPolicy();
    candidate.newChatToolPolicy = diagnosticScopedToolPolicy();
    cardputer::OperationResult result = saveAndApplyDeviceSettings(candidate);
    cardputer::Settings loaded;
    if (result.success) {
        result = cardputer::loadSettings(loaded);
    }
    if (result.success &&
        (loaded.displayBrightness != candidate.displayBrightness ||
         loaded.screenSleepMinutes != candidate.screenSleepMinutes ||
         loaded.keyboardRepeatMs != candidate.keyboardRepeatMs ||
         loaded.powerProfile != candidate.powerProfile ||
         loaded.masterToolPolicy != candidate.masterToolPolicy ||
         loaded.newChatToolPolicy != candidate.newChatToolPolicy ||
         getCpuFrequencyMhz() != 240)) {
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

void runToolApiTest()
{
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED || std::time(nullptr) < 1700000000) {
        Serial.println("TOOLTEST result=failed stage=network");
        return;
    }
    bool writeSucceeded = false;
    const String expectedName = "firmware_tool_" + String(millis()) + ".py";
    const String expectedPath = cardputer::workspaceFilePath(expectedName);
    const std::string expectedContent = "print('CARDMIND_TOOL_OK')\n";
    const cardputer::SharedFileLinkResult initialLink =
        cardputer::projectHasSharedFileLink(activeProjectId, expectedName);
    if (SD.exists(expectedPath) || !initialLink.success || initialLink.linked) {
        const String error = !initialLink.success
            ? initialLink.error : String("fixture collision");
        Serial.printf(
            "TOOLTEST result=failed stage=fixture api=failed write=failed file=failed link=failed cleanup=pass error=%s\n",
            serialSafeError(error, 120).c_str());
        return;
    }
    const std::string prompt =
        "Use write_file exactly once with JSON arguments name \"" +
        std::string(expectedName.c_str()) +
        "\" and content \"print('CARDMIND_TOOL_OK')\\n\". Do not answer until the tool result.";
    const std::vector<cardputer::Message> testHistory = {
        {"user", prompt},
    };
    cardputer::Settings requestSettings = settings;
    requestSettings.webSearchApiKey = "";
    requestSettings.masterToolPolicy =
        cardputer::defaultGlobalToolPermissionPolicy();
    cardputer::ProjectDocument project = {};
    project.toolPolicy = cardputer::inheritedToolPermissionPolicy();
    cardputer::ChatDocument chat = {};
    chat.toolPolicy = cardputer::inheritedToolPermissionPolicy();
    const std::uint8_t filesGroup = static_cast<std::uint8_t>(
        1U << static_cast<std::uint8_t>(cardputer::ToolCapabilityGroup::Files));
    const cardputer::ToolRequestPlan requestPlan =
        cardputer::resolveChatToolRequestPlan(
            requestSettings, project, chat,
            {cardputer::ToolMessageIntentMode::Required, filesGroup},
            true, true, true, false);
    const String planError = toolRequestPlanError(requestPlan);
    if (!planError.isEmpty()) {
        Serial.printf(
            "TOOLTEST result=failed stage=plan api=failed write=failed file=failed link=failed cleanup=pass error=%s\n",
            planError.c_str());
        return;
    }
    const cardputer::ChatResult result = cardputer::streamChatCompletionWithTools(
        requestSettings, testHistory, "", requestPlan, [](const std::string&) {},
        [&writeSucceeded, &expectedName, &expectedContent,
         &requestPlan](const cardputer::ToolCall& call) {
            JsonDocument arguments;
            const DeserializationError parseError =
                deserializeJson(arguments, call.arguments);
            const bool exactCall = call.name == "write_file" && !parseError &&
                arguments.size() == 2 &&
                arguments["name"].is<const char*>() &&
                arguments["content"].is<const char*>() &&
                String(arguments["name"].as<const char*>()) == expectedName &&
                std::string(arguments["content"].as<const char*>()) ==
                    expectedContent;
            if (!exactCall) {
                return cardputer::ToolExecutionResult{
                    false,
                    "{\"ok\":false,\"error\":\"unexpected diagnostic tool call\"}",
                    "TOOLTEST rejected a non-owned tool call",
                };
            }
            const cardputer::ToolExecutionResult execution =
                cardputer::routeProjectToolCall(
                    settings, requestPlan, activeProjectId, call,
                    []() { return false; });
            writeSucceeded = execution.success;
            return execution;
        },
        [](const cardputer::PendingToolContinuation&) {
            return cardputer::OperationResult{
                false, "TOOLTEST did not authorize confirmation"};
        }, []() { return false; });
    const bool fileCreated = SD.exists(expectedPath);
    const cardputer::SharedFileLinkResult linked =
        cardputer::projectHasSharedFileLink(activeProjectId, expectedName);
    cardputer::OperationResult cleanup = {true, ""};
    if (linked.success && linked.linked) {
        cleanup = cardputer::unlinkSharedFileFromProject(
            activeProjectId, expectedName);
    }
    if (cleanup.success && SD.exists(expectedPath)) {
        cleanup = cardputer::deleteWorkspaceFile(expectedName);
    }
    const cardputer::SharedFileLinkResult afterCleanup =
        cardputer::projectHasSharedFileLink(activeProjectId, expectedName);
    cardputer::OperationResult repeatedCleanup = afterCleanup.success
        ? cardputer::OperationResult{true, ""}
        : cardputer::OperationResult{false, afterCleanup.error};
    if (repeatedCleanup.success && afterCleanup.linked) {
        repeatedCleanup = cardputer::unlinkSharedFileFromProject(
            activeProjectId, expectedName);
    }
    if (repeatedCleanup.success && SD.exists(expectedPath)) {
        repeatedCleanup = cardputer::deleteWorkspaceFile(expectedName);
    }
    const cardputer::SharedFileLinkResult finalLink =
        cardputer::projectHasSharedFileLink(activeProjectId, expectedName);
    const bool cleanupVerified = cleanup.success && repeatedCleanup.success &&
        finalLink.success && !finalLink.linked && !SD.exists(expectedPath);
    if (!result.success || !writeSucceeded || !fileCreated || !linked.success ||
        !linked.linked || !cleanupVerified) {
        Serial.printf("TOOLTEST result=failed stage=tool_roundtrip api=%s write=%s file=%s link=%s cleanup=%s error=%s\n",
                      result.success ? "pass" : "failed",
                      writeSucceeded ? "pass" : "failed",
                      fileCreated ? "pass" : "failed",
                      linked.success && linked.linked ? "pass" : "failed",
                      cleanupVerified ? "pass" : "failed",
                      !result.error.isEmpty() ? result.error.c_str() :
                          (!linked.success ? linked.error.c_str() :
                           (!cleanup.success ? cleanup.error.c_str() :
                            (!repeatedCleanup.success ? repeatedCleanup.error.c_str() :
                             finalLink.error.c_str()))));
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
                            0,
                            {cardputer::ChatCapabilityState::Inherit,
                             cardputer::ChatCapabilityState::Inherit,
                             cardputer::ChatCapabilityState::Inherit,
                             cardputer::ChatCapabilityState::Inherit},
                            true, batteryLevel, batteryCharging);
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

cardputer::OperationResult runSftpTransferRemoteTest(bool& cleanupComplete);

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
    if (command == "P2SDFAULTMISSING" || command == "P2SDFAULTFULL" ||
        command == "P2SDFAULTREMOVED" || command == "P2SDFAULTREPLACED" ||
        command == "P2SDFAULTCLEAR") {
        if (command == "P2SDFAULTMISSING") {
            cardputer::setSdStorageFaultOverrideForDiagnostics(
                cardputer::SdStorageState::Missing);
        } else if (command == "P2SDFAULTFULL") {
            cardputer::setSdStorageFaultOverrideForDiagnostics(
                cardputer::SdStorageState::Full);
        } else if (command == "P2SDFAULTREMOVED") {
            cardputer::setSdStorageFaultOverrideForDiagnostics(
                cardputer::SdStorageState::Removed);
        } else if (command == "P2SDFAULTREPLACED") {
            cardputer::setSdStorageFaultOverrideForDiagnostics(
                cardputer::SdStorageState::Replaced);
        } else {
            cardputer::clearSdStorageFaultOverrideForDiagnostics();
        }
        refreshRuntimeSdState();
        const cardputer::OperationResult read = cardputer::requireSdReadAccess();
        const cardputer::OperationResult write = cardputer::requireSdWriteAccess(
            1, cardputer::kStorageOperationalFloorBytes);
        Serial.printf(
            "P2SDFAULT result=pass command=%s state=%s error=%s read=%s write=%s\n",
            command.c_str(),
            cardputer::sdStorageStateName(currentSdStorageStatus.state),
            cardputer::sdStorageErrorCode(currentSdStorageStatus.state),
            read.success ? "pass" : "rejected",
            write.success ? "pass" : "rejected");
        return;
    }
    if (command == "SDMOUNTTEST") {
        SD.end();
        SPI.end();
        const cardputer::OperationResult result = cardputer::initializeVoiceStorage();
        Serial.printf(
            "SDMOUNTTEST result=%s card_type=%u total_bytes=%llu used_bytes=%llu error=%s\n",
            result.success ? "pass" : "failed",
            static_cast<unsigned int>(SD.cardType()),
            static_cast<unsigned long long>(SD.totalBytes()),
            static_cast<unsigned long long>(SD.usedBytes()),
            result.success ? "none" : serialSafeError(result.error, 180).c_str());
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
    if (command == "HOTFIXNAVTEST") {
        runHotfixNavigationLatencyTest();
        return;
    }
    if (command == "HOTFIXINPUTTEST") {
        runHotfixInputLatencyTest();
        return;
    }
    if (command == "HOTFIXSDTEST") {
        runHotfixSdAccessSafetyTest();
        return;
    }
    if (command == "PROJECTSCHEMATEST") {
        runProjectSchemaTest();
        return;
    }
    if (command == "MIGRATIONTEST") {
        runProjectMigrationTest();
        return;
    }
    if (command == "MIGRATIONRECOVERYTEST") {
        runProjectMigrationRecoveryTest();
        return;
    }
    if (command == "PROJECTPARITYTEST") {
        runProjectParityTest();
        return;
    }
    const String p2SharedTestPrefix = "P2SHAREDTEST";
    if (command.startsWith(p2SharedTestPrefix)) {
        runP2SharedProjectIsolationTest(command.substring(p2SharedTestPrefix.length()));
        return;
    }
    const String p2SharedCleanupPrefix = "P2SHAREDCLEAN";
    if (command.startsWith(p2SharedCleanupPrefix)) {
        runP2SharedCleanup(command.substring(p2SharedCleanupPrefix.length()));
        return;
    }
    if (command == "RETRYPERSISTENCETEST") {
        runRetryPersistenceTest();
        return;
    }
    if (command == "COMPACTIONTEST") {
        runCompactionPersistenceTest();
        return;
    }
    const String p2ArchiveTestPrefix = "P2ARCHIVETEST";
    if (command.startsWith(p2ArchiveTestPrefix)) {
        runP2ArchiveTest(command.substring(p2ArchiveTestPrefix.length()));
        return;
    }
    const String p2BinaryTestPrefix = "P2BINARYTEST";
    if (command.startsWith(p2BinaryTestPrefix)) {
        runP2BinaryFileTest(command.substring(p2BinaryTestPrefix.length()));
        return;
    }
    const String p2SummaryTestPrefix = "P2SUMMARYTEST";
    if (command.startsWith(p2SummaryTestPrefix)) {
        runP2SummaryTest(command.substring(p2SummaryTestPrefix.length()));
        return;
    }
    if (command == "P2LIMITTEST") {
        runPhaseTwoLimitTest();
        return;
    }
    if (command == "PROJECTCHATTEST") {
        runProjectChatIsolationTest();
        return;
    }
    const String p2RequestSettingsPrefix = "P2REQUESTSETTINGSTEST";
    if (command.startsWith(p2RequestSettingsPrefix)) {
        runP2RequestSettingsTest(command.substring(p2RequestSettingsPrefix.length()));
        return;
    }
    if (command == "INSTRUCTIONTEST") {
        runInstructionPrecedenceTest();
        return;
    }
    if (command == "CHATQOLTEST") {
        runChatQolTest();
        return;
    }
    const String p2UnicodeSetupPrefix = "P2UNICODESETUP";
    if (command.startsWith(p2UnicodeSetupPrefix)) {
        runP2UnicodeSetup(command.substring(p2UnicodeSetupPrefix.length()));
        return;
    }
    const String p2UnicodeCleanupPrefix = "P2UNICODECLEAN";
    if (command.startsWith(p2UnicodeCleanupPrefix)) {
        runP2UnicodeCleanup(command.substring(p2UnicodeCleanupPrefix.length()));
        return;
    }
    const String p2LargeSetupPrefix = "P2LARGESETUP";
    if (command.startsWith(p2LargeSetupPrefix)) {
        runP2LargeSetup(command.substring(p2LargeSetupPrefix.length()));
        return;
    }
    const String p2LargeVerifyPrefix = "P2LARGEVERIFY";
    if (command.startsWith(p2LargeVerifyPrefix)) {
        runP2LargeVerify(command.substring(p2LargeVerifyPrefix.length()));
        return;
    }
    const String p2LargeCleanupPrefix = "P2LARGECLEAN";
    if (command.startsWith(p2LargeCleanupPrefix)) {
        runP2LargeCleanup(command.substring(p2LargeCleanupPrefix.length()));
        return;
    }
    const String p2AtomicSetupPrefix = "P2ATOMICSETUP";
    if (command.startsWith(p2AtomicSetupPrefix)) {
        runP2AtomicSetup(command.substring(p2AtomicSetupPrefix.length()));
        return;
    }
    const String p2AtomicCleanupPrefix = "P2ATOMICCLEAN";
    if (command.startsWith(p2AtomicCleanupPrefix)) {
        runP2AtomicCleanup(command.substring(p2AtomicCleanupPrefix.length()));
        return;
    }
    const String workspaceScaleSetupPrefix = "P2FILESCALESETUP";
    if (command.startsWith(workspaceScaleSetupPrefix)) {
        runWorkspaceScaleSetup(command.substring(workspaceScaleSetupPrefix.length()));
        return;
    }
    const String workspaceToolPagePrefix = "P2FILETOOLPAGETEST";
    if (command.startsWith(workspaceToolPagePrefix)) {
        runWorkspaceToolPaginationTest(command.substring(workspaceToolPagePrefix.length()));
        return;
    }
    const String workspaceScaleCleanupPrefix = "P2FILESCALECLEAN";
    if (command.startsWith(workspaceScaleCleanupPrefix)) {
        runWorkspaceScaleCleanup(command.substring(workspaceScaleCleanupPrefix.length()));
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
    if (command == "SSHOPTIONSTEST") {
        cardputer::markOperation("ssh_options_test");
        const cardputer::OperationResult result = runSshCommandOptionsTest();
        cardputer::markOperation("idle");
        Serial.printf(
            "SSHOPTIONSTEST result=%s heap=%u largest_heap=%u stack_free=%u error=%s\n",
            result.success ? "pass" : "failed",
            static_cast<unsigned int>(ESP.getFreeHeap()),
            static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
            static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)),
            result.success ? "none" : result.error.c_str());
        return;
    }
    if (command == "SSHOUTPUTTEST") {
        cardputer::markOperation("ssh_output_test");
        const cardputer::OperationResult result =
            runSshCommandOutputStorageTest();
        cardputer::markOperation("idle");
        Serial.printf(
            "SSHOUTPUTTEST result=%s heap=%u largest_heap=%u stack_free=%u error=%s\n",
            result.success ? "pass" : "failed",
            static_cast<unsigned int>(ESP.getFreeHeap()),
            static_cast<unsigned int>(
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
            static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)),
            result.success ? "none" : result.error.c_str());
        return;
    }
    if (command == "SSHOUTPUTE2E") {
        ensureNetworkReady();
        cardputer::markOperation("ssh_output_e2e");
        String retainedName;
        std::uint32_t outputBytes = 0;
        const cardputer::OperationResult result =
            runSshCommandOutputRemoteTest(retainedName, outputBytes);
        cardputer::markOperation("idle");
        Serial.printf(
            "SSHOUTPUTE2E result=%s log=%s output_bytes=%u heap=%u largest_heap=%u stack_free=%u error=%s\n",
            result.success ? "pass" : "failed",
            retainedName.isEmpty() ? "none" : retainedName.c_str(),
            static_cast<unsigned int>(outputBytes),
            static_cast<unsigned int>(ESP.getFreeHeap()),
            static_cast<unsigned int>(
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
            static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)),
            result.success ? "none" : result.error.c_str());
        return;
    }
    if (command == "SSHOUTPUTCLEAN") {
        bool alreadyAbsent = false;
        bool removed = false;
        const cardputer::OperationResult result =
            cleanupSshCommandOutputRemoteTest(alreadyAbsent, removed);
        Serial.printf(
            "SSHOUTPUTCLEAN result=%s already_absent=%s removed=%s error=%s\n",
            result.success ? "pass" : "failed",
            alreadyAbsent ? "yes" : "no",
            removed ? "yes" : "no",
            result.success ? "none" : result.error.c_str());
        return;
    }
    if (command == "MODELSFTPTEST") {
        ensureNetworkReady();
        cardputer::markOperation("model_sftp_test");
        const std::uint32_t startedAt = millis();
        bool cleanupComplete = false;
        const cardputer::OperationResult result =
            runModelSftpRemoteTest(cleanupComplete);
        const std::uint32_t elapsedMs = millis() - startedAt;
        cardputer::markOperation("idle");
        Serial.printf(
            "MODELSFTPTEST result=%s elapsed_ms=%u heap=%u minimum_heap=%u largest_heap=%u stack_free=%u cleanup=%s error=%s\n",
            result.success ? "pass" : "failed",
            static_cast<unsigned int>(elapsedMs),
            static_cast<unsigned int>(ESP.getFreeHeap()),
            static_cast<unsigned int>(ESP.getMinFreeHeap()),
            static_cast<unsigned int>(
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
            static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)),
            cleanupComplete ? "yes" : "no",
            result.success ? "none" : result.error.c_str());
        return;
    }
    if (command == "SFTPTRANSFERTEST") {
        ensureNetworkReady();
        cardputer::markOperation("sftp_transfer_test");
        const std::uint32_t startedAt = millis();
        bool cleanupComplete = false;
        const cardputer::OperationResult result =
            runSftpTransferRemoteTest(cleanupComplete);
        const std::uint32_t elapsedMs = millis() - startedAt;
        cardputer::markOperation("idle");
        Serial.printf(
            "SFTPTRANSFERTEST result=%s elapsed_ms=%u heap=%u minimum_heap=%u largest_heap=%u stack_free=%u cleanup=%s error=%s\n",
            result.success ? "pass" : "failed",
            static_cast<unsigned int>(elapsedMs),
            static_cast<unsigned int>(ESP.getFreeHeap()),
            static_cast<unsigned int>(ESP.getMinFreeHeap()),
            static_cast<unsigned int>(
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
            static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr)),
            cleanupComplete ? "yes" : "no",
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
