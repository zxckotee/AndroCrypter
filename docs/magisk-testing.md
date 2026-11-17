# Testing with Magisk

HCVault has two product modes. Only the **non-mounted** path is implemented end-to-end today
(SAF → native decrypt → FAT browse in-app). **Mounted mode** (decrypting FUSE → `losetup` → kernel
`mount` via libsu `RootService`) is described in [architecture.md](architecture.md) but not built yet.

This document covers what you can verify on a Magisk phone **now**, and how CI checks root detection.

## What works with Magisk today

| Capability | Status |
| --- | --- |
| Detect Magisk / `su` via libsu (`RootEnvironmentProbe`) | Yes — home screen shows status |
| Grant/deny root in Magisk → updated status after restart | Manual |
| Unlock `.hc` in userspace (no kernel mount) | Yes — same as non-root |
| Expose volume under `/mnt` for other apps | **No** — not implemented |

## Manual test on a rooted device (10 minutes)

1. Build or install the debug APK (`assembleDebug` or CI artifact).
2. Install [Magisk](https://github.com/topjohnwu/Magisk) and ensure **Zygisk** / **su** works
   (`adb shell su -c id` → `uid=0`).
3. Install HCVault, open **Home**:
   - Before grant: expect **Root denied** or **No root**.
   - Magisk prompt → **Grant** → force-stop and reopen → expect **Magisk &lt;version&gt;** and the
     message that kernel mount is still in development.
4. **Open volume** → pick the CI fixture or your `.hc` → enter password → browse root entries
   (userspace path; other apps still do not see a mount).
5. Optional: deny root in Magisk → reopen → status returns to **Root denied**; unlock in-app still
   works without `su`.

### Instrumented test with Magisk

```bash
# Emulator / CI (no root):
adb shell am instrument -w \
  -e class org.hcvault.app.RootProbeInstrumentedTest \
  org.hcvault.app.test/androidx.test.runner.AndroidJUnitRunner

# Physical device with Magisk and su granted to the app:
adb shell am instrument -w \
  -e expect_magisk 1 \
  -e class org.hcvault.app.RootProbeInstrumentedTest \
  org.hcvault.app.test/androidx.test.runner.AndroidJUnitRunner
```

Use the same `-e expect_magisk 1` when running `scripts/ci/run-instrumented-tests.sh` on a rooted
hardware device (not supported in GitHub-hosted CI).

## Docker “magisk tier” (future)

`docker-compose.yml` defines `android-ci-privileged` (`/dev/fuse`, `SYS_ADMIN`) for loop/mount
experiments. The script `scripts/ci/run-magisk-tier.sh` is not wired until the mount daemon exists.

## Next implementation steps (for real Magisk integration)

1. Native/userspace **decrypting FUSE** exporting `plain.img` or block device.
2. **RootService** + mount-master shell: `losetup`, `mount`, teardown on volume close.
3. **Instrumented test** on privileged runner or rooted hardware: assert mountpoint visible in
   `mount` output and file readable from a second UID (where the test harness allows).