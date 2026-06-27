#pragma once

#include "net/protocol.hpp"
#include "net/tcp_socket.hpp"

namespace packetbridge::transfer {

bool send_file_manifest(net::TcpSocket& socket, const net::protocol::FileManifest& manifest);
bool receive_file_manifest(net::TcpSocket& socket, net::protocol::FileManifest& manifest);

}  // namespace packetbridge::transfer
