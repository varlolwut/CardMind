# CardMind Web console

The Web console turns a Cardputer ADV into a local browser-accessible LLM, file,
SSH, and SFTP terminal. All requests still originate from the Cardputer; the
browser is a local control surface and is not an API proxy hosted elsewhere.

## Start and sign in

1. Connect CardMind to a 2.4 GHz Wi-Fi network.
2. Open the **Web Console** carousel card, then choose **Open Web Console**.
3. Keep the access screen open. It shows the local HTTP address and the installation
   password for this device.
4. Open the address from a device on the same local network and enter the password.

The installation password is stable for one installation. It is stored in NVS,
shown only on the Cardputer, and is never returned by the Web console API. Press the
plain Esc-marked key on the Cardputer to close the console and all active browser and
SSH sessions. The device returns to the Web Console menu without restarting.

## Interface

The responsive interface is split into four sections. Desktop and tablet layouts use
a persistent sidebar and compact top bar. A phone uses a thumb-accessible fixed bottom
navigation bar, single-column cards, and places the live SSH terminal before profile
configuration and SFTP controls.

### Chat

- Select or create a project, then use the two-pane desktop layout to keep its chats
  beside the active thread; on a phone the active conversation and composer appear
  first.
- Edit project instructions, model override, context budget, output budget, and
  automatic compaction from **Details**. The same panel contains the project's eight
  capability policies plus the chat's separate model and policy overrides.
- Select, create, rename, pin, archive, duplicate, export, or delete chats.
- Load earlier raw turns, clear a chat without deleting it, and monitor total messages
  and active context usage.
- Send prompts with SSE streaming, cancel an active request, or retry the previous
  browser prompt.
- Choose **Auto**, **No tools**, or any combination of Web, Files, SSH, and Python in
  the composer. The one-message selection resets to **Auto** after send and never edits
  persistent chat policy.
- Read compact `W`, `F`, `S`, and `P` states beside the chat title, or the accessible
  eight-row policy/effective/source view in **Details**.
- Review safe pending previews, including bounded file diffs and exact SSH commands,
  and choose **Allow once**, **Allow for chat**, or **Deny** when offered. Mandatory
  confirmation omits **Allow for chat**; an interrupted request can only be
  acknowledged, never replayed.
- Edit instructions that belong only to the active chat.
- Chat history written by the browser is the same microSD-backed history shown on
  the Cardputer.

### Workspace

- Browse the Shared workspace on microSD, including nested UTF-8 paths.
- File inventories are fetched in bounded pages, so the number of workspace entries is
  limited by microSD/filesystem capacity rather than browser or firmware list storage.
- Upload or download arbitrary files up to the microSD/filesystem limits. Only valid
  UTF-8 files with CardMind's safe text extensions are opened in the editor, rendered
  as text QR data, or exposed to model text tools. Other formats remain transfer-only.
- Open a bounded text window, move with Previous and Next, and save the edited window
  back into the same complete file. Replacement is staged before the original changes.
- Link or unlink a selected Shared file for the active project. Linked files are the
  only pre-existing Shared files visible to the model in that project.
- Rename or delete unlinked files and import a selected CardMind project bundle.
- Render up to 320 UTF-8 bytes as a QR code on the Cardputer, either from the QR
  editor or from the complete contents of a selected workspace file.

Incomplete uploads are removed after an error. Existing files are not overwritten
silently.

The browser never needs to hold a complete large document. The Cardputer reads and
replaces bounded text windows and commits through a temporary file, so an interrupted
save does not replace the last valid copy.

Saving does not create automatic file versions. Successful range and whole-file
replacements leave only the current file; target-specific `.tmp` staging and the
short-lived `.bak` recovery copy are removed after commit. Failed validation or
filesystem replacement leaves the current bytes unchanged and cleans recoverable
staging artifacts.

### SSH and SFTP

- Create, select, update, or remove the same named SSH profiles used on-device.
- Connect an interactive PTY. A hardware keyboard can type into the focused terminal;
  the command field supports phone touch keyboards.
