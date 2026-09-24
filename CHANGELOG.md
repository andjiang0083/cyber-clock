# Changelog

All notable changes to CYBER CLOCK. Both targets (M5StickS3 and M5Cardputer)
share one version number; a release is only tagged when both builds are in it.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
the project uses `MAJOR.MINOR.PATCH` with a strong preference for small patch
steps — visual overhauls and feature work land in the same minor line.

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
