# CYBER CLOCK — architecture notes

[中文](ARCHITECTURE_CN.md) · **English**

How the firmware is put together, why it is put together that way, and the
specific bugs that shaped the design. If you are about to change something, read
the section that covers it first — several of these decisions are counter-intuitive
until you know what broke.

---

## 1. Two targets, one program

```
cyber-clock/
├── src/                StickS3 build        ─┐ same program,
├── cardputer/src/      Cardputer build      ─┘ two copies
└── arcadia/core/       vendored GUI/input/IMU/power-save framework
```

Both applications are single-file programs (`main.cpp`, ~800 and ~740 lines) that
pull in a small vendored framework (`arcadia/core/`) and a handful of local
modules. The duplication between `src/` and `cardputer/src/` is a known cost — it
is item 8 on the [roadmap](ROADMAP.md) — but it keeps each target buildable in
isolation with no shared-component plumbing.

The framework supplies the things both targets need identically:

| Header | What it provides |
|---|---|
| `arcadia.h` | one-stop include |
| `display.h` | `canvas` sprite, rotation, atomic `push()` |
| `input.h` | `arcGetKey()` — unified tap/hold event for the StickS3 buttons |
| `color.h` | `c565()`, blending, palettes |
| `imu.h` | `ArcIMU` complementary filter |
| `powersave.h` | Light Sleep helpers |
| `screen.h`, `hud.h`, `audio.h`, `math3d.h`, `stars.h`, `app.h`, `autodemo.h` | used by other projects; harmless here |

---

## 2. Rendering: sprite double buffer, and the fallback that fixed the black screen

Everything is drawn into an off-screen sprite and flipped in one operation:

```cpp
ArcDisplay::begin(SCREEN_ROTATION, BRIGHTNESS_LEVELS[g_brightnessIdx]);  // 240×135 landscape
...
clear(TFT_BLACK);
drawMatrixRain();
drawTime();
drawDate();
drawBattery();
drawAudioBars();
push();                     // single atomic sprite → panel copy
```

Without the sprite, per-primitive drawing produces visible tearing on the rain.
The sprite is 240×135×2 bytes = 64,800 bytes, allocated from PSRAM.

**The trap:** `canvas.createSprite(240, 135)` can return `false` (PSRAM
fragmentation, an early `M5.begin()` ordering issue, a wrong rotation setup). When
it fails, *every* draw call on the canvas silently becomes a no-op, and the screen
sits on whatever the last `fillScreen` left — a black panel with no crash, no
panic and no serial output. That is exactly the v0.1 black-screen bug.

**The fix** is a validity flag rather than a retry loop: if the sprite could not be
created, the render path draws straight to `M5.Display` instead. Slower, some
tearing in the rain, but the clock is still readable — and a clock that shows the
wrong picture is better than a clock that shows nothing.

```cpp
if (g_spriteOk) { ...draw to canvas...; push(); }
else            { ...draw to M5.Display directly...; }
```

Two related details:

- **`drawBattery()` caches the I²C read for 1 s.** `M5.Power.getBatteryLevel()`
  is an I²C transaction; at 33 fps that is 33 transactions per second for a value
  that moves once an hour.
- **The splash screen draws directly** (`canvas.startWrite()` / `endWrite()` in
  the rain loop) so that it still works before the first full render.

---

## 3. Time

The clock never calls `getLocalTime()` in the loop. Instead:

```cpp
syncNTP();                        // sets g_epoch (time_t) and g_epochBase = millis()
// every frame:
epoch = g_epoch + (millis() - g_epochBase) / 1000;
localtime_r(&epoch, &tm);         // hour / minute / second / date / weekday
```

That is one integer add and one `localtime` per frame, with no SNTP polling and no
drift accumulation beyond the ESP32's own RTC.

- NTP servers: `ntp.aliyun.com`, `ntp.tencent.com`, `pool.ntp.org`; 20 retries
  × 250 ms.
- The timezone offset lives in NVS (`cyberclock` namespace, key `tz`) and is
  passed to `configTime(offset * 3600, 0, ...)`. It is a **fixed offset** — see
  [roadmap item 4](ROADMAP.md) for real DST rules.
