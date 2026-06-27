#include "common/constants.hpp"
#include "common/logger.hpp"
#include "net/endpoint.hpp"
#include "net/protocol.hpp"
#include "net/socket_runtime.hpp"
#include "net/tcp_socket.hpp"
#include "transfer/chunk_codec.hpp"
#include "transfer/file_utils.hpp"
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

bool send_file_chunks(packetbridge::net::TcpSocket& socket,
                      const std::string& file_path,
                      const packetbridge::net::protocol::FileManifest& manifest,
                      packetbridge::transfer::ProgressTracker& progress) {
    std::ifstream input(file_path, std::ios::binary);
    if (!input) {
        packetbridge::common::error("Unable to open input file: " + file_path);
        return false;
    }

    std::vector<std::uint8_t> buffer(manifest.chunk_size);
    std::uint64_t bytes_sent = 0;
    std::uint64_t chunk_index = 0;

    while (bytes_sent < manifest.file_size) {
        const std::uint64_t remaining = manifest.file_size - bytes_sent;
        const std::size_t to_read = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, buffer.size()));

        input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(to_read));
        const std::streamsize bytes_read = input.gcount();
        if (bytes_read <= 0) {
            packetbridge::common::error("File read failed before transfer completed");
            return false;
        }

        packetbridge::net::protocol::ChunkHeader header;
        header.session_id = manifest.session_id;
        header.chunk_index = chunk_index;
        header.byte_offset = bytes_sent;
        header.payload_size = static_cast<std::uint32_t>(bytes_read);

        if (!packetbridge::transfer::send_chunk_header(socket, header)) {
            packetbridge::common::error("Failed to send chunk header");
            return false;
        }

        if (!socket.send_all(buffer.data(), static_cast<std::size_t>(bytes_read))) {
            return false;
        }

        bytes_sent += static_cast<std::uint64_t>(bytes_read);
        ++chunk_index;
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
    try {
        size = packetbridge::transfer::file_size(file_path);
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

    packetbridge::transfer::ProgressTracker progress(manifest.file_size, manifest.chunk_count);
    if (!send_file_chunks(socket, file_path, manifest, progress)) {
        return 1;
    }

    packetbridge::common::info("Transfer complete: " + manifest.filename);
    packetbridge::common::info("Chunks sent: " + std::to_string(progress.chunks_completed()) +
                               "/" + std::to_string(manifest.chunk_count));
    packetbridge::common::info("Bytes sent: " + std::to_string(progress.bytes_transferred()) +
                               "/" + std::to_string(manifest.file_size));
    packetbridge::common::info("Transfer summary: " + progress.summary_line());
    return 0;
}
