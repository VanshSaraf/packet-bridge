#include "discovery/discovery_message.hpp"

#include "common/constants.hpp"

#include <chrono>
#include <sstream>
#include <string>
#include <vector>

namespace packetbridge::discovery {
namespace {

constexpr const char* kDiscoveryMarker = "PACKETBRIDGE_DISCOVER";

std::vector<std::string> split_message(const std::string& message) {
    std::vector<std::string> parts;
    std::stringstream stream(message);
    std::string part;

    while (std::getline(stream, part, '|')) {
        parts.push_back(part);
    }

    return parts;
}

std::optional<unsigned long> parse_unsigned(const std::string& value) {
    if (value.empty()) {
        return std::nullopt;
    }

    for (const char ch : value) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
    }

    try {
        return std::stoul(value);
    } catch (...) {
        return std::nullopt;
    }
}

bool is_valid_peer_name(const std::string& peer_name) {
    return !peer_name.empty() &&
           peer_name.size() <= common::kMaxPeerNameLength &&
           peer_name.find('|') == std::string::npos;
}

}  // namespace

std::string build_discovery_message(const std::string& peer_name, std::uint16_t transfer_port) {
    return std::string(kDiscoveryMarker) + "|" +
           std::to_string(common::kProtocolVersion) + "|" +
           peer_name + "|" +
           std::to_string(transfer_port);
}

std::optional<Peer> parse_discovery_message(const std::string& message, const std::string& sender_ip) {
    const std::vector<std::string> parts = split_message(message);
    if (parts.size() != 4 || parts[0] != kDiscoveryMarker) {
        return std::nullopt;
    }

    const auto version = parse_unsigned(parts[1]);
    if (!version || *version != common::kProtocolVersion) {
        return std::nullopt;
    }

    if (!is_valid_peer_name(parts[2])) {
        return std::nullopt;
    }

    const auto transfer_port = parse_unsigned(parts[3]);
    if (!transfer_port || *transfer_port == 0 || *transfer_port > 65535) {
        return std::nullopt;
    }

    Peer peer;
    peer.name = parts[2];
    peer.ip_address = sender_ip;
    peer.transfer_port = static_cast<std::uint16_t>(*transfer_port);
    peer.last_seen = std::chrono::steady_clock::now();
    peer.protocol_version = static_cast<std::uint16_t>(*version);
    return peer;
}

}  // namespace packetbridge::discovery
