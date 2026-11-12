# VeraCrypt vendoring notes

HCVault embeds a **minimal** subset of VeraCrypt for offline volume decryption. Full GUI, platform
drivers, and most of `Common/` are omitted.

## What we ship

| Area | Notes |
|------|------|
| `Common/` | `Crc`, `Endian`, `Pkcs5`, `Crypto`, `Xts`, `Volumes` (+ headers they pull in). |
| `Crypto/` | AES, SHA-2, Serpent, Twofish, Whirlpool, Camellia, Kuznyechik, Streebog, Blake2s, `cpu`. |
| Patches | None as standalone `.patch` files yet — behavioral changes live in `native/compat/` and
  `native/wrapper/vc_api.c`. |

## Compatibility shims (`native/compat/`)

- `hcvault_preinclude.h` / `hcvault_stubs.c` — replace Windows-centric and UI-only symbols.
- `encryption_thread_pool_minimal.c` — single-thread XTS path for mobile.

## Upstream refresh

1. Record the upstream tag in this file when bumping.
2. Re-apply shims; rebuild `veracrypt_volume` and run `scripts/ci/run-native-tests.sh`.
3. Update `docs/licenses.md` if license texts change.

Current pin: vendored tree as committed in this repository (bootstrap import).
