#!/usr/bin/env python3
"""Prepare checksum-pinned Persian drafts. Never mark machine output reviewed."""

import argparse
from collections import Counter
import fcntl
import hashlib
import html
import json
from pathlib import Path
import re
import sqlite3
import sys
import time
import unicodedata
import os
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app.ingest import atomic_json, digest
from app.knowledge import read_esco
from app.text import CONFIG, normalize


def source_hash(row):
    fields = {
        k: row[k]
        for k in (
            "id",
            "uri",
            "kind",
            "labelEn",
            "aliasesEn",
            "descriptionEn",
            "sourceVersion",
        )
    }
    return hashlib.sha256(
        json.dumps(fields, ensure_ascii=False, sort_keys=True).encode()
    ).hexdigest()


def clean_source(value, sentence=False):
    text = html.unescape(re.sub(r"<[^>]*>", " ", value))
    text = " ".join(text.split())
    if sentence and text:
        text = text[0].upper() + text[1:]
        if text[-1] not in ".!?":
            text += "."
    return text


def clean_fa(text, title=False):
    text = unicodedata.normalize("NFC", text).translate(
        str.maketrans({"ي": "ی", "ك": "ک"})
    )
    text = " ".join(text.split()).strip()
    return text.rstrip(".۔ ") if title else text


def has_persian(text):
    return bool(
        re.search(
            r"[\u0621-\u063a\u0641-\u064a\u067e\u0686\u0698\u06a9\u06af\u06cc]", text
        )
    )


def quality_flags(row, draft):
    flags = []
    title = draft["labelFa"]
    if not title or not has_persian(title):
        flags.append("title-needs-persian")
    if normalize(title) == normalize(row["labelEn"]):
        flags.append("unchanged-title")
    if row.get("descriptionEn") and not has_persian(draft["descriptionFa"]):
        flags.append("description-needs-persian")
    if len(title) > max(240, len(row["labelEn"]) * 5):
        flags.append("title-length-outlier")
    if any(not has_persian(v) for v in draft["aliasesFa"]):
        flags.append("alias-needs-review")
    if "<unk>" in json.dumps(draft, ensure_ascii=False):
        flags.append("unknown-token")
    return flags


class DraftTranslator:
    def __init__(self, root, cache, spec, threads=2):
        import ctranslate2
        import sentencepiece

        root = Path(root)
        for name, expected in spec["checksums"].items():
            if digest(root / name) != expected:
                raise ValueError("translation model checksum mismatch: " + name)
        self.source = sentencepiece.SentencePieceProcessor(
            model_file=str(root / "src.spm.model")
        )
        self.target = sentencepiece.SentencePieceProcessor(
            model_file=str(root / "tgt.spm.model")
        )
        self.translator = ctranslate2.Translator(
            str(root),
            device="cpu",
            compute_type="int8",
            intra_threads=threads,
            inter_threads=4,
        )
        self.beam = spec["beamSize"]
        self.identity = hashlib.sha256(
            json.dumps(spec, sort_keys=True).encode()
        ).hexdigest()
        self.cache = sqlite3.connect(cache)
        self.cache.execute(
            "CREATE TABLE IF NOT EXISTS translations(model TEXT,source TEXT,target TEXT,PRIMARY KEY(model,source))"
        )
        self.values = {
            s: t
            for s, t in self.cache.execute(
                "SELECT source,target FROM translations WHERE model=?", (self.identity,)
            )
        }
        self.truncated = set()

    def segments(self, text):
        # No truncation: split at sentence boundaries, then at token boundaries.
        parts = re.split(r"(?<=[.!?])\s+|\n+", clean_source(text))
        out = []
        for part in parts:
            tokens = self.source.encode(part, out_type=str)
            for start in range(0, len(tokens), 160):
                out.append(self.source.decode(tokens[start : start + 160]))
        return out

    def prepare(self, strings, name):
        missing = {
            piece
            for text in strings
            for piece in self.segments(text)
            if piece not in self.values
        }
        missing = sorted(
            missing, key=lambda value: (len(self.source.encode(value)), value)
        )
        start_time = time.monotonic()
        for start in range(0, len(missing), 256):
            batch = missing[start : start + 256]
            results = self.translator.translate_batch(
                [self.source.encode(v, out_type=str) + ["</s>"] for v in batch],
                beam_size=self.beam,
                max_batch_size=32,
                max_decoding_length=384,
                disable_unk=True,
                return_end_token=True,
            )
            for original, result in zip(batch, results, strict=True):
                tokens = result.hypotheses[0]
                if not tokens or tokens[-1] != "</s>":
                    raise ValueError(
                        "translation hit decoding limit; retry with shorter source: "
                        + original[:80]
                    )
                translated = clean_fa(self.target.decode(tokens[:-1]))
                if not translated:
                    raise ValueError("empty translation for nonempty source")
                self.values[original] = translated
                self.cache.execute(
                    "INSERT OR REPLACE INTO translations VALUES(?,?,?)",
                    (self.identity, original, translated),
                )
            self.cache.commit()
            if start % 256 == 0 or start + 256 >= len(missing):
                print(
                    json.dumps(
                        {
                            "phase": name,
                            "done": min(start + 256, len(missing)),
                            "total": len(missing),
                            "seconds": round(time.monotonic() - start_time, 1),
                        }
                    ),
                    flush=True,
                )

    def translate(self, text, title=False):
        return clean_fa(
            " ".join(self.values[piece] for piece in self.segments(text)), title
        )


