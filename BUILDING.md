# Building and flashing CYBER CLOCK

[中文](BUILDING_CN.md) · **English**

Everything needed to go from `git clone` to a clock on your desk, plus the traps
we actually hit. If you only want to flash a prebuilt image, the
[README](README.md#quick-start) has the two-command version — you do not need
any of this.

---

## 1. What you need

| | |
|---|---|
| Board | M5Stack **StickS3** or M5Stack **Cardputer** (both ESP32-S3) |
| Cable | USB-C **data** cable — charge-only cables are the single most common "board not detected" cause |
| Host | macOS, Linux or Windows |
| Python | 3.8+ |
| PlatformIO | Core 6.x (CLI or the VS Code extension) |

Install PlatformIO Core if you do not have it:

```bash
python3 -m pip install -U platformio     # or: brew install platformio
pio --version                            # PlatformIO Core, version 6.x
```

Drivers: the StickS3 and Cardputer both use the ESP32-S3's **native USB-Serial/JTAG**
peripheral, so macOS and Linux need no driver at all, and Windows 10/11 usually
picks it up automatically. If Windows shows an unknown device, install the
Espressif USB-JTAG driver — but note that the firmware is built with
`ARDUINO_USB_CDC_ON_BOOT=1`, so the port **disappears and reappears** when the
application starts. That is normal, not a crash.

---

## 2. Get the source

```bash
git clone https://github.com/andjiang0083/cyber-clock.git
cd cyber-clock
```

The repository carries a vendored copy of the GUI framework the firmware needs
(`arcadia/core/`, pulled in as `#include <core/arcadia.h>`). Do not delete that
folder — the build fails immediately with
`fatal error: core/arcadia.h: No such file or directory`.

---

## 3. Build

### StickS3 — from the repository root

```bash
pio run
```

### Cardputer — from its own project directory

```bash
cd cardputer
pio run
```

Two separate PlatformIO projects, two separate `.pio/` build directories. Both
targets compile the same sources in spirit but each has its own copy under its
own `src/` — when you change shared behaviour, change it in **both**
(`src/` and `cardputer/src/`). This is the one place where the code base is
duplicated, and it is called out in [CONTRIBUTING.md](CONTRIBUTING.md).

Artifacts land in `.pio/build/<env>/`:

| File | What it is |
|---|---|
| `firmware.bin` | application image only — for `pio run -t upload` |
| `firmware.factory.bin` | PlatformIO's own merged image (bootloader + partitions + app) |
| `bootloader.bin`, `partitions.bin` | the pieces, if you want to merge by hand |

---

## 4. Flash

```bash
# macOS            Linux              Windows
# /dev/cu.usbmodem*  /dev/ttyACM*     COMx
pio run --target upload --upload-port /dev/cu.usbmodem1101
```

Find the port with `pio device list`. If the upload starts and then stalls at
`Connecting...`, hold the board's reset button while it retries, or unplug and
replug the cable.

Serial log (115200 baud, with the ESP32 exception decoder enabled in
`platformio.ini`):

```bash
pio device monitor
```

> The serial port belongs to the application, not to a debug console: the
> firmware does not print anything periodic, so an empty monitor window is the
> normal case. The clock has no on-screen debug either — deliberately.

---

## 5. Merged image for M5Burner / esptool

M5Burner wants a **single merged image** written from offset `0x0`, and it wants
the bootloader built with **DIO** flash mode (already set in `platformio.ini`:
`board_build.flash_mode = dio`). Either take `firmware.factory.bin` from step 3,
or merge explicitly — `tools/make-release.sh` does exactly this:

```bash
./tools/make-release.sh            # writes both merged images into dist/
```

By hand, for the StickS3:

```bash
ESPTOOL=~/.platformio/packages/tool-esptoolpy/esptool.py
python3 $ESPTOOL --chip esp32s3 merge_bin \
  --output dist/cyber-clock-v1.1.0-merged.bin \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0000 .pio/build/sticks3/bootloader.bin \
  0x8000 .pio/build/sticks3/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/sticks3/firmware.bin
```

and for the Cardputer, from `cardputer/`:

```bash
python3 $ESPTOOL --chip esp32s3 merge_bin \
  --output ../dist/cyber-clock-cardputer-v1.1.0-merged.bin \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0000 .pio/build/cardputer/bootloader.bin \
  0x8000 .pio/build/cardputer/partitions.bin \
  0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/cardputer/firmware.bin
```

### Verify the image before you publish or flash it

```bash
# 1. the flash-mode byte in the bootloader header must be 0x02 (DIO)
python3 -c "d=open('dist/cyber-clock-v1.1.0-merged.bin','rb').read(); print('DIO' if d[2]==0x02 else hex(d[2]))"

# 2. checksum
shasum -a 256 dist/cyber-clock-v1.1.0-merged.bin
```

A merged image built with QIO (`0x00`) flashes fine with esptool but is rejected
or hangs from M5Burner. If the board boots to a blank screen after an M5Burner
flash, this byte is the first thing to check.

---

## 6. Reproducibility

The release images were built from this repository with:

| Component | Version |
|---|---|
| PlatformIO Core | 6.1.19 |
| `platform = espressif32` (resolved) | 7.0.1 |
| Arduino core for ESP32 | 3.3.9 |
| M5Unified | 0.2.23 |
| M5GFX | 0.2.30 |
| esptool | 5.3.0 |

`platformio.ini` does **not** pin library versions (`m5stack/M5Unified` without a
version constraint), so a fresh clone today may pull a newer M5Unified than the
table above and produce an image a few kilobytes larger. That is expected: the
source is identical, the linked library is not. Pinning the platform and the
libraries is an open [roadmap](ROADMAP.md) item — a PR that pins them and
records the resulting sizes is welcome.

Measured on the author's machine, with the PlatformIO package cache already
populated:

| Step | Time |
|---|---|
| StickS3 clean build (`rm -rf .pio && pio run`) | 145 s |
| Cardputer build after that (packages cached) | 22 s |

First-ever build on a clean machine is slower — PlatformIO downloads the
platform, the toolchain and the Arduino framework before it compiles anything.

---

## 7. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| `fatal error: core/arcadia.h: No such file or directory` | `arcadia/` missing or moved | Restore the folder; the include path is `-I${PROJECT_DIR}/arcadia` (StickS3) / `-I${PROJECT_DIR}/../arcadia` (Cardputer) |
| Cardputer build picks up StickS3 headers | you built the wrong directory | The Cardputer project must be built from `cardputer/`, not from the root |
| `pio device list` shows nothing | charge-only cable, or the app already took the port | Swap the cable; the port disappears after boot because of `USB_CDC_ON_BOOT` |
| Upload hangs at `Connecting...` | the board is busy in Light Sleep / the app | Hold the reset button while the upload starts |
| Blank screen after flashing a merged image | QIO bootloader, or a truncated download | Check byte 2 == `0x02`, re-download and verify `sha256` |
| Blank screen after a normal upload, but the splash appears on reset | sprite allocation failed | The firmware falls back to direct rendering; if it persists, open an issue with the build target and the exact steps |
| M5Burner refuses the file | not a merged image, or not from `0x0` | Use the merged image from step 5 |
| Clock shows a boot-uptime counter | NTP never succeeded | Check the 2.4 GHz network; re-run provisioning (`BtnA` held during splash / `W` on the Cardputer) |
| Chinese SSID shows as `□` | ASCII-only font and keyboard | Known limitation, see the [README](README.md#known-limits) |
| Clock freezes and needs a power cycle | suspected hardware-level hang, not a panic | Report it with the build target — this is the class of bug the issue form asks about |

---

## 8. Changing things

| What | Where |
|---|---|
| Version string shown on the splash | `src/config.h` and `cardputer/src/config.h` (`VERSION`) |
| Idle thresholds (60 s / 180 s) | `IDLE_POWERSAVE_MS`, `IDLE_SLEEP_MS` in both `config.h` |
| Brightness steps | `BRIGHTNESS_LEVELS` / `BRIGHTNESS_PCT` in both `config.h` |
| Rain density, character pool, trail length | `MATRIX_DROPS`, `MATRIX_CHARS`, `TRAIL_LEN` in `main.cpp` / `config.h` |
| Palette | `initThemes()` in both `main.cpp` |
| Frame-rate cap | `delay(30)` at the end of `loop()` |

After a change, rebuild **both** targets before you call it done.
