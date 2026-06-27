#include "net/tcp_socket.hpp"

#include "common/logger.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>

#if defined(_WIN32)
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace packetbridge::net {
namespace {

constexpr int kSocketFailure = -1;

void log_socket_error(const std::string& action) {
    const int error = last_socket_error();
    common::error(action + ": " + socket_error_message(error));
}

}  // namespace

TcpSocket::TcpSocket(socket_t socket) : socket_(socket) {}

TcpSocket::~TcpSocket() {
    close();
}

TcpSocket::TcpSocket(TcpSocket&& other) noexcept : socket_(other.socket_) {
    other.socket_ = kInvalidSocket;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = other.socket_;
        other.socket_ = kInvalidSocket;
    }
    return *this;
}

bool TcpSocket::connect_to(const Endpoint& endpoint) {
    close();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* results = nullptr;
    const std::string port = std::to_string(endpoint.port);
    const int resolve_result = getaddrinfo(endpoint.host.c_str(), port.c_str(), &hints, &results);
    if (resolve_result != 0) {
#if defined(_WIN32)
        common::error("TCP resolve failed for " + endpoint.to_string() + ": " +
                      socket_error_message(resolve_result));
#else
        common::error("TCP resolve failed for " + endpoint.to_string() + ": " +
                      gai_strerror(resolve_result));
#endif
        return false;
    }

    for (addrinfo* candidate = results; candidate != nullptr; candidate = candidate->ai_next) {
        socket_t candidate_socket =
            ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
        if (!is_valid_socket(candidate_socket)) {
            continue;
        }

        if (::connect(candidate_socket, candidate->ai_addr,
                      static_cast<socket_length_t>(candidate->ai_addrlen)) == 0) {
            socket_ = candidate_socket;
            freeaddrinfo(results);
            return true;
        }

        close_socket(candidate_socket);
    }

    freeaddrinfo(results);
    log_socket_error("TCP connect failed for " + endpoint.to_string());
    return false;
}

bool TcpSocket::send_all(const void* data, std::size_t size) {
    if (!is_open()) {
        common::error("TCP send failed: socket is not open");
        return false;
    }

    const auto* bytes = static_cast<const char*>(data);
    std::size_t total_sent = 0;

    while (total_sent < size) {
        const std::size_t remaining = size - total_sent;
        const int chunk_size = static_cast<int>(
            std::min<std::size_t>(remaining, static_cast<std::size_t>(std::numeric_limits<int>::max())));
        const int sent = ::send(socket_, bytes + total_sent, chunk_size, 0);
        if (sent == kSocketFailure) {
            log_socket_error("TCP send failed");
            return false;
        }
        if (sent == 0) {
            common::error("TCP send failed: connection closed");
            return false;
        }
        total_sent += static_cast<std::size_t>(sent);
    }

    return true;
}

bool TcpSocket::send_all(const std::vector<std::uint8_t>& data) {
    return send_all(data.data(), data.size());
}

bool TcpSocket::recv_exact(void* data, std::size_t size) {
    if (!is_open()) {
        common::error("TCP receive failed: socket is not open");
        return false;
    }

    auto* bytes = static_cast<char*>(data);
    std::size_t total_received = 0;

    while (total_received < size) {
        const std::size_t remaining = size - total_received;
        const int chunk_size = static_cast<int>(
            std::min<std::size_t>(remaining, static_cast<std::size_t>(std::numeric_limits<int>::max())));
        const int received = ::recv(socket_, bytes + total_received, chunk_size, 0);
        if (received == kSocketFailure) {
            log_socket_error("TCP receive failed");
            return false;
        }
        if (received == 0) {
            common::error("TCP receive failed: connection closed");
            return false;
        }
        total_received += static_cast<std::size_t>(received);
    }

    return true;
}

bool TcpSocket::recv_exact(std::vector<std::uint8_t>& data) {
    return recv_exact(data.data(), data.size());
}

void TcpSocket::close() {
    if (is_open()) {
        close_socket(socket_);
        socket_ = kInvalidSocket;
    }
}

bool TcpSocket::is_open() const {
    return is_valid_socket(socket_);
}

socket_t TcpSocket::native_handle() const {
    return socket_;
}

}  // namespace packetbridge::net
