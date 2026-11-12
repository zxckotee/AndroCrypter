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

# 2. Fixtures + native
stage fixtures-verify "$ROOT/scripts/fixtures/generate-fixtures.sh" --verify
if [[ -n "${TEST_VOLUME_PASSWORD:-}" ]]; then
  stage native-tests "$ROOT/scripts/ci/run-native-tests.sh"
else
  skip_stage native-tests "no TEST_VOLUME_PASSWORD"
fi

# 3. APK + NDK
stage build-ndk "$ROOT/scripts/ci/build-ndk.sh"
stage build-apk "$ROOT/scripts/ci/build-debug-apk.sh"

# 4. Emulator tier
if has_kvm; then
  stage emulator "$ROOT/scripts/ci/start-emulator.sh"
  if [[ -n "${TEST_VOLUME_PASSWORD:-}" ]]; then
    stage instrumented "$ROOT/scripts/ci/run-instrumented-tests.sh"
  else
    skip_stage instrumented "no TEST_VOLUME_PASSWORD"
  fi
  stage elf-alignment "$ROOT/scripts/ci/check-elf-alignment.sh"
else
  if [[ "${ALLOW_NO_KVM_SKIP:-}" == "1" ]]; then
    skip_stage emulator "no-kvm"
    skip_stage instrumented "no-kvm"
    skip_stage elf-alignment "no-kvm"
  else
    log "no KVM: set ALLOW_NO_KVM_SKIP=1 to skip emulator stages"
    die "missing /dev/kvm"
  fi
fi
