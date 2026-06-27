#pragma once

#include "discovery/peer.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace packetbridge::discovery {

std::string build_discovery_message(const std::string& peer_name, std::uint16_t transfer_port);
std::optional<Peer> parse_discovery_message(const std::string& message, const std::string& sender_ip);

}  // namespace packetbridge::discovery
