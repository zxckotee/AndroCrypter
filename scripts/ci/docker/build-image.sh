#!/usr/bin/env bash
# Build the CI image (Ubuntu 24.04 + JDK 17 + Android SDK/NDK + host toolchain).
source "$(dirname "$0")/../lib.sh"

TAG="${IMAGE_TAG:-hcvault/android-ci:local}"

docker build \
  --file "$ROOT/docker/android-ci/Dockerfile" \
  --tag "$TAG" \
  ${CACHE_FROM:+--cache-from "$CACHE_FROM"} \
  --build-arg NDK_VERSION="$NDK_VERSION" \
  --build-arg CMAKE_VERSION="$CMAKE_VERSION" \
  "$ROOT" 2>&1 | tee "$ART/build-image.log"

docker image inspect "$TAG" --format 'image={{.Id}} size={{.Size}}' | tee -a "$ART/build-image.log"
