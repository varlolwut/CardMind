# CardMind Web console

The Web console turns a Cardputer ADV into a local browser-accessible LLM, file,
SSH, and SFTP terminal. All requests still originate from the Cardputer; the
browser is a local control surface and is not an API proxy hosted elsewhere.

## Start and sign in

1. Connect CardMind to a 2.4 GHz Wi-Fi network.
2. Open **Tools → Web console** on the Cardputer.
3. Keep the access screen open. It shows the local HTTP address and the installation
   password for this device.
4. Open the address from a device on the same local network and enter the password.

The installation password is stable for one installation. It is stored in NVS,
shown only on the Cardputer, and is never returned by the Web console API. Press the
plain Esc-marked key on the Cardputer to close the console and all active browser and
SSH sessions. The device returns to Tools without restarting.

## Interface

The responsive interface is split into four sections. On a narrow phone display the
section buttons scroll horizontally.

### Chat

- Select, create, rename, pin, archive, duplicate, export, or delete chats.
- Send prompts with SSE streaming, cancel an active request, or retry the previous
  browser prompt.
- Edit instructions that belong only to the active chat.
- Chat history written by the browser is the same microSD-backed history shown on
  the Cardputer.

### Files

- Browse the `/assistant/files` workspace on microSD.
- Upload or download supported UTF-8 files.
- Read and atomically edit files in 12,288-byte chunks without loading a complete
  large document into ESP32 RAM.
- Rename or delete files and import a selected CardMind chat bundle.

Incomplete uploads are removed after an error. Existing files are not overwritten
silently.

### SSH and SFTP

- Create, select, update, or remove the same named SSH profiles used on-device.
- Connect an interactive PTY. A hardware keyboard can type into the focused terminal;
  the command field supports phone touch keyboards.
- Confirm a new or changed SHA-256 host fingerprint before authentication.
- Browse remote directories and transfer files between the remote host and the
  CardMind microSD workspace.

Passwords and private-key passphrases are write-only. An uploaded private key is
installed outside the downloadable workspace. Browser terminal output is retained
only in the current page; on-device terminal sessions also use the rotating microSD
scrollback log.

### Settings

- Change the non-secret OpenAI-compatible base URL and model.
- View battery, Wi-Fi RSSI, heap, largest free block, and microSD usage.

API keys and Wi-Fi credentials are intentionally not returned to the browser. Use
the on-device setup portal when a secret must be replaced.

## Security model

- The server listens only while the Web console screen is open on the Cardputer.
- Sessions expire after 15 minutes without an authenticated request.
- Authentication uses an HttpOnly, SameSite=Strict session cookie.
- State-changing requests require a session-specific CSRF token.
- Five failed password attempts cause a 30-second lockout.
- The page and API send `Cache-Control: no-store` where credentials or private state
  could otherwise remain in browser caches.
- SSH host keys use explicit trust-on-first-use and changed keys require a new
  confirmation.

The local console uses HTTP because the Cardputer cannot provision a browser-trusted
certificate for an arbitrary private LAN address. Treat the connected Wi-Fi network
as trusted, close the console when finished, and do not expose port 80 through router
port forwarding.

## Troubleshooting

- If the address does not open, confirm both devices are on the same LAN and that a
  VPN is not routing the private Cardputer address away from Wi-Fi.
- Cardputer ADV supports 2.4 GHz Wi-Fi only.
- If a browser session expires, sign in again with the password shown on-device.
- If SSH reports a changed fingerprint, verify the server out of band before
  accepting it.
- Pressing the plain Esc-marked key always closes the Web console. If a network or
  model operation is active, the same key first cancels that operation and then the
  next press closes the console.
