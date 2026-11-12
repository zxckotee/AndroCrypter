#!/usr/bin/env bash
# Push fixture to the device and run androidTest (VolumeOpenInstrumentedTest).
source "$(dirname "$0")/lib.sh"
cd "$ROOT"

[[ -n "${TEST_VOLUME_PASSWORD:-}" ]] || die "TEST_VOLUME_PASSWORD is required for instrumented tests"

adb wait-for-device

fixture_rel="$(jq -r '.fixtures[0].file' "$ROOT/testdata/manifest.json")"
fixture_local="$ROOT/$fixture_rel"
remote_path="/data/local/tmp/hcvault-fixture.hc"
adb push "$fixture_local" "$remote_path"

./gradlew --no-daemon :app:connectedDebugAndroidTest \
  -Pandroid.testInstrumentationRunnerArguments.TEST_VOLUME_PASSWORD="$TEST_VOLUME_PASSWORD" \
  -Pandroid.testInstrumentationRunnerArguments.fixture_path="$remote_path" \
  2>&1 | mask | tee "$ART/instrumented.log"
