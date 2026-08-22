# CardMind development roadmap

`main` contains hardware-tested releases. Ongoing integration work is performed in
`develop`; focused feature branches merge into `develop`, and a validated snapshot
is promoted to `main`. Secrets must never be committed. Every release candidate
must pass host tests, a clean firmware build, and the hardware checklist on a
Cardputer ADV.

## P0: stability and navigation foundation

- Use one explicit screen and navigation state model across the carousel, chat,
  settings, files, voice, and diagnostics.
- Make plain arrow keys, Enter, and Esc consistent on every screen, with accurate
  footer hints and short carousel-style transitions.
- Keep startup, Wi-Fi, model refresh, and long network operations non-blocking so
  they cannot overwrite the active screen.
- Add cancellation for chat, search, speech recognition, speech playback, and
  downloads.
- Write a secret-free crash journal to microSD with reset reason, free heap,
  largest free block, stack headroom, and the last safe operation.
- Run the hardware regression checklist for chat streaming, web search, STT, TTS,
  microSD, file access, repeated requests, and reboot recovery.

## P1: files and documents

- Add a UTF-8 editor with create, open, save, save as, rename, and delete.
- Use chunked access and atomic replacement for files up to 491,520 bytes without
  loading the whole document into RAM.
- Add overwrite confirmation, find, page navigation, and bookmarks.
- Provide reader modes for text, Markdown, CSV, JSON, and HTML source.
- Add document speech playback for a page, selection, or document, with volume,
  pause, resume, and stop controls.
- Import, export, upload, and download workspace files through the web interface.

## P1: protected web console

- Add a responsive local console that mirrors the streaming Cardputer chat.
- Send, edit, cancel, and retry prompts from a phone or computer.
- Create, switch, rename, archive, and delete chats; edit per-chat instructions.
- Select models and manage non-secret connection settings.
- Browse, upload, download, and edit microSD workspace files.
- Show battery, memory, storage, Wi-Fi, and service diagnostics.
- Keep credentials write-only and masked; require session authentication and an
  idle timeout.

## P1: SSH client feasibility and implementation

- Prototype wolfSSH against the pinned M5Stack ESP32 3.2.1 toolchain and measure
  firmware size, heap fragmentation, stack headroom, and connection stability.
- Require host-key fingerprint confirmation and persist trusted hosts explicitly.
- Support password and public-key authentication without logging credentials.
- Add a PTY terminal with a practical ANSI/VT100 subset and microSD scrollback.
- Add remote command execution and evaluate SFTP after the interactive client is
  stable.
- Do not run SSH concurrently with memory-heavy LLM, search, STT, or TTS work until
  hardware measurements prove that the combination is safe.

## P2: chat and search quality of life

- Rename, pin, archive, duplicate, export, and import chats.
- Show an explicit context-size meter and offer controlled summarization or
  archival of old turns instead of silently discarding them.
- Preserve unsent drafts when switching screens or recovering from an error.
- Add clear retry and cancel actions with the original API or network error.
- Show search sources in a dedicated viewer, retain source URLs, and cache bounded
  snippets on microSD.

## P2: device utilities

- Add brightness, sleep, keyboard repeat, speaker volume, and power profiles.
- Support verified firmware updates from GitHub releases while preserving NVS and
  microSD data; evaluate rollback partitions before enabling unattended updates.
- Back up and restore non-secret settings, chats, and workspace metadata.
- Add compact offline tools where they improve the device: notes and checklists,
  timer, calculator, QR display, and system monitor.

## Network scope

NaiveProxy is not planned as an on-device component. Its Chromium network-stack
dependency and browser-fingerprint goal do not fit the ESP32-S3 platform. CardMind
can use a router, travel router, OpenWrt device, or other gateway that runs
NaiveProxy for the whole Wi-Fi network. Any future embedded tunnel or proxy client
will be evaluated separately and will not be described as NaiveProxy-compatible
without a real interoperable implementation.
