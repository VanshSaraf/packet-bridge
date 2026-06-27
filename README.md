# PacketBridge

PacketBridge is a Secure P2P LAN Transfer Engine planned around automatic local peer discovery, reliable chunked file transfer, checkpointed sessions, and SHA-256 integrity verification.

## Planned Features

- UDP-based automatic peer discovery on the local network.
- TCP-based reliable file transfer using fixed-size chunks.
- Custom binary protocol with versioned message types.
- Transfer sessions backed by file manifests.
- ACK/checkpoint-based continuation support for interrupted transfers.
- SHA-256 verification for received files.
- Clean modular C++17 implementation with CMake builds.

## Architecture Overview

- `apps/`: command-line entry points for listener, broadcaster, sender, and receiver roles.
- `include/common` and `src/common`: shared constants, logging, and general utilities.
- `include/net` and `src/net`: socket runtime compatibility, endpoints, TCP client/server wrappers, UDP socket wrapper, low-level socket helpers, and protocol types.
- `include/discovery` and `src/discovery`: UDP discovery message helpers and peer models.
- `include/transfer` and `src/transfer`: manifest serialization, checkpoint recovery, filename safety helpers, file sizing, chunk counting, and future transfer session logic.
- `docs/`: design notes and protocol documentation as the engine evolves.
- `tests/`: unit and integration tests.

## Current Status

Foundation/scaffold, reusable networking core, UDP LAN discovery, and chunked TCP file transfer v1 are in place. The project includes shared constants, logging, socket runtime setup, endpoint helpers, low-level socket utilities, TCP client/server abstractions, a UDP socket abstraction, discovery message parsing/building, a live peer table in the listener, manifest exchange, chunk headers, checkpoint-based interrupted-transfer continuation, progress metrics, byte-count verification, and SHA-256 integrity verification for received files.

Transfer data is not encrypted; SHA-256 verifies file integrity, not confidentiality.

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

## File Transfer Usage

Start the receiver first:

```sh
./build/packetbridge_receiver
```

Optionally choose an output directory:

```sh
./build/packetbridge_receiver ./received
```

From another terminal or device, send a file to the receiver IP:

```sh
./build/packetbridge_sender 192.168.1.25 ./example.bin
```

The sender connects to the default transfer TCP port, sends a binary file manifest, then streams chunks. Each chunk has a binary header with its index, byte offset, and payload size. After all chunks, the sender sends the expected SHA-256 digest. The receiver writes `received_<filename>`, validates each chunk header, reports progress with speed and ETA, verifies that the received byte count matches the manifest, computes SHA-256 locally, and compares it with the sender-provided digest.

If a transfer is interrupted, restart the receiver and sender with the same file and output directory. The receiver uses a `received_<filename>.pbcheckpoint` sidecar file to request only missing chunks. The checkpoint file is removed after successful SHA-256 verification.

Example receiver integrity output:

```text
Expected SHA-256: <64 lowercase hex characters>
Computed SHA-256: <64 lowercase hex characters>
SHA-256 integrity verified
```

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

1. Add ACK tracking for richer sender/receiver status.
2. Expand transfer session lifecycle management.
3. Add optional confidentiality features for transfer data.
4. Add automated tests for protocol, networking, and transfer workflows.
