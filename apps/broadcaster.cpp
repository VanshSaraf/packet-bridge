#include "common/constants.hpp"
#include "common/logger.hpp"
#include "discovery/discovery_message.hpp"
#include "net/endpoint.hpp"
#include "net/socket_runtime.hpp"
#include "net/udp_socket.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <unistd.h>
#endif

namespace {

std::atomic_bool g_running{true};

void handle_signal(int) {
    g_running = false;
}

void print_usage() {
    packetbridge::common::info("Usage: packetbridge_broadcaster [peer_name]");
    packetbridge::common::info("Example: packetbridge_broadcaster laptop-one");
    packetbridge::common::info("If peer_name is omitted, the local hostname is used when available.");
}

bool is_help_arg(const char* arg) {
    const std::string value = arg == nullptr ? "" : arg;
    return value == "-h" || value == "--help";
}

std::string hostname_or_default() {
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname) - 1) == 0 && hostname[0] != '\0') {
        return hostname;
    }
    return "packetbridge-node";
}

std::string safe_peer_name(const std::string& requested_name) {
    if (requested_name.empty() || requested_name.find('|') != std::string::npos) {
        return "packetbridge-node";
    }

    if (requested_name.size() > packetbridge::common::kMaxPeerNameLength) {
        return requested_name.substr(0, packetbridge::common::kMaxPeerNameLength);
    }

    return requested_name;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 2 || (argc == 2 && is_help_arg(argv[1]))) {
        print_usage();
        return argc > 2 ? 1 : 0;
    }

    packetbridge::net::SocketRuntime socket_runtime;

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    const std::string peer_name = safe_peer_name(argc > 1 ? argv[1] : hostname_or_default());
    const auto discovery_port = packetbridge::common::kDefaultDiscoveryUdpPort;
    const auto transfer_port = packetbridge::common::kDefaultTransferTcpPort;
    const std::string message =
        packetbridge::discovery::build_discovery_message(peer_name, transfer_port);

    packetbridge::net::UdpSocket socket;
    if (!socket.open() || !socket.enable_broadcast()) {
        return 1;
    }

    const packetbridge::net::Endpoint broadcast_endpoint{"255.255.255.255", discovery_port};

    packetbridge::common::info("PacketBridge broadcaster running");
    packetbridge::common::info("Peer name: " + peer_name);
    packetbridge::common::info("Discovery UDP port: " + std::to_string(discovery_port));
    packetbridge::common::info("Transfer TCP port: " + std::to_string(transfer_port));
    packetbridge::common::info("Broadcast interval: 2 seconds");

    while (g_running) {
        if (socket.send_to(message.data(), message.size(), broadcast_endpoint)) {
            packetbridge::common::info("Discovery announcement sent to " +
                                       broadcast_endpoint.to_string());
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    packetbridge::common::info("PacketBridge broadcaster stopped");
    return 0;
}
