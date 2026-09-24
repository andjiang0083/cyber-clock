# CYBER CLOCK — cyberpunk desk clock for M5StickS3 & M5Cardputer

[中文](README_CN.md) · **English**

**One 240×135 landscape clock, two M5 handhelds: NTP timekeeping, a column-trail digital-rain engine, six neon palettes, a live microphone spectrum — and a power save that drops the StickS3 into Light Sleep with the backlight off.**

> *On the desk it is a HUD. In the hand it is an instrument.*

![CYBER CLOCK on an M5StickS3 — Matrix Green theme in FULL mode: the time 10:43, the date 2026-07-08 WED, a full battery indicator and the digital rain behind it](docs/images/cyber-clock-sticks3-matrix-green.jpg)

*M5StickS3 · Matrix Green theme · `FULL` mode (v1.0.1 build; the UI is unchanged in v1.1.0).*

Both firmware builds live in this repository: `src/` is the **M5StickS3** build (two buttons), `cardputer/` is the **M5Cardputer** build (physical keyboard, same UI). They share one code base — the graphics, rain engine, themes, WiFi provisioning and timezone picker are the same code; only the input layer and the sleep depth differ.

The M5Stack Tab5 port of the same rain engine lives in its own project: [matrix-rain-tab5](https://github.com/andjiang0083/matrix-rain-tab5) (ESP32-P4, 1280×720 MIPI-DSI).

---

## The two builds

| | **M5StickS3** (`src/`) | **M5Cardputer** (`cardputer/`) |
|---|---|---|
| Board | M5Stack StickS3 — ESP32-S3, 8 MB flash, 8 MB octal PSRAM | M5Stack Cardputer — ESP32-S3, 8 MB flash, 8 MB octal PSRAM |
| Display | 1.14″ 240×135 ST7789, SPI, landscape | 1.14″ 240×135 ST7789, SPI, landscape |
| Input | `BtnA` (left) / `BtnB` (right), short + long press | physical QWERTY keyboard |
| Microphone | ES8311 | built-in mic |
| IMU | BMI270 — shake to switch theme | BMI270 — shake to switch theme |
| Brightness | 7 levels (1 % → 100 %) | 7 levels (`+` / `-`) |
| Deepest power save | **Light Sleep, backlight off, `BtnA` hardware wake** | 80 MHz + 1 fps (no Light Sleep — see [known limits](#known-limits)) |
| Build | `pio run` in the repository root | `cd cardputer && pio run` |
| Firmware | `cyber-clock-vX.Y.Z-merged.bin` | `cyber-clock-cardputer-vX.Y.Z-merged.bin` |

---

## Status

Everything below is the state of the **v1.1.0** builds. Nothing here is a roadmap item.

| Module | State | Notes |
|---|---|---|
| NTP timekeeping | ✅ works | `ntp.aliyun.com`, `ntp.tencent.com`, `pool.ntp.org`; 20 retries × 250 ms; if sync fails the clock falls back to counting uptime |
| Timezone picker | ⚠️ fixed offsets | UTC−12 … UTC+12, stored in NVS, applied at NTP sync. **No daylight-saving rules** |
| Digital rain | ✅ works | 55 drops, 6-character trail with head-bright/tail-dim decay, 33 fps ceiling |
| Rain character sets | ✅ works | `MATRIX` `NUMBERS` `BINARY` `HEX` `SYMBOLS` `ALPHA`, each with its own head colour |
| Six themes | ✅ works | Neon Cyan, Matrix Green, Aurora Purple, Ember Orange, Blood Red, Night Red |
| Three display modes | ✅ works | `FULL` / `TIME` / `SAVER` (rain only) |
| Brightness | ✅ works | 7 levels; hold `BtnB` on the StickS3, `+` / `-` on the Cardputer |
| Microphone spectrum | ✅ works | 40 bars, per-bar hue, white peak flash, VU decay; silently disabled if the mic fails to start |
| IMU shake → theme | ✅ works | complementary-filter attitude rate, 3 consecutive detections |
| Battery indicator | ✅ works | 4 bars, top-right, I²C read cached for 1 s |
| WiFi provisioning | ✅ works | scan → list → password → connect → NVS; the StickS3 uses an on-screen keyboard driven by two buttons |
| Idle power save | ✅ works | 60 s → 80 MHz + 1 fps; StickS3 only: 180 s → Light Sleep |
| USB charging override | ✅ works | plugged in ⇒ every power-save stage is disabled and the clock runs at full speed |
| Non-ASCII SSIDs | ⚠️ known | Chinese/emoji SSIDs display as boxes — the font and the on-screen keyboard are ASCII-only |
| Cardputer Light Sleep | ❌ not possible | the keyboard is on a TCA8418 I²C expander with no wake line |
| OTA update | ❌ not implemented | flashing is over USB-C |
| Weather, auto-brightness, alarms | ❌ not implemented | see [ROADMAP.md](ROADMAP.md) |

### Known limits

1. **The Cardputer cannot Light Sleep.** Its keys are scanned over I²C, so there is no GPIO to wake the CPU from. 80 MHz with a 1 fps refresh is the floor. The StickS3 reaches ~0.3 mA because `BtnA` sits on GPIO0 and can be armed as an `ext0` wake source.
2. **Timezones are offsets, not rules.** Selecting `UTC+8` means UTC+8 forever — no DST transition.
3. **ASCII only.** Both the GLCD font and the on-screen keyboard stop at ASCII, so a Chinese SSID shows as `□`.

---

## Quick start

### Option 1 — flash a release image (no toolchain needed)

1. Download `cyber-clock-v1.1.0-merged.bin` (StickS3) or `cyber-clock-cardputer-v1.1.0-merged.bin` (Cardputer) from [Releases](../../releases).
2. **M5Burner**: *Custom* → *Import Custom FW* → pick the `.bin` (or the `m5burner/` bundle) → select **Cyber Clock** → *Burn*.
3. **esptool**, if you prefer the command line:

   ```bash
   esptool.py --chip esp32s3 --baud 921600 write_flash 0x0 cyber-clock-v1.1.0-merged.bin
   ```

   The merged image contains the bootloader, partition table and application, and is written from offset `0x0`. Verify the download against the `sha256` in the release notes.

### Option 2 — build from source

```bash
git clone https://github.com/andjiang0083/cyber-clock.git
cd cyber-clock
pio run                              # StickS3
pio run --target upload --upload-port /dev/cu.usbmodemXXXX

cd cardputer
pio run                              # Cardputer
pio run --target upload --upload-port /dev/cu.usbmodemXXXX
```

Full walkthrough, toolchain versions and every trap we hit: [BUILDING.md](BUILDING.md) ([中文](BUILDING_CN.md)).

### First boot

1. The splash runs for 3 s over a live rain background (any input skips it).
2. **No saved WiFi** → the provisioning wizard starts: scan, pick a network, type the password, then pick a timezone. Credentials and offset go to NVS.
3. **Saved WiFi** → it connects, syncs NTP, then **turns the radio off** — the clock no longer needs the network.
4. The timezone picker only appears when no offset has been stored yet. To re-run provisioning later: hold `BtnA` while the splash is on screen (StickS3), or press `W` at any time (Cardputer).

> **No network, no clock.** The first boot needs a 2.4 GHz access point. After that the ESP32's RTC keeps time; if NTP never succeeded, the display counts up from boot instead.

---

## Controls

### M5StickS3

| Action | `BtnA` (left) | `BtnB` (right) |
|---|---|---|
| **Tap** | cycle display mode: `FULL → TIME → SAVER` | in `FULL`/`TIME`: next theme · in `SAVER`: next rain character set |
| **Hold** (≈ 0.5 s) | next rain character set | cycle brightness (7 levels) |
| **Shake** | — | random theme |

The two-button mapping is deliberately closed: tap for the everyday things, hold for the settings.

### M5Cardputer

| Key | Action |
|---|---|
| `S` | cycle display mode: `FULL → TIME → SAVER` |
| `R` | next rain character set |
| `C` | in `FULL`/`TIME`: next theme · in `SAVER`: next rain character set |
| `W` | connect to WiFi and re-sync NTP (scan → pick → password → sync) |
| `+` / `-` | brightness up / down (7 levels) |
| shake | random theme |

Keyboard provisioning: `;` / `.` move through the network or timezone list, `Enter` confirms, `Del` deletes a character, `Esc` cancels. Letter keys are edge-detected, so holding a key does not repeat.

---

## Display modes, rain and themes

| Mode | Rain | Clock | Date | Battery | Spectrum |
|---|---|---|---|---|---|
| **FULL** | ✅ | ✅ | ✅ | ✅ | ✅ |
| **TIME** | ✅ | ✅ | — | ✅ | — |
| **SAVER** | ✅ | — | — | — | — |

| Rain set | Character pool | Head colour |
|---|---|---|
| `MATRIX` | `01#*+:.A-Za-z<>/\|~@%&$` | blue-green |
| `NUMBERS` | `0-9` | pure green |
| `BINARY` | `01` | cyan |
| `HEX` | `0-9A-F` | blue-violet |
| `SYMBOLS` | `#*+:.<>/\|~@%&$` | warm red |
| `ALPHA` | `A-Z` | pale blue |

| # | Theme | Main | Accent | Mood |
|---|---|---|---|---|
| 1 | **Neon Cyan** | `#00FFFF` | `#FF00FF` | classic cyberpunk |
| 2 | **Matrix Green** | `#00FF41` | `#004400` | the Matrix |
| 3 | **Aurora Purple** | `#8844FF` | `#00DDFF` | synthwave |
| 4 | **Ember Orange** | `#FF6600` | `#FF2200` | burning street |
| 5 | **Blood Red** | `#FF0044` | `#FFAA00` | Blade Runner |
| 6 | **Night Red** | `#340000` | `#4C0000` | dark-room comfort |

Brightness levels: 1 % · 5 % · 10 % · 30 % · 50 % · 80 % · 100 % (default 30 %). The level is **not** persisted — a reboot returns to 30 %, deliberately: nobody wants to hunt for a 1 % screen in daylight.

---

## Power save

Three stages, all disabled while USB power is connected (charging ⇒ full speed, and the green LED comes back on):

| Stage | Trigger | CPU | Refresh | Microphone | Backlight |
|---|---|---|---|---|---|
| Normal | — | 240 MHz | ~33 fps | on | on |
| Idle | 60 s without input | 80 MHz | 1 fps (renders when the second changes) | off | on |
| **Light Sleep** (StickS3 only) | 180 s without input | paused | one frame per second, after a 200 ms poll wake | off | **off** |

Any input restores full speed in ≤ 200 ms. In Light Sleep an RTC timer wakes the CPU every 200 ms to poll the buttons, and `BtnA` (GPIO0) is additionally armed as an `ext0` wake source so it responds instantly. On the Cardputer the deepest stage does not exist, for the hardware reason above.

The current figures in [ARCHITECTURE.md](docs/ARCHITECTURE.md) are design targets taken from the ESP32-S3 datasheet, not ammeter readings — see [ROADMAP.md](ROADMAP.md) if you own a power meter and want to close that gap.

---

## How it works

```
setup()
 ├─ M5.begin() + ArcDisplay::begin(rotation 1)   → 240×135 landscape, sprite double buffer
 ├─ initThemes() / initMatrixRain()              → 6 palettes, 55 drops
 ├─ 3 s splash over a live rain background       → skippable by any input
 ├─ tzLoad() from NVS                            → applied when NTP syncs
 ├─ wifiSetupBegin()                             → only if there are no usable credentials
 │   └─ pickTimezone()                           → buttons (StickS3) or `;` `.` `N` (Cardputer)
 └─ initAudio()                                  → silently disabled if the mic refuses

loop()   [~33 fps; 1 fps when idle]
 ├─ charging?          → cancel every power-save stage, restore full speed
 ├─ idle state machine → 60 s ⇒ 80 MHz, 180 s ⇒ Light Sleep (StickS3)
 ├─ input              → arcGetKey() (buttons) / getKey() (keyboard, edge-detected)
 ├─ updateTime()       → epoch + elapsed millis, no per-second NTP call
 ├─ updateIMU() / checkShake()
 ├─ updateAudio()      → 40-band spectrum from 160-sample blocks
 ├─ render(mode)       → clear → rain → clock → date → battery → spectrum
 ├─ ArcDisplay::push() → atomic sprite flip
 └─ [Light Sleep] esp_light_sleep_start() with timer + ext0 wake
```

The interesting parts in detail — the column-trail decay maths, the sprite-allocation fallback that killed the black screen, why the Cardputer cannot sleep, the WiFi layer ported from Launcher — are written up in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) ([中文](docs/ARCHITECTURE_CN.md)).

---

## Performance

Measured on a clean build (`pio run` in a fresh clone; PlatformIO Core 6.1.19, espressif32 platform 7.0.1, Arduino core 3.3.9, M5Unified 0.2.23 / M5GFX 0.2.30):

| Target | RAM | Flash | Frame rate |
|---|---|---|---|
| M5StickS3 | 52,872 B / 327,680 B (16.1 %) | 1,176,559 B / 3,342,336 B (35.2 %) | ~33 fps normal, 1 fps idle |
| M5Cardputer | 53,480 B / 327,680 B (16.3 %) | 1,176,451 B / 3,342,336 B (35.2 %) | ~33 fps normal, 1 fps idle |

The loop is `delay(30)`, which is where the 33 fps ceiling comes from — the rain never needs more, and the leftover CPU headroom is what makes the Light Sleep stage viable. There is deliberately **no on-screen FPS counter**: this project's rule is that the display only carries information that belongs on a clock.

---

## Repository layout

```
cyber-clock/
├── platformio.ini              StickS3 build config
├── src/                        StickS3 application
│   ├── main.cpp                rain engine, clock, UI, power-save state machine
│   ├── config.h                every tunable constant (version, timings, brightness, rain)
│   ├── wifisetup.cpp/.h        scan → pick → password → connect → NVS
│   ├── textinput.cpp/.h        on-screen keyboard for the two-button StickS3
│   ├── timesync.h              shared NTP declaration
│   └── idf/idf_wifi.cpp/.h     ESP-IDF WiFi (scan / AP / connect / wrong-password)
├── cardputer/                  Cardputer application (same UI, keyboard input)
│   ├── platformio.ini
│   └── src/
├── arcadia/core/               vendored snapshot of the author's GUI framework
├── docs/ARCHITECTURE.md        design notes and per-module deep dives
├── m5burner/                   M5Burner catalogue entry
├── tools/make-release.sh       merged-image builder
└── .github/                    issue forms, PR template
```

### About `arcadia/`

`arcadia/core/` is a **vendored snapshot** of *Arcadia*, the small header-only GUI / input / IMU / power-save framework the author uses across his ESP32 projects. The firmware pulls it in with `#include <core/arcadia.h>`, and it is committed here so that `git clone && pio run` works with no extra steps (the include path is set in `platformio.ini` via `-I${PROJECT_DIR}/arcadia`). Treat it as a copy, not as the upstream source.

---

## Contributing

Issues and pull requests are welcome. The project-specific conventions (they come from real breakage, not taste) are in [CONTRIBUTING.md](CONTRIBUTING.md) ([中文](CONTRIBUTING_CN.md)); concrete, claimable work is in [ROADMAP.md](ROADMAP.md) ([中文](ROADMAP_CN.md)).

When reporting a bug, the two details that save the most time are **which build (StickS3 or Cardputer)** and **whether the device recovers by itself or needs a power cycle** — those separate a software panic from a hardware-level hang, and the two have completely different root causes.

## Credits and licence

Built on M5Stack's M5Unified/M5GFX, Espressif's Arduino core and ESP-IDF, and a WiFi provisioning layer that started life in [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher) (MIT). Full attribution — including the files that came from Launcher — is in [CREDITS.md](CREDITS.md).

**MIT** licence, see [LICENSE](LICENSE). M5Stack, StickS3, Cardputer and ESP32 are trademarks of their respective owners; this is an unaffiliated community project by someone who owns the hardware. The cyberpunk themes are original palettes — no film assets, no third-party artwork, no proprietary fonts.
