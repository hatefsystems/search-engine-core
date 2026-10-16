"""Run inside the network-isolated full-model container; emit measured HTTP results."""

import json
import hashlib
from pathlib import Path
import statistics
import time
import urllib.request
import urllib.error
import urllib.parse

BASE = "http://127.0.0.1:8080"


def request(path, body=None):
    req = urllib.request.Request(
        BASE + path,
        data=json.dumps(body).encode() if body is not None else None,
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=8) as response:
        return json.load(response)


for attempt in range(90):
    try:
        ready = request("/health/ready")
        if ready["model"] == "ready":
            break
    except (OSError, ValueError):
        pass
    time.sleep(1)
else:
    raise AssertionError("full-model container did not become ready")
assert ready["mode"] == "hybrid" and ready["icons"] > 0
assert ready["knowledge"] == "hybrid", ready
# Regression checks run against the real bundled catalog, not fixture metadata.
category_rows = request("/v1/icons/categories")["categories"]
assert category_rows and all(
    row["labelFa"] and not row["labelFa"].isascii() for row in category_rows
)
for query in ["قلب", "دل", "heart", "دوست داشتن"]:
    result = request(
        "/v1/icons/search?" + urllib.parse.urlencode({"q": query, "limit": 20})
    )
    assert result["engine"] == "hybrid", result
    assert result["items"][0]["id"] == "lucide:heart", (query, result)
for source, expected in [
    ("simple-icons", "simple-icons:terraform"),
    ("iconify", "iconify:devicon:terraform"),
]:
    result = request(
        "/v1/icons/search?" + urllib.parse.urlencode({"q": "ترافورم", "source": source})
    )
    assert any(item["id"] == expected for item in result["items"]), result
for query in ["AWS", "آمازون وب سرویسز"]:
    result = request("/v1/icons/search?" + urllib.parse.urlencode({"q": query}))
    assert any("amazonwebservices" in item["id"] for item in result["items"]), result
for path, color in [
    ("simple-icons/terraform", b"#844FBA"),
    ("iconify/devicon/terraform", b"#5c4ee5"),
]:
    with urllib.request.urlopen(BASE + "/assets/icons/" + path + ".svg") as asset:
        assert color in asset.read()

release_root = Path("/opt/icon-artifacts")
release = json.loads((release_root / "release.json").read_text())
assert release["approvedIcons"] == release["indexedIcons"] == ready["icons"]
assert release["concepts"] == release["indexedConcepts"] > 18000
concept_result = request(
    "/v1/concepts/search?" + urllib.parse.urlencode({"q": "رایانش ابری", "limit": 3})
)
assert concept_result["items"] and any(
    item["labelFa"] for item in concept_result["items"]
)
for query, expected in [
    ("وکالت در دادگاه", "lucide:scale"),
    ("تدریس خصوصی ریاضی", "lucide:calculator"),
    ("نقاشی ساختمان", "lucide:paintbrush"),
]:
    result = request(
        "/v1/icons/search?" + urllib.parse.urlencode({"q": query, "limit": 3})
    )
    assert result["items"][0]["id"] == expected, (query, result)
for name, expected in release["files"].items():
    with (release_root / name).open("rb") as stream:
        assert hashlib.file_digest(stream, "sha256").hexdigest() == expected, name
assert (release_root / "models/README.md").is_file()
assert not (release_root / "upstream").exists()
results = []
for case in json.loads(Path(__file__).with_name("evaluation.json").read_text()):
    begin = time.monotonic()
    response = request("/v1/icons/suggest", {"text": case["text"], "limit": 5})
    elapsed = (time.monotonic() - begin) * 1000
    assert response["engine"] == "hybrid", response
    ids = [i["id"] for i in response["suggestions"]]
    relevant = set(case["relevant"])
    results.append(
        {
            "text": case["text"],
            "latencyMs": elapsed,
            "recallAt3": len(relevant & set(ids[:3])) / len(relevant),
            "recallAt5": len(relevant & set(ids[:5])) / len(relevant),
        }
    )
# Combined fields exceed 512 tokens before tokenizer truncation. Timeout fallback is valid.
long_response = request(
    "/v1/icons/suggest", {"text": "cloud " * 330, "description": "cloud " * 330}
)
assert long_response["engine"] in ("hybrid", "lexical")
for attempt in range(15):
    try:
        recovered = request("/v1/icons/suggest", {"text": "medical services"})
        if recovered["engine"] == "hybrid":
            break
    except urllib.error.HTTPError as error:
        if error.code != 429:
            raise
    time.sleep(1)
else:
    raise AssertionError("inference slot did not recover")
with urllib.request.urlopen(BASE + "/assets/icons/lucide/cloud.svg") as asset:
    assert asset.status == 200 and b"<svg" in asset.read()
status = Path("/proc/1/status").read_text()
peak = next(
    int(line.split()[1]) / 1024
    for line in status.splitlines()
    if line.startswith("VmHWM:")
)
report = {
    "release": {k: v for k, v in release.items() if k != "files"},
    "ready": ready,
    "requests": len(results),
    "meanHttpMs": statistics.mean(r["latencyMs"] for r in results),
    "p95HttpMs": sorted(r["latencyMs"] for r in results)[47],
    "peakRssMiB": peak,
    "recallAt3": statistics.mean(r["recallAt3"] for r in results),
    "recallAt5": statistics.mean(r["recallAt5"] for r in results),
    "longRequestEngine": long_response["engine"],
    "memoryEvents": Path("/sys/fs/cgroup/memory.events").read_text(),
    "cpuStat": Path("/sys/fs/cgroup/cpu.stat").read_text(),
    "results": results,
}
print(json.dumps(report, ensure_ascii=False, indent=2))
