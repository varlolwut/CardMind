# CardMind

Direct text and voice LLM assistant firmware for M5Stack Cardputer ADV. The device connects directly to a configurable OpenAI-compatible chat API, separately configured speech-to-text and text-to-speech APIs, and an optional Exa web search API over verified TLS. Chats, temporary audio, and model-created text files are stored on microSD. No phone or intermediary server is required after setup.

## Releases

Every successful build of `main` publishes a GitHub Release named after the firmware version in `CardputerAssistant.ino`. Each release contains:

- `CardMind-cardputer-adv.bin`: application-only update image for an existing CardMind installation using the 8 MB `default_8MB` partition layout; flash at `0x10000` to preserve NVS settings.
- `CardMind-cardputer-adv-merged.bin`: complete 8 MB installation image with bootloader and partition table; flash at `0x0` for a clean installation. A full-image flash may erase existing device settings.
- `SHA256SUMS.txt`: checksums for both images.

The same files are retained as a workflow artifact for every successful `main` build.

## Security and first setup

The firmware contains no Wi-Fi or API credentials. On a clean device it creates a WPA2-protected access point with a unique password shown only on the Cardputer screen. The password is generated once per installation and remains the same across restarts; a full flash erase generates a new one. Connect to that network, choose to use it without Internet, and manually open `http://192.168.4.1` in a full browser. The form scans nearby 2.4 GHz networks, while still allowing a hidden SSID to be entered manually, and accepts the Wi-Fi password, API key, HTTPS API base URL, and model id. Automatic captive redirection is intentionally disabled so iOS password managers remain available. Settings are saved in ESP32 NVS. Credentials are never printed to serial.

The HTTPS clients validate the configured chat endpoint against ISRG Root X1 and the default Groq and Exa endpoints against GTS Root R4, then synchronize the clock before making TLS connections. They do not call `setInsecure()`. Configurable API base URLs may be entered either as an origin such as `https://api.example.com` or with the version suffix such as `https://api.example.com/v1`; the firmware prevents a duplicate `/v1` path.

## Controls

- Enter: send the current prompt
- Hold G0: record up to 60 seconds; release to transcribe into the editable prompt
- Backspace: delete one UTF-8 code point
- Fn+1: open the separate-chat manager
- Fn+2: open the on-device model picker populated by `GET /v1/models`
- Fn+3: switch English/Russian keyboard layout
- Fn+4: open the main menu with controls help, Wi-Fi, file download, and web setup
- Fn+5 or Fn+Up (`;` key): scroll toward older messages
- Fn+6 or Fn+Down (`.` key): scroll toward newer messages
- Fn+7: create and switch to a new chat immediately
- Fn+8: speak the latest assistant response through the built-in speaker
- Ctrl+Backspace: clear the current draft

The complete controls reference is available on-device under **Fn+4 > Controls help**. In menus, use Fn+5/Fn+6 to move, Enter to select, and Fn+` to cancel. In the chat manager, Fn+Delete opens a confirmation screen before deleting the selected chat.

The chat request uses `POST /v1/chat/completions`, Bearer authorization, and SSE `choices[0].delta.content` streaming. Voice input records 16 kHz mono PCM WAV to `/voice.wav` on microSD, uploads it as multipart form data to the separately configured `/v1/audio/transcriptions` endpoint, then deletes the temporary file. Speech language is detected automatically and is independent of the active keyboard layout. The default STT configuration is Groq `whisper-large-v3-turbo`; its API key is separate from the chat API key.

Speech output uses ElevenLabs `POST /v1/text-to-speech/{voice_id}/stream` with `pcm_16000`, verified against GTS Root R1. The default model is `eleven_multilingual_v2`, which supports Russian and English, and the default voice is George (`JBFqnCBsd6RMkjVDRZzb`). Audio is streamed to temporary `/tts.pcm` on microSD and played in bounded chunks, so a complete recording is never buffered in ESP32 RAM. Playback volume defaults to 75% and cycles through 25%, 50%, 75%, and 100% under **Fn+4 > TTS volume**; the selected level survives restarts. Configure a separate ElevenLabs key under **Fn+4 > Web setup**, use **Fn+8** for manual playback, or toggle **Auto TTS** in the main menu. Auto playback defaults to off to avoid consuming credits unexpectedly. Responses over 5,000 UTF-8 bytes fail with an explicit request for a shorter answer.

## Chats and files

Each conversation has an independent context and a separate versioned JSON file under `/assistant/chats`. The active chat survives restarts. The firmware keeps at most 20 chats, 64 messages and 32,768 UTF-8 bytes of active context per chat; the oldest complete user/assistant pairs are trimmed when needed.

The model receives four standard OpenAI function tools: `list_files`, chunked `read_file`, `write_file`, and atomic `append_file`. They are restricted to `/assistant/files`, validate filenames and UTF-8, reject traversal, and allow only `.txt`, `.md`, `.json`, `.csv`, `.html`, and `.svg` files. A single file is limited to 491,520 bytes (20 times the original limit); each model call reads or writes at most 12,288 bytes so large files never have to fit in ESP32 RAM. To download files, open Fn+4, select **Files download portal**, connect to the protected access point shown on screen, and open `http://192.168.4.1`. The portal is read-only and uses the installation-specific setup password.

