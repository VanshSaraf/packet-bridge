#include "common/constants.hpp"
#include "common/logger.hpp"
#include "net/protocol.hpp"
#include "net/socket_runtime.hpp"
#include "net/tcp_server.hpp"
#include "transfer/chunk_codec.hpp"
#include "transfer/file_utils.hpp"
#include "transfer/manifest_codec.hpp"
#include "transfer/progress_tracker.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace {

void print_usage() {
    packetbridge::common::info("Usage: packetbridge_receiver [output_dir]");
}

bool receive_file_chunks(packetbridge::net::TcpSocket& socket,
                         const std::string& output_path,
                         const packetbridge::net::protocol::FileManifest& manifest,
                         packetbridge::transfer::ProgressTracker& progress) {
    std::ofstream output(output_path, std::ios::binary | std::ios::in | std::ios::out |
                                      std::ios::trunc);
    if (!output) {
        packetbridge::common::error("Unable to create output file: " + output_path);
        return false;
    }

    std::vector<std::uint8_t> buffer(manifest.chunk_size);

    for (std::uint64_t expected_index = 0; expected_index < manifest.chunk_count; ++expected_index) {
        packetbridge::net::protocol::ChunkHeader header;
        if (!packetbridge::transfer::receive_chunk_header(socket, header)) {
            packetbridge::common::error("Failed to receive chunk header");
            return false;
        }

        if (header.chunk_index != expected_index ||
            !packetbridge::transfer::validate_chunk_header(manifest, header)) {
            packetbridge::common::error("Invalid chunk header received");
            return false;
        }

        if (!socket.recv_exact(buffer.data(), header.payload_size)) {
            return false;
        }

        output.seekp(static_cast<std::streamoff>(header.byte_offset), std::ios::beg);
        output.write(reinterpret_cast<const char*>(buffer.data()),
                     static_cast<std::streamsize>(header.payload_size));
        if (!output) {
            packetbridge::common::error("Failed while writing output file");
            return false;
        }

        progress.add_progress(header.payload_size, 1);
        if (progress.should_report()) {
            packetbridge::common::info(progress.progress_line());
        }
    }

    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 2) {
        print_usage();
        return 1;
    }

    packetbridge::net::SocketRuntime socket_runtime;

    const std::string output_dir = argc == 2 ? argv[1] : ".";
    const auto transfer_port = packetbridge::common::kDefaultTransferTcpPort;

    packetbridge::net::TcpServer server;
    if (!server.bind_and_listen(transfer_port)) {
        return 1;
    }

    packetbridge::common::info("PacketBridge receiver listening on TCP port " +
                               std::to_string(transfer_port));
    packetbridge::common::info("Waiting for one sender connection");

    packetbridge::net::TcpSocket client = server.accept_client();
    if (!client.is_open()) {
        return 1;
    }

    packetbridge::common::info("Sender connected");

    packetbridge::net::protocol::FileManifest manifest;
    if (!packetbridge::transfer::receive_file_manifest(client, manifest)) {
        packetbridge::common::error("Failed to receive valid file manifest");
        return 1;
    }

    const std::string sanitized_filename =
        packetbridge::transfer::sanitize_filename(manifest.filename);
    const std::string output_filename = "received_" + sanitized_filename;
    const std::string output_path =
        packetbridge::transfer::join_output_path(output_dir, output_filename);

    packetbridge::common::info("Incoming file: " + sanitized_filename);
    packetbridge::common::info("File size: " + std::to_string(manifest.file_size) + " bytes");
    packetbridge::common::info("Total chunks: " + std::to_string(manifest.chunk_count));
    packetbridge::common::info("Chunk size: " + std::to_string(manifest.chunk_size) + " bytes");
    packetbridge::common::info("Writing to: " + output_path);

    packetbridge::transfer::ProgressTracker progress(manifest.file_size, manifest.chunk_count);
    if (!receive_file_chunks(client, output_path, manifest, progress)) {
        return 1;
    }

    if (progress.bytes_transferred() == manifest.file_size) {
        packetbridge::common::info("Byte-count verification passed: " +
                                   std::to_string(progress.bytes_transferred()) + " bytes received");
        packetbridge::common::info("Transfer complete: " + output_path);
        packetbridge::common::info("Chunks received: " +
                                   std::to_string(progress.chunks_completed()) + "/" +
                                   std::to_string(manifest.chunk_count));
        packetbridge::common::info("Transfer summary: " + progress.summary_line());
    } else {
        packetbridge::common::error("Byte-count verification failed: expected " +
                                    std::to_string(manifest.file_size) + " bytes, received " +
                                    std::to_string(progress.bytes_transferred()));
        return 1;
    }

    return 0;
}
