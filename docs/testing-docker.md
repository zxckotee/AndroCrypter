# Testing in Docker

All CI stages run inside one image built from `docker/android-ci/Dockerfile`
(Ubuntu 24.04, JDK 17, Android SDK + NDK r27 + CMake 3.22.1, host C++ toolchain, aarch64 cross
toolchain with qemu-user, Android emulator with `google_apis` x86_64 system image).

## Why our own image (primary) and not `budtmo/docker-android`

| | Own Dockerfile (primary) | `budtmo/docker-android` |
|-|-|-|
| Reproducibility | every download pinned by version and sha256 | tied to image tags, NDK/CMake not controlled |
| Size | only what CI needs | VNC/noVNC/Appium included |
| Native toolchain | NDK, gtest, cross-gcc, qemu in the same image | has to be layered on top |
| Manual debugging | no UI | convenient noVNC |

`budtmo/docker-android` remains a documented option for interactive debugging:
`docker run -d -p 6080:6080 --device /dev/kvm budtmo/docker-android` and `adb connect`. CI does not use it.

## Running

```bash
export HOST_UID=$(id -u) HOST_GID=$(id -g) KVM_GID=$(stat -c %g /dev/kvm 2>/dev/null || echo 993)
export TEST_VOLUME_PASSWORD='...'                         # fixtures password, from env only
scripts/ci/docker/build-image.sh
docker compose run --rm android-ci scripts/ci/run-local.sh
```

## KVM and the no-KVM fallback

- With KVM: `android-ci` maps `/dev/kvm`; the emulator runs with `-accel on` (boot in about a minute).
- Without KVM use `android-ci-nokvm`: static analysis, JVM tests, native tests and APK build run;
  emulator stages are reported as `SKIP(no-kvm)`, never as `PASS`. Without `ALLOW_NO_KVM_SKIP=1`
  a missing KVM is an error. Software emulation (`-accel off`) works but takes 10+ minutes to boot
  and makes UI tests flaky, so it is not used in CI.

## Scripts

| Script | Purpose |
|--------|---------|
| `scripts/ci/docker/build-image.sh` | build the CI image |
| `scripts/ci/run-local.sh` | all stages, summary in `artifacts/summary.txt`, non-zero exit on any failure |
| `scripts/ci/build-debug-apk.sh` | debug APK and androidTest APK |
| `scripts/ci/teardown.sh` | stop emulator/adb, remove temporary fixtures (never fails) |

Further scripts (`build-ndk.sh`, `run-native-tests.sh`, `start-emulator.sh`,
`run-instrumented-tests.sh`, `run-magisk-tier.sh`) are added together with the epics that need them.

## Artifacts and exit codes

`artifacts/` contains `summary.txt`, one `<stage>.log` per stage, JUnit XML, the debug APK. The fixture
password is masked in all logs. Every stage failure makes `run-local.sh` exit with code 1 after
teardown ran.
