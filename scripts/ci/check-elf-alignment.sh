#!/usr/bin/env bash
# Fail if packaged JNI libs are not 16 KiB aligned (Android 15+ page size readiness).
source "$(dirname "$0")/lib.sh"

fail=0
mapfile -t libs < <(find "$ROOT/core-crypto/build" -name 'libhcvault_jni.so' 2>/dev/null)
[[ ${#libs[@]} -gt 0 ]] || die "no libhcvault_jni.so under core-crypto/build (run build-ndk first)"
for so in "${libs[@]}"; do
  if ! readelf -lW "$so" | grep -q 'ALIGN 0x4000'; then
    log "misaligned: $so"
    fail=1
  else
    log "aligned: $so"
  fi
done

[[ $fail -eq 0 ]] || die "ELF segment alignment check failed (expected ALIGN 0x4000)"
