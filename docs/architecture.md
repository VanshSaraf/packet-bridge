# PacketBridge Architecture Notes

PacketBridge is organized as a small core library plus role-specific command-line applications.

The networking layer owns cross-platform socket setup, endpoint conversion, low-level socket helpers, TCP client/server wrappers, and a UDP socket wrapper. Discovery uses UDP broadcast packets to announce local peers, while transfer uses TCP sessions for manifest exchange and file byte streaming. Future protocol work will add acknowledgements, checkpoint requests, and hash verification packets.

The current TCP wrappers provide connect, listen, accept, send-all, and receive-exact behavior with RAII cleanup. The UDP wrapper provides open, bind, broadcast enablement, receive timeouts, send-to, and receive-from behavior. Transfer v1 uses a compact binary manifest followed by raw file bytes.

## Discovery Flow

The broadcaster builds a text packet in the form `PACKETBRIDGE_DISCOVER|protocol_version|peer_name|transfer_port` and sends it to the LAN broadcast address every 2 seconds. The listener binds to the configured UDP discovery port, validates incoming packets, ignores malformed or incompatible messages, and stores peers by IP address plus transfer port.

The listener keeps a live peer table with each peer name, sender IP address, advertised transfer port, protocol version, and last-seen time. Entries are removed after they have not been observed for more than 10 seconds.

## Transfer Flow

The receiver binds to the default transfer TCP port and accepts one sender connection. The sender connects to the receiver IP, sends a binary file manifest with protocol version, filename, file size, chunk size, chunk count, and a placeholder session id, then streams the file bytes.

The receiver sanitizes the incoming filename, writes the output as `received_<filename>`, reads exactly the declared byte count, and reports whether the received count matches the manifest.

This document is intentionally brief for the foundation stage and should grow alongside the protocol and session implementations.
