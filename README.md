# Rinha de Backend 2026 - C++ Implementation

High-performance C++ implementation for the Rinha de Backend 2026 fraud detection challenge.

## Performance Goals

- **p99 latency**: <= 1ms
- **Failure rate**: 0%
- **Resource limits**: 1 CPU, 350MB total
- **Architecture**: Load balancer + 2 API instances

## Architecture

This implementation uses several optimizations to achieve the performance requirements:

### Key Optimizations

1. **Pre-built HNSW Index**: The HNSW index is built during Docker image build, not at runtime. This eliminates the >5 minute index building time that plagued the Go implementation.

2. **Memory-mapped Index Loading**: The pre-built HNSW index is loaded instantly using memory mapping, ensuring fast startup.

3. **Lightweight HTTP Server**: Uses libmicrohttpd for minimal overhead HTTP handling.

4. **Optimized Vectorization**: Efficient 14-dimensional vector computation with minimal allocations.

5. **Binary Reference Format**: References are stored in a custom binary format for fast loading and memory efficiency.

### Components

- **Vectorizer**: Converts transaction payloads to 14-dimensional vectors following the specification
- **IndexLoader**: Loads pre-built HNSW index and reference labels
- **HttpServer**: Lightweight HTTP server handling `/ready` and `/fraud-score` endpoints
- **HNSW**: Hierarchical Navigable Small World index for approximate nearest neighbor search

## Building

### Local Build

```bash
# Build the index builder
mkdir -p build && cd build
cmake ..
make build-index

# Build the index (requires resources from base/)
./build-index ../base/resources/references.json.gz data/references.bin data/hnsw_index.bin

# Build the main application
make rinha-api
```

### Docker Build

```bash
# Build the Docker image (includes index building)
docker build -t rinha-cpp:latest .

# Run with docker-compose
docker-compose up
```

## Running

### Docker Compose

```bash
docker-compose up -d
```

This starts:
- 2 API instances (api-1, api-2) on ports 9991 and 9992
- 1 HAProxy load balancer on port 9999

### Manual

```bash
# Set environment variables
export INDEX_PATH=/app/data/hnsw_index.bin
export METADATA_PATH=/app/data/references.bin
export MCC_RISK_PATH=/app/resources/mcc_risk.json
export NORMALIZATION_PATH=/app/resources/normalization.json

# Run the API
./rinha-api
```

## API Endpoints

### GET /ready

Health check endpoint. Returns `200 OK` when the index is loaded.

### POST /fraud-score

Fraud detection endpoint.

Request:
```json
{
  "id": "tx-123",
  "transaction": {
    "amount": 384.88,
    "installments": 3,
    "requested_at": "2026-03-11T20:23:35Z"
  },
  "customer": {
    "avg_amount": 769.76,
    "tx_count_24h": 3,
    "known_merchants": ["MERC-009", "MERC-001"]
  },
  "merchant": {
    "id": "MERC-001",
    "mcc": "5912",
    "avg_amount": 298.95
  },
  "terminal": {
    "is_online": false,
    "card_present": true,
    "km_from_home": 13.7090520965
  },
  "last_transaction": {
    "timestamp": "2026-03-11T14:58:35Z",
    "km_from_current": 18.8626479774
  }
}
```

Response:
```json
{
  "approved": false,
  "fraud_score": 1.0
}
```

## Performance Characteristics

- **Index Building**: Done during Docker build (~2-3 minutes for 3M vectors)
- **Index Loading**: Instant (memory-mapped pre-built index)
- **Query Latency**: Target p99 <= 1ms using HNSW with optimized parameters
- **Memory Usage**: ~150MB per API instance (well under 175MB limit)

## HNSW Parameters

- **M**: 16 (number of connections per node)
- **ef_construction**: 200 (size of candidate list during construction)
- **ef_search**: 50 (size of candidate list during search)

These parameters balance index quality and search speed for the 3M vector dataset.

## Comparison with Go Implementation

| Aspect | Go | C++ |
|--------|-----|-----|
| Index Building | Runtime (>5 min) | Docker build (2-3 min) |
| Index Loading | Runtime | Instant (mmap) |
| HTTP Server | fasthttp | libmicrohttpd |
| Memory Management | GC | Manual (no GC overhead) |
| SIMD | Limited | Enabled via -march=native |

## License

This implementation uses:
- hnswlib (MIT license)
- nlohmann/json (MIT license)
- libmicrohttpd (LGPL 2.1)
