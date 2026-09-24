# Roadmap

[中文](ROADMAP_CN.md) · **English**

Concrete, claimable work. Each item says what "done" looks like and whether it
needs hardware on the bench. If you want to take one, open an issue naming it —
that avoids two people doing the same thing.

**How to claim:** comment on (or open) an issue titled `roadmap: <item>`, say
which device you have, and link the PR when it is up. Items marked *needs a
device* cannot be merged without somebody flashing it.

---

## 1. Pin the toolchain and record the numbers

**What:** `platformio.ini` currently resolves `espressif32` and `m5stack/M5Unified`
to "whatever is newest". A fresh clone therefore produces an image that differs in
size from the released one (see [BUILDING.md](BUILDING.md) §6).

**Done when:** both `platformio.ini` files pin the platform and library versions,
a clean clone reproduces the released image sizes within a few bytes, and
`BUILDING.md` §6 shows the pinned table.

**Needs a device:** no.

---

## 2. Measure the real current draw

**What:** every power figure in the README/ARCHITECTURE is a datasheet target, not
a measurement. Nobody has put a meter on this board.

**Done when:** measured mA for (a) normal, (b) 80 MHz + 1 fps, (c) Light Sleep with
the backlight off, on StickS3 with a full battery; the README table updated with
the measured values and the instrument named.

**Needs a device:** yes — a StickS3 and a USB power meter or a current probe.

---

## 3. Investigate a Light Sleep path for the Cardputer

**What:** the Cardputer cannot sleep because its keys live behind a TCA8418 I²C
expander with no GPIO wake line. That expander has an **INT** output — if it is
routed to an ESP32 GPIO on the Cardputer, `ext0`/`ext1` wake becomes possible.

**Done when:** either a working sleep path with keyboard wake (with measured
current), or a documented "not possible on this board revision" finding with the
schematic/pin evidence, added to [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

**Needs a device:** yes — a Cardputer, a multimeter and patience with I²C.

---

## 4. Real timezone rules instead of fixed offsets

**What:** the picker stores an integer UTC offset, so a device in a DST region is
an hour off twice a year.

**Done when:** the picker offers a short list of POSIX TZ strings (e.g.
`CET-1CEST,M3.5.0,M10.5.0/3`) and `configTzTime()` is used instead of
`configTime(offset, 0, ...)`. Keep the integer-offset path as a fallback, and keep
the picker usable with two buttons.

**Needs a device:** recommended (the picker is a UI change).

---

## 5. Non-ASCII SSID handling

**What:** a Chinese or emoji SSID renders as `□` in the network list.

**Done when:** the list shows *something* meaningful — even a sanitised
`<non-ascii ssid>` placeholder plus the BSSID, which is enough to pick the right
network — and the limitation note is updated.

**Needs a device:** recommended.

---

## 6. Auto-brightness

**What:** the level is manual and resets to 30 % every boot. The IMU is already
initialised and already knows when the device is picked up.

**Done when:** pickup/lay-down or a manual "night mode" gesture switches between
two configurable levels, with the manual buttons still winning for the rest of the
session. Must not fight the idle state machine.

**Needs a device:** yes.

---

## 7. Weather on the `TIME` screen

**What:** the display has room for one more line, and the device already had a
network connection at boot.

**Done when:** an optional, keyed weather fetch (OpenWeatherMap or similar) can be
enabled in `config.h`; it degrades silently with no network; it does not keep the
radio on after the fetch; and it never blocks the clock. **A clock must tell the
time even when every other feature fails** — that is the acceptance criterion.

**Needs a device:** recommended.

---

## 8. Remove the duplicated `src/` ↔ `cardputer/src/` tree

**What:** the two targets carry two copies of the same program. Every shared
change has to be made twice, and they have already drifted in small ways.

**Done when:** the shared sources live in one place (a `lib/` or `components/`
directory referenced by both `platformio.ini` files) and both targets build
identically to the current output. Do it as a small series of commits, not one
big move.

**Needs a device:** no (build-only verification is acceptable if both targets
produce a working image on a real board before merging — coordinate with the
maintainer).

---

## 9. More photos

**What:** both targets now have a photo in the README (`docs/images/` — Matrix
Green on the StickS3, Ember Orange on the Cardputer, both `FULL` mode, taken
2026-07-08). Two gaps remain: the other four themes have no picture at all, and
the Cardputer shot is a close-up whose keyboard is cropped at the bottom.

**Done when:** either a wider Cardputer shot that shows the whole keyboard, or a
`SAVER` / dark-room shot in `Night Red` — committed under `docs/images/` and
embedded next to the existing two with the theme named in the caption.

**Needs a device:** yes, plus a camera.

---

## Not planned

Being explicit about what this project will not become:

- **A general-purpose app launcher / multi-app platform.** It is a clock.
- **A music player or a Bluetooth speaker.** The microphone is for the spectrum.
- **A cloud-connected device.** After NTP sync the radio goes off on purpose.
- **Always-on debug chrome on the panel.** Diagnostics belong on the serial port.
