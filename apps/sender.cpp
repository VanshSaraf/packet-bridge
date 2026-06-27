#include "common/constants.hpp"
#include "common/logger.hpp"
#include "net/endpoint.hpp"
#include "net/protocol.hpp"
#include "net/socket_runtime.hpp"
#include "net/tcp_socket.hpp"
#include "transfer/file_utils.hpp"
#include "transfer/manifest_codec.hpp"

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

bool send_file_bytes(packetbridge::net::TcpSocket& socket,
                     const std::string& file_path,
                     std::uint64_t file_size,
                     std::uint32_t chunk_size) {
    std::ifstream input(file_path, std::ios::binary);
    if (!input) {
        packetbridge::common::error("Unable to open input file: " + file_path);
        return false;
    }

    std::vector<std::uint8_t> buffer(chunk_size);
    std::uint64_t bytes_sent = 0;
    int last_percent = -1;

    while (bytes_sent < file_size) {
        const std::uint64_t remaining = file_size - bytes_sent;
        const std::size_t to_read = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, buffer.size()));

        input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(to_read));
        const std::streamsize bytes_read = input.gcount();
        if (bytes_read <= 0) {
            packetbridge::common::error("File read failed before transfer completed");
            return false;
        }

        if (!socket.send_all(buffer.data(), static_cast<std::size_t>(bytes_read))) {
            return false;
        }

        bytes_sent += static_cast<std::uint64_t>(bytes_read);
        const int percent = file_size == 0
                                ? 100
                                : static_cast<int>((bytes_sent * 100) / file_size);
        if (percent != last_percent && (percent % 10 == 0 || percent == 100)) {
            packetbridge::common::info("Transfer progress: " + std::to_string(percent) + "%");
            last_percent = percent;
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

    packetbridge::net::TcpSocket socket;
    if (!socket.connect_to(packetbridge::net::Endpoint{receiver_ip, transfer_port})) {
        return 1;
    }

    if (!packetbridge::transfer::send_file_manifest(socket, manifest)) {
        packetbridge::common::error("Failed to send file manifest");
        return 1;
    }

    if (!send_file_bytes(socket, file_path, manifest.file_size, manifest.chunk_size)) {
        return 1;
    }

    packetbridge::common::info("Transfer complete: " + manifest.filename);
    return 0;
}
