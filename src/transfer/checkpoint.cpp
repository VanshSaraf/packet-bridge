#include "transfer/checkpoint.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace packetbridge::transfer {
namespace {

constexpr const char* kCheckpointHeader = "PACKETBRIDGE_CHECKPOINT_V1";

std::string value_after_equals(const std::string& line) {
    const std::string::size_type pos = line.find('=');
    if (pos == std::string::npos) {
        return {};
    }
    return line.substr(pos + 1);
}

std::uint64_t parse_u64(const std::string& value) {
    return static_cast<std::uint64_t>(std::stoull(value));
}

std::set<std::uint64_t> parse_completed_chunks(const std::string& value) {
    std::set<std::uint64_t> chunks;
    std::stringstream stream(value);
    std::string part;
    while (std::getline(stream, part, ',')) {
        if (!part.empty()) {
            chunks.insert(parse_u64(part));
        }
    }
    return chunks;
}

}  // namespace

std::string checkpoint_path_for_output(const std::string& output_path) {
    return output_path + ".pbcheckpoint";
}

bool load_checkpoint(const std::string& path, TransferCheckpoint& checkpoint) {
    std::ifstream input(path);
    if (!input) {
        return false;
    }

    std::string line;
    if (!std::getline(input, line) || line != kCheckpointHeader) {
        return false;
    }

    TransferCheckpoint loaded;
    try {
        while (std::getline(input, line)) {
            if (line.find("filename=") == 0) {
                loaded.original_filename = value_after_equals(line);
            } else if (line.find("file_size=") == 0) {
                loaded.file_size = parse_u64(value_after_equals(line));
            } else if (line.find("chunk_size=") == 0) {
                loaded.chunk_size = static_cast<std::uint32_t>(parse_u64(value_after_equals(line)));
            } else if (line.find("chunk_count=") == 0) {
                loaded.total_chunks = parse_u64(value_after_equals(line));
            } else if (line.find("completed=") == 0) {
                loaded.completed_chunks = parse_completed_chunks(value_after_equals(line));
            }
        }
    } catch (const std::exception&) {
        return false;
    }

    checkpoint = loaded;
    return true;
}

bool checkpoint_matches_manifest(const TransferCheckpoint& checkpoint,
                                 const net::protocol::FileManifest& manifest) {
    if (checkpoint.original_filename != manifest.filename ||
        checkpoint.file_size != manifest.file_size ||
        checkpoint.chunk_size != manifest.chunk_size ||
        checkpoint.total_chunks != manifest.chunk_count) {
        return false;
    }

    for (const std::uint64_t index : checkpoint.completed_chunks) {
        if (index >= manifest.chunk_count) {
            return false;
        }
    }

    return true;
}

TransferCheckpoint create_checkpoint_from_manifest(const net::protocol::FileManifest& manifest) {
    TransferCheckpoint checkpoint;
    checkpoint.original_filename = manifest.filename;
    checkpoint.file_size = manifest.file_size;
    checkpoint.chunk_size = manifest.chunk_size;
    checkpoint.total_chunks = manifest.chunk_count;
    return checkpoint;
}

std::vector<std::uint64_t> missing_chunks(const TransferCheckpoint& checkpoint) {
    std::vector<std::uint64_t> missing;
    for (std::uint64_t index = 0; index < checkpoint.total_chunks; ++index) {
        if (checkpoint.completed_chunks.find(index) == checkpoint.completed_chunks.end()) {
            missing.push_back(index);
        }
    }
    return missing;
}

bool save_checkpoint(const std::string& path, const TransferCheckpoint& checkpoint) {
    const std::string temp_path = path + ".tmp";
    {
        std::ofstream output(temp_path, std::ios::trunc);
        if (!output) {
            return false;
        }

        output << kCheckpointHeader << '\n';
        output << "filename=" << checkpoint.original_filename << '\n';
        output << "file_size=" << checkpoint.file_size << '\n';
        output << "chunk_size=" << checkpoint.chunk_size << '\n';
        output << "chunk_count=" << checkpoint.total_chunks << '\n';
        output << "completed=";
        bool first = true;
        for (const std::uint64_t index : checkpoint.completed_chunks) {
            if (!first) {
                output << ',';
            }
            output << index;
            first = false;
        }
        output << '\n';
    }

    std::remove(path.c_str());
    return std::rename(temp_path.c_str(), path.c_str()) == 0;
}

bool remove_checkpoint(const std::string& path) {
    return std::remove(path.c_str()) == 0 || !file_exists(path);
}

bool file_exists(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return static_cast<bool>(file);
}

}  // namespace packetbridge::transfer
