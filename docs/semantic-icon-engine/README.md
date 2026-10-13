# Deployment and operations

Run commands from the repository root. Docker Compose setup is recommended; host Python is an alternative. The main application remains independent of the optional icon engine.

## Prepare with Docker Compose (recommended)

Keep the repository's normal `.env` configuration and run from the repository root:

```sh
docker compose --profile icon-setup run --rm --build icon-setup
docker compose --profile icons up -d --build semantic-icon-engine
docker compose up -d --build search-engine
```

Wait for the first command to exit successfully before starting the runtime. It runs a temporary administrative container using the pinned Python 3.12 image, hashed Python packages and Git. No host Python installation, virtual environment or compiler is needed. The `admin` Dockerfile target contains Git; the default `runtime` target remains non-root and starts only the API.

Setup performs `prepare`, `ingest` and complete `index` in order. It verifies pinned revisions and model checksums, saves `import-report.json` and reports approved/indexed counts. The artifact mount is writable only in the setup container; the runtime mount remains read-only. Source clones and downloaded files are retained on the host:

| Setting | Default |
| --- | --- |
| `ICON_ARTIFACT_DIR` | `./services/semantic-icon-engine/data/offline` |
| `ICON_UPSTREAM_DIR` | `./services/semantic-icon-engine/data/upstream` |
| `ICON_SETUP_CPUS` | `1.0` |
| `ICON_SETUP_MEMORY` | `2048m` |

Setup runs as root inside its temporary container so Docker-created bind directories are writable. New files use a 022 umask, making artifacts readable by runtime UID 10001. If switching to a host UID, create both mount directories first and use `run --user "$(id -u):$(id -g)"`; the UID must own any retained source clones and writable artifacts.

The initial run needs network access and disk space for source clones, the roughly 470 MB model and catalog/vectors. Explicit reruns recheck upstream pins, reuse downloaded model files and resume already committed vector shards. Concurrent setup commands against the same artifact directory are rejected. Interrupted jobs can be rerun. When updating catalog/model pins, use a new artifact directory as described below; incompatible old vectors are never silently overwritten.

To skip vector construction while keeping lexical search available:

```sh
docker compose --profile icon-setup run --rm --build icon-setup --skip-index
# Build/resume vectors later, optionally reducing the administrative batch size:
docker compose --profile icon-setup run --rm icon-setup --batch 4
```

`--skip-index` still prepares the pinned model. Semantic mode remains off by default. After successful indexing, put `ICON_SEMANTIC_ENABLED=true` in `.env` and recreate `semantic-icon-engine` with the `icons` profile to enable it. Normal `docker compose up` does not enable either optional profile, and runtime startup never prepares artifacts.

The setup container forwards `HTTP_PROXY`, `HTTPS_PROXY` and `NO_PROXY` from the shell or `.env`. Use a proxy address reachable from the container; `127.0.0.1` inside a container refers to that container. Docker image/package build downloads use Docker's own proxy configuration. Proxy credentials must remain outside Git.

## Prepare online with host Python (alternative)

```sh
python3.12 -m venv .venv-icons
.venv-icons/bin/pip install --require-hashes -r services/semantic-icon-engine/requirements.txt
export ICON_ARTIFACT_DIR="$PWD/services/semantic-icon-engine/data/offline"
mkdir -p "$ICON_ARTIFACT_DIR"
.venv-icons/bin/python services/semantic-icon-engine/scripts/manage.py prepare \
  --upstream "$PWD/services/semantic-icon-engine/data/upstream" \
  --models "$ICON_ARTIFACT_DIR/models"
.venv-icons/bin/python services/semantic-icon-engine/scripts/manage.py ingest \
  --upstream "$PWD/services/semantic-icon-engine/data/upstream" \
  --data "$ICON_ARTIFACT_DIR/data"
```

