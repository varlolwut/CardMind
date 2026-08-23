# Limits and compatibility

## Hardware

- Cardputer ADV uses ESP32-S3FN8: 8 MB flash, 512 KB internal SRAM, and no PSRAM.
- Wi-Fi is 2.4 GHz only.
- Display resolution is 240 × 135 pixels.
- Persistent chats, workspace files, audio, logs, and backups require microSD.
- Release builds pin the M5Stack ESP32 board package to 3.2.1. Version 3.3.9 is not
  supported because it produces a silent microphone stream on Cardputer ADV with the
  pinned M5Unified audio implementation.

## Current firmware limits

- Up to 20 chats.
- Active context: 64 messages and 32,768 UTF-8 bytes per chat; older complete turns
  are archived on microSD.
- Chat instructions: 2,048 bytes.
- Workspace: up to 40 supported files; 491,520 bytes per file.
- Supported workspace formats: `.txt`, `.md`, `.json`, `.jsonl`, `.csv`, `.html`,
  `.svg`, and `.py`; text must be valid UTF-8.
- QR display: up to 320 UTF-8 bytes. Longer files must be shortened before they can
  be rendered as a QR code on the 240 × 135 display.
- Voice recording: up to 60 seconds. TTS input: up to 5,000 bytes.
- Eight SSH profiles; terminal log rotates at 512 KiB.
- Web sessions expire after 15 minutes of inactivity.

Large files behave as complete files in the UI. The firmware transfers and commits
them incrementally because loading 480 KiB beside Wi-Fi and TLS state is unsafe.

## Provider compatibility

Chat requires OpenAI-compatible streaming in the documented delta shape. A service
that only imitates the endpoint path but returns a different SSE schema is not
compatible. STT, TTS, and search are separate optional APIs.

## Python

The full image includes MicroPython as a separate mode. Scripts live in the shared
`/assistant/files` microSD workspace and are limited to 65,536 bytes each. CardMind,
the model's file tools, the normal Web console, and MicroPython all see the same
files. Open **Web Console → Start Python workspace**. Browser launches hand off the
current authenticated session automatically; launches from the Cardputer show the IP
address and installation password before restart.

This is MicroPython, not desktop CPython. Native CPython wheels, large data-science
packages, subprocesses, and operating-system APIs are unavailable. Only one user
script runs at a time. A runaway script cannot be force-stopped safely; use **Restart
Python** in the browser or restart the device, then edit the script before running it
again.
