# Stage 2: emulator with rootAVD and Magisk

Stage 1 ([android-sim-fidelity.md](android-sim-fidelity.md)) validates mount plumbing in a
**privileged Docker userland** with Magisk stubs. Stage 2 runs the same contracts on a **real
Android system image** with **Magisk via [rootAVD](https://github.com/newbit1/rootAVD)** (or an
equivalent patched AVD), so SELinux, vold, and the real `su` protocol participate.

## Goals

| Goal | Stage 1 | Stage 2 |
| --- | --- | --- |
| `mount(2)` + loop + FUSE on decrypted image | Docker sim | Emulator + root |
| libsu / Magisk detection | Stub `su` | Real Magisk |
| `hcvault-mountd` on device | Host binary in sim only | Push via RootService (planned) |
| Instrumented mount visibility | N/A | Second UID / `run-as` checks |

## rootAVD / Magisk tier setup (manual)

These steps are **not** run in default GitHub CI until the magisk tier is fully wired.

1. Build the CI image (`scripts/ci/docker/build-image.sh`) and start **`android-ci-privileged`**
   (`docker-compose.yml` sets `RUN_MAGISK_TIER=1`).
2. Create or reuse the CI AVD (`scripts/ci/start-emulator.sh`, `HCVAULT_AVD=hcvault_ci`).
3. Patch the AVD with rootAVD (host-side, once per system image revision):
   - Clone rootAVD and follow its README for your API level (e.g. API 34 `google_apis` x86_64).
   - Reboot the emulator; verify `adb shell su -c id` → `uid=0(root)`.
   - Grant **HCVault** root in Magisk when prompted.
4. Set `HCVAULT_SIM_EXECUTOR=adb` when running sim-style helpers so `exec_adb` uses real `adb shell`
   (see `scripts/sim/lib/executor.sh`).

Mounted-mode end-to-end tests remain **skipped** until `MountedModeSupport.IMPLEMENTED` and
`hcvault-mountd` ship in the app; see [magisk-testing.md](magisk-testing.md).

## CI: `run-magisk-tier.sh`

```bash
# After emulator boot, inside android-ci-privileged:
export RUN_MAGISK_TIER=1
scripts/ci/run-magisk-tier.sh
```

The script sources `exec_adb` (adb when a device is online, otherwise Stage 1 `exec_local` stub) and
reports **`SKIP(...)`** with a specific reason—not `PASS`—when rootAVD/Magisk or mount wiring is
missing. Summary lines land in `artifacts/magisk-tier-summary.txt`.

Optional escape hatch for local sim-only experiments (same as sim tier):

```bash
ALLOW_MAGISK_SKIP=1 RUN_MAGISK_TIER=1 scripts/ci/run-magisk-tier.sh
```

## Gap register mapping (from Stage 1 charter)

Copied from [android-sim-fidelity.md](android-sim-fidelity.md) **Not reproduced** table, with Stage 2
coverage and what remains for physical devices.

| Gap (Stage 1) | Why it matters | Stage 2 emulator | Still needs real device |
| --- | --- | --- | --- |
| SELinux (`magisk`/`su` domains, file contexts) | loop/mount denied per ROM | **Partial** — enforcing image + Magisk policies | OEM-specific rules |
| Android kernel config (exfat/ntfs3, loop limits) | driver missing → mount fails | **Partial** — AVD kernel config is fixed per image | OEM kernels |
| vold / MediaProvider FUSE for `/sdcard` | bind visibility to other apps | **Partial** — real vold stack | OEM storage paths |
| OEM mount namespace tweaks | propagation differs from AOSP | **Low** — AOSP emulator behavior | **Yes** |
| Doze / phantom process killer | root daemon lifetime | **Partial** — emulator power model simpler | **Yes** |
| Zygisk / DenyList | root hiding breaks detection | Magisk toggle testing | **Yes** (DenyList combos) |
| Real Magisk `su` protocol versions | libsu handshake | **Yes** — rootAVD Magisk | Magisk version matrix |

**Reproduced in Stage 1 and re-checked in Stage 2:** directory layout, per-app mount namespace
(`unshare -m`), real `/dev/fuse` and loop on hardware-backed emulator, capability profiles via
`HCVAULT_PROC_FILESYSTEMS`.

## Relationship to Docker services

| Compose service | Tier |
| --- | --- |
| `android-sim` | Stage 1 sim acceptance (`run-sim-tests.sh`) |
| `android-ci` / `android-ci-nokvm` | JVM, native, stock emulator (no root) |
| `android-ci-privileged` | KVM + fuse; invokes `run-magisk-tier.sh` when `RUN_MAGISK_TIER=1` |

See [testing-docker.md](testing-docker.md) for KVM and artifact conventions.