# Android simulator: fidelity charter

Stage 1 proves the code against a Docker "Android userland". It cannot be identical to a phone:
the container shares the CI host's Linux kernel. This document says what the simulator
reproduces, what it does not, and which later step closes each gap.

## Reproduced (and asserted by automated tests)

| Aspect | How the simulator reproduces it |
|--------|---------------------------------|
| Directory layout | `/data/data/<pkg>`, `/data/media/0`, `/sdcard` symlink, `/mnt`, `/data/local/tmp` |
| App identity | dedicated non-root UID (10123), no capabilities |
| Per-app mount namespace | `unshare -m` + `mount --make-rslave /` (Android zygote model) |
| Magisk-style root | `su` shim: `-c`, `-M/--mount-master`, grant database (allow/deny by UID); `magisk -v` |
| Real kernel plumbing | real `/dev/fuse`, `/dev/loop-control`, `mount(2)`, ext4/vfat/exfat when the host has them |
| Failure modes | daemon `kill -9`, missing FS driver (capability profile), no free loop device, busy unmount |

## Not reproduced (gap register)

| Gap | Why it matters | Closed by |
|-----|----------------|-----------|
| SELinux (`magisk`/`su` domains, file contexts) | mount and loop access can be denied per ROM | Stage 2 emulator (enforcing), real devices |
| Android kernel config | many kernels lack exfat/ntfs3, limit loop devices | capability profiles (soft), real devices (hard) |
| vold / MediaProvider FUSE for `/sdcard` | decides whether a bind mount is visible to normal apps | Stage 2 emulator, real devices |
| OEM mount namespace tweaks | propagation of mounts to app namespaces differs | real devices |
| Doze / phantom process killer | root daemon lifetime | Stage 2 emulator, real devices |
| Zygisk / DenyList | root hiding can break su detection | real devices |
| Real Magisk su protocol versions | libsu handshake details | Stage 2 emulator with real Magisk |

## Capability profiles

The mount daemon reads the list of kernel filesystems from `HCVAULT_PROC_FILESYSTEMS`
(default `/proc/filesystems`). Tests point it at a fixture file to emulate a kernel without
`exfat`/`ntfs3`, so the "no driver" branches are exercised without another kernel.

## Spike result (E0)

`scripts/sim/spike-capabilities.sh` checks FUSE, loop, `mount(2)`, per-FS kernel support and
namespaces. It is run first in every sim CI job; missing hard prerequisites fail the job.
When a CI host lacks a kernel FS driver (for example vfat/exfat), the dependent tests report
`SKIP(no-kernel-fs)` in `artifacts/summary.txt`, never PASS.

## Docker (E5)

Build and probe the simulator image (`docker/android-sim/Dockerfile` pins `veracrypt-console`;
override the CLI with `HCVAULT_VERACRYPT` on the host if needed):

```bash
export TEST_VOLUME_PASSWORD='...'   # optional, for scripts/sim/generate-fixtures.sh
scripts/ci/docker/build-sim-image.sh
docker compose run --rm android-sim scripts/sim/setup-layout.sh
docker compose run --rm android-sim scripts/sim/spike-capabilities.sh
```

Capability profiles: point `HCVAULT_PROC_FILESYSTEMS` at a fixture such as
`testdata/sim/proc-filesystems.no-exfat` to emulate a kernel without exfat/ntfs3.
