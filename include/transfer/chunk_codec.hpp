#pragma once

#include "net/protocol.hpp"
#include "net/tcp_socket.hpp"

#include <cstdint>
#include <vector>

namespace packetbridge::transfer {

std::vector<std::uint8_t> serialize_chunk_header(const net::protocol::ChunkHeader& header);
bool deserialize_chunk_header(const std::vector<std::uint8_t>& payload,
                              net::protocol::ChunkHeader& header);
bool send_chunk_header(net::TcpSocket& socket, const net::protocol::ChunkHeader& header);
bool receive_chunk_header(net::TcpSocket& socket, net::protocol::ChunkHeader& header);
bool validate_chunk_header(const net::protocol::FileManifest& manifest,
                           const net::protocol::ChunkHeader& header);

}  // namespace packetbridge::transfer
