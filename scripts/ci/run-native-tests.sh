#!/usr/bin/env bash
# Host-side CMake build + gtest (when available) and volume_smoke against fixtures.
source "$(dirname "$0")/lib.sh"
cd "$ROOT"

[[ -n "${TEST_VOLUME_PASSWORD:-}" ]] || die "TEST_VOLUME_PASSWORD is required for native-tests"

BUILD="$ROOT/native/build-ci"
export LIBRARY_PATH="${LIBRARY_PATH:-/usr/lib/gcc/x86_64-linux-gnu/13}"
cmake -S "$ROOT/native" -B "$BUILD" -DHCVAULT_BUILD_TESTS=ON \
  -DCMAKE_C_COMPILER="${CMAKE_C_COMPILER:-gcc}" \
  -DCMAKE_CXX_COMPILER="${CMAKE_CXX_COMPILER:-g++}"
cmake --build "$BUILD" -j"$(nproc)"

fixture="$(jq -r '.fixtures[0].file' "$ROOT/testdata/manifest.json")"
export HCVAULT_FIXTURE="$ROOT/$fixture"

if command -v veracrypt >/dev/null 2>&1; then
  oracle="$(mktemp -d /tmp/hcvault-oracle.XXXXXX)"
  if "$ROOT/scripts/fixtures/make-oracle-matrix.sh" "$oracle" quick; then
    export HCVAULT_ORACLE_DIR="$oracle"
  fi
fi

if [[ -x "$BUILD/hcvault_volume_tests" ]]; then
  "$BUILD/hcvault_volume_tests"
fi
if [[ -x "$BUILD/vfs_smoke" ]]; then
  "$BUILD/vfs_smoke"
fi
"$BUILD/volume_smoke" "$ROOT/$fixture"
