#!/usr/bin/env bash
# Fish-friendly wrapper: oracle matrix + native interop tests (quick matrix).
# Usage from repo root:  bash scripts/native/run-oracle-quick.sh [/tmp/om]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

ORACLE_DIR="${1:-/tmp/hcvault-oracle}"
BUILD="${HCVAULT_NATIVE_BUILD:-$ROOT/native/build-ci}"

if ! command -v veracrypt >/dev/null 2>&1 && [[ -z "${HCVAULT_VERACRYPT:-}" ]]; then
  echo "WARN: veracrypt not in PATH; matrix step may SKIP (exit 3). Set HCVAULT_VERACRYPT." >&2
fi

bash "$ROOT/scripts/fixtures/make-oracle-matrix.sh" "$ORACLE_DIR" quick

CC="${CMAKE_C_COMPILER:-gcc}"
CXX="${CMAKE_CXX_COMPILER:-g++}"
cmake -S "$ROOT/native" -B "$BUILD" -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX"
cmake --build "$BUILD" -j"$(nproc)"

HCVAULT_ORACLE_DIR="$ORACLE_DIR" "$BUILD/hcvault_volume_tests" --gtest_filter='Interop*'