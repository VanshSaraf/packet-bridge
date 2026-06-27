#pragma once

#include <cstdint>
#include <string>

namespace packetbridge::net {

struct Endpoint {
    std::string host;
    std::uint16_t port = 0;

    std::string to_string() const;
};

}  // namespace packetbridge::net
