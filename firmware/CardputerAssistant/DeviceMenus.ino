namespace {

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

std::vector<String> aiMenuItems()
{
    return {
        "Default model: " + settings.model,
        "API and services setup",
        settings.globalInstructions.isEmpty()
            ? String("Global instructions: OFF")
            : String("Global instructions: ON"),
        "Master access",
        "Defaults for new chats",
        "Pending confirmations",
        "Activity",
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

String projectChatHistoryQuotaLabel(std::uint32_t quotaBytes)
{
    return quotaBytes == 0
        ? String("SD available")
        : String(quotaBytes / (1024U * 1024U)) + " MiB";
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
        "Chat archive quota: " +
            projectChatHistoryQuotaLabel(settings.projectChatHistoryQuotaBytes),
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

String resetReasonLabel()
{
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "Power on";
        case ESP_RST_EXT: return "External reset";
        case ESP_RST_SW: return "Software restart";
        case ESP_RST_PANIC: return "Software panic";
        case ESP_RST_INT_WDT: return "Interrupt watchdog";
        case ESP_RST_TASK_WDT: return "Task watchdog";
        case ESP_RST_WDT: return "Watchdog";
        case ESP_RST_DEEPSLEEP: return "Deep sleep wake";
        case ESP_RST_BROWNOUT: return "Brownout";
        case ESP_RST_SDIO: return "SDIO reset";
        default: return "Unknown (" + String(static_cast<int>(esp_reset_reason())) + ")";
    }
}

std::vector<String> workspaceFileItems()
{
    const String importLabel = workspaceListMode == WorkspaceListMode::ImportProject
        ? String("Choose a project bundle below")
        : String("Choose a chat bundle below");
    std::vector<String> items = {
        workspaceListMode != WorkspaceListMode::Browse
            ? importLabel
            : String("+ New text file")};
    items.reserve(workspaceFiles.size() + 3);
    for (const auto& file : workspaceFiles) {
        items.push_back((file.directory ? String("[DIR] ") : String()) +
                        file.name + (file.directory ? String() :
                        "  " + String(file.size) + " B"));
    }
    if (workspacePageOffset > 0) {
        items.push_back("< Previous entries");
    }
    if (!workspacePageEof) {
        items.push_back("Next entries >");
    }
    return items;
}

std::vector<String> fileActionItems()
{
    const cardputer::SharedFileLinkResult linked =
        cardputer::projectHasSharedFileLink(activeProjectId, fileViewerName);
    const String linkAction = linked.success && linked.linked
        ? String("Unlink from active project")
        : String("Link to active project");
    if (!cardputer::isWorkspaceTextFile(std::string(fileViewerName.c_str()))) {
        return {
            "Save copy as...",
            "Rename...",
            linkAction,
            "Delete file",
            "Back",
        };
    }
    const String mode = cardputer::documentReaderModeLabel(fileReaderMode).c_str();
    return {
        "View as " + mode,
        "Edit visible section",
        "Read visible section",
        "Read selected lines...",
        "Read entire document",
        "Find text...",
        "Find next",
        "Save bookmark here",
        "Open bookmark",
        "Save copy as...",
        "Rename...",
        linkAction,
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
        {"CONTEXTS", "PROJECTS", "Chats · Files · Instructions", 0x2F1C, cardputer::CarouselIcon::Chats},
        {"MODELS & TOOLS", "AI", settings.model, 0xA23F, cardputer::CarouselIcon::Ai},
        {"SPEECH", "VOICE", "STT · TTS · Volume", 0xFD20, cardputer::CarouselIcon::Voice},
        {"CONNECTIVITY", "NETWORK", networkSubtitle, 0xB7E6, cardputer::CarouselIcon::Network},
        {"WORKSPACE", "FILES", "Edit · Read · Export", 0x4DFF, cardputer::CarouselIcon::Files},
        {"BROWSER CONTROL", "WEB CONSOLE", "Chat · Files · Terminal", 0xFB4D, cardputer::CarouselIcon::Web},
        {"SYSTEM", "DEVICE", "Settings · API · Update", 0xFFE0, cardputer::CarouselIcon::Device},
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

std::uint32_t nextProjectChatHistoryQuotaBytes(std::uint32_t currentQuotaBytes)
{
    constexpr std::uint32_t kMebibyte = 1024U * 1024U;
    if (currentQuotaBytes == 0) {
        return 16U * kMebibyte;
    }
    if (currentQuotaBytes < 16U * kMebibyte) {
        return 16U * kMebibyte;
    }
    if (currentQuotaBytes < 64U * kMebibyte) {
        return 64U * kMebibyte;
    }
    if (currentQuotaBytes < 256U * kMebibyte) {
        return 256U * kMebibyte;
    }
    if (currentQuotaBytes < 1024U * kMebibyte) {
        return 1024U * kMebibyte;
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
        "FN+2  Next capabilities",
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

void renderAiMenu()
{
    cardputer::showSelectionList("AI", aiMenuItems(), aiMenuIndex,
                                 menuStatus.isEmpty() ? "UP/DOWN  ENTER  ESC back" : menuStatus);
}

void renderToolActivity()
{
    const String position = toolActivityLines.empty()
        ? String("0/0")
        : String(toolActivityFirstLine + 1) + "/" + String(toolActivityLines.size());
    cardputer::showReadOnlyTextViewer(
        "TOOL ACTIVITY", toolActivityLines, toolActivityFirstLine, position);
}

void openToolActivity()
{
    const cardputer::ToolActivitiesResult loaded =
        cardputer::loadRecentToolActivities();
    if (!loaded.success) {
        menuStatus = loaded.error;
        renderAiMenu();
        return;
    }
    toolActivityLines.clear();
    for (const cardputer::ToolActivityRecord& activity : loaded.activities) {
        toolActivityLines.push_back(
            std::string(activity.tool.c_str()) + " · " +
            cardputer::toolActivityStatusName(activity.status));
        toolActivityLines.push_back(
            std::string(cardputer::toolActivityTargetName(activity.target)) +
            " · " + std::to_string(activity.durationMs) + " ms");
        String result = String(activity.outputBytes) + " B";
        if (activity.exitStatus.present) {
            result += " · exit " + String(activity.exitStatus.value);
        }
        toolActivityLines.push_back(result.c_str());
    }
    if (toolActivityLines.empty()) {
        toolActivityLines.push_back("No tool activity yet");
    }
    toolActivityFirstLine = 0;
    currentScreen = Screen::ToolActivity;
    renderToolActivity();
}

void renderGlobalInstructions()
{
    cardputer::showTextEditor(
        "GLOBAL INSTRUCTIONS", globalInstructionsInput, keyboardLayout,
        cardputer::kMaximumChatInstructionsBytes, globalInstructionsStatus,
        "Applied to every chat",
        "ENTER save  FN+DEL clear  ESC back");
}

void renderGlobalMasterPolicy()
{
    cardputer::showSelectionList(
        "MASTER ACCESS", capabilityPolicyItems(settings.masterToolPolicy),
        capabilityPolicyIndex,
        menuStatus.isEmpty() ? String("ENTER change  ESC AI") : menuStatus);
}

void renderNewChatDefaultsPolicy()
{
    cardputer::showSelectionList(
        "NEW CHAT DEFAULTS", capabilityPolicyItems(settings.newChatToolPolicy),
        capabilityPolicyIndex,
        menuStatus.isEmpty() ? String("ENTER change  ESC AI") : menuStatus);
}

void renderRequestInstructions()
{
    cardputer::showTextEditor(
        "NEXT REQUEST INSTRUCTIONS", requestInstructionsInput, keyboardLayout,
        cardputer::kMaximumRequestInstructionsBytes, requestInstructionsStatus,
        "Overrides chat instructions once",
        "ENTER use once  FN+DEL clear  ESC back");
}

void openAiMenu()
{
    aiMenuIndex = 0;
    menuStatus = "";
    currentScreen = Screen::AiMenu;
    renderAiMenu();
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
    const String previousWifiSsid = settings.wifiSsid;
    const String previousWifiPassword = settings.wifiPassword;
    const cardputer::WebConsoleResult result = cardputer::runWebConsole(
        settings, activeChatId, kFirmwareVersion);
    cardputer::markOperation("idle");
    if (!result.success) {
        menuStatus = result.error;
    } else {
        const cardputer::OperationResult settingsResult = cardputer::loadSettings(settings);
        const cardputer::OperationResult runtimeResult = settingsResult.success
            ? applyDisplayAndCpuSettings(settings)
            : settingsResult;
        const bool wifiChanged = settingsResult.success &&
            (settings.wifiSsid != previousWifiSsid ||
             settings.wifiPassword != previousWifiPassword);
        const cardputer::OperationResult wifiResult = wifiChanged
            ? cardputer::connectToWifi(settings)
            : cardputer::OperationResult{true, ""};
        const cardputer::OperationResult clockResult = wifiResult.success && wifiChanged
            ? cardputer::synchronizeTlsClock()
            : wifiResult;
        const cardputer::OperationResult activeResult = initializeChats();
        const cardputer::OperationResult listResult = activeResult.success
            ? refreshProjectPage(0)
            : activeResult;
        if (!runtimeResult.success) {
            menuStatus = runtimeResult.error;
        } else if (!wifiResult.success) {
            menuStatus = wifiResult.error;
        } else if (!clockResult.success) {
            menuStatus = clockResult.error;
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

bool cardputerEscapePressed()
{
    const Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
    return keys.esc || keyboardWordContains(keys, '`');
}

void waitForModalKeyRelease()
{
    while (!M5Cardputer.Keyboard.keyList().empty()) {
        M5Cardputer.update();
        delay(5);
    }
}

bool confirmPythonWorkspaceStart(const String& address, const String& password)
{
    waitForModalKeyRelease();
    cardputer::showPythonWorkspaceAccess(address, password);
    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            const Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
            if (keys.esc || keyboardWordContains(keys, '`')) {
                waitForModalKeyRelease();
                return false;
            }
            if (keys.enter) {
                waitForModalKeyRelease();
                return true;
            }
        }
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

}  // namespace
