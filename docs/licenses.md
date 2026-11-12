# Third-party licenses

HCVault application code is under [Apache-2.0](../LICENSE). This document tracks bundled or
derived third-party code.

## VeraCrypt / TrueCrypt (native volume subset)

- **Source**: upstream VeraCrypt, vendored under `native/third_party/veracrypt/` (subset of
  `Common/` and `Crypto/` only).
- **Licenses**: Apache License 2.0 and TrueCrypt License 3.0 (dual-licensed upstream).
- **Usage**: linked into the static `veracrypt_volume` library and exposed through the narrow
  `vc_*` C ABI in `native/wrapper/vc_api.c`. See `native/third_party/veracrypt/patches/README.md`
  for local modifications.
- **Full texts**: `LICENSES/VeraCrypt-License.txt`, `LICENSES/VeraCrypt-README.txt`.

## Jetpack / AndroidX / Kotlin / coroutines

Apache License 2.0 — resolved via Gradle (`gradle/libs.versions.toml`).

## Dagger Hilt

Apache License 2.0.

## libsu

Apache License 2.0 (John Wu).

## GoogleTest (native unit tests only)

BSD 3-Clause — fetched at CMake configure time when `HCVAULT_BUILD_TESTS=ON`; not shipped in the APK.

## Compliance notes

- VeraCrypt and TrueCrypt names are trademarks of their respective owners; HCVault is an independent
  compatible client (see [NOTICE](../NOTICE)).
- Do not commit real user passwords or keyfiles. Test fixtures use synthetic passwords via
  `TEST_VOLUME_PASSWORD` only.
