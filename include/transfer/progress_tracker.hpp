#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace packetbridge::transfer {

class ProgressTracker {
public:
    ProgressTracker(std::uint64_t total_bytes, std::uint64_t total_chunks);

    void add_progress(std::uint64_t bytes, std::uint64_t chunks);
    bool should_report();
    std::string progress_line() const;
    std::string summary_line() const;

    std::uint64_t bytes_transferred() const;
    std::uint64_t chunks_completed() const;
    double elapsed_seconds() const;
    double average_mbps() const;

private:
    std::uint64_t total_bytes_ = 0;
    std::uint64_t total_chunks_ = 0;
    std::uint64_t bytes_transferred_ = 0;
    std::uint64_t chunks_completed_ = 0;
    std::chrono::steady_clock::time_point started_at_;
    std::chrono::steady_clock::time_point last_report_at_;
};

std::string format_duration(double seconds);

}  // namespace packetbridge::transfer
