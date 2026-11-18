#!/usr/bin/env bash
# Sim acceptance pipeline (E7): spike, fixtures, ext4/mount integration.
# Run inside privileged android-sim (docker compose run --rm android-sim …).
# android-ci / android-ci-nokvm lack fuse and root — use the sim-mount CI job instead.
source "$(dirname "$0")/lib.sh"

declare -A RESULT
STAGES=()
FAILED=0

stage() { # stage <name> <cmd...>
  local name=$1
  shift
  STAGES+=("$name")
  if run_logged "$name" "$@"; then RESULT[$name]=PASS; else RESULT[$name]=FAIL; FAILED=1; fi
}

skip_stage() { STAGES+=("$1"); RESULT[$1]="SKIP($2)"; }

finish() {
  local rc=$?
  {
    echo "== SIM SUMMARY =="
    for s in "${STAGES[@]}"; do printf '%-28s %s\n' "$s" "${RESULT[$s]}"; done
  } | tee "$ART/sim-summary.txt"
  if [[ $rc -ne 0 || $FAILED -ne 0 ]]; then exit 1; fi
}
trap finish EXIT

sim_fixture_stage() {
  local name=sim-fixtures
  STAGES+=("$name")
  if run_logged "$name" "$ROOT/scripts/sim/generate-fixtures.sh"; then
    if grep -qE 'SKIP\(' "$ART/$name.log"; then
      local why
      why=$(grep -m1 -oE 'SKIP\([^)]+\)' "$ART/$name.log" | sed 's/^SKIP(//;s/)$//')
      RESULT[$name]="SKIP($why)"
    else
      RESULT[$name]=PASS
    fi
  else
    RESULT[$name]=FAIL
    FAILED=1
  fi
}

sim_picture_stage() {
  local name=sim-acceptance-picture
  STAGES+=("$name")
  if run_logged "$name" "$ROOT/scripts/sim/acceptance-picture.sh"; then
    if grep -q 'acceptance-picture: PASS' "$ART/$name.log"; then
      RESULT[$name]=PASS
    elif grep -qE 'SKIP\(' "$ART/$name.log"; then
      local why
      why=$(grep -m1 -oE 'SKIP\([^)]+\)' "$ART/$name.log" | sed 's/^SKIP(//;s/)$//')
      RESULT[$name]="SKIP($why)"
    else
      RESULT[$name]=FAIL
      FAILED=1
    fi
  else
    RESULT[$name]=FAIL
    FAILED=1
  fi
}

sim_mount_stage() {
  local name=sim-mount-ext4
  STAGES+=("$name")
  if run_logged "$name" "$ROOT/scripts/sim/test-mount-ext4.sh"; then
    if grep -q 'test-mount-ext4: PASS' "$ART/$name.log"; then
      RESULT[$name]=PASS
    elif grep -qE '^SKIP:' "$ART/$name.log"; then
      local why
      why=$(grep -m1 '^SKIP:' "$ART/$name.log" | sed 's/^SKIP: //')
      RESULT[$name]="SKIP($why)"
    else
      RESULT[$name]=PASS
    fi
  else
    RESULT[$name]=FAIL
    FAILED=1
  fi
}

cd "$ROOT"

if [[ "${ALLOW_SIM_SKIP:-}" == "1" ]] && [[ $EUID -ne 0 ]] && [[ ! -w /dev/fuse ]]; then
  log "sim tier unavailable (no root, no /dev/fuse): honest SKIP (ALLOW_SIM_SKIP=1)"
  skip_stage sim-spike "no-privileged-sim"
  skip_stage sim-fixtures "no-privileged-sim"
  skip_stage sim-mount-ext4 "no-privileged-sim"
  skip_stage sim-acceptance-picture "no-privileged-sim"
  exit 0
fi

export ART
export HCVAULT_SIM_FIXTURE_DIR="${HCVAULT_SIM_FIXTURE_DIR:-$ART/sim-fixtures}"

stage sim-spike "$ROOT/scripts/sim/spike-capabilities.sh"
sim_fixture_stage
sim_mount_stage
sim_picture_stage

exit 0