#include "transfer/continuation_codec.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace packetbridge::transfer {
namespace {

constexpr std::uint32_t kContinueRequestMagic = 0x50424352;  // PBCR
constexpr std::uint64_t kMaxChunkIndexes = 1000000;

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

bool send_continue_request(net::TcpSocket& socket, const net::protocol::ContinueRequest& request) {
    return socket.send_all(serialize_continue_request(request));
}

bool receive_continue_request(net::TcpSocket& socket, net::protocol::ContinueRequest& request) {
    constexpr std::size_t fixed_size = sizeof(std::uint32_t) + 3 * sizeof(std::uint64_t);
    std::vector<std::uint8_t> fixed_payload(fixed_size);
    if (!socket.recv_exact(fixed_payload)) {
        return false;
    }

    std::size_t offset = sizeof(std::uint32_t) + 2 * sizeof(std::uint64_t);
    const std::uint64_t missing_count = read_u64(fixed_payload, offset);
    if (missing_count > kMaxChunkIndexes) {
        return false;
    }

    std::vector<std::uint8_t> payload = fixed_payload;
    const std::size_t index_bytes = static_cast<std::size_t>(missing_count * sizeof(std::uint64_t));
    payload.resize(fixed_size + index_bytes);
    if (index_bytes > 0 && !socket.recv_exact(payload.data() + fixed_size, index_bytes)) {
        return false;
    }

    return deserialize_continue_request(payload, request);
}

std::vector<std::uint8_t> serialize_continue_request(
    const net::protocol::ContinueRequest& request) {
    std::vector<std::uint8_t> payload;
    payload.reserve(sizeof(std::uint32_t) + (3 + request.missing_chunk_indexes.size()) *
                                            sizeof(std::uint64_t));

    append_u32(payload, kContinueRequestMagic);
    append_u64(payload, request.session_id);
    append_u64(payload, request.total_chunks);
    append_u64(payload, static_cast<std::uint64_t>(request.missing_chunk_indexes.size()));
    for (const std::uint64_t index : request.missing_chunk_indexes) {
        append_u64(payload, index);
    }

    return payload;
}

bool deserialize_continue_request(const std::vector<std::uint8_t>& payload,
                                  net::protocol::ContinueRequest& request) {
    constexpr std::size_t fixed_size = sizeof(std::uint32_t) + 3 * sizeof(std::uint64_t);
    if (payload.size() < fixed_size) {
        return false;
    }

    std::size_t offset = 0;
    const std::uint32_t magic = read_u32(payload, offset);
    if (magic != kContinueRequestMagic) {
        return false;
    }

    request.session_id = read_u64(payload, offset);
    request.total_chunks = read_u64(payload, offset);
    const std::uint64_t missing_count = read_u64(payload, offset);
    if (missing_count > kMaxChunkIndexes ||
        payload.size() != fixed_size + missing_count * sizeof(std::uint64_t)) {
        return false;
    }

    request.missing_chunk_indexes.clear();
    request.missing_chunk_indexes.reserve(static_cast<std::size_t>(missing_count));
    for (std::uint64_t i = 0; i < missing_count; ++i) {
        request.missing_chunk_indexes.push_back(read_u64(payload, offset));
    }

    return true;
}

bool validate_continue_request(const net::protocol::FileManifest& manifest,
                               const net::protocol::ContinueRequest& request) {
    if (request.session_id != manifest.session_id ||
        request.total_chunks != manifest.chunk_count) {
        return false;
    }

    std::uint64_t previous = 0;
    bool has_previous = false;
    for (const std::uint64_t index : request.missing_chunk_indexes) {
        if (index >= manifest.chunk_count || (has_previous && index <= previous)) {
            return false;
        }
        previous = index;
        has_previous = true;
    }

    return true;
}

}  // namespace packetbridge::transfer
