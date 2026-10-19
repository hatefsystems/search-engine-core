# syntax=docker/dockerfile:1
# Published together from the dependency lock; CI overrides these defaults by digest.
ARG BUILD_BASE_IMAGE=ghcr.io/hatefsystems/search-engine-core/build-deps@sha256:eedd665fe424036125ec25f9340a77fd4d26fd0db7a656b2748064ca15ac8922
ARG RUNTIME_BASE_IMAGE=ghcr.io/hatefsystems/search-engine-core/runtime-base@sha256:f9eb2b9808673d8a1b2293c1f0dad96db69a8f29acb95e59f6d9b476a872f3fe

FROM ${BUILD_BASE_IMAGE} AS builder
ARG BUILD_JOBS=2
# RUN if ! pkg-config --exists vips; then apt-get update && apt-get install -y --no-install-recommends libvips-dev && rm -rf /var/lib/apt/lists/*; fi
WORKDIR /deps
COPY CMakeLists.txt ./
COPY src/ ./src/
COPY include/ ./include/
COPY tests/ ./tests/
RUN cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_STANDARD=20 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON \
        -DCMAKE_CXX_EXTENSIONS=OFF \
        -DCMAKE_PREFIX_PATH=/usr/local \
        -DBUILD_TESTS=ON && \
    cmake --build build --parallel "${BUILD_JOBS}"

FROM builder AS tested
COPY templates/ ./templates/
RUN ctest --test-dir build --output-on-failure --no-tests=error \
    -R '^(slug_generator_test|slug_cache_test|public_profile_test|profile_editor_test|profile_content_test|profile_validator_test|seo_generator_test)$'

FROM ${RUNTIME_BASE_IMAGE} AS runner
# RUN if ! ldconfig -p | grep -q libvips.so; then apt-get update && apt-get install -y --no-install-recommends libvips42 && rm -rf /var/lib/apt/lists/*; fi
ENV PORT=3000 \
    MINIFY_JS=true \
    SEARCH_REDIS_URI=tcp://127.0.0.1:6379 \
    SEARCH_REDIS_POOL_SIZE=4 \
    SEARCH_INDEX_NAME=search_index
WORKDIR /app
COPY --from=tested /deps/build/server ./server
COPY public/ ./public/
COPY locales/ ./locales/
COPY templates/ ./templates/
COPY --chmod=755 migrations/start.sh ./start.sh
COPY scripts/verify-runtime.sh /tmp/verify-runtime.sh
RUN bash /tmp/verify-runtime.sh && rm /tmp/verify-runtime.sh
EXPOSE ${PORT}
ENTRYPOINT ["/app/start.sh"]
