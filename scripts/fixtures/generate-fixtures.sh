#!/usr/bin/env bash
# Verify or document regeneration of test volume fixtures.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MANIFEST="$ROOT/testdata/manifest.json"

die() { echo "FATAL: $*" >&2; exit 1; }

usage() {
  cat <<'EOF'
Usage: generate-fixtures.sh [--verify]

  --verify   Validate manifest.json and on-disk fixtures (default when no args).
EOF
}

verify_manifest() {
  command -v jq >/dev/null 2>&1 || die "jq is required"
  [[ -f "$MANIFEST" ]] || die "missing $MANIFEST"
  jq -e '.version > 0 and (.fixtures | length) > 0' "$MANIFEST" >/dev/null

  while IFS= read -r entry; do
    local file size path actual
    file="$(jq -r '.file' <<<"$entry")"
    size="$(jq -r '.sizeBytes' <<<"$entry")"
    path="$ROOT/$file"
    [[ -f "$path" ]] || die "missing fixture: $path"
    actual="$(stat -c%s "$path")"
    [[ "$actual" == "$size" ]] || die "size mismatch for $file: expected $size got $actual"
  done < <(jq -c '.fixtures[]' "$MANIFEST")
  echo "manifest ok ($(jq '.fixtures | length' "$MANIFEST") fixtures)"
}

run_smoke() {
  if [[ -z "${TEST_VOLUME_PASSWORD:-}" ]]; then
    echo "TEST_VOLUME_PASSWORD unset — skipping native smoke (metadata-only verify)"
    return 0
  fi
  local build_dir="$ROOT/native/build-smoke"
  cmake -S "$ROOT/native" -B "$build_dir" -DHCVAULT_BUILD_TESTS=OFF
  cmake --build "$build_dir" -j"$(nproc)" --target volume_smoke
  while IFS= read -r file; do
    "$build_dir/volume_smoke" "$ROOT/$file" || die "smoke failed for $file"
  done < <(jq -r '.fixtures[].file' "$MANIFEST")
  echo "native smoke ok"
}

main() {
  local mode=verify
  [[ $# -eq 0 ]] && mode=verify
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --verify) mode=verify ;;
      -h|--help) usage; exit 0 ;;
      *) die "unknown arg: $1" ;;
    esac
    shift
  done
  if [[ "$mode" == verify ]]; then
    verify_manifest
    run_smoke
  fi
}

main "$@"