Workspace tools are attached only when the current prompt explicitly mentions a file, microSD, a supported filename extension, or starts with `/file`. This prevents ordinary questions from accidentally entering repeated tool-calling rounds. Mention the filename again in a later follow-up when further file access is required.

Current-information prompts and explicit `/search` or `/web` commands attach a `web_search` tool. The OpenAI-compatible chat API relays tool calls, while the Cardputer executes the search itself through the separately configured Exa endpoint and returns bounded source snippets and URLs to the model. Search setup is optional: create an Exa key at `https://dashboard.exa.ai`, then enter it under Fn+4 > **Web setup (API/key)**. Exa's starter tier currently requires no payment method. If search is not configured, current-information prompts fail once with an actionable setup message instead of entering repeated tool rounds.

## Build

Use the project-local Arduino CLI configuration in `toolchain/arduino-cli.yaml`, FQBN `m5stack:esp32:m5stack_cardputer`, ArduinoJson 7.2.1, and the pinned libraries in `vendor/`.

Cardputer ADV voice input must currently be built with M5Stack ESP32 core 3.2.1, whose pinned package uses ESP-IDF 5.4.1. ESP-IDF 5.5.x has a confirmed legacy-I2S regression for the ADV ES8311 microphone that returns a constant sample value instead of audio. The isolated core can be installed without changing the Arduino IDE's global core:

```powershell
arduino-cli core install m5stack:esp32@3.2.1 --config-file toolchain/arduino-cli.yaml
```

The source contains serial-safe diagnostics at 115200 baud: `STATUS`, `SELFTEST`, `STORAGETEST`, `APITEST`, `TOOLTEST`, `WEBTEST`, `FETCHTEST`, `SEARCHTEST`, `E2ETEST`, `STTTLS`, `STTAUTH`, `TTSHW`, `TTSTLS`, `TTSAUTH`, and `TTSTEST`. `STATUS` reports chat/file/search/TTS readiness without names or contents. `STORAGETEST` performs temporary chat and file round trips on microSD and removes both test records. `APITEST` sends a fixed prompt using credentials already stored in NVS. `TOOLTEST` verifies proxy tool-calling with a temporary file and removes it. `WEBTEST` runs a fixed search using the stored search configuration. `FETCHTEST` extracts a fixed public HTTPS page through Exa Contents. `SEARCHTEST` verifies the complete model-to-search-to-model round trip. `E2ETEST` creates a temporary chat, submits `/search cardputer zero` through the same streaming, rendering, tool, and persistence path as keyboard input, restores the original chat, and removes the temporary chat. `TTSHW` plays a local PCM test without a network or key. `TTSTLS` verifies the default ElevenLabs endpoint without a key and expects its authenticated HTTP rejection. `TTSAUTH` validates stored TTS credentials without generating billable audio. `TTSTEST` generates and plays one fixed English phrase. API tests print only pass/fail metadata and never print a key or response content. None of these commands returns credentials, the Wi-Fi SSID, audio, file contents, or model response text.

Cardputer ADV supports 2.4 GHz Wi-Fi. If connection fails, press Fn+4 and choose the Wi-Fi menu to select a visible 2.4 GHz network and enter its password.

The on-device Wi-Fi flow scans nearby networks, shows signal strength and security state, accepts a masked password, verifies the connection, and only then commits the new credentials to NVS. The web setup remains available for API key and base URL changes.

