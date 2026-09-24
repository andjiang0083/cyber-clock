# Contributing to CYBER CLOCK

[中文](CONTRIBUTING_CN.md) · **English**

Issues and pull requests are welcome. This file is short on purpose: it lists the
conventions that exist **because we broke something**, not personal taste.

## Before you start

| | |
|---|---|
| Build | PlatformIO Core 6.x — see [BUILDING.md](BUILDING.md) |
| Hardware | M5StickS3 and/or M5Cardputer. Say in the PR which one you actually flashed |
| Bug reports | Use the [issue form](../../issues/new/choose). It asks for the two details that matter most: **which target**, and **whether the device recovers on its own or needs a power cycle** |

## The rules that come from real breakage

1. **Two targets, one behaviour.** `src/` (StickS3) and `cardputer/src/` (Cardputer)
   are duplicated copies of the same program. If you change shared behaviour,
   change it in **both** and build **both** (`pio run` at the root, then
   `pio run` inside `cardputer/`). A PR that only builds one target will be asked
   to build the other.
2. **`config.h` is the only place for tunables.** Version string, idle
   thresholds, brightness steps, rain density, audio constants — all of it lives
   there, in both copies. No magic numbers in `main.cpp`.
3. **No new on-screen chrome.** The display carries what belongs on a clock. No
   FPS counter, no version watermark, no persistent debug overlay. If you need
   diagnostics, they go to the serial log — and even that stays quiet by default.
   This rule exists because the 240×135 panel has no room to spare, and because a
   clock that shows its own internals is not a clock.
4. **Power save is a contract.** Anything that keeps a peripheral busy must be
   released when the idle state machine engages (the microphone already is), and
   any new input path must (a) reset `g_lastActivity` and (b) cancel the idle
   state. USB charging must keep cancelling every stage. If your change makes the
   StickS3 fail to reach Light Sleep, that is a regression.
5. **Long press is not a shortcut for "call the short-press action twice".**
   v1.1.0 fixed a bug where a single hold fired both the tap and the hold action.
   Keep the tap/hold paths mutually exclusive.
6. **ASCII only.** The GLCD font has no CJK glyphs and the on-screen keyboard is
   ASCII. Do not add non-ASCII strings to the UI; they render as `□`.
7. **Don't break the vendored include.** `arcadia/core/` is a snapshot committed
   for buildability. If you must patch it, say so in the PR — and expect the
   maintainer to sync the change upstream by hand.
8. **Verify visually, not by watching a serial log.** The firmware is meant to be
   judged on the panel: check the rain, the clock, the battery bars and the
   spectrum after your change. A clean build proves nothing about the picture.

## Pull request checklist

Copy this into your PR description:

```
Target(s) changed: [ ] StickS3  [ ] Cardputer
Both targets build:            [ ] yes
Flashed and verified on:       [ ] StickS3  [ ] Cardputer  [ ] neither (build-only)
Checked after the change:      [ ] 3 display modes  [ ] 6 themes  [ ] 6 rain sets
                               [ ] brightness  [ ] power save entry/exit  [ ] USB override
Screenshots / photos:          (attached if the change is visual)
```

## Style

- Commit subjects follow the existing history: `feat:`, `fix:`, `docs:`,
  `refactor:`, `release:`.
- Small, reviewable patches. This project deliberately prefers many small steps
  over one large rewrite — a "big refactor" PR is likely to be asked to split.
- C++17, two-space indent, matching the surrounding file. Keep the existing
  comment style (bilingual comments are fine and common in this code base).
- Do not reformat unrelated code in a functional PR.

## Good first contributions

See [ROADMAP.md](ROADMAP.md) — every item there is scoped to be doable without
owning every piece of hardware, and the ones marked *needs a device* say so.
