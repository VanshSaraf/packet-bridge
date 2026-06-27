#pragma once

#include "net/socket_utils.hpp"
#include "net/tcp_socket.hpp"

#include <cstdint>

namespace packetbridge::net {

class TcpServer {
public:
    TcpServer() = default;
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    TcpServer(TcpServer&& other) noexcept;
    TcpServer& operator=(TcpServer&& other) noexcept;

    bool bind_and_listen(std::uint16_t port, int backlog = 16);
    TcpSocket accept_client();
    void close();

    bool is_open() const;
    socket_t native_handle() const;

private:
    socket_t socket_ = kInvalidSocket;
};

}  // namespace packetbridge::net
