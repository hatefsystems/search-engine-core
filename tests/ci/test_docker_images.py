"""Regression tests for cache invalidation, registry errors and publish gates."""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("docker_images", ROOT / "scripts/docker_images.py")
images = importlib.util.module_from_spec(spec)
spec.loader.exec_module(images)


class DependencyHashTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for filename in [*images.BASE_FILES.values(), "docker/dependencies.lock.json", "scripts/docker_images.py"]:
            destination = self.root / filename
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / filename, destination)
        self.before = self.tags()

    def tags(self, **kwargs):
        return images.image_tags("Owner/Repo", root=self.root, **kwargs)

    def change(self, filename):
        with (self.root / filename).open("a") as stream:
            stream.write("\n# changed\n")

    def test_source_and_assets_do_not_change_dependency_tags(self):
        for directory in ("src", "public", "templates", "tests"):
            (self.root / directory).mkdir()
            (self.root / directory / "changed").write_text("new content")
        self.assertEqual(self.before, self.tags())

    def test_runtime_change_preserves_build_and_mongo_images(self):
        self.change(images.BASE_FILES["runtime-base"])
        after = self.tags()
        self.assertEqual(self.before["mongodb-drivers"], after["mongodb-drivers"])
        self.assertEqual(self.before["build-deps"], after["build-deps"])
        self.assertNotEqual(self.before["runtime-base"], after["runtime-base"])

    def test_build_dependency_version_invalidates_its_runtime_child(self):
        lock_path = self.root / "docker/dependencies.lock.json"
        lock = json.loads(lock_path.read_text())
        lock["build"]["CMAKE_VERSION"] = "changed"
        lock_path.write_text(json.dumps(lock))
        after = self.tags()
        self.assertEqual(self.before["mongodb-drivers"], after["mongodb-drivers"])
        for name in ("build-deps", "runtime-base"):
            self.assertNotEqual(self.before[name], after[name])

    def test_parent_change_invalidates_all_children(self):
        self.change(images.BASE_FILES["mongodb-drivers"])
        for name, tag in self.tags().items():
            self.assertNotEqual(self.before[name], tag)

    def test_cache_version_and_force_nonce_create_new_versions(self):
        for after in (self.tags(cache_version="2"), self.tags(nonce="run-1")):
            for name, tag in after.items():
                self.assertNotEqual(self.before[name], tag)
        self.assertNotEqual(self.tags(nonce="run-1"), self.tags(nonce="run-2"))


class RegistryTests(unittest.TestCase):
    ref = "ghcr.io/owner/repo/build-deps:deps-123"

    def result(self, stderr="", stdout="", code=1):
        return subprocess.CompletedProcess([], code, stdout, stderr)

    def test_only_missing_manifests_are_cache_misses(self):
        for error in (f"ERROR: {self.ref}: not found", "ERROR: manifest unknown"):
            with patch.object(images, "run", return_value=self.result(stderr=error)):
                self.assertIsNone(images.inspect_image(self.ref))

    def test_registry_failures_do_not_trigger_rebuilds(self):
        for error in ("unauthorized", "403 Forbidden", "429 Too Many Requests",
                      "lookup ghcr.io: no such host", "context deadline exceeded",
                      "ERROR: credential helper not found"):
            with patch.object(images, "run", return_value=self.result(stderr=error)):
                with self.assertRaises(RuntimeError):
                    images.inspect_image(self.ref)

    def test_resolves_tag_to_digest(self):
        digest = "sha256:" + "a" * 64
        with patch.object(images, "run", return_value=self.result(
                code=0, stdout=json.dumps({"digest": digest}))):
            self.assertEqual(images.inspect_image(self.ref),
                             "ghcr.io/owner/repo/build-deps@" + digest)


