# Hatef Semantic Icon Engine

A private, local SQLite/FTS5 icon catalog with optional multilingual E5 ONNX inference. The C++ application exposes same-origin routes and the profile editor stores stable icon IDs.

Start with the [deployment and operations guide](../../docs/semantic-icon-engine/README.md). See [architecture and API](../../docs/semantic-icon-engine/architecture.md), [licensing and security](../../docs/semantic-icon-engine/security.md), and [validation results](../../docs/semantic-icon-engine/validation.md).

Runtime startup never downloads sources, models or vectors. Preparation, ingestion, indexing and quantization are explicit administrative commands. The optional Compose `icons` profile defaults to lexical matching; set `ICON_SEMANTIC_ENABLED=true` only after preparing compatible vectors and validating the resource budget on the deployment host.
