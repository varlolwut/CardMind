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
project bundle. The effective model is chosen in order from the global **Default
model**, the project override, and the optional chat override; a blank narrower
override inherits the broader value.

- Enter sends text; Up and Down scroll the transcript.
- `Fn+1` opens Chats; its action menu can clear messages without deleting the chat.
- `Fn+2` opens **Next capabilities**.
- `Fn+3` switches the English/Russian keyboard layout.
- `Fn+4` returns to the carousel, `Fn+7` creates a chat, and `Fn+8` speaks the
  latest answer when TTS is configured. Press `Esc` to stop TTS download or playback.
- Each project and chat has its own eight capability policies. **Inherit** uses the
  broader policy; **Off**, **Ask**, and **Allow** may narrow access, but cannot raise it
  above the global **Master access** ceiling. Chat instructions override conflicting
  project instructions.
- Open **Next capabilities** in chat actions to use **Auto**, **No tools**, or require
  any combination of Web, Files, SSH, and Python for the next message. This selection
  resets to **Auto** after send and never changes the stored chat policy. If a required
  capability is denied or unavailable, the request fails explicitly.
- The chat header shows compact `W`, `F`, `S`, and `P` states. **Capability status**
  lists all eight chat policies together with their effective decisions and limiting
  scope, so color is not the only indication.
- An **Ask** decision shows a safe preview and offers **Allow once**, **Allow for
  chat**, and **Deny**. File replacements include a bounded diff, and SSH includes the
  exact command. Mandatory confirmations do not offer **Allow for chat**. A pending
  decision cannot be replayed after a restart; acknowledge or discard it and send a
  new message.
- Complete raw history remains on microSD. Only the model request is reduced to the
  configured context budget. With automatic compaction enabled, CardMind summarizes
  omitted turns and includes that summary in later requests. Use **Regenerate context
  summary** in chat actions when a new summary is needed.
- Raw chat archives have no fixed CardMind size ceiling. They grow within available
  microSD capacity unless a per-chat history quota is configured. A write that would
  exceed the quota or safe free-space reserve fails before changing the archive.
  Device presets are Available capacity, 16, 64, 256, and 1024 MiB.

## Voice and speech

Open Voice from the carousel, choose STT language, and start recording. CardMind
stores temporary PCM on microSD and rejects silent or implausibly short recordings
before contacting the provider. Spoken replies are optional; volume is adjustable in
Voice settings.

## Search

Choose **Web** under **Next capabilities** to require Web access for one prompt, or
leave the selection on **Auto** and let the model choose among allowed tools. Search
and page extraction are separate capabilities. CardMind shows each tool stage and
reports provider errors directly. Search is unavailable until its endpoint and key
are configured.

## Shared workspace

Files appear as ordinary documents: select a file, open it, move through the document,
edit the visible text window, and save it. Nested UTF-8 paths are supported. Large
files are streamed internally; the document remains one file rather than a set of
user-visible parts. Storage and editing limits are listed in [Limits](limitations.md).

Text editing, QR text display, and model text tools are limited to these
case-insensitive extensions: `.txt`, `.md`, `.json`, `.jsonl`, `.csv`, `.html`,
`.svg`, `.py`, `.yaml`, `.yml`, `.toml`, `.ini`, `.cfg`, `.conf`, `.log`, `.xml`,
`.css`, `.js`, `.mjs`, `.cjs`, `.ts`, `.tsx`, `.sh`, `.bash`, `.zsh`, `.c`, `.h`,
`.cc`, `.cpp`, `.cxx`, `.hh`, `.hpp`, `.hxx`, `.ino`, `.env`, `.properties`, and
`.sql`. Other files are transfer-only: they can still be uploaded, downloaded,
renamed, linked, unlinked, deleted, and transferred over SFTP without being decoded
as text.

Workspace file count and file size have no CardMind-specific ceiling. Lists are read in
bounded pages, while available microSD capacity, filesystem limits, 32-bit file offsets,
and the safe free-space reserve define the practical limits. The model's `list_files`
tool reads at most 16 source entries per call and follows `next_offset` until `eof=true`;
the complete directory is never accumulated in RAM.

CardMind does not create automatic versions when a file is saved. A replacement is
written to a target-specific `.tmp` file, the previous file is kept only as a
short-lived `.bak` recovery copy, and both artifacts are removed after a verified
commit. A rejected save keeps the current file unchanged and removes its staged data;
after an interrupted commit, recovery preserves the last valid copy. Export the
affected project bundle or copy the needed workspace file off the card when you need
long-term version retention.

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
The **AI** carousel card contains the **Default model**, API and service setup, global
instructions, **Master access**, **Defaults for new chats**, pending confirmations, and
recent tool activity.
Device contains display and power preferences, API setup, firmware update, and
diagnostics. Network and Python controls have dedicated carousel
cards. Help lists the global shortcuts; feature screens show their local controls.

Export needed project bundles and copy other workspace data off the card before
replacing a microSD card. A normal firmware update preserves NVS and microSD. A clean
flash erases NVS, including Wi-Fi, service keys, and the installation password.
