#pragma once

#include "net/protocol.hpp"
#include "net/tcp_socket.hpp"

#include <string>

namespace packetbridge::transfer {

bool send_hash_packet(net::TcpSocket& socket, const net::protocol::HashPacket& packet);
bool receive_hash_packet(net::TcpSocket& socket, net::protocol::HashPacket& packet);

}  // namespace packetbridge::transfer
