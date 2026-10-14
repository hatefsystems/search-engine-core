#!/usr/bin/env python3
"""Publish the tested local image without rebuilding; serialize this job in CI."""

import argparse
import json
import os
from pathlib import Path
import re

from docker_images import inspect_image, require_publish_context, run, summary


def publish(candidate, output):
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
    info = json.loads(
        run(["docker", "image", "inspect", candidate], capture=True).stdout
    )[0]
    if (info.get("Config", {}).get("Labels") or {}).get(
        "org.opencontainers.image.revision"
    ) != sha:
        raise ValueError("candidate revision does not match this workflow")
    image = f"ghcr.io/{repository}/semantic-icon-engine"
    # A new run/attempt has a distinct immutable release tag, even on the same commit.
    tag = f"{image}:sha-{sha}-{run_id}-{attempt}"
    run(["docker", "tag", candidate, tag])
    run(["docker", "push", "--platform", "linux/amd64", tag])
    immutable = inspect_image(tag)
    if not immutable:
        raise RuntimeError("published image is missing")
    manifest = json.loads(
        run(
            ["docker", "buildx", "imagetools", "inspect", immutable, "--raw"],
            capture=True,
        ).stdout
    )
    if manifest.get("config", {}).get("digest") != info["Id"]:
        raise ValueError("published image differs from the tested image")
    result = {
        "image": tag,
        "digestRef": immutable,
        "commit": sha,
        "testedImageId": info["Id"],
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
    args = parser.parse_args()
    publish(args.candidate, args.output)
