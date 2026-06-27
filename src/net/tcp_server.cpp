#include "net/tcp_server.hpp"

#include "common/logger.hpp"

#include <string>

#if defined(_WIN32)
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace packetbridge::net {
namespace {

void log_socket_error(const std::string& action) {
    const int error = last_socket_error();
    common::error(action + ": " + socket_error_message(error));
}

}  // namespace

TcpServer::~TcpServer() {
    close();
}

TcpServer::TcpServer(TcpServer&& other) noexcept : socket_(other.socket_) {
    other.socket_ = kInvalidSocket;
}

TcpServer& TcpServer::operator=(TcpServer&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = other.socket_;
        other.socket_ = kInvalidSocket;
    }
    return *this;
}

bool TcpServer::bind_and_listen(std::uint16_t port, int backlog) {
    close();

    socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!is_open()) {
        log_socket_error("TCP server socket creation failed");
        return false;
    }

    if (!set_reuse_address(socket_)) {
        log_socket_error("TCP server set reuse address failed");
        close();
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (::bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        log_socket_error("TCP server bind failed on port " + std::to_string(port));
        close();
        return false;
    }

    if (::listen(socket_, backlog) != 0) {
        log_socket_error("TCP server listen failed on port " + std::to_string(port));
        close();
        return false;
    }

    return true;
}

TcpSocket TcpServer::accept_client() {
    if (!is_open()) {
        common::error("TCP accept failed: server socket is not open");
        return TcpSocket{};
    }

    sockaddr_storage client_address{};
    socket_length_t client_address_size = sizeof(client_address);
    socket_t client_socket = ::accept(
        socket_, reinterpret_cast<sockaddr*>(&client_address), &client_address_size);
    if (!is_valid_socket(client_socket)) {
        log_socket_error("TCP accept failed");
        return TcpSocket{};
    }

    return TcpSocket{client_socket};
}

void TcpServer::close() {
    if (is_open()) {
        close_socket(socket_);
        socket_ = kInvalidSocket;
    }
}

bool TcpServer::is_open() const {
    return is_valid_socket(socket_);
}

socket_t TcpServer::native_handle() const {
    return socket_;
}

}  // namespace packetbridge::net
