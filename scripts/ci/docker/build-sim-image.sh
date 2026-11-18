#!/usr/bin/env bash
# Build the Android userland simulator image (privileged mount/FUSE experiments).
source "$(dirname "$0")/../lib.sh"

TAG="${SIM_IMAGE_TAG:-hcvault/android-sim:local}"

docker build \
  --file "$ROOT/docker/android-sim/Dockerfile" \
  --tag "$TAG" \
  ${CACHE_FROM:+--cache-from "$CACHE_FROM"} \
  "$ROOT" 2>&1 | tee "$ART/build-sim-image.log"

docker image inspect "$TAG" --format 'image={{.Id}} size={{.Size}}' | tee -a "$ART/build-sim-image.log"