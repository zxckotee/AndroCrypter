#!/usr/bin/env bash
# Assemble the debug app and the androidTest APK; copy the debug APK to artifacts/.
source "$(dirname "$0")/lib.sh"
cd "$ROOT"

./gradlew --no-daemon :app:assembleDebug :app:assembleDebugAndroidTest 2>&1 | tee "$ART/gradle-assemble.log"
[[ "${PIPESTATUS[0]}" -eq 0 ]] || die "gradle assemble failed"

APK="$(ls app/build/outputs/apk/debug/*.apk | head -1)"
[[ -f "$APK" ]] || die "no debug apk produced"
cp "$APK" "$ART/"
log "debug apk: $ART/$(basename "$APK")"
