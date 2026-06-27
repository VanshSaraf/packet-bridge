#include "transfer/chunk_codec.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace packetbridge::transfer {
namespace {

constexpr std::uint32_t kChunkHeaderMagic = 0x50424348;  // PBCH
constexpr std::size_t kChunkHeaderSize =
    sizeof(std::uint32_t) + sizeof(std::uint64_t) + sizeof(std::uint64_t) +
    sizeof(std::uint64_t) + sizeof(std::uint32_t);

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

bool send_chunk_header(net::TcpSocket& socket, const net::protocol::ChunkHeader& header) {
    return socket.send_all(serialize_chunk_header(header));
}

bool receive_chunk_header(net::TcpSocket& socket, net::protocol::ChunkHeader& header) {
    std::vector<std::uint8_t> payload(kChunkHeaderSize);
    if (!socket.recv_exact(payload)) {
        return false;
    }

    return deserialize_chunk_header(payload, header);
}

std::vector<std::uint8_t> serialize_chunk_header(const net::protocol::ChunkHeader& header) {
    std::vector<std::uint8_t> payload;
    payload.reserve(kChunkHeaderSize);

    append_u32(payload, kChunkHeaderMagic);
    append_u64(payload, header.session_id);
    append_u64(payload, header.chunk_index);
    append_u64(payload, header.byte_offset);
    append_u32(payload, header.payload_size);
    return payload;
}

bool deserialize_chunk_header(const std::vector<std::uint8_t>& payload,
                              net::protocol::ChunkHeader& header) {
    if (payload.size() != kChunkHeaderSize) {
        return false;
    }

    std::size_t offset = 0;
    const std::uint32_t magic = read_u32(payload, offset);
    if (magic != kChunkHeaderMagic) {
        return false;
    }

    header.session_id = read_u64(payload, offset);
    header.chunk_index = read_u64(payload, offset);
    header.byte_offset = read_u64(payload, offset);
    header.payload_size = read_u32(payload, offset);
    return true;
}

bool validate_chunk_header(const net::protocol::FileManifest& manifest,
                           const net::protocol::ChunkHeader& header) {
    if (manifest.chunk_size == 0 ||
        header.chunk_index >= manifest.chunk_count ||
        header.payload_size == 0 ||
        header.payload_size > manifest.chunk_size ||
        header.session_id != manifest.session_id) {
        return false;
    }

    const std::uint64_t expected_offset = header.chunk_index * manifest.chunk_size;
    if (header.byte_offset != expected_offset ||
        header.byte_offset >= manifest.file_size ||
        header.byte_offset + header.payload_size > manifest.file_size) {
        return false;
    }

    const std::uint64_t expected_size =
        header.chunk_index + 1 == manifest.chunk_count
            ? manifest.file_size - header.byte_offset
            : manifest.chunk_size;
    return header.payload_size == expected_size;
}

}  // namespace packetbridge::transfer
