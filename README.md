# PacketBridge

PacketBridge is a Secure P2P LAN Transfer Engine planned around automatic local peer discovery, reliable chunked file transfer, checkpointed sessions, and SHA-256 integrity verification.

## Planned Features

- UDP-based automatic peer discovery on the local network.
- TCP-based reliable file transfer using fixed-size chunks.
- Custom binary protocol with versioned message types.
- Transfer sessions backed by file manifests.
- ACK/checkpoint-based continuation support for interrupted transfers.
- SHA-256 verification for received files and future chunk validation.
- Clean modular C++17 implementation with CMake builds.

## Architecture Overview

- `apps/`: command-line entry points for listener, broadcaster, sender, and receiver roles.
- `include/common` and `src/common`: shared constants, logging, and general utilities.
- `include/net` and `src/net`: socket runtime compatibility, endpoints, TCP client/server wrappers, UDP socket wrapper, low-level socket helpers, and protocol types.
- `include/discovery` and `src/discovery`: UDP discovery message helpers and peer models.
- `include/transfer` and `src/transfer`: future manifests, sessions, chunk transfer, checkpoint continuation, and verification logic.
- `docs/`: design notes and protocol documentation as the engine evolves.
- `tests/`: unit and integration tests.

## Current Status

Foundation/scaffold, reusable networking core, and UDP LAN discovery are in place. The project includes shared constants, logging, socket runtime setup, endpoint helpers, low-level socket utilities, TCP client/server abstractions, a UDP socket abstraction, discovery message parsing/building, and a live peer table in the listener.

File transfer behavior is not implemented yet.

## Discovery Usage

Start a listener:

```sh
./build/packetbridge_listener
```

Start a broadcaster with an optional peer name:

```sh
./build/packetbridge_broadcaster my-device
```

The broadcaster sends a UDP packet every 2 seconds on the configured discovery port. The listener accepts compatible packets, ignores malformed input, updates known peers, and periodically prints a live peer table.

## Build Instructions

```sh
cmake -S . -B build
cmake --build build
```

Run a placeholder executable:

```sh
./build/packetbridge_listener
```

On multi-configuration generators, such as Visual Studio, the executable may be under a configuration directory:

```sh
cmake --build build --config Debug
```

## Roadmap

1. Define binary serialization and parsing for transfer protocol messages.
2. Add transfer manifests and session lifecycle management.
3. Implement TCP sender and receiver chunk streaming.
4. Add ACK/checkpoint persistence and interrupted-transfer continuation.
5. Add SHA-256 integrity verification.
6. Expand automated tests for protocol, networking, and transfer workflows.