- **If NTP never succeeds, `g_timeValid` stays false and the display counts up
  from boot.** Note the consequence for power save: the idle state machine is
  gated on `g_timeValid`, so a clock that never got the time also never sleeps —
  deliberately, because a screen showing the wrong time is worse than a screen
  burning 30 mA.

---

## 4. The digital rain

55 drops, each with an independent `x`, `y`, speed, head character, head
brightness and a 5-character trail (6 glyphs including the head).

```
        head   ← brightness 80…199, +60 with 10 % probability, clamped to 255
      trail[0] ← headBri / 5
      trail[1] ← headBri / 4
      trail[2] ← headBri / 3
      trail[3] ← headBri / 2
      trail[4] ← 5   (nearly invisible)
```

Each glyph is 6×8 px, the trail is drawn at `8 px` vertical pitch, and the tail
brightness is floored at 5 so the trail never becomes a hard edge. Trail colour is
computed as `c565(bri/8, bri/2, bri/4)` — a blue-green ramp.

The **head** colour depends on the active character set, which is what makes
`NUMBERS`, `BINARY`, `HEX`, `SYMBOLS` and `ALPHA` look like different effects
rather than different strings:

| Set | Head colour expression |
|---|---|
| `MATRIX` | `c565(bri/6, min(bri,120), bri/3)` — blue-green |
| `NUMBERS` | `c565(bri/10, min(bri,140), bri/10)` — pure green |
| `BINARY` | `c565(0, min(bri,200), bri/8)` — cyan |
| `HEX` | `c565(bri/8, bri/2, min(bri,100))` — blue-violet |
| `SYMBOLS` | `c565(min(bri,100), bri/2, bri/3)` — warm red |
| `ALPHA` | `c565(bri/10, bri/3, min(bri,140))` — pale blue |

When a drop leaves the bottom of the screen it respawns with a fresh random `x`
and head brightness, so the density stays constant and the pattern never repeats
within a session. The seed is `srand(esp_random())` at boot, so it differs every
power-up.

**Gotcha, and it is a nasty one:** the enum member cannot be called `HEX`:

```cpp
// Arduino's Print.h contains:  #define HEX 16
enum class RainMode : uint8_t { MATRIX, NUMBER, BINARY, HEXADECIMAL, SYMBOL, ALPHA, MODES_N };
```

Naming it `HEX` makes the preprocessor rewrite the enumerator to `16` and the
build fails with a confusing error somewhere else entirely. `HEXADECIMAL` is not
a style choice, it is a workaround, and the comment in the source says so.

---

## 5. Input

### StickS3 — `arcGetKey()`

The framework's input layer turns the two physical buttons into four events:
`'A'` / `'a'` (tap / hold) and `'B'` / `'b'`. The hold threshold is M5Unified's
default 500 ms (`Button_Class::_msecHold`), and it is not overridden here. The
mapping in `loop()` is then a plain switch:

| Event | Action |
|---|---|
| `'A'` | cycle display mode |
| `'a'` | next rain character set |
| `'B'` | next theme (`FULL`/`TIME`) or next rain set (`SAVER`) |
| `'b'` | next brightness level |

**v1.1.0 fixed a double-trigger.** The mechanism is worth knowing: in M5Unified,
`wasHold()` fires on the frame the hold threshold is crossed, while `wasClicked()`
fires on the **release** frame — so one long press produces two events, and the
device did both the hold action and the tap action. The fix lives in the
framework's input layer: after a hold event, `wasClicked()` is swallowed for a
300 ms window, so one physical gesture yields exactly one event. That is why the
checklist in [CONTRIBUTING.md](CONTRIBUTING.md) calls it out.

### Cardputer — `getKey()`

The keyboard is scanned by `M5Cardputer.Keyboard` and translated to the same
conceptual events:

- **Symbol keys** (`+`, `-`, `;`, `.`) are returned immediately — they are
  stateless and repeat-free by nature.
- **Letter keys** are edge-detected with a 26-bit mask:

  ```cpp
  mask |= 1UL << (k - 'a');
  edge  = mask & ~g_prevHeldMask;   // only on the frame the key goes down
  g_prevHeldMask = mask;
  ```

  Without this, holding `S` would cycle display modes 33 times a second.

