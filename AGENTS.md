# AGENTS.md - Technical Guide for kvllay

## Overview
`kvllay` is a lightweight, in-memory key-value database written in C++17. It implements a subset of the Redis RESP2 protocol and supports raw inline text commands. It is compatible with `redis-cli` and Redis client libraries.

---

## Directory Structure
```
.
├── include/
│   └── kvllay/
│       ├── kvllay.hpp      # Umbrella header including all module headers
│       ├── constants.hpp   # Centralized version, network, store, and protocol constants
│       ├── resp.hpp        # RESP2 serialization and streaming command parser
│       ├── store.hpp       # In-memory key-value storage engine (thread-safe, TTL, GC)
│       ├── commands.hpp    # Command dispatcher and individual command handlers
│       └── server.hpp      # Cross-platform TCP socket server (POSIX / Winsock)
├── src/
│   └── main.cpp            # Entry point, CLI argument parsing, server bootstrap
├── docs/
│   ├── ru.md               # Comprehensive Russian documentation and Redis benchmarks
│   └── en.md               # Comprehensive English documentation and Redis benchmarks
├── Dockerfile              # Multi-stage scratch build for ultra-minimal (< 2 MB) container
├── docker-compose.yml      # Local container orchestration
├── build/                  # Build artifact directory (ignored by git)
├── makefile                # Platform-detecting Makefile (Linux, Windows, Docker)
├── tasks.md                # Feature backlog and development roadmap
├── README.md               # User-facing project documentation
└── CLAUDE.md               # Agent reference pointer (@AGENTS.md)
```

---

## Core Components & Architecture

### 1. Storage Engine (`include/kvllay/store.hpp`)
- **Class**: `kvllay::Store`
- **Underlying Storage**: `std::unordered_map<std::string, std::string> data_`
- **Synchronization**: `mutable std::shared_mutex mutex_`
  - Read operations (`get`, `exists`, `keys`, `size`) acquire `std::shared_lock<std::shared_mutex>`.
  - Write operations (`set`, `del`, `flushdb`) acquire `std::unique_lock<std::shared_mutex>`.
- **Pattern Matching (`keys`)**:
  - `*`: returns all keys.
  - Prefix matching: `key*` -> `rfind(prefix, 0) == 0`.
  - Suffix matching: `*key` -> tail substring comparison.
  - Substring matching: `*key*` -> `find(substr) != std::string::npos`.
  - Exact match fallback.
- **Copy Semantics**: Non-copyable (copy constructor and copy assignment are deleted).

### 2. Protocol Engine (`include/kvllay/resp.hpp`)
- **Class**: `kvllay::Resp`
- **Parser Return Type**: `kvllay::ParseStatus` (`Success`, `Incomplete`, `Error`)
- **Protocol Formats Handled**:
  - **RESP2 Array**: Format `*<count>\r\n$<len>\r\n<data>\r\n...`. Handles negative lengths (null values) and tracks bytes consumed via `consumed_bytes`.
  - **Inline Text**: Space-delimited string terminated by `\r\n` or `\n`. Supports single and double-quoted arguments with backslash escaping (`\"`, `\'`).
- **Serialization Helpers**:
  - `simple_string(str)`: `+<str>\r\n`
  - `error(err)`: `-ERR <err>\r\n` (or preserves `-ERR ` / `-WRONGTYPE ` prefix if present)
  - `integer(val)`: `:<val>\r\n`
  - `bulk_string(val)`: `$<len>\r\n<val>\r\n`
  - `null_bulk_string()`: `$-1\r\n`
  - `empty_array()`: `*0\r\n`
  - `array(vector<string>)`: `*<count>\r\n` followed by serialized bulk strings.

### 3. Command Dispatcher (`include/kvllay/commands.hpp`)
- **Classes / Structs**:
  - `kvllay::CommandResult`: `{ std::string response; bool should_close; }`
  - `kvllay::CommandHandler`: Dispatches tokenized command arguments to internal member functions.
- **Command Routing**:
  - Command name is normalized to uppercase using `std::transform(..., ::toupper)`.
  - `QUIT` returns `+OK\r\n` with `should_close = true` without checking auth.
  - `AUTH` evaluates credentials before global auth enforcement.
  - If server password is configured and client is unauthenticated, all other commands return `-NOAUTH Authentication required.\r\n`.
  - Unknown commands return `-ERR unknown command '<name>'\r\n`.
- **Current Implemented Commands**:
  - `AUTH [username] password`: Authenticates connection. Accepts optional username for Redis 6+ compatibility. Returns `-WRONGPASS ...` on mismatch.
  - `PING [message]`: Returns `+PONG\r\n` or the message as bulk string.
  - `SET key value`: Sets string value, returns `+OK\r\n`.
  - `GET key`: Returns bulk string or null bulk string (`$-1\r\n`).
  - `DEL key [key ...]`: Deletes keys, returns integer count of removed keys.
  - `EXISTS key [key ...]`: Returns integer count of existing keys.
  - `KEYS [pattern]`: Returns RESP array of matching keys. Default pattern is `*`.
  - `FLUSHDB` / `FLUSHALL`: Clears store, returns `+OK\r\n`.
  - `DBSIZE`: Returns integer count of total keys.
  - `ECHO message`: Returns argument as bulk string.
  - `COMMAND` / `COMMAND DOCS`: Returns `*0\r\n` to pass redis-cli handshake.
  - `INFO`: Returns server version, kvllay version, uptime in seconds, and keyspace count.

