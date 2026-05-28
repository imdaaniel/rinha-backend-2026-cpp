# Multi-stage build for C++ Rinha API
FROM ubuntu:22.04 AS builder

WORKDIR /build

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    zlib1g-dev \
    nlohmann-json3-dev \
    libmicrohttpd-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
COPY . .

# Copy resources
COPY resources /resources

# Allow overriding the reference binary path at build time
ARG REF_BIN=data/references_300k.bin

# Copy pre-built metadata
COPY ${REF_BIN} data/references.bin

# Copy pre-built HNSW index from cpp/data
COPY data/hnsw_index.bin data/hnsw_index.bin

# Build main application directly with g++ (bypassing CMake issues)
RUN g++ -O3 -std=c++20 \
    src/main.cpp \
    src/vectorizer.cpp \
    src/index_loader.cpp \
    src/http_server.cpp \
    src/metadata.cpp \
    -I. -Isrc -Ihnswlib \
    -pthread -lz -lmicrohttpd \
    -o rinha-api 2>&1 || \
    (echo "Compilation failed, checking files:" && ls -la src/ && exit 1)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libmicrohttpd12 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy built binary and data
COPY --from=builder /build/rinha-api /app/
COPY --from=builder /build/data /app/data

# Copy resources
COPY --from=builder /resources /app/resources

# Set environment variables
ENV INDEX_PATH=/app/data/hnsw_index.bin
ENV METADATA_PATH=/app/data/references.bin
ENV MCC_RISK_PATH=/app/resources/mcc_risk.json
ENV NORMALIZATION_PATH=/app/resources/normalization.json

# Expose port
EXPOSE 9999

# Run the application
CMD ["/app/rinha-api"]
