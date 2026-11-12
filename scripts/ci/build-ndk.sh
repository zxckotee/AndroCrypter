#!/usr/bin/env bash
# Build the JNI shared library for all CI ABIs via Gradle (core-crypto externalNativeBuild).
source "$(dirname "$0")/lib.sh"
cd "$ROOT"

./gradlew --no-daemon :core-crypto:assembleDebug

for abi in ${ABIS//,/ }; do
  so="$ROOT/core-crypto/build/intermediates/cmake/debug/obj/$abi/libhcvault_jni.so"
  [[ -f "$so" ]] || die "missing libhcvault_jni.so for $abi"
  cp "$so" "$ART/libhcvault_jni-$abi.so"
  log "ndk: $ART/libhcvault_jni-$abi.so"
done
