#ifndef LOGGER_HPP
#define LOGGER_HPP
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <sstream>
#include <cstddef>

enum class LogLevel { Info, Warning, Error };

struct LogEntry {
	LogLevel level;
	std::string time;   // "HH:MM:SS"
	std::string text;   // message body (may contain embedded newlines, e.g. shader infoLog)
};

// Global thread-safe log buffer. Model-loading worker threads call log();
// the UI thread takes a snapshot() to draw. Header-inline Meyers singleton so it is
// safe to use during static init / before the GL context exists.
class Logger {
public:
	static Logger& getInstance() {
		static Logger instance;
		return instance;
	}

	void log(LogLevel level, const std::string& msg);
	void clear();
	std::vector<LogEntry> snapshot();          // copy taken under the lock
	unsigned long long revision();             // bumped on every add/clear

	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

private:
	Logger() = default;
	~Logger() = default;

	static constexpr std::size_t kMaxEntries = 1000;   // drop-oldest cap

	std::mutex mutex_;
	std::vector<LogEntry> entries_;
	unsigned long long revision_ = 0;
};

// Stream-style macros: ergonomic drop-in for the existing
// `std::cout << "literal" << runtimeValue << std::endl;` sites.
//   LOG_ERROR("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog);
#define LOG_AT(level, expr)                               \
	do {                                                  \
		std::ostringstream _log_oss;                      \
		_log_oss << expr;                                 \
		Logger::getInstance().log(level, _log_oss.str()); \
	} while (0)

#define LOG_WARN(expr)  LOG_AT(LogLevel::Warning, expr)
#define LOG_ERROR(expr) LOG_AT(LogLevel::Error, expr)

#endif // !LOGGER_HPP
