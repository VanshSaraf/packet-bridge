#include "net/socket_runtime.hpp"

#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#endif

namespace packetbridge::net {

SocketRuntime::SocketRuntime() {
#if defined(_WIN32)
    WSADATA data{};
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        throw std::runtime_error("WSAStartup failed with code " + std::to_string(result));
    }
#endif
}

SocketRuntime::~SocketRuntime() {
#if defined(_WIN32)
    WSACleanup();
#endif
}

}  // namespace packetbridge::net
