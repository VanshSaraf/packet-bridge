#include "common/constants.hpp"
#include "common/logger.hpp"
#include "discovery/discovery_message.hpp"
#include "discovery/peer.hpp"
#include "net/socket_runtime.hpp"
#include "net/udp_socket.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

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
    packetbridge::common::info("Usage: packetbridge_listener");
    packetbridge::common::info("Example: packetbridge_listener");
    packetbridge::common::info("Listens for UDP discovery announcements and prints live peers.");
}

bool is_help_arg(const char* arg) {
    const std::string value = arg == nullptr ? "" : arg;
    return value == "-h" || value == "--help";
}

std::string hostname_or_empty() {
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname) - 1) == 0) {
        return hostname;
    }
    return {};
}

std::string peer_key(const packetbridge::discovery::Peer& peer) {
    return peer.ip_address + ":" + std::to_string(peer.transfer_port);
}

bool looks_like_loopback(const std::string& ip_address) {
    return ip_address == "127.0.0.1" || ip_address == "::1";
}

void remove_stale_peers(std::map<std::string, packetbridge::discovery::Peer>& peers,
                        std::chrono::steady_clock::time_point now) {
    constexpr auto stale_after = std::chrono::seconds(10);

    for (auto it = peers.begin(); it != peers.end();) {
        if (now - it->second.last_seen > stale_after) {
            packetbridge::common::warn("Peer stale: " + it->second.name + " at " + it->first);
            it = peers.erase(it);
        } else {
            ++it;
        }
    }
}

void print_peer_table(const std::map<std::string, packetbridge::discovery::Peer>& peers,
                      std::chrono::steady_clock::time_point now) {
    std::cout << "\nAvailable PacketBridge peers\n";
    std::cout << std::left << std::setw(6) << "Index"
              << std::setw(24) << "Name"
              << std::setw(18) << "IP"
              << std::setw(15) << "Transfer Port"
              << "Last Seen\n";
    std::cout << std::string(75, '-') << '\n';

    if (peers.empty()) {
        std::cout << "No peers discovered yet.\n";
        return;
    }

    std::size_t index = 1;
    for (const auto& entry : peers) {
        const auto seconds_ago =
            std::chrono::duration_cast<std::chrono::seconds>(now - entry.second.last_seen).count();
        std::cout << std::left << std::setw(6) << index++
                  << std::setw(24) << entry.second.name
                  << std::setw(18) << entry.second.ip_address
                  << std::setw(15) << entry.second.transfer_port
                  << seconds_ago << "s ago\n";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc > 1) {
        print_usage();
        return argc == 2 && is_help_arg(argv[1]) ? 0 : 1;
    }

    packetbridge::net::SocketRuntime socket_runtime;

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    const auto discovery_port = packetbridge::common::kDefaultDiscoveryUdpPort;
    const std::string local_hostname = hostname_or_empty();

    packetbridge::net::UdpSocket socket;
    if (!socket.bind_to(discovery_port) ||
        !socket.set_receive_timeout(std::chrono::milliseconds(500))) {
        return 1;
    }

    packetbridge::common::info("PacketBridge listener running");
    packetbridge::common::info("Listening for discovery packets on UDP port " +
                               std::to_string(discovery_port));
    packetbridge::common::info("Peer entries expire after 10 seconds without an announcement");

    std::map<std::string, packetbridge::discovery::Peer> peers;
    std::vector<std::uint8_t> buffer(1024);
    auto next_table_print = std::chrono::steady_clock::now();

    while (g_running) {
        const auto receive_result = socket.receive_from(buffer);
        const auto now = std::chrono::steady_clock::now();

        if (receive_result.ok) {
            const std::string message(buffer.begin(), buffer.begin() + receive_result.bytes_received);
            auto parsed = packetbridge::discovery::parse_discovery_message(
                message, receive_result.remote.host);
            if (parsed) {
                if (!local_hostname.empty() && parsed->name == local_hostname &&
                    looks_like_loopback(parsed->ip_address)) {
                    continue;
                }

                const std::string key = peer_key(*parsed);
                const bool is_new_peer = peers.find(key) == peers.end();
                peers[key] = *parsed;

                if (is_new_peer) {
                    packetbridge::common::info("New peer discovered: " + parsed->name +
                                               " at " + key);
                }
            }
        }

        remove_stale_peers(peers, now);

        if (now >= next_table_print) {
            print_peer_table(peers, now);
            next_table_print = now + std::chrono::seconds(3);
        }
    }

    packetbridge::common::info("PacketBridge listener stopped");
    return 0;
}
