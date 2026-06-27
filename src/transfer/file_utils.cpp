#include "transfer/file_utils.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace packetbridge::transfer {

std::string basename_from_path(const std::string& path) {
    const std::string::size_type pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string sanitize_filename(const std::string& filename) {
    const std::string base = basename_from_path(filename);
    std::string sanitized;
    sanitized.reserve(base.size());

    for (const unsigned char ch : base) {
        if (std::isalnum(ch) || ch == '.' || ch == '-' || ch == '_') {
            sanitized.push_back(static_cast<char>(ch));
        } else {
            sanitized.push_back('_');
        }
    }

    while (!sanitized.empty() && sanitized.front() == '.') {
        sanitized.erase(sanitized.begin());
    }

    if (sanitized.empty()) {
        return "file";
    }

    return sanitized;
}

std::uint64_t file_size(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("unable to open file: " + path);
    }

    const std::ifstream::pos_type size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("unable to read file size: " + path);
    }

    return static_cast<std::uint64_t>(size);
}

std::uint64_t chunk_count(std::uint64_t size, std::uint32_t chunk_size) {
    if (chunk_size == 0) {
        return 0;
    }
    return (size + chunk_size - 1) / chunk_size;
}

std::string join_output_path(const std::string& directory, const std::string& filename) {
    if (directory.empty() || directory == ".") {
        return filename;
    }

    const char last = directory.back();
    if (last == '/' || last == '\\') {
        return directory + filename;
    }

    return directory + "/" + filename;
}

}  // namespace packetbridge::transfer
