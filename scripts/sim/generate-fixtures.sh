#!/usr/bin/env bash
# Sim fixtures: VeraCrypt volume with a PNG on /data/media/0 (uses TEST_VOLUME_PASSWORD).
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=scripts/ci/lib.sh
source "$ROOT/scripts/ci/lib.sh"
# shellcheck source=scripts/sim/lib/executor.sh
source "$ROOT/scripts/sim/lib/executor.sh"

OUT="${HCVAULT_SIM_FIXTURE_DIR:-$ART/sim-fixtures}"
VC="${HCVAULT_VERACRYPT:-veracrypt}"
HC="${OUT}/sim-golden.hc"
MNT="${OUT}/mnt"
PNG="${OUT}/fixture.png"
BUILD="${HCVAULT_BUILD_DIR:-$ROOT/native/build-sim}"
MOUNTD="$BUILD/hcvault-mountd"
PW_FILE="$OUT/password.bin"

mkdir -p "$OUT"
SUDO=""
if [[ $EUID -ne 0 ]]; then
  sudo -n true 2>/dev/null && SUDO="sudo -n"
fi

cleanup() {
  if [[ -x "$MOUNTD" ]]; then
    $SUDO "$MOUNTD" umount --target "$MNT" 2>/dev/null || $SUDO umount "$MNT" 2>/dev/null || true
    $SUDO "$MOUNTD" cleanup 2>/dev/null || true
  elif mountpoint -q "$MNT" 2>/dev/null; then
    umount "$MNT" 2>/dev/null || true
  fi
  losetup -D 2>/dev/null || true
  rm -f "$PW_FILE"
}
trap cleanup EXIT

seed_via_mountd() {
  if [[ ! -c /dev/fuse ]]; then
    log "SKIP(no-fuse): cannot seed fixture PNG"
    return 1
  fi
  if [[ -z "$SUDO" && $EUID -ne 0 ]]; then
    log "SKIP(no-root): cannot seed fixture PNG"
    return 1
  fi
  cmake -S "$ROOT/native" -B "$BUILD" -DHCVAULT_BUILD_TESTS=OFF \
    -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER:-gcc}" -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER:-g++}" >/dev/null
  cmake --build "$BUILD" -j"$(nproc)" --target hcvault-mountd >/dev/null
  rm -rf "$MNT"
  mkdir -p "$MNT"
  printf '%s' "$TEST_VOLUME_PASSWORD" >"$PW_FILE"
  chmod 0600 "$PW_FILE"
  if ! $SUDO "$MOUNTD" mount --volume "$HC" --target "$MNT" --fstype vfat --password-fd 0 <"$PW_FILE"; then
    log "FAIL: hcvault-mountd seed mount"
    return 1
  fi
  install -m 0644 "$PNG" "$MNT/hcvault-fixture.png"
  return 0
}

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

SIZE="${HCVAULT_SIM_HC_SIZE:-8M}"
SIZE_BYTES=$((8 * 1024 * 1024))
case "$SIZE" in
  *M|*m) SIZE_BYTES=$((${SIZE%[Mm]} * 1024 * 1024)) ;;
  *K|*k) SIZE_BYTES=$((${SIZE%[Kk]} * 1024)) ;;
  *) SIZE_BYTES=$SIZE ;;
esac

make_png
sha256sum "$PNG" | awk '{print $1}' >"$PNG.sha256"

if [[ ! -f "$HC" ]]; then
  cmake -S "$ROOT/native" -B "$BUILD" -DHCVAULT_BUILD_TESTS=ON \
    -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER:-gcc}" -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER:-g++}" >/dev/null
  cmake --build "$BUILD" -j"$(nproc)" --target hcvault-mkvol >/dev/null
  printf '%s' "$TEST_VOLUME_PASSWORD" >"$PW_FILE"
  chmod 0600 "$PW_FILE"
  log "creating sim volume via hcvault-mkvol: $HC"
  "$BUILD/hcvault-mkvol" --size "$SIZE_BYTES" --format fat12 --password-file "$PW_FILE" -o "$HC"
fi

seed_via_vfs() {
  cmake -S "$ROOT/native" -B "$BUILD" -DHCVAULT_BUILD_TESTS=ON \
    -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER:-gcc}" -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER:-g++}" >/dev/null
  cmake --build "$BUILD" -j"$(nproc)" --target hcvault-seed-png >/dev/null
  printf '%s' "$TEST_VOLUME_PASSWORD" >"$PW_FILE"
  chmod 0600 "$PW_FILE"
  "$BUILD/hcvault-seed-png" --volume "$HC" --png "$PNG" --password-file "$PW_FILE"
}

log "seeding $HC via userspace FAT writer"
if ! seed_via_vfs; then
  echo "SKIP(seed-vfs): PNG not written into volume"
  exit 0
fi

if [[ -c /dev/fuse ]] && { [[ -n "$SUDO" ]] || [[ $EUID -eq 0 ]]; }; then
  log "optional mountd smoke after vfs seed"
  seed_via_mountd || log "mountd seed skipped (kernel vfat/fuse policy)"
fi

if [[ -d /data/media/0 ]]; then
  install -m 0644 "$PNG" /data/media/0/hcvault-fixture.png 2>/dev/null || true
fi

log "sim fixtures ready: $HC ($PNG on volume and /data/media/0 when layout exists)"