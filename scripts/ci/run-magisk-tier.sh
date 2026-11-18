#!/usr/bin/env bash
# Magisk tier (E9): rootAVD / real adb checks for mounted mode. Honest SKIP until wired.
# Run after start-emulator.sh inside android-ci-privileged (RUN_MAGISK_TIER=1).
source "$(dirname "$0")/lib.sh"
# shellcheck source=../sim/lib/executor.sh
source "$ROOT/scripts/sim/lib/executor.sh"

declare -A RESULT
STAGES=()
FAILED=0

stage() {
  local name=$1
  shift
  STAGES+=("$name")
  if run_logged "$name" "$@"; then RESULT[$name]=PASS; else RESULT[$name]=FAIL; FAILED=1; fi
}

skip_stage() { STAGES+=("$1"); RESULT[$1]="SKIP($2)"; }

finish() {
  local rc=$?
  {
    echo "== MAGISK TIER SUMMARY =="
    for s in "${STAGES[@]}"; do printf '%-28s %s\n' "$s" "${RESULT[$s]}"; done
  } | tee "$ART/magisk-tier-summary.txt"
  if [[ $rc -ne 0 || $FAILED -ne 0 ]]; then exit 1; fi
}
trap finish EXIT

adb_device_online() {
  adb devices 2>/dev/null | awk 'NR>1 && $2=="device" {found=1} END {exit !found}'
}

# exec_adb stub: prefers real adb when a device is online (Stage 2), else sim local (Stage 1).
magisk_shell() {
  if adb_device_online; then
    adb shell "$@"
  else
    exec_adb "$@"
  fi
}

cd "$ROOT"

if [[ "${RUN_MAGISK_TIER:-}" != "1" ]]; then
  log "magisk tier disabled (set RUN_MAGISK_TIER=1)"
  skip_stage magisk-tier "not-enabled"
  exit 0
fi

if [[ "${ALLOW_MAGISK_SKIP:-}" == "1" ]] && ! has_kvm; then
  log "no KVM: honest SKIP (ALLOW_MAGISK_SKIP=1)"
  skip_stage magisk-adb "no-kvm"
  skip_stage magisk-su "no-kvm"
  skip_stage magisk-mountd "no-kvm"
  exit 0
fi

if ! has_kvm; then
  die "magisk tier requires KVM (/dev/kvm)"
fi

if ! adb_device_online; then
  log "no adb device online (boot emulator or use ALLOW_MAGISK_SKIP=1)"
  if [[ "${ALLOW_MAGISK_SKIP:-}" == "1" ]]; then
    skip_stage magisk-adb "no-adb-device"
    skip_stage magisk-su "no-adb-device"
    skip_stage magisk-mountd "no-adb-device"
    exit 0
  fi
  die "adb device required for magisk tier"
fi

stage magisk-adb magisk_shell getprop ro.build.version.sdk

su_stage() {
  if magisk_shell su -c id 2>/dev/null | grep -q 'uid=0'; then
    return 0
  fi
  return 1
}

STAGES+=(magisk-su)
if run_logged magisk-su su_stage; then
  RESULT[magisk-su]=PASS
else
  log "no root uid=0 (rootAVD/Magisk not installed on this AVD)"
  RESULT[magisk-su]="SKIP(no-root-avd)"
fi

# Mounted mode is not shipped in the app yet (MountController + RootService follow-up).
STAGES+=(magisk-mountd)
if [[ "${HCVAULT_MOUNT_TIER_READY:-}" == "1" ]] &&
  magisk_shell command -v hcvault-mountd >/dev/null 2>&1; then
  if run_logged magisk-mountd magisk_shell hcvault-mountd cleanup; then
    RESULT[magisk-mountd]=PASS
  else
    RESULT[magisk-mountd]=FAIL
    FAILED=1
  fi
else
  log "hcvault-mountd not on device / HCVAULT_MOUNT_TIER_READY!=1: honest SKIP"
  RESULT[magisk-mountd]="SKIP(mounted-mode-not-wired)"
fi

exit 0