#!/usr/bin/env bash
# Generates a matrix of VeraCrypt volumes with the real VeraCrypt CLI (the interop oracle).
# Usage: make-oracle-matrix.sh OUTDIR [quick|full]
# Passwords are random throwaways written to OUTDIR/matrix.tsv; nothing here is committed.
set -euo pipefail

OUT="${1:?usage: make-oracle-matrix.sh OUTDIR [quick|full]}"
MODE="${2:-quick}"
VC="${HCVAULT_VERACRYPT:-veracrypt}"

if ! command -v "$VC" >/dev/null 2>&1; then
  echo "SKIP(no-veracrypt): $VC not found" >&2
  exit 3
fi

mkdir -p "$OUT"
rm -f "$OUT"/*.hc "$OUT"/*.key "$OUT"/matrix.tsv
head -c 8192 /dev/urandom >"$OUT/random-source.bin"
TSV="$OUT/matrix.tsv"
: >"$TSV"

SIZE="${HCVAULT_ORACLE_SIZE:-3M}"

rand_pw() { head -c 18 /dev/urandom | base64 | tr -d '/+=' | cut -c1-20; }

create() {
  # name type cipher hash pim keyfiles(comma, relative names) [fs]
  local name="$1" type="$2" cipher="$3" hash="$4" pim="$5" keyfiles="$6" fs="${7:-FAT}"
  local pw
  pw="$(rand_pw)"
  local kf_args=()
  local kf_abs=""
  if [[ -n "$keyfiles" ]]; then
    for k in ${keyfiles//,/ }; do
      kf_abs+="${kf_abs:+,}$OUT/$k"
    done
    kf_args=(-k "$kf_abs")
  else
    kf_args=(-k "")
  fi
  "$VC" --text --non-interactive --create "$OUT/$name.hc" --size "$SIZE" --volume-type "$type" \
    --encryption "$cipher" --hash "$hash" --filesystem "$fs" -p "$pw" --pim "$pim" "${kf_args[@]}" \
    --random-source "$OUT/random-source.bin" >/dev/null 2>&1
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$name" "$type" "$cipher" "$hash" "$pim" "$keyfiles" "$pw" >>"$TSV"
}

head -c 4096 /dev/urandom >"$OUT/kf1.key"
head -c 100 /dev/urandom >"$OUT/kf2.key"
head -c 70000 /dev/urandom >"$OUT/kf3.key"

if [[ "$MODE" == "full" ]]; then
  CIPHERS=(AES Serpent Twofish Camellia Kuznyechik AES-Twofish AES-Twofish-Serpent Serpent-AES
    Serpent-Twofish-AES Twofish-Serpent Camellia-Kuznyechik Camellia-Serpent Kuznyechik-AES
    Kuznyechik-Serpent-Camellia Kuznyechik-Twofish)
  HASHES=(sha512 whirlpool sha256 blake2s streebog)
  for c in "${CIPHERS[@]}"; do
    for h in "${HASHES[@]}"; do
      create "m-${c}-${h}" normal "$c" "$h" 0 ""
    done
  done
else
  for c in AES Serpent Twofish Camellia Kuznyechik AES-Twofish-Serpent Kuznyechik-Serpent-Camellia; do
    create "m-${c}-sha512" normal "$c" sha512 0 ""
  done
  for h in whirlpool sha256 blake2s streebog; do
    create "m-AES-${h}" normal AES "$h" 0 ""
  done
fi

create "pim-sha512" normal AES sha512 15 ""
create "pim-sha256" normal AES sha256 7 ""
create "pim-streebog" normal Serpent streebog 9 ""
create "kf-one" normal AES sha512 0 "kf1.key"
create "kf-three" normal AES-Twofish sha512 0 "kf1.key,kf2.key,kf3.key"

# Hidden volumes: outer volume (no filesystem, per VeraCrypt CLI procedure), then hidden inside it.
make_hidden() {
  local name="$1" cipher="$2" hash="$3"
  local outer_pw hid_pw
  outer_pw="$(rand_pw)"
  hid_pw="$(rand_pw)"
  "$VC" --text --non-interactive --create "$OUT/$name.hc" --size "$SIZE" --volume-type normal \
    --encryption "$cipher" --hash "$hash" --filesystem none -p "$outer_pw" --pim 0 -k "" \
    --random-source "$OUT/random-source.bin" >/dev/null 2>&1
  "$VC" --text --non-interactive --create "$OUT/$name.hc" --size 1M --volume-type hidden \
    --encryption "$cipher" --hash "$hash" --filesystem FAT -p "$hid_pw" --pim 0 -k "" \
    --random-source "$OUT/random-source.bin" >/dev/null 2>&1
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$name" normal "$cipher" "$hash" 0 "" "$outer_pw" >>"$TSV"
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$name" hidden "$cipher" "$hash" 0 "" "$hid_pw" >>"$TSV"
}

make_hidden "hidden-aes" AES sha512
make_hidden "hidden-serpent" Serpent sha256

echo "oracle matrix: $(wc -l <"$TSV") credential rows in $OUT"
