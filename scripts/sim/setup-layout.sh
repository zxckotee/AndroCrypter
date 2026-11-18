#!/usr/bin/env bash
# Prepare Android-like paths, app UID 10123, Magisk/su stubs, and rslave mount propagation.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=scripts/sim/lib/executor.sh
source "$ROOT/scripts/sim/lib/executor.sh"

die() { echo "FATAL: $*" >&2; exit 1; }

[[ $EUID -eq 0 ]] || die "run as root inside android-sim (privileged container)"

ensure_app_user() {
  if ! getent passwd "$HCVAULT_APP_UID" >/dev/null; then
    useradd -u "$HCVAULT_APP_UID" -M -s /usr/sbin/nologin -c 'hcvault sim app' hcvault_app
  fi
}

install_stubs() {
  mkdir -p "$HCVAULT_SIM_ROOT/bin"
  # Symlink so su-shim can resolve $ROOT to /workspace (copies break path math).
  ln -sf "$ROOT/scripts/sim/magisk-stub.sh" "$HCVAULT_SIM_ROOT/bin/magisk"
  ln -sf "$ROOT/scripts/sim/su-shim.sh" "$HCVAULT_SIM_ROOT/bin/su"
}

layout_dirs() {
  local app_data="/data/data/$HCVAULT_APP_PKG"
  mkdir -p "$app_data/files" /data/media/0 /data/local/tmp /mnt
  ln -sfn /data/media/0 /sdcard
  chown -R "$HCVAULT_APP_UID:$HCVAULT_APP_UID" "$app_data"
  chmod 1777 /data/local/tmp
  chmod 0771 /data/media/0
}

default_su_grants() {
  local db="/data/data/$HCVAULT_APP_PKG/files/su_grants.tsv"
  if [[ ! -f "$db" ]]; then
    printf '%s:allow\n' "$HCVAULT_APP_UID" >"$db"
    chown "$HCVAULT_APP_UID:$HCVAULT_APP_UID" "$db"
    chmod 0600 "$db"
  fi
}

ensure_sudoers() {
  local f=/etc/sudoers.d/hcvault-sim
  if [[ ! -f "$f" ]]; then
    printf 'hcvault_app ALL=(root) NOPASSWD: ALL\n' >"$f"
    chmod 0440 "$f"
  fi
}

apply_rslave() {
  mount --make-rslave / 2>/dev/null || true
}

main() {
  ensure_app_user
  ensure_sudoers
  install_stubs
  layout_dirs
  default_su_grants
  apply_rslave
  echo "sim layout: pkg=$HCVAULT_APP_PKG uid=$HCVAULT_APP_UID stubs=$HCVAULT_SIM_ROOT/bin"
}

main "$@"