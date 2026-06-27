#include "transfer/manifest_codec.hpp"

#include "common/constants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace packetbridge::transfer {
namespace {

constexpr std::uint32_t kManifestMagic = 0x5042464d;  // PBFM
constexpr std::uint32_t kManifestHeaderSize =
    sizeof(std::uint32_t) + sizeof(std::uint16_t) + sizeof(std::uint16_t) +
    sizeof(std::uint64_t) + sizeof(std::uint32_t) + sizeof(std::uint64_t) +
    sizeof(std::uint64_t) + sizeof(std::uint16_t);

void append_u16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    output.push_back(static_cast<std::uint8_t>(value & 0xff));
}

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

std::uint16_t read_u16(const std::vector<std::uint8_t>& input, std::size_t& offset) {
    const std::uint16_t value =
        (static_cast<std::uint16_t>(input[offset]) << 8) |
        static_cast<std::uint16_t>(input[offset + 1]);
    offset += 2;
    return value;
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

bool send_file_manifest(net::TcpSocket& socket, const net::protocol::FileManifest& manifest) {
    if (manifest.filename.size() > common::kMaxFilenameLength) {
        return false;
    }

    std::vector<std::uint8_t> payload;
    payload.reserve(kManifestHeaderSize + manifest.filename.size());

    append_u32(payload, kManifestMagic);
    append_u16(payload, manifest.protocol_version);
    append_u16(payload, static_cast<std::uint16_t>(manifest.filename.size()));
    append_u64(payload, manifest.file_size);
    append_u32(payload, manifest.chunk_size);
    append_u64(payload, manifest.chunk_count);
    append_u64(payload, manifest.session_id);
    append_u16(payload, 0);
    payload.insert(payload.end(), manifest.filename.begin(), manifest.filename.end());

    const std::uint32_t payload_size = static_cast<std::uint32_t>(payload.size());
    std::vector<std::uint8_t> frame;
    append_u32(frame, payload_size);

    return socket.send_all(frame) && socket.send_all(payload);
}

bool receive_file_manifest(net::TcpSocket& socket, net::protocol::FileManifest& manifest) {
    std::vector<std::uint8_t> size_frame(sizeof(std::uint32_t));
    if (!socket.recv_exact(size_frame)) {
        return false;
    }

    std::size_t offset = 0;
    const std::uint32_t payload_size = read_u32(size_frame, offset);
    if (payload_size < kManifestHeaderSize ||
        payload_size > kManifestHeaderSize + common::kMaxFilenameLength) {
        return false;
    }

    std::vector<std::uint8_t> payload(payload_size);
    if (!socket.recv_exact(payload)) {
        return false;
    }

    offset = 0;
    const std::uint32_t magic = read_u32(payload, offset);
    if (magic != kManifestMagic) {
        return false;
    }

    manifest.protocol_version = read_u16(payload, offset);
    const std::uint16_t filename_size = read_u16(payload, offset);
    manifest.file_size = read_u64(payload, offset);
    manifest.chunk_size = read_u32(payload, offset);
    manifest.chunk_count = read_u64(payload, offset);
    manifest.session_id = read_u64(payload, offset);
    const std::uint16_t flags = read_u16(payload, offset);
    (void)flags;

    if (manifest.protocol_version != common::kProtocolVersion ||
        filename_size == 0 ||
        filename_size > common::kMaxFilenameLength ||
        offset + filename_size != payload.size() ||
        manifest.chunk_size == 0) {
        return false;
    }

    manifest.filename = std::string(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                                    payload.end());
    manifest.file_sha256 = {};
    return true;
}

}  // namespace packetbridge::transfer
