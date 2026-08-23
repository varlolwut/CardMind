# CardMind

Direct text and voice LLM assistant firmware for M5Stack Cardputer ADV. The device connects directly to a configurable OpenAI-compatible chat API, separately configured speech-to-text and text-to-speech APIs, and an optional Exa web search API over verified TLS. Chats, temporary audio, and model-created text files are stored on microSD. No phone or intermediary server is required after setup.

## Releases

Every successful build of `main` publishes a GitHub Release named after the firmware version in `CardputerAssistant.ino`. Each release contains:

- `CardMind-cardputer-adv.bin`: application-only update image for an existing CardMind installation using the 8 MB `default_8MB` partition layout; flash at `0x10000` to preserve NVS settings.
- `CardMind-cardputer-adv-merged.bin`: complete 8 MB installation image with bootloader and partition table; flash at `0x0` for a clean installation. A full-image flash may erase existing device settings.
- `SHA256SUMS.txt`: checksums for both images.

The same files are retained as a workflow artifact for every successful `main` build.
Release publication runs only for `main`; manually dispatched builds of another branch
produce an artifact without replacing a published release.

## Install a release

CardMind is dedicated firmware for **M5Stack Cardputer ADV**. Installing it replaces the application currently running on the device; it does not run alongside Bruce or another firmware.

### Clean installation

