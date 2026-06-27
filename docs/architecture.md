# PacketBridge Architecture

PacketBridge is a compact C++17 networking project built around direct LAN peer discovery and file transfer. The codebase favors simple synchronous control flow, explicit protocol messages, and small reusable modules.

## Module Layout

```text
apps/
  listener.cpp       UDP discovery listener
  broadcaster.cpp   UDP discovery announcer
  receiver.cpp      TCP file receiver
  sender.cpp        TCP file sender

include/ and src/
  common/           constants and logger
  net/              socket runtime, TCP/UDP wrappers, protocol structs
  discovery/        peer model and discovery message helpers
  transfer/         manifest, chunk, checkpoint, hash, and progress helpers
  crypto/           SHA-256 implementation
```

## Discovery Architecture

Discovery is decentralized. A broadcaster sends a UDP announcement to the LAN broadcast address every 2 seconds:

```text
PACKETBRIDGE_DISCOVER|protocol_version|peer_name|transfer_port
```

The listener binds to the configured discovery port, validates each packet, and stores live peers by IP address plus transfer port. Peer entries expire after 10 seconds without a fresh announcement.

```text
broadcaster -> UDP broadcast -> listener
listener -> parse and validate
listener -> update peer table
listener -> print live peer list
```

## Transfer Architecture

File transfer uses one TCP connection from sender to receiver. The receiver listens on the default transfer port and accepts one sender connection per process run.

```text
sender -> manifest -> receiver
receiver -> continuation request -> sender
sender -> chunk header -> receiver
sender -> chunk payload -> receiver
sender -> hash packet -> receiver
receiver -> verify output
```

The manifest describes the transfer before bytes move:

- protocol version
- filename
- file size
- chunk size
- chunk count
- session placeholder

Each chunk is framed with a header before its payload:

- session placeholder
- chunk index
- byte offset
- payload size

The receiver validates the chunk header before accepting payload bytes and writes the payload at the declared offset.

## Checkpoint Architecture

Checkpoint continuation is receiver-driven. After the manifest arrives, the receiver checks for a matching sidecar file:

```text
received_<filename>.pbcheckpoint
```

The checkpoint records:

- original filename
- file size
- chunk size
- total chunks
- completed chunk indexes

If the checkpoint matches the manifest and the output file exists, the receiver sends a continuation request listing missing chunk indexes. The sender skips chunks already present on the receiver.

```text
receiver -> load checkpoint
receiver -> compute missing chunks
receiver -> send missing chunk list
sender -> send only requested chunks
receiver -> update checkpoint after each chunk
receiver -> remove checkpoint after successful SHA-256 verification
```

## Integrity Verification

PacketBridge uses SHA-256 to verify file integrity after transfer:

```text
sender -> compute SHA-256 for input
sender -> send chunks
sender -> send hash packet
receiver -> compute SHA-256 for output
receiver -> compare expected and computed hashes
```

SHA-256 detects accidental corruption or mismatched content. It does not encrypt data and does not provide confidentiality.

## Protocol Sequence

Fresh transfer:

```text
sender                              receiver
  | -------- manifest ------------> |
  | <--- missing chunks: all ------- |
  | -------- chunk header --------> |
  | -------- chunk payload -------> |
  |              ...                |
  | -------- hash packet ---------> |
  |                                 | verify SHA-256
```

Checkpoint continuation:

```text
sender                              receiver
  | -------- manifest ------------> |
  | <--- missing chunks: subset ---- |
  | ---- requested chunk header ---> |
  | ---- requested chunk payload --> |
  |              ...                |
  | -------- hash packet ---------> |
  |                                 | verify and delete checkpoint
```

## Design Tradeoffs

- Synchronous sockets keep the control flow easy to understand.
- TCP provides reliable byte delivery, while PacketBridge adds application-level framing.
- The receiver owns checkpoint state because it owns the partially written output file.
- Human-readable checkpoint files make local inspection easy during development.
- SHA-256 is implemented in-repo to avoid external dependencies.

## Future Work

- Automated tests for codecs, checkpoint loading, and transfer loops.
- Richer acknowledgement messages for sender/receiver status.
- Multiple concurrent transfers.
- Optional confidentiality features.
- Better local interface detection for discovery self-filtering.
