#include "net/endpoint.hpp"

#include <string>

namespace packetbridge::net {

std::string Endpoint::to_string() const {
    return host + ":" + std::to_string(port);
}

}  // namespace packetbridge::net
