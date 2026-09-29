# Changelog

All notable changes to CYBER CLOCK. Both targets (M5StickS3 and M5Cardputer)
share one version number; a release is only tagged when both builds are in it.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
the project uses `MAJOR.MINOR.PATCH` with a strong preference for small patch
steps — visual overhauls and feature work land in the same minor line.

## [1.1.1] — 2026-09-29

Maintenance release: a full code review of both targets (fixes only, no visual
changes), plus the Cardputer's PSRAM configuration corrected.

### Fixed

- **USB power detection.** Power save was gated on `M5.Power.isCharging()`, which
  reads the charger's STAT pin — that goes `false` the instant the battery is
  full, so a desk clock on a cable still dimmed to zero and entered Light Sleep.
  Now gated on a 1 s-cached VBUS voltage reading.
- **Light Sleep button wake.** The `ext0` wake source was armed on GPIO0 (a
  Stick-Bus I²C line) instead of `BtnA`'s GPIO11, so a short tap could not wake
  the clock; the post-wake button sampling window was also too narrow (10 ms).
- **WiFi radio left on after a cancelled/failed setup pass** (StickS3): a scan
  that found nothing, or a cancelled SSID/password prompt, returned without
  `launcherWifiStop()`, keeping the radio powered for the rest of the session.
- **Microphone treated as dead after one hiccup.** A single transient
  `M5.Mic.record()` failure (full buffer slot) permanently disabled the spectrum
  until the next wake; three consecutive failures are now required.
- **Keyboard direction inverted on the Cardputer timezone screen** (`;` scrolled
  down while the WiFi list scrolled up) — both lists now use `;` = up, `.` = down.
- `createSprite()` failure is now a readable `SPRITE FAIL` on screen instead of a
  silent black panel.
- Cardputer: the on-screen keyboard's 10th cell ran 11 px past the right edge.

### Changed

- Power save no longer requires a valid NTP time (a clock that never reached the
  network now still saves power) and the idle timer starts from the end of setup
  on every path.
- Cardputer target: `board_build.psram = disable`, no `-DBOARD_HAS_PSRAM`, no
  `memory_type` override — Cardputer-Adv (ESP32-S3FN8) has no PSRAM, and neither
  build uses it. Flash usage drops ~36 KB.
- Both targets: build includes resolve through `${PROJECT_DIR}` so the dev tree
  and the published tree can build with the same `platformio.ini` line.
- Dead code removed (`RAIN_NAMES`, an unreachable Enter branch, a duplicate IMU
  `begin()`, a self-assigning `getAttitude()` call).

### Notes

- Release images (merged, DIO flash mode) and their SHA-256 are in
  [`dist/`](dist); `tools/make-release.sh` reproduces them.
- Battery meter, rain engine, themes and layout are byte-for-byte the same
  behaviour as 1.1.0 — this release only fixes state handling.

## [1.1.0] — 2026-08-01

First public release. Cardputer reaches feature parity with the StickS3 build.

### Added

- **M5Cardputer target** (`cardputer/`): keyboard input, built-in microphone
  spectrum, IMU shake-to-theme, keyboard-driven WiFi provisioning.
- **Three display modes** on both targets: `FULL` (rain + clock + date +
  battery + spectrum), `TIME` (rain + clock + battery), `SAVER` (rain only).
- **Six rain character sets**: `MATRIX`, `NUMBERS`, `BINARY`, `HEX`, `SYMBOLS`,
  `ALPHA` — each with its own head colour.
- **Timezone picker** (UTC−12 … UTC+12) persisted to NVS; the choice is applied
  when NTP syncs.
- Startup splash with a live digital-rain background, skippable with any input.

### Changed

- Single code review pass over both targets: unified WiFi shutdown path,
  `config.h` centralises every tunable constant, shake detection sensitivity
  corrected, per-boot random seed for the rain (`esp_random()`).
- Cardputer AP gateway address corrected.

### Fixed

- Long-press double-trigger on the StickS3 buttons: one hold could fire both the
  short-press and the long-press action.

### Known limitations

- Cardputer has **no Light Sleep** — its keyboard sits on an I²C expander
  (TCA8418) with no GPIO wake line, so 80 MHz + 1 fps is as deep as it goes.
- Timezones are fixed UTC offsets, without daylight-saving rules.
- Non-ASCII (e.g. Chinese) SSIDs render as boxes: the display font and the
  on-screen keyboard are ASCII-only.

## [1.0.1] — 2026-07-10

### Added

- `[W]` key on the Cardputer connects to WiFi and re-syncs NTP on demand,
  without rebooting; the splash screen documents it.

## [1.0.0] — 2026-07-08

### Added

- Port of the clock onto the Arcadia framework: column-trail digital rain with
  head-bright/tail-dim decay, six rain effects, on-screen keyboard input method.
- M5Cardputer port: physical keyboard controls, microphone spectrum, IMU shake
  detection, keyboard-based WiFi provisioning.
- Startup splash with an animated digital-rain background.

## [0.3.2] — 2026-06-28

### Added

- Timezone picker ported from the stick-clock project.

## [0.3.1] — 2026-06-28

### Fixed

- M5Burner compatibility: the merged image is built with `flash_mode=dio`.

## [0.3.0] — 2026-06-28

### Added

- Microphone spectrum (40 bars, peak flash, VU decay).
- USB charging detection disables every power-save stage.

## [0.2.1] — 2026-06-28

### Fixed

- WiFi SSID garbage (`strnlen` + explicit `std::string` size).
- Hold `KEY1` at boot to force a fresh network configuration.

## [0.2.0] — 2026-06-28

### Added

- WiFi provisioning: scan → network list → on-screen keyboard password entry →
  connect → credentials stored in NVS.

## [0.1.1] — 2026-06-28

### Added

- Six colour themes, seven brightness levels, three-stage power save
  (CPU down-clock, render throttling, Light Sleep with button wake).
- `Night Red` theme for dark rooms.

## [0.1.0] — 2026-06-27

### Added

- First working StickS3 clock: NTP timekeeping, digital rain, two-button UI.
