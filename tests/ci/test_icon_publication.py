"""Publication must reuse the tested image and never publish untrusted PRs."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import publish_icon_image as publication


class IconPublicationTests(unittest.TestCase):
    sha = "1" * 40
    config = "sha256:" + "a" * 64
    immutable = "ghcr.io/owner/repo/semantic-icon-engine@sha256:" + "b" * 64

    def invoke(self, event="push", head=None, remote_config=None):
        commands = []

        def run(command, **kwargs):
            commands.append(command)
            if command[:3] == ["docker", "image", "inspect"]:
                out = json.dumps(
                    [
                        {
                            "Id": self.config,
                            "Config": {
                                "Labels": {
                                    "org.opencontainers.image.revision": self.sha
                                }
                            },
                        }
                    ]
                )
            elif command[:4] == ["docker", "buildx", "imagetools", "inspect"]:
                out = json.dumps({"config": {"digest": remote_config or self.config}})
            elif command[:2] == ["git", "ls-remote"]:
                out = (head or self.sha) + "\trefs/heads/master\n"
            else:
                out = ""
            return subprocess.CompletedProcess(command, 0, out, "")

        env = {
            "GITHUB_EVENT_NAME": event,
            "GITHUB_REF": "refs/heads/master",
            "GITHUB_REPOSITORY": "Owner/Repo",
            "GITHUB_SHA": self.sha,
            "GITHUB_RUN_ID": "123",
            "GITHUB_RUN_ATTEMPT": "1",
        }
        with tempfile.TemporaryDirectory() as directory, patch.dict(
            os.environ, env
        ), patch.object(publication, "run", side_effect=run), patch.object(
            publication, "inspect_image", return_value=self.immutable
        ), patch.object(
            publication, "summary"
        ):
            result = publication.publish(
                "tested-local-image", Path(directory) / "published.json"
            )
        return result, commands

    def test_pushes_tested_candidate_without_any_rebuild(self):
        result, commands = self.invoke()
        self.assertEqual(result["digestRef"], self.immutable)
        self.assertTrue(result["promotedLatest"])
        self.assertTrue(
            any(
                c[:2] == ["docker", "tag"] and c[2] == "tested-local-image"
                for c in commands
            )
        )
        self.assertFalse(any("build" in c for c in commands))

    def test_pull_request_is_rejected_before_docker_operations(self):
        with patch.dict(
            os.environ,
            {"GITHUB_EVENT_NAME": "pull_request", "GITHUB_REF": "refs/heads/master"},
        ), patch.object(publication, "run") as run:
            with self.assertRaises(RuntimeError):
                publication.publish("candidate", "unused")
            run.assert_not_called()

    def test_stale_master_does_not_promote_latest(self):
        result, commands = self.invoke(head="2" * 40)
        self.assertFalse(result["promotedLatest"])
        self.assertFalse(
            any(c[:4] == ["docker", "buildx", "imagetools", "create"] for c in commands)
        )

    def test_different_registry_config_stops_promotion(self):
        with self.assertRaisesRegex(ValueError, "differs"):
            self.invoke(remote_config="sha256:" + "c" * 64)


if __name__ == "__main__":
    unittest.main()
