#pragma once

namespace packetbridge::net {

class SocketRuntime {
public:
    SocketRuntime();
    ~SocketRuntime();

    SocketRuntime(const SocketRuntime&) = delete;
    SocketRuntime& operator=(const SocketRuntime&) = delete;

    SocketRuntime(SocketRuntime&&) = delete;
    SocketRuntime& operator=(SocketRuntime&&) = delete;
};

}  // namespace packetbridge::net
