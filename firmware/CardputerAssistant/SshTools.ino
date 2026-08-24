namespace {

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

}  // namespace
