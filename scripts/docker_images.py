#!/usr/bin/env python3
"""Build the core's dependency images locally or publish them from trusted CI."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
from contextlib import contextmanager

ROOT = Path(__file__).resolve().parents[1]
BASE_FILES = {
    "mongodb-drivers": "docker/Dockerfile.mongodb",
    "build-deps": "docker/Dockerfile.build-deps",
    "runtime-base": "docker/Dockerfile.runtime-base",
}


def run(args, *, capture=False, check=True):
    return subprocess.run(args, cwd=ROOT, text=True, check=check,
                          stdout=subprocess.PIPE if capture else None,
                          stderr=subprocess.PIPE if capture else None)


def summary(message):
    print(message, flush=True)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as stream:
            stream.write(message + "\n\n")


@contextmanager
def timed(name):
    started = time.monotonic()
    try:
        yield
    finally:
        summary(f"{name}: {time.monotonic() - started:.1f}s")


def locked_versions():
    return json.loads((ROOT / "docker/dependencies.lock.json").read_text())


def image_tags(repository, cache_version="1", nonce="", root=ROOT):
    """Only dependency inputs participate; downstream hashes include parents."""
    lock = json.loads((root / "docker/dependencies.lock.json").read_text())
    common = {"ubuntu": lock["ubuntu_image"], "platform": lock["platform"],
              "cache_version": cache_version, "force_nonce": nonce,
              "builder": hashlib.sha256((root / "scripts/docker_images.py").read_bytes()).hexdigest()}
    tags = {}
    parent = ""
    for name, filename in BASE_FILES.items():
        versions = lock["mongodb"] if name == "mongodb-drivers" else (
            lock["build"] if name == "build-deps" else {})
        payload = json.dumps({"common": common, "versions": versions,
                              "parent": parent}, sort_keys=True).encode()
        digest = hashlib.sha256(payload + (root / filename).read_bytes()).hexdigest()
        tags[name] = f"ghcr.io/{repository.lower()}/{name}:deps-{digest}"
        parent = digest
    return tags


def inspect_image(ref):
    result = run(["docker", "buildx", "imagetools", "inspect", ref,
                  "--format", "{{json .Manifest}}"], capture=True, check=False)
    if result.returncode:
        error = result.stderr.strip()
        # Do not treat authorization, DNS, rate limiting or transport errors as
        # cache misses. Buildx's exact registry-not-found message is permitted.
        if error == f"ERROR: {ref}: not found" or "manifest unknown" in error.lower():
            return None
        raise RuntimeError(f"Cannot inspect {ref}: {error}")
    digest = json.loads(result.stdout)["digest"]
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", digest):
        raise RuntimeError(f"Invalid manifest digest for {ref}")
    return ref.rsplit(":", 1)[0] + "@" + digest


def can_publish():
    return (os.environ.get("GITHUB_EVENT_NAME") in {"push", "workflow_dispatch"}
            and os.environ.get("GITHUB_REF") == "refs/heads/master")


def require_publish_context():
    if not can_publish():
        raise RuntimeError("Publishing is limited to push/manual runs on master.")


def build_image(filename, tag, build_args, *, push=False, force=False, cache=None):
    # setup-docker-action can select a named context. Keep its Docker driver
    # and daemon instead of forcing the builder attached to the default context.
    command = ["docker", "buildx", "build",
               "--platform", locked_versions()["platform"], "--progress", "plain",
               "--provenance=false", "--file", filename, "--tag", tag]
    command += ["--push"] if push else ["--load"]
    if force:
        command += ["--no-cache"]
    elif cache:
        command += ["--cache-from", f"type=registry,ref={cache}"]
    if push and cache:
        command += ["--cache-to", f"type=registry,ref={cache},mode=max,ignore-error=true"]
    for key, value in build_args.items():
        command += ["--build-arg", f"{key}={value}"]
    repository = os.environ.get("GITHUB_REPOSITORY", "hatefsystems/search-engine-core")
    command += ["--label", f"org.opencontainers.image.source=https://github.com/{repository}"]
    run(command + ["."])


def emit(values, env_file=None):
    content = "".join(f"{key}={value}\n" for key, value in values.items())
    print(content, end="", flush=True)
    for name in ("GITHUB_ENV", "GITHUB_OUTPUT"):
        if os.environ.get(name):
            with open(os.environ[name], "a") as stream:
                stream.write(content)
    if env_file:
        path = Path(env_file)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)


def prepare(args):
    if args.push:
        require_publish_context()
    nonce = ""
    if args.force:
        nonce = "-".join(filter(None, [os.environ.get("GITHUB_RUN_ID"),
                                      os.environ.get("GITHUB_RUN_ATTEMPT")]))
        nonce = nonce or str(time.time_ns())
    tags = image_tags(args.repository, args.cache_version, nonce)
    lock = locked_versions()
    resolved = {}
    for name, filename in BASE_FILES.items():
        with timed(name):
            existing = None if args.force or args.offline else inspect_image(tags[name])
            if existing:
                summary(f"Reuse `{existing}`")
                resolved[name] = existing
                continue
            build_args = {"UBUNTU_IMAGE": lock["ubuntu_image"], "BUILD_JOBS": args.jobs}
            if name == "mongodb-drivers":
                build_args.update(lock["mongodb"])
            elif name == "build-deps":
                build_args.update(lock["build"])
                build_args["MONGODB_BASE_IMAGE"] = resolved["mongodb-drivers"]
            else:
                build_args["BUILD_BASE_IMAGE"] = resolved["build-deps"]
            build_image(filename, tags[name], build_args, push=args.push, force=args.force,
                        cache=None if args.offline else f"ghcr.io/{args.repository.lower()}/buildcache-{name}:cache")
            resolved[name] = inspect_image(tags[name]) if args.push else tags[name]
            if not resolved[name]:
                raise RuntimeError(f"Published image is missing: {tags[name]}")
    emit({"BUILD_BASE_IMAGE": resolved["build-deps"],
          "RUNTIME_BASE_IMAGE": resolved["runtime-base"],
          "MONGODB_BASE_IMAGE": resolved["mongodb-drivers"]}, args.env_file)


def verify_public():
    failures = []
    # Use the built-in manifest command with an empty config so credentials
    # cannot accidentally make a private package appear publicly readable.
    with tempfile.TemporaryDirectory() as config:
        for key in ("MONGODB_BASE_IMAGE", "BUILD_BASE_IMAGE", "RUNTIME_BASE_IMAGE"):
            ref = os.environ[key]
            result = run(["docker", "--config", config, "manifest", "inspect", ref],
                         capture=True, check=False)
            if result.returncode:
                failures.append(ref)
    if failures:
        summary("Anonymous pulls failed for: " + ", ".join(f"`{ref}`" for ref in failures))
        raise RuntimeError("Check registry connectivity and set each new GHCR package's "
                           "visibility to Public in its Package settings, then rerun. "
                           "GitHub has no supported REST endpoint for this setting.")


def build_core(args):
    tag = f"search-engine-core-ci:{os.environ.get('GITHUB_SHA', 'local')}"
    with timed("Core compilation, short tests and runtime verification"):
        build_image("Dockerfile", tag,
                    {"BUILD_BASE_IMAGE": os.environ["BUILD_BASE_IMAGE"],
                     "RUNTIME_BASE_IMAGE": os.environ["RUNTIME_BASE_IMAGE"],
                     "BUILD_JOBS": args.jobs}, force=args.force,
                    cache=f"ghcr.io/{args.repository.lower()}/buildcache-server:cache")
    emit({"CANDIDATE_IMAGE": tag})


def publish_core(args):
    require_publish_context()
    sha = os.environ["GITHUB_SHA"]
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise RuntimeError("GITHUB_SHA must be a complete commit SHA")
    with timed("Core publication"):
        # Reuse the already verified build and export its layers to the registry.
        # BuildKit executes no changed steps between build and publication.
        image = f"ghcr.io/{args.repository.lower()}"
        build_image("Dockerfile", image + ":" + sha,
                    {"BUILD_BASE_IMAGE": os.environ["BUILD_BASE_IMAGE"],
                     "RUNTIME_BASE_IMAGE": os.environ["RUNTIME_BASE_IMAGE"],
                     "BUILD_JOBS": args.jobs}, push=True,
                    cache=f"{image}/buildcache-server:cache")
        immutable = inspect_image(image + ":" + sha)
        if not immutable:
            raise RuntimeError("Published server image is missing")
        # The workflow serializes publishers. An older queued run must not
        # move latest backwards if master advanced while it was compiling.
        head = run(["git", "ls-remote", "origin", "refs/heads/master"], capture=True).stdout.split()
        if head and head[0] == sha:
            run(["docker", "buildx", "imagetools", "create", "--tag", image + ":latest", immutable])
            summary(f"Promoted `{immutable}` to `latest`.")
        else:
            summary(f"Published `{immutable}`; master advanced, so latest was not updated.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["refs", "prepare", "build", "publish", "verify-public"])
    parser.add_argument("--repository", default=os.environ.get("GITHUB_REPOSITORY", "hatefsystems/search-engine-core"))
    parser.add_argument("--cache-version", default=os.environ.get("CACHE_VERSION", "1"))
    parser.add_argument("--jobs", default=os.environ.get("BUILD_JOBS", "2"))
    parser.add_argument("--force", action="store_true", default=os.environ.get("FORCE_REBUILD") == "true")
    parser.add_argument("--push", action="store_true")
    parser.add_argument("--offline", action="store_true", help="Skip GHCR lookups; build all bases locally")
    parser.add_argument("--env-file", help="Write resolved base references for local Docker/Compose builds")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", args.repository):
        parser.error("repository must be owner/repository")
    if not args.jobs.isdigit() or int(args.jobs) < 1:
        parser.error("jobs must be a positive integer")
    if args.offline and args.push:
        parser.error("--offline cannot publish")
    if args.command == "refs":
        print(json.dumps(image_tags(args.repository, args.cache_version), indent=2))
    elif args.command == "prepare":
        prepare(args)
    elif args.command == "build":
        build_core(args)
    elif args.command == "publish":
        publish_core(args)
    else:
        verify_public()


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, KeyError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
