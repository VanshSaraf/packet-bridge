#pragma once

#include "net/endpoint.hpp"
#include "net/socket_utils.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace packetbridge::net {

class TcpSocket {
public:
    TcpSocket() = default;
    explicit TcpSocket(socket_t socket);
    ~TcpSocket();

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;

    bool connect_to(const Endpoint& endpoint);
    bool send_all(const void* data, std::size_t size);
    bool send_all(const std::vector<std::uint8_t>& data);
    bool recv_exact(void* data, std::size_t size);
    bool recv_exact(std::vector<std::uint8_t>& data);
    void close();

    bool is_open() const;
    socket_t native_handle() const;

private:
    socket_t socket_ = kInvalidSocket;
};

}  // namespace packetbridge::net
