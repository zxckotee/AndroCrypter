#!/usr/bin/env bash
# CI/containers often run as a host UID with no $HOME; Gradle still needs a debug keystore.
source "$(dirname "$0")/lib.sh"

home="${HOME:-/home/ci}"
export HOME="$home"
mkdir -p "$HOME/.android"
ks="$HOME/.android/debug.keystore"

if [[ -f "$ks" ]]; then
  log "debug keystore: $ks (existing)"
  exit 0
fi

if ! command -v keytool >/dev/null 2>&1; then
  die "keytool not found (need JDK in PATH)"
fi

keytool -genkeypair -v \
  -keystore "$ks" \
  -storepass android -alias androiddebugkey -keypass android \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -dname 'CN=Android Debug,O=Android,C=US' >/dev/null

log "debug keystore: $ks (created)"