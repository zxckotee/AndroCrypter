#!/usr/bin/env bash
# Sim fixtures: VeraCrypt volume with a PNG on /data/media/0 (uses TEST_VOLUME_PASSWORD).
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=scripts/ci/lib.sh
source "$ROOT/scripts/ci/lib.sh"

OUT="${HCVAULT_SIM_FIXTURE_DIR:-$ART/sim-fixtures}"
VC="${HCVAULT_VERACRYPT:-veracrypt}"
HC="${OUT}/sim-golden.hc"
MNT="${OUT}/mnt"
PNG="${OUT}/fixture.png"

mkdir -p "$OUT"
cleanup() {
  if mountpoint -q "$MNT" 2>/dev/null; then
    umount "$MNT" 2>/dev/null || true
  fi
  losetup -D 2>/dev/null || true
}
trap cleanup EXIT

make_png() {
  python3 - "$PNG" <<'PY'
import struct, zlib, sys
path = sys.argv[1]
w, h = 4, 4
rows = b"".join(b"\x00" + (b"\xff\x00\x00" * w) for _ in range(h))
compressed = zlib.compress(rows)
png = (
    b"\x89PNG\r\n\x1a\n"
    + struct.pack(">I", 13) + b"IHDR" + struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    + struct.pack(">I", zlib.crc32(b"IHDR" + struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) & 0xFFFFFFFF)
    + struct.pack(">I", len(compressed)) + b"IDAT" + compressed
    + struct.pack(">I", zlib.crc32(b"IDAT" + compressed) & 0xFFFFFFFF)
    + struct.pack(">I", 0) + b"IEND"
    + struct.pack(">I", zlib.crc32(b"IEND") & 0xFFFFFFFF)
)
open(path, "wb").write(png)
PY
}

if [[ -z "${TEST_VOLUME_PASSWORD:-}" ]]; then
  log "TEST_VOLUME_PASSWORD unset — writing PNG only at $PNG"
  make_png
  echo "SKIP(no-password): $HC not created"
  exit 0
fi

if ! command -v "$VC" >/dev/null 2>&1; then
  log "SKIP(no-veracrypt): $VC not found (see docker/android-sim/Dockerfile or set HCVAULT_VERACRYPT)"
  make_png
  exit 0
fi

SIZE="${HCVAULT_SIM_HC_SIZE:-8M}"
if [[ ! -f "$HC" ]]; then
  log "creating sim VeraCrypt volume: $HC"
  head -c 8192 /dev/urandom >"$OUT/random-source.bin"
  "$VC" --text --non-interactive --create "$HC" --size "$SIZE" --volume-type normal \
    --encryption AES --hash sha512 --filesystem FAT -p "$TEST_VOLUME_PASSWORD" --pim 0 -k "" \
    --random-source "$OUT/random-source.bin" >/dev/null
fi

rm -rf "$MNT"
mkdir -p "$MNT"
log "mounting $HC (host mount for fixture seeding)"
"$VC" --text --non-interactive --mount "$HC" "$MNT" -p "$TEST_VOLUME_PASSWORD" --pim 0 --protect-hidden=no >/dev/null

make_png
install -m 0644 "$PNG" "$MNT/hcvault-fixture.png"

if [[ -d /data/media/0 ]]; then
  install -m 0644 "$PNG" /data/media/0/hcvault-fixture.png 2>/dev/null || true
fi

log "sim fixtures ready: $HC ($PNG on volume and /data/media/0 when layout exists)"