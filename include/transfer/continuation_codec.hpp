#pragma once

#include "net/protocol.hpp"
#include "net/tcp_socket.hpp"

namespace packetbridge::transfer {

bool send_continue_request(net::TcpSocket& socket, const net::protocol::ContinueRequest& request);
bool receive_continue_request(net::TcpSocket& socket, net::protocol::ContinueRequest& request);
bool validate_continue_request(const net::protocol::FileManifest& manifest,
                               const net::protocol::ContinueRequest& request);

}  // namespace packetbridge::transfer
