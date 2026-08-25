# Projects and Shared workspace

CardMind stores durable project, chat, and file data on microSD. Projects isolate
conversation context while the Shared workspace lets selected files be reused without
duplicating their contents.

## Projects and chats

A project contains any number of paginated chats. Creating, renaming, duplicating,
archiving, and deleting a project never changes another project. Each project keeps an
active chat and these defaults:

- instructions applied to every chat;
- an optional model override;
- request-context budget from 8 KiB to 256 KiB;
- maximum response tokens;
- automatic context compaction on or off.

On the Cardputer, select a project and press Enter to open **Project Actions**. This
screen changes the project model, instructions, context budget, response budget, and
compaction preference without requiring the Web Console. The same fields are available
under **Chat → Project details** in the Web Console.

Configuration is resolved from broad to narrow: global settings, project settings,
then chat settings. A narrower value overrides only the field it defines. For example,
a project may request concise answers while one chat asks for a detailed table.

Raw messages are appended to the project's microSD history. When they no longer fit
the request budget, CardMind omits the oldest complete messages from the API request.
If automatic compaction is enabled, the selected model creates a factual summary for
the omitted range. The summary helps later requests, but it never replaces or deletes
the raw history. Chat actions can display earlier messages and regenerate the summary.

## Shared files and project links

The Shared workspace is physically stored under CardMind's microSD data directory and
is visible from Files, Web Console, SFTP, and MicroPython. A Shared file is not visible
to the model merely because it exists. Link it to a project from the file actions or
the Web Console before asking the model to read or append it.

Links contain only the normalized relative path. One file can be linked to several
projects without copying data, and deleting a project removes its links rather than
the Shared files. A linked file must be unlinked from every project before it can be
renamed or deleted. Files successfully created by a model tool are linked to the
active project automatically.

Nested UTF-8 paths such as `research/заметки/result.md` are supported. CardMind rejects
absolute paths, drive prefixes, `.` and `..` segments, control characters, and its
internal temporary or recovery suffixes.

## Large files and safe saves

A file may be up to 256 MiB when the microSD card has enough free space. Upload,
download, copy, search, SFTP, and project bundle operations stream data instead of
loading the complete file into RAM. Text editors move through one bounded window while
the user continues to work with a single file.

An edited window is written to a staged replacement, reopened and checked, and only
then exchanged with the original. The short-lived recovery copy is removed after a
successful commit. If power, media, or free space fails before commit, the previous
complete file remains the recovery source. CardMind does not retain automatic version
history.

## Project bundles

**Export project** creates one portable project bundle in Shared workspace. It contains
the project settings, chat metadata, complete raw histories, summaries, and Shared-link
paths. It does not copy linked Shared file contents and does not export API keys, Wi-Fi
credentials, SSH secrets, or installation credentials. Model SSH permission is reset
to disabled when a bundle is imported.

Import validates the bundle and creates a new project. Existing projects and Shared
files are not overwritten. Recreate or relink Shared files that are not already present
on the destination card.

## Upgrade migration

On the first P2 boot, CardMind creates storage schema v2, imports existing chats into a
project named **Default**, and adopts the existing workspace as **Shared workspace**.
The migration is staged and validated before the new schema becomes active. If power is
lost during staging, CardMind discards the incomplete staging tree and retries without
modifying the legacy source data.

Keep a normal backup before changing or reformatting the microSD card. Firmware update
alone does not erase NVS or microSD data.
