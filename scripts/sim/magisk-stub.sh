#!/usr/bin/env bash
# Minimal Magisk CLI stub for Stage 1 simulator (RootEnvironmentProbe: magisk -v / --path).
set -Eeuo pipefail

MAGISK_VER="${HCVAULT_MAGISK_VERSION:-26.4-sim}"
MAGISK_HOME="${HCVAULT_SIM_ROOT:-/opt/hcvault-sim}"

usage() {
  echo "usage: magisk {-v|--version|--path}" >&2
  exit 1
}

[[ $# -ge 1 ]] || usage

case "$1" in
  -v|--version)
    echo "$MAGISK_VER"
    ;;
  --path)
    echo "$MAGISK_HOME"
    ;;
  *)
    echo "magisk: unsupported argument: $1" >&2
    exit 1
    ;;
esac