- Connection, authentication, and PTY setup run in the background. Their current
  stage remains visible and the rest of the Web console stays responsive.
- The terminal supports arrows, Backspace, Delete, Home/End, Tab, Enter,
  Ctrl+C/D/L, ANSI colors, cursor movement, and alternate-screen applications.
- Confirm a new or changed SHA-256 host fingerprint before authentication.
- Browse remote directories and transfer files between the remote host and the
  CardMind microSD workspace.

Passwords and private-key passphrases are write-only. An uploaded private key is
installed outside the downloadable workspace. Browser terminal output is retained
only in the current page; on-device terminal sessions also use the rotating microSD
scrollback log.

Tool policies apply only to model actions; they do not disable the manual terminal or
SFTP browser. Model access is split into **SSH read**, **SSH mutate**, and **SFTP
read/write** rows at global, project, and chat scope. The current model SSH action is
classified as SSH mutate, requires a complete trusted profile, shows the exact command,
and always requires confirmation even when the effective policy is
**Allow**. No model-facing SSH read or SFTP tool is currently exposed, so those rows
report unavailable; manual terminal and SFTP operations remain available. Imported
project bundles force chat SSH read and SSH mutate policies to **Off**.

Destructive actions and editable names use CardMind dialogs instead of browser-native
prompts, so confirmation remains usable and visually consistent on desktop and mobile.

### Settings

- Change 2.4 GHz Wi-Fi, the OpenAI-compatible base URL, global **Default model**, and
  write-only API key.
- Set the global **Master access** ceiling and the policies copied into new chats for
  all eight capabilities. Project and chat editors may narrow but never exceed it.
- Configure the optional STT, web-search, and TTS providers, automatic speech, and
  speaker volume.
- Adjust brightness, display sleep, keyboard repeat, and the power profile.
- Set the per-chat raw-history quota. `0` uses available microSD capacity; a non-zero
  quota must be 2-4095 MiB. Quota and free-space failures leave the existing archive
  unchanged, and older turns remain available through **Load earlier**.
- Refresh the provider model list and test chat API authentication.
- View and export firmware, reset, uptime, CPU, stack, heap, battery, Wi-Fi, microSD,
  service, and Python diagnostics.
- Temporarily enable detailed request timing for diagnostics. The switch is
  session-only and does not expose API keys, passwords, or SSH secrets.
- Start the installed Python workspace directly from the Settings page.
- Review recent foreground tool activity with tool name, target, status, duration,
  output size, and SSH exit status when present.
- Choose browser-only density, motion, contrast, and terminal wake-lock preferences.

API keys and Wi-Fi passwords are intentionally not returned to the browser. A blank
secret field preserves the saved value; explicit remove controls are provided for
optional services. Select **End session** to close browser/SSH sessions and apply a
changed Wi-Fi network. If the network changed, reopen the console at its new address.

## Python workspace

The **Web Console** device menu and the normal Web Console Settings page both start
an isolated MicroPython workspace. It
reuses the configured 2.4 GHz Wi-Fi network and installation password, then restarts
into the Python image. Starting it from the browser transfers the current IP address
and a one-time handoff token, waits for the restart, and opens the authenticated Python
workspace automatically. Starting it from the Cardputer first shows the IP address and
installation password; press Enter to continue or Esc to cancel.

- create, open, edit, and save `.py` files in the shared `/assistant/files`
  workspace on microSD;
- run one script and inspect its captured output;
- restart the Python environment;
- return to the normal CardMind firmware.

Normal chat, SSH, and Workspace pages are not active while the Python image is
running. Returning to CardMind does not delete scripts. A model running in CardMind
can create a `.py` file with its normal file tool; the same file can then be opened
and executed after switching to MicroPython mode. **Return to CardMind** selects the
CardMind partition and performs the required controlled restart. If the Python page
is unavailable, reconnect the Cardputer to its configured 2.4 GHz network and reopen
the address shown before the mode switch. `RST` alone restarts the selected mode.

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