1. Open the latest [GitHub Release](https://github.com/varlolwut/CardMind/releases/latest) and download `CardMind-cardputer-adv-merged.bin` and `SHA256SUMS.txt` into the same directory.
2. Verify the download before flashing:

   ```powershell
   Get-FileHash .\CardMind-cardputer-adv-merged.bin -Algorithm SHA256
   Get-Content .\SHA256SUMS.txt
   ```

   The two SHA-256 values for the merged image must match.
3. Install Espressif's `esptool` in a local Python environment if it is not already available:

   ```powershell
   py -m venv .venv
   .\.venv\Scripts\python.exe -m pip install --upgrade esptool
   ```

4. Connect the Cardputer ADV with a data-capable USB cable and find its COM port in Windows Device Manager. The commands below use `COM8` as an example; replace it when necessary.
5. Perform the clean flash. **The erase command deletes the previous firmware, NVS credentials, and setup password. Files and chats on microSD are not erased.**

   ```powershell
   .\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM8 erase_flash
   .\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM8 --baud 921600 write_flash 0x0 .\CardMind-cardputer-adv-merged.bin
   ```

The ESP32-S3 normally enters its bootloader automatically. If connection repeatedly fails, hold G0, press and release Reset, retry the command, and release G0 after `Connecting...` appears.

### Update an existing CardMind installation

Use the application-only image to keep NVS credentials and the installation-specific setup password. This method is valid only when the existing installation already uses CardMind's 8 MB `default_8MB` partition layout.

```powershell
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM8 --baud 921600 write_flash 0x10000 .\CardMind-cardputer-adv.bin
```

Do not run `erase_flash` for an ordinary update. Chats and workspace files live on microSD and remain independent of either update method.

CardMind 1.9 and newer can also check and install a newer published application image
under **Device → Firmware update**. The updater requires the two OTA app partitions,
downloads to microSD, verifies the release asset size and GitHub SHA-256 digest, and
verifies the same digest again while writing the inactive app partition. It never
disables TLS verification and does not erase NVS or microSD data. Updates are manual:
CardMind does not install them in the background.

## First-run configuration

1. Insert a FAT32-formatted microSD card and start CardMind.
2. A clean installation creates a protected Wi-Fi network and shows its unique SSID and password on the display. The password is generated once and remains stable until NVS is erased.
3. Connect a phone or computer to that network. On iOS, choose to keep using the network without Internet. CardMind intentionally does not force the captive-login window.
4. Open `http://192.168.4.1` manually in Safari, Chrome, or another full browser.
5. Select a visible **2.4 GHz** Wi-Fi network, enter its password, and configure the chat service:
   - **API base URL:** HTTPS origin or versioned base, for example `https://api.example.com` or `https://api.example.com/v1`.
   - **API key:** Bearer token issued by that service.
   - **Model:** a model id accepted by the service. CardMind later refreshes the list with `GET /v1/models`.
6. Save the form and wait for CardMind to verify and store the values. The device switches from its setup access point to the selected home network only after the complete form has been received.

The required chat service must implement OpenAI-compatible `POST /v1/chat/completions`, Bearer authorization, and SSE streaming with `choices[0].delta.content`. The base URL is fully configurable and is not tied to a particular provider.

All credentials are stored only in ESP32 NVS and are never compiled into release images or printed to serial. To change them later, open **Fn+4 → Device → Web setup**. Wi-Fi can be changed through **Fn+4 → Network**, which scans nearby 2.4 GHz networks and saves new credentials only after a successful connection.

### Optional voice, search, and speech services

The same web form configures optional services independently from the chat API:

- **Speech to text:** an OpenAI-compatible `/v1/audio/transcriptions` service. The included defaults target Groq and `whisper-large-v3-turbo`, but both URL and model are configurable.
- **Web search:** an Exa Search/Contents API key. Search is invoked only for current-information requests or explicit `/search` and `/web` commands.
- **Text to speech:** ElevenLabs streaming TTS key, model, and voice. Auto playback is off by default; manual playback uses Fn+8.

Missing optional credentials disable only their associated feature and produce an explicit setup error. They do not prevent normal text chat.

## Security and first setup

The firmware contains no Wi-Fi or API credentials. On a clean device it creates a WPA2-protected access point with a unique password shown only on the Cardputer screen. The password is generated once per installation and remains the same across restarts; a full flash erase generates a new one. Connect to that network, choose to use it without Internet, and manually open `http://192.168.4.1` in a full browser. The form scans nearby 2.4 GHz networks, while still allowing a hidden SSID to be entered manually, and accepts the Wi-Fi password, API key, HTTPS API base URL, and model id. Automatic captive redirection is intentionally disabled so iOS password managers remain available. Settings are saved in ESP32 NVS. Credentials are never printed to serial.

The HTTPS clients validate the configured chat endpoint against ISRG Root X1 and the default Groq and Exa endpoints against GTS Root R4, then synchronize the clock before making TLS connections. These embedded PEM certificates are public root trust anchors, not private keys or service credentials. They allow the device to verify server certificates and grant no access to any account or API. They do not call `setInsecure()`. Configurable API base URLs may be entered either as an origin such as `https://api.example.com` or with the version suffix such as `https://api.example.com/v1`; the firmware prevents a duplicate `/v1` path.

## Controls

- Enter: send the current prompt
- Hold G0: record up to 60 seconds; release to transcribe into the editable prompt
- Backspace: delete one UTF-8 code point
- Fn+1: open the separate-chat manager
- Fn+2: open the on-device model picker populated by `GET /v1/models`
- Fn+3: switch English/Russian keyboard layout
- Fn+4: open the animated main carousel
- Left/Right (`,` and `/`, the printed arrow keys): browse carousel cards without Fn
- Fn+5 or Fn+Up (`;` key): scroll toward older messages
- Fn+6 or Fn+Down (`.` key): scroll toward newer messages
- Fn+7: create and switch to a new chat immediately
- Fn+8: speak the latest assistant response through the built-in speaker
- Ctrl+Backspace: clear the current draft
- Esc during chat, search, STT upload, TTS playback, or firmware download: cancel the active operation

The complete controls reference is available on-device under **Fn+4 → Help**. The carousel contains Chats, AI, Voice, Network, Files, Device, Tools, and Help cards. Browse it with the printed Left/Right arrow keys (plain `,` and `/`), press Enter to open a card, and press plain `` ` `` (the Esc-marked key) to go back. Inside lists, including every SSH and SFTP menu, the plain arrow-marked `;` and `.` keys move the selection and Enter confirms it; Fn is not required. In the chat manager, Fn+Delete opens a confirmation screen before deleting the selected chat. During an LLM response or tool chain, Esc cancels the active network stream with an explicit status instead of waiting for completion. Wi-Fi, microSD, and battery status remain visible in the carousel header; battery level is also shown in the chat header. **Tools → Web console** starts a protected console on the current Wi-Fi network. Open the local address shown on the Cardputer and sign in with the installation password displayed only on its screen. The console mirrors chat history, streams prompts, can stop or retry the latest browser request, creates, switches, renames, or deletes chats, and edits per-chat instructions. It can also change the non-secret OpenAI-compatible base URL and model, display Wi-Fi/heap/SD diagnostics, browse large microSD workspace files in 12,288-byte chunks, edit a chunk atomically, upload or download, rename, or delete a file. Uploads stream directly to microSD, reject duplicate names and invalid UTF-8, and remove incomplete data after an error. API keys remain write-only and are never returned to the browser. Press Esc on the Cardputer to close the console without restarting. Sessions expire after 15 minutes of inactivity and mutating requests require a session-specific CSRF token. See [Web console guide](docs/web-console.md) for the complete interface and security reference.

On every normal restart CardMind initializes local storage, opens the main carousel, and connects Wi-Fi and starts NTP synchronization in the background. The model list is fetched only when the model picker is opened. The previously active chat remains selected but is not shown during startup.

The standalone SSH client is under **Tools → SSH tool**. It supports up to eight named profiles, password or PEM private-key authentication, explicit SHA-256 host-key trust, an interactive PTY, and SFTP. Profiles can be created, edited, selected, or removed directly on the Cardputer or through the protected Web console; secrets are write-only in the browser and stay in NVS. A `.pem` or `.key` file can be installed from the microSD workspace on-device, then is moved to `/assistant/ssh` outside the downloadable workspace. A new or changed host key is shown in full and must be explicitly trusted before authentication.

In the on-device terminal, ordinary arrows, Tab, Enter, Backspace, and Ctrl combinations are forwarded to the remote PTY. **Fn+5/Fn+6** scroll output, **Fn+7** returns to live output, **Opt+Up/Down** recalls the 20-command in-memory session history, **Fn+8** opens terminal help, and Esc disconnects. Sanitized scrollback is appended to `/assistant/ssh/terminal.log` and rotates at 512 KiB. The SFTP browser lists remote directories, creates and removes directories, renames remote entries, and transfers files between the remote host and the 491,520-byte microSD workspace. The Web console provides the same stored profiles, an interactive keyboard-driven terminal with reconnect, clear and fullscreen controls, host-key confirmation, plus SFTP listing and bidirectional workspace transfer. SSH runs exclusively while its screen or protected Web console is open and does not overlap memory-heavy LLM, search, STT, or TTS requests.

The chat request uses `POST /v1/chat/completions`, Bearer authorization, and SSE `choices[0].delta.content` streaming. Voice input records 16 kHz mono PCM WAV to `/voice.wav` on microSD, uploads it as multipart form data to the separately configured `/v1/audio/transcriptions` endpoint, then deletes the temporary file. Speech language is detected automatically and is independent of the active keyboard layout. The default STT configuration is Groq `whisper-large-v3-turbo`; its API key is separate from the chat API key.

Speech output uses ElevenLabs `POST /v1/text-to-speech/{voice_id}/stream` with `pcm_16000`, verified against GTS Root R1. The default model is `eleven_multilingual_v2`, which supports Russian and English, and the default voice is George (`JBFqnCBsd6RMkjVDRZzb`). Audio is streamed to temporary `/tts.pcm` on microSD and played in bounded chunks, so a complete recording is never buffered in ESP32 RAM. Playback volume defaults to 75% and cycles through 25%, 50%, 75%, and 100% under **Fn+4 → Voice → TTS volume**; the selected level survives restarts. Configure a separate ElevenLabs key under **Fn+4 → Device → Web setup**, use **Fn+8** for manual playback, or toggle **Auto TTS** under Voice. Auto playback defaults to off to avoid consuming credits unexpectedly. Responses over 5,000 UTF-8 bytes fail with an explicit request for a shorter answer.

## Chats and files

Each conversation has an independent context and a separate versioned JSON file under `/assistant/chats`. The active chat survives restarts. The firmware keeps at most 20 chats, 64 messages and 32,768 UTF-8 bytes of active context per chat; the oldest complete user/assistant pairs are trimmed when needed.

Older complete turns are archived to a streamed 2 MiB microSD journal instead of
being silently discarded. Chat actions show the total context meter and provide
rename, pin, archive, duplicate, retry, Markdown export, and portable `.chat.jsonl`
export. Portable bundles retain the title, per-chat instructions, active messages,
and archived-message boundary; import is available under **Files → Import chat
bundle**. Failed requests retain both the original error and a retryable prompt.
The latest bounded web-search snippets and source URLs are cached on microSD and can
be opened from **Chats → Latest search sources**.

Each chat can also store up to 2,048 UTF-8 bytes of private instructions. Open **Chats → select a chat → Chat instructions**, type a prompt such as `Answer as briefly as possible`, and press Enter to save it. An empty value disables the feature. The instruction is sent as a system message only for that chat and is not displayed or mixed into its visible message history. Existing version-1 chat files are loaded automatically and acquire the new field the next time they are saved.

The model receives four standard OpenAI function tools: `list_files`, chunked `read_file`, `write_file`, and atomic `append_file`. They are restricted to `/assistant/files`, validate filenames and UTF-8, reject traversal, and allow only `.txt`, `.md`, `.json`, `.csv`, `.html`, and `.svg` files. A single file is limited to 491,520 bytes (20 times the original limit); each model call reads or writes at most 12,288 bytes so large files never have to fit in ESP32 RAM. Open **Fn+4 → Files → Browse SD workspace** to create, view, edit, search, copy, rename, or delete files directly on the Cardputer. The viewer and editor operate on 2,048-byte UTF-8 chunks, while saves stream the untouched prefix and suffix through a temporary file before an atomic replacement. In the editor, **Opt+Left/Right** moves the UTF-8 cursor and **Fn+Enter** inserts a newline. Search streams through the complete file without loading it into RAM; one persistent byte-offset bookmark per file is stored separately under `/assistant` and follows copy and rename operations. Existing destinations are never overwritten silently. Choose **Files → Web file manager** to browse, edit, and download files through the protected console on the current Wi-Fi network; Esc returns to CardMind without a restart.

Workspace tools are attached only when the current prompt explicitly mentions a file, microSD, a supported filename extension, or starts with `/file`. This prevents ordinary questions from accidentally entering repeated tool-calling rounds. Mention the filename again in a later follow-up when further file access is required.

The **Tools** carousel card collects active utilities. It provides quick notes and a
checklist stored in the workspace, 5/15/25-minute timers, a bounded arithmetic
calculator, QR rendering, the standalone SSH/SFTP client, and a live one-second
system monitor for battery, Wi-Fi RSSI, heap, stack, microSD, CPU, and uptime. The
longer static diagnostics report remains under Device for troubleshooting. **Device → Backup** creates a
transactional local backup of non-secret preferences, chats, and workspace metadata;
restore requires an explicit on-device confirmation. Wi-Fi passwords, API keys, SSH
passwords, and private keys are excluded.

Current-information prompts and explicit `/search` or `/web` commands attach a `web_search` tool. The OpenAI-compatible chat API relays tool calls, while the Cardputer executes the search itself through the separately configured Exa endpoint and returns bounded source snippets and URLs to the model. Search setup is optional: create an Exa key at `https://dashboard.exa.ai`, then enter it under **Fn+4 → Device → Web setup**. If search is not configured, current-information prompts fail once with an actionable setup message instead of entering repeated tool rounds.

## Build

Use FQBN `m5stack:esp32:m5stack_cardputer`, ArduinoJson 7.2.1, and the pinned libraries in `vendor/`. The local Arduino CLI configuration is generated per checkout and ignored by Git because Arduino CLI stores absolute paths in it.

The optional SSH client is built from the official BSD-3-Clause libssh2 1.11.1
source. Prepare its Arduino package after cloning the upstream source:

```powershell
python tools/prepare_libssh2.py vendor/libssh2 vendor/libssh2-arduino
```

Create the isolated configuration from the repository root. Replace `arduino-cli` with the full executable path when it is not available in `PATH`:

```powershell
$ProjectRoot = (Resolve-Path .).Path
arduino-cli config init --overwrite --dest-file toolchain/arduino-cli.yaml
arduino-cli config set directories.data "$ProjectRoot/toolchain/data" --config-file toolchain/arduino-cli.yaml
arduino-cli config set directories.downloads "$ProjectRoot/toolchain/downloads" --config-file toolchain/arduino-cli.yaml
arduino-cli config set directories.user "$ProjectRoot/toolchain/user" --config-file toolchain/arduino-cli.yaml
arduino-cli config set board_manager.additional_urls https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json --config-file toolchain/arduino-cli.yaml
```

Cardputer ADV voice input must currently be built with M5Stack ESP32 core 3.2.1, whose pinned package uses ESP-IDF 5.4.1. ESP-IDF 5.5.x has a confirmed legacy-I2S regression for the ADV ES8311 microphone that returns a constant sample value instead of audio. The isolated core can be installed without changing the Arduino IDE's global core:

```powershell
arduino-cli core install m5stack:esp32@3.2.1 --config-file toolchain/arduino-cli.yaml
arduino-cli lib install ArduinoJson@7.2.1 --config-file toolchain/arduino-cli.yaml
```

Compile the exact release layout from the repository root:

```powershell
arduino-cli compile `
  --config-file toolchain/arduino-cli.yaml `
  --fqbn "m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB" `
  --libraries vendor `
  --build-path firmware/CardputerAssistant/build/m5stack.esp32.m5stack_cardputer `
  firmware/CardputerAssistant
```

Upload without erasing NVS:

```powershell
arduino-cli upload `
  --config-file toolchain/arduino-cli.yaml `
  --fqbn "m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB" `
  --port COM8 `
  --input-dir firmware/CardputerAssistant/build/m5stack.esp32.m5stack_cardputer `
  firmware/CardputerAssistant
```

The source contains serial-safe diagnostics at 115200 baud: `PING`, `STATUS`, `SELFTEST`, `CANCELTEST`, `STORAGETEST`, `CHATQOLTEST`, `FILETEST`, `DEVICESETTINGSTEST`, `BACKUPTEST`, `OFFLINETEST`, `SEARCHCACHETEST`, `OTACHECK`, `OTADOWNLOADTEST`, `OTAINSTALLTEST`, `SSHCHECK`, `SSHPROBE`, `APITEST`, `TOOLTEST`, `WEBTEST`, `FETCHTEST`, `SEARCHTEST`, `E2ETEST`, `STTTLS`, `STTAUTH`, `TTSHW`, `TTSTLS`, `TTSAUTH`, and `TTSTEST`. `PING` provides a side-effect-free `PONG` handshake for deterministic USB CDC automation. `STATUS` reports chat/file/search/TTS readiness, crash-journal state, and the previous bounded operation name without names or contents. CardMind appends a secret-free boot record to `/assistant/diagnostics.log` on microSD with the reset reason and memory headroom; the journal rotates at 64 KiB. `STORAGETEST` performs temporary chat and file round trips on microSD and removes both test records. `FILETEST` creates a maximum-size 491,520-byte file, edits UTF-8 content in its middle, reads it by offset, copies and renames it, then removes every test file. `CHATQOLTEST`, `DEVICESETTINGSTEST`, `BACKUPTEST`, and `OFFLINETEST` exercise portable chats, persistent device preferences, transactional backup, and offline utilities. `CANCELTEST` checks chat, search, STT, and TTS cancellation before network I/O. `OTACHECK` validates current public release metadata and partition layout; `OTADOWNLOADTEST` downloads and SHA-256-verifies the application image on microSD, removes it, and never installs it. `OTAINSTALLTEST` is a physical serial-only release test: it refuses equal or older versions or a non-dual-partition layout, installs a verified newer image, and reboots only after `Update.end()` succeeds. `SSHCHECK` verifies the packaged libssh2 runtime. `SSHPROBE` performs a real host-key handshake without authenticating or printing the fingerprint. `APITEST` sends a fixed prompt using credentials already stored in NVS. `TOOLTEST` verifies proxy tool-calling with a temporary file and removes it. `WEBTEST` runs a fixed search using the stored search configuration. `FETCHTEST` extracts a fixed public HTTPS page through Exa Contents. `SEARCHTEST` verifies the complete model-to-search-to-model round trip. `E2ETEST` creates a temporary chat, submits `/search cardputer zero` through the same streaming, rendering, tool, and persistence path as keyboard input, restores the original chat, and removes the temporary chat. `TTSHW` plays a local PCM test without a network or key. `TTSTLS` verifies the default ElevenLabs endpoint without a key and expects its authenticated HTTP rejection. `TTSAUTH` accepts a valid least-privilege key even when it intentionally lacks unrelated `user_read`; `TTSTEST` is the functional synthesis and playback check. API tests print only pass/fail metadata and never print a key or response content. None of these commands returns credentials, the Wi-Fi SSID, audio, file contents, or model response text.

SSH-specific release checks additionally include `SSHPROFILETEST`, `SSHSESSIONTEST`, `SFTPTEST`, and `SSHDEMOTEST`. They verify multi-profile NVS round trips, trusted-profile PTY authentication, SFTP directory listing, file transfer, and a complete SSH/SFTP/PTY lifecycle against the public read-only Rebex test service without printing secrets, remote names, or remote contents.

Run the repeatable device regression from PowerShell after flashing. `offline` checks
the board, microSD, chats, maximum-size workspace files, settings, backup, utilities,
SSH runtime/profile storage, speaker, and the complete Web-console start/status/exit
lifecycle. `full` adds the configured chat, tool, search, OTA download, SSH/SFTP, STT,
and TTS integrations. It deliberately does not run `OTAINSTALLTEST`, because that
command installs a different release and reboots the device. The log is serial-safe
and contains no credentials or model response content.

```powershell
.\tools\device_regression.ps1 `
  -Port COM8 `
  -BaudRate 115200 `
  -Suite full `
  -LogPath build\device-regression.log
```

Cardputer ADV supports 2.4 GHz Wi-Fi. If connection fails, open **Fn+4 → Network**, select a visible 2.4 GHz network, and enter its password.

The on-device Wi-Fi flow scans nearby networks, shows signal strength and security state, accepts a masked password, verifies the connection, and only then commits the new credentials to NVS. The web setup remains available for API key and base URL changes.

## License

CardMind is open-source software distributed under the [MIT License](LICENSE). Third-party board packages and libraries retain their own licenses.
