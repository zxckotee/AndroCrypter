# HCVault

Open-source Android client for VeraCrypt-compatible volumes (`.hc`).

Two modes:

1. **Non-mounted (default, no root).** Decryption and the file system run in userspace; files are
   browsed, imported and exported from the app. This is the MVP.
2. **Mounted (Magisk).** With Magisk or a compatible `su` (libsu, mount-master) the decrypted volume
   is exposed through loop + kernel mount. Planned after the non-mounted MVP.

Status: bootstrap. See [docs/architecture.md](docs/architecture.md) for the design and
[docs/testing-docker.md](docs/testing-docker.md) for the CI/test setup.

## Quick start

Requirements: Docker with Compose v2. KVM (`/dev/kvm`) is needed only for the emulator stages.

```bash
export HOST_UID=$(id -u) HOST_GID=$(id -g) KVM_GID=$(stat -c %g /dev/kvm 2>/dev/null || echo 993)
export TEST_VOLUME_PASSWORD='choose-a-test-password'   # synthetic fixture password, never committed

scripts/ci/docker/build-image.sh                        # once
docker compose run --rm android-ci scripts/ci/run-local.sh

# no KVM: static + JVM + build stages only
docker compose run --rm android-ci-nokvm
```

Logs, JUnit reports and the debug APK end up in `artifacts/`. Any failing stage makes the script
exit non-zero.

## Without Docker

JDK 17, Android SDK (platform 36, build-tools 36.0.0, NDK 27.2.12479018, CMake 3.22.1) and
`ANDROID_HOME` are required:

```bash
./gradlew detekt lintDebug testDebugUnitTest assembleDebug
```

## Project layout

| Path | Purpose |
|------|---------|
| `app/` | Compose UI, Hilt, navigation |
| `core-common/` | Result types, wipeable secrets, settings contract, logging |
| `core-crypto/` | Volume engine contract (JNI + native layer follow) |
| `core-storage/` | SAF helpers, DocumentsProvider |
| `core-mount/` | libsu / RootService (mounted mode) |
| `core-test-fixtures/` | Fixture manifest helpers for tests |
| `build-logic/` | Gradle convention plugins |
| `scripts/ci/` | CI scripts (run identically locally and in CI) |
| `docs/` | Architecture, testing, threat model, licensing |

## License

Application code: Apache-2.0 (see [LICENSE](LICENSE) and [NOTICE](NOTICE)).