`prepare` explicitly fetches the source commit IDs in `config/sources.lock.json` and downloads the pinned ONNX model, tokenizer and model card. Model SHA-256 checksums are verified. Git object identities and tracked-file cleanliness are checked at import. Neither command runs at API startup. Retain the full model card and source notices with the artifacts.

Default policy discovers every source pack but publishes only reviewed data. See [security.md](security.md) before expanding that policy. A complete discovery report does **not** mean every upstream icon was approved. Import output includes counts, exclusion reasons, source revisions, snapshot version and database bytes. SQLite stores SVG blobs once per hash and retains each canonical ID and alias independently.

## Generate vectors explicitly

All approved icons are searchable immediately after ingestion. Index priority categories first if needed:

```sh
.venv-icons/bin/python services/semantic-icon-engine/scripts/manage.py index \
  --data "$ICON_ARTIFACT_DIR/data" --models "$ICON_ARTIFACT_DIR/models" \
  --vectors "$ICON_ARTIFACT_DIR/vectors" --priority healthcare --max-icons 512 --batch 8
# Resume the same snapshot and finish the remaining icons.
.venv-icons/bin/python services/semantic-icon-engine/scripts/manage.py index \
  --data "$ICON_ARTIFACT_DIR/data" --models "$ICON_ARTIFACT_DIR/models" \
  --vectors "$ICON_ARTIFACT_DIR/vectors" --batch 8
```

Each 512-row shard is written before its atomic manifest entry. An interrupted run replays the manifest, ignores unpublished files and skips committed IDs. Partial indexes work, with `semanticCoverage` in suggestion responses. Readers pin a manifest at startup; restart the service to adopt newly published shards. Batch size is constrained to 1–32, default 8. Use a separate administrative job budget; do not index on the shared application server during busy periods.

After a catalog update, use a **new vector directory**. `--reuse-vectors /path/to/previous/vectors` reuses rows with the same model version, icon ID and searchable-text hash. Other rows are encoded anew. Model/normalization/dictionary changes reject incompatible reuse. Never edit snapshot files in place.

## Start through the existing Compose project

```sh
# Keep the repository's normal .env configuration.
chmod -R a+rX "$ICON_ARTIFACT_DIR"
docker compose --profile icons config -q
docker compose --profile icons up -d --build semantic-icon-engine
# Rebuild the main application to install the C++ bridge and editor changes.
docker compose up -d --build search-engine
# Optional, after a complete or deliberately partial index is ready:
ICON_SEMANTIC_ENABLED=true docker compose --profile icons up -d semantic-icon-engine
```

The service binds only container port 8080 on `search-network`, without a published host port. Defaults: 1 CPU, 1536 MiB, one worker, one inference slot, 64 PIDs, non-root UID/GID 10001, read-only root filesystem, dropped capabilities and a 32 MiB temporary filesystem. `ICON_CPUS`, `ICON_MEMORY`, `ICON_ARTIFACT_DIR`, and `ICON_SEMANTIC_ENABLED` are Compose overrides. These ceilings are not proof of production capacity. Validate them with the intended catalog and request lengths.

With `icons` disabled, the main application still starts. The bridge returns 503 for icon requests. Profile creation, editing and autosave remain available. Existing selections persist, although catalog images cannot load until the service returns. Custom images continue through the existing profile media service. To stop the optional service explicitly: `docker compose --profile icons stop semantic-icon-engine`.

## Package for a disconnected host

```sh
.venv-icons/bin/python services/semantic-icon-engine/scripts/manage.py export \
  "$ICON_ARTIFACT_DIR" /tmp/hatef-icons-offline.tgz
docker compose --profile icons build semantic-icon-engine
docker image save "$(docker compose --profile icons images -q semantic-icon-engine)" \
  -o /tmp/hatef-icons-image.tar
(cd /tmp && sha256sum hatef-icons-offline.tgz hatef-icons-image.tar > hatef-icons-SHA256SUMS)
```

