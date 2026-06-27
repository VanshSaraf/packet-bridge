#include "transfer/progress_tracker.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace packetbridge::transfer {
namespace {

std::string progress_bar(double percent) {
    constexpr int width = 20;
    const int filled = static_cast<int>((percent / 100.0) * width);

    std::string bar = "[";
    for (int i = 0; i < width; ++i) {
        bar += i < filled ? '#' : '-';
    }
    bar += "]";
    return bar;
}

}  // namespace

ProgressTracker::ProgressTracker(std::uint64_t total_bytes, std::uint64_t total_chunks)
    : total_bytes_(total_bytes),
      total_chunks_(total_chunks),
      started_at_(std::chrono::steady_clock::now()),
      last_report_at_(started_at_) {}

void ProgressTracker::add_progress(std::uint64_t bytes, std::uint64_t chunks) {
    bytes_transferred_ += bytes;
    chunks_completed_ += chunks;
}

bool ProgressTracker::should_report() {
    const auto now = std::chrono::steady_clock::now();
    if (chunks_completed_ == total_chunks_ ||
        now - last_report_at_ >= std::chrono::milliseconds(500)) {
        last_report_at_ = now;
        return true;
    }
    return false;
}

std::string ProgressTracker::progress_line() const {
    const double percent = total_bytes_ == 0
                               ? 100.0
                               : (static_cast<double>(bytes_transferred_) * 100.0) /
                                     static_cast<double>(total_bytes_);
    const double speed = average_mbps();
    const double remaining_bytes = static_cast<double>(
        bytes_transferred_ >= total_bytes_ ? 0 : total_bytes_ - bytes_transferred_);
    const double eta = speed > 0.0 ? remaining_bytes / (speed * 1024.0 * 1024.0) : 0.0;

    std::ostringstream stream;
    stream << progress_bar(std::min(100.0, percent)) << ' '
           << std::fixed << std::setprecision(1) << std::min(100.0, percent) << "% | "
           << chunks_completed_ << "/" << total_chunks_ << " chunks | "
           << std::setprecision(2) << speed << " MB/s | ETA " << format_duration(eta);
    return stream.str();
}

std::string ProgressTracker::summary_line() const {
    std::ostringstream stream;
    stream << "elapsed " << format_duration(elapsed_seconds())
           << ", average " << std::fixed << std::setprecision(2)
           << average_mbps() << " MB/s";
    return stream.str();
}

std::uint64_t ProgressTracker::bytes_transferred() const {
    return bytes_transferred_;
}

std::uint64_t ProgressTracker::chunks_completed() const {
    return chunks_completed_;
}

double ProgressTracker::elapsed_seconds() const {
    const auto elapsed = std::chrono::steady_clock::now() - started_at_;
    return std::chrono::duration<double>(elapsed).count();
}

double ProgressTracker::average_mbps() const {
    const double elapsed = elapsed_seconds();
    if (elapsed <= 0.0) {
        return 0.0;
    }
    return (static_cast<double>(bytes_transferred_) / (1024.0 * 1024.0)) / elapsed;
}

std::string format_duration(double seconds) {
    const auto total_seconds = static_cast<long long>(std::max(0.0, seconds));
    const long long hours = total_seconds / 3600;
    const long long minutes = (total_seconds % 3600) / 60;
    const long long secs = total_seconds % 60;

    std::ostringstream stream;
    if (hours > 0) {
        stream << std::setw(2) << std::setfill('0') << hours << ":";
    }
    stream << std::setw(2) << std::setfill('0') << minutes << ":"
           << std::setw(2) << std::setfill('0') << secs;
    return stream.str();
}

}  // namespace packetbridge::transfer
