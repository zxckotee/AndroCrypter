#!/usr/bin/env bash
# E7: disk-with-picture — mount via hcvault-mountd, sha256 round-trip, clean unmount.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=scripts/ci/lib.sh
source "$ROOT/scripts/ci/lib.sh"
# shellcheck source=scripts/sim/lib/executor.sh
source "$ROOT/scripts/sim/lib/executor.sh"

OUT="${HCVAULT_SIM_FIXTURE_DIR:-${ART:-$ROOT/artifacts}/sim-fixtures}"
HC="$OUT/sim-golden.hc"
GOLDEN_PNG="$OUT/fixture.png"
GOLDEN_SHA="$OUT/fixture.png.sha256"
BUILD="${HCVAULT_BUILD_DIR:-$ROOT/native/build-sim}"
MP="$OUT/mountd-mnt"

SUDO=""
if [[ $EUID -ne 0 ]]; then
  if sudo -n true 2>/dev/null; then SUDO="sudo -n"; else echo "SKIP(no-root): acceptance-picture"; exit 0; fi
fi

if [[ -z "${TEST_VOLUME_PASSWORD:-}" ]]; then
  echo "SKIP(no-password): acceptance-picture"
  exit 0
fi

if [[ ! -f "$HC" || ! -f "$GOLDEN_PNG" || ! -f "$GOLDEN_SHA" ]]; then
  echo "SKIP(no-fixtures): run generate-fixtures first ($HC)"
  exit 0
fi

VFS_ONLY=0
if ! kernel_has_fs vfat 2>/dev/null; then
  if ! grep -qw vfat "${HCVAULT_PROC_FILESYSTEMS:-/proc/filesystems}" 2>/dev/null; then
    VFS_ONLY=1
  fi
fi

EXPECT_SHA="$(awk '{print $1}' "$GOLDEN_SHA")"
if [[ -z "$EXPECT_SHA" ]]; then
  echo "SKIP(no-fixtures): missing $GOLDEN_SHA"
  exit 0
fi

cmake -S "$ROOT/native" -B "$BUILD" -DHCVAULT_BUILD_TESTS=ON \
  -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER:-gcc}" -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER:-g++}" >/dev/null
if [[ "$VFS_ONLY" == "1" ]]; then
  cmake --build "$BUILD" -j"$(nproc)" --target hcvault-vfs-digest >/dev/null
  printf '%s' "$TEST_VOLUME_PASSWORD" >"$OUT/password.bin"
  chmod 0600 "$OUT/password.bin"
  if ! "$BUILD/hcvault-vfs-digest" --volume "$HC" --password-file "$OUT/password.bin" \
    --expect-sha256 "$EXPECT_SHA" --path HCVAULTF.PNG; then
    echo "FAIL: vfs digest"
    exit 1
  fi
  echo "acceptance-picture: PASS vfs sha256=$EXPECT_SHA"
  exit 0
fi
if [[ ! -c /dev/fuse ]]; then
  echo "SKIP(no-fuse): kernel vfat mount path needs FUSE"
  exit 0
fi

cmake --build "$BUILD" -j"$(nproc)" --target hcvault-mountd >/dev/null

cleanup() {
  $SUDO "$BUILD/hcvault-mountd" umount --target "$MP" 2>/dev/null || $SUDO umount "$MP" 2>/dev/null || true
  $SUDO "$BUILD/hcvault-mountd" cleanup 2>/dev/null || true
  $SUDO losetup -D 2>/dev/null || true
  rm -rf "$MP"
}
trap cleanup EXIT

rm -rf "$MP"
mkdir -p "$MP"

if ! printf '%s' "$TEST_VOLUME_PASSWORD" | $SUDO "$BUILD/hcvault-mountd" mount \
  --volume "$HC" --target "$MP" --fstype vfat 2>"$OUT/mountd-picture.err"; then
  echo "FAIL: hcvault-mountd mount"
  head -5 "$OUT/mountd-picture.err" >&2
  exit 1
fi

if ! mountpoint -q "$MP"; then
  echo "FAIL: mountpoint not active"
  exit 1
fi

MNT_FILE="$MP/HCVAULTF.PNG"
if [[ ! -f "$MNT_FILE" ]]; then
  echo "FAIL: missing $MNT_FILE on mounted volume"
  ls -la "$MP" >&2 || true
  exit 1
fi

GOT_SHA="$(sha256sum "$MNT_FILE" | awk '{print $1}')"
if [[ "$GOT_SHA" != "$EXPECT_SHA" ]]; then
  echo "FAIL: sha256 mismatch got=$GOT_SHA expect=$EXPECT_SHA"
  exit 1
fi

if ! $SUDO "$BUILD/hcvault-mountd" umount --target "$MP"; then
  echo "FAIL: umount"
  exit 1
fi

if mountpoint -q "$MP" 2>/dev/null; then
  echo "FAIL: mount still present after umount"
  exit 1
fi

if grep -F "$MP" /proc/mounts 2>/dev/null | grep -qv '^#'; then
  echo "FAIL: stale entry in /proc/mounts for $MP"
  grep -F "$MP" /proc/mounts >&2 || true
  exit 1
fi

$SUDO "$BUILD/hcvault-mountd" cleanup >/dev/null 2>&1 || true
echo "acceptance-picture: PASS sha256=$EXPECT_SHA"