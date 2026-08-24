namespace {

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

}  // namespace
