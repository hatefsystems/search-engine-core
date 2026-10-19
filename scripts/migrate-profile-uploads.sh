#!/usr/bin/env bash
# Copy uploads from a stopped legacy application container before mounting a volume.
set -euo pipefail
if [[ $# -ne 2 ]]; then
    echo "Usage: $0 STOPPED_CONTAINER UPLOADS_VOLUME" >&2
    exit 2
fi
profile_container=$1
profile_uploads_volume=$2
if [[ $(docker inspect --format '{{.State.Running}}' "$profile_container") != false ]]; then
    echo 'Stop the application container first so no uploads can arrive during migration.' >&2
    exit 1
fi
profile_backup_dir=$(mktemp -d)
trap 'rm -rf "$profile_backup_dir"' EXIT
mkdir "$profile_backup_dir/files"
if ! docker cp "$profile_container:/app/uploads/." "$profile_backup_dir/files" 2>"$profile_backup_dir/error"; then
    if [[ $(cat "$profile_backup_dir/error") == *"Could not find the file /app/uploads"* ]]; then
        echo 'No legacy /app/uploads directory exists; missing files require backup recovery or re-upload.'
        exit 0
    fi
    cat "$profile_backup_dir/error" >&2
    exit 1
fi
docker volume create "$profile_uploads_volume" > /dev/null
profile_image=$(docker inspect --format '{{.Image}}' "$profile_container")
docker run --rm --network none --entrypoint /bin/sh \
    --mount "type=bind,src=$profile_backup_dir/files,dst=/source,readonly" \
    --mount "type=volume,src=$profile_uploads_volume,dst=/destination" \
    "$profile_image" -ec 'cp -an /source/. /destination/'
echo "Copied existing uploads to $profile_uploads_volume; existing destination files were preserved."
