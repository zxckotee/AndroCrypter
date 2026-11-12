#!/usr/bin/env bash
# Best-effort cleanup; never fails the pipeline.
source "$(dirname "$0")/lib.sh"
set +e

if command -v adb >/dev/null 2>&1; then
  adb emu kill >/dev/null 2>&1
fi
if [[ -f "$ROOT/.emulator.pid" ]]; then
  kill "$(cat "$ROOT/.emulator.pid")" >/dev/null 2>&1
  rm -f "$ROOT/.emulator.pid"
fi
if command -v adb >/dev/null 2>&1; then
  adb kill-server >/dev/null 2>&1
fi
rm -rf "$ROOT/native/build/tmp-fixtures"
exit 0
