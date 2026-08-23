# Getting started

This guide installs CardMind on a Cardputer ADV, completes the protected first-run
setup, and verifies the first streamed response.

## Before you begin

You need:

- M5Stack Cardputer ADV;
- a data-capable USB-C cable;
- a FAT32-formatted microSD card;
- a 2.4 GHz Wi-Fi network;
- an HTTPS OpenAI-compatible chat endpoint, API key, and model id;
- Python 3 for `esptool`, or another ESP32-S3 flashing tool.

Installing CardMind replaces the firmware already on the device. Files on microSD are
not erased by a flash operation, but a clean flash erases NVS credentials and the
installation password.

## Download and verify a release

Download these files from the
[latest release](https://github.com/varlolwut/cardmind/releases/latest):

- `CardMind-cardputer-adv-full.bin` for a clean installation, including the Python
  workspace and recovery image;
- `CardMind-cardputer-adv.bin` for updating an existing CardMind installation;
- `SHA256SUMS.txt` for verification.

Verify the selected image before flashing:

```powershell
Get-FileHash .\CardMind-cardputer-adv-full.bin -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

The image hash must match the corresponding entry exactly.

## Clean installation

Create a local Python environment and install Espressif's flashing tool:

```powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install --upgrade esptool
```

Connect the Cardputer, find its COM port in Device Manager, and replace `COM8` in the
following commands when necessary:

```powershell
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM8 erase_flash
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM8 --baud 921600 write_flash 0x0 .\CardMind-cardputer-adv-full.bin
```

If automatic bootloader entry repeatedly fails, switch the Cardputer off, hold G0,
apply power, retry the command, and release G0 after `Connecting...` appears.

## Update an existing CardMind installation

Use the application image at `0x10000` only when the same full CardMind layout is
already installed. Moving from CardMind 1.10 or another firmware requires the full
image because an application-only update cannot add the Python and recovery regions.

```powershell
.\.venv\Scripts\python.exe -m esptool --chip esp32s3 --port COM8 --baud 921600 write_flash 0x10000 .\CardMind-cardputer-adv.bin
```

Do not run `erase_flash` for a normal update. NVS settings and microSD data remain
intact. CardMind can also install a newer release from **Device → Firmware update**
after the first full installation. The verified application is downloaded to
microSD, installed by the isolated recovery mode, and checked again before CardMind
boots.

## First-run setup

1. Insert the FAT32 microSD card and restart CardMind.
2. Read the setup SSID and unique password from the Cardputer display. The password is
   generated once and remains stable until NVS is erased.
3. Join that network from a phone or computer. On iOS, choose to keep using the
   network without Internet.
4. Open `http://192.168.4.1` manually in a full browser. CardMind intentionally avoids
   a forced captive-login window so password managers and other apps remain usable.
5. Select a visible 2.4 GHz network or enter a hidden SSID, then enter its password.
6. Enter the OpenAI-compatible HTTPS base URL, API key, and model id.
7. Save the complete form. CardMind verifies and stores it before switching away from
   the setup network.

The base URL may be an origin such as `https://api.example.com` or a versioned base
such as `https://api.example.com/v1`. CardMind prevents a duplicate `/v1` segment.

## Verify the installation

1. Wait for the carousel header to show Wi-Fi and microSD status.
2. Open **Chat** and type a short prompt.
3. Press Enter. A valid connection shows the assistant response as it streams.
4. Open **AI → Models** to verify model discovery when the provider implements
   `GET /v1/models`.

Optional STT, TTS, and web search can be configured later. Their absence does not
disable text chat. Continue with the [User guide](user-guide.md) and
[Configuration](configuration.md).
