#!/usr/bin/env python3
"""Publish the tested local image without rebuilding; serialize this job in CI."""

import argparse
import json
import os
from pathlib import Path
import re

from docker_images import inspect_image, require_publish_context, run, summary


PLATFORM = "linux/amd64"
MANIFEST_TYPES = {
    "application/vnd.oci.image.manifest.v1+json",
    "application/vnd.docker.distribution.manifest.v2+json",
}


def inspect_candidate(candidate, expected_digest):
    """Bind the loaded platform manifest to the digest emitted by Buildx."""
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", expected_digest):
        raise ValueError("invalid expected manifest digest")
    info = json.loads(
        run(
            ["docker", "image", "inspect", "--platform", PLATFORM, candidate],
            capture=True,
        ).stdout
    )[0]
    # With the containerd image store, Id identifies a manifest/index, not its
    # config. Require the explicit platform descriptor rather than guessing.
    descriptor = info.get("Descriptor") or {}
    if descriptor.get("mediaType") not in MANIFEST_TYPES:
        raise ValueError("candidate must have a single-platform manifest descriptor")
    if descriptor.get("digest") != expected_digest:
        raise ValueError("candidate manifest differs from the built image")
    if info.get("Os") != "linux" or info.get("Architecture") != "amd64":
        raise ValueError("candidate platform must be linux/amd64")
    return info


def publish(candidate, output, expected_digest):
    require_publish_context()
    repository = os.environ["GITHUB_REPOSITORY"].lower()
    sha = os.environ["GITHUB_SHA"]
    run_id = os.environ["GITHUB_RUN_ID"]
    attempt = os.environ["GITHUB_RUN_ATTEMPT"]
    if (
        not re.fullmatch(r"[a-z0-9_.-]+/[a-z0-9_.-]+", repository)
        or not re.fullmatch(r"[0-9a-f]{40}", sha)
        or not run_id.isdigit()
        or not attempt.isdigit()
    ):
        raise ValueError("invalid publication identity")
    info = inspect_candidate(candidate, expected_digest)
    if (info.get("Config", {}).get("Labels") or {}).get(
        "org.opencontainers.image.revision"
    ) != sha:
        raise ValueError("candidate revision does not match this workflow")
    image = f"ghcr.io/{repository}/semantic-icon-engine"
    # A new run/attempt has a distinct immutable release tag, even on the same commit.
    tag = f"{image}:sha-{sha}-{run_id}-{attempt}"
    run(["docker", "tag", info["Id"], tag])
    run(["docker", "push", "--platform", PLATFORM, tag])
    immutable = inspect_image(tag)
    if not immutable:
        raise RuntimeError("published image is missing")
    # Compare manifest with manifest. Equality covers config and every layer,
    # without conflating the manifest digest with the config digest.
    if immutable.rsplit("@", 1)[-1] != expected_digest:
        raise ValueError("published image differs from the tested image")
    result = {
        "image": tag,
        "digestRef": immutable,
        "commit": sha,
        "testedImageId": info["Id"],
        "testedManifestDigest": expected_digest,
    }
    head = run(
        ["git", "ls-remote", "origin", "refs/heads/master"], capture=True
    ).stdout.split()
    if head and head[0] == sha:
        run(
            [
                "docker",
                "buildx",
                "imagetools",
                "create",
                "--tag",
                image + ":latest",
                immutable,
            ]
        )
        result["promotedLatest"] = True
    else:
        result["promotedLatest"] = False
    Path(output).write_text(json.dumps(result, indent=2) + "\n")
    summary(
        f"Tested icon release: `{immutable}`. Set `ICON_ENGINE_IMAGE={immutable}` in deployment configuration."
    )
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", default="hatef-icons:release-test")
    parser.add_argument("--output", default="build/icons/published-image.json")
    parser.add_argument("--expected-digest", required=True)
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    if args.verify_only:
        inspect_candidate(args.candidate, args.expected_digest)
        print(f"Verified local {PLATFORM} manifest: {args.expected_digest}")
    else:
        publish(args.candidate, args.output, args.expected_digest)
