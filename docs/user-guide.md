# User guide

CardMind starts on the carousel. Use the plain Left and Right arrow keys to move,
Enter to open a card, and the plain Esc key to return. `Fn` is not required for menu
navigation.

## Chat

- Enter sends text; Up and Down scroll the transcript.
- `F1` clears the active conversation after confirmation.
- `F2` opens AI and model settings.
- `F3` switches the English/Russian keyboard layout.
- Each chat has its own messages, title, archive, and instructions. Instructions are
  a small system prompt such as “Answer in three short bullets”.
- Older complete turns move to the microSD archive when the active context reaches
  its limit. They remain readable but are not sent to the model.

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

The model can list, read, create, and append workspace files when file tools are
enabled. It cannot access credentials, NVS, firmware, or arbitrary microSD paths.

## Tools and device management

Tools contains notes, checklists, timer, calculator, QR display, SSH, and diagnostics.
Web Console has its own carousel card with the local address and configuration shortcut.
Device contains network, firmware update, storage, backup, battery,
display, and reset actions. Help lists all shortcuts available in the active screen.

Back up chats and workspace data before replacing a microSD card. A normal firmware
update preserves NVS and microSD. A clean flash erases NVS, including Wi-Fi, service
keys, and the installation password.
