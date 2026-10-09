#!/usr/bin/env python3
"""Administrative jobs. Never invoked by runtime startup."""
import argparse
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app.text import CONFIG
from app.ingest import ingest, atomic_json, digest


def prepare(upstream, models):
    lock = json.loads((CONFIG / "sources.lock.json").read_text())
    upstream = Path(upstream)
    upstream.mkdir(parents=True, exist_ok=True)
    for source in ("lucide", "simple-icons", "iconify"):
        dest = upstream / source
        if not dest.exists():
            subprocess.run(["git", "init", str(dest)], check=True)
            subprocess.run(
                [
                    "git",
                    "-C",
                    str(dest),
                    "remote",
                    "add",
                    "origin",
                    lock[source]["repository"],
                ],
                check=True,
            )
        subprocess.run(
            [
                "git",
                "-C",
                str(dest),
                "fetch",
                "--depth",
                "1",
                "origin",
                lock[source]["revision"],
            ],
            check=True,
        )
        subprocess.run(
            ["git", "-C", str(dest), "checkout", "--detach", lock[source]["revision"]],
            check=True,
        )
    model = lock["model"]
    dest = Path(models)
    dest.mkdir(parents=True, exist_ok=True)
    checks = {}
    for remote, name in [
        (model["file"], "model.onnx"),
        ("tokenizer.json", "tokenizer.json"),
        ("README.md", "README.md"),
    ]:
        target = dest / name
        if not target.exists():
            url = f"https://huggingface.co/{model['repository']}/resolve/{model['revision']}/{remote}"
            urllib.request.urlretrieve(url, str(target) + ".part")
            os.replace(str(target) + ".part", target)
        checks[name] = digest(target)
        if checks[name] != model["checksums"][name]:
            raise ValueError("download checksum mismatch: " + name)
    atomic_json(dest / "manifest.json", {**model, "checksums": checks})


def bundle_export(root, output):
    root = Path(root).resolve()
    checks = {}
    for p in sorted(root.rglob("*")):
        if p.is_symlink():
            raise ValueError("symlinks forbidden")
        if p.is_file():
            checks[str(p.relative_to(root))] = digest(p)
    with tarfile.open(output, "w:gz") as tar:
        payload = json.dumps(checks, sort_keys=True).encode()
        item = tarfile.TarInfo("checksums.json")
        item.size = len(payload)
        tar.addfile(item, io.BytesIO(payload))
        for name in checks:
            tar.add(root / name, arcname="bundle/" + name, recursive=False)


def bundle_restore(archive, destination, max_bytes):
    dest = Path(destination)
    if dest.exists():
        raise ValueError("restore requires a new directory")
    temp = dest.with_name(dest.name + ".staging")
    temp.mkdir(parents=True)
    try:
        with tarfile.open(archive, "r:gz") as tar:
            total = 0
            seen = set()
            for member in tar:
                path = Path(member.name)
                if (
                    member.name in seen
                    or not member.isfile()
                    or path.is_absolute()
                    or ".." in path.parts
                    or (member.name != "checksums.json" and path.parts[0] != "bundle")
                ):
                    raise ValueError("unsafe bundle entry")
                seen.add(member.name)
                total += member.size
                if total > max_bytes:
                    raise ValueError("bundle size limit")
                out = temp / path
                out.parent.mkdir(parents=True, exist_ok=True)
                with tar.extractfile(member) as src, out.open("wb") as dst:
                    shutil.copyfileobj(src, dst, 1024 * 1024)
        expected = json.loads((temp / "checksums.json").read_text())
        actual = {
            str(p.relative_to(temp / "bundle")): digest(p)
            for p in (temp / "bundle").rglob("*")
            if p.is_file()
        }
        if actual != expected:
            raise ValueError("bundle checksum mismatch")
        os.replace(temp / "bundle", dest)
    finally:
        shutil.rmtree(temp, ignore_errors=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="command", required=True)
    prep = sub.add_parser("prepare")
    prep.add_argument("--upstream", required=True)
    prep.add_argument("--models", required=True)
    imp = sub.add_parser("ingest")
    imp.add_argument("--upstream", required=True)
    imp.add_argument("--data", required=True)
    imp.add_argument("--policy")
    idx = sub.add_parser("index")
    idx.add_argument("--data", required=True)
    idx.add_argument("--models", required=True)
    idx.add_argument("--vectors", required=True)
    idx.add_argument("--priority")
    idx.add_argument("--reuse-vectors")
    idx.add_argument("--max-icons", type=int)
    idx.add_argument("--batch", type=int, default=8)
    exp = sub.add_parser("export")
    exp.add_argument("root")
    exp.add_argument("output")
    restore = sub.add_parser("restore")
    restore.add_argument("archive")
    restore.add_argument("destination")
    restore.add_argument("--max-bytes", type=int, default=20 * 1024**3)
    a = p.parse_args()
    if a.command == "prepare":
        prepare(a.upstream, a.models)
    elif a.command == "ingest":
        print(
            json.dumps(
                ingest(a.data, a.upstream, a.policy), ensure_ascii=False, indent=2
            )
        )
    elif a.command == "index":
        from app.catalog import Catalog
        from app.embeddings import E5, build

        if not 1 <= a.batch <= 32 or (a.max_icons is not None and a.max_icons <= 0):
            p.error("batch must be 1..32 and max-icons positive")
        build(
            a.vectors,
            Catalog(a.data),
            E5(a.models, int(os.getenv("ICON_THREADS", "1"))),
            a.batch,
            a.priority,
            a.max_icons,
            a.reuse_vectors,
        )
    elif a.command == "export":
        bundle_export(a.root, a.output)
    else:
        bundle_restore(a.archive, a.destination, a.max_bytes)
