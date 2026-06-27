#pragma once

#include "net/protocol.hpp"
#include "net/tcp_socket.hpp"

namespace packetbridge::transfer {

bool send_chunk_header(net::TcpSocket& socket, const net::protocol::ChunkHeader& header);
bool receive_chunk_header(net::TcpSocket& socket, net::protocol::ChunkHeader& header);
bool validate_chunk_header(const net::protocol::FileManifest& manifest,
                           const net::protocol::ChunkHeader& header);

}  // namespace packetbridge::transfer
