cmake_minimum_required(VERSION 3.14)

project(PacketBridge
    VERSION 0.1.0
    DESCRIPTION "Secure P2P LAN Transfer Engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(packetbridge_core
    src/common/logger.cpp
    src/crypto/sha256.cpp
    src/discovery/discovery_message.cpp
    src/net/endpoint.cpp
    src/net/socket_runtime.cpp
    src/net/socket_utils.cpp
    src/net/tcp_server.cpp
    src/net/tcp_socket.cpp
    src/net/udp_socket.cpp
    src/transfer/chunk_codec.cpp
    src/transfer/file_utils.cpp
    src/transfer/hash_codec.cpp
    src/transfer/manifest_codec.cpp
    src/transfer/progress_tracker.cpp
)

target_include_directories(packetbridge_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
    target_compile_definitions(packetbridge_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(packetbridge_core PUBLIC ws2_32)
endif()

add_executable(packetbridge_listener apps/listener.cpp)
target_link_libraries(packetbridge_listener PRIVATE packetbridge_core)

add_executable(packetbridge_broadcaster apps/broadcaster.cpp)
target_link_libraries(packetbridge_broadcaster PRIVATE packetbridge_core)

add_executable(packetbridge_sender apps/sender.cpp)
target_link_libraries(packetbridge_sender PRIVATE packetbridge_core)

add_executable(packetbridge_receiver apps/receiver.cpp)
target_link_libraries(packetbridge_receiver PRIVATE packetbridge_core)

enable_testing()