Both targets share the same *semantics* (`S`↔`A`, `C`↔`B`, `+`/`-`↔brightness hold),
which is why the two `main.cpp` files still read like the same program.

---

## 6. Microphone spectrum

40 bars across the bottom, sampled in 160-sample blocks every 50 ms:

- bar `i` occupies 6 px (`x = i * 6`), 4 px of bar plus a 2 px gap;
- hue is spread across the full wheel: `hue = i * 240 / 40`;
- bar brightness rises with amplitude (`80 + h * 25`), giving a dim base and a
  bright tip;
- the top pixel flashes white when the bar is tall (≥ 7);
- a 2 px baseline is drawn at y=130/131 even when the mic is off, so the layout
  does not jump.

`initAudio()` retries on wake from power save (`g_micFailed` is cleared) because
the ES8311 does not always come back cleanly after Light Sleep. If the mic never
starts, the spectrum is simply absent — the clock does not care.

---

## 7. IMU shake detection

`ArcIMU` maintains a complementary-filter attitude estimate (τ = 1.0 s). Shake
detection does not look at raw acceleration; it looks at the **rate of change of
attitude**:

```cpp
if (fabs(p - g_prevP) > SHAKE_THRESHOLD && fabs(r - g_prevR) > SHAKE_THRESHOLD) g_shakeCnt++;
else g_shakeCnt decays;
if (g_shakeCnt >= 3) { g_theme = random; g_shakeCnt = 0; }
```

Threshold 0.15 rad per frame, 3 consecutive frames. v1.1.0 corrected the
sensitivity — the earlier version fired while the device was merely being picked
up, which is a theme change nobody asked for.

---

## 8. Power save

A three-stage state machine evaluated once per loop:

```
                       any input / USB plugged in
        ┌──────────────────────────────────────────────┐
        │                                              ▼
   ┌─────────┐  60 s idle   ┌──────────┐  180 s idle   ┌──────────────┐
   │ NORMAL  │─────────────▶│ IDLE     │──────────────▶│ LIGHT SLEEP  │
   │ 240 MHz │              │ 80 MHz   │               │ backlight off│
   │ ~33 fps │◀─────────────│ 1 fps    │◀──────────────│ 0.3 mA class │
   └─────────┘   any input  └──────────┘   any input   └──────────────┘
                                                        (StickS3 only)
```

Details that matter:

- **The whole machine is gated on `g_timeValid` and on `!isCharging()`.** Plugging
  in USB cancels every stage, restores 240 MHz, restores brightness, turns the
  green LED back on and re-initialises the microphone.
- **Light Sleep** uses a 200 ms RTC timer plus `ext0` on `GPIO_NUM_0` (`BtnA`):

  ```cpp
  esp_sleep_enable_timer_wakeup(SLEEP_POLL_US);          // 200 ms
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, LOW);         // BtnA
  esp_light_sleep_start();
  ```

  On wake, `esp_sleep_get_wakeup_cause()` is checked first: an `EXT0` cause is
  trusted immediately (that is a real button press). Otherwise the firmware does
  `delay(5)` and up to **five** `M5.update()` reads before deciding nobody
  pressed anything — `BtnB` is behind the I²C power/IO expander and is not
  reliably readable in the first milliseconds after a wake.
- **The green LED (GPIO24, active low)** is driven HIGH during Light Sleep and LOW
  on restore, so a sleeping clock does not leave a bright LED glowing on a
  bedside table.
- **Idle rendering is not "draw every frame and throw it away":** in the idle and
  Light Sleep stages the firmware renders only when `g_s` (seconds) changes, i.e.
  1 fps. This is where most of the idle saving actually comes from.
- **Cardputer has no third stage.** Its keys are scanned over I²C through a
  TCA8418 expander with no GPIO wake line, so 80 MHz + 1 fps is the floor. See
  [roadmap item 3](ROADMAP.md) — the expander does have an INT output, so this may
  be solvable if that line is routed on the board.

### Tried and reverted: IMU motion wake

v0.1.2 replaced Light Sleep with "wake on IMU motion" so the clock would light up
when picked up. It was reverted in the very next commit: the IMU keeps drawing
current while polling, the wake was unreliable, and the resulting power draw was
worse than the problem it solved. **Button wake is the shipped behaviour** —
recorded here so nobody re-invents it.

