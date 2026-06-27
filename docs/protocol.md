# PacketBridge Protocol

This document describes PacketBridge messages at a high level. The current protocol is intentionally small and readable.

## Discovery Message

Direction: broadcaster -> listener over UDP broadcast.

Purpose: announce a peer on the LAN.

Format:

```text
PACKETBRIDGE_DISCOVER|protocol_version|peer_name|transfer_port
```

Fields:

- marker: fixed text identifying PacketBridge discovery packets
- protocol_version: current protocol version
- peer_name: display name for the peer
- transfer_port: TCP port where the peer can receive files

Invalid markers, incompatible versions, invalid peer names, and invalid ports are ignored.

## File Manifest

Direction: sender -> receiver over TCP.

Purpose: describe the file before chunk data starts.

Fields:

- protocol version
- filename
- file size
- chunk size
- chunk count
- session placeholder

The receiver validates the manifest before creating output or checkpoint state.

## Continuation Request

Direction: receiver -> sender over TCP, immediately after the manifest.

Purpose: tell the sender which chunks are still needed.

Fields:

- session placeholder
- total chunks
- missing chunk indexes

For a fresh transfer, every chunk index is listed. For checkpoint continuation, only missing indexes are listed.

## Chunk Header

Direction: sender -> receiver over TCP, before each chunk payload.

Purpose: frame each chunk and let the receiver validate where it belongs.

Fields:

- session placeholder
- chunk index
- byte offset
- payload size

The receiver rejects invalid indexes, invalid sizes, offsets outside the manifest range, and chunks that exceed the declared file size.

## Chunk Payload

Direction: sender -> receiver over TCP, immediately after a chunk header.

Purpose: carry raw file bytes for the chunk described by the header.

The receiver writes the payload at the header's byte offset and updates the checkpoint after a successful write.

## Hash Packet

Direction: sender -> receiver over TCP, after all requested chunks.

Purpose: provide the expected SHA-256 digest for the complete file.

Fields:

- session placeholder
- SHA-256 digest as 64 lowercase hex characters

The receiver computes SHA-256 for the output file and compares it with the sender-provided digest.
