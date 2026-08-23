# User guide

CardMind starts on the carousel. Use the plain Left and Right arrow keys to move,
Enter to open a card, and the plain Esc key to return. `Fn` is not required for menu
navigation.

## Chat

- Enter sends text; Up and Down scroll the transcript.
- `Fn+1` opens Chats; its action menu can clear messages without deleting the chat.
- `Fn+2` opens model selection.
- `Fn+3` switches the English/Russian keyboard layout.
- `Fn+4` returns to the carousel, `Fn+7` creates a chat, and `Fn+8` speaks the
  latest answer when TTS is configured. Press `Esc` to stop TTS download or playback.
- Each chat has its own messages, title, archive, and instructions. Instructions are
  a small system prompt such as “Answer in three short bullets”.
- Older complete turns move to the microSD archive when the active context reaches
  its limit. Open **Chats → selected chat → Context** to read the archive. Archived
  turns are not sent to the model.

## Voice and speech

Open Voice from the carousel, choose STT language, and start recording. CardMind
stores temporary PCM on microSD and rejects silent or implausibly short recordings
before contacting the provider. Spoken replies are optional; volume is adjustable in
Voice settings.

## Search

Use `/search question` or enable Web for one prompt. Search and page extraction are
separate tools. CardMind shows each tool stage and reports provider errors directly.
Search is unavailable until its endpoint and key are configured.

## Workspace

Files appear as ordinary documents: select a file, open it, edit it, and save it.
Large files are streamed internally; implementation windows are never shown as parts
of the document. Supported text formats are listed in [Limits](limitations.md).

The model automatically receives list, read, create, and append tools when a request
explicitly concerns a file, document, note, or script. You can also begin a request
with `/file`. It cannot access credentials, NVS, firmware, or arbitrary microSD paths.

## Tools and device management

Tools contains notes, checklists, timer, calculator, QR display, SSH, and a compact
system monitor. The extended two-page diagnostics view is under **Device**.
Web Console has its own carousel card with the local address, configuration shortcut,
and **Start Python workspace** action.
Device contains display and power preferences, backup and restore, API setup,
firmware update, and diagnostics. Network and Python controls have dedicated carousel
cards. Help lists the global shortcuts; feature screens show their local controls.

Back up chats and workspace data before replacing a microSD card. A normal firmware
update preserves NVS and microSD. A clean flash erases NVS, including Wi-Fi, service
keys, and the installation password.
