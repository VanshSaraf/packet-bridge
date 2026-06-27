#pragma once

#include "net/protocol.hpp"
#include "net/tcp_socket.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace packetbridge::transfer {

std::vector<std::uint8_t> serialize_hash_packet(const net::protocol::HashPacket& packet);
bool deserialize_hash_packet(const std::vector<std::uint8_t>& payload,
                             net::protocol::HashPacket& packet);
bool send_hash_packet(net::TcpSocket& socket, const net::protocol::HashPacket& packet);
bool receive_hash_packet(net::TcpSocket& socket, net::protocol::HashPacket& packet);

}  // namespace packetbridge::transfer
