namespace {

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
    cardputer::markOperation("voice_recording");
    const cardputer::VoiceRecordingResult recording = cardputer::recordVoiceWhileButtonHeld(
        [](std::uint32_t elapsedMs, std::uint16_t level) {
            cardputer::showVoiceRecording(
                elapsedMs, cardputer::maximumVoiceRecordingMs(), level);
        });
    cardputer::markOperation("idle");
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
    cardputer::markOperation("stt_request");
    const cardputer::CancelCallback sttCancelled = []() {
        M5Cardputer.update();
        return M5Cardputer.Keyboard.keysState().esc;
    };
    const cardputer::TranscriptionResult transcription =
        cardputer::transcribeVoiceRecording(settings, sttCancelled);
    cardputer::markOperation("idle");
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

cardputer::SpeechPlaybackCommand chatSpeechPlaybackControl()
{
    M5Cardputer.update();
    return M5Cardputer.Keyboard.keysState().esc
        ? cardputer::SpeechPlaybackCommand::Stop
        : cardputer::SpeechPlaybackCommand::Continue;
}

bool speechWasStopped(const cardputer::OperationResult& result)
{
    return result.error == "Speech playback stopped" ||
           result.error == "Speech synthesis canceled by user";
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
    cardputer::showBusyScreen("SPEAKING", "ESC stops download or playback");
    cardputer::markOperation("tts_request");
    const cardputer::OperationResult result = cardputer::synthesizeAndPlaySpeechControlled(
        settings, message->content, chatSpeechPlaybackControl);
    cardputer::markOperation("idle");
    if (speechWasStopped(result)) {
        setTransientStatus("Speech stopped", 1500);
        Serial.println("INFO event=tts_playback result=stopped source=manual");
    } else if (result.success) {
        setTransientStatus("Spoken", 1500);
        Serial.println("INFO event=tts_playback result=ok source=manual");
    } else {
        statusMessage = result.error;
        Serial.println("ERROR event=tts_playback result=failed source=manual");
    }
    render();
}

std::string joinedViewerLines(std::size_t firstLine, std::size_t lastLine)
{
    if (fileViewerLines.empty() || firstLine >= fileViewerLines.size()) {
        return "";
    }
    const std::size_t boundedLast = std::min(lastLine, fileViewerLines.size() - 1);
    std::string text;
    for (std::size_t index = firstLine; index <= boundedLast; ++index) {
        if (!text.empty()) {
            text += '\n';
        }
        text += fileViewerLines[index];
    }
    return text;
}

std::size_t speechSegmentBytes(const std::string& text, std::size_t offset)
{
    constexpr std::size_t maximumBytes = 4500;
    const std::size_t remaining = text.size() - offset;
    std::size_t bytes = std::min(maximumBytes, remaining);
    while (bytes > 0 && !cardputer::isValidUtf8(text.substr(offset, bytes))) {
        --bytes;
    }
    if (bytes == 0) {
        return 0;
    }
    if (bytes < remaining) {
        const std::size_t separator = text.find_last_of("\n.?! ", offset + bytes - 1);
        if (separator != std::string::npos && separator >= offset + bytes / 2) {
            bytes = separator - offset + 1;
        }
    }
    return bytes;
}

cardputer::OperationResult playDocumentSpeechText(const std::string& text,
                                                  const String& source)
{
    if (text.empty()) {
        return {false, "Selected document text is empty"};
    }
    bool paused = false;
    bool stopped = false;
    const cardputer::SpeechPlaybackControl control = [&paused, &stopped, &source]() {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            const Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();
            if (keys.esc || keyboardWordContains(keys, '`')) {
                stopped = true;
            } else if (keys.enter) {
                paused = !paused;
                cardputer::showBusyScreen(
                    "DOCUMENT TTS",
                    paused ? String("Paused - ENTER resume, ESC stop")
                           : source + " - ENTER pause, ESC stop");
            }
        }
        if (stopped) {
            return cardputer::SpeechPlaybackCommand::Stop;
        }
        return paused ? cardputer::SpeechPlaybackCommand::Pause
                      : cardputer::SpeechPlaybackCommand::Continue;
    };
    std::size_t offset = 0;
    while (offset < text.size()) {
        const std::size_t bytes = speechSegmentBytes(text, offset);
        if (bytes == 0) {
            return {false, "Document TTS could not split text at a valid UTF-8 boundary"};
        }
        cardputer::showBusyScreen("DOCUMENT TTS", source + " - ENTER pause, ESC stop");
        const cardputer::OperationResult spoken = cardputer::synthesizeAndPlaySpeechControlled(
            settings, text.substr(offset, bytes), control);
        if (!spoken.success || stopped || !spoken.error.isEmpty()) {
            return spoken;
        }
        offset += bytes;
    }
    return {true, ""};
}

cardputer::OperationResult prepareDocumentSpeech()
{
    if (!cardputer::ttsSettingsAreComplete(settings)) {
        return {false, "TTS is not configured; use Voice > Web setup"};
    }
    ensureNetworkReady();
    if (WiFi.status() != WL_CONNECTED) {
        return {false, statusMessage.isEmpty() ? String("Wi-Fi is not connected") : statusMessage};
    }
    if (std::time(nullptr) < 1700000000) {
        return {false, "TLS time is not synchronized"};
    }
    return {true, ""};
}

cardputer::OperationResult speakEntireDocument()
{
    std::uint32_t offset = 0;
    while (true) {
        const cardputer::WorkspaceChunkResult chunk = cardputer::readWorkspaceFileChunk(
            fileViewerName, offset, 3000);
        if (!chunk.success) {
            return {false, chunk.error};
        }
        const std::string speech = cardputer::documentSpeechText(fileReaderMode, chunk.content);
        const cardputer::OperationResult spoken = playDocumentSpeechText(
            speech, String("Reading ") + fileViewerName);
        if (!spoken.success || !spoken.error.isEmpty()) {
            return spoken;
        }
        if (chunk.eof) {
            return {true, ""};
        }
        offset = chunk.nextOffset;
    }
}

void retryLastRequest()
{
    if (retryPrompt.empty()) {
        menuStatus = "No failed request is available to retry";
        renderChatActions();
        return;
    }
    if (history.empty() || history.back().role != "user" ||
        history.back().content != retryPrompt) {
        menuStatus = "Retry context no longer matches the failed request";
        renderChatActions();
        return;
    }
    history.pop_back();
    inputBuffer = retryPrompt;
    activeResponse.clear();
    const cardputer::OperationResult saved = saveCurrentChat();
    if (!saved.success) {
        history.push_back({"user", retryPrompt});
        inputBuffer.clear();
        menuStatus = saved.error;
        renderChatActions();
        return;
    }
    currentScreen = Screen::Chat;
    menuStatus = "";
    submitPrompt();
}

}  // namespace
