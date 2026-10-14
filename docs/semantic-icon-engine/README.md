# Deployment and operations

Run commands from the repository root. Deploy the complete CI-published image; no host Python, icon preparation or local image build is needed. The main application remains independent of the optional icon engine.

## Deploy complete published images (recommended)

After a trusted `master` CI run publishes the images, keep the normal `.env` configuration and run:

```sh
docker compose --profile icons pull
docker compose --profile icons up -d --no-build
```

Every active service in the default Compose file has an image reference and no build definition. The icon image includes the approved SQLite/SVG catalog, pinned model/tokenizer/model card, complete vector index, original notices and `release.json` with per-file SHA-256 checksums. Artifacts live at `/opt/icon-artifacts` inside the image; no host artifact mount is required. Source clones, import/index checkpoints and build tooling are excluded from the release package.

The existing dedicated production stack also supports `--profile icons`:

```sh
docker compose -f docker/docker-compose.prod.yml --profile icons pull
docker compose -f docker/docker-compose.prod.yml --profile icons up -d --no-build
```

Default references use `latest` for convenience. For reproducible deployments, set the **full digest references** from the CI publication summary in `.env`:

```dotenv
# Replace the placeholders with the real published digests.
SEARCH_ENGINE_IMAGE=ghcr.io/hatefsystems/search-engine-core@sha256:CORE_DIGEST
ICON_ENGINE_IMAGE=ghcr.io/hatefsystems/search-engine-core/semantic-icon-engine@sha256:ICON_DIGEST
# Optional semantic inference; lexical remains the default.
ICON_SEMANTIC_ENABLED=true
```

`JS_MINIFIER_IMAGE`, `REDIS_SYNC_IMAGE` and `CRAWLER_SCHEDULER_IMAGE` can also select full image/digest references in the default stack. Preserve the previous references for rollback; restore them and run `pull`/`up --no-build`. Profile data and uploaded media remain in the existing persistent volumes. Do not mount a host folder over `/opt/icon-artifacts`, which would hide the bundled files.

The first icon image is published only after this implementation is merged and its `master` CI completes. PR runs build and test but never publish or save the trusted preparation cache. Allow `GITHUB_TOKEN` to write packages. New GHCR packages can be private: either configure package visibility for anonymous pulls or authenticate the deployment host with `docker login ghcr.io` and an appropriately scoped read-packages credential.

## CI preparation, testing and publication

The main Docker build orchestrator calls `semantic-icon-engine.yml` on the existing CI/CD triggers. It:

1. Runs security/API/vector/package tests and validates production/development Compose configurations.
2. Restores preparation artifacts only for an exact hash of source pins, license policy/notices, normalization, parser/index code and dependency/base-image inputs. Changed inputs generate a new catalog/index. Only successful trusted master runs save this cache; code-only API changes can reuse prepared artifacts.
3. On a cache miss, uses the one-shot admin container to prepare/import/index and verifies an unchanged second index run. No host Python dependencies are installed for this preparation job.
4. Validates full catalog/vector coverage and stages only runtime files. The cache is revalidated before packaging; it is not an authority for licensing or integrity.
5. Builds the `release` target once using a named artifact context. Model, catalog and vector files use separate image layers; source clones and secrets are not supplied to that context.
6. Tests that same image without network or artifact mounts, non-root/read-only, at 1 CPU and 1536 MiB with no swap. Verifies all bundled checksums, 50 queries, long-input recovery and restart. Then starts the same image through production Compose without any build or artifact volume.
7. On trusted master only, tags/pushes the tested local image to GHCR without rebuilding and checks the registry config digest against the tested image ID. Tags include commit, run ID and attempt. `latest` advances only if the publishing commit is still master HEAD. The immutable reference is recorded in `published-image.json` and the workflow summary.

Preparation artifacts and BuildKit layers use separate caches. The runtime needs no external model/icon network access. Deleting the CI preparation cache is safe; the next run regenerates it from pinned inputs. Cache storage can be evicted by GitHub, which also causes a normal regeneration.

## Prepare with Docker Compose for local development

Keep the repository's normal `.env` configuration and run from the repository root:

