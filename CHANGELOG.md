# Changelog

A high-level history of OpenCatFlap, grouped by periods of activity.

## Autumn 2026 (Oct – Nov): Tests, API and antenna tuning
- Native unit test suite with mocks for config, helper, lock, MQTT, RFID, state machine, webserver and wifi, plus embedded hardware tests and a GitHub Actions workflow.
- Webserver reworked into a JSON API (`/api`, `/api/cats`, `/api/certs`) with over-the-air firmware updates (`/api/update`); partition table adjusted.
- Firmware reports the EM4095 antenna's resonant frequency for tuning; tuning notes in `EM4095_TUNING.md`.
- PCB revision v2 (NP0 capacitors, switchable tuning bank) finalised and ordered; v3 being drafted.

## Summer 2026 (Jul): Code review and hardening
- Fixed the servo channel attach, an MQTT memory leak, unsafe string copies, certificate memory corruption, and mutex use inside interrupts.
- RFID clock now counted by the PCNT hardware peripheral instead of software interrupts.
- CRC check on received RFID data.
- KiCad project updated.

## Winter 2025/26 (Dec – Jan): New RFID implementation
- RFID driver rewritten from scratch (`OCFRFID`); it reads tags reliably. Notes in `RFID.md`.
- First access-control logic connecting RFID reads to the state machine and flap control.

## Spring 2025 (Apr): Mechanical design refresh
- New Fusion 360 model of the enclosure and a larger KiCad schematic and PCB update.

## Autumn 2024 (Aug – Dec): State machine rewrite
- Firmware rebuilt around a state machine (`OCFStateMachine`), with locks (`OCFLock`) and helpers split out.
- Persistent configuration (`OCFConfig`) and an HTML configuration page.
- MQTT messages for RFID events and lock state.
- First working end-to-end cat control.

## Spring/Summer 2024 (Jan – Jul): Mainboard v2
- Wiring fixes, then a second board revision (schematics and PCB).
- Firmware ported to the new mainboard; first RFID tests on it.
- Custom partition table; README with parts list expanded.

## Autumn 2023 (Sep – Oct): MQTT, CAD and cleanup
- MQTT integration became functional.
- CAD files and images for the enclosure added.
- Code restructured into classes; repository cleaned up.
- Initial RFID setup.

## Summer 2023 (Jun – Jul): Initial prototype
- ESP32 firmware for servo-driven locks, controlled through a debug web endpoint, locking again automatically.
- Motion sensors with direction detection.
- Persistent state, NTP time and WiFi.
- First MQTT client and draft schematics.
