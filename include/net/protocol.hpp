#pragma once

#include "common/constants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace packetbridge::net::protocol {

inline constexpr std::array<char, 4> kMagicBytes = {'P', 'B', 'R', 'G'};
inline constexpr std::uint16_t kVersion = common::kProtocolVersion;
inline constexpr std::size_t kSha256DigestSize = 32;

enum class MessageType : std::uint8_t {
    PeerAnnouncement = 1,
    FileManifest = 2,
    ChunkData = 3,
    ChunkAck = 4,
    ContinueRequest = 5,
    HashPacket = 6,
    Error = 255,
};

// Sent from sender to receiver before file bytes to describe the transfer plan.
struct FileManifest {
    std::string filename;
    std::uint64_t file_size = 0;
    std::uint32_t chunk_size = static_cast<std::uint32_t>(common::kDefaultChunkSize);
    std::uint64_t chunk_count = 0;
    std::array<std::uint8_t, kSha256DigestSize> file_sha256{};
    std::uint16_t protocol_version = kVersion;
    std::uint64_t session_id = 0;
};

// Prefixes each TCP payload chunk with its transfer and ordering metadata.
struct ChunkHeader {
    std::uint64_t session_id = 0;
    std::uint64_t chunk_index = 0;
    std::uint32_t payload_size = 0;
};

// Confirms durable receipt of chunks so a sender can checkpoint progress.
struct ChunkAck {
    std::uint64_t session_id = 0;
    std::uint64_t highest_contiguous_chunk = 0;
};

// Asks a peer to continue from the first missing chunk after interruption.
struct ContinueRequest {
    std::uint64_t session_id = 0;
    std::uint64_t next_required_chunk = 0;
};

// Carries a SHA-256 digest for whole-file or per-chunk integrity checks.
struct HashPacket {
    std::uint64_t session_id = 0;
    std::uint64_t chunk_index = 0;
    std::array<std::uint8_t, kSha256DigestSize> sha256{};
};

}  // namespace packetbridge::net::protocol
