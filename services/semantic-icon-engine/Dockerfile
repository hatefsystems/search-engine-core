FROM python:3.12.8-slim-bookworm@sha256:2199a62885a12290dc9c5be3ca0681d367576ab7bf037da120e564723292a2f0
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 HF_HUB_OFFLINE=1 TRANSFORMERS_OFFLINE=1 TOKENIZERS_PARALLELISM=false OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1
WORKDIR /app
COPY requirements.txt ./
RUN pip install --no-cache-dir --require-hashes -r requirements.txt && useradd --uid 10001 --create-home icons
COPY app ./app
COPY config ./config
COPY scripts ./scripts
USER 10001:10001
EXPOSE 8080
CMD ["uvicorn","app.api:app","--host","0.0.0.0","--port","8080","--workers","1","--no-access-log","--limit-concurrency","32","--timeout-keep-alive","5"]
