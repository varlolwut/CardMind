namespace {

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

    if (currentScreen == Screen::ProjectActions) {
        const std::size_t itemCount = projectActionItems().size();
        if (cancelPressed) {
            currentScreen = Screen::ProjectList;
            menuStatus = "";
            renderProjectList();
        } else if (upPressed) {
            projectActionsIndex = projectActionsIndex > 0 ? projectActionsIndex - 1 : 0;
            menuStatus = "";
            renderProjectActions();
        } else if (downPressed) {
            projectActionsIndex = std::min(projectActionsIndex + 1, itemCount - 1);
            menuStatus = "";
            renderProjectActions();
        } else if (enterPressed && projectActionsIndex == 0) {
            const cardputer::OperationResult activated = activateProject(selectedProjectId);
            if (!activated.success) {
                menuStatus = activated.error;
                renderProjectActions();
                return;
            }
            openChatList(Screen::ProjectList);
        } else if (enterPressed && projectActionsIndex == 1) {
            cardputer::ProjectDocumentResult project =
                cardputer::loadProject(selectedProjectId);
            if (!project.success) {
                menuStatus = project.error;
                renderProjectActions();
                return;
            }
            if (availableModels.empty()) {
                refreshModels();
                if (availableModels.empty()) {
                    menuStatus = statusMessage;
                    renderProjectActions();
                    return;
                }
            }
            const auto selected = std::find(
                availableModels.begin(), availableModels.end(), project.project.model);
            modelPickerIndex = project.project.model.isEmpty() ||
                selected == availableModels.end()
                ? 0
                : static_cast<std::size_t>(
                    std::distance(availableModels.begin(), selected)) + 1;
            menuStatus = "";
            currentScreen = Screen::ProjectModelPicker;
            renderProjectModelPicker();
        } else if (enterPressed && projectActionsIndex == 2) {
            cardputer::ProjectDocumentResult project =
                cardputer::loadProject(selectedProjectId);
            if (!project.success) {
                menuStatus = project.error;
                renderProjectActions();
                return;
            }
            projectInstructionsInput = project.project.instructions;
            projectInstructionsStatus = "";
            currentScreen = Screen::ProjectInstructions;
            renderProjectInstructions();
        } else if (enterPressed && projectActionsIndex >= 3 &&
                   projectActionsIndex <= 5) {
            cardputer::ProjectDocumentResult project =
                cardputer::loadProject(selectedProjectId);
            if (!project.success) {
                menuStatus = project.error;
                renderProjectActions();
                return;
            }
            if (projectActionsIndex == 3) {
                const std::uint32_t current = project.project.contextByteBudget;
                project.project.contextByteBudget = current < 16384 ? 16384 :
                    current < 32768 ? 32768 : current < 65536 ? 65536 :
                    current < 131072 ? 131072 : current < 262144 ? 262144 : 8192;
            } else if (projectActionsIndex == 4) {
                const std::uint32_t current = project.project.maximumOutputTokens;
                project.project.maximumOutputTokens = current < 512 ? 512 :
                    current < 1024 ? 1024 : current < 2048 ? 2048 :
                    current < 4096 ? 4096 : current < 8192 ? 8192 : 512;
            } else {
                project.project.automaticCompaction =
                    !project.project.automaticCompaction;
            }
            const cardputer::OperationResult saved = cardputer::saveProject(project.project);
            menuStatus = saved.success ? String("Project setting saved") : saved.error;
            renderProjectActions();
        } else if (enterPressed && projectActionsIndex == 6) {
            const cardputer::ProjectDocumentResult duplicated = cardputer::duplicateProject(
                selectedProjectId, selectedProjectTitle + " copy");
            if (!duplicated.success) {
                menuStatus = duplicated.error;
                renderProjectActions();
                return;
            }
            selectedProjectId = duplicated.project.summary.id;
            selectedProjectTitle = duplicated.project.summary.title;
            menuStatus = "Project duplicated";
            renderProjectActions();
        } else if (enterPressed && projectActionsIndex == 7) {
            cardputer::ProjectDocumentResult project =
                cardputer::loadProject(selectedProjectId);
            if (!project.success) {
                menuStatus = project.error;
            } else {
                const cardputer::OperationResult saved = cardputer::setProjectArchived(
                    selectedProjectId, !project.project.summary.archived);
                menuStatus = saved.success ? String("Project archive state changed")
                                           : saved.error;
            }
            renderProjectActions();
        } else if (enterPressed && projectActionsIndex == 8) {
            const String filename = "project_" + selectedProjectId +
                ".cardmind-project.jsonl";
            const cardputer::OperationResult exported = cardputer::exportProjectBundle(
                selectedProjectId, filename);
            menuStatus = exported.success ? "Exported as " + filename : exported.error;
            renderProjectActions();
        } else if (enterPressed) {
            currentScreen = Screen::ProjectList;
            menuStatus = "";
            renderProjectList();
        }
        return;
    }

    if (currentScreen == Screen::ProjectModelPicker) {
        const std::vector<String> items = projectModelItems();
        if (cancelPressed) {
            menuStatus = "";
            currentScreen = Screen::ProjectActions;
            renderProjectActions();
        } else if (upPressed) {
            modelPickerIndex = modelPickerIndex > 0 ? modelPickerIndex - 1 : 0;
            renderProjectModelPicker();
        } else if (downPressed && !items.empty()) {
            modelPickerIndex = std::min(modelPickerIndex + 1, items.size() - 1);
            renderProjectModelPicker();
        } else if (enterPressed) {
            if (modelPickerIndex >= items.size()) {
                menuStatus = "Project model selection is out of range";
                renderProjectModelPicker();
                return;
            }
            cardputer::ProjectDocumentResult project =
                cardputer::loadProject(selectedProjectId);
            if (!project.success) {
                menuStatus = project.error;
                renderProjectModelPicker();
                return;
            }
            project.project.model = modelPickerIndex == 0
                ? String() : availableModels[modelPickerIndex - 1];
            const cardputer::OperationResult saved =
                cardputer::saveProject(project.project);
            if (!saved.success) {
                menuStatus = saved.error;
                renderProjectModelPicker();
                return;
            }
            menuStatus = project.project.model.isEmpty()
                ? String("Project uses global model")
                : String("Project model saved");
            currentScreen = Screen::ProjectActions;
            renderProjectActions();
        }
        return;
    }

    if (currentScreen == Screen::ProjectInstructions) {
        if (cancelPressed) {
            projectInstructionsInput.clear();
            projectInstructionsStatus = "";
            currentScreen = Screen::ProjectActions;
            renderProjectActions();
        } else if (keys.fn && keys.f3) {
            keyboardLayout = keyboardLayout == cardputer::KeyboardLayout::English
                ? cardputer::KeyboardLayout::Russian
                : cardputer::KeyboardLayout::English;
            projectInstructionsStatus = keyboardLayout == cardputer::KeyboardLayout::English
                ? String("English layout") : String("Russian layout");
            renderProjectInstructions();
        } else if (clearDraftPressed) {
            projectInstructionsInput.clear();
            projectInstructionsStatus = "Instructions cleared; ENTER to save";
            renderProjectInstructions();
        } else if (backspacePressed) {
            if (!projectInstructionsInput.empty()) {
                projectInstructionsInput =
                    cardputer::removeLastUtf8CodePoint(projectInstructionsInput);
            }
            projectInstructionsStatus = "";
            renderProjectInstructions();
        } else if (enterPressed) {
            cardputer::ProjectDocumentResult project =
                cardputer::loadProject(selectedProjectId);
            if (!project.success) {
                projectInstructionsStatus = project.error;
                renderProjectInstructions();
                return;
            }
            project.project.instructions = projectInstructionsInput;
            const cardputer::OperationResult saved =
                cardputer::saveProject(project.project);
            if (!saved.success) {
                projectInstructionsStatus = saved.error;
                renderProjectInstructions();
                return;
            }
            projectInstructionsInput.clear();
            projectInstructionsStatus = "";
            menuStatus = project.project.instructions.empty()
                ? String("Project instructions disabled")
                : String("Project instructions saved");
            currentScreen = Screen::ProjectActions;
            renderProjectActions();
        } else if (!keys.fn && !keys.ctrl && !keys.alt && !keys.opt) {
            for (const char character : printableNewKeys(newPresses)) {
                const std::string text = keyboardLayout == cardputer::KeyboardLayout::Russian
                    ? cardputer::mapKeyToRussian(character)
                    : std::string(1, character);
                if (projectInstructionsInput.size() + text.size() >
                    cardputer::kMaximumProjectInstructionsBytes) {
                    projectInstructionsStatus = "Instruction limit: 16384 bytes";
                    break;
                }
                projectInstructionsInput += text;
                projectInstructionsStatus = "";
            }
            renderProjectInstructions();
        }
        return;
    }

    if (currentScreen == Screen::ProjectList) {
        const std::vector<String> items = projectListItems();
        if (cancelPressed) {
            currentScreen = Screen::MainCarousel;
            menuStatus = "";
            renderCarousel();
        } else if (upPressed) {
            projectListIndex = projectListIndex > 0 ? projectListIndex - 1 : 0;
            menuStatus = "";
            renderProjectList();
        } else if (downPressed && !items.empty()) {
            projectListIndex = std::min(projectListIndex + 1, items.size() - 1);
            menuStatus = "";
            renderProjectList();
        } else if (enterPressed && projectListIndex == 0) {
            const cardputer::ProjectDocumentResult created = cardputer::createProject(
                "New project");
            if (!created.success) {
                menuStatus = created.error;
                renderProjectList();
                return;
            }
            const cardputer::OperationResult activated = activateProject(
                created.project.summary.id);
            if (!activated.success) {
                menuStatus = activated.error;
                renderProjectList();
                return;
            }
            openChatList(Screen::ProjectList);
        } else if (enterPressed && projectListIndex <= projects.size()) {
            openProjectActions(projects[projectListIndex - 1]);
        } else if (enterPressed) {
            std::size_t navigationIndex = projects.size() + 1;
            if (!projectPreviousPageOffsets.empty()) {
                if (projectListIndex == navigationIndex) {
                    const std::uint32_t previous = projectPreviousPageOffsets.back();
                    projectPreviousPageOffsets.pop_back();
                    const cardputer::OperationResult loaded = refreshProjectPage(previous);
                    menuStatus = loaded.success ? String() : loaded.error;
                    renderProjectList();
                    return;
                }
                ++navigationIndex;
            }
            if (!projectPageEof && projectListIndex == navigationIndex) {
                projectPreviousPageOffsets.push_back(projectPageOffset);
                const cardputer::OperationResult loaded = refreshProjectPage(
                    projectNextPageOffset);
                if (!loaded.success) {
                    projectPreviousPageOffsets.pop_back();
                    menuStatus = loaded.error;
                }
                renderProjectList();
            }
        }
        return;
    }

    if (currentScreen == Screen::ChatList) {
        const std::vector<String> items = chatListItems();
        const std::size_t itemCount = items.size();
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
            if (chatListIndex == 0 || chatListIndex > chats.size()) {
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
            if (chatListIndex > 0 && chatListIndex <= chats.size()) {
                openChatActions(chats[chatListIndex - 1]);
            } else if (chatListIndex == 0) {
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
            } else {
                std::size_t navigationIndex = chats.size() + 1;
                if (!chatPreviousPageOffsets.empty()) {
                    if (chatListIndex == navigationIndex) {
                        const std::uint32_t previous = chatPreviousPageOffsets.back();
                        chatPreviousPageOffsets.pop_back();
                        const cardputer::OperationResult loaded = refreshChatPage(previous);
                        menuStatus = loaded.success ? String() : loaded.error;
                        renderChatList();
                        return;
                    }
                    ++navigationIndex;
                }
                if (!chatPageEof && chatListIndex == navigationIndex) {
                    chatPreviousPageOffsets.push_back(chatPageOffset);
                    const cardputer::OperationResult loaded = refreshChatPage(
                        chatNextPageOffset);
                    if (!loaded.success) {
                        chatPreviousPageOffsets.pop_back();
                        menuStatus = loaded.error;
                    }
                    renderChatList();
                }
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
            const cardputer::ChatDocumentResult loaded = cardputer::loadProjectChat(
                activeProjectId, selectedChatId, 1, 1);
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
            const cardputer::ChatDocumentResult loaded = cardputer::loadProjectChat(
                activeProjectId, selectedChatId, 1, 1);
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
            const cardputer::OperationResult saved =
                cardputer::saveProjectChatMetadata(updated);
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
            archivedChatPreviousOffsets.clear();
            const cardputer::OperationResult loaded = loadArchivedChatViewerPage(0);
            if (!loaded.success) {
                menuStatus = loaded.error;
                renderChatActions();
            } else {
                menuStatus = "";
                currentScreen = Screen::ArchivedChatViewer;
                renderArchivedChatViewer();
            }
        } else if (enterPressed && chatActionsIndex == 4) {
            retryLastRequest();
        } else if (enterPressed && chatActionsIndex == 5) {
            openLatestSearchSources();
        } else if (enterPressed && (chatActionsIndex == 6 || chatActionsIndex == 7)) {
            const cardputer::ChatDocumentResult loaded = cardputer::loadProjectChat(
                activeProjectId, selectedChatId, 1, 1);
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
            cardputer::OperationResult result = cardputer::saveProjectChatMetadata(updated);
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
            const cardputer::ChatDocumentResult duplicated =
                cardputer::duplicateProjectChat(activeProjectId, selectedChatId);
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
            const cardputer::OperationResult exported = cardputer::exportProjectChatMarkdown(
                activeProjectId, selectedChatId, filename);
            menuStatus = exported.success ? "Exported as " + filename : exported.error;
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 10) {
            const String filename = "project_" + activeProjectId +
                ".cardmind-project.jsonl";
            const cardputer::OperationResult exported = cardputer::exportProjectBundle(
                activeProjectId, filename);
            menuStatus = exported.success ? "Exported as " + filename : exported.error;
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 11) {
            const cardputer::ProjectDocumentResult project =
                cardputer::loadProject(activeProjectId);
            const cardputer::ChatDocumentResult chat = cardputer::loadProjectChat(
                activeProjectId, selectedChatId, 96, 131072);
            if (!project.success || !chat.success) {
                menuStatus = project.success ? chat.error : project.error;
                renderChatActions();
                return;
            }
            if (selectedChatId != activeChatId) {
                menuStatus = "Open this chat before regenerating its summary";
                renderChatActions();
                return;
            }
            const std::size_t retainedCount = std::min<std::size_t>(8, chat.chat.messages.size());
            const std::size_t summaryCount = chat.chat.messages.size() - retainedCount;
            if (summaryCount == 0) {
                menuStatus = "This chat is too short to compact";
                renderChatActions();
                return;
            }
            std::vector<cardputer::Message> source(
                chat.chat.messages.begin(), chat.chat.messages.begin() + summaryCount);
            cardputer::showBusyScreen("COMPACTING", "ESC cancels");
            const cardputer::OperationResult compacted = regenerateActiveContextSummary(
                project.project, source,
                chat.chat.summarizedMessageCount +
                    static_cast<std::uint32_t>(source.size()));
            menuStatus = compacted.success ? String("Context summary regenerated")
                                           : compacted.error;
            renderChatActions();
        } else if (enterPressed && chatActionsIndex == 12) {
            clearChatId = selectedChatId;
            clearChatTitle = selectedChatTitle;
            currentScreen = Screen::ClearChatConfirm;
            cardputer::showConfirmation("CLEAR MESSAGES", clearChatTitle,
                                        "ENTER clear  ESC cancel");
        } else if (enterPressed && chatActionsIndex == 13) {
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

    if (currentScreen == Screen::ArchivedChatViewer) {
        if (cancelPressed) {
            archivedChatPreviousOffsets.clear();
            currentScreen = Screen::ChatActions;
            renderChatActions();
        } else if (upPressed) {
            if (archivedChatViewerFirstLine > 0) {
                --archivedChatViewerFirstLine;
                renderArchivedChatViewer();
            } else if (!archivedChatPreviousOffsets.empty()) {
                const std::uint32_t previousOffset = archivedChatPreviousOffsets.back();
                archivedChatPreviousOffsets.pop_back();
                const cardputer::OperationResult loaded =
                    loadArchivedChatViewerPage(previousOffset);
                if (!loaded.success) {
                    menuStatus = loaded.error;
                    currentScreen = Screen::ChatActions;
                    renderChatActions();
                } else {
                    archivedChatViewerFirstLine = archivedChatViewerLines.size() > 8
                        ? archivedChatViewerLines.size() - 8 : 0;
                    renderArchivedChatViewer();
                }
            }
        } else if (downPressed) {
            if (archivedChatViewerFirstLine + 8 < archivedChatViewerLines.size()) {
                ++archivedChatViewerFirstLine;
                renderArchivedChatViewer();
            } else if (!archivedChatEof && archivedChatNextOffset > archivedChatPageOffset) {
                archivedChatPreviousOffsets.push_back(archivedChatPageOffset);
                const cardputer::OperationResult loaded =
                    loadArchivedChatViewerPage(archivedChatNextOffset);
                if (!loaded.success) {
                    archivedChatPreviousOffsets.pop_back();
                    menuStatus = loaded.error;
                    currentScreen = Screen::ChatActions;
                    renderChatActions();
                } else {
                    renderArchivedChatViewer();
                }
            }
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
            cardputer::ChatDocumentResult loaded = cardputer::loadProjectChat(
                activeProjectId, selectedChatId, 1, 1);
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
            const cardputer::OperationResult saved =
                cardputer::saveProjectChatMetadata(loaded.chat);
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
                if (instructionsInput.size() + text.size() >
                    cardputer::kMaximumProjectChatInstructionsBytes) {
                    instructionsStatus = "Instruction limit: 16384 bytes";
                    break;
                }
                instructionsInput += text;
                instructionsStatus = "";
            }
            renderChatInstructions();
        }
        return;
    }

    if (currentScreen == Screen::ClearChatConfirm) {
        if (cancelPressed) {
            clearChatId = "";
            clearChatTitle = "";
            currentScreen = Screen::ChatActions;
            renderChatActions();
        } else if (enterPressed) {
            const bool clearingActive = clearChatId == activeChatId;
            cardputer::OperationResult result = cardputer::clearProjectChatHistory(
                activeProjectId, clearChatId);
            if (result.success) {
                result = refreshChatList();
            }
            if (result.success && clearingActive) {
                result = activateChat(clearChatId);
            }
            clearChatId = "";
            clearChatTitle = "";
            if (!result.success) {
                currentScreen = Screen::ChatActions;
                menuStatus = result.error;
                renderChatActions();
            } else if (clearingActive) {
                currentScreen = Screen::Chat;
                setTransientStatus("Chat messages cleared", 2000);
                render();
            } else {
                currentScreen = Screen::ChatActions;
                menuStatus = "Chat messages cleared";
                renderChatActions();
            }
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
            cardputer::OperationResult result = cardputer::deleteProjectChat(
                activeProjectId, deleteChatId);
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
            return;
        } else if (leftPressed) {
            moveCarousel(cardputer::CarouselDirection::Previous);
        } else if (rightPressed) {
            moveCarousel(cardputer::CarouselDirection::Next);
        } else if (enterPressed) {
            if (carouselIndex == 0) {
                openProjectList();
            } else if (carouselIndex == 1) {
                openAiMenu();
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

    if (currentScreen == Screen::AiMenu) {
        const std::size_t itemCount = aiMenuItems().size();
        if (cancelPressed) {
            currentScreen = Screen::MainCarousel;
            menuStatus = "";
            renderCarousel();
        } else if (upPressed) {
            aiMenuIndex = aiMenuIndex > 0 ? aiMenuIndex - 1 : 0;
            renderAiMenu();
        } else if (downPressed) {
            aiMenuIndex = std::min(aiMenuIndex + 1, itemCount - 1);
            renderAiMenu();
        } else if (enterPressed) {
            if (aiMenuIndex == 0) {
                openModelPicker(Screen::AiMenu);
            } else if (aiMenuIndex == 1) {
                globalInstructionsInput = settings.globalInstructions.c_str();
                globalInstructionsStatus = "";
                currentScreen = Screen::GlobalInstructions;
                renderGlobalInstructions();
            } else {
                currentScreen = Screen::MainCarousel;
                menuStatus = "";
                renderCarousel();
            }
        }
        return;
    }

    if (currentScreen == Screen::GlobalInstructions) {
        if (cancelPressed) {
            globalInstructionsInput.clear();
            globalInstructionsStatus = "";
            currentScreen = Screen::AiMenu;
            renderAiMenu();
        } else if (keys.fn && keys.f3) {
            keyboardLayout = keyboardLayout == cardputer::KeyboardLayout::English
                ? cardputer::KeyboardLayout::Russian
                : cardputer::KeyboardLayout::English;
            globalInstructionsStatus = keyboardLayout == cardputer::KeyboardLayout::English
                ? String("English layout")
                : String("Russian layout");
            renderGlobalInstructions();
        } else if (clearDraftPressed) {
            globalInstructionsInput.clear();
            globalInstructionsStatus = "Instructions cleared; ENTER to save";
            renderGlobalInstructions();
        } else if (backspacePressed) {
            if (!globalInstructionsInput.empty()) {
                globalInstructionsInput =
                    cardputer::removeLastUtf8CodePoint(globalInstructionsInput);
            }
            globalInstructionsStatus = "";
            renderGlobalInstructions();
        } else if (enterPressed) {
            cardputer::Settings updated = settings;
            updated.globalInstructions = globalInstructionsInput.c_str();
            const cardputer::OperationResult saved = cardputer::saveSettings(updated);
            if (!saved.success) {
                globalInstructionsStatus = saved.error;
                renderGlobalInstructions();
                return;
            }
            settings = updated;
            globalInstructionsInput.clear();
            globalInstructionsStatus = "";
            menuStatus = settings.globalInstructions.isEmpty()
                ? String("Global instructions disabled")
                : String("Global instructions saved");
            currentScreen = Screen::AiMenu;
            renderAiMenu();
        } else if (!keys.fn && !keys.ctrl && !keys.alt && !keys.opt) {
            for (const char character : printableNewKeys(newPresses)) {
                const std::string text = keyboardLayout == cardputer::KeyboardLayout::Russian
                    ? cardputer::mapKeyToRussian(character)
                    : std::string(1, character);
                if (globalInstructionsInput.size() + text.size() >
                    cardputer::kMaximumChatInstructionsBytes) {
                    globalInstructionsStatus = "Instruction limit: 2048 bytes";
                    break;
                }
                globalInstructionsInput += text;
                globalInstructionsStatus = "";
            }
            renderGlobalInstructions();
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
                return cardputerEscapePressed();
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
                    result = cardputer::synchronizePythonModeSettings(settings, password, "");
                }
                if (!result.success) {
                    password = "";
                    menuStatus = result.error;
                    renderWebConsoleMenu();
                    return;
                }
                const String address = WiFi.status() == WL_CONNECTED
                    ? String("http://") + WiFi.localIP().toString() + "/"
                    : String("Wi-Fi unavailable");
                if (!confirmPythonWorkspaceStart(address, password)) {
                    password = "";
                    menuStatus = "Python start canceled";
                    renderWebConsoleMenu();
                    return;
                }
                result = cardputer::activatePythonMode();
                if (!result.success) {
                    password = "";
                    menuStatus = result.error;
                    renderWebConsoleMenu();
                    return;
                }
                cardputer::showPythonWorkspaceRunning(address, password);
                password = "";
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
        const std::vector<String> items = workspaceFileItems();
        const std::size_t itemCount = items.size();
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
            } else if (workspaceFileIndex <= workspaceFiles.size()) {
                if (workspaceFiles[workspaceFileIndex - 1].directory) {
                    menuStatus = "Choose a file inside this directory";
                    renderWorkspaceFileList();
                    return;
                }
                openSelectedWorkspaceFile();
            } else {
                std::size_t navigationIndex = workspaceFiles.size() + 1;
                if (!workspacePreviousPageOffsets.empty()) {
                    if (workspaceFileIndex == navigationIndex) {
                        const std::uint32_t previous = workspacePreviousPageOffsets.back();
                        workspacePreviousPageOffsets.pop_back();
                        const cardputer::OperationResult loaded = refreshWorkspacePage(previous);
                        menuStatus = loaded.success ? String() : loaded.error;
                        renderWorkspaceFileList();
                        return;
                    }
                    ++navigationIndex;
                }
                if (!workspacePageEof && workspaceFileIndex == navigationIndex) {
                    workspacePreviousPageOffsets.push_back(workspacePageOffset);
                    const cardputer::OperationResult loaded = refreshWorkspacePage(
                        workspaceNextPageOffset);
                    if (!loaded.success) {
                        workspacePreviousPageOffsets.pop_back();
                        menuStatus = loaded.error;
                    }
                    renderWorkspaceFileList();
                }
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
                const cardputer::SharedFileLinkResult linked =
                    cardputer::projectHasSharedFileLink(activeProjectId, fileViewerName);
                if (!linked.success) {
                    menuStatus = linked.error;
                } else {
                    const cardputer::OperationResult result = linked.linked
                        ? cardputer::unlinkSharedFileFromProject(
                              activeProjectId, fileViewerName)
                        : cardputer::linkSharedFileToProject(
                              activeProjectId, fileViewerName);
                    menuStatus = result.success
                        ? (linked.linked ? String("File unlinked from project")
                                         : String("File linked to project"))
                        : result.error;
                }
                renderFileActions();
            } else if (fileActionsIndex == 12) {
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
                if (fileNameInput.size() >= 180) {
                    fileNameStatus = "Path limit: 180 bytes";
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
        if (cancelPressed) {
            currentScreen = diagnosticsReturnScreen;
            menuStatus = "";
            render();
        } else if (upPressed || leftPressed) {
            diagnosticsIndex = diagnosticsIndex == 0 ? 1 : 0;
            renderDiagnostics();
        } else if (downPressed || rightPressed) {
            diagnosticsIndex = diagnosticsIndex == 0 ? 1 : 0;
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

    if (currentScreen == Screen::Chat && cancelPressed) {
        openChatList(Screen::ProjectList);
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
