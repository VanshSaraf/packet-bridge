#pragma once

#include "common/constants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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

// Sender -> receiver: sent before chunk data to describe the transfer plan.
struct FileManifest {
    std::string filename;
    std::uint64_t file_size = 0;
    std::uint32_t chunk_size = static_cast<std::uint32_t>(common::kDefaultChunkSize);
    std::uint64_t chunk_count = 0;
    std::array<std::uint8_t, kSha256DigestSize> file_sha256{};
    std::uint16_t protocol_version = kVersion;
    std::uint64_t session_id = 0;
};

// Sender -> receiver: sent before each chunk payload so the receiver can
// validate ordering, size, and write offset before accepting bytes.
struct ChunkHeader {
    std::uint64_t session_id = 0;
    std::uint64_t chunk_index = 0;
    std::uint64_t byte_offset = 0;
    std::uint32_t payload_size = 0;
};

// Receiver -> sender: reserved for future durable chunk receipt tracking.
struct ChunkAck {
    std::uint64_t session_id = 0;
    std::uint64_t highest_contiguous_chunk = 0;
};

// Receiver -> sender: lists chunks still needed for interrupted-transfer continuation.
struct ContinueRequest {
    std::uint64_t session_id = 0;
    std::uint64_t total_chunks = 0;
    std::vector<std::uint64_t> missing_chunk_indexes;
};

// Sender -> receiver: sent after all chunks to carry the expected file digest.
struct HashPacket {
    std::uint64_t session_id = 0;
    std::string sha256_hex;
};

}  // namespace packetbridge::net::protocol
