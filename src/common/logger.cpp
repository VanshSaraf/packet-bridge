#include "common/logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace packetbridge::common {
namespace {

std::mutex& log_mutex() {
    static std::mutex mutex;
    return mutex;
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};
#if defined(_WIN32)
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif

    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

void log(std::ostream& stream, const char* level, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex());
    stream << "[" << timestamp() << "] "
           << "[" << level << "] "
           << message << '\n';
}

}  // namespace

void info(const std::string& message) {
    log(std::cout, "INFO", message);
}

void warn(const std::string& message) {
    log(std::cout, "WARN", message);
}

void error(const std::string& message) {
    log(std::cerr, "ERROR", message);
}

}  // namespace packetbridge::common
