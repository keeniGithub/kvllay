<div align="center">
  <img src="logo.png" alt="kvllay logo" width="128" style="image-rendering: pixelated; image-rendering: crisp-edges;">
  <h1>kvllay</h1>
  <p>In-memory key-value store</p>

  <p>
    <a href="#">Release</a> •
    <a href="#">Русская документация</a> •
    <a href="#">English Documentation</a>
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
  - `DEL key [key ...]`
  - `EXISTS key [key ...]`
  - `KEYS [pattern]`
  - `DBSIZE`
  - `FLUSHDB`
  - `ECHO message`
  - `COMMAND` / `COMMAND DOCS` (redis-cli handshake)
  - `INFO`
  - `QUIT`
- **Thread Safety**: `std::shared_mutex` (fast concurrent reads with `GET`, synchronized writes with `SET`/`DEL`).
- **Cross-Platform**: unified codebase for Linux (POSIX sockets) and Windows (Winsock).

## Build and Run

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
```

### With password
```bash
redis-cli -p 6379 -a "mypassword"
127.0.0.1:6379> GET user
"Alex"
```
