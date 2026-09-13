<div align="center">
  <img src="logo.png" alt="kvllay logo" width="128" style="image-rendering: pixelated; image-rendering: crisp-edges;">
  <h1>kvllay</h1>
  <p><em>[pronounced: <strong>key-vi-lay</strong> · «кей-ви-лей» (key-value allay)]</em></p>
  <p>In-memory key-value store</p>

  <p>
    <a href="https://github.com/thekeny/kvllay/releases">Release</a> •
    <a href="docs/ru.md">Русская документация</a> •
    <a href="docs/en.md">English Documentation</a>
  </p>
</div>

Compatible with standard `redis-cli` and official client SDK libraries for any programming language.

## Features
- **RESP2 Protocol**: full support for Redis command formats (arrays, bulk strings, errors, integers, inline commands).
- **Security (AUTH)**: password protection (`--requirepass` / `-a`), safeguards against unauthorized access (`NOAUTH` / `WRONGPASS`).
- **Network Configuration**: bind to any network interface (`0.0.0.0` for network access, `127.0.0.1` for local access only).
- **Commands**:
  - `AUTH [username] password`
  - `PING [message]`
  - `SET key value`
  - `GET key`
  - `MSET key value [key value ...]`
  - `MGET key [key ...]`
  - `DEL key [key ...]`
  - `EXISTS key [key ...]`
  - `KEYS [pattern]`
  - `DBSIZE`
  - `FLUSHDB`
  - `EXPIRE key seconds` / `PEXPIRE key milliseconds`
  - `TTL key` / `PTTL key`
  - `PERSIST key`
  - `SETEX key seconds value`
  - `INCR key` / `DECR key`
  - `INCRBY key increment` / `DECRBY key decrement`
  - `SAVE` (synchronous snapshot)
  - `BGSAVE` (background snapshot without `fork()`)
  - `LASTSAVE` (UNIX epoch timestamp of last save)
  - `BGREWRITEAOF` (background AOF compaction without `fork()`)
  - `ECHO message`
  - `COMMAND` / `COMMAND DOCS` (redis-cli handshake)
  - `INFO` (includes `# Persistence`)
  - `QUIT`
- **Zero-Fork Persistence (Snapshots & AOF)**:
  - **Snapshots (`dump.kvl`)**: Compact binary format with CRC32 data integrity, atomic file rename, and zero `fork()` (no page-table pauses or Copy-On-Write memory doubling).
  - **Append-Only Log (`kvllay.aof`)**: Asynchronous double-buffered logger with configurable fsync (`always`, `everysec`, `no`), decoupling client request latency from disk I/O.
- **Atomic Counters & Rate Limiting**: thread-safe counters with overflow checks for high-throughput rate limiters.
- **Thread Safety**: `std::shared_mutex` (fast concurrent reads with `GET`, synchronized writes with `SET`/`DEL`).
- **TTL & Eviction**: Hybrid passive (`Lazy`) + active background garbage collector.
- **Ultra-Lightweight**: Docker image under **1.6 MB** (`scratch` static binary).
- **Cross-Platform**: unified codebase for Linux (POSIX sockets) and Windows (Winsock).

## Download Standalone Binary

You can download ready-to-run standalone binaries from [Releases](https://github.com/thekeny/kvllay/releases) (no dependencies required):
- **Linux (x86_64)**: `chmod +x kvllay-linux-x86_64 && ./kvllay-linux-x86_64`
- **Windows (x86_64)**: `.\kvllay-windows-x86_64.exe`

## Quickstart with Docker

```bash
# Run pre-built image directly from Docker Hub (no cloning required!)
docker run -d --name kvllay -p 6379:6379 kenyka/kvllay:latest

# Or with Docker Compose
docker compose up -d
```

## Build and Run Locally

### Building
```bash
make compile
```

### Running the Server
```bash
# Basic run (port 6379, listening on all interfaces 0.0.0.0)
make run

# Run with password authentication
./build/kvllay -p 6379 -a "mypassword"

# Restrict access to localhost only
./build/kvllay -p 6379 -h 127.0.0.1

# Run with periodic background snapshot (every 60 seconds)
./build/kvllay -p 6379 --save 60

# Run with Append-Only Log (AOF) persistence (fsync every second)
./build/kvllay -p 6379 --aof kvllay.aof --appendfsync everysec

# Run with both snapshots and AOF
./build/kvllay -p 6379 --snapshot dump.kvl --aof

# View all options
./build/kvllay --help
```

## Usage with `redis-cli`

### Without password
```bash
redis-cli -p 6379
127.0.0.1:6379> PING
PONG
127.0.0.1:6379> SET user "Alex"
OK
127.0.0.1:6379> GET user
"Alex"
127.0.0.1:6379> SETEX temp 60 "secret"
OK
127.0.0.1:6379> TTL temp
(integer) 60
127.0.0.1:6379> INCR hits
(integer) 1
127.0.0.1:6379> INCRBY hits 10
(integer) 11
```

### With password
```bash
redis-cli -p 6379 -a "mypassword"
127.0.0.1:6379> GET user
"Alex"
```

## Benchmark Kvllay vs Redis

| Workload | kvllay v1.0.0 | Redis v8.x (8.8.0) | Comparison |
| :--- | :---: | :---: | :--- |
| **Single-Client: SET** | **78,077 RPS** | 69,913 RPS | **kvllay +11.7% faster** |
| **Single-Client: GET** | **82,471 RPS** | 70,437 RPS | **kvllay +17.1% faster** |
| **Single-Client: INCR** | **84,718 RPS** | 70,422 RPS | **kvllay +20.3% faster** |
| **Single-Client: MSET (5 keys)** | **78,665 RPS** | 59,143 RPS | **kvllay +33.0% faster** |
| **Parallel 8-Thread: SET** | **120,268 RPS** | 119,629 RPS | **kvllay ahead (32 shards)** |
| **Concurrent 50 Clients: GET** | **136,799 RPS** | 131,752 RPS | **kvllay +3.8% faster** |
| **Concurrent 50 Clients: INCR** | **139,860 RPS** | 136,799 RPS | **kvllay +2.2% faster** |
| **Latency p50 (Parallel 8-Thread)** | **0.047 ms (47 μs)** | 0.052 ms (52 μs) | **kvllay 10% lower latency** |
| **Idle RAM** | **~4.1 MB** | ~15.2 MB | **kvllay 3.7x lighter** |
| **50,000 Keys RAM** | **~11.4 MB** | ~20.0 MB | **kvllay 43% less RAM** |
| **Docker Image Size** | **~1.6 MB** | ~140 MB | **kvllay 87x smaller** |
| **Cold Start** | **~3.2 ms** | ~7.6 ms | **kvllay 2.4x faster** |

![Throughput: Single-Client RPS](docs/images/benchmark_single_client.png)

![Throughput: Multi-Threaded RPS](docs/images/benchmark_multithreaded.png)

*See full benchmarks, methodology, and visual graphs in the [Russian Documentation](docs/ru.md#6-бенчмарк-сравнение-с-redis) and [English Documentation](docs/en.md#6-benchmark-comparison-with-redis).*
