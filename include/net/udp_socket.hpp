#pragma once

#include "net/endpoint.hpp"
#include "net/socket_utils.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace packetbridge::net {

struct UdpReceiveResult {
    bool ok = false;
    bool timed_out = false;
    std::size_t bytes_received = 0;
    Endpoint remote;
};

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    bool open();
    bool bind_to(std::uint16_t port);
    bool enable_broadcast();
    bool set_receive_timeout(std::chrono::milliseconds timeout);
    bool send_to(const void* data, std::size_t size, const Endpoint& endpoint);
    bool send_to(const std::vector<std::uint8_t>& data, const Endpoint& endpoint);
    UdpReceiveResult receive_from(void* buffer, std::size_t size);
    UdpReceiveResult receive_from(std::vector<std::uint8_t>& buffer);
    void close();

    bool is_open() const;
    socket_t native_handle() const;

private:
    socket_t socket_ = kInvalidSocket;
};

}  // namespace packetbridge::net
