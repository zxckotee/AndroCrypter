#!/usr/bin/env bash
# Shared helpers for scripts/ci/*. Source this file; do not execute it.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ART="${ART:-$ROOT/artifacts}"
mkdir -p "$ART"

: "${NDK_VERSION:=27.2.12479018}"
: "${CMAKE_VERSION:=3.22.1}"
: "${ABIS:=arm64-v8a,x86_64}"
export NDK_VERSION CMAKE_VERSION ABIS

log() { printf '[%(%H:%M:%S)T] %s\n' -1 "$*"; }
die() { log "FATAL: $*" >&2; exit "${2:-1}"; }

# run_logged <name> <cmd...>: tee output to artifacts/<name>.log, return the command's exit code.
run_logged() {
  local name=$1
  shift
  log ">> $name"
  set +e
  "$@" 2>&1 | mask | tee "$ART/$name.log"
  local rc=${PIPESTATUS[0]}
  set -e
  return "$rc"
}

# mask: never echo the synthetic fixture password into CI logs.
mask() {
  if [[ -n "${TEST_VOLUME_PASSWORD:-}" ]]; then
    python3 -c '
import os, sys
secret = os.environ["TEST_VOLUME_PASSWORD"]
for line in sys.stdin:
    sys.stdout.write(line.replace(secret, "***"))
    sys.stdout.flush()
'
  else
    cat
  fi
}

has_kvm() { [[ -w /dev/kvm ]]; }
