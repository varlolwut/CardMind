# Security model

- Wi-Fi passwords, API keys, SSH secrets, and installation credentials are stored in
  ESP32 NVS and are never compiled into releases or returned by APIs.
- Public CA certificates in source are trust anchors, not secrets. They authenticate
  remote servers; private keys must never be committed.
- HTTPS connectors verify both the certificate chain and hostname. CardMind does not
  use insecure TLS mode.
- The Web console is local HTTP because a browser-trusted certificate cannot be
  provisioned for an arbitrary LAN address. It uses an HttpOnly SameSite session,
  CSRF tokens, inactivity expiry, login throttling, and `no-store` responses.
- SSH uses explicit trust-on-first-use. A changed host key blocks authentication.
- Firmware updates require a verified digest before installation.

microSD is removable and is not encrypted by CardMind. Anyone with physical access
can read chats, workspace documents, logs, and temporary audio. Keep secrets out of
workspace files and remove the card before lending the device.

Do not expose the Web console through router port forwarding. Use it only on a trusted
LAN and end the session when finished.
