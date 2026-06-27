#include "common/logger.hpp"
#include "net/socket_runtime.hpp"

int main() {
    packetbridge::net::SocketRuntime socket_runtime;
    packetbridge::common::info(
        "PacketBridge sender placeholder - networking core is ready; transfer sessions will be implemented next.");
    return 0;
}
