#!/usr/bin/env bash
# Full CI run, identical locally and in CI. Any failing stage => exit 1.
# Usage: docker compose run --rm android-ci scripts/ci/run-local.sh
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

cleanup() {
  local rc=$?
  "$ROOT/scripts/ci/teardown.sh" || true
  {
    echo "== SUMMARY =="
    for s in "${STAGES[@]}"; do printf '%-28s %s\n' "$s" "${RESULT[$s]}"; done
  } | tee "$ART/summary.txt"
  if [[ $rc -ne 0 || $FAILED -ne 0 ]]; then exit 1; fi
}
trap cleanup EXIT

cd "$ROOT"

# 1. Static analysis and JVM unit tests
stage detekt       ./gradlew --no-daemon detekt
stage android-lint ./gradlew --no-daemon lintDebug
stage jvm-unit     ./gradlew --no-daemon testDebugUnitTest

# 2. APK
stage build-apk "$ROOT/scripts/ci/build-debug-apk.sh"
