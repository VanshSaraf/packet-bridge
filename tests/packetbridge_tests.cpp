#include "crypto/sha256.hpp"
#include "net/protocol.hpp"
#include "transfer/checkpoint.hpp"
#include "transfer/chunk_codec.hpp"
#include "transfer/continuation_codec.hpp"
#include "transfer/file_utils.hpp"
#include "transfer/hash_codec.hpp"
#include "transfer/manifest_codec.hpp"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace {

void write_file(const std::string& path, const std::string& data) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << data;
}

void test_sha256_file() {
    const std::string path = "/tmp/packetbridge_sha256_abc.txt";
    write_file(path, "abc");
    assert(packetbridge::crypto::sha256_file(path) ==
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    std::remove(path.c_str());
}

void test_filename_sanitization() {
    using packetbridge::transfer::sanitize_filename;
    assert(sanitize_filename("../../secret.txt") == "secret.txt");
    assert(sanitize_filename("..hidden") == "hidden");
    assert(sanitize_filename("unsafe name?.bin") == "unsafe_name_.bin");
    assert(sanitize_filename("////") == "file");
}

void test_manifest_codec() {
    packetbridge::net::protocol::FileManifest manifest;
    manifest.filename = "sample.bin";
    manifest.file_size = 1025;
    manifest.chunk_size = 512;
    manifest.chunk_count = 3;
    manifest.session_id = 42;

    const auto frame = packetbridge::transfer::serialize_file_manifest(manifest);
    packetbridge::net::protocol::FileManifest decoded;
    assert(packetbridge::transfer::deserialize_file_manifest(frame, decoded));
    assert(decoded.filename == manifest.filename);
    assert(decoded.file_size == manifest.file_size);
    assert(decoded.chunk_size == manifest.chunk_size);
    assert(decoded.chunk_count == manifest.chunk_count);
    assert(decoded.session_id == manifest.session_id);
}

void test_chunk_codec() {
    packetbridge::net::protocol::ChunkHeader header;
    header.session_id = 7;
    header.chunk_index = 2;
    header.byte_offset = 131072;
    header.payload_size = 65536;

    const auto payload = packetbridge::transfer::serialize_chunk_header(header);
    packetbridge::net::protocol::ChunkHeader decoded;
    assert(packetbridge::transfer::deserialize_chunk_header(payload, decoded));
    assert(decoded.session_id == header.session_id);
    assert(decoded.chunk_index == header.chunk_index);
    assert(decoded.byte_offset == header.byte_offset);
    assert(decoded.payload_size == header.payload_size);

    packetbridge::net::protocol::FileManifest manifest;
    manifest.file_size = 262144;
    manifest.chunk_size = 65536;
    manifest.chunk_count = 4;
    manifest.session_id = 7;
    assert(packetbridge::transfer::validate_chunk_header(manifest, decoded));
}

void test_hash_codec() {
    packetbridge::net::protocol::HashPacket packet;
    packet.session_id = 99;
    packet.sha256_hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

    const auto payload = packetbridge::transfer::serialize_hash_packet(packet);
    packetbridge::net::protocol::HashPacket decoded;
    assert(packetbridge::transfer::deserialize_hash_packet(payload, decoded));
    assert(decoded.session_id == packet.session_id);
    assert(decoded.sha256_hex == packet.sha256_hex);
}

void test_continuation_codec() {
    packetbridge::net::protocol::ContinueRequest request;
    request.session_id = 5;
    request.total_chunks = 6;
    request.missing_chunk_indexes = {1, 3, 5};

    const auto payload = packetbridge::transfer::serialize_continue_request(request);
    packetbridge::net::protocol::ContinueRequest decoded;
    assert(packetbridge::transfer::deserialize_continue_request(payload, decoded));
    assert(decoded.session_id == request.session_id);
    assert(decoded.total_chunks == request.total_chunks);
    assert(decoded.missing_chunk_indexes == request.missing_chunk_indexes);

    packetbridge::net::protocol::FileManifest manifest;
    manifest.session_id = 5;
    manifest.chunk_count = 6;
    assert(packetbridge::transfer::validate_continue_request(manifest, decoded));
}

void test_checkpoint_roundtrip() {
    const std::string path = "/tmp/packetbridge_test.pbcheckpoint";

    packetbridge::net::protocol::FileManifest manifest;
    manifest.filename = "payload.bin";
    manifest.file_size = 4096;
    manifest.chunk_size = 1024;
    manifest.chunk_count = 4;

    auto checkpoint = packetbridge::transfer::create_checkpoint_from_manifest(manifest);
    checkpoint.completed_chunks.insert(0);
    checkpoint.completed_chunks.insert(2);

    assert(packetbridge::transfer::save_checkpoint(path, checkpoint));

    packetbridge::transfer::TransferCheckpoint loaded;
    assert(packetbridge::transfer::load_checkpoint(path, loaded));
    assert(packetbridge::transfer::checkpoint_matches_manifest(loaded, manifest));

    const auto missing = packetbridge::transfer::missing_chunks(loaded);
    assert((missing == std::vector<std::uint64_t>{1, 3}));
    assert(packetbridge::transfer::remove_checkpoint(path));
    assert(!packetbridge::transfer::file_exists(path));
}

}  // namespace

int main() {
    test_sha256_file();
    test_filename_sanitization();
    test_manifest_codec();
    test_chunk_codec();
    test_hash_codec();
    test_continuation_codec();
    test_checkpoint_roundtrip();
    return 0;
}
