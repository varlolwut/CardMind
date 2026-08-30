# Troubleshooting

## Setup and Wi-Fi

- Open `http://192.168.4.1` manually if iOS opens a captive-login window.
- Cardputer ADV cannot join 5 GHz-only networks. Select a 2.4 GHz SSID.
- If the setup AP rejects a known password after a clean flash, forget the old network
  and use the password currently shown on-device.

## Chat and search

- HTTP 404 usually means the base URL and `/v1` were combined incorrectly. Enter the
  documented provider base once.
- An SSE error means the status, content type, event framing, or delta JSON did not
  match the expected contract. Use the connection check and read the explicit body.
- “Unavailable tool” means the model requested a tool that is disabled or unconfigured.
  Configure the separate search connector before selecting **Web** for the next
  message; SSH read, model-facing SFTP, and Python execution also remain unavailable
  when no matching model tool exists.

## Voice

- If the recording meter remains at zero, verify that the firmware was built with
  M5Stack ESP32 board package 3.2.1. Builds made with 3.3.9 can initialize the ADV
  microphone successfully while returning only zero-valued samples.
- A transcript from silence usually indicates a provider hallucination. CardMind
  applies local signal validation, but noisy gain or the wrong microphone path still
  requires checking diagnostics.
- A reconnect during upload points to memory pressure or Wi-Fi loss. Save the reset
  reason and diagnostics bundle before retrying.

## Web console, SSH, and storage

- Both browser and Cardputer must be on the same trusted LAN. VPN routing can hide the
  local address.
- Verify a changed SSH fingerprint out of band; never approve it merely to continue.
- If microSD is missing or read-only, chats, workspace, speech files, and logs cannot
  be persisted. Power down before reseating the card.

If the device restarts, record the exact action, reset reason, free heap, largest block,
and serial output. A reproducible watchdog or panic is a firmware defect, not a normal
recovery path.
