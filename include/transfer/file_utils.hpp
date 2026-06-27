#pragma once

#include <cstdint>
#include <string>

namespace packetbridge::transfer {

std::string basename_from_path(const std::string& path);
std::string sanitize_filename(const std::string& filename);
std::uint64_t file_size(const std::string& path);
std::uint64_t chunk_count(std::uint64_t size, std::uint32_t chunk_size);
std::string join_output_path(const std::string& directory, const std::string& filename);

}  // namespace packetbridge::transfer
