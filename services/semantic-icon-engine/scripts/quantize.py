#!/usr/bin/env python3
"""Offline experiment only. Requires onnx==1.17.0 in the admin environment."""
import argparse
import json
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app.embeddings import E5
from app.ingest import atomic_json, digest
from onnxruntime.quantization import quantize_dynamic, QuantType

p = argparse.ArgumentParser()
p.add_argument("source")
p.add_argument("destination")
a = p.parse_args()
source = Path(a.source)
dest = Path(a.destination)
model = E5(source)
del model
spec = json.loads((source / "manifest.json").read_text())
dest.mkdir(parents=True, exist_ok=False)
quantize_dynamic(
    str(source / "model.onnx"),
    str(dest / "model.onnx"),
    weight_type=QuantType.QInt8,
    op_types_to_quantize=["MatMul"],
    per_channel=False,
)
for name in ["tokenizer.json", "README.md"]:
    shutil.copyfile(source / name, dest / name)
spec["parentChecksums"] = spec["checksums"]
spec["checksums"] = {
    name: digest(dest / name) for name in ["model.onnx", "tokenizer.json", "README.md"]
}
spec["quantization"] = "dynamic-int8-matmul"
atomic_json(dest / "manifest.json", spec)
print(
    json.dumps(
        {
            "modelBytes": (dest / "model.onnx").stat().st_size,
            "quantization": spec["quantization"],
        }
    )
)
