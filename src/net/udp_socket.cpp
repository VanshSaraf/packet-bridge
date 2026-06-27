#include "net/udp_socket.hpp"

#include "common/logger.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <string>

#if defined(_WIN32)
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#endif

namespace packetbridge::net {
namespace {

Endpoint endpoint_from_sockaddr(const sockaddr_storage& storage) {
    char host[INET6_ADDRSTRLEN] = {};
    std::uint16_t port = 0;

    if (storage.ss_family == AF_INET) {
        const auto* address = reinterpret_cast<const sockaddr_in*>(&storage);
        inet_ntop(AF_INET, &address->sin_addr, host, sizeof(host));
        port = ntohs(address->sin_port);
    } else if (storage.ss_family == AF_INET6) {
        const auto* address = reinterpret_cast<const sockaddr_in6*>(&storage);
        inet_ntop(AF_INET6, &address->sin6_addr, host, sizeof(host));
        port = ntohs(address->sin6_port);
    }

    return Endpoint{host, port};
}

void log_socket_error(const std::string& action) {
    const int error = last_socket_error();
    common::error(action + ": " + socket_error_message(error));
}

}  // namespace

UdpSocket::~UdpSocket() {
    close();
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept : socket_(other.socket_) {
    other.socket_ = kInvalidSocket;
}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = other.socket_;
        other.socket_ = kInvalidSocket;
    }
    return *this;
}

bool UdpSocket::open() {
    close();
    socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!is_open()) {
        log_socket_error("UDP socket creation failed");
        return false;
    }
    return true;
}

bool UdpSocket::bind_to(std::uint16_t port) {
    if (!is_open() && !open()) {
        return false;
    }

    if (!set_reuse_address(socket_)) {
        log_socket_error("UDP set reuse address failed");
        close();
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (::bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        log_socket_error("UDP bind failed on port " + std::to_string(port));
        close();
        return false;
    }

    return true;
}

bool UdpSocket::enable_broadcast() {
    if (!is_open()) {
        common::error("UDP broadcast setup failed: socket is not open");
        return false;
    }

    const int enabled = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST,
                   reinterpret_cast<const char*>(&enabled), sizeof(enabled)) != 0) {
        log_socket_error("UDP broadcast setup failed");
        return false;
    }

    return true;
}

bool UdpSocket::set_receive_timeout(std::chrono::milliseconds timeout) {
    if (!is_open()) {
        common::error("UDP receive timeout setup failed: socket is not open");
        return false;
    }

#if defined(_WIN32)
    const DWORD timeout_ms = static_cast<DWORD>(timeout.count());
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms)) != 0) {
        log_socket_error("UDP receive timeout setup failed");
        return false;
    }
#else
    timeval value{};
    value.tv_sec = static_cast<time_t>(timeout.count() / 1000);
    value.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value)) != 0) {
        log_socket_error("UDP receive timeout setup failed");
        return false;
    }
#endif

    return true;
}

bool UdpSocket::send_to(const void* data, std::size_t size, const Endpoint& endpoint) {
    if (!is_open()) {
        common::error("UDP send failed: socket is not open");
        return false;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* results = nullptr;
    const std::string port = std::to_string(endpoint.port);
    const int resolve_result = getaddrinfo(endpoint.host.c_str(), port.c_str(), &hints, &results);
    if (resolve_result != 0) {
#if defined(_WIN32)
        common::error("UDP resolve failed for " + endpoint.to_string() + ": " +
                      socket_error_message(resolve_result));
#else
        common::error("UDP resolve failed for " + endpoint.to_string() + ": " +
                      gai_strerror(resolve_result));
#endif
        return false;
    }

    const auto* bytes = static_cast<const char*>(data);
    const int send_size = static_cast<int>(
        std::min<std::size_t>(size, static_cast<std::size_t>(std::numeric_limits<int>::max())));
    const int sent = ::sendto(socket_, bytes, send_size, 0, results->ai_addr,
                              static_cast<socket_length_t>(results->ai_addrlen));
    freeaddrinfo(results);

    if (sent < 0) {
        log_socket_error("UDP send failed to " + endpoint.to_string());
        return false;
    }

    if (static_cast<std::size_t>(sent) != size) {
        common::error("UDP send failed: datagram was only partially sent");
        return false;
    }

    return true;
}

bool UdpSocket::send_to(const std::vector<std::uint8_t>& data, const Endpoint& endpoint) {
    return send_to(data.data(), data.size(), endpoint);
}

UdpReceiveResult UdpSocket::receive_from(void* buffer, std::size_t size) {
    UdpReceiveResult result{};
    if (!is_open()) {
        common::error("UDP receive failed: socket is not open");
        return result;
    }

    sockaddr_storage remote_address{};
    socket_length_t remote_address_size = sizeof(remote_address);
    const int receive_size = static_cast<int>(
        std::min<std::size_t>(size, static_cast<std::size_t>(std::numeric_limits<int>::max())));
    const int received = ::recvfrom(
        socket_, static_cast<char*>(buffer), receive_size, 0,
        reinterpret_cast<sockaddr*>(&remote_address), &remote_address_size);

    if (received < 0) {
        const int error = last_socket_error();
        if (is_timeout_error(error)) {
            result.timed_out = true;
            return result;
        }
        log_socket_error("UDP receive failed");
        return result;
    }

    result.ok = true;
    result.bytes_received = static_cast<std::size_t>(received);
    result.remote = endpoint_from_sockaddr(remote_address);
    return result;
}

UdpReceiveResult UdpSocket::receive_from(std::vector<std::uint8_t>& buffer) {
    return receive_from(buffer.data(), buffer.size());
}

void UdpSocket::close() {
    if (is_open()) {
        close_socket(socket_);
        socket_ = kInvalidSocket;
    }
}

bool UdpSocket::is_open() const {
    return is_valid_socket(socket_);
}

socket_t UdpSocket::native_handle() const {
    return socket_;
}

}  // namespace packetbridge::net
