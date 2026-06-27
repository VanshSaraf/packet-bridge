#pragma once

#include "net/protocol.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace packetbridge::transfer {

struct TransferCheckpoint {
    std::string original_filename;
    std::uint64_t file_size = 0;
    std::uint32_t chunk_size = 0;
    std::uint64_t total_chunks = 0;
    std::set<std::uint64_t> completed_chunks;
};

std::string checkpoint_path_for_output(const std::string& output_path);
bool load_checkpoint(const std::string& path, TransferCheckpoint& checkpoint);
bool checkpoint_matches_manifest(const TransferCheckpoint& checkpoint,
                                 const net::protocol::FileManifest& manifest);
TransferCheckpoint create_checkpoint_from_manifest(const net::protocol::FileManifest& manifest);
std::vector<std::uint64_t> missing_chunks(const TransferCheckpoint& checkpoint);
bool save_checkpoint(const std::string& path, const TransferCheckpoint& checkpoint);
bool remove_checkpoint(const std::string& path);
bool file_exists(const std::string& path);

}  // namespace packetbridge::transfer
