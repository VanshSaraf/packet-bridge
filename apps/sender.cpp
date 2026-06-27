#include "common/constants.hpp"
#include "common/logger.hpp"
#include "crypto/sha256.hpp"
#include "net/endpoint.hpp"
#include "net/protocol.hpp"
#include "net/socket_runtime.hpp"
#include "net/tcp_socket.hpp"
#include "transfer/chunk_codec.hpp"
#include "transfer/continuation_codec.hpp"
#include "transfer/file_utils.hpp"
#include "transfer/hash_codec.hpp"
#include "transfer/manifest_codec.hpp"
#include "transfer/progress_tracker.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <string>
#include <vector>

namespace {

void print_usage() {
    packetbridge::common::info("Usage: packetbridge_sender <receiver_ip> <file_path>");
}

std::uint32_t payload_size_for_chunk(
    const packetbridge::net::protocol::FileManifest& manifest,
    std::uint64_t chunk_index) {
    const std::uint64_t offset = chunk_index * manifest.chunk_size;
    const std::uint64_t remaining = manifest.file_size - offset;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(remaining, manifest.chunk_size));
}

std::uint64_t bytes_for_chunks(const packetbridge::net::protocol::FileManifest& manifest,
                               const std::vector<std::uint64_t>& chunk_indexes) {
    std::uint64_t total = 0;
    for (const std::uint64_t index : chunk_indexes) {
        total += payload_size_for_chunk(manifest, index);
    }
    return total;
}

bool send_file_chunks(packetbridge::net::TcpSocket& socket,
                      const std::string& file_path,
                      const packetbridge::net::protocol::FileManifest& manifest,
                      const std::vector<std::uint64_t>& chunk_indexes,
                      packetbridge::transfer::ProgressTracker& progress) {
    std::ifstream input(file_path, std::ios::binary);
    if (!input) {
        packetbridge::common::error("Unable to open input file: " + file_path);
        return false;
    }

    std::vector<std::uint8_t> buffer(manifest.chunk_size);

    for (const std::uint64_t chunk_index : chunk_indexes) {
        const std::uint64_t byte_offset = chunk_index * manifest.chunk_size;
        const std::uint32_t payload_size = payload_size_for_chunk(manifest, chunk_index);

        input.seekg(static_cast<std::streamoff>(byte_offset), std::ios::beg);
        if (!input) {
            packetbridge::common::error("File seek failed before chunk read");
            return false;
        }

        input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(payload_size));
        const std::streamsize bytes_read = input.gcount();
        if (bytes_read != static_cast<std::streamsize>(payload_size)) {
            packetbridge::common::error("File read failed before chunk transfer completed");
            return false;
        }

        packetbridge::net::protocol::ChunkHeader header;
        header.session_id = manifest.session_id;
        header.chunk_index = chunk_index;
        header.byte_offset = byte_offset;
        header.payload_size = payload_size;

        if (!packetbridge::transfer::send_chunk_header(socket, header)) {
            packetbridge::common::error("Failed to send chunk header");
            return false;
        }

        if (!socket.send_all(buffer.data(), static_cast<std::size_t>(bytes_read))) {
            return false;
        }

        progress.add_progress(static_cast<std::uint64_t>(bytes_read), 1);
        if (progress.should_report()) {
            packetbridge::common::info(progress.progress_line());
        }
    }

    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        print_usage();
        return 1;
    }

    packetbridge::net::SocketRuntime socket_runtime;

    const std::string receiver_ip = argv[1];
    const std::string file_path = argv[2];
    const auto transfer_port = packetbridge::common::kDefaultTransferTcpPort;
    const std::uint32_t chunk_size =
        static_cast<std::uint32_t>(packetbridge::common::kDefaultChunkSize);

    std::uint64_t size = 0;
    std::string expected_hash;
    try {
        size = packetbridge::transfer::file_size(file_path);
        packetbridge::common::info("Computing SHA-256 for input file");
        expected_hash = packetbridge::crypto::sha256_file(file_path);
    } catch (const std::exception& ex) {
        packetbridge::common::error(ex.what());
        return 1;
    }

    packetbridge::net::protocol::FileManifest manifest;
    manifest.filename = packetbridge::transfer::sanitize_filename(
        packetbridge::transfer::basename_from_path(file_path));
    manifest.file_size = size;
    manifest.chunk_size = chunk_size;
    manifest.chunk_count = packetbridge::transfer::chunk_count(size, chunk_size);
    manifest.protocol_version = packetbridge::common::kProtocolVersion;
    manifest.session_id = 0;

    packetbridge::common::info("Connecting to receiver " + receiver_ip + ":" +
                               std::to_string(transfer_port));
    packetbridge::common::info("File: " + manifest.filename);
    packetbridge::common::info("File size: " + std::to_string(manifest.file_size) + " bytes");
    packetbridge::common::info("Total chunks: " + std::to_string(manifest.chunk_count));
    packetbridge::common::info("Chunk size: " + std::to_string(manifest.chunk_size) + " bytes");

    packetbridge::net::TcpSocket socket;
    if (!socket.connect_to(packetbridge::net::Endpoint{receiver_ip, transfer_port})) {
        return 1;
    }

    if (!packetbridge::transfer::send_file_manifest(socket, manifest)) {
        packetbridge::common::error("Failed to send file manifest");
        return 1;
    }

    packetbridge::net::protocol::ContinueRequest request;
    if (!packetbridge::transfer::receive_continue_request(socket, request) ||
        !packetbridge::transfer::validate_continue_request(manifest, request)) {
        packetbridge::common::error("Failed to receive valid continuation request");
        return 1;
    }

    const std::uint64_t already_present =
        manifest.chunk_count - static_cast<std::uint64_t>(request.missing_chunk_indexes.size());
    const bool continuing = already_present > 0;
    packetbridge::common::info(continuing ? "Continuing interrupted transfer"
                                          : "Starting fresh transfer");
    packetbridge::common::info("Chunks already present on receiver: " +
                               std::to_string(already_present));
    packetbridge::common::info("Chunks to send: " +
                               std::to_string(request.missing_chunk_indexes.size()));

    const std::uint64_t bytes_to_send = bytes_for_chunks(manifest, request.missing_chunk_indexes);
    packetbridge::transfer::ProgressTracker progress(
        bytes_to_send, static_cast<std::uint64_t>(request.missing_chunk_indexes.size()));
    if (!send_file_chunks(socket, file_path, manifest, request.missing_chunk_indexes, progress)) {
        return 1;
    }

    packetbridge::net::protocol::HashPacket hash_packet;
    hash_packet.session_id = manifest.session_id;
    hash_packet.sha256_hex = expected_hash;
    if (!packetbridge::transfer::send_hash_packet(socket, hash_packet)) {
        packetbridge::common::error("Failed to send SHA-256 hash packet");
        return 1;
    }

    packetbridge::common::info("Transfer complete: " + manifest.filename);
    packetbridge::common::info("Chunks sent: " + std::to_string(progress.chunks_completed()) +
                               "/" + std::to_string(request.missing_chunk_indexes.size()));
    packetbridge::common::info("Bytes sent: " + std::to_string(progress.bytes_transferred()) +
                               "/" + std::to_string(bytes_to_send));
    packetbridge::common::info("Expected SHA-256: " + expected_hash);
    packetbridge::common::info("Transfer summary: " + progress.summary_line());
    return 0;
}
