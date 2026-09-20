# Core Docker builds

The core is built on GitHub-hosted `ubuntu-latest` runners for `linux/amd64`.
All base images and registry layer caches live under `ghcr.io/<owner>/<repo>`.
Application behavior, Debug compilation, and test compilation are unchanged.
CI runs the slug, public profile, editor, profile validator, and SEO generator
tests and checks the runtime's dynamic libraries. Database integration services
are not started during the image build.

## Images and versioning

| Image                 | Contents                                                       | Rebuilt when                        |
| --------------------- | -------------------------------------------------------------- | ----------------------------------- |
| `mongodb-drivers`     | MongoDB C 2.1.1 and C++ 4.1.2 drivers                          | MongoDB inputs or Ubuntu change     |
| `build-deps`          | Drivers, CMake 3.31.8, compilers and project/test dependencies | Build inputs or its parent change   |
| `runtime-base`        | Ubuntu runtime packages and shared libraries from build-deps   | Runtime inputs or its parent change |
| Repository root image | Server, public files, locales, templates and start script      | Application inputs change           |

Base tags are `deps-<sha256>` hashes of the relevant Dockerfile, locked versions,
platform, parent identity, build orchestration script, and `cache_version`.
Source, tests and web assets are excluded. CI checks GHCR before building a base;
missing tags are built, while authentication and network errors stop the job.
Published parents are consumed by digest, never by `latest`.

The server retains its existing repository path. A successful trusted run
publishes the full commit SHA tag; `latest` moves only if that commit is still
the head of `master`. PRs build and test without logging in or publishing.
When a PR needs an unpublished base, it builds and loads it in the same Docker
daemon before building its children. Sidecar publication jobs do not run on PRs.

The core's publisher jobs are serialized, covering shared parents even when
two runs have different child hashes. Sidecar builds remain independent.
GitHub concurrency can replace an older pending run with a newer one; it does
not interrupt the active publisher.

## First publication and public access

1. Enable GitHub Actions and allow the workflow's `GITHUB_TOKEN` to write packages.
   Organization policy must also permit package creation.
2. Run the existing CI/CD workflow on `master`. It creates all three bases with
   repository source labels, using `GITHUB_TOKEN`; a personal token is not needed.
3. For **each** new base package, open its GitHub **Package settings → Change
   visibility → Public**. A public repository does not automatically make a
   newly published package public. GitHub does not provide a supported REST
   operation for changing package visibility.
4. Rerun the workflow if its anonymous-pull check stopped at the visibility gate.
   Already-published bases are reused. Confirm the server package is public too
   if it is newly created rather than the existing package.

The visibility gate intentionally uses an empty Docker credential configuration.
It also fails on connectivity errors: check its logs before changing settings.
Fork validation becomes available once the public bases are bootstrapped; a fork
can also build missing bases locally. Do not use `pull_request_target` to run
untrusted PR code with publication credentials.

References: [GHCR access and visibility](https://docs.github.com/en/packages/learn-github-packages/configuring-a-packages-access-control-and-visibility),
[Docker registry cache](https://docs.docker.com/build/ci/github-actions/cache/).

## Local builds

For normal development, run:

```bash
docker compose up
```

The search-engine service builds the application using the published `build-deps`
and `runtime-base` defaults in `Dockerfile`. Both are pinned to matching GHCR
digests, verified against the dependency tags from the current lock. Missing base
images are downloaded; dependency Dockerfiles are not built locally. No base-image
variables, generated env file, or `prepare` command are needed. The application's
existing `.env` is still used for its runtime configuration.

`pull_policy: build` rechecks the application build on every `up`, including when
an application image already exists. Docker reuses unchanged layers; changing only
web assets does not recompile C++. The core targets `linux/amd64`, matching the
published bases. Other Compose services retain their existing build behavior.

`bash scripts/build-docker.sh` builds just the core as `searchenginecore:latest`
with the same Dockerfile defaults. Set `BUILD_JOBS` to adjust compilation parallelism
(default 2), or `IMAGE_TAG` to change this helper's local output tag. Direct
`docker build .` also uses the published bases.

For an explicitly selected pair, optionally set nonempty `BUILD_BASE_IMAGE` and
`RUNTIME_BASE_IMAGE` in the environment (or `.env` for Compose). CI continues to
supply its own resolved digests, so these development defaults do not change CI's
dependency selection. Keep both Dockerfile defaults together when updating them:
first publish a matching pair through CI, then replace their two digest references.
They intentionally do not follow mutable `latest` tags.

### Explicit dependency maintenance

Developers changing the dependency definitions can still opt into building bases:

```bash
python3 scripts/docker_images.py prepare --env-file build/docker-bases.env
set -a
source build/docker-bases.env
set +a
docker compose up
```

Only this explicit maintenance path requires Python 3 and an active Buildx builder
using the Docker driver, so locally loaded parents and their children use the same
Docker daemon. It inherits the active context, including the named context selected
by `setup-docker-action` in CI. CI enables the containerd image store for registry
cache export; local builds do not export registry caches.

Use `--repository owner/repo` with `prepare` for another repository, or `--offline`
to skip GHCR lookups and build all bases locally. The latter still downloads Ubuntu,
packages and source dependencies; it is not a network-disconnected build. The
previous single `BASE_IMAGE` argument is no longer supported.

## Updates, cache and validation

- Edit `docker/dependencies.lock.json` for dependency updates. Keep the uSockets
  commit equal to the submodule of the selected uWebSockets commit. Catch2 is
  locked to the v3.4.0 commit. Previously unpinned libraries are fixed to commits.
- Ubuntu is pinned by digest; refresh it explicitly for OS updates. Apt packages
  come from Ubuntu's repositories at build time, so this is versioned image
  reuse, not a claim of bit-for-bit reproducibility from package sources.
- Bump manual `cache_version` to create a new base generation, including updated
  apt packages. Set `force_rebuild` to bypass lookup and layer cache and include
  the run ID/attempt in new base identities. Existing base tags are not replaced.
- Registry caches have separate `buildcache-<image>:cache` paths and use `mode=max`.
  Missing caches or failed optional cache exports do not fail the image build.
  Keep base versions required for rollback; cache entries can be removed without
  removing images. No automatic deletion policy is installed by this change.
- BuildKit layer caching is retained. A source change still recompiles the C++
  project; persistent ccache/object caching is outside this change. Asset-only
  changes occur after compilation and preserve the compile layers. Template
  changes rerun the profile rendering tests; other asset changes preserve the
  test layers.
- Workflow summaries record each base's preparation, core compile/test/validation,
  and publication duration. Compare a bootstrap run with a source-only run and
  an asset-only run. Source-only runs must reuse all three bases; asset-only runs
  must also reuse compile layers. Template edits rerun rendering tests. No unmeasured speedup percentage is claimed.

Run `python3 -m unittest discover -s tests/ci -v` for hash propagation, missing
manifest vs. registry failure handling, local PR bootstrap and publication gates.
Full acceptance also requires a hosted workflow run, anonymous base pulls,
successful CTest output and the runtime dependency check. To roll back, pull a
previous full-SHA server tag or its recorded digest; deployment is not automated.

Changing the original MongoDB Dockerfile to require locked build arguments also
retires the old standalone driver workflow. The core workflow now owns the entire
base chain. Other service Dockerfiles and production deployment addresses remain
unchanged.
