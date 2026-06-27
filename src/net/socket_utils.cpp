#include "net/socket_utils.hpp"

#include <cerrno>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <unistd.h>
#endif

namespace packetbridge::net {

bool is_valid_socket(socket_t socket) {
    return socket != kInvalidSocket;
}

void close_socket(socket_t socket) {
    if (!is_valid_socket(socket)) {
        return;
    }

#if defined(_WIN32)
    closesocket(socket);
#else
    ::close(socket);
#endif
}

int last_socket_error() {
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool is_timeout_error(int error_code) {
#if defined(_WIN32)
    return error_code == WSAETIMEDOUT || error_code == WSAEWOULDBLOCK;
#else
    return error_code == EAGAIN || error_code == EWOULDBLOCK;
#endif
}

std::string socket_error_message(int error_code) {
#if defined(_WIN32)
    return "socket error " + std::to_string(error_code);
#else
    return std::strerror(error_code);
#endif
}

bool set_reuse_address(socket_t socket) {
    const int enabled = 1;
    return setsockopt(
               socket,
               SOL_SOCKET,
               SO_REUSEADDR,
               reinterpret_cast<const char*>(&enabled),
               sizeof(enabled)) == 0;
}

}  // namespace packetbridge::net
