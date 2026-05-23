#ifndef LOGGER_HPP
#define LOGGER_HPP

// =============================================================================
//  Logger.hpp
//  ---------------------------------------------------------------------------
//  A small, thread-safe logger. Because the server runs one thread per client,
//  multiple threads may try to write a log line at the same instant. Without
//  synchronisation their output would interleave character-by-character and be
//  unreadable. This logger serialises writes behind a single mutex and stamps
//  every line with a timestamp and severity level.
//
//  Output goes to std::clog (stderr) and, optionally, to a log file on disk so
//  that the server keeps a persistent record of connections and messages.
// =============================================================================

#include <fstream>
#include <mutex>
#include <string>

namespace chat {

enum class LogLevel { INFO, WARN, ERROR };

class Logger {
public:
    // Returns the process-wide singleton instance.
    static Logger& instance();

    // Mirror all log output to the given file (in addition to stderr).
    // Safe to call once during start-up.
    void enableFileLogging(const std::string& path);

    // Core logging entry points.
    void info (const std::string& msg);
    void warn (const std::string& msg);
    void error(const std::string& msg);

    // Non-copyable, non-movable singleton.
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    ~Logger();

    void log(LogLevel level, const std::string& msg);
    static std::string timestamp();
    static const char* levelToString(LogLevel level);

    std::mutex    mutex_;
    std::ofstream file_;
    bool          fileEnabled_ = false;
};

}  // namespace chat

#endif  // LOGGER_HPP
