// =============================================================================
//  Logger.cpp
//  ---------------------------------------------------------------------------
//  Implementation of the thread-safe Logger declared in Logger.hpp.
// =============================================================================

#include "Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace chat {

Logger& Logger::instance() {
    // Guaranteed thread-safe initialisation since C++11 ("magic statics").
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::enableFileLogging(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.open(path, std::ios::app);
    if (file_.is_open()) {
        fileEnabled_ = true;
    } else {
        // Fall back to stderr-only logging; report the failure.
        std::clog << "[WARN] could not open log file: " << path << '\n';
        fileEnabled_ = false;
    }
}

void Logger::info (const std::string& msg) { log(LogLevel::INFO,  msg); }
void Logger::warn (const std::string& msg) { log(LogLevel::WARN,  msg); }
void Logger::error(const std::string& msg) { log(LogLevel::ERROR, msg); }

void Logger::log(LogLevel level, const std::string& msg) {
    // Build the line outside the lock to keep the critical section short.
    std::ostringstream line;
    line << timestamp() << " [" << levelToString(level) << "] " << msg << '\n';
    const std::string out = line.str();

    std::lock_guard<std::mutex> lock(mutex_);
    std::clog << out;
    std::clog.flush();
    if (fileEnabled_ && file_.is_open()) {
        file_ << out;
        file_.flush();
    }
}

std::string Logger::timestamp() {
    using namespace std::chrono;
    const auto now   = system_clock::now();
    const auto timeT = system_clock::to_time_t(now);
    const auto ms    = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tmBuf{};
#if defined(_WIN32)
    localtime_s(&tmBuf, &timeT);
#else
    localtime_r(&timeT, &tmBuf);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

}  // namespace chat
