#!/usr/bin/env bash
# Magisk-style su shim: -c, -M/--mount-master; grant database under the app data dir.
set -Eeuo pipefail

_shim="$(readlink -f "${BASH_SOURCE[0]}")"
ROOT="$(cd "$(dirname "$_shim")/../.." && pwd)"
# shellcheck source=scripts/sim/lib/executor.sh
source "$ROOT/scripts/sim/lib/executor.sh"

: "${HCVAULT_APP_PKG:=org.hcvault.app}"

mount_master=0
cmd=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    -M|--mount-master) mount_master=1; shift ;;
    -c)
      shift
      [[ $# -ge 1 ]] || { echo "su: -c requires a command" >&2; exit 1; }
      cmd=$1
      shift
      break
      ;;
    --help|-h)
      echo "su [-M] -c <command>" >&2
      exit 0
      ;;
    *)
      echo "su: unknown option: $1" >&2
      exit 1
      ;;
  esac
done

[[ -n "$cmd" ]] || { echo "su: expected -c <command>" >&2; exit 1; }

caller_uid="${HCVAULT_SU_CALLER_UID:-$(id -u)}"
grant_db="/data/data/$HCVAULT_APP_PKG/files/su_grants.tsv"

if [[ ! -f "$grant_db" ]] || ! grep -q "^${caller_uid}:allow" "$grant_db" 2>/dev/null; then
  echo "su: permission denied for uid $caller_uid" >&2
  exit 1
fi

run_as_root() {
  elevate() {
    if [[ $EUID -eq 0 ]]; then
      "$@"
      return
    fi
    if sudo -n -u root "$@" 2>/dev/null; then
      return
    fi
    echo "su: elevation failed (need root or passwordless sudo for hcvault_app)" >&2
    exit 1
  }
  if [[ $mount_master -eq 1 ]]; then
    elevate unshare -m --propagation slave -- bash -c \
      'mount --make-rslave / 2>/dev/null || true; exec bash -c "$1"' _ "$cmd"
  else
    elevate bash -c "$cmd"
  fi
}

run_as_root