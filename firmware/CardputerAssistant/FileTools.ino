namespace {

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
    refreshBatteryStatus();
    const String battery = batteryLevel >= 0
        ? (batteryLevel <= 15 ? String("LOW ") : String("")) + String(batteryLevel) +
          "%" + (batteryCharging ? " +" : "")
        : String("Unavailable");
    const String wifi = WiFi.status() == WL_CONNECTED
        ? String(WiFi.RSSI()) + " dBm"
        : String("Disconnected");
    const std::uint64_t totalBytes = fileWorkspaceReady ? SD.totalBytes() : 0;
    const std::uint64_t usedBytes = fileWorkspaceReady ? SD.usedBytes() : 0;
    const String storage = fileWorkspaceReady
        ? String(static_cast<unsigned long>(usedBytes / (1024U * 1024U))) + "/" +
          String(static_cast<unsigned long>(totalBytes / (1024U * 1024U))) + " MiB"
        : String("Unavailable");
    const cardputer::DeviceDiagnosticsView view = {
        String(kFirmwareVersion),
        battery,
        wifi,
        storage,
        String(ESP.getFreeHeap() / 1024U) + " KiB",
        String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024U) + " KiB",
        String(uxTaskGetStackHighWaterMark(nullptr)) + " B",
        String(getCpuFrequencyMhz()) + " MHz",
        systemMonitorUptime(),
        String(chats.size()),
        resetReasonLabel(),
        cardputer::previousOperation(),
        WiFi.status() == WL_CONNECTED,
        fileWorkspaceReady,
        crashJournalReady,
        sshStorageReady,
    };
    cardputer::showDeviceDiagnostics(view, diagnosticsIndex);
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

cardputer::OperationResult refreshWorkspacePage(std::uint32_t offset)
{
    const cardputer::WorkspaceFilesPageResult page = cardputer::listWorkspaceFilesPage(
        offset, 32);
    if (!page.success) {
        return {false, page.error};
    }
    workspaceFiles = page.files;
    workspacePageOffset = offset;
    workspaceNextPageOffset = page.nextOffset;
    workspacePageEof = page.eof;
    workspaceFileIndex = 0;
    return {true, ""};
}

void openWorkspaceFileList()
{
    workspaceListMode = WorkspaceListMode::Browse;
    workspaceListReturnScreen = Screen::FilesMenu;
    workspacePreviousPageOffsets.clear();
    const cardputer::OperationResult result = refreshWorkspacePage(0);
    if (!result.success) {
        menuStatus = result.error;
        renderFilesMenu();
        return;
    }
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
    workspacePreviousPageOffsets.clear();
    std::uint32_t offset = 0;
    while (true) {
        const cardputer::OperationResult result = refreshWorkspacePage(offset);
        if (!result.success) {
            return result;
        }
        for (std::size_t index = 0; index < workspaceFiles.size(); ++index) {
            if (workspaceFiles[index].name == name) {
                workspaceFileIndex = index + 1;
                return {true, ""};
            }
        }
        if (workspacePageEof) {
            break;
        }
        workspacePreviousPageOffsets.push_back(offset);
        offset = workspaceNextPageOffset;
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

}  // namespace
