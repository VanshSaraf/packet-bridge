#pragma once

#include "net/protocol.hpp"
#include "net/tcp_socket.hpp"

#include <cstdint>
#include <vector>

namespace packetbridge::transfer {

std::vector<std::uint8_t> serialize_file_manifest(const net::protocol::FileManifest& manifest);
bool deserialize_file_manifest(const std::vector<std::uint8_t>& payload,
                               net::protocol::FileManifest& manifest);
bool send_file_manifest(net::TcpSocket& socket, const net::protocol::FileManifest& manifest);
bool receive_file_manifest(net::TcpSocket& socket, net::protocol::FileManifest& manifest);

}  // namespace packetbridge::transfer
