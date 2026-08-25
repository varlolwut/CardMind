# CardMind

CardMind is open-source assistant firmware for the **M5Stack Cardputer ADV**. It
connects directly to configurable OpenAI-compatible chat, speech, and web-search
services over verified TLS. Chats, workspace files, temporary audio, and diagnostic
records live on microSD; credentials stay in ESP32 NVS.

CardMind is dedicated firmware. Installing it replaces the application currently on
the device; it does not run alongside Bruce or another firmware.

## What it does

- SD-backed projects with independent chats, project defaults, per-chat instructions,
  configurable context budgets, SSE streaming, and English/Russian keyboard layouts.
- Voice input through a configurable OpenAI-compatible transcription endpoint and
  optional spoken replies through a configurable TTS service.
- Model-assisted web search and page extraction through an optional search service.
- A Shared microSD workspace with nested UTF-8 paths, explicit project links, and files
  up to 256 MiB using bounded viewing, editing, upload, download, and SFTP transfers.
- Standalone SSH terminal and SFTP browser on the Cardputer and in the protected Web
  console.
- A MicroPython mode for small scripts stored in the same microSD workspace that
  CardMind and the model use.
- Offline notes, checklists, timers, calculator, QR rendering, diagnostics, backup,
  and verified firmware updates.

## Quick start

1. Download the full image and `SHA256SUMS.txt` from the
   [latest release](https://github.com/varlolwut/cardmind/releases/latest).
2. Verify the checksum, connect the Cardputer ADV with a data-capable USB cable, and
   flash `CardMind-cardputer-adv-full.bin` at `0x0`.
3. Insert a FAT32-formatted microSD card and restart the device.
4. Join the protected setup network shown on the Cardputer display and open
   `http://192.168.4.1` manually.
5. Select a **2.4 GHz** Wi-Fi network and enter an OpenAI-compatible HTTPS base URL,
   API key, and model id.
6. Return to the carousel, open Chat, and send a prompt.

The complete flashing commands, clean-update distinction, and first-run flow are
described in [Getting started](docs/getting-started.md).

## Requirements and important limits

- Hardware: M5Stack Cardputer ADV with 8 MB flash; a microSD card is strongly
  recommended and required for persistent chats, files, and audio workflows.
- Wi-Fi: 2.4 GHz only. The ESP32-S3 radio cannot join a 5 GHz-only network.
- Chat API: OpenAI-compatible `POST /v1/chat/completions` with Bearer authorization
  and SSE `choices[0].delta.content`; model discovery uses `GET /v1/models`.
- TLS verification is always enabled. Endpoints with an unsupported private or custom
  CA fail explicitly.
- Raw chat history is retained on microSD. Each project has a configurable 8–256 KiB
  request-context budget and optional model-generated compaction; compaction never
  deletes the original turns.
- A workspace file is limited to 256 MiB. Text editors expose one bounded window at a
  time; uploads, downloads, search, copying, and SFTP are streamed.
- Cardputer ADV has no PSRAM. Memory-heavy network operations are deliberately
  serialized.

See [Limits and compatibility](docs/limitations.md) before choosing providers or
designing large workflows.

## Documentation

The [documentation index](docs/README.md) separates installation, daily use,
configuration, security, limits, and troubleshooting.

- [Getting started](docs/getting-started.md)
- [User guide and controls](docs/user-guide.md)
- [Projects and Shared workspace](docs/projects-workspace.md)
- [Service configuration](docs/configuration.md)
- [Web console](docs/web-console.md)
- [SSH and SFTP](docs/ssh-sftp.md)
- [Limits and compatibility](docs/limitations.md)
- [Security model](docs/security.md)
- [Troubleshooting](docs/troubleshooting.md)

## License

CardMind is distributed under the [MIT License](LICENSE). Third-party board packages
and libraries retain their own licenses; see the
[third-party notices](THIRD_PARTY_NOTICES.md) for the components included in release
images. Releases include a license bundle and the exact LGPL-covered Arduino core
source archive used to build the firmware.