def generate(
    archive, model, cache, output, threads=2, partition_index=0, partition_count=1
):
    root = Path(output)
    root.mkdir(parents=True, exist_ok=True)
    with (root / ".generation.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if (root / "manifest.json").exists():
            raise ValueError(
                "use a new output directory; existing drafts must not be overwritten"
            )
        return _generate(
            archive, model, cache, output, threads, partition_index, partition_count
        )


def _generate(
    archive, model, cache, output, threads=2, partition_index=0, partition_count=1
):
    source_spec = json.loads((CONFIG / "knowledge.lock.json").read_text())["esco"]
    spec = json.loads((CONFIG / "translation.lock.json").read_text())
    rows, _ = read_esco(archive, source_spec)
    rows.sort(key=lambda row: row["id"])
    full_count = len(rows)
    if not 1 <= partition_count <= 32 or not 0 <= partition_index < partition_count:
        raise ValueError("invalid translation partition")
    rows = rows[partition_index::partition_count]
    translator = DraftTranslator(model, cache, spec, threads)
    # Sentence case and punctuation improve phrase translation. English remains immutable.
    translator.prepare([clean_source(r["labelEn"], True) for r in rows], "titles")
    translator.prepare([r["descriptionEn"] for r in rows], "descriptions")
    translator.prepare(
        [clean_source(alias, True) for r in rows for alias in r["aliasesEn"]], "aliases"
    )
    glossary = {}
    for item in json.loads((CONFIG / "knowledge-fa.json").read_text())["concepts"]:
        for alias in [item["labelEn"], *item.get("aliasesEn", [])]:
            glossary.setdefault(normalize(alias), set()).add(item["labelFa"])
    drafts = []
    glossary_hits = 0
    for row in rows:
        title = translator.translate(clean_source(row["labelEn"], True), True)
        matches = glossary.get(normalize(row["labelEn"]), set())
        if len(matches) == 1:
            title = next(iter(matches))
            glossary_hits += 1
        draft = {
            "id": row["id"],
            "sourceSha256": source_hash(row),
            "labelFa": title,
            "descriptionFa": translator.translate(row["descriptionEn"]),
            # Preserve source order and cardinality for side-by-side review.
            "aliasesFa": [
                translator.translate(clean_source(v, True), True)
                for v in row["aliasesEn"]
            ],
            "translationStatus": "machine-draft",
            "reviewer": "",
        }
        draft["qualityFlags"] = quality_flags(row, draft)
        drafts.append(draft)
    provenance = (
        {
            "partition": {
                "index": partition_index,
                "count": partition_count,
                "fullRecords": full_count,
            }
        }
        if partition_count > 1
        else {}
    )
    manifest = write_drafts(
        output,
        rows,
        drafts,
        source_spec,
        spec,
        glossary_hits,
        {"intraThreads": threads, "interThreads": 4},
        provenance,
    )
    print(
        json.dumps(
            {k: v for k, v in manifest.items() if k not in ("generator", "shards")},
            ensure_ascii=False,
            indent=2,
        ),
        flush=True,
    )
    return manifest


def write_drafts(
    output,
    rows,
    drafts,
    source_spec,
    spec,
    glossary_hits=0,
    execution=None,
    provenance=None,
):
    execution = execution or {}
    duplicates = Counter(normalize(r["labelFa"]) for r in drafts)
    for row in drafts:
        row["qualityFlags"] = [
            flag for flag in row["qualityFlags"] if flag != "shared-persian-title"
        ]
        if duplicates[normalize(row["labelFa"])] > 1:
            row["qualityFlags"].append("shared-persian-title")
    root = Path(output)
    root.mkdir(parents=True, exist_ok=True)
    shards = []
    for start in range(0, len(drafts), 500):
        path = root / f"drafts-{start // 500:03d}.jsonl"
        batch = drafts[start : start + 500]
        path.write_text(
            "".join(
                json.dumps(row, ensure_ascii=False, sort_keys=True) + "\n"
                for row in batch
            )
        )
        shards.append(
            {"file": path.name, "sha256": digest(path), "records": len(batch)}
        )
    manifest = {
        "schema": 1,
        "sourceVersion": source_spec["version"],
        "sourceSha256": source_spec["sha256"],
        "records": len(drafts),
        "generator": spec,
        "implementationSha256": digest(Path(__file__)),
        "execution": {
            "inputWindowTokens": 160,
            "maxDecodingTokens": 384,
            "batchSize": 32,
            "intraThreads": execution.get("intraThreads"),
            "interThreads": execution.get("interThreads"),
        },
        "preparation": provenance or {},
        "glossarySource": "assistant-authored knowledge-fa.json; not human reviewed",
        "glossaryTitleMatches": glossary_hits,
        "humanReviewed": 0,
        "status": "machine-draft",
        "titleCoverage": sum(bool(r["labelFa"]) for r in drafts),
        "descriptionCoverage": sum(bool(r["descriptionFa"]) for r in drafts),
        "nonemptySourceDescriptions": sum(bool(r["descriptionEn"]) for r in rows),
        "translatedAliasCount": sum(len(r["aliasesFa"]) for r in drafts),
        "sourceAliasCount": sum(len(r["aliasesEn"]) for r in rows),
        "persianScriptTitles": sum(has_persian(r["labelFa"]) for r in drafts),
        "flaggedRecords": sum(bool(r["qualityFlags"]) for r in drafts),
        "qualityFlags": dict(
            Counter(flag for r in drafts for flag in r["qualityFlags"])
        ),
        "notice": "Full machine draft coverage is not expert translation approval or independent human acceptance. Drafts are not trusted runtime aliases.",
        "shards": shards,
    }
    atomic_json(root / "manifest.json", manifest)
    load_drafts(root / "manifest.json", rows, source_spec)
    return manifest


def load_drafts(manifest_path, source_rows=None, source_spec=None):
    path = Path(manifest_path)
    manifest = json.loads(path.read_text())
    if (
        manifest.get("schema") != 1
        or manifest.get("status") != "machine-draft"
        or manifest.get("humanReviewed") != 0
    ):
        raise ValueError("invalid machine draft manifest")
    if source_spec and (
        manifest["sourceVersion"] != source_spec["version"]
        or manifest["sourceSha256"] != source_spec["sha256"]
    ):
        raise ValueError("draft source version or checksum mismatch")
    drafts = {}
    files = set()
    for shard in manifest["shards"]:
        name = shard["file"]
        if (
            Path(name).name != name
            or name in files
            or (path.parent / name).is_symlink()
        ):
            raise ValueError("unsafe or duplicate draft shard")
        files.add(name)
        target = path.parent / name
        if digest(target) != shard["sha256"]:
            raise ValueError("draft shard checksum mismatch")
        rows = [json.loads(line) for line in target.read_text().splitlines()]
        if len(rows) != shard["records"]:
            raise ValueError("draft shard count mismatch")
        for row in rows:
            if (
                row["id"] in drafts
                or row.get("translationStatus") != "machine-draft"
                or row.get("reviewer") != ""
            ):
                raise ValueError("duplicate draft or falsely reviewed machine output")
            if (
                not isinstance(row.get("labelFa"), str)
                or not row["labelFa"].strip()
                or not isinstance(row.get("descriptionFa"), str)
            ):
                raise ValueError("missing draft title or description")
            if not isinstance(row.get("aliasesFa"), list) or not all(
                isinstance(v, str) and v for v in row["aliasesFa"]
            ):
                raise ValueError("invalid draft aliases")
            drafts[row["id"]] = row
    if len(drafts) != manifest["records"]:
        raise ValueError("draft coverage count mismatch")
    if source_rows is not None:
        originals = {r["id"]: r for r in source_rows}
        if set(originals) != set(drafts):
            raise ValueError("draft coverage must equal all source IDs")
        for id_, row in originals.items():
            if drafts[id_]["sourceSha256"] != source_hash(row):
                raise ValueError("draft source record mismatch")
            if row["descriptionEn"] and not drafts[id_]["descriptionFa"].strip():
                raise ValueError("missing source description translation")
            if len(row["aliasesEn"]) != len(drafts[id_]["aliasesFa"]):
                raise ValueError("missing source alias translation")
    return manifest, drafts


def download_model(output):
    """Explicit admin download. Neither generation nor runtime fetches weights."""
    spec = json.loads((CONFIG / "translation.lock.json").read_text())
    root = Path(output)
    root.mkdir(parents=True, exist_ok=True)
    for name, expected in spec["checksums"].items():
        if Path(name).name != name:
            raise ValueError("unsafe translation model path")
        path = root / name
        if path.exists() and digest(path) == expected:
            continue
        temp = path.with_suffix(path.suffix + ".part")
        url = f"https://huggingface.co/{spec['repository']}/resolve/{spec['revision']}/{name}"
        try:
            with urllib.request.urlopen(url, timeout=120) as response, temp.open(
                "wb"
            ) as out:
                total = 0
                while chunk := response.read(1024**2):
                    total += len(chunk)
                    if total > 500 * 1024**2:
                        raise ValueError("translation model file exceeds size limit")
                    out.write(chunk)
            if digest(temp) != expected:
                raise ValueError("translation model checksum mismatch: " + name)
            os.replace(temp, path)
        finally:
            temp.unlink(missing_ok=True)
    return {
        "repository": spec["repository"],
        "revision": spec["revision"],
        "verifiedFiles": len(spec["checksums"]),
    }


def download_source(path):
    spec = json.loads((CONFIG / "knowledge.lock.json").read_text())["esco"]
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if digest(path) != spec["sha256"]:
            raise ValueError("ESCO archive checksum mismatch")
        return
    temp = path.with_suffix(".part")
    try:
        with urllib.request.urlopen(spec["url"], timeout=120) as response, temp.open(
            "wb"
        ) as out:
            total = 0
            while chunk := response.read(1024**2):
                total += len(chunk)
                if total > 50 * 1024**2:
                    raise ValueError("ESCO archive exceeds size limit")
                out.write(chunk)
        if digest(temp) != spec["sha256"]:
            raise ValueError("ESCO archive checksum mismatch")
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def merge_drafts(archive, parts, output, run_url=""):
    source_spec = json.loads((CONFIG / "knowledge.lock.json").read_text())["esco"]
    spec = json.loads((CONFIG / "translation.lock.json").read_text())
    originals, _ = read_esco(archive, source_spec)
    originals.sort(key=lambda row: row["id"])
    manifests = sorted(Path(parts).glob("*/manifest.json"))
    if not manifests or (Path(output) / "manifest.json").exists():
        raise ValueError("merge requires prepared partitions and a new output")
    combined, seen_parts, provenance = {}, set(), []
    glossary_hits = 0
    for path in manifests:
        manifest, rows = load_drafts(path, source_spec=source_spec)
        part = manifest.get("preparation", {}).get("partition", {})
        index, count = part.get("index"), part.get("count")
        if (
            not isinstance(index, int)
            or count != len(manifests)
            or index in seen_parts
            or not 0 <= index < count
            or part.get("fullRecords") != len(originals)
        ):
            raise ValueError(
                "incomplete, duplicate or incompatible translation partitions"
            )
        if manifest.get("generator") != spec or manifest.get(
            "implementationSha256"
        ) != digest(Path(__file__)):
            raise ValueError(
                "translation recipe or implementation mismatch across partitions"
            )
        load_drafts(path, originals[index::count], source_spec)
        if set(rows) & set(combined):
            raise ValueError("overlapping translation partitions")
        combined.update(rows)
        seen_parts.add(index)
        glossary_hits += manifest["glossaryTitleMatches"]
        provenance.append(
            {
                "index": index,
                "manifestSha256": digest(path),
                "records": len(rows),
                "execution": manifest["execution"],
            }
        )
    if seen_parts != set(range(len(manifests))):
        raise ValueError("missing translation partition")
    result = write_drafts(
        output,
        originals,
        [combined[r["id"]] for r in originals],
        source_spec,
        spec,
        glossary_hits,
        provenance={
            "method": "deterministic source-ID partitions; recomputed global duplicate-title warnings",
            "runUrl": run_url,
            "parts": provenance,
        },
    )
    print(
        json.dumps(
            {
                "mergedRecords": result["records"],
                "parts": len(manifests),
                "humanReviewed": 0,
            }
        ),
        flush=True,
    )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "action",
        choices=["download-source", "download-model", "generate", "merge", "validate"],
    )
    parser.add_argument("--archive")
    parser.add_argument("--output", default=str(CONFIG / "esco-fa-drafts"))
    parser.add_argument("--model")
    parser.add_argument("--cache", default="/tmp/esco-translation-cache.sqlite")
    parser.add_argument("--threads", type=int, default=2)
    parser.add_argument("--partition-index", type=int, default=0)
    parser.add_argument("--partition-count", type=int, default=1)
    parser.add_argument("--parts")
    parser.add_argument("--run-url", default="")
    args = parser.parse_args()
    if args.action == "download-source":
        if not args.archive:
            parser.error("download-source requires a destination --archive")
        download_source(args.archive)
    elif args.action == "download-model":
        if not args.model:
            parser.error("download-model requires a destination --model")
        print(json.dumps(download_model(args.model)))
    elif not args.archive:
        parser.error("generate and validate require --archive")
    elif args.action == "merge":
        if not args.parts:
            parser.error("merge requires --parts")
        merge_drafts(args.archive, args.parts, args.output, args.run_url)
    elif args.action == "generate":
        if not args.model or not 1 <= args.threads <= 16:
            parser.error("generate requires a local pinned --model and threads 1..16")
        generate(
            args.archive,
            args.model,
            args.cache,
            args.output,
            args.threads,
            args.partition_index,
            args.partition_count,
        )
    else:
        spec = json.loads((CONFIG / "knowledge.lock.json").read_text())["esco"]
        rows, _ = read_esco(args.archive, spec)
        manifest, _ = load_drafts(Path(args.output) / "manifest.json", rows, spec)
        print(
            json.dumps(
                {
                    "validatedRecords": manifest["records"],
                    "humanReviewed": 0,
                    "runtimeAliasesActivated": False,
                }
            )
        )


if __name__ == "__main__":
    main()
