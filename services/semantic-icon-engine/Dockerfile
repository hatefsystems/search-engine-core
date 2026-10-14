FROM python:3.12.8-slim-bookworm@sha256:2199a62885a12290dc9c5be3ca0681d367576ab7bf037da120e564723292a2f0 AS base
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 HF_HUB_OFFLINE=1 TRANSFORMERS_OFFLINE=1 TOKENIZERS_PARALLELISM=false OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1
WORKDIR /app
COPY requirements.txt ./
RUN pip install --no-cache-dir --require-hashes -r requirements.txt && useradd --uid 10001 --create-home icons
COPY app ./app
COPY config ./config
COPY scripts ./scripts
COPY licenses ./licenses

FROM base AS admin
RUN apt-get update && apt-get install -y --no-install-recommends git ca-certificates && rm -rf /var/lib/apt/lists/*
ENV HF_HUB_OFFLINE=0 TRANSFORMERS_OFFLINE=0
ENTRYPOINT ["python","scripts/bootstrap.py"]
CMD ["--artifacts","/artifacts","--upstream","/upstream"]

FROM base AS runtime-base
USER 10001:10001
EXPOSE 8080
CMD ["uvicorn","app.api:app","--host","0.0.0.0","--port","8080","--workers","1","--no-access-log","--limit-concurrency","32","--timeout-keep-alive","5"]

FROM runtime-base AS release
ARG VCS_REF
LABEL org.opencontainers.image.source="https://github.com/hatefsystems/search-engine-core" org.opencontainers.image.revision=$VCS_REF
# The named context contains only verified runtime artifacts, not upstream clones.
COPY --from=icon-artifacts --chown=10001:10001 /models/ /opt/icon-artifacts/models/
COPY --from=icon-artifacts --chown=10001:10001 /data/ /opt/icon-artifacts/data/
COPY --from=icon-artifacts --chown=10001:10001 /vectors/ /opt/icon-artifacts/vectors/
COPY --from=icon-artifacts --chown=10001:10001 /release.json /opt/icon-artifacts/release.json
ENV ICON_DATA_DIR=/opt/icon-artifacts/data ICON_MODEL_DIR=/opt/icon-artifacts/models ICON_VECTOR_DIR=/opt/icon-artifacts/vectors

# Preserve the lightweight default target for explicit local development.
FROM runtime-base AS runtime
