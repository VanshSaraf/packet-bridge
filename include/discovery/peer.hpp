#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace packetbridge::discovery {

struct Peer {
    std::string name;
    std::string ip_address;
    std::uint16_t transfer_port = 0;
    std::chrono::steady_clock::time_point last_seen;
    std::uint16_t protocol_version = 0;
};

}  // namespace packetbridge::discovery
