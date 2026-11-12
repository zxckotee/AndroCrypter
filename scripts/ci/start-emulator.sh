#!/usr/bin/env bash
# Boot the CI AVD (google_apis x86_64) and wait until the device is online.
source "$(dirname "$0")/lib.sh"

export ANDROID_AVD_HOME="${ANDROID_AVD_HOME:-/opt/avd}"
AVD_NAME="${HCVAULT_AVD:-hcvault_ci}"

if ! avdmanager list avd | grep -q "Name: $AVD_NAME"; then
  echo no | avdmanager create avd -n "$AVD_NAME" -k "system-images;android-34;google_apis;x86_64" \
    -d pixel_6 || die "failed to create AVD"
fi

adb start-server
adb kill-server 2>/dev/null || true
adb start-server

nohup emulator -avd "$AVD_NAME" -no-window -no-audio -no-boot-anim -accel on \
  -gpu swiftshader_indirect -grpc 8554 >"$ART/emulator.log" 2>&1 &
echo $! >"$ROOT/.emulator.pid"

adb wait-for-device
booted=""
for _ in $(seq 1 120); do
  booted="$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')"
  [[ "$booted" == "1" ]] && break
  sleep 2
done
[[ "$booted" == "1" ]] || die "emulator failed to boot (see $ART/emulator.log)"
log "emulator ready"
