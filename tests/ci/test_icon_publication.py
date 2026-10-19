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
    # Docker's containerd store returns a manifest ID, distinct from config ID.
    config = "sha256:" + "a" * 64
    manifest = "sha256:" + "b" * 64
    immutable = "ghcr.io/owner/repo/semantic-icon-engine@" + manifest

    def invoke(
        self,
        event="push",
        head=None,
        remote_digest=None,
        candidate_digest=None,
        expected_digest=None,
        media_type="application/vnd.oci.image.manifest.v1+json",
        architecture="amd64",
        revision=None,
    ):
        commands = []
        self.commands = commands

        def run(command, **kwargs):
            commands.append(command)
            if command[:3] == ["docker", "image", "inspect"]:
                out = json.dumps(
                    [
                        {
                            "Id": candidate_digest or self.manifest,
                            "Descriptor": {
                                "digest": candidate_digest or self.manifest,
                                "mediaType": media_type,
                            },
                            "Os": "linux",
                            "Architecture": architecture,
                            "Config": {
                                "Labels": {
                                    "org.opencontainers.image.revision": revision
                                    or self.sha
                                }
                            },
                        }
                    ]
                )
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
            publication,
            "inspect_image",
            return_value=(
                "ghcr.io/owner/repo/semantic-icon-engine@" + remote_digest
                if remote_digest
                else self.immutable
            ),
        ), patch.object(
            publication, "summary"
        ):
            result = publication.publish(
                "tested-local-image",
                Path(directory) / "published.json",
                expected_digest or self.manifest,
            )
        return result, commands

    def test_pushes_tested_candidate_without_any_rebuild(self):
        result, commands = self.invoke()
        self.assertEqual(result["digestRef"], self.immutable)
        self.assertTrue(result["promotedLatest"])
        self.assertNotEqual(result["testedImageId"], self.config)
        self.assertEqual(result["testedManifestDigest"], self.manifest)
        self.assertIn(
            [
                "docker",
                "image",
                "inspect",
                "--platform",
                "linux/amd64",
                "tested-local-image",
            ],
            commands,
        )
        self.assertTrue(
            any(c[:2] == ["docker", "tag"] and c[2] == self.manifest for c in commands)
        )
        self.assertFalse(any("build" in c for c in commands))

    def test_pull_request_is_rejected_before_docker_operations(self):
        with patch.dict(
            os.environ,
            {"GITHUB_EVENT_NAME": "pull_request", "GITHUB_REF": "refs/heads/master"},
        ), patch.object(publication, "run") as run:
            with self.assertRaises(RuntimeError):
                publication.publish("candidate", "unused", self.manifest)
            run.assert_not_called()

    def test_stale_master_does_not_promote_latest(self):
        result, commands = self.invoke(head="2" * 40)
        self.assertFalse(result["promotedLatest"])
        self.assertFalse(
            any(c[:4] == ["docker", "buildx", "imagetools", "create"] for c in commands)
        )

    def test_different_registry_manifest_stops_promotion(self):
        with self.assertRaisesRegex(ValueError, "differs"):
            self.invoke(remote_digest="sha256:" + "c" * 64)
        self.assertFalse(any("create" in c for c in self.commands))

    def test_different_candidate_stops_before_push(self):
        with self.assertRaisesRegex(ValueError, "candidate manifest differs"):
            self.invoke(candidate_digest="sha256:" + "c" * 64)
        self.assertFalse(any("push" in c or "tag" in c for c in self.commands))

    def test_invalid_build_digest_stops_before_docker(self):
        with self.assertRaisesRegex(ValueError, "invalid expected"):
            self.invoke(expected_digest="not-a-digest")
        self.assertEqual(self.commands, [])

    def test_index_or_missing_descriptor_stops_before_push(self):
        for media_type in ["application/vnd.oci.image.index.v1+json", None]:
            with self.subTest(media_type=media_type), self.assertRaisesRegex(
                ValueError, "single-platform manifest"
            ):
                self.invoke(media_type=media_type)
            self.assertFalse(any("push" in c for c in self.commands))

    def test_wrong_platform_stops_before_push(self):
        with self.assertRaisesRegex(ValueError, "platform must be"):
            self.invoke(architecture="arm64")
        self.assertFalse(any("push" in c for c in self.commands))

    def test_wrong_revision_stops_before_push(self):
        with self.assertRaisesRegex(ValueError, "revision does not match"):
            self.invoke(revision="2" * 40)
        self.assertFalse(any("push" in c for c in self.commands))


if __name__ == "__main__":
    unittest.main()
