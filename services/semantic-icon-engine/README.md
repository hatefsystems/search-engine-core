# Hatef Semantic Icon Engine

A private, local SQLite/FTS5 icon catalog with optional multilingual E5 ONNX inference. The C++ application exposes same-origin routes and the profile editor stores stable icon IDs.

Start with the [deployment and operations guide](../../docs/semantic-icon-engine/README.md). See [architecture and API](../../docs/semantic-icon-engine/architecture.md), [licensing and security](../../docs/semantic-icon-engine/security.md), and [validation results](../../docs/semantic-icon-engine/validation.md).

Runtime startup never downloads sources, models or vectors. Preparation, ingestion, indexing and quantization are explicit administrative commands. The optional Compose `icons` profile defaults to lexical matching; set `ICON_SEMANTIC_ENABLED=true` only after preparing compatible vectors and validating the resource budget on the deployment host.

For deployment, use the CI-published complete image from the repository root:

```sh
docker compose --profile icons pull
docker compose --profile icons up -d --no-build
```

The image contains the validated catalog, pinned model and complete vectors. First publication occurs after a trusted master CI run; pin `ICON_ENGINE_IMAGE` to its published digest for repeatable deployment and rollback. No setup container, host Python or artifact mount is needed in production. Explicit local development builds and the setup job use `docker-compose.dev.yml`; see the runbook.

The frozen [500-query Persian benchmark](../../docs/semantic-icon-engine/persian-500-benchmark.md) measures manual search across 25 domains. Its current hybrid Hit@3 is 29.2%; the 90% target has not been reached. Full per-query evidence and reproduction commands are included.
