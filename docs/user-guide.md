# User guide

CardMind starts on the carousel. Use the plain Left and Right arrow keys to move,
Enter to open a card, and the plain Esc key to return. `Fn` is not required for menu
navigation.

## Projects and chat

Projects are the first card in the carousel. A project contains independent chats and
its own model, instructions, context budget, output budget, and compaction preference.
Opening a project shows only its chats, so context is never mixed between projects.
Use Up and Down to select, Enter to open actions, and Esc to move back one level.
Project actions can change the model and instructions or choose **Global default**,
adjust context and response budgets, toggle automatic compaction, and manage the
project bundle.

- Enter sends text; Up and Down scroll the transcript.
- `Fn+1` opens Chats; its action menu can clear messages without deleting the chat.
- `Fn+2` opens model selection.
- `Fn+3` switches the English/Russian keyboard layout.
- `Fn+4` returns to the carousel, `Fn+7` creates a chat, and `Fn+8` speaks the
  latest answer when TTS is configured. Press `Esc` to stop TTS download or playback.
- Each chat has its own messages, title, archive state, instructions, and optional SSH
  permission. Chat instructions override conflicting project instructions.
- Complete raw history remains on microSD. Only the model request is reduced to the
  configured context budget. With automatic compaction enabled, CardMind summarizes
  omitted turns and includes that summary in later requests. Use **Regenerate context
  summary** in chat actions when a new summary is needed.

## Voice and speech

Open Voice from the carousel, choose STT language, and start recording. CardMind
stores temporary PCM on microSD and rejects silent or implausibly short recordings
before contacting the provider. Spoken replies are optional; volume is adjustable in
Voice settings.

## Search

Use `/search question` or enable Web for one prompt. Search and page extraction are
separate tools. CardMind shows each tool stage and reports provider errors directly.
Search is unavailable until its endpoint and key are configured.

## Shared workspace

Files appear as ordinary documents: select a file, open it, move through the document,
edit the visible text window, and save it. Nested UTF-8 paths are supported. Large
files are streamed internally; the document remains one file rather than a set of
user-visible parts. Storage and editing limits are listed in [Limits](limitations.md).

Shared files are not exposed to a project automatically. Link a selected file to the
active project before asking the model to read or append it. A file created by the
model is linked to the active project after a successful write. One Shared file may be
linked to several projects without being copied. The model cannot access credentials,
NVS, firmware, unlinked Shared files, or arbitrary microSD paths.

See [Projects and Shared workspace](projects-workspace.md) for migration, bundles, and
settings precedence.

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
