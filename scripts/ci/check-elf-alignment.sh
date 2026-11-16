#!/usr/bin/env bash
# Fail if packaged JNI libs are not 16 KiB aligned (Android 15+ page size readiness).
source "$(dirname "$0")/lib.sh"

# Check the libs merged into the AAR/APK (one path per ABI), not every intermediate copy.
mapfile -t libs < <(
  find "$ROOT/core-crypto/build/intermediates/merged_native_libs" -path '*/mergeDebugNativeLibs/out/lib/*/libhcvault_jni.so' 2>/dev/null | sort -u
)
if [[ ${#libs[@]} -eq 0 ]]; then
  mapfile -t libs < <(find "$ROOT/core-crypto/build" -path '*/jni/*/libhcvault_jni.so' 2>/dev/null | sort -u | head -8)
fi
[[ ${#libs[@]} -gt 0 ]] || die "no libhcvault_jni.so under core-crypto/build (run build-ndk or assembleDebug first)"

so_load_align_ok() {
  local so=$1
  local aligns
  aligns="$(readelf -lW "$so" | awk '/^  LOAD/ {print $NF}')"
  [[ -n "$aligns" ]] || return 1
  while IFS= read -r align; do
    [[ "$align" == "0x4000" ]] || return 1
  done <<<"$aligns"
  return 0
}

fail=0
for so in "${libs[@]}"; do
  if so_load_align_ok "$so"; then
    log "aligned: $so"
  else
    log "misaligned: $so"
    readelf -lW "$so" | awk '/^  LOAD/ {printf "  LOAD align=%s\n", $NF}' >&2 || true
    fail=1
  fi
done

[[ $fail -eq 0 ]] || die "ELF segment alignment check failed (expected LOAD Align 0x4000 for all PT_LOAD)"