```sh
docker compose -f docker-compose.yml -f docker-compose.dev.yml --profile icon-setup run --rm --build icon-setup
docker compose -f docker-compose.yml -f docker-compose.dev.yml --profile icons up -d --build semantic-icon-engine
docker compose -f docker-compose.yml -f docker-compose.dev.yml up -d --build search-engine
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
docker compose -f docker-compose.yml -f docker-compose.dev.yml --profile icon-setup run --rm --build icon-setup --skip-index
# Build/resume vectors later, optionally reducing the administrative batch size:
docker compose -f docker-compose.yml -f docker-compose.dev.yml --profile icon-setup run --rm icon-setup --batch 4
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

## Start a local development build

```sh
# Keep the repository's normal .env configuration.
chmod -R a+rX "$ICON_ARTIFACT_DIR"
docker compose --profile icons config -q
docker compose -f docker-compose.yml -f docker-compose.dev.yml --profile icons up -d --build semantic-icon-engine
# Rebuild the main application to install the C++ bridge and editor changes.
docker compose -f docker-compose.yml -f docker-compose.dev.yml up -d --build search-engine
# Optional, after a complete or deliberately partial index is ready:
ICON_SEMANTIC_ENABLED=true docker compose -f docker-compose.yml -f docker-compose.dev.yml --profile icons up -d semantic-icon-engine
```

The service binds only container port 8080 on `search-network`, without a published host port. Defaults: 1 CPU, 1536 MiB, one worker, one inference slot, 64 PIDs, non-root UID/GID 10001, read-only root filesystem, dropped capabilities and a 32 MiB temporary filesystem. `ICON_CPUS`, `ICON_MEMORY`, `ICON_ARTIFACT_DIR`, and `ICON_SEMANTIC_ENABLED` are Compose overrides. These ceilings are not proof of production capacity. Validate them with the intended catalog and request lengths.

With `icons` disabled, the main application still starts. The bridge returns 503 for icon requests. Profile creation, editing and autosave remain available. Existing selections persist, although catalog images cannot load until the service returns. Custom images continue through the existing profile media service. To stop the optional service explicitly: `docker compose --profile icons stop semantic-icon-engine`.

## Transfer a published image to a disconnected host

On a connected machine, pull the chosen digest and export it:

```sh
docker pull "$ICON_ENGINE_IMAGE"
docker tag "$ICON_ENGINE_IMAGE" hatef-icons:offline
docker save hatef-icons:offline -o /tmp/hatef-icons-image.tar
sha256sum /tmp/hatef-icons-image.tar > /tmp/hatef-icons-image.tar.sha256
```

Transfer the image archive and its trusted checksum. On the destination, verify the checksum and load the image, set `ICON_ENGINE_IMAGE=hatef-icons:offline` in `.env`, and run:

```sh
docker load -i /path/to/hatef-icons-image.tar
docker compose --profile icons up -d --no-build --pull never semantic-icon-engine
```

The image contains model, catalog and vectors, so no separate icon bundle restore is needed. The existing `manage.py export/restore` commands remain available for explicit administrative data transfers and development mounts.

## Updates, backup and recovery

1. Back up the entire active artifact root, its outer checksum and the deployed Git/image revision.
2. Prepare changed source pins in an administrative checkout. Recheck source licenses, notices and brand policy. Policy reviews are bound to the Iconify source revision.
3. Ingest into the candidate data directory. Compatible unchanged source rows are copied from the previous snapshot; changed sources are checkpointed again.
4. Compare the new import report, counts, exclusions and representative SVGs. Never overwrite source notices based only on a reported SPDX identifier.
5. Build a new vector directory, optionally reusing the previous one. Run the 50-query evaluation plus a separately collected held-out dataset.
6. Export a new bundle and switch the mounted artifact root in a controlled restart. Keep the previous bundle for rollback.

A model upgrade requires updating its revision **and** reviewed checksums, preparing it, building a separate index and evaluating it. Quantization creates a separate derived manifest with parent checksums and a separate embedding version. Do not mix vector shards between versions.

For a published deployment, rollback means restoring the previous `ICON_ENGINE_IMAGE` digest and recreating the icon service. For a development mount, restore the previous bundle into a new directory, point `ICON_ARTIFACT_DIR` at it and recreate the local service. Profile IDs remain stored; an ID absent from the rollback catalog may be temporarily unavailable. Never delete profile selections during rollback.

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