### 4. Networking & Server (`include/kvllay/server.hpp`)
- **Class**: `kvllay::Server`
- **Platform Abstraction**:
  - Windows: Uses `winsock2.h`, `ws2tcpip.h`, `SOCKET`, `WSAStartup`/`WSACleanup`, linked with `-lws2_32`.
  - POSIX / Linux: Uses standard socket API (`sys/socket.h`, `netinet/in.h`, `arpa/inet.h`, `unistd.h`).
- **Connection Model**:
  - Main thread creates socket, binds to specified IP/port, sets `SO_REUSEADDR`, and listens with `SOMAXCONN`.
  - Accept loop creates a detached `std::thread(&Server::handle_client, this, client_socket)` per connection.
  - Sets `TCP_NODELAY` on every accepted socket.
- **Client Handler Pipeline**:
  - 4096-byte chunk buffer reads into persistent per-client `std::string client_buffer`.
  - Loops over `client_buffer` with `Resp::parse_command`:
    - `Success`: Erases parsed bytes from `client_buffer`, executes command via `command_handler_.dispatch`, transmits full response using `send_all`. Closes socket if `should_close` is set.
    - `Incomplete`: Breaks loop and waits for more socket data via `recv`.
    - `Error`: Sends `-ERR Protocol error\r\n` and closes connection immediately.
  - Tracks connection-level `bool authenticated` (defaults to `true` if server password is empty, `false` otherwise).

### 5. CLI Entrypoint (`src/main.cpp`)
- Parses CLI flags and positional arguments:
  - `-p`, `--port <port>`: Port to listen on (default: `6379`).
  - `-h`, `--bind`, `--host <host>`: Bind IP (default: `0.0.0.0`).
  - `-a`, `--requirepass`, `--password <pass>`: Server auth password.
  - Positional fallback: `kvllay [port] [host] [password]`.
  - `--help`: Prints usage options.
- Instantiates `kvllay::Server` and executes `server.run()`.

---

## Build and Run

### Compiler Requirements
- C++17 compatible compiler (`g++`, `clang++`, or `MSVC`).
- POSIX threads (`-pthread`) on Linux.
- Winsock (`-lws2_32`) on Windows.

### Make Targets
- `make compile`: Compiles binary into `build/kvllay` (`build/kvllay.exe` on Windows).
- `make run`: Compiles and runs binary with default settings (`0.0.0.0:6379`).
- `make clean`: Removes binary from `build/`.

### Manual Compilation
```bash
# Linux
g++ -std=c++17 -Wall -Wextra -O2 -I header -I include -I include/kvllay src/main.cpp -o build/kvllay -pthread

# Windows (MinGW)
g++ -std=c++17 -Wall -Wextra -O2 -I header -I include -I include/kvllay -D _WIN32_WINNT=0x0A00 src/main.cpp -o build/kvllay.exe -lws2_32
```

---

## Protocol Verification & Interaction

Interact with and verify the running server using standard CLI tools:

### redis-cli
```bash
# Connect without authentication
redis-cli -p 6379 PING
redis-cli -p 6379 SET key value
redis-cli -p 6379 GET key

# Connect with authentication
redis-cli -p 6379 -a "mypassword" GET key
```

### Raw TCP (Netcat / Telnet)
```bash
# Inline command format
echo -e "PING\r\n" | nc 127.0.0.1 6379

# RESP2 array format
echo -e "*3\r\n\$3\r\nSET\r\n\$4\r\nname\r\n\$4\r\njohn\r\n" | nc 127.0.0.1 6379
```

---

## Guide for Extending the Codebase

When modifying or expanding `kvllay`, adhere to the following architecture patterns:

### 1. Adding a New Command
1. **Declare and implement logic in `include/kvllay/store.hpp`** (if storage mutation or lookup is required):
   - Add thread-safe method using `std::shared_lock` for read-only operations or `std::unique_lock` for mutations.
2. **Add handler method in `include/kvllay/commands.hpp`**:
   - Signature: `CommandResult handle_<name>(const std::vector<std::string>& args)`.
   - Validate argument count (`args.size()`). Return `Resp::error(...)` on invalid count or format.
   - Call `store_` method and serialize return value using `Resp::*` helper functions.
3. **Route in `CommandHandler::dispatch`**:
   - Check normalized `cmd == "<NAME>"`.
   - Invoke `handle_<name>(args)`.
4. **Verification**:
   - Verify command handling against expected RESP2 responses using `redis-cli` or raw TCP sockets.
5. **Update documentation**:
   - Add the command to `README.md`, `tasks.md`, and update this file (`AGENTS.md`).

### 2. Implementing Expiration (TTL)
- Storage representation: Change `data_` value type or add secondary index in `Store` (e.g., `std::unordered_map<std::string, uint64_t> expires_`).
- Expired key eviction:
  - Passive: Check expiration timestamp inside `get()`, `exists()`, `keys()`. If expired, erase key and treat as nonexistent.
  - Active: Background thread running periodic sweep with `unique_lock`.

### 3. Implementing Persistence (AOF / Snapshot)
- AOF: Append mutating command raw RESP strings to file on successful execution in `CommandHandler` or dedicated worker thread.
- On startup, read AOF file and feed through `Resp::parse_command` directly into `CommandHandler`.

---

## Coding Conventions and Invariants
- **Language**: C++17 standard features only. Avoid external dependencies.
- **Header Structure**: All core logic resides in headers (`include/kvllay/*.hpp`). Header inclusion is guarded with `#ifndef` and `#pragma once`.
- **Namespace**: All core types are inside `namespace kvllay`.
- **Thread Safety**: Storage modifications MUST synchronize via `mutex_` in `Store`. Handlers and network layers must not bypass the storage mutex.
- **Protocol Precision**: All responses must strictly adhere to the Redis RESP2 format (`\r\n` line endings, valid integer formats, null representation).
