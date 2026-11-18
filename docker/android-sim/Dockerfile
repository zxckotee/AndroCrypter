# Privileged Android userland simulator (Stage 1). Shares the host kernel; see docs/android-sim-fidelity.md.
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
# Pinned VeraCrypt CLI from Ubuntu's IdCrypt package repo (override on host via HCVAULT_VERACRYPT).
ARG VERACRYPT_PKG=veracrypt-console=1.26.31-1~ubuntu24.04-1

ENV HCVAULT_SIM_ROOT=/opt/hcvault-sim \
    HCVAULT_APP_PKG=org.hcvault.app \
    HCVAULT_APP_UID=10123 \
    HCVAULT_PROC_FILESYSTEMS=/proc/filesystems \
    HCVAULT_VERACRYPT=veracrypt \
    PATH=/opt/hcvault-sim/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl python3 python3-pil sudo \
      build-essential cmake ninja-build g++ pkg-config jq \
      util-linux mount \
      dosfstools exfatprogs e2fsprogs cryptsetup-bin \
      fuse3 libfuse3-2 \
      "$VERACRYPT_PKG" \
 && rm -rf /var/lib/apt/lists/* \
 && ln -sf /usr/bin/veracrypt /usr/local/bin/veracrypt

RUN mkdir -p "$HCVAULT_SIM_ROOT/bin" /data /data/media/0 /data/local/tmp /mnt /workspace \
 && chmod 1777 /data/local/tmp /workspace \
 && echo 'hcvault_app ALL=(root) NOPASSWD: ALL' > /etc/sudoers.d/hcvault-sim \
 && chmod 0440 /etc/sudoers.d/hcvault-sim

WORKDIR /workspace