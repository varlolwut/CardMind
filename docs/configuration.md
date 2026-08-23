# Service configuration

CardMind stores secrets in NVS and non-secret workspace data on microSD. Secret
fields are write-only: the setup portal and Web console never return their values.

## Chat API

Required fields are an HTTPS base URL, Bearer API key, and model id. CardMind uses:

- `GET /v1/models` for optional model discovery;
- `POST /v1/chat/completions` with `stream: true`;
- SSE text from `choices[0].delta.content`.

The base may end at the origin or `/v1`; CardMind normalizes it once. TLS certificate
and hostname verification are mandatory.

## Speech and search

STT, TTS, web search, and page extraction are independent connectors. Configure only
the services you intend to use. A provider key is never shared with another connector
unless the user enters the same value explicitly.

STT must accept recorded audio and return transcript text. TTS must return a supported
audio stream. Search must return grounded result objects; page extraction accepts a
URL produced by search. CardMind reports schema and HTTP failures instead of silently
switching providers.

## Wi-Fi and local access

Cardputer ADV supports 2.4 GHz Wi-Fi only. The first-run page scans nearby networks
and still permits a hidden SSID. The protected setup AP password is generated once
per installation and remains stable until NVS is erased.

The Web console has a separate installation password shown on-device. See
[Web console](web-console.md) and [Security](security.md).

The protected Web console can replace write-only credentials and all provider
settings. Leave a secret field blank to preserve it. Optional STT, search, and TTS
keys also have explicit remove controls. A changed Wi-Fi network is applied after
**End session** closes the local console; reopen it at the address shown afterward.
