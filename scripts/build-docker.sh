#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# Published defaults live in Dockerfile. Only pass explicit, nonempty overrides;
# ordinary local builds never build dependency images.
build_args=()
for variable in BUILD_BASE_IMAGE RUNTIME_BASE_IMAGE; do
    if [[ -n "${!variable:-}" ]]; then
        build_args+=(--build-arg "$variable=${!variable}")
    fi
done

# Use the active context, just as dependency preparation does.
docker buildx build --platform linux/amd64 --load \
    --progress=plain --file Dockerfile \
    "${build_args[@]}" \
    --build-arg "BUILD_JOBS=${BUILD_JOBS:-2}" \
    --tag "${IMAGE_TAG:-searchenginecore:latest}" .