Transfer the two archives, checksums and this pinned repository revision through your artifact distribution process. The artifact archive contains catalog, source notices, model, tokenizer, model card and vectors. The image archive supplies all runtime dependencies; no package installation is required on the disconnected host. Keep the checksums through a trusted channel; an archive's internal checksums detect corruption, not malicious replacement of the entire archive.

On the destination, verify the outer checksums, then restore into a **new** directory:

```sh
cd /path/to/transfers
sha256sum -c hatef-icons-SHA256SUMS
docker load -i /path/to/hatef-icons-image.tar
# Run manage.py in an existing administrative environment, or in the loaded image:
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v /path/to/transfers:/transfer:ro -v /srv/hatef:/destination \
  --entrypoint python LOADED_IMAGE \
  scripts/manage.py restore /transfer/hatef-icons-offline.tgz /destination/icons
export ICON_ARTIFACT_DIR=/srv/hatef/icons
# Use the matching loaded image name/tag; do not request a disconnected build.
docker compose --profile icons up -d --no-build --pull never semantic-icon-engine
```

Preserve the Compose project/image name across hosts or tag the loaded image to the name shown by `docker compose --profile icons config --images`. Bundle restore rejects absolute/traversal paths, links, duplicate archive entries, checksum mismatches and excessive expanded bytes (20 GiB default, configurable). It stages extraction before activation. It refuses an existing destination.

## Updates, backup and recovery

1. Back up the entire active artifact root, its outer checksum and the deployed Git/image revision.
2. Prepare changed source pins in an administrative checkout. Recheck source licenses, notices and brand policy. Policy reviews are bound to the Iconify source revision.
3. Ingest into the candidate data directory. Compatible unchanged source rows are copied from the previous snapshot; changed sources are checkpointed again.
4. Compare the new import report, counts, exclusions and representative SVGs. Never overwrite source notices based only on a reported SPDX identifier.
5. Build a new vector directory, optionally reusing the previous one. Run the 50-query evaluation plus a separately collected held-out dataset.
6. Export a new bundle and switch the mounted artifact root in a controlled restart. Keep the previous bundle for rollback.

A model upgrade requires updating its revision **and** reviewed checksums, preparing it, building a separate index and evaluating it. Quantization creates a separate derived manifest with parent checksums and a separate embedding version. Do not mix vector shards between versions.

Rollback means restoring the previous bundle into a new directory, pointing `ICON_ARTIFACT_DIR` at it and restarting only the icon service. Profile IDs remain stored; an ID absent from the rollback catalog may be temporarily unavailable. Never delete profile selections during rollback.

## Troubleshooting

| Symptom                    | Check/action                                                                                                                                                        |
| -------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Readiness 503              | Check `current.json`, file permissions, SQLite checksum and nonzero approved icon count. Reimport or restore a valid snapshot.                                      |
| Ready in lexical mode      | Semantic mode is disabled, or model/vector files are missing, corrupted or incompatible. Inspect the single startup warning; prepare a matching bundle and restart. |
| 429                        | Per-client token bucket or inference/bridge concurrency is full. Honor `Retry-After`; reduce request bursts.                                                        |
| Timeout fallback           | Two-second inference deadline elapsed. The running inference keeps its slot until it exits; new inference work is rejected instead of accumulating.                 |
| Icons missing              | Review license/brand exclusions and rejected SVG reasons. Check canonical ID and source revision.                                                                   |
| Import/index lock error    | Another administrative writer is active. Do not remove locks to run concurrent writers.                                                                             |
| Wrong catalog after update | Readers pin snapshots. Restart after activating the new complete bundle.                                                                                            |
| Memory pressure            | Keep lexical mode active. Measure realistic request lengths and concurrency before enabling semantic mode or increasing its budget.                                 |

`GET /health/live` tests process liveness. `GET /health/ready` reports catalog version, icon count, model status and runtime mode. `/metrics` exposes suggestion count, total successful suggestion seconds and inference timeout count on the private service. Query text is not logged by the Python service or C++ icon-route tracing. There is no public administrative endpoint.
