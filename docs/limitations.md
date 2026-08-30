# Limits and compatibility

## Hardware

- Cardputer ADV uses ESP32-S3FN8: 8 MB flash, 512 KB internal SRAM, and no PSRAM.
- Wi-Fi is 2.4 GHz only.
- Display resolution is 240 × 135 pixels.
- Persistent chats, workspace files, audio, and logs require microSD.
- Release builds pin the M5Stack ESP32 board package to 3.2.1. Version 3.3.9 is not
  supported because it produces a silent microphone stream on Cardputer ADV with the
  pinned M5Unified audio implementation.

## Current firmware limits

- Projects, chats, and Shared workspace files use paginated microSD indexes rather than
  a small in-memory count. A 500-file workspace is exercised by the hardware release
  suite without making 500 a product limit. Bookmark metadata rejects more than
  4,096 entries; that validation ceiling is not a tested operating point on this
  no-PSRAM device.
- Request context is configurable per project from 8 KiB to 256 KiB. The default is
  32 KiB. Complete raw history stays on microSD and does not have a fixed 16- or
  64-message ceiling.
- Project and chat instructions: 16 KiB each. A compacted context summary may use up
  to 128 KiB on microSD.
- A typed or pasted chat prompt is limited to 16 KiB of valid UTF-8.
- Workspace files have no CardMind-specific size ceiling. Available microSD capacity,
  filesystem limits, 32-bit file offsets, and the safe free-space reserve are the
  effective limits. Nested relative UTF-8 paths are supported; absolute paths,
  traversal, control characters, and internal `.tmp`/`.bak` targets are rejected.
- Arbitrary binary files may be stored, downloaded, copied, and transferred over SFTP.
  Editing and model read/append tools require valid UTF-8 text and use bounded internal
  windows of at most 12 KiB per operation.
- QR display: up to 320 UTF-8 bytes. Longer files must be shortened before they can
  be rendered as a QR code on the 240 × 135 display.
- Voice recording: up to 60 seconds. TTS input: up to 5,000 bytes.
- Eight SSH profiles; terminal log rotates at 512 KiB.
- Web sessions expire after 15 minutes of inactivity.

Large files behave as complete files in the UI. The firmware transfers and commits
them incrementally because loading a multi-megabyte document beside Wi-Fi and TLS
state is unsafe. Replacement also needs enough temporary free space plus a small
recovery floor; only the operation that cannot complete is rejected.

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