class PipelineTests(unittest.TestCase):
    def options(self, **kwargs):
        values = dict(push=False, force=False, offline=False, repository="owner/repo",
                      cache_version="1", jobs="2", env_file=None)
        values.update(kwargs)
        return argparse.Namespace(**values)

    def test_pr_and_non_master_runs_cannot_publish(self):
        for event, ref in (("pull_request", "refs/heads/master"),
                           ("pull_request_target", "refs/heads/master"),
                           ("workflow_dispatch", "refs/heads/feature")):
            with patch.dict(os.environ, {"GITHUB_EVENT_NAME": event, "GITHUB_REF": ref}):
                with self.assertRaises(RuntimeError):
                    images.prepare(self.options(push=True))

    @patch.object(images, "summary")
    @patch.object(images, "emit")
    @patch.object(images, "build_image")
    @patch.object(images, "inspect_image", return_value="ghcr.io/owner/base@sha256:" + "a" * 64)
    def test_existing_images_skip_all_dependency_builds(self, inspect, build, emit, summary):
        images.prepare(self.options())
        self.assertEqual(inspect.call_count, 3)
        build.assert_not_called()

    @patch.object(images, "summary")
    @patch.object(images, "emit")
    @patch.object(images, "build_image")
    @patch.object(images, "inspect_image", return_value=None)
    def test_fork_bootstrap_builds_local_parents_without_pushing(self, inspect, build, emit, summary):
        images.prepare(self.options())
        calls = build.call_args_list
        self.assertEqual(len(calls), 3)
        self.assertFalse(any(call.kwargs["push"] for call in calls))
        self.assertEqual(calls[1].args[2]["MONGODB_BASE_IMAGE"], calls[0].args[1])
        self.assertEqual(calls[2].args[2]["BUILD_BASE_IMAGE"], calls[1].args[1])

    @patch.object(images, "summary")
    @patch.object(images, "emit")
    @patch.object(images, "build_image")
    @patch.object(images, "inspect_image")
    def test_forced_local_rebuild_skips_lookup(self, inspect, build, emit, summary):
        images.prepare(self.options(force=True))
        inspect.assert_not_called()
        self.assertTrue(all(call.kwargs["force"] for call in build.call_args_list))

    @patch.object(images, "summary")
    @patch.object(images, "build_image")
    @patch.object(images, "inspect_image", side_effect=RuntimeError("registry unavailable"))
    def test_prepare_stops_on_registry_failure(self, inspect, build, summary):
        with self.assertRaises(RuntimeError):
            images.prepare(self.options())
        build.assert_not_called()


    @patch.object(images, "summary")
    @patch.object(images, "build_image")
    @patch.object(images, "inspect_image", return_value="ghcr.io/owner/repo@sha256:" + "a" * 64)
    @patch.object(images, "run")
    def test_old_commit_never_promotes_latest(self, run, inspect, build, summary):
        sha = "1" * 40
        run.return_value = subprocess.CompletedProcess([], 0, "2" * 40 + "\trefs/heads/master\n", "")
        with patch.dict(os.environ, {"GITHUB_EVENT_NAME": "push", "GITHUB_REF": "refs/heads/master",
                                     "GITHUB_SHA": sha, "BUILD_BASE_IMAGE": "build@sha256:a",
                                     "RUNTIME_BASE_IMAGE": "runtime@sha256:b"}):
            images.publish_core(self.options())
        self.assertEqual(run.call_count, 1)
        self.assertEqual(run.call_args.args[0][:2], ["git", "ls-remote"])

    @patch.object(images, "summary")
    @patch.object(images, "build_image")
    @patch.object(images, "inspect_image", return_value="ghcr.io/owner/repo@sha256:" + "a" * 64)
    @patch.object(images, "run")
    def test_current_commit_promotes_verified_digest(self, run, inspect, build, summary):
        sha = "1" * 40
        run.return_value = subprocess.CompletedProcess([], 0, sha + "\trefs/heads/master\n", "")
        with patch.dict(os.environ, {"GITHUB_EVENT_NAME": "push", "GITHUB_REF": "refs/heads/master",
                                     "GITHUB_SHA": sha, "BUILD_BASE_IMAGE": "build@sha256:a",
                                     "RUNTIME_BASE_IMAGE": "runtime@sha256:b"}):
            images.publish_core(self.options())
        self.assertEqual(run.call_args.args[0][-2:],
                         ["ghcr.io/owner/repo:latest", "ghcr.io/owner/repo@sha256:" + "a" * 64])


if __name__ == "__main__":
    unittest.main()
