FROM ubuntu:22.04 as builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    libcurl4-openssl-dev \
    pkg-config \
    libz-dev \
    wget \
    gnupg \
    redis-tools \
    libhiredis-dev \
    && rm -rf /var/lib/apt/lists/*

# Install MongoDB C driver
RUN wget -qO - https://www.mongodb.org/static/pgp/server-6.0.asc | apt-key add - \
    && echo "deb [ arch=amd64,arm64 ] https://repo.mongodb.org/apt/ubuntu jammy/mongodb-org/6.0 multiverse" | tee /etc/apt/sources.list.d/mongodb-org-6.0.list \
    && apt-get update \
    && apt-get install -y mongodb-org-shell libmongoc-dev libmongocxx-dev \
    && rm -rf /var/lib/apt/lists/*

# Install redis++ from source
WORKDIR /deps
RUN git clone --recursive https://github.com/sewenew/redis-plus-plus.git \
    && cd redis-plus-plus \
    && mkdir build \
    && cd build \
    && cmake .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig

WORKDIR /app
COPY . .

# Build using CMake
RUN rm -rf build && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_PREFIX_PATH="/usr/local/lib/cmake/mongocxx-4.0.0;/usr/local/lib/cmake/bsoncxx-4.0.0" .. && \
    make -j$(nproc)

FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libcurl4 \
    redis-tools \
    libhiredis0.14 \
    wget \
    gnupg \
    && rm -rf /var/lib/apt/lists/*

# Install MongoDB client runtime
RUN wget -qO - https://www.mongodb.org/static/pgp/server-6.0.asc | apt-key add - \
    && echo "deb [ arch=amd64,arm64 ] https://repo.mongodb.org/apt/ubuntu jammy/mongodb-org/6.0 multiverse" | tee /etc/apt/sources.list.d/mongodb-org-6.0.list \
    && apt-get update \
    && apt-get install -y mongodb-mongosh \
    && rm -rf /var/lib/apt/lists/*

# Copy built artifacts from builder stage
COPY --from=builder /app/build/search-engine-core /usr/local/bin/
COPY --from=builder /usr/local/lib/libredis++.so* /usr/local/lib/
RUN ldconfig

EXPOSE 8080

CMD ["search-engine-core"]