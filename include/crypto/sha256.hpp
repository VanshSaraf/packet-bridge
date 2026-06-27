#pragma once

#include <string>

namespace packetbridge::crypto {

std::string sha256_file(const std::string& path);
bool is_sha256_hex(const std::string& value);

}  // namespace packetbridge::crypto
