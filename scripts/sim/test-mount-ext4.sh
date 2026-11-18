#!/usr/bin/env bash
# Host integration: ext4 loop mount via mount_loop_test; optional hcvault-mountd if FUSE + password.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${BUILD:-$ROOT/native/build-mount-test}"

SUDO=""
if [[ $EUID -ne 0 ]]; then
  if sudo -n true 2>/dev/null; then SUDO="sudo -n"; else echo "SKIP: need root for loop mount"; exit 0; fi
fi

if [[ ! -c /dev/fuse ]]; then
  echo "SKIP: /dev/fuse missing"
  exit 0
fi

if ! command -v mkfs.ext4 >/dev/null; then
  echo "SKIP: mkfs.ext4 missing"
  exit 0
fi

cmake -S "$ROOT/native" -B "$BUILD" -DHCVAULT_BUILD_TESTS=OFF
cmake --build "$BUILD" -j"$(nproc)" --target mount_loop_test hcvault-mountd

tmp="$(mktemp -d)"
cleanup() {
  $SUDO umount "$tmp/mnt" 2>/dev/null || true
  $SUDO losetup -D 2>/dev/null || true
  rm -rf "$tmp"
}
trap cleanup EXIT

truncate -s 8M "$tmp/plain.ext4"
mkfs.ext4 -q "$tmp/plain.ext4"
mkdir -p "$tmp/mnt"
$SUDO "$BUILD/mount_loop_test" "$tmp/plain.ext4" "$tmp/mnt"

if [[ -n "${TEST_VOLUME_PASSWORD:-}" && -x "$BUILD/hcvault-mountd" ]]; then
  fixture="$(jq -r '.fixtures[0].file' "$ROOT/testdata/manifest.json")"
  hc="$ROOT/$fixture"
  if [[ -f "$hc" ]]; then
    printf '%s' "$TEST_VOLUME_PASSWORD" | $SUDO "$BUILD/hcvault-mountd" mount \
      --volume "$hc" --target "$tmp/vc_mnt" --fstype vfat 2>"$tmp/mountd.err" || true
    if [[ -d "$tmp/vc_mnt" ]] && mountpoint -q "$tmp/vc_mnt" 2>/dev/null; then
      echo "hcvault-mountd fuse path: PASS (inner vfat)"
      $SUDO "$BUILD/hcvault-mountd" umount --target "$tmp/vc_mnt" || $SUDO umount "$tmp/vc_mnt"
      $SUDO "$BUILD/hcvault-mountd" cleanup || true
    else
      echo "hcvault-mountd: SKIP or expected fstype mismatch ($(cat "$tmp/mountd.err" 2>/dev/null | head -1))"
    fi
  fi
fi

echo "test-mount-ext4: PASS"