FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG CMDLINE_TOOLS_URL=https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
ARG CMDLINE_TOOLS_SHA256=2d2d50857e4eb553af5a6dc3ad507a17adf43d115264b1afc116f95c92e5e258
ARG NDK_VERSION=27.2.12479018
ARG CMAKE_VERSION=3.22.1
ARG PLATFORM=android-36
ARG BUILD_TOOLS=36.0.0
ARG SYSIMG="system-images;android-34;google_apis;x86_64"

ENV ANDROID_HOME=/opt/android-sdk \
    ANDROID_SDK_ROOT=/opt/android-sdk \
    ANDROID_AVD_HOME=/opt/avd \
    JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 \
    HOME=/home/ci \
    PATH=/opt/android-sdk/cmdline-tools/latest/bin:/opt/android-sdk/platform-tools:/opt/android-sdk/emulator:$PATH

RUN apt-get update && apt-get install -y --no-install-recommends \
      openjdk-17-jdk-headless ca-certificates curl unzip git jq xz-utils python3 \
      build-essential cmake ninja-build ccache pkg-config \
      gcc-aarch64-linux-gnu g++-aarch64-linux-gnu qemu-user-static \
      libgtest-dev libgmock-dev \
      dosfstools exfatprogs fuse3 libfuse3-dev cryptsetup-bin \
      libpulse0 libnss3 libgl1 libx11-6 libxkbfile1 libxcomposite1 libxdamage1 \
      libxrandr2 libxi6 libasound2t64 libdbus-1-3 libxcursor1 libxfixes3 libxtst6 \
 && rm -rf /var/lib/apt/lists/*

RUN mkdir -p "$ANDROID_HOME/cmdline-tools" && cd /tmp \
 && curl -fsSL "$CMDLINE_TOOLS_URL" -o cmd.zip \
 && echo "$CMDLINE_TOOLS_SHA256  cmd.zip" | sha256sum -c - \
 && unzip -q cmd.zip -d "$ANDROID_HOME/cmdline-tools" \
 && mv "$ANDROID_HOME/cmdline-tools/cmdline-tools" "$ANDROID_HOME/cmdline-tools/latest" \
 && rm cmd.zip

RUN yes | sdkmanager --licenses >/dev/null \
 && sdkmanager "platform-tools" "emulator" "platforms;${PLATFORM}" "build-tools;${BUILD_TOOLS}" \
      "ndk;${NDK_VERSION}" "cmake;${CMAKE_VERSION}" "${SYSIMG}" \
 && chmod -R a+rwX "$ANDROID_HOME"

# Arbitrary UID support: compose runs the container as the host user.
RUN mkdir -p /home/ci/.gradle /home/ci/.ccache /home/ci/.android /opt/avd /workspace \
 && chmod -R 1777 /home/ci /opt/avd /workspace

WORKDIR /workspace
