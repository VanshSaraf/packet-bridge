#pragma once

#include <cstddef>
#include <cstdint>

namespace packetbridge::common {

inline constexpr const char* kAppName = "PacketBridge";
inline constexpr std::uint16_t kProtocolVersion = 1;

inline constexpr std::uint16_t kDefaultDiscoveryUdpPort = 47110;
inline constexpr std::uint16_t kDefaultTransferTcpPort = 47111;

inline constexpr std::size_t kDefaultChunkSize = 64 * 1024;
inline constexpr std::size_t kMaxFilenameLength = 255;
inline constexpr std::size_t kMaxPeerNameLength = 64;

}  // namespace packetbridge::common
