#include "crypto/sha256.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace packetbridge::crypto {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

std::uint32_t rotate_right(std::uint32_t value, std::uint32_t bits) {
    return (value >> bits) | (value << (32 - bits));
}

std::uint32_t choose(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (~x & z);
}

std::uint32_t majority(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

std::uint32_t big_sigma0(std::uint32_t value) {
    return rotate_right(value, 2) ^ rotate_right(value, 13) ^ rotate_right(value, 22);
}

std::uint32_t big_sigma1(std::uint32_t value) {
    return rotate_right(value, 6) ^ rotate_right(value, 11) ^ rotate_right(value, 25);
}

std::uint32_t small_sigma0(std::uint32_t value) {
    return rotate_right(value, 7) ^ rotate_right(value, 18) ^ (value >> 3);
}

std::uint32_t small_sigma1(std::uint32_t value) {
    return rotate_right(value, 17) ^ rotate_right(value, 19) ^ (value >> 10);
}

class Sha256 {
public:
    void update(const std::uint8_t* data, std::size_t size) {
        bit_count_ += static_cast<std::uint64_t>(size) * 8;
        for (std::size_t i = 0; i < size; ++i) {
            buffer_[buffer_size_++] = data[i];
            if (buffer_size_ == buffer_.size()) {
                process_block(buffer_.data());
                buffer_size_ = 0;
            }
        }
    }

    std::string final_hex() {
        buffer_[buffer_size_++] = 0x80;
        if (buffer_size_ > 56) {
            while (buffer_size_ < buffer_.size()) {
                buffer_[buffer_size_++] = 0;
            }
            process_block(buffer_.data());
            buffer_size_ = 0;
        }

        while (buffer_size_ < 56) {
            buffer_[buffer_size_++] = 0;
        }

        for (int shift = 56; shift >= 0; shift -= 8) {
            buffer_[buffer_size_++] = static_cast<std::uint8_t>((bit_count_ >> shift) & 0xff);
        }
        process_block(buffer_.data());

        std::ostringstream stream;
        stream << std::hex << std::setfill('0');
        for (const std::uint32_t word : state_) {
            stream << std::setw(8) << word;
        }
        return stream.str();
    }

private:
    void process_block(const std::uint8_t* block) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t offset = i * 4;
            words[i] = (static_cast<std::uint32_t>(block[offset]) << 24) |
                       (static_cast<std::uint32_t>(block[offset + 1]) << 16) |
                       (static_cast<std::uint32_t>(block[offset + 2]) << 8) |
                       static_cast<std::uint32_t>(block[offset + 3]);
        }

        for (std::size_t i = 16; i < words.size(); ++i) {
            words[i] = small_sigma1(words[i - 2]) + words[i - 7] +
                       small_sigma0(words[i - 15]) + words[i - 16];
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];
        std::uint32_t f = state_[5];
        std::uint32_t g = state_[6];
        std::uint32_t h = state_[7];

        for (std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t temp1 =
                h + big_sigma1(e) + choose(e, f, g) + kRoundConstants[i] + words[i];
            const std::uint32_t temp2 = big_sigma0(a) + majority(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_ = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_size_ = 0;
    std::uint64_t bit_count_ = 0;
};

bool is_hex_digit(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

}  // namespace

std::string sha256_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("unable to open file for SHA-256: " + path);
    }

    Sha256 sha256;
    std::array<char, 64 * 1024> buffer{};
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize read = file.gcount();
        if (read > 0) {
            sha256.update(reinterpret_cast<const std::uint8_t*>(buffer.data()),
                          static_cast<std::size_t>(read));
        }
    }

    if (!file.eof()) {
        throw std::runtime_error("unable to read file for SHA-256: " + path);
    }

    return sha256.final_hex();
}

bool is_sha256_hex(const std::string& value) {
    if (value.size() != 64) {
        return false;
    }

    for (const char ch : value) {
        if (!is_hex_digit(ch)) {
            return false;
        }
    }
    return true;
}

}  // namespace packetbridge::crypto
