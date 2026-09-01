# Security model

## Logical controls

- Wi-Fi passwords, API keys, SSH passwords, SSH key passphrases, private-key records,
  and installation credentials are stored in ESP32 NVS. They are not compiled into
  releases or returned by read APIs.
- Model and workspace tools cannot address NVS, credential records, private-key
  records, or their internal identifiers. Write-only UI and API fields prevent an
  ordinary authenticated client from reading a stored secret back.
- Public CA certificates in source are trust anchors, not secrets. HTTPS connectors
  verify both the certificate chain and hostname; CardMind does not use insecure TLS
  mode.
- The Web console is local HTTP because a browser-trusted certificate cannot be
  provisioned for an arbitrary LAN address. It uses an HttpOnly SameSite session,
  CSRF tokens, inactivity expiry, login throttling, and `no-store` responses.
- SSH uses explicit trust-on-first-use. In normal operation, a changed host key blocks
  authentication until the exact trusted entry is deliberately removed and the host
  is reviewed again.
- Firmware updates through CardMind require a verified digest before installation.

These are software access controls. They do not encrypt stored data and do not protect
against an attacker who controls the hardware or replaces the firmware.

## Data at rest

| Location | Examples | Current protection |
| --- | --- | --- |
| ESP32 NVS | Wi-Fi and provider credentials, installation password, SSH passwords/passphrases, private-key records | No CardMind encryption-at-rest claim. The supported build does not enable or verify NVS encryption, flash encryption, or secure boot. Treat these values as plaintext to an attacker with physical flash or debug access. |
| Removable microSD | Chats, workspace files, command-output and terminal logs, temporary audio, public SSH `known_hosts` data | Not encrypted or integrity-protected by CardMind. The card can be read or modified outside the device. Command output may contain sensitive remote data even though host fingerprints are not credentials. |
| Volatile runtime state | The selected SSH secret/key, browser session state, active terminal data | Held only while needed by the running software and cleared by its normal lifecycle. This is not protection from a powered-device, debug, memory-corruption, or modified-firmware attacker. |

The exact supported firmware configuration has no NVS key partition, does not mark the
NVS partition as encrypted, and does not enable NVS encryption, flash encryption, or
secure boot. CardMind does not inspect or assert a particular device's eFuse state, so
a separately provisioned board must not be assumed to have a supported or verified
encrypted-storage configuration.

Physical access can therefore expose NVS credentials, read or alter microSD content,
replace trusted-host data, or install firmware that observes secrets while CardMind is
using them. Digest verification in the normal update flow does not provide secure-boot
protection against a physical flashing/debug path. Ordinary file deletion, profile
deletion, or card formatting is not documented as forensic secure erase; CardMind does
not verify that every prior flash page or card block has been overwritten.

Removing microSD and keeping it separately protects only that card from the person who
has the device; it does not protect secrets in NVS. After loss, theft, repair by an
untrusted party, or unexplained physical access, treat the device and card as
compromised: revoke or rotate provider and SSH credentials, revalidate SSH host keys,
and reprovision the device before trusting it again. Avoid putting secrets in workspace
files or remote commands whose output will be retained in logs.

## Network boundary

Do not expose the Web console through router port forwarding. Use it only on a trusted
LAN and end the session when finished. Local HTTP protections reduce unauthorized Web
actions but do not encrypt traffic from an observer on that network.

## Deferred encryption decision

Phase 4 does not add a vault or automatically enable platform security features.
ESP-IDF supports NVS encryption, flash encryption, and secure boot, but enabling them
changes provisioning, firmware updates, key ownership, backup, and recovery behavior.
CardMind will evaluate those trade-offs and any supported ship/migration decision in
the Phase 9 physical-access audit. Until that decision is implemented and verified,
this document makes no encrypted-at-rest or physical-tamper-resistance claim.
