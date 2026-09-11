#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

if [[ -z "${BUILD_BASE_IMAGE:-}" || -z "${RUNTIME_BASE_IMAGE:-}" ]]; then
    python3 scripts/docker_images.py prepare --env-file build/docker-bases.env
    set -a
    # Generated references only; this file is not committed.
    # shellcheck disable=SC1091
    source build/docker-bases.env
    set +a
fi

# Use the active context, just as dependency preparation does.
docker buildx build --platform linux/amd64 --load \
    --progress=plain --file Dockerfile \
    --build-arg "BUILD_BASE_IMAGE=$BUILD_BASE_IMAGE" \
    --build-arg "RUNTIME_BASE_IMAGE=$RUNTIME_BASE_IMAGE" \
    --build-arg "BUILD_JOBS=${BUILD_JOBS:-2}" \
    --tag "${IMAGE_TAG:-searchenginecore:latest}" .
