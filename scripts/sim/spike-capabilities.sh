#!/usr/bin/env bash
# Probe whether this host/container can run the Android userland simulator:
# FUSE, loop devices, mount(2), per-filesystem kernel support, namespaces, FS tools.
# Prints a PASS/FAIL/SKIP table and exits non-zero only on hard prerequisites.
set -Eeuo pipefail

SUDO=""
if [[ $EUID -ne 0 ]]; then
  if sudo -n true 2>/dev/null; then SUDO="sudo -n"; else echo "FATAL: need root or passwordless sudo" >&2; exit 2; fi
fi

HARD_FAIL=0
row() { printf '%-34s %-6s %s\n' "$1" "$2" "${3:-}"; }
hard() { row "$1" FAIL "$2"; HARD_FAIL=1; }

tmp="$(mktemp -d)"
cleanup() { $SUDO umount "$tmp/mnt" 2>/dev/null || true; $SUDO losetup -D 2>/dev/null || true; rm -rf "$tmp"; }
trap cleanup EXIT

echo "== kernel: $(uname -r) =="

[[ -c /dev/fuse ]] && row "/dev/fuse" PASS || hard "/dev/fuse" "missing"
[[ -c /dev/loop-control ]] && row "/dev/loop-control" PASS || hard "/dev/loop-control" "missing"
grep -qw fuse /proc/filesystems && row "fs: fuse" PASS || hard "fs: fuse" "not in /proc/filesystems"

for fs in ext4 vfat exfat ntfs3 ntfs; do
  if grep -qw "$fs" /proc/filesystems; then row "kernel fs: $fs" PASS; else row "kernel fs: $fs" SKIP "no kernel driver (profile: absent)"; fi
done

if [[ -c /dev/mapper/control ]]; then
  row "dm-crypt" PASS
else
  row "dm-crypt" SKIP "device-mapper not available"
fi

if command -v mkfs.ext4 >/dev/null; then
  truncate -s 8M "$tmp/a.img" && mkfs.ext4 -q "$tmp/a.img"
  mkdir -p "$tmp/mnt"
  if $SUDO mount -o loop "$tmp/a.img" "$tmp/mnt" 2>/dev/null; then
    row "loop + mount(ext4)" PASS; $SUDO umount "$tmp/mnt"
  else
    hard "loop + mount(ext4)" "mount -o loop failed"
  fi
else
  row "loop + mount(ext4)" SKIP "mkfs.ext4 missing"
fi

if $SUDO unshare -m true 2>/dev/null; then row "unshare -m" PASS; else hard "unshare -m" "mount namespaces unavailable"; fi

for tool in mkfs.vfat fsck.vfat mkfs.exfat fsck.exfat e2fsck veracrypt cmake g++ jq; do
  if command -v "$tool" >/dev/null; then row "tool: $tool" PASS; else row "tool: $tool" SKIP "not installed"; fi
done

exit "$HARD_FAIL"
