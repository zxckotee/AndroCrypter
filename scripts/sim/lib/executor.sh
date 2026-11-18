#!/usr/bin/env bash
# Command execution helpers for the Android userland simulator. Source this file.
set -Eeuo pipefail

_SIM_LIB="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$_SIM_LIB/../../.." && pwd)"

: "${HCVAULT_APP_PKG:=org.hcvault.app}"
: "${HCVAULT_APP_UID:=10123}"
: "${HCVAULT_SIM_ROOT:=/opt/hcvault-sim}"
: "${HCVAULT_PROC_FILESYSTEMS:=/proc/filesystems}"
: "${HCVAULT_SIM_EXECUTOR:=local}"

# sim_mount_ns <cmd...>  Per-app mount namespace (zygote-style rslave propagation).
sim_mount_ns() {
  unshare -m --propagation slave -- bash -c '
    set -Eeuo pipefail
    mount --make-rslave / 2>/dev/null || true
    exec "$@"
  ' _ "$@"
}

# exec_local <cmd...>  Run as the simulated app user (no root caps).
exec_local() {
  local -a cmd=("$@")
  local home="/data/data/$HCVAULT_APP_PKG"
  local path="$HCVAULT_SIM_ROOT/bin:/usr/local/bin:/usr/bin:/bin"
  if command -v runuser >/dev/null 2>&1 && [[ $EUID -eq 0 ]]; then
    runuser -u "#$HCVAULT_APP_UID" -- env \
      "HOME=$home" "PATH=$path" "HCVAULT_PROC_FILESYSTEMS=$HCVAULT_PROC_FILESYSTEMS" \
      "${cmd[@]}"
  else
    env "HOME=$home" "PATH=$path" "HCVAULT_PROC_FILESYSTEMS=$HCVAULT_PROC_FILESYSTEMS" "${cmd[@]}"
  fi
}

# exec_adb <cmd...>  Stage 1 stub: no adb server; delegates to exec_local.
exec_adb() {
  if [[ "$HCVAULT_SIM_EXECUTOR" == "adb" ]] && command -v adb >/dev/null 2>&1; then
    adb shell "$@"
    return $?
  fi
  exec_local "$@"
}

# kernel_has_fs <name>  Honors HCVAULT_PROC_FILESYSTEMS capability profiles.
kernel_has_fs() {
  local fs=$1
  local list="${HCVAULT_PROC_FILESYSTEMS:-/proc/filesystems}"
  [[ -f "$list" ]] || list=/proc/filesystems
  grep -qw "$fs" "$list"
}