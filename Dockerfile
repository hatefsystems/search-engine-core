# syntax=docker/dockerfile:1
ARG BUILD_BASE_IMAGE
ARG RUNTIME_BASE_IMAGE

FROM ${BUILD_BASE_IMAGE} AS builder
ARG BUILD_JOBS=2
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
RUN ctest --test-dir build --output-on-failure --no-tests=error \
    -R '^(slug_generator_test|slug_cache_test)$'

FROM ${RUNTIME_BASE_IMAGE} AS runner
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
