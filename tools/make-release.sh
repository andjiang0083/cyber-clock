#!/bin/sh
# ---------------------------------------------------------------------------
# Build both targets and produce merged (M5Burner / esptool) images in dist/.
#
#   sh tools/make-release.sh
#
# Requires PlatformIO Core and the esptool that ships with the espressif32
# platform. The version string comes from src/config.h, so bump VERSION there
# (both targets) before running this.
# ---------------------------------------------------------------------------
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
DIST="$ROOT/dist"
PIO=${PIO:-pio}

ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

[ -f "$ESPTOOL" ] || { echo "esptool not found: $ESPTOOL" >&2; exit 1; }
[ -f "$BOOT_APP0" ] || { echo "boot_app0.bin not found: $BOOT_APP0" >&2; exit 1; }

VERSION=$(sed -n 's/^#define VERSION "\(.*\)"/\1/p' "$ROOT/src/config.h" | head -1)
[ -n "$VERSION" ] || { echo "could not read VERSION from src/config.h" >&2; exit 1; }

STICK_ENV=sticks3
CARD_ENV=cardputer
STICK_OUT="$DIST/cyber-clock-$VERSION-merged.bin"
CARD_OUT="$DIST/cyber-clock-cardputer-$VERSION-merged.bin"

mkdir -p "$DIST"

echo "== building $STICK_ENV =="
(cd "$ROOT" && $PIO run)

echo "== building $CARD_ENV =="
(cd "$ROOT/cardputer" && $PIO run)

echo "== merging =="
python3 "$ESPTOOL" --chip esp32s3 merge_bin \
  --output "$STICK_OUT" \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0000 "$ROOT/.pio/build/$STICK_ENV/bootloader.bin" \
  0x8000 "$ROOT/.pio/build/$STICK_ENV/partitions.bin" \
  0xe000 "$BOOT_APP0" \
  0x10000 "$ROOT/.pio/build/$STICK_ENV/firmware.bin"

python3 "$ESPTOOL" --chip esp32s3 merge_bin \
  --output "$CARD_OUT" \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0000 "$ROOT/cardputer/.pio/build/$CARD_ENV/bootloader.bin" \
  0x8000 "$ROOT/cardputer/.pio/build/$CARD_ENV/partitions.bin" \
  0xe000 "$BOOT_APP0" \
  0x10000 "$ROOT/cardputer/.pio/build/$CARD_ENV/firmware.bin"

echo "== verifying =="
for f in "$STICK_OUT" "$CARD_OUT"; do
  mode=$(python3 -c "import sys; d=open(sys.argv[1],'rb').read(); print('DIO' if d[2]==0x02 else hex(d[2]))" "$f")
  [ "$mode" = "DIO" ] || { echo "FAIL: $f has flash mode $mode, expected DIO" >&2; exit 1; }
  printf '%s  %s  (%s bytes)\n' "$(shasum -a 256 "$f" | cut -d' ' -f1)" "$(basename "$f")" "$(wc -c <"$f" | tr -d ' ')"
done

echo
echo "done — attach the two .bin files to the GitHub release and paste the sha256 above into the release notes."
