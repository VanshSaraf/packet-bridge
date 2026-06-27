#include "common/constants.hpp"
#include "common/logger.hpp"
#include "crypto/sha256.hpp"
#include "net/protocol.hpp"
#include "net/socket_runtime.hpp"
#include "net/tcp_server.hpp"
#include "transfer/checkpoint.hpp"
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
#include <set>
#include <string>
#include <vector>

namespace {

void print_usage() {
    packetbridge::common::info("Usage: packetbridge_receiver [output_dir]");
    packetbridge::common::info("Example: packetbridge_receiver ./received");
    packetbridge::common::info("If output_dir is omitted, files are written to the current directory.");
}

bool is_help_arg(const char* arg) {
    const std::string value = arg == nullptr ? "" : arg;
    return value == "-h" || value == "--help";
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

bool prepare_output_file(const std::string& output_path, bool fresh_transfer) {
    if (fresh_transfer) {
        std::ofstream create(output_path, std::ios::binary | std::ios::trunc);
        return static_cast<bool>(create);
    }

    return packetbridge::transfer::file_exists(output_path);
}

bool receive_file_chunks(packetbridge::net::TcpSocket& socket,
                         const std::string& output_path,
                         const std::string& checkpoint_path,
                         const packetbridge::net::protocol::FileManifest& manifest,
                         const std::vector<std::uint64_t>& chunk_indexes,
                         packetbridge::transfer::TransferCheckpoint& checkpoint,
                         packetbridge::transfer::ProgressTracker& progress,
                         bool fresh_transfer) {
    if (!prepare_output_file(output_path, fresh_transfer)) {
        packetbridge::common::error("Unable to prepare output file: " + output_path);
        return false;
    }

    std::fstream output(output_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!output) {
        packetbridge::common::error("Unable to open output file: " + output_path);
        return false;
    }

    std::vector<std::uint8_t> buffer(manifest.chunk_size);

    for (const std::uint64_t expected_index : chunk_indexes) {
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
        checkpoint.completed_chunks.insert(header.chunk_index);
        if (!packetbridge::transfer::save_checkpoint(checkpoint_path, checkpoint)) {
            packetbridge::common::error("Failed to update checkpoint file");
            return false;
        }

        if (progress.should_report()) {
            packetbridge::common::info(progress.progress_line());
        }
    }

    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 2 || (argc == 2 && is_help_arg(argv[1]))) {
        print_usage();
        return argc > 2 ? 1 : 0;
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
    packetbridge::common::info("Output directory: " + output_dir);

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
    const std::string checkpoint_path =
        packetbridge::transfer::checkpoint_path_for_output(output_path);

    packetbridge::common::info("Incoming file: " + sanitized_filename);
    packetbridge::common::info("File size: " + std::to_string(manifest.file_size) + " bytes");
    packetbridge::common::info("Total chunks: " + std::to_string(manifest.chunk_count));
    packetbridge::common::info("Chunk size: " + std::to_string(manifest.chunk_size) + " bytes");
    packetbridge::common::info("Writing to: " + output_path);

    packetbridge::transfer::TransferCheckpoint checkpoint;
    bool loaded_checkpoint =
        packetbridge::transfer::file_exists(output_path) &&
        packetbridge::transfer::load_checkpoint(checkpoint_path, checkpoint) &&
        packetbridge::transfer::checkpoint_matches_manifest(checkpoint, manifest);
    if (!loaded_checkpoint) {
        checkpoint = packetbridge::transfer::create_checkpoint_from_manifest(manifest);
    }

    std::vector<std::uint64_t> chunks_to_receive =
        packetbridge::transfer::missing_chunks(checkpoint);

    packetbridge::net::protocol::ContinueRequest request;
    request.session_id = manifest.session_id;
    request.total_chunks = manifest.chunk_count;
    request.missing_chunk_indexes = chunks_to_receive;
    if (!packetbridge::transfer::send_continue_request(client, request)) {
        packetbridge::common::error("Failed to send continuation request");
        return 1;
    }

    const std::uint64_t already_present =
        manifest.chunk_count - static_cast<std::uint64_t>(chunks_to_receive.size());
    const bool continuing = already_present > 0;
    packetbridge::common::info(continuing ? "Continuing from checkpoint"
                                          : "Starting fresh output file");
    packetbridge::common::info("Chunks already present: " + std::to_string(already_present));
    packetbridge::common::info("Chunks requested: " + std::to_string(chunks_to_receive.size()));

    if (!packetbridge::transfer::save_checkpoint(checkpoint_path, checkpoint)) {
        packetbridge::common::error("Failed to initialize checkpoint file");
        return 1;
    }

    const std::uint64_t bytes_to_receive = bytes_for_chunks(manifest, chunks_to_receive);
    packetbridge::transfer::ProgressTracker progress(
        bytes_to_receive, static_cast<std::uint64_t>(chunks_to_receive.size()));
    if (!receive_file_chunks(client, output_path, checkpoint_path, manifest, chunks_to_receive,
                             checkpoint, progress, !loaded_checkpoint)) {
        return 1;
    }

    packetbridge::common::info("Chunks received this run: " +
                               std::to_string(progress.chunks_completed()) + "/" +
                               std::to_string(chunks_to_receive.size()));
    packetbridge::common::info("Transfer summary: " + progress.summary_line());

    packetbridge::net::protocol::HashPacket hash_packet;
    if (!packetbridge::transfer::receive_hash_packet(client, hash_packet) ||
        hash_packet.session_id != manifest.session_id) {
        packetbridge::common::error("Failed to receive valid SHA-256 hash packet");
        return 1;
    }

    std::string computed_hash;
    try {
        packetbridge::common::info("Computing SHA-256 for received file");
        computed_hash = packetbridge::crypto::sha256_file(output_path);
    } catch (const std::exception& ex) {
        packetbridge::common::error(ex.what());
        return 1;
    }

    packetbridge::common::info("Expected SHA-256: " + hash_packet.sha256_hex);
    packetbridge::common::info("Computed SHA-256: " + computed_hash);
    if (computed_hash == hash_packet.sha256_hex) {
        if (!packetbridge::transfer::remove_checkpoint(checkpoint_path)) {
            packetbridge::common::warn("Unable to remove checkpoint file: " + checkpoint_path);
        }
        packetbridge::common::info("Byte-count verification passed: " +
                                   std::to_string(manifest.file_size) + " bytes available");
        packetbridge::common::info("SHA-256 integrity verified");
        packetbridge::common::info("Transfer complete: " + output_path);
    } else {
        packetbridge::common::error("SHA-256 integrity failed");
        return 1;
    }

    return 0;
}
