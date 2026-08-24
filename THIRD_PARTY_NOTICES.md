# Third-party notices

CardMind is distributed under the MIT License. Its firmware and full-flash
release image also contain the third-party components listed below. The exact
versions, immutable pins, source locations and license-file paths are recorded
in [`third_party/manifest.json`](third_party/manifest.json).

| Component | Pinned version | License |
| --- | --- | --- |
| M5Stack Arduino ESP32 board package | 3.2.1 (Arduino-ESP32 3.2.0) | LGPL-2.1-or-later |
| ESP-IDF runtime libraries | 5.4.1, commit `2f7dcd86…` | Apache-2.0 with component-specific permissive terms |
| M5Cardputer | commit `f1392858…` | MIT; bundled Adafruit TCA8418 code is BSD-3-Clause |
| M5Unified | commit `774d920c…` | MIT |
| M5GFX | commit `93b480bb…` | MIT; the linked eFont glyph data is BSD-3-Clause |
| ArduinoJson | 7.2.1 | MIT |
| libssh2 | 1.11.1 | BSD-3-Clause |
| MicroPython ESP32_GENERIC_S3 image | 1.28.0, build 20260406 | MIT with bundled third-party components |

The corresponding license texts are in [`third_party/licenses`](third_party/licenses).
The MicroPython binary included in a full CardMind image is accepted only when
its SHA-256 digest matches the manifest. The release workflow verifies the same
pins and digest before publishing artifacts.

Every release also provides the exact M5Stack Arduino ESP32 3.2.1 source archive
next to the firmware binaries. Together with the CardMind source archive and
build workflow on the same release page, this lets recipients rebuild CardMind
with a modified LGPL-covered Arduino core.

ESP-IDF and MicroPython aggregate additional upstream components. Their official
license inventories and exact source revisions are linked from the manifest.
CardMind does not modify the downloaded MicroPython application image.
