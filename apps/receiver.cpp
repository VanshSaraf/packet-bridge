#include "common/constants.hpp"
#include "common/logger.hpp"
#include "net/protocol.hpp"
#include "net/socket_runtime.hpp"
#include "net/tcp_server.hpp"
#include "transfer/file_utils.hpp"
#include "transfer/manifest_codec.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace {

void print_usage() {
    packetbridge::common::info("Usage: packetbridge_receiver [output_dir]");
}

bool receive_file_bytes(packetbridge::net::TcpSocket& socket,
                        const std::string& output_path,
                        std::uint64_t expected_size,
                        std::uint32_t chunk_size,
                        std::uint64_t& bytes_received) {
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        packetbridge::common::error("Unable to create output file: " + output_path);
        return false;
    }

    std::vector<std::uint8_t> buffer(chunk_size);
    int last_percent = -1;

    while (bytes_received < expected_size) {
        const std::uint64_t remaining = expected_size - bytes_received;
        const std::size_t to_receive = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, buffer.size()));

        if (!socket.recv_exact(buffer.data(), to_receive)) {
            return false;
        }

        output.write(reinterpret_cast<const char*>(buffer.data()),
                     static_cast<std::streamsize>(to_receive));
        if (!output) {
            packetbridge::common::error("Failed while writing output file");
            return false;
        }

        bytes_received += static_cast<std::uint64_t>(to_receive);
        const int percent = expected_size == 0
                                ? 100
                                : static_cast<int>((bytes_received * 100) / expected_size);
        if (percent != last_percent && (percent % 10 == 0 || percent == 100)) {
            packetbridge::common::info("Receive progress: " + std::to_string(percent) + "%");
            last_percent = percent;
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
    packetbridge::common::info("Writing to: " + output_path);

    std::uint64_t bytes_received = 0;
    if (!receive_file_bytes(client, output_path, manifest.file_size, manifest.chunk_size,
                            bytes_received)) {
        return 1;
    }

    if (bytes_received == manifest.file_size) {
        packetbridge::common::info("Byte-count verification passed: " +
                                   std::to_string(bytes_received) + " bytes received");
        packetbridge::common::info("Transfer complete: " + output_path);
    } else {
        packetbridge::common::error("Byte-count verification failed: expected " +
                                    std::to_string(manifest.file_size) + " bytes, received " +
                                    std::to_string(bytes_received));
        return 1;
    }

    return 0;
}
