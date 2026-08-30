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
        if (!cardputer::sshPrivateKeyIsInstalled(current.privateKeyId)) {
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
    cardputer::OperationResult result =
        cardputer::loadSshProfiles(original, originalSelected);
    if (!result.success) return result;

    std::vector<cardputer::SshProfileSummary> originalSummaries;
    std::size_t originalSummarySelected = 0;
    result = cardputer::loadSshProfileSummaries(
        originalSummaries, originalSummarySelected);
    if (!result.success || originalSummaries.size() != original.size() ||
        (!original.empty() && originalSummarySelected != originalSelected)) {
        return {false, "SSH public summary does not match the original inventory"};
    }

    const auto sameProfile = [](const cardputer::SshProfile& left,
                                const cardputer::SshProfile& right) {
        return left.name == right.name && left.host == right.host &&
               left.port == right.port && left.username == right.username &&
               left.password == right.password && left.authMode == right.authMode &&
               left.privateKeyPassphrase == right.privateKeyPassphrase;
    };
    const auto sameSummary = [](const cardputer::SshProfileSummary& left,
                                const cardputer::SshProfileSummary& right) {
        return left.id == right.id && left.name == right.name &&
               left.host == right.host && left.port == right.port &&
               left.username == right.username && left.authMode == right.authMode;
    };
    const auto findId = [](const std::vector<cardputer::SshProfileSummary>& profiles,
                           std::uint64_t id) {
        for (std::size_t index = 0; index < profiles.size(); ++index) {
            if (profiles[index].id == id) return index;
        }
        return profiles.size();
    };

    std::vector<std::uint64_t> fixtureIds;
    std::vector<String> fixtureNames;
    const String fixturePrefix = "P4T" + String(millis(), HEX) + "-";
    const std::uint64_t originalSelectedId = original.empty()
        ? 0 : originalSummaries[originalSelected].id;

    const auto cleanup = [&]() -> cardputer::OperationResult {
        while (true) {
            std::vector<cardputer::SshProfile> profiles;
            std::size_t selected = 0;
            cardputer::OperationResult cleanupResult =
                cardputer::loadSshProfiles(profiles, selected);
            if (!cleanupResult.success) return cleanupResult;
            std::vector<cardputer::SshProfileSummary> summaries;
            cleanupResult = cardputer::loadSshProfileSummaries(summaries, selected);
            if (!cleanupResult.success || summaries.size() != profiles.size()) {
                return {false, "SSH profile fixture cleanup could not load matching summaries"};
            }

            std::size_t ownedIndex = profiles.size();
            for (std::size_t index = original.size(); index < profiles.size(); ++index) {
                bool ownedId = false;
                for (std::uint64_t id : fixtureIds) {
                    ownedId = ownedId || summaries[index].id == id;
                }
                bool ownedName = false;
                for (const String& name : fixtureNames) {
                    ownedName = ownedName || profiles[index].name == name;
                }
                const bool ownedFields = ownedName &&
                    profiles[index].host == "127.0.0.1" &&
                    profiles[index].port == 22 &&
                    profiles[index].username == "p4test" &&
                    profiles[index].password == "temporary" &&
                    profiles[index].authMode == cardputer::SshAuthMode::Password &&
                    profiles[index].privateKeyPassphrase.isEmpty();
                if (ownedId || ownedFields) {
                    ownedIndex = index;
                    break;
                }
            }
            if (ownedIndex == profiles.size()) break;
            cleanupResult = cardputer::deleteSshProfile(ownedIndex);
            if (!cleanupResult.success) return cleanupResult;
        }

        if (originalSelectedId != 0) {
            std::vector<cardputer::SshProfileSummary> summaries;
            std::size_t selected = 0;
            cardputer::OperationResult cleanupResult =
                cardputer::loadSshProfileSummaries(summaries, selected);
            if (!cleanupResult.success) return cleanupResult;
            const std::size_t originalIndex = findId(summaries, originalSelectedId);
            if (originalIndex >= summaries.size()) {
                return {false, "SSH profile fixture cleanup lost the original selection"};
            }
            cleanupResult = cardputer::selectSshProfile(originalIndex);
            if (!cleanupResult.success) return cleanupResult;
        }

        std::vector<cardputer::SshProfile> restored;
        std::size_t restoredSelected = 0;
        cardputer::OperationResult cleanupResult =
            cardputer::loadSshProfiles(restored, restoredSelected);
        if (!cleanupResult.success || restored.size() != original.size() ||
            (!original.empty() && restoredSelected != originalSelected)) {
            return {false, "SSH profile fixture cleanup did not restore the inventory"};
        }
        std::vector<cardputer::SshProfileSummary> restoredSummaries;
        std::size_t restoredSummarySelected = 0;
        cleanupResult = cardputer::loadSshProfileSummaries(
            restoredSummaries, restoredSummarySelected);
        if (!cleanupResult.success ||
            restoredSummaries.size() != originalSummaries.size() ||
            (!originalSummaries.empty() &&
             restoredSummarySelected != originalSummarySelected)) {
            return {false, "SSH profile fixture cleanup did not restore IDs"};
        }
        for (std::size_t index = 0; index < original.size(); ++index) {
            if (!sameProfile(restored[index], original[index]) ||
                !sameSummary(restoredSummaries[index], originalSummaries[index])) {
                return {false, "SSH profile fixture cleanup changed original data"};
            }
        }
        return {true, ""};
    };

    cardputer::OperationResult testResult = {true, ""};
    do {
        std::vector<cardputer::SshProfileSummary> summaries = originalSummaries;
        while (summaries.size() < cardputer::kMaximumSshProfiles) {
            const String name = fixturePrefix + String(fixtureNames.size());
            bool collision = name.length() > 32;
            for (const cardputer::SshProfileSummary& summary : summaries) {
                collision = collision || summary.name == name;
            }
            if (collision) {
                testResult = {false, "SSH profile fixture name collision"};
                break;
            }
            fixtureNames.push_back(name);
            const cardputer::SshProfile fixture = {
                name, "127.0.0.1", 22, "p4test", "temporary",
                cardputer::SshAuthMode::Password, ""};
            const std::vector<cardputer::SshProfileSummary> beforeCreate = summaries;
            testResult = cardputer::saveSshProfileAt(fixture, summaries.size());
            if (!testResult.success) break;

            std::size_t selected = 0;
            testResult = cardputer::loadSshProfileSummaries(summaries, selected);
            if (!testResult.success ||
                summaries.size() != beforeCreate.size() + 1 ||
                selected != beforeCreate.size() || summaries.back().id == 0) {
                testResult = {false, "SSH profile fixture did not receive an opaque ID"};
                break;
            }
            for (const cardputer::SshProfileSummary& existing : beforeCreate) {
                if (existing.id == summaries.back().id) {
                    testResult = {false, "SSH profile fixture ID collided with stored identity"};
                    break;
                }
            }
            if (!testResult.success) break;
            fixtureIds.push_back(summaries.back().id);
        }
        if (!testResult.success) break;

        std::vector<cardputer::SshProfile> beforeSixth;
        std::size_t beforeSixthSelected = 0;
        testResult = cardputer::loadSshProfiles(beforeSixth, beforeSixthSelected);
        if (!testResult.success || beforeSixth.size() != summaries.size()) {
            testResult = {false, "SSH full profiles and summaries differ before cap check"};
            break;
        }
        const std::vector<cardputer::SshProfileSummary> beforeSixthSummaries = summaries;
        const cardputer::SshProfile sixth = {
            fixturePrefix + "sixth", "127.0.0.1", 22, "p4test", "temporary",
            cardputer::SshAuthMode::Password, ""};
        fixtureNames.push_back(sixth.name);
        if (cardputer::saveSshProfileAt(sixth, summaries.size()).success) {
            testResult = {false, "SSH profile store accepted a sixth profile"};
            break;
        }

        std::vector<cardputer::SshProfile> afterSixth;
        std::size_t afterSixthSelected = 0;
        testResult = cardputer::loadSshProfiles(afterSixth, afterSixthSelected);
        if (!testResult.success || afterSixth.size() != beforeSixth.size() ||
            afterSixthSelected != beforeSixthSelected ||
            afterSixth.size() != beforeSixthSummaries.size()) {
            testResult = {false, "Rejected sixth SSH profile changed the inventory"};
            break;
        }
        std::size_t afterSummarySelected = 0;
        testResult = cardputer::loadSshProfileSummaries(
            summaries, afterSummarySelected);
        if (!testResult.success ||
            summaries.size() != beforeSixthSummaries.size() ||
            afterSummarySelected != beforeSixthSelected) {
            testResult = {false, "Rejected sixth SSH profile changed public authority"};
            break;
        }
        for (std::size_t index = 0; index < summaries.size(); ++index) {
            if (!sameSummary(summaries[index], beforeSixthSummaries[index]) ||
                !sameProfile(afterSixth[index], beforeSixth[index])) {
                testResult = {false, "Rejected sixth SSH profile mutated stored data"};
                break;
            }
        }
        if (!testResult.success || fixtureIds.empty()) break;

        const std::uint64_t editedId = fixtureIds.front();
        const std::size_t editedIndex = findId(summaries, editedId);
        if (editedIndex >= summaries.size() || editedIndex >= afterSixth.size()) {
            testResult = {false, "SSH profile fixture ID disappeared before edit"};
            break;
        }
        cardputer::SshProfile edited = afterSixth[editedIndex];
        edited.name = fixturePrefix + "edited";
        fixtureNames.push_back(edited.name);
        testResult = cardputer::saveSshProfileAt(edited, editedIndex);
        if (!testResult.success) break;
        testResult = cardputer::selectSshProfile(editedIndex);
        if (!testResult.success) break;
        cardputer::SshProfile selectedProfile;
        testResult = cardputer::loadSshProfile(selectedProfile);
        if (!testResult.success || !sameProfile(selectedProfile, edited)) {
            testResult = {false, "SSH selected JIT profile did not match the edited fixture"};
            break;
        }
        std::size_t selected = 0;
        testResult = cardputer::loadSshProfileSummaries(summaries, selected);
        if (!testResult.success || selected != editedIndex ||
            editedIndex >= summaries.size() ||
            summaries[editedIndex].id != editedId) {
            testResult = {false, "SSH profile ID changed during edit or selection"};
            break;
        }

        if (fixtureIds.size() >= 2) {
            const std::uint64_t shiftedId = fixtureIds[1];
            const std::size_t shiftedBefore = findId(summaries, shiftedId);
            if (shiftedBefore >= summaries.size() || shiftedBefore <= editedIndex) {
                testResult = {false, "SSH shifted fixture ID is not in the expected position"};
                break;
            }
            std::vector<cardputer::SshProfile> beforeDelete;
            std::size_t beforeDeleteSelected = 0;
            testResult = cardputer::loadSshProfiles(
                beforeDelete, beforeDeleteSelected);
            if (!testResult.success) break;
            if (shiftedBefore >= beforeDelete.size()) {
                testResult = {false, "SSH shifted fixture profile is missing before deletion"};
                break;
            }
            const cardputer::SshProfile shiftedProfile = beforeDelete[shiftedBefore];

            testResult = cardputer::deleteSshProfile(editedIndex);
            if (!testResult.success) break;
            fixtureIds.erase(fixtureIds.begin());

            std::vector<cardputer::SshProfile> afterDelete;
            std::size_t afterDeleteSelected = 0;
            testResult = cardputer::loadSshProfiles(afterDelete, afterDeleteSelected);
            if (!testResult.success) break;
            testResult = cardputer::loadSshProfileSummaries(summaries, selected);
            const std::size_t shiftedAfter = findId(summaries, shiftedId);
            if (!testResult.success || shiftedAfter >= summaries.size() ||
                shiftedAfter != shiftedBefore - 1 ||
                shiftedAfter >= afterDelete.size() ||
                !sameProfile(afterDelete[shiftedAfter], shiftedProfile)) {
                testResult = {false, "SSH profile ID did not move with its full profile"};
            }
        }
    } while (false);

    const cardputer::OperationResult cleaned = cleanup();
    if (!cleaned.success) return cleaned;
    const cardputer::OperationResult cleanedAgain = cleanup();
    if (!cleanedAgain.success) return cleanedAgain;
    return testResult;
}
cardputer::OperationResult runSshCommandOptionsTest()
{
    const cardputer::SshCommandArgumentsResult defaults =
        cardputer::parseSshCommandArguments("{\"command\":\"printf ok\"}");
    if (!defaults.success || defaults.command != "printf ok" ||
        defaults.timeoutMs != cardputer::kDefaultSshCommandTimeoutMs ||
        defaults.maximumInlineOutputBytes !=
            cardputer::kDefaultSshCommandInlineOutputBytes) {
        return {false, "SSH command option defaults are incorrect"};
    }
    const cardputer::SshCommandArgumentsResult boundaries =
        cardputer::parseSshCommandArguments(
            "{\"command\":\"true\",\"timeout_ms\":1000,"
            "\"max_inline_output_bytes\":1}");
    if (!boundaries.success || boundaries.timeoutMs != 1000 ||
        boundaries.maximumInlineOutputBytes != 1) {
        return {false, "SSH command option boundary values were not preserved"};
    }
    static constexpr const char* kInvalidArguments[] = {
        "{\"command\":\"true\",\"timeout_ms\":999}",
        "{\"command\":\"true\",\"timeout_ms\":60001}",
        "{\"command\":\"true\",\"max_inline_output_bytes\":0}",
        "{\"command\":\"true\",\"max_inline_output_bytes\":16385}",
        "{\"command\":\"true\",\"timeout_ms\":\"1000\"}",
        "{\"command\":\"echo\\u0000mutate\"}",
        "{\"command\":\"true\",\"unexpected\":1}",
    };
    for (const char* invalidArguments : kInvalidArguments) {
        const cardputer::SshCommandArgumentsResult rejected =
            cardputer::parseSshCommandArguments(invalidArguments);
        if (rejected.success || rejected.error.isEmpty()) {
            return {false, "SSH command option validation accepted an invalid object"};
        }
    }
    std::string output = "abc";
    if (!cardputer::appendSshCommandOutput(output, "de", 2, 5) ||
        output != "abcde" ||
        cardputer::appendSshCommandOutput(output, "f", 1, 5) ||
        !output.empty()) {
        return {false, "SSH command overflow did not clear partial output"};
    }
    return {true, ""};
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

String sshCommandOutputRemoteFixture;

cardputer::OperationResult removeSshCommandOutputFixture(const String& name)
{
    if (name.isEmpty()) {
        return {true, ""};
    }
    const cardputer::OperationResult readable = cardputer::requireSdReadAccess();
    if (!readable.success) {
        return readable;
    }
    const String path = cardputer::workspaceFilePath(name);
    if (SD.exists(path) && !SD.remove(path)) {
        return {false, "Failed to remove the exact SSH command output fixture"};
    }
    return SD.exists(path)
        ? cardputer::OperationResult{
              false, "SSH command output fixture still exists after cleanup"}
        : cardputer::OperationResult{true, ""};
}

cardputer::OperationResult runSshCommandOutputStorageTest()
{
    const std::array<std::uint8_t, 5> raw = {'A', 0, 0xFF, 'B', 'C'};
    std::vector<String> fixtures;
    cardputer::OperationResult outcome = {true, ""};
    const auto remember = [&fixtures](
        const cardputer::SshCommandOutputCapture& capture) {
        if (capture.hasLog()) {
            fixtures.push_back(capture.logName());
        }
    };
    const auto reject = [&outcome](const String& error) {
        if (outcome.success) {
            outcome = {false, error};
        }
    };

    do {
        cardputer::SshCommandOutputCapture exact(4);
        cardputer::OperationResult result = exact.append(raw.data(), 4);
        if (result.success) result = exact.finalize();
        if (!result.success || exact.hasLog() ||
            exact.inlineOutput().size() != 4) {
            reject(result.success
                ? String("Exact-cap SSH output did not remain inline")
                : result.error);
            break;
        }

        cardputer::SshCommandOutputCapture spilled(4);
        result = spilled.append(raw.data(), raw.size());
        remember(spilled);
        if (result.success) result = spilled.finalize();
        if (!result.success || !spilled.isComplete() ||
            spilled.verifiedOutputBytes() != raw.size()) {
            reject(result.success
                ? String("Cap-plus-one SSH output was not verified")
                : result.error);
            break;
        }
        File stored = SD.open(
            cardputer::workspaceFilePath(spilled.logName()), FILE_READ);
        std::array<std::uint8_t, 5> loaded = {};
        const std::size_t loadedBytes = stored
            ? stored.read(loaded.data(), loaded.size()) : 0;
        if (stored) stored.close();
        if (loadedBytes != raw.size() ||
            !std::equal(raw.begin(), raw.end(), loaded.begin())) {
            reject("Spilled SSH output bytes did not match the source bytes");
            break;
        }

        cardputer::SshCommandOutputCapture promoted(16);
        result = promoted.append(raw.data(), 3);
        if (result.success) result = promoted.promoteToLog();
        remember(promoted);
        if (result.success) result = promoted.finalize();
        if (!result.success || !promoted.isComplete() ||
            promoted.verifiedOutputBytes() != 3) {
            reject(result.success
                ? String("Below-cap SSH output promotion was not verified")
                : result.error);
            break;
        }

        const std::array<cardputer::SdStorageState, 4> faultStates = {
            cardputer::SdStorageState::Missing,
            cardputer::SdStorageState::Full,
            cardputer::SdStorageState::Removed,
            cardputer::SdStorageState::Replaced,
        };
        for (const cardputer::SdStorageState state : faultStates) {
            cardputer::setSdStorageFaultOverrideForDiagnostics(state);
            cardputer::SshCommandOutputCapture beforeCreation(1);
            result = beforeCreation.append(raw.data(), 2);
            cardputer::clearSdStorageFaultOverrideForDiagnostics();
            if (result.success || beforeCreation.hasLog()) {
                reject("SSH output log was created while microSD was unavailable");
                break;
            }
        }
        if (!outcome.success) break;

        for (const cardputer::SdStorageState state : faultStates) {
            cardputer::SshCommandOutputCapture afterCreation(1);
            result = afterCreation.append(raw.data(), 2);
            remember(afterCreation);
            if (!result.success) {
                reject(result.error);
                break;
            }
            cardputer::setSdStorageFaultOverrideForDiagnostics(state);
            const cardputer::OperationResult faulted =
                afterCreation.append(raw.data() + 2, 1);
            cardputer::clearSdStorageFaultOverrideForDiagnostics();
            const cardputer::OperationResult finalized =
                afterCreation.finalize();
            if (faulted.success || finalized.success ||
                afterCreation.isComplete() ||
                afterCreation.verifiedOutputBytes() != 0) {
                reject("SSH output storage fault after creation was not fail-closed");
                break;
            }
        }
    } while (false);

    cardputer::clearSdStorageFaultOverrideForDiagnostics();
    for (const String& fixture : fixtures) {
        const cardputer::OperationResult first =
            removeSshCommandOutputFixture(fixture);
        const cardputer::OperationResult second =
            removeSshCommandOutputFixture(fixture);
        if (!first.success || !second.success) {
            outcome = {
                false,
                first.success ? second.error : first.error,
            };
        }
    }
    return outcome;
}

cardputer::OperationResult runSshCommandOutputRemoteTest(
    String& retainedName,
    std::uint32_t& outputBytes)
{
    retainedName = "";
    outputBytes = 0;
    if (!sshCommandOutputRemoteFixture.isEmpty()) {
        return {
            false,
            "The previous exact SSH output fixture must be cleaned before another run",
        };
    }
    const cardputer::SshProfile profile = {
        "Rebex output test", "test.rebex.net", 22, "demo", "password",
        cardputer::SshAuthMode::Password, ""};
    cardputer::SshClient client;
    cardputer::OperationResult result = client.connect(profile, 60000);
    if (!result.success) return result;

    const cardputer::SshTrustResult existing = cardputer::checkTrustedSshHost(
        profile.host, profile.port, client.fingerprint());
    if (!existing.success || (existing.found && !existing.matches)) {
        client.close();
        return {
            false,
            existing.success ? String("Rebex test host key changed") : existing.error,
        };
    }
    const bool temporaryTrust = !existing.found;
    if (temporaryTrust) {
        result = cardputer::trustSshHost(
            profile.host, profile.port, client.fingerprint());
    }
    if (result.success) {
        result = client.authenticate(profile, 60000);
    }

    cardputer::SshCommandOutputCapture capture(1);
    bool cancelAfterOutput = false;
    int exitStatus = -1;
    if (result.success) {
        const std::function<bool()> cancel = [&cancelAfterOutput]() {
            return cancelAfterOutput;
        };
        const cardputer::SshCommandOutputCallback output =
            [&capture, &cancelAfterOutput](
                const std::uint8_t* data,
                std::size_t bytes) {
            const cardputer::OperationResult appended =
                capture.append(data, bytes);
            if (appended.success && capture.hasLog()) {
                cancelAfterOutput = true;
            }
            return appended;
        };
        result = client.executeCommandStreamingControlled(
            "ls -lR /pub", exitStatus, 60000, cancel, output);
    }
    client.close();

    cardputer::OperationResult stored = {true, ""};
    if (capture.hasOutput()) {
        stored = capture.promoteToLog();
    }
    const cardputer::OperationResult finalized = capture.finalize();
    if (stored.success && !finalized.success) {
        stored = finalized;
    }
    if (capture.hasLog()) {
        retainedName = capture.logName();
        sshCommandOutputRemoteFixture = retainedName;
    }
    if (capture.isComplete()) {
        outputBytes = capture.verifiedOutputBytes();
    }

    if (result.success || !cancelAfterOutput ||
        result.error != "SSH command canceled by user") {
        const String observed = result.success
            ? String("remote command completed before cancellation")
            : result.error;
        result = {
            false,
            String("Read-only SSH output command did not end with the exact cancellation outcome: ") +
                observed,
        };
    } else if (!capture.hasLog() || !stored.success ||
               !capture.isComplete() || outputBytes == 0) {
        result = {
            false,
            stored.success
                ? String("Canceled SSH output was not retained completely")
                : stored.error,
        };
    } else {
        result = {true, ""};
    }

    if (temporaryTrust) {
        const cardputer::OperationResult forgotten =
            cardputer::forgetTrustedSshHost(profile.host, profile.port);
        if (!forgotten.success) {
            result = {
                false,
                result.success
                    ? forgotten.error
                    : result.error + "; temporary host-key cleanup failed: " +
                          forgotten.error,
            };
        }
    }
    return result;
}

cardputer::OperationResult cleanupSshCommandOutputRemoteTest(
    bool& alreadyAbsent,
    bool& removed)
{
    alreadyAbsent = sshCommandOutputRemoteFixture.isEmpty();
    removed = false;
    if (alreadyAbsent) {
        return {true, ""};
    }
    const cardputer::OperationResult readable = cardputer::requireSdReadAccess();
    if (!readable.success) {
        return readable;
    }
    const String name = sshCommandOutputRemoteFixture;
    const String path = cardputer::workspaceFilePath(name);
    alreadyAbsent = !SD.exists(path);
    const cardputer::OperationResult first =
        removeSshCommandOutputFixture(name);
    const cardputer::OperationResult second =
        removeSshCommandOutputFixture(name);
    if (!first.success || !second.success) {
        return first.success ? second : first;
    }
    removed = !alreadyAbsent;
    sshCommandOutputRemoteFixture = "";
    return {true, ""};
}

cardputer::OperationResult runModelSftpRemoteTest(bool& cleanupComplete)
{
    cleanupComplete = false;
    const std::function<bool()> neverCancel = []() { return false; };
    char nonce[17] = {};
    std::snprintf(
        nonce, sizeof(nonce), "%08lx%08lx",
        static_cast<unsigned long>(esp_random()),
        static_cast<unsigned long>(esp_random()));
    const String sourcePath = String("/tmp/cardmind-p4-06-") + nonce + "-source";
    const String destinationPath =
        String("/tmp/cardmind-p4-06-") + nonce + "-destination";
    const std::string sourceContent = std::string("ABC") + "\xD1\x8F" + "Z";
    const std::string existingContent = "existing";
    const std::string finalContent = "final";
    bool sourceCleanupCandidate = false;
    bool destinationCleanupCandidate = false;
    std::array<String, 3> temporaryCleanupCandidates = {};
    std::size_t temporaryCleanupCandidateCount = 0;
    bool cleanupCandidateTrackingFailed = false;

    const auto makeCall = [](const char* id, const char* name,
                             const JsonDocument& arguments) {
        cardputer::ToolCall call = {id, name, ""};
        serializeJson(arguments, call.arguments);
        return call;
    };
    const auto write = [&](const String& path, const std::string& content,
                           bool overwrite) {
        JsonDocument arguments;
        arguments["path"] = path;
        arguments["content"] = content;
        arguments["overwrite"] = overwrite;
        return cardputer::executeSftpWriteTool(
            makeCall("p4-06-write", "sftp_write", arguments), neverCancel);
    };
    const auto move = [&](bool overwrite) {
        JsonDocument arguments;
        arguments["source_path"] = sourcePath;
        arguments["destination_path"] = destinationPath;
        arguments["overwrite"] = overwrite;
        return cardputer::executeSftpMoveTool(
            makeCall("p4-06-move", "sftp_move", arguments), neverCancel);
    };
    const auto read = [&](const String& path, std::uint64_t offset,
                          std::size_t maximumBytes) {
        JsonDocument arguments;
        arguments["path"] = path;
        arguments["offset"] = offset;
        arguments["max_bytes"] = maximumBytes;
        return cardputer::executeSftpReadTool(
            makeCall("p4-06-read", "sftp_read", arguments), neverCancel);
    };
    const auto expectRead = [&](const String& path, std::uint64_t offset,
                                std::size_t maximumBytes,
                                const std::string& expected,
                                std::uint64_t nextOffset, bool eof) {
        const cardputer::ToolExecutionResult result =
            read(path, offset, maximumBytes);
        if (!result.success) return cardputer::OperationResult{false, result.error};
        JsonDocument output;
        const DeserializationError parsed = deserializeJson(output, result.output);
        if (parsed || !output["ok"].is<bool>() ||
            !output["ok"].as<bool>() || !output["content"].is<const char*>() ||
            output["content"].as<std::string>() != expected ||
            output["next_offset"].as<std::uint64_t>() != nextOffset ||
            output["eof"].as<bool>() != eof) {
            return cardputer::OperationResult{
                false, "Model SFTP read returned an unexpected bounded chunk"};
        }
        return cardputer::OperationResult{true, ""};
    };

    const auto findOwnedName = [&](cardputer::SshClient& client,
                                   const String& path, bool& found) {
        found = false;
        const String name = path.substring(path.lastIndexOf('/') + 1);
        std::uint32_t offset = 0;
        for (std::size_t pageIndex = 0; pageIndex < 64; ++pageIndex) {
            const cardputer::SftpPageResult page =
                client.listSftpDirectoryPageControlled(
                    "/tmp", offset, cardputer::kMaximumModelSftpPageEntries,
                    30000, neverCancel);
            if (!page.success) {
                return cardputer::OperationResult{false, page.error};
            }
            for (const cardputer::SftpEntry& entry : page.entries) {
                if (entry.name == name) {
                    found = true;
                    return cardputer::OperationResult{true, ""};
                }
            }
            if (page.eof) return cardputer::OperationResult{true, ""};
            if (page.nextOffset <= offset) {
                return cardputer::OperationResult{
                    false, "Model SFTP cleanup page did not advance"};
            }
            offset = page.nextOffset;
        }
        return cardputer::OperationResult{
            false, "Model SFTP cleanup directory exceeded the bounded scan"};
    };
    const auto rememberTemporaryPath = [&](const String& error) {
        const String prefix = "/tmp/.cardmind-";
        const int start = error.indexOf(prefix);
        if (start < 0) return cardputer::OperationResult{true, ""};
        const int pathLength = prefix.length() + 16 + 4;
        if (start + pathLength > static_cast<int>(error.length())) {
            cleanupCandidateTrackingFailed = true;
            return cardputer::OperationResult{
                false, "Model SFTP write reported a malformed temporary path"};
        }
        const String path = error.substring(start, start + pathLength);
        if (!path.endsWith(".tmp")) {
            cleanupCandidateTrackingFailed = true;
            return cardputer::OperationResult{
                false, "Model SFTP write reported a malformed temporary path"};
        }
        for (int index = prefix.length(); index < prefix.length() + 16; ++index) {
            const char value = path[index];
            if (!((value >= '0' && value <= '9') ||
                  (value >= 'a' && value <= 'f'))) {
                cleanupCandidateTrackingFailed = true;
                return cardputer::OperationResult{
                    false, "Model SFTP write reported a malformed temporary path"};
            }
        }
        for (std::size_t index = 0;
             index < temporaryCleanupCandidateCount; ++index) {
            if (temporaryCleanupCandidates[index] == path) {
                return cardputer::OperationResult{true, ""};
            }
        }
        if (temporaryCleanupCandidateCount >= temporaryCleanupCandidates.size()) {
            cleanupCandidateTrackingFailed = true;
            return cardputer::OperationResult{
                false, "Model SFTP write reported too many temporary paths"};
        }
        temporaryCleanupCandidates[temporaryCleanupCandidateCount++] = path;
        return cardputer::OperationResult{true, ""};
    };

    cardputer::SshProfile profile;
    cardputer::OperationResult preflight = cardputer::loadSshProfile(profile);
    cardputer::SshClient preflightClient;
    if (preflight.success) {
        preflight = connectTrustedSsh(profile, preflightClient);
    }
    if (preflight.success) {
        preflight = preflightClient.openSftpControlled(30000, neverCancel);
    }
    bool sourceCollision = false;
    bool destinationCollision = false;
    if (preflight.success) {
        preflight = findOwnedName(preflightClient, sourcePath, sourceCollision);
    }
    if (preflight.success) {
        preflight = findOwnedName(
            preflightClient, destinationPath, destinationCollision);
    }
    preflightClient.close();
    if (!preflight.success) {
        cleanupComplete = true;
        return preflight;
    }
    if (sourceCollision || destinationCollision) {
        cleanupComplete = true;
        return {
            false, "Model SFTP exact-owned fixture path already exists"};
    }

    const auto inspectExactPath = [&](const String& path, bool& found) {
        cardputer::SshClient client;
        cardputer::OperationResult inspected = connectTrustedSsh(profile, client);
        if (inspected.success) {
            inspected = client.openSftpControlled(30000, neverCancel);
        }
        if (inspected.success) inspected = findOwnedName(client, path, found);
        client.close();
        return inspected;
    };
    const auto cleanup = [&](cardputer::OperationResult outcome) {
        const bool hasCleanupCandidate = sourceCleanupCandidate ||
            destinationCleanupCandidate || temporaryCleanupCandidateCount > 0;
        if (!hasCleanupCandidate) {
            cleanupComplete = !cleanupCandidateTrackingFailed;
            return outcome;
        }
        cardputer::OperationResult cleaned = {true, ""};
        cardputer::SshClient client;
        if (cleaned.success) cleaned = connectTrustedSsh(profile, client);
        if (cleaned.success) cleaned = client.openSftpControlled(30000, neverCancel);
        const std::array<String, 5> paths = {
            sourcePath,
            destinationPath,
            temporaryCleanupCandidates[0],
            temporaryCleanupCandidates[1],
            temporaryCleanupCandidates[2],
        };
        const std::array<bool, 5> owned = {
            sourceCleanupCandidate,
            destinationCleanupCandidate,
            temporaryCleanupCandidateCount > 0,
            temporaryCleanupCandidateCount > 1,
            temporaryCleanupCandidateCount > 2,
        };
        for (std::size_t index = 0; cleaned.success && index < paths.size(); ++index) {
            if (!owned[index]) continue;
            bool found = false;
            cleaned = findOwnedName(client, paths[index], found);
            if (cleaned.success && found) {
                cleaned = client.removeSftpPath(paths[index], false, 30000);
            }
        }
        for (std::size_t pass = 0; cleaned.success && pass < 2; ++pass) {
            for (std::size_t index = 0; cleaned.success && index < paths.size(); ++index) {
                if (!owned[index]) continue;
                bool found = false;
                cleaned = findOwnedName(client, paths[index], found);
                if (cleaned.success && found) {
                    cleaned = {false, "Exact model SFTP fixture remains after cleanup"};
                }
            }
        }
        client.close();
        cleanupComplete = cleaned.success && !cleanupCandidateTrackingFailed;
        if (!cleanupComplete) {
            const String cleanupError = cleaned.success
                ? String("Exact model SFTP cleanup candidate tracking failed")
                : cleaned.error;
            return cardputer::OperationResult{
                false, outcome.success
                    ? cleanupError
                    : outcome.error + "; exact remote cleanup failed: " + cleanupError};
        }
        return outcome;
    };

    cardputer::OperationResult outcome = {true, ""};
    sourceCleanupCandidate = true;
    cardputer::ToolExecutionResult result =
        write(sourcePath, sourceContent, false);
    if (!result.success) {
        const cardputer::OperationResult remembered =
            rememberTemporaryPath(result.error);
        outcome = remembered.success
            ? cardputer::OperationResult{false, result.error}
            : remembered;
    }
    if (outcome.success) {
        destinationCleanupCandidate = true;
        result = write(destinationPath, existingContent, false);
        if (!result.success) {
            const cardputer::OperationResult remembered =
                rememberTemporaryPath(result.error);
            outcome = remembered.success
                ? cardputer::OperationResult{false, result.error}
                : remembered;
        }
    }
    if (outcome.success) {
        JsonDocument firstArguments;
        firstArguments["path"] = "/tmp";
        firstArguments["offset"] = 0;
        firstArguments["max_entries"] = 1;
        const cardputer::ToolExecutionResult first =
            cardputer::executeSftpListTool(
                makeCall("p4-06-list-1", "sftp_list", firstArguments),
                neverCancel);
        JsonDocument firstOutput;
        if (!first.success || deserializeJson(firstOutput, first.output) ||
            !firstOutput["entries"].is<JsonArray>() ||
            firstOutput["entries"].as<JsonArray>().size() != 1 ||
            firstOutput["eof"].as<bool>()) {
            outcome = {false, first.success
                ? String("First model SFTP list page was not bounded") : first.error};
        } else {
            const std::uint32_t next =
                firstOutput["next_offset"].as<std::uint32_t>();
            JsonDocument secondArguments;
            secondArguments["path"] = "/tmp";
            secondArguments["offset"] = next;
            secondArguments["max_entries"] = 1;
            const cardputer::ToolExecutionResult second =
                cardputer::executeSftpListTool(
                    makeCall("p4-06-list-2", "sftp_list", secondArguments),
                    neverCancel);
            JsonDocument secondOutput;
            if (next == 0 || !second.success ||
                deserializeJson(secondOutput, second.output) ||
                !secondOutput["entries"].is<JsonArray>() ||
                secondOutput["entries"].as<JsonArray>().size() != 1 ||
                secondOutput["next_offset"].as<std::uint32_t>() <= next) {
                outcome = {false, second.success
                    ? String("Second model SFTP list page did not advance")
                    : second.error};
            }
        }
    }
    if (outcome.success) {
        outcome = expectRead(sourcePath, 0, 4, "ABC", 3, false);
    }
    if (outcome.success) {
        outcome = expectRead(
            sourcePath, 3, 4, std::string("\xD1\x8F") + "Z", 6, true);
    }
    if (outcome.success && read(sourcePath, 4, 4).success) {
        outcome = {false, "Model SFTP read accepted a UTF-8 continuation offset"};
    }
    if (outcome.success && move(false).success) {
        outcome = {false, "Model SFTP no-overwrite move replaced an existing target"};
    }
    if (outcome.success) {
        outcome = expectRead(sourcePath, 0, 8, sourceContent, 6, true);
    }
    if (outcome.success) {
        outcome = expectRead(destinationPath, 0, 16, existingContent, 8, true);
    }
    if (outcome.success) {
        result = move(true);
        if (!result.success) outcome = {false, result.error};
    }
    if (outcome.success) {
        bool sourceStillExists = false;
        outcome = inspectExactPath(sourcePath, sourceStillExists);
        if (outcome.success && sourceStillExists) {
            outcome = {
                false, "Model SFTP overwrite move left the source path present"};
        }
    }
    if (outcome.success) {
        outcome = expectRead(destinationPath, 0, 8, sourceContent, 6, true);
    }
    if (outcome.success) {
        result = write(destinationPath, finalContent, true);
        if (!result.success) {
            const cardputer::OperationResult remembered =
                rememberTemporaryPath(result.error);
            outcome = remembered.success
                ? cardputer::OperationResult{false, result.error}
                : remembered;
        }
    }
    if (outcome.success) {
        outcome = expectRead(destinationPath, 0, 8, finalContent, 5, true);
    }
    return cleanup(outcome);
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

struct WorkspaceFileSelectionResult {
    bool success;
    bool selected;
    bool empty;
    cardputer::WorkspaceFile file;
    String error;
};

using WorkspaceFilePredicate = bool (*)(const cardputer::WorkspaceFile& file);
using WorkspaceFileLabel = String (*)(const cardputer::WorkspaceFile& file);

bool includeWorkspaceFile(const cardputer::WorkspaceFile& file)
{
    return !file.directory;
}

bool includeWorkspacePrivateKey(const cardputer::WorkspaceFile& file)
{
    return !file.directory && (file.name.endsWith(".pem") || file.name.endsWith(".key"));
}

String workspaceUploadLabel(const cardputer::WorkspaceFile& file)
{
    return file.name + "  " + String(file.size) + " B";
}

String workspacePrivateKeyLabel(const cardputer::WorkspaceFile& file)
{
    return file.name;
}

WorkspaceFileSelectionResult selectWorkspaceFilePage(
    const String& title,
    WorkspaceFilePredicate include,
    WorkspaceFileLabel label)
{
    constexpr std::uint32_t kPageEntries = 16;
    std::uint32_t offset = 0;
    bool foundAnyCandidate = false;
    while (true) {
        const cardputer::WorkspaceFilesPageResult page =
            cardputer::listWorkspaceFilesPage(offset, kPageEntries);
        if (!page.success) {
            return {false, false, false, {}, page.error};
        }
        std::vector<String> items;
        std::vector<int> actions;
        items.reserve(page.files.size() + 2);
        actions.reserve(page.files.size() + 2);
        if (offset > 0) {
            items.push_back("< Previous entries");
            actions.push_back(-1);
        }
        for (std::size_t index = 0; index < page.files.size(); ++index) {
            if (include(page.files[index])) {
                foundAnyCandidate = true;
                items.push_back(label(page.files[index]));
                actions.push_back(static_cast<int>(index));
            }
        }
        if (page.eof && !foundAnyCandidate) {
            return {true, false, true, {}, ""};
        }
        if (!page.eof) {
            items.push_back("Next entries >");
            actions.push_back(-2);
        }
        if (items.empty()) {
            return {true, false, true, {}, ""};
        }
        const int selected = modalSelection(
            title + " " + String(offset + 1), items, 0,
            "UP/DOWN  ENTER  ESC cancel");
        if (selected < 0) {
            return {true, false, false, {}, ""};
        }
        const int action = actions[static_cast<std::size_t>(selected)];
        if (action == -1) {
            offset = offset >= kPageEntries ? offset - kPageEntries : 0;
            continue;
        }
        if (action == -2) {
            if (page.nextOffset <= offset) {
                return {false, false, false, {},
                        "Workspace pagination did not advance"};
            }
            offset = page.nextOffset;
            continue;
        }
        return {true, true, false,
                page.files[static_cast<std::size_t>(action)], ""};
    }
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
            const WorkspaceFileSelectionResult selection = selectWorkspaceFilePage(
                "UPLOAD FROM SD", includeWorkspaceFile, workspaceUploadLabel);
            if (!selection.success) {
                result = {false, selection.error};
                break;
            }
            if (selection.empty) {
                cardputer::showTextViewer("SFTP UPLOAD", {"SD workspace is empty."}, 0,
                                         "ENTER/ESC close");
                delay(1200);
                continue;
            }
            if (!selection.selected) continue;
            std::string remoteName = selection.file.name.c_str();
            if (!modalTextInput("REMOTE NAME", "Destination filename", remoteName, 255,
                                false, remoteName)) continue;
            cardputer::showBusyScreen("SFTP UPLOAD", "Writing remote file...");
            cardputer::markOperation("sftp_upload");
            result = client.uploadSftpFile(selection.file.name,
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
            if (!modalTextInput("DOWNLOAD TO SD", "Workspace filename", localName, 502,
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

cardputer::OperationResult installSshKeyFromWorkspace(std::uint64_t profileId)
{
    const WorkspaceFileSelectionResult selection = selectWorkspaceFilePage(
        "INSTALL SSH KEY", includeWorkspacePrivateKey, workspacePrivateKeyLabel);
    if (!selection.success) return {false, selection.error};
    if (selection.empty) return {false, "Put a .pem or .key file in the SD workspace first"};
    if (!selection.selected) return {true, "Key installation cancelled"};
    const String sourcePath = cardputer::workspaceFilePath(selection.file.name);
    cardputer::OperationResult result = cardputer::installSshPrivateKey(
        sourcePath, profileId);
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
        std::vector<cardputer::SshProfileSummary> summaries;
        std::size_t summarySelected = 0;
        result = cardputer::loadSshProfileSummaries(summaries, summarySelected);
        if (!result.success || summaries.size() != profiles.size() ||
            (!profiles.empty() && summarySelected != selectedIndex)) {
            return {false, "SSH profile IDs do not match the Device inventory"};
        }
        const String selectedName = profiles.empty() ? String("not configured")
                                                      : profiles[selectedIndex].name;
        const bool selectedKeyInstalled = !profiles.empty() &&
            cardputer::sshPrivateKeyIsInstalled(
                profiles[selectedIndex].privateKeyId);
        const int action = modalSelection("SSH TOOL", {
            "Connect: " + selectedName,
            "SFTP: " + selectedName,
            "Manage profiles (" + String(profiles.size()) + ")",
            String("Install private key: ") + (selectedKeyInstalled ? "yes" : "no"),
            "Terminal shortcuts",
            "Back to Tools"}, 0, status.isEmpty() ? "UP/DOWN  ENTER  ESC back" : status);
        status = "";
        if (action < 0 || action == 5) return {true, ""};
        if ((action == 0 || action == 1 || action == 3) && profiles.empty()) {
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
            result = installSshKeyFromWorkspace(summaries[selectedIndex].id);
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
