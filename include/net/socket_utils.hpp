#pragma once

#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

namespace packetbridge::net {

#if defined(_WIN32)
using socket_t = SOCKET;
using socket_length_t = int;
inline constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
using socket_t = int;
using socket_length_t = socklen_t;
inline constexpr socket_t kInvalidSocket = -1;
#endif

bool is_valid_socket(socket_t socket);
void close_socket(socket_t socket);
int last_socket_error();
bool is_timeout_error(int error_code);
std::string socket_error_message(int error_code);
bool set_reuse_address(socket_t socket);

}  // namespace packetbridge::net
