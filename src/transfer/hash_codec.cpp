#include "transfer/hash_codec.hpp"

#include "crypto/sha256.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace packetbridge::transfer {
namespace {

constexpr std::uint32_t kHashPacketMagic = 0x50424853;  // PBHS
constexpr std::size_t kHashPacketSize =
    sizeof(std::uint32_t) + sizeof(std::uint64_t) + 64;

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xff));
    }
}

void append_u64(std::vector<std::uint8_t>& output, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xff));
    }
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& input, std::size_t& offset) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value = (value << 8) | input[offset++];
    }
    return value;
}

std::uint64_t read_u64(const std::vector<std::uint8_t>& input, std::size_t& offset) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | input[offset++];
    }
    return value;
}

}  // namespace

bool send_hash_packet(net::TcpSocket& socket, const net::protocol::HashPacket& packet) {
    if (!crypto::is_sha256_hex(packet.sha256_hex)) {
        return false;
    }

    std::vector<std::uint8_t> payload;
    payload.reserve(kHashPacketSize);
    append_u32(payload, kHashPacketMagic);
    append_u64(payload, packet.session_id);
    payload.insert(payload.end(), packet.sha256_hex.begin(), packet.sha256_hex.end());
    return socket.send_all(payload);
}

bool receive_hash_packet(net::TcpSocket& socket, net::protocol::HashPacket& packet) {
    std::vector<std::uint8_t> payload(kHashPacketSize);
    if (!socket.recv_exact(payload)) {
        return false;
    }

    std::size_t offset = 0;
    const std::uint32_t magic = read_u32(payload, offset);
    if (magic != kHashPacketMagic) {
        return false;
    }

    packet.session_id = read_u64(payload, offset);
    packet.sha256_hex = std::string(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                                    payload.end());
    return crypto::is_sha256_hex(packet.sha256_hex);
}

}  // namespace packetbridge::transfer
