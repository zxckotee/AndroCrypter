#!/usr/bin/env bash
# Build the JNI shared library for all CI ABIs via Gradle (core-crypto externalNativeBuild).
source "$(dirname "$0")/lib.sh"
cd "$ROOT"

./gradlew --no-daemon :core-crypto:assembleDebug

find_jni_so() {
  local abi=$1
  local merged="$ROOT/core-crypto/build/intermediates/merged_native_libs/debug/mergeDebugNativeLibs/out/lib/$abi/libhcvault_jni.so"
  [[ -f "$merged" ]] && { printf '%s' "$merged"; return 0; }
  find "$ROOT/core-crypto/.cxx" "$ROOT/core-crypto/build" -type f -name libhcvault_jni.so 2>/dev/null \
    | grep "/${abi}/" \
    | grep -v -E 'debugAndroidTest|AndroidTest' \
    | head -1
}

for abi in ${ABIS//,/ }; do
  so="$(find_jni_so "$abi")"
  [[ -n "$so" && -f "$so" ]] || die "missing libhcvault_jni.so for $abi (searched merged_native_libs, .cxx, build/)"
  cp "$so" "$ART/libhcvault_jni-$abi.so"
  log "ndk: $ART/libhcvault_jni-$abi.so (from $so)"
done