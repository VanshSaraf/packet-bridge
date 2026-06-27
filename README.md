# PacketBridge

PacketBridge is a C++17 peer-to-peer LAN transfer engine for discovering nearby peers and moving files directly across a local network. It combines UDP-based peer discovery, TCP-based chunked file transfer, checkpoint continuation, and SHA-256 integrity verification in a small, readable systems-programming codebase.

## Features

| Area | Status | Notes |
| --- | --- | --- |
| UDP LAN discovery | Implemented | Broadcast announcements with a live peer table. |
| TCP file transfer | Implemented | Receiver listens on a fixed transfer port; sender connects directly. |
| File manifest | Implemented | Filename, size, chunk size, chunk count, protocol version, and session placeholder. |
| Chunked transfer | Implemented | Per-chunk headers include index, byte offset, and payload size. |
| Checkpoint continuation | Implemented | Receiver requests only missing chunks when a matching checkpoint exists. |
| Progress metrics | Implemented | Progress bar, chunks, speed, ETA, elapsed time, and summaries. |
| SHA-256 verification | Implemented | Sender sends expected digest; receiver hashes output and compares. |
| Encryption | Not implemented | SHA-256 verifies integrity, not confidentiality. |
| Automated tests | Implemented | Lightweight assert-based codec and utility tests. |

## Architecture Overview

PacketBridge is organized as a small core library plus four command-line apps:

- `packetbridge_listener`: listens for UDP discovery announcements and prints live peers.
- `packetbridge_broadcaster`: broadcasts this device as an available PacketBridge peer.
- `packetbridge_receiver`: listens for one TCP file transfer and writes the received file.
- `packetbridge_sender`: connects to a receiver and transfers a file.

Core modules:

- `common`: logging and shared constants.
- `net`: cross-platform socket runtime, socket helpers, endpoint helpers, TCP wrappers, UDP wrapper, and protocol structs.
- `discovery`: discovery message builder/parser and peer model.
- `transfer`: manifests, chunk headers, checkpoint files, SHA-256 hash packet handling, progress tracking, and file utilities.
- `crypto`: self-contained SHA-256 file hashing.

## How It Works

Discovery uses a simple UDP text packet:

```text
PACKETBRIDGE_DISCOVER|protocol_version|peer_name|transfer_port
```

The listener validates incoming announcements, ignores malformed packets, filters incompatible protocol versions, and maintains a peer table keyed by IP address plus transfer port.

Transfer uses a direct TCP connection:

```text
sender -> manifest -> receiver
receiver -> continuation request with missing chunk indexes -> sender
sender -> chunk header -> receiver
sender -> chunk bytes -> receiver
repeat until requested chunks are complete
sender -> SHA-256 hash packet -> receiver
receiver -> local SHA-256 comparison and checkpoint cleanup
```

If a transfer stops early, the receiver keeps a human-readable `received_<filename>.pbcheckpoint` sidecar file. Restart the receiver and sender with the same file and output directory; the receiver asks for only missing chunks. After successful SHA-256 verification, the checkpoint file is removed.

## Build

```sh
cmake -S . -B build
cmake --build build
```

On multi-configuration generators, such as Visual Studio:

```sh
cmake --build build --config Debug
```

The build produces:

```text
packetbridge_listener
packetbridge_broadcaster
packetbridge_receiver
packetbridge_sender
packetbridge_tests
```

Run tests:

```sh
ctest --test-dir build --output-on-failure
```

## Usage Guide

Start discovery listener:

```sh
./build/packetbridge_listener
```

Start discovery broadcaster:

```sh
./build/packetbridge_broadcaster laptop-one
```

Start a file receiver:

```sh
./build/packetbridge_receiver ./received
```

Send a file to a receiver:

```sh
./build/packetbridge_sender 192.168.1.25 ./example.bin
```

For local testing, use loopback:

```sh
./build/packetbridge_receiver /tmp/packetbridge-out
./build/packetbridge_sender 127.0.0.1 ./example.bin
```

Each app supports `--help`:

```sh
./build/packetbridge_sender --help
```

## Example Output

Sender:

```text
PacketBridge sender
Connecting to receiver 127.0.0.1:47111
Total chunks: 4
Chunks already present on receiver: 0
Chunks to send: 4
[####################] 100.0% | 4/4 chunks | 120.00 MB/s | ETA 00:00
Expected SHA-256: <64 lowercase hex characters>
Transfer summary: elapsed 00:00, average 100.00 MB/s
```

Receiver:

```text
PacketBridge receiver listening on TCP port 47111
Incoming file: example.bin
Chunks requested: 4
SHA-256 integrity verified
Transfer complete: ./received/received_example.bin
```

## Protocol Flow

Discovery:

```text
broadcaster -> UDP broadcast announcement -> listener
listener -> validates marker, version, peer name, and transfer port
listener -> updates peer table and last-seen time
```

Transfer:

```text
sender -> binary file manifest -> receiver
receiver -> missing chunk list -> sender
sender -> chunk header and chunk payload -> receiver
receiver -> writes payload at declared byte offset
sender -> hash packet -> receiver
receiver -> computes local SHA-256 and compares
```

Checkpoint continuation:

```text
receiver -> loads matching checkpoint, if present
receiver -> sends only missing chunk indexes
sender -> skips chunks already present on receiver
receiver -> updates checkpoint after each written chunk
receiver -> removes checkpoint after successful integrity verification
```

## Project Structure

```text
apps/                 Command-line entry points
include/common/       Shared constants and logging headers
include/net/          Socket and protocol headers
include/discovery/    Discovery models and message helpers
include/transfer/     Transfer helpers and codecs
include/crypto/       SHA-256 interface
src/                  Implementations
docs/                 Architecture, protocol, and testing notes
tests/                Placeholder for future automated tests
```

## Current Limitations

- Transfer data is not encrypted.
- Receiver accepts one sender connection per process run.
- Discovery uses IPv4 broadcast by default.
- Checkpoint files trust local disk state and validate completion with final SHA-256.
- Discovery and transfer still run as separate executables.
- Sender target IP is provided manually.

## Troubleshooting

- If discovery shows no peers, check local firewall settings and that UDP port `47110` is allowed.
- If file transfer cannot connect, make sure the receiver is running and TCP port `47111` is reachable.
- If the receiver reports an integrity failure, keep the output file for inspection and retry the transfer.
- If CMake is unavailable, install CMake or use an IDE with CMake support.
- On Windows, the project links Winsock2 automatically through CMake.

## Roadmap

1. Add richer sender/receiver status using acknowledgement messages.
2. Expand transfer session lifecycle management.
3. Add optional confidentiality features for transfer data.
4. Add broader integration coverage.
5. Add packaging and release artifacts.

## License

PacketBridge is released under the MIT License. See [LICENSE](LICENSE).
