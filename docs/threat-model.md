# Threat model: mounted mode and mount daemon

This document covers security properties for **kernel-mounted VeraCrypt volumes** via
`hcvault-mountd`, the libsu `RootService` bridge (planned), and the Stage 1 Docker simulator.
Userspace-only unlock (no `su`) is out of scope here; see [architecture.md](architecture.md).

## Assets and trust boundaries

| Asset | Owner | Notes |
| --- | --- | --- |
| Volume password / PIM | App process (`SecretChars`) | Must never hit disk, logcat, or shell argv |
| Decrypted plaintext (`plain.img` via FUSE) | Mount daemon (root) | Equivalent to a mounted VeraCrypt volume |
| Kernel mount at `/mnt/...` | Kernel + root daemon | Visible to other UIDs on the device unless bind-isolated |
| Registry (`mount-registry.jsonl`) | Root daemon | Tracks loop device, FUSE path, mountpoint for teardown |

```text
  [App UID] --AIDL/Binder--> [RootService / hcvault-mountd (root)]
        |                              |
        |  password via pipe/fd only     | FUSE + losetup + mount(2)
        v                              v
  [SecretChars wiped]            [Kernel VFS mount]
```

The app is **untrusted** relative to the daemon: any caller with root grant could invoke `su`.
HCVault assumes the user explicitly grants Magisk root to the app; the daemon must not widen
that trust to unrelated UIDs without Android’s normal mount visibility rules.

## Mount daemon (`hcvault-mountd`)

**Entry points** (native): `mount`, `umount`, `cleanup`, `serve` (Unix socket control).

| Threat | Mitigation (current / planned) |
| --- | --- |
| Password on command line or in environment | **Password only on `--password-fd`** (default stdin); documented in `--help`; Kotlin must use a pipe or `ParcelFileDescriptor`, never string concat in `sh -c` |
| Password left in heap after unlock | `wipe_password()` after `vc_open` in `cmd_mount` |
| Daemon keeps volume open after app crash | Registry records fuse mount, loop, and kernel mount; **`cleanup` / `CLEANUP` on control socket** replays teardown |
| Stale mounts after `kill -9` on daemon child | **Append-only registry** at `HCVAULT_MOUNT_REGISTRY` or `$XDG_RUNTIME_DIR/hcvault-mountd-registry.jsonl` (mode 0700 parent); reboot and `cleanup` recover |
| Arbitrary local users invoking mount | Production: **RootService + AIDL** with app UID binding; socket `serve --peer-uid` uses `SO_PEERCRED` (accepts peer or uid 0 only) |
| Mount daemon replaced by malware | Magisk grant is user-controlled; ship **`hcvault-mountd` in app APK** and verify path/hash before exec (follow-up) |
| FUSE mount under `/tmp` | Acceptable on sim; on-device path must be **root-only** (`0700`) and unlinked on umount |

**Residual:** A malicious app with root can still read mounted filesystem content the kernel exposes.
Mounted mode is for user convenience (other apps see files), not DRM.

## Password file descriptor

Design rule: **one consumer, one read, then close.**

1. App opens `SecretChars` → writes UTF-8 bytes to a **pipe** (or `memfd` on supported builds).
2. Child `hcvault-mountd mount ... --password-fd N` inherits only that fd; parent closes its write end after spawn.
3. Daemon reads bounded length (cap 4096), strips newlines, passes to `vc_open`, then wipes.

| Threat | Mitigation |
| --- | --- |
| Password visible in `/proc/<pid>/cmdline` | No password in argv |
| Password in `ps` / crash dumps | Avoid `export`; fd is binary pipe, not a named file in world-readable `/tmp` |
| Second process reading the same fd | `O_CLOEXEC` on unrelated fds; pipe is single-consumer |
| Log leakage | CI `mask()` redacts `TEST_VOLUME_PASSWORD`; daemon never prints password |

**Do not** pass passwords through `adb shell` arguments, Binder `String`, or registry JSON.

## Registry cleanup

Each successful mount appends one JSON line: `id`, `volume`, `mountpoint`, `fuse_mount`, `loop_dev`, `ts`.

| Threat | Mitigation |
| --- | --- |
| Cleanup skipped on crash | `hcvault-mountd cleanup` and socket command `CLEANUP` call `hm_registry_cleanup_all()` |
| Partial teardown (busy mount) | `teardown_entry` umounts (lazy), detaches loop, `fusermount3 -u` |
| Registry growth / forged lines | Lines are **daemon-written only**; malformed lines ignored on parse; file deleted after successful cleanup pass |
| World-readable registry leaks paths | Default under **`~/.cache/hcvault/` (0700)** or `XDG_RUNTIME_DIR`; override with `HCVAULT_MOUNT_REGISTRY` only on dev hosts |
| `hm_registry_remove` not implemented | Umount path must still call cleanup or targeted teardown (follow-up per-mount `remove`) |

On app upgrade or Magisk deny, operators should run **`cleanup`** once to drop orphaned loop/FUSE/mount triples.

## Stage 1 simulator (Docker)

The privileged **android-sim** image runs `hcvault-mountd` as root with Magisk **stubs**, not a real
SELinux domain. Threats around multi-user isolation and `su` policy are **not representative** of
production; see [android-sim-fidelity.md](android-sim-fidelity.md) and [stage2-emulator.md](stage2-emulator.md).

## Hardening checklist (before shipping mounted mode)

- [ ] Wire `RootService` → AIDL; no shell passwords
- [ ] Pin daemon binary path from app package native lib or dedicated extract dir
- [ ] Run instrumented test: mount → kill daemon → `cleanup` → assert no `loop`/`mount` leaks
- [ ] ProGuard/R8 keep rules for AIDL and JNI (see `core-mount` / `core-crypto` `consumer-rules.pro`)
- [ ] Document user-visible risk: mounted volume is **as exposed as any USB disk mount**