---

## 9. WiFi provisioning

```
first boot / [W] / BtnA held during splash
        │
        ▼
  credentials in NVS? ──yes──▶ connect (timeout) ──ok──▶ NTP sync ──▶ radio off
        │no                            │fail
        ▼                              ▼
   scan networks ──▶ pick SSID ──▶ type password ──▶ connect
        │                                              │wrong password
        ▼                                              ▼
   (list UI)                                     "Wrong password" → retry
        │
        └──▶ first time only: pick timezone ──▶ NTP sync ──▶ radio off
```

| Piece | File | Notes |
|---|---|---|
| WiFi driver layer | `src/idf/idf_wifi.cpp/.h` | ESP-IDF `esp_wifi_*` directly (no Arduino `WiFi.h`), with event-group connect/disconnect tracking and disconnect-reason inspection for the wrong-password case. **Derived from [bmorcelli/Launcher](https://github.com/bmorcelli/Launcher)** — see [CREDITS.md](../CREDITS.md) |
| Provisioning flow | `src/wifisetup.cpp/.h` | scan → list → password → connect → NTP → store |
| On-screen keyboard (StickS3) | `src/textinput.cpp/.h` | a character ring: `BtnA` scrolls, `BtnB` selects, hold `BtnB` = OK. Control items `DEL` / `SPC` / `OK` / `EXIT` sit at the head of the ring |
| Storage | `Preferences` namespace `cyberclock` | `wifi_ssid`, `wifi_pass`, `tz` |

Notes:

- **SSID list corruption:** building the SSID `std::string` from the raw `uint8_t`
  SSID field produced garbage, because the field is not NUL-terminated. Fixed with
  `strnlen` plus an explicit size — a good example of why the SSID field is copied
  rather than printed.
- **Non-ASCII SSIDs** render as `□`: the GLCD font has no glyphs for them.
- **The radio is switched off after NTP** (`wifiStopAll()`), and the clock runs
  from the RTC afterwards. This is a design rule, not an optimisation detail.
- The SSID-list UI in `wifisetup.cpp` and the timezone picker in `main.cpp`
  deliberately duplicate each other. The project rule is that shared abstractions
  must be free of M5GFX dependencies before they get extracted — see the comment
  in `wifisetup.cpp`.

---

## 10. Budgets

Measured on a clean build (PlatformIO Core 6.1.19, espressif32 7.0.1, Arduino
core 3.3.9, M5Unified 0.2.23, M5GFX 0.2.30):

| Target | RAM | Flash |
|---|---|---|
| M5StickS3 | 52,872 B / 327,680 B (16.1 %) | 1,176,559 B / 3,342,336 B (35.2 %) |
| M5Cardputer | 53,480 B / 327,680 B (16.3 %) | 1,176,451 B / 3,342,336 B (35.2 %) |

The sprite (64,800 B) dominates RAM; the rest is the M5GFX/M5Unified stack and the
WiFi driver. Roughly 2 MB of the app partition stays free, which is why OTA would
be feasible if someone wanted it.

---

## 11. Gotcha index

| Symptom / task | Cause | Where it is handled |
|---|---|---|
| Black screen, no panic, no serial output | sprite allocation failed ⇒ all canvas draws are no-ops | `g_spriteOk` fallback, §2 |
| `enum class` member `HEX` breaks the build | Arduino `Print.h` `#define HEX 16` | renamed `HEXADECIMAL`, §4 |
| Garbage SSID text | SSID field is not NUL-terminated | `strnlen` + explicit size, §9 |
| One long press fires two actions | tap and hold paths not mutually exclusive | fixed in v1.1.0, §5 |
| Holding a key repeats 33×/s | keyboard scanned every frame without edge detection | 26-bit edge mask, §5 |
| Clock never sleeps | power save is gated on `g_timeValid` | §3, §8 |
| Board not detected over USB | charge-only cable, or the app owns the CDC port | [BUILDING.md](../BUILDING.md) §7 |
| M5Burner image rejected / blank | bootloader built with QIO instead of DIO | `board_build.flash_mode = dio`, [BUILDING.md](../BUILDING.md) §5